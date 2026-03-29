#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include "memory.h"

namespace schema
{
    bool init();

    uint32_t get( const char* module, const char* class_name, const char* field_name );

    inline uint32_t client( const char* class_name, const char* field_name )
    {
        return get( "client.dll", class_name, field_name );
    }
}
