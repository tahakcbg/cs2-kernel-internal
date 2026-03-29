#include "menu.h"
#include "sdk/offsets.h"

constexpr float SIDEBAR_W = 155.0f;
constexpr float MENU_W    = 750.0f;
constexpr float MENU_H    = 480.0f;
constexpr float ROUNDING  = 10.0f;

static void draw_sidebar( ImDrawList* draw, ImVec2 pos )
{
    // backgrounds
    draw->AddRectFilled( pos, ImVec2( pos.x + SIDEBAR_W, pos.y + MENU_H ),
        ImColor( 22, 22, 26, 230 ), ROUNDING, ImDrawFlags_RoundCornersLeft );
    draw->AddRectFilled( ImVec2( pos.x + SIDEBAR_W, pos.y ), ImVec2( pos.x + MENU_W, pos.y + MENU_H ),
        ImColor( 18, 18, 22, 210 ), ROUNDING, ImDrawFlags_RoundCornersRight );
    draw->AddLine( ImVec2( pos.x + SIDEBAR_W, pos.y + 10 ), ImVec2( pos.x + SIDEBAR_W, pos.y + MENU_H - 10 ),
        ImColor( 40, 40, 45, 255 ) );

    // title
    draw->AddText( ImVec2( pos.x + 18, pos.y + 18 ), ImColor( 160, 120, 235, 255 ), "vigil" );
    draw->AddText( ImVec2( pos.x + 18, pos.y + 36 ), ImColor( 70, 70, 80, 255 ), "kernel internal" );
    draw->AddLine( ImVec2( pos.x + 14, pos.y + 58 ), ImVec2( pos.x + SIDEBAR_W - 14, pos.y + 58 ),
        ImColor( 40, 40, 45, 255 ) );

    // tabs
    struct tab_entry { const char* label; const char* section; };
    static tab_entry tabs[] = {
        { "Aimbot",   "Combat" },
        { "Visuals",  "Combat" },
        { "Players",  "Visuals" },
        { "World",    "Visuals" },
        { "Movement", "Misc" },
        { "Settings", "Misc" },
    };
    constexpr int count = 6;

    float y = 68.0f;
    const char* last_sec = "";

    for ( int i = 0; i < count; i++ )
    {
        if ( strcmp( tabs[i].section, last_sec ) != 0 )
        {
            last_sec = tabs[i].section;
            draw->AddText( ImVec2( pos.x + 18, pos.y + y ), ImColor( 55, 55, 65, 255 ), last_sec );
            y += 20.0f;
        }

        ImVec2 mn( pos.x + 8, pos.y + y );
        ImVec2 mx( pos.x + SIDEBAR_W - 8, pos.y + y + 28.0f );

        bool hovered = ImGui::IsMouseHoveringRect( mn, mx );
        if ( hovered && ImGui::IsMouseClicked( 0 ) )
            menu::active_tab = i;

        if ( menu::active_tab == i )
        {
            draw->AddRectFilled( mn, mx, ImColor( 160, 120, 235, 25 ), 5.0f );
            draw->AddRectFilled( ImVec2( mn.x, mn.y + 4 ), ImVec2( mn.x + 3, mx.y - 4 ), ImColor( 160, 120, 235, 255 ), 2.0f );
            draw->AddText( ImVec2( mn.x + 14, mn.y + 5 ), ImColor( 160, 120, 235, 255 ), tabs[i].label );
        }
        else if ( hovered )
        {
            draw->AddRectFilled( mn, mx, ImColor( 255, 255, 255, 8 ), 5.0f );
            draw->AddText( ImVec2( mn.x + 14, mn.y + 5 ), ImColor( 160, 160, 170, 255 ), tabs[i].label );
        }
        else
        {
            draw->AddText( ImVec2( mn.x + 14, mn.y + 5 ), ImColor( 100, 100, 110, 255 ), tabs[i].label );
        }

        y += 32.0f;
    }

    // user info
    draw->AddLine( ImVec2( pos.x + 14, pos.y + MENU_H - 50 ), ImVec2( pos.x + SIDEBAR_W - 14, pos.y + MENU_H - 50 ),
        ImColor( 40, 40, 45, 255 ) );
    draw->AddCircleFilled( ImVec2( pos.x + 28, pos.y + MENU_H - 28 ), 10.0f, ImColor( 60, 60, 70, 255 ) );
    draw->AddText( ImVec2( pos.x + 25, pos.y + MENU_H - 35 ), ImColor( 150, 150, 160, 255 ), "U" );
    draw->AddText( ImVec2( pos.x + 44, pos.y + MENU_H - 38 ), ImColor( 200, 200, 210, 255 ), "User" );
    draw->AddText( ImVec2( pos.x + 44, pos.y + MENU_H - 22 ), ImColor( 80, 80, 90, 255 ), "kernel internal" );
}

