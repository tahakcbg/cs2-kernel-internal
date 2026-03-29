#pragma once
#include <cstdint>

struct vec3
{
    float x, y, z;

    vec3 operator+( const vec3& o ) const { return { x + o.x, y + o.y, z + o.z }; }
    vec3 operator-( const vec3& o ) const { return { x - o.x, y - o.y, z - o.z }; }
    vec3 operator*( float s ) const { return { x * s, y * s, z * s }; }
    float length() const;
};

struct vec2
{
    float x, y;
};

struct view_matrix_t
{
    float m[4][4];
};

struct bone_data
{
    float x, y, z;
    float pad;
};

// entity list helpers
struct CEntityInstance;
struct CEntityIdentity
{
    CEntityInstance* entity;       // 0x00
    char pad0[0x08];              // 0x08
    int32_t handle;               // 0x10
    char pad1[0x04];              // 0x14
    const char* designerName;     // 0x18
};

struct CEntityIdentityEx
{
    char pad[0x10];
    CEntityIdentity* identities[512];
};
