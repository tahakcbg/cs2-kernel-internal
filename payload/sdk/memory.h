#pragma once
#include <cstdint>
#include <Windows.h>
#include <TlHelp32.h>

namespace mem
{
    template<typename T>
    inline T read( uintptr_t addr )
    {
        if ( !addr ) return {};
        return *reinterpret_cast<T*>( addr );
    }

    template<typename T>
    inline void write( uintptr_t addr, const T& val )
    {
        if ( !addr ) return;
        *reinterpret_cast<T*>( addr ) = val;
    }

    inline uintptr_t get_module( const wchar_t* name )
    {
        return reinterpret_cast<uintptr_t>( GetModuleHandleW( name ) );
    }
}
