#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>
#include "MinHook.h"
#include "DX11BlurEffect.h"
#include "poppins_font.h"
#include "imgui.h"
#include "widgets.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND, UINT, WPARAM, LPARAM );

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using present_fn = HRESULT( STDMETHODCALLTYPE* )( IDXGISwapChain*, UINT, UINT );
using resize_fn  = HRESULT( STDMETHODCALLTYPE* )( IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT );
static present_fn g_original_present = nullptr;
static resize_fn  g_original_resize = nullptr;
static WNDPROC    g_original_wndproc = nullptr;
static bool       g_menu_open = true;
static std::atomic<bool> g_running{ true };

static LRESULT WINAPI hooked_wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wp, lp ) )
        return TRUE;

    if ( g_menu_open )
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

    return CallWindowProcW( g_original_wndproc, hwnd, msg, wp, lp );
}

static ID3D11Device*            g_device = nullptr;
static ID3D11DeviceContext*     g_context = nullptr;
static ID3D11RenderTargetView*  g_rtv = nullptr;
static HWND                     g_hwnd = nullptr;
static bool                     g_init = false;

static HRESULT STDMETHODCALLTYPE hooked_resize( IDXGISwapChain* swap, UINT buffer_count,
    UINT width, UINT height, DXGI_FORMAT format, UINT flags )
{
    if ( g_rtv ) { g_rtv->Release( ); g_rtv = nullptr; }
    return g_original_resize( swap, buffer_count, width, height, format, flags );
}

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

        ImFontConfig font_cfg;
        font_cfg.PixelSnapH = false;
        font_cfg.OversampleH = 5;
        font_cfg.OversampleV = 5;
        font_cfg.RasterizerMultiply = 1.2f;
        io.Fonts->AddFontFromMemoryTTF( poppin_font, sizeof( poppin_font ), 15.0f, &font_cfg );

        ImGui_ImplWin32_Init( g_hwnd );
        ImGui_ImplDX11_Init( g_device, g_context );

        auto& style = ImGui::GetStyle( );
        style.WindowRounding = 10.0f;
        style.FrameRounding = 5.0f;
        style.ScrollbarRounding = 3.0f;
        style.ScrollbarSize = 4.0f;
        style.FramePadding = ImVec2( 6, 3 );
        style.ItemSpacing = ImVec2( 6, 3 );
        style.WindowPadding = ImVec2( 6, 6 );
        style.WindowBorderSize = 0;

        auto& c = style.Colors;
        c[ImGuiCol_WindowBg]        = ImVec4( 0.08f, 0.08f, 0.10f, 0.95f );
        c[ImGuiCol_ChildBg]         = ImVec4( 0.10f, 0.10f, 0.12f, 1.00f );
        c[ImGuiCol_Border]          = ImVec4( 0.20f, 0.20f, 0.22f, 1.00f );
        c[ImGuiCol_FrameBg]         = ImVec4( 0.14f, 0.14f, 0.16f, 1.00f );
        c[ImGuiCol_FrameBgHovered]  = ImVec4( 0.18f, 0.18f, 0.20f, 1.00f );
        c[ImGuiCol_FrameBgActive]   = ImVec4( 0.22f, 0.22f, 0.24f, 1.00f );
        c[ImGuiCol_SliderGrab]      = ImVec4( 0.39f, 0.55f, 0.92f, 1.00f );
        c[ImGuiCol_SliderGrabActive]= ImVec4( 0.45f, 0.60f, 0.95f, 1.00f );
        c[ImGuiCol_Button]          = ImVec4( 0.14f, 0.14f, 0.16f, 1.00f );
        c[ImGuiCol_ButtonHovered]   = ImVec4( 0.22f, 0.22f, 0.26f, 1.00f );
        c[ImGuiCol_ButtonActive]    = ImVec4( 0.30f, 0.30f, 0.36f, 1.00f );
        c[ImGuiCol_Header]          = ImVec4( 0.14f, 0.14f, 0.16f, 1.00f );
        c[ImGuiCol_HeaderHovered]   = ImVec4( 0.20f, 0.20f, 0.24f, 1.00f );
        c[ImGuiCol_HeaderActive]    = ImVec4( 0.26f, 0.26f, 0.30f, 1.00f );
        c[ImGuiCol_CheckMark]       = ImVec4( 0.39f, 0.55f, 0.92f, 1.00f );
        c[ImGuiCol_Text]            = ImVec4( 0.85f, 0.85f, 0.90f, 1.00f );
        c[ImGuiCol_TextDisabled]    = ImVec4( 0.45f, 0.45f, 0.50f, 1.00f );
        c[ImGuiCol_TitleBg]         = ImVec4( 0.08f, 0.08f, 0.10f, 1.00f );
        c[ImGuiCol_TitleBgActive]   = ImVec4( 0.08f, 0.08f, 0.10f, 1.00f );
        c[ImGuiCol_Separator]       = ImVec4( 0.20f, 0.20f, 0.22f, 1.00f );

        style.FrameRounding = 4.0f;
        style.FramePadding = ImVec2( 4, 3 );
        style.GrabMinSize = 12.0f;
        style.GrabRounding = 3.0f;

        c[ImGuiCol_SliderGrab]       = ImVec4( 0.63f, 0.47f, 0.92f, 1.00f );
        c[ImGuiCol_SliderGrabActive] = ImVec4( 0.72f, 0.55f, 0.97f, 1.00f );
        c[ImGuiCol_FrameBg]          = ImVec4( 0.12f, 0.12f, 0.14f, 1.00f );
        c[ImGuiCol_FrameBgHovered]   = ImVec4( 0.16f, 0.16f, 0.18f, 1.00f );
        c[ImGuiCol_FrameBgActive]    = ImVec4( 0.20f, 0.20f, 0.22f, 1.00f );

        c[ImGuiCol_CheckMark]        = ImVec4( 0.63f, 0.47f, 0.92f, 1.00f );

        c[ImGuiCol_Button]           = ImVec4( 0.15f, 0.15f, 0.18f, 1.00f );
        c[ImGuiCol_ButtonHovered]    = ImVec4( 0.22f, 0.22f, 0.26f, 1.00f );
        c[ImGuiCol_ButtonActive]     = ImVec4( 0.63f, 0.47f, 0.92f, 0.40f );

        g_original_wndproc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW( g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( hooked_wndproc ) )
        );

        blurEffect.Initialize( g_device, g_context );
        g_init = true;
    }

    if ( !g_rtv )
    {
        ID3D11Texture2D* back_buffer = nullptr;
        if ( SUCCEEDED( swap->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &back_buffer ) ) ) )
        {
            g_device->CreateRenderTargetView( back_buffer, nullptr, &g_rtv );
            back_buffer->Release( );
        }
    }

    if ( !g_rtv )
        return g_original_present( swap, sync, flags );

    ImGui_ImplDX11_NewFrame( );
    ImGui_ImplWin32_NewFrame( );
    ImGui::NewFrame( );

    if ( GetAsyncKeyState( VK_INSERT ) & 1 )
        g_menu_open = !g_menu_open;

    if ( g_menu_open )
    {
        static int active_tab = 0;

        constexpr float sidebar_w = 155.0f;
        constexpr float menu_w = 750.0f;
        constexpr float menu_h = 480.0f;
        constexpr float rounding = 10.0f;

        ImGui::SetNextWindowSize( ImVec2( menu_w, menu_h ) );
        ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ) );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
        ImGui::Begin( "##menu", &g_menu_open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings );
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();

        blurEffect.BeginBlur( swap );
        blurEffect.ApplyBlur( draw, pos, ImVec2( menu_w, menu_h ), 5.0f, rounding, ImDrawFlags_RoundCornersAll );
        blurEffect.EndBlur();

        draw->AddRectFilled( pos, ImVec2( pos.x + sidebar_w, pos.y + menu_h ),
            ImColor( 22, 22, 26, 230 ), rounding, ImDrawFlags_RoundCornersLeft );

        draw->AddRectFilled( ImVec2( pos.x + sidebar_w, pos.y ), ImVec2( pos.x + menu_w, pos.y + menu_h ),
            ImColor( 18, 18, 22, 210 ), rounding, ImDrawFlags_RoundCornersRight );

        draw->AddLine( ImVec2( pos.x + sidebar_w, pos.y + 10 ), ImVec2( pos.x + sidebar_w, pos.y + menu_h - 10 ),
            ImColor( 40, 40, 45, 255 ) );

        draw->AddText( ImVec2( pos.x + 18, pos.y + 18 ), ImColor( 160, 120, 235, 255 ), "vigil" );
        draw->AddText( ImVec2( pos.x + 18, pos.y + 36 ), ImColor( 70, 70, 80, 255 ), "kernel internal" );

        draw->AddLine( ImVec2( pos.x + 14, pos.y + 58 ), ImVec2( pos.x + sidebar_w - 14, pos.y + 58 ),
            ImColor( 40, 40, 45, 255 ) );

        struct tab_info { const char* label; const char* section; };
        tab_info tabs[] = {
            { "Aimbot",   "Combat" },
            { "Visuals",  "Combat" },
            { "Players",  "Visuals" },
            { "World",    "Visuals" },
            { "Movement", "Misc" },
            { "Settings", "Misc" },
        };
        constexpr int tab_count = 6;

        float tab_y = 68.0f;
        const char* last_section = "";

        for ( int i = 0; i < tab_count; i++ )
        {
            if ( strcmp( tabs[i].section, last_section ) != 0 )
            {
                last_section = tabs[i].section;
                draw->AddText( ImVec2( pos.x + 18, pos.y + tab_y ), ImColor( 55, 55, 65, 255 ), last_section );
                tab_y += 20.0f;
            }

            ImVec2 tab_min( pos.x + 8, pos.y + tab_y );
            ImVec2 tab_max( pos.x + sidebar_w - 8, pos.y + tab_y + 28.0f );

            bool hovered = ImGui::IsMouseHoveringRect( tab_min, tab_max );
            bool clicked = hovered && ImGui::IsMouseClicked( 0 );
            if ( clicked ) active_tab = i;

            if ( active_tab == i )
            {
                draw->AddRectFilled( tab_min, tab_max, ImColor( 160, 120, 235, 25 ), 5.0f );
                draw->AddRectFilled( ImVec2( tab_min.x, tab_min.y + 4 ), ImVec2( tab_min.x + 3, tab_max.y - 4 ),
                    ImColor( 160, 120, 235, 255 ), 2.0f );
                draw->AddText( ImVec2( tab_min.x + 14, tab_min.y + 5 ), ImColor( 160, 120, 235, 255 ), tabs[i].label );
            }
            else if ( hovered )
            {
                draw->AddRectFilled( tab_min, tab_max, ImColor( 255, 255, 255, 8 ), 5.0f );
                draw->AddText( ImVec2( tab_min.x + 14, tab_min.y + 5 ), ImColor( 160, 160, 170, 255 ), tabs[i].label );
            }
            else
            {
                draw->AddText( ImVec2( tab_min.x + 14, tab_min.y + 5 ), ImColor( 100, 100, 110, 255 ), tabs[i].label );
            }

            tab_y += 32.0f;
        }

        draw->AddLine( ImVec2( pos.x + 14, pos.y + menu_h - 50 ), ImVec2( pos.x + sidebar_w - 14, pos.y + menu_h - 50 ),
            ImColor( 40, 40, 45, 255 ) );
        draw->AddCircleFilled( ImVec2( pos.x + 28, pos.y + menu_h - 28 ), 10.0f, ImColor( 60, 60, 70, 255 ) );
        draw->AddText( ImVec2( pos.x + 28 - 3, pos.y + menu_h - 35 ), ImColor( 150, 150, 160, 255 ), "U" );
        draw->AddText( ImVec2( pos.x + 44, pos.y + menu_h - 38 ), ImColor( 200, 200, 210, 255 ), "User" );
        draw->AddText( ImVec2( pos.x + 44, pos.y + menu_h - 22 ), ImColor( 80, 80, 90, 255 ), "kernel internal" );

        ImGui::SetCursorPos( ImVec2( sidebar_w + 12, 12 ) );
        ImGui::PushItemWidth( menu_w - sidebar_w - 80 );
        ImGui::BeginChild( "##content", ImVec2( menu_w - sidebar_w - 32, menu_h - 32 ), false, ImGuiWindowFlags_NoBackground );

        switch ( active_tab )
        {
        case 0: // aimbot
        {
            ui::SectionHeader( "Aimbot" );

            static bool aim_enabled = false;
            static bool aim_silent = false;
            static bool aim_autowall = false;
            static bool aim_autofire = false;
            static int aim_fov = 60;
            static int aim_smooth = 5;
            static int aim_hitchance = 55;

            ui::Toggle( "Master switch", &aim_enabled );
            ui::Toggle( "Silent aim", &aim_silent );
            ui::Toggle( "Autowall", &aim_autowall );
            ui::Toggle( "Autofire", &aim_autofire );
            ImGui::Spacing();
            ui::SliderInt( "Field of view", &aim_fov, 1, 180, "%d" );
            ui::SliderInt( "Hitchance", &aim_hitchance, 0, 100, "%d%%" );
            ui::SliderInt( "Smooth", &aim_smooth, 1, 20, "%d" );
            break;
        }
        case 1: // visuals
        {
            ui::SectionHeader( "ESP" );

            static bool esp_box = false;
            static bool esp_name = false;
            static bool esp_health = false;
            static bool esp_snaplines = false;
            static bool esp_skeleton = false;

            ui::Toggle( "Box ESP", &esp_box );
            ui::Toggle( "Name", &esp_name );
            ui::Toggle( "Health bar", &esp_health );
            ui::Toggle( "Snaplines", &esp_snaplines );
            ui::Toggle( "Skeleton", &esp_skeleton );
            break;
        }
        case 2: // players
        {
            ui::SectionHeader( "Players" );

            static bool glow = false;
            static bool chams = false;
            static bool chams_xqz = false;

            ui::Toggle( "Glow", &glow );
            ui::Toggle( "Chams", &chams );
            ui::Toggle( "Chams through walls", &chams_xqz );
            break;
        }
        case 3: // world
        {
            ui::SectionHeader( "World" );

            static bool night_mode = false;
            static bool no_flash = false;
            static bool no_smoke = false;
            static float fov_val = 90.0f;

            ui::Toggle( "Night mode", &night_mode );
            ui::Toggle( "No flash", &no_flash );
            ui::Toggle( "No smoke", &no_smoke );
            ImGui::Spacing();
            ui::Slider( "FOV override", &fov_val, 60.0f, 130.0f, "%.0f" );
            break;
        }
        case 4: // movement
        {
            ui::SectionHeader( "Movement" );

            static bool bhop = false;
            static bool autostrafe = false;
            static bool edge_jump = false;

            ui::Toggle( "Bunny hop", &bhop );
            ui::Toggle( "Auto strafe", &autostrafe );
            ui::Toggle( "Edge jump", &edge_jump );
            break;
        }
        case 5: // settings
        {
            ui::SectionHeader( "Settings" );

            ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.55f, 1.0f ), "Menu key: INSERT" );
            ImGui::Spacing();

            if ( ImGui::Button( "Eject", ImVec2( -1, 28 ) ) )
                g_running = false;
            break;
        }
        }

        ImGui::EndChild();
        ImGui::PopItemWidth();

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
        MH_CreateHook( vtable[13], &hooked_resize, reinterpret_cast<void**>( &g_original_resize ) );
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
