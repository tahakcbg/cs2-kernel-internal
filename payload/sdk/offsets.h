#pragma once
#include <cstdint>
#include "schema.h"
#include "memory.h"

// only globals need manual updates
//
// update globals from: https://github.com/a2x/cs2-dumper (offsets.h)

namespace offsets
{
    namespace globals
    {
        constexpr uintptr_t dwEntityList            = 0x24B0258;
        constexpr uintptr_t dwLocalPlayerPawn        = 0x206A9E0;
        constexpr uintptr_t dwLocalPlayerController  = 0x22F5028;
        constexpr uintptr_t dwViewMatrix             = 0x2310F10;
    }

    namespace base_entity
    {
        inline uint32_t m_iHealth()          { return schema::client( "C_BaseEntity", "m_iHealth" ); }
        inline uint32_t m_iTeamNum()         { return schema::client( "C_BaseEntity", "m_iTeamNum" ); }
        inline uint32_t m_pGameSceneNode()   { return schema::client( "C_BaseEntity", "m_pGameSceneNode" ); }
        inline uint32_t m_fFlags()           { return schema::client( "C_BaseEntity", "m_fFlags" ); }
        inline uint32_t m_vOldOrigin()       { return schema::client( "C_BaseEntity", "m_vOldOrigin" ); }
    }

    namespace model_entity
    {
        inline uint32_t m_vecViewOffset()    { return schema::client( "C_BaseModelEntity", "m_vecViewOffset" ); }
    }

    namespace player_pawn
    {
        inline uint32_t m_iShotsFired()      { return schema::client( "C_CSPlayerPawn", "m_iShotsFired" ); }
        inline uint32_t m_aimPunchAngle()    { return schema::client( "C_CSPlayerPawn", "m_aimPunchAngle" ); }
        inline uint32_t m_entitySpottedState() { return schema::client( "C_CSPlayerPawn", "m_entitySpottedState" ); }
    }

    namespace player_controller
    {
        inline uint32_t m_iszPlayerName()    { return schema::client( "CCSPlayerController", "m_sSanitizedPlayerName" ); }
        inline uint32_t m_hPlayerPawn()      { return schema::client( "CCSPlayerController", "m_hPlayerPawn" ); }
        inline uint32_t m_bPawnIsAlive()     { return schema::client( "CCSPlayerController", "m_bPawnIsAlive" ); }
    }

    namespace scene_node
    {
        inline uint32_t m_vecAbsOrigin()     { return schema::client( "CGameSceneNode", "m_vecAbsOrigin" ); }
    }

    namespace skeleton
    {
        inline uint32_t m_modelState()       { return schema::client( "CSkeletonInstance", "m_modelState" ); }
    }
}
