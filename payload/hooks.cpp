#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include "MinHook.h"
#include "hooks.h"
#include "menu.h"
#include "style.h"
#include "sdk/schema.h"
#include "DX11BlurEffect.h"
#include "imgui/imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND, UINT, WPARAM, LPARAM );

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using present_fn = HRESULT( STDMETHODCALLTYPE* )( IDXGISwapChain*, UINT, UINT );
using resize_fn  = HRESULT( STDMETHODCALLTYPE* )( IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT );

static present_fn  o_present = nullptr;
static resize_fn   o_resize  = nullptr;
static WNDPROC     o_wndproc = nullptr;

static ID3D11Device*           g_device  = nullptr;
static ID3D11DeviceContext*    g_context = nullptr;
static ID3D11RenderTargetView* g_rtv     = nullptr;
static HWND                    g_hwnd    = nullptr;
static bool                    g_ready   = false;

static LRESULT WINAPI wndproc_hook( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wp, lp ) )
        return TRUE;

    if ( menu::open )
    {
        switch ( msg )
        {
        case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_MBUTTONDOWN: case WM_MBUTTONUP:
        case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_CHAR:
            return TRUE;
        }
    }

    return CallWindowProcW( o_wndproc, hwnd, msg, wp, lp );
}

static HRESULT STDMETHODCALLTYPE resize_hook( IDXGISwapChain* swap, UINT count,
    UINT w, UINT h, DXGI_FORMAT fmt, UINT flags )
{
    if ( g_rtv ) { g_rtv->Release(); g_rtv = nullptr; }
    return o_resize( swap, count, w, h, fmt, flags );
}

static HRESULT STDMETHODCALLTYPE present_hook( IDXGISwapChain* swap, UINT sync, UINT flags )
{
    if ( !g_ready )
    {
        if ( FAILED( swap->GetDevice( __uuidof( ID3D11Device ), reinterpret_cast<void**>( &g_device ) ) ) )
            return o_present( swap, sync, flags );

        g_device->GetImmediateContext( &g_context );

        DXGI_SWAP_CHAIN_DESC sd{};
        swap->GetDesc( &sd );
        g_hwnd = sd.OutputWindow;

        ID3D11Texture2D* bb = nullptr;
        swap->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &bb ) );
        g_device->CreateRenderTargetView( bb, nullptr, &g_rtv );
        bb->Release();

        ImGui::CreateContext();
        style::apply();

        ImGui_ImplWin32_Init( g_hwnd );
        ImGui_ImplDX11_Init( g_device, g_context );

        o_wndproc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW( g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( wndproc_hook ) )
        );

        blurEffect.Initialize( g_device, g_context );
        g_ready = true;
    }

    // recreate rtv after resize
    if ( !g_rtv )
    {
        ID3D11Texture2D* bb = nullptr;
        if ( SUCCEEDED( swap->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &bb ) ) ) )
        {
            g_device->CreateRenderTargetView( bb, nullptr, &g_rtv );
            bb->Release();
        }
    }

    if ( !g_rtv )
        return o_present( swap, sync, flags );

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if ( GetAsyncKeyState( VK_INSERT ) & 1 )
        menu::open = !menu::open;

    if ( menu::open )
        menu::render( swap );

    ImGui::Render();
    g_context->OMSetRenderTargets( 1, &g_rtv, nullptr );
    ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );

    return o_present( swap, sync, flags );
}

void hooks::init( HMODULE self )
{
    schema::init();

    if ( MH_Initialize() != MH_OK )
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
        MH_CreateHook( vtable[8],  &present_hook, reinterpret_cast<void**>( &o_present ) );
        MH_CreateHook( vtable[13], &resize_hook,  reinterpret_cast<void**>( &o_resize ) );
        MH_EnableHook( MH_ALL_HOOKS );
        sc->Release(); dev->Release(); ctx->Release();
    }

    DestroyWindow( hwnd );
    UnregisterClassW( wc.lpszClassName, wc.hInstance );

    while ( hooks::running )
        Sleep( 100 );

    MH_DisableHook( MH_ALL_HOOKS );
    MH_Uninitialize();
}
