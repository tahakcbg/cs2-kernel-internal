#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

// function pointers for usermode driver DLL
using fn_map_buffer   = void* ( __stdcall* )( uint64_t phys_addr, uint32_t size );
using fn_unmap_buffer = void  ( __stdcall* )( void* mapped_va );

class driver
{
public:
    bool load( const std::wstring& driver_path, const std::wstring& dll_path );
    void unload( );

    void* map_physical( uint64_t phys_addr, uint32_t size );
    void unmap_physical( void* mapped_va, uint32_t size );

    bool read_physical( uint64_t phys_addr, void* buffer, uint32_t size );
    bool write_physical( uint64_t phys_addr, const void* buffer, uint32_t size );

    bool is_loaded( ) const { return m_loaded; }

private:
    bool m_loaded{ false };
    HMODULE m_dll{ nullptr };
    fn_map_buffer   m_map{ nullptr };
    fn_unmap_buffer m_unmap{ nullptr };
    std::wstring m_service_name{ L"drvsvc" };
};
