#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND, UINT, WPARAM, LPARAM );

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using present_fn = HRESULT( STDMETHODCALLTYPE* )( IDXGISwapChain*, UINT, UINT );
static present_fn g_original_present = nullptr;
static WNDPROC    g_original_wndproc = nullptr;
static bool       g_menu_open = true;
static std::atomic<bool> g_running{ true };

static LRESULT WINAPI hooked_wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if ( g_menu_open )
    {
        ImGui_ImplWin32_WndProcHandler( hwnd, msg, wp, lp );
        return TRUE;
    }
    return CallWindowProcW( g_original_wndproc, hwnd, msg, wp, lp );
}

static ID3D11Device*            g_device = nullptr;
static ID3D11DeviceContext*     g_context = nullptr;
static ID3D11RenderTargetView*  g_rtv = nullptr;
static HWND                     g_hwnd = nullptr;
static bool                     g_init = false;

static HRESULT STDMETHODCALLTYPE hooked_present( IDXGISwapChain* swap, UINT sync, UINT flags )
{
    if ( !g_init )
    {
        if ( FAILED( swap->GetDevice( __uuidof( ID3D11Device ), reinterpret_cast<void**>( &g_device ) ) ) )
            return g_original_present( swap, sync, flags );

        g_device->GetImmediateContext( &g_context );

        DXGI_SWAP_CHAIN_DESC sd{};
        swap->GetDesc( &sd );
        g_hwnd = sd.OutputWindow;

        ID3D11Texture2D* back_buffer = nullptr;
        swap->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &back_buffer ) );
        g_device->CreateRenderTargetView( back_buffer, nullptr, &g_rtv );
        back_buffer->Release( );

        ImGui::CreateContext( );
        ImGuiIO& io = ImGui::GetIO( );
        io.IniFilename = nullptr;

        ImGui_ImplWin32_Init( g_hwnd );
        ImGui_ImplDX11_Init( g_device, g_context );
        ImGui::StyleColorsDark( );

        g_original_wndproc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW( g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( hooked_wndproc ) )
        );

        g_init = true;
    }

    ImGui_ImplDX11_NewFrame( );
    ImGui_ImplWin32_NewFrame( );
    ImGui::NewFrame( );

    if ( GetAsyncKeyState( VK_INSERT ) & 1 )
        g_menu_open = !g_menu_open;

    if ( g_menu_open )
    {
        ImGui::Begin( "cs2-imgui", &g_menu_open, ImGuiWindowFlags_AlwaysAutoResize );
        ImGui::Text( "injected via kernel driver (physical memory)" );
        ImGui::Text( "DTB-based page table walking" );
        ImGui::Separator( );
        ImGui::Text( "press INSERT to toggle this menu" );

        static float test_slider = 0.5f;
        ImGui::SliderFloat( "test slider", &test_slider, 0.0f, 1.0f );

        static int test_counter = 0;
        if ( ImGui::Button( "click me" ) )
            test_counter++;
        ImGui::SameLine( );
        ImGui::Text( "count: %d", test_counter );

        static float color[3] = { 0.4f, 0.7f, 1.0f };
        ImGui::ColorEdit3( "color picker", color );

        if ( ImGui::Button( "eject" ) )
            g_running = false;

        ImGui::End( );
    }

    ImGui::Render( );
    g_context->OMSetRenderTargets( 1, &g_rtv, nullptr );
    ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );

    return g_original_present( swap, sync, flags );
}

static void init_thread( HMODULE self )
{
    if ( MH_Initialize( ) != MH_OK )
        return;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof( wc );
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW( nullptr );
    wc.lpszClassName = L"DummyDX";
    RegisterClassExW( &wc );

    HWND hwnd = CreateWindowExW( 0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr );

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* sc = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL fl;

    if ( SUCCEEDED( D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &sc, &dev, &fl, &ctx ) ) )
    {
        void** vtable = *reinterpret_cast<void***>( sc );

        MH_CreateHook( vtable[8], &hooked_present, reinterpret_cast<void**>( &g_original_present ) );
        MH_EnableHook( MH_ALL_HOOKS );

        sc->Release( );
        dev->Release( );
        ctx->Release( );
    }

    DestroyWindow( hwnd );
    UnregisterClassW( wc.lpszClassName, wc.hInstance );

    while ( true )
        Sleep( 1000 );
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD reason, LPVOID )
{
    if ( reason == DLL_PROCESS_ATTACH )
    {
        auto thread = CreateThread( nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>( init_thread ),
            hModule, 0, nullptr );
        if ( thread )
            CloseHandle( thread );
    }
    return TRUE;
}
