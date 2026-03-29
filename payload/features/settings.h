#pragma once


namespace settings
{
    namespace aimbot
    {
        inline bool enabled = false;
        inline bool silent = false;
        inline bool autowall = false;
        inline bool autofire = false;
        inline int fov = 60;
        inline int smooth = 5;
        inline int hitchance = 55;
    }

    namespace visuals
    {
        inline bool box_esp = false;
        inline bool name_esp = false;
        inline bool health_bar = false;
        inline bool snaplines = false;
        inline bool skeleton = false;
        inline bool glow = false;
        inline bool chams = false;
        inline bool chams_xqz = false;
    }

    namespace world
    {
        inline bool night_mode = false;
        inline bool no_flash = false;
        inline bool no_smoke = false;
        inline float fov_override = 90.0f;
    }

    namespace movement
    {
        inline bool bhop = false;
        inline bool autostrafe = false;
        inline bool edge_jump = false;
    }
}