static void draw_content()
{
    using namespace settings;

    switch ( menu::active_tab )
    {
    case 0:
        ui::SectionHeader( "Aimbot" );
        ui::Toggle( "Master switch",  &aimbot::enabled );
        ui::Toggle( "Silent aim",     &aimbot::silent );
        ui::Toggle( "Autowall",       &aimbot::autowall );
        ui::Toggle( "Autofire",       &aimbot::autofire );
        ImGui::Spacing();
        ui::SliderInt( "Field of view", &aimbot::fov, 1, 180, "%d" );
        ui::SliderInt( "Hitchance",     &aimbot::hitchance, 0, 100, "%d%%" );
        ui::SliderInt( "Smooth",        &aimbot::smooth, 1, 20, "%d" );
        break;

    case 1:
        ui::SectionHeader( "ESP" );
        ui::Toggle( "Box ESP",    &visuals::box_esp );
        ui::Toggle( "Name",       &visuals::name_esp );
        ui::Toggle( "Health bar", &visuals::health_bar );
        ui::Toggle( "Snaplines",  &visuals::snaplines );
        ui::Toggle( "Skeleton",   &visuals::skeleton );
        break;

    case 2:
        ui::SectionHeader( "Players" );
        ui::Toggle( "Glow",                &visuals::glow );
        ui::Toggle( "Chams",               &visuals::chams );
        ui::Toggle( "Chams through walls", &visuals::chams_xqz );
        break;

    case 3:
        ui::SectionHeader( "World" );
        ui::Toggle( "Night mode", &world::night_mode );
        ui::Toggle( "No flash",   &world::no_flash );
        ui::Toggle( "No smoke",   &world::no_smoke );
        ImGui::Spacing();
        ui::Slider( "FOV override", &world::fov_override, 60.0f, 130.0f, "%.0f" );
        break;

    case 4:
        ui::SectionHeader( "Movement" );
        ui::Toggle( "Bunny hop",    &movement::bhop );
        ui::Toggle( "Auto strafe",  &movement::autostrafe );
        ui::Toggle( "Edge jump",    &movement::edge_jump );
        break;

    case 5:
    {
        ui::SectionHeader( "Settings" );
        ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.55f, 1.0f ), "Menu key: INSERT" );
        ImGui::Spacing();

        // schema debug
        ui::SectionHeader( "Schema Debug" );
        ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.55f, 1.0f ), "m_iHealth: 0x%X", offsets::base_entity::m_iHealth() );
        ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.55f, 1.0f ), "m_iTeamNum: 0x%X", offsets::base_entity::m_iTeamNum() );
        ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.55f, 1.0f ), "m_hPlayerPawn: 0x%X", offsets::player_controller::m_hPlayerPawn() );
        ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.55f, 1.0f ), "m_pGameSceneNode: 0x%X", offsets::player_controller::m_bPawnIsAlive() );
        break;
    }
    }
}

void menu::render( IDXGISwapChain* swap )
{
    ImGui::SetNextWindowSize( ImVec2( MENU_W, MENU_H ) );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0, 0, 0, 0 ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
    ImGui::Begin( "##menu", &menu::open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings );
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();

    blurEffect.BeginBlur( swap );
    blurEffect.ApplyBlur( draw, pos, ImVec2( MENU_W, MENU_H ), 5.0f, ROUNDING, ImDrawFlags_RoundCornersAll );
    blurEffect.EndBlur();

    draw_sidebar( draw, pos );

    ImGui::SetCursorPos( ImVec2( SIDEBAR_W + 12, 12 ) );
    ImGui::PushItemWidth( MENU_W - SIDEBAR_W - 80 );
    ImGui::BeginChild( "##content", ImVec2( MENU_W - SIDEBAR_W - 32, MENU_H - 32 ), false, ImGuiWindowFlags_NoBackground );
    draw_content();
    ImGui::EndChild();
    ImGui::PopItemWidth();

    ImGui::End();
}
