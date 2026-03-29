#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace ui
{
    inline bool Toggle( const char* label, bool* v )
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if ( window->SkipItems ) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID id = window->GetID( label );
        const ImVec2 label_size = ImGui::CalcTextSize( label, nullptr, true );

        const float height = 18.0f;
        const float width = 34.0f;
        const float radius = height * 0.5f;
        const ImVec2 pos = window->DC.CursorPos;

        const float total_w = width + 8.0f + label_size.x;
        const ImRect total_bb( pos, ImVec2( pos.x + ImGui::GetContentRegionAvail().x, pos.y + ImMax( height, label_size.y ) ) );
        const ImRect toggle_bb( pos, ImVec2( pos.x + width, pos.y + height ) );

        ImGui::ItemSize( total_bb, 0.0f );
        if ( !ImGui::ItemAdd( total_bb, id ) )
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior( total_bb, id, &hovered, &held );
        if ( pressed ) *v = !*v;

        ImGuiStorage* storage = window->DC.StateStorage;
        float t = storage->GetFloat( id, *v ? 1.0f : 0.0f );
        t += ( ( *v ? 1.0f : 0.0f ) - t ) * ImGui::GetIO().DeltaTime * 12.0f;
        storage->SetFloat( id, t );

        ImDrawList* draw = window->DrawList;
        float alpha = ImGui::GetStyle().Alpha;

        // track
        ImU32 track_col = *v
            ? ImColor( 0.63f, 0.47f, 0.92f, 0.6f * alpha )
            : ImColor( 0.25f, 0.25f, 0.28f, alpha );
        draw->AddRectFilled( toggle_bb.Min, toggle_bb.Max, track_col, radius );

        // knob
        float knob_x = ImLerp( pos.x + radius, pos.x + width - radius, t );
        float knob_r = radius - 3.0f;
        draw->AddCircleFilled( ImVec2( knob_x, pos.y + radius ), knob_r, ImColor( 1.0f, 1.0f, 1.0f, alpha ) );

        // label on the right
        draw->AddText( ImVec2( pos.x + width + 8.0f, pos.y + ( height - label_size.y ) * 0.5f ),
            ImColor( 0.78f, 0.78f, 0.82f, alpha ), label );

        return pressed;
    }

    inline bool Slider( const char* label, float* v, float v_min, float v_max, const char* fmt = "%.0f" )
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if ( window->SkipItems ) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID id = window->GetID( label );
        const ImVec2 label_size = ImGui::CalcTextSize( label, nullptr, true );

        const float height = 22.0f;
        const float track_h = 4.0f;
        const float knob_r = 7.0f;
        const float value_w = 40.0f;

        const ImVec2 pos = window->DC.CursorPos;
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const float label_w = label_size.x + 8.0f;
        const float slider_w = avail_w - label_w - value_w - 8.0f;

        const ImRect total_bb( pos, ImVec2( pos.x + avail_w, pos.y + height ) );
        const float slider_x = pos.x + label_w;
        const float slider_y = pos.y + height * 0.5f;

        ImGui::ItemSize( total_bb, 0.0f );
        if ( !ImGui::ItemAdd( total_bb, id ) )
            return false;

        bool hovered, held;
        ImGui::ButtonBehavior( ImRect( slider_x, pos.y, slider_x + slider_w, pos.y + height ), id, &hovered, &held );

        if ( held )
        {
            float t = ImClamp( ( g.IO.MousePos.x - slider_x ) / slider_w, 0.0f, 1.0f );
            *v = ImLerp( v_min, v_max, t );
            ImGui::MarkItemEdited( id );
        }

        float t = ( *v - v_min ) / ( v_max - v_min );
        float alpha = ImGui::GetStyle().Alpha;
        ImDrawList* draw = window->DrawList;

        // label
        draw->AddText( ImVec2( pos.x, pos.y + ( height - label_size.y ) * 0.5f ),
            ImColor( 0.65f, 0.65f, 0.70f, alpha ), label );

        // track background
        draw->AddRectFilled(
            ImVec2( slider_x, slider_y - track_h * 0.5f ),
            ImVec2( slider_x + slider_w, slider_y + track_h * 0.5f ),
            ImColor( 0.20f, 0.20f, 0.22f, alpha ), track_h * 0.5f );

        // track filled
        draw->AddRectFilled(
            ImVec2( slider_x, slider_y - track_h * 0.5f ),
            ImVec2( slider_x + slider_w * t, slider_y + track_h * 0.5f ),
            ImColor( 0.63f, 0.47f, 0.92f, 0.7f * alpha ), track_h * 0.5f );

        // knob
        float knob_x = slider_x + slider_w * t;
        draw->AddCircleFilled( ImVec2( knob_x, slider_y ), knob_r,
            ImColor( 0.85f, 0.85f, 0.88f, alpha ) );
        draw->AddCircle( ImVec2( knob_x, slider_y ), knob_r,
            ImColor( 0.63f, 0.47f, 0.92f, 0.5f * alpha ), 0, 1.5f );

        // value text
        char val_buf[32];
        snprintf( val_buf, sizeof( val_buf ), fmt, *v );
        ImVec2 val_size = ImGui::CalcTextSize( val_buf );
        draw->AddRectFilled(
            ImVec2( slider_x + slider_w + 8, pos.y + 2 ),
            ImVec2( slider_x + slider_w + 8 + value_w, pos.y + height - 2 ),
            ImColor( 0.15f, 0.15f, 0.17f, alpha ), 4.0f );
        draw->AddText(
            ImVec2( slider_x + slider_w + 8 + ( value_w - val_size.x ) * 0.5f, pos.y + ( height - val_size.y ) * 0.5f ),
            ImColor( 0.78f, 0.78f, 0.82f, alpha ), val_buf );

        return held;
    }

    // int version
    inline bool SliderInt( const char* label, int* v, int v_min, int v_max, const char* fmt = "%d" )
    {
        float fv = (float)*v;
        bool changed = Slider( label, &fv, (float)v_min, (float)v_max, fmt );
        *v = (int)fv;
        return changed;
    }

    // section header
    inline void SectionHeader( const char* label )
    {
        ImGui::Spacing();
        ImGui::TextColored( ImVec4( 0.63f, 0.47f, 0.92f, 1.0f ), "%s", label );
        ImGui::Spacing();
    }
}
