#pragma once
#include "imgui/imgui.h"
#include "poppins_font.h"

namespace style
{
    inline void apply()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        ImFontConfig cfg;
        cfg.PixelSnapH = false;
        cfg.OversampleH = 5;
        cfg.OversampleV = 5;
        cfg.RasterizerMultiply = 1.2f;
        io.Fonts->AddFontFromMemoryTTF( poppin_font, sizeof( poppin_font ), 15.0f, &cfg );

        auto& s = ImGui::GetStyle();
        s.WindowRounding    = 10.0f;
        s.FrameRounding     = 4.0f;
        s.GrabRounding      = 3.0f;
        s.ScrollbarRounding = 3.0f;
        s.ScrollbarSize     = 4.0f;
        s.GrabMinSize       = 12.0f;
        s.FramePadding      = ImVec2( 4, 3 );
        s.ItemSpacing       = ImVec2( 6, 3 );
        s.WindowPadding     = ImVec2( 6, 6 );
        s.WindowBorderSize  = 0;

        auto& c = s.Colors;
        c[ImGuiCol_WindowBg]         = ImVec4( 0.08f, 0.08f, 0.10f, 0.95f );
        c[ImGuiCol_ChildBg]          = ImVec4( 0.10f, 0.10f, 0.12f, 1.00f );
        c[ImGuiCol_Border]           = ImVec4( 0.20f, 0.20f, 0.22f, 1.00f );
        c[ImGuiCol_FrameBg]          = ImVec4( 0.12f, 0.12f, 0.14f, 1.00f );
        c[ImGuiCol_FrameBgHovered]   = ImVec4( 0.16f, 0.16f, 0.18f, 1.00f );
        c[ImGuiCol_FrameBgActive]    = ImVec4( 0.20f, 0.20f, 0.22f, 1.00f );
        c[ImGuiCol_SliderGrab]       = ImVec4( 0.63f, 0.47f, 0.92f, 1.00f );
        c[ImGuiCol_SliderGrabActive] = ImVec4( 0.72f, 0.55f, 0.97f, 1.00f );
        c[ImGuiCol_Button]           = ImVec4( 0.15f, 0.15f, 0.18f, 1.00f );
        c[ImGuiCol_ButtonHovered]    = ImVec4( 0.22f, 0.22f, 0.26f, 1.00f );
        c[ImGuiCol_ButtonActive]     = ImVec4( 0.63f, 0.47f, 0.92f, 0.40f );
        c[ImGuiCol_Header]           = ImVec4( 0.14f, 0.14f, 0.16f, 1.00f );
        c[ImGuiCol_HeaderHovered]    = ImVec4( 0.20f, 0.20f, 0.24f, 1.00f );
        c[ImGuiCol_HeaderActive]     = ImVec4( 0.26f, 0.26f, 0.30f, 1.00f );
        c[ImGuiCol_CheckMark]        = ImVec4( 0.63f, 0.47f, 0.92f, 1.00f );
        c[ImGuiCol_Text]             = ImVec4( 0.85f, 0.85f, 0.90f, 1.00f );
        c[ImGuiCol_TextDisabled]     = ImVec4( 0.45f, 0.45f, 0.50f, 1.00f );
        c[ImGuiCol_TitleBg]          = ImVec4( 0.08f, 0.08f, 0.10f, 1.00f );
        c[ImGuiCol_TitleBgActive]    = ImVec4( 0.08f, 0.08f, 0.10f, 1.00f );
        c[ImGuiCol_Separator]        = ImVec4( 0.20f, 0.20f, 0.22f, 1.00f );
    }
}
