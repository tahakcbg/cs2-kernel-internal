#include "driver.hpp"
#include <iostream>

bool driver::load( const std::wstring& driver_path, const std::wstring& dll_path )
{
    // load kernel driver via service manager
    auto scm = OpenSCManagerW( nullptr, nullptr, SC_MANAGER_ALL_ACCESS );
    if ( !scm )
    {
        std::wcerr << L"[driver] OpenSCManager failed: " << GetLastError( ) << std::endl;
        return false;
    }

    auto service = OpenServiceW( scm, m_service_name.c_str( ), SERVICE_ALL_ACCESS );
    if ( !service )
    {
        service = CreateServiceW(
            scm, m_service_name.c_str( ), m_service_name.c_str( ),
            SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL, driver_path.c_str( ),
            nullptr, nullptr, nullptr, nullptr, nullptr
        );
    }

    if ( !service )
    {
        std::wcerr << L"[driver] CreateService failed: " << GetLastError( ) << std::endl;
        CloseServiceHandle( scm );
        return false;
    }

    if ( !StartServiceW( service, 0, nullptr ) )
    {
        auto err = GetLastError( );
        if ( err != ERROR_SERVICE_ALREADY_RUNNING )
        {
            std::wcerr << L"[driver] StartService failed: " << err << std::endl;
            CloseServiceHandle( service );
            CloseServiceHandle( scm );
            return false;
        }
    }

    CloseServiceHandle( service );
    CloseServiceHandle( scm );
    std::wcout << L"[driver] kernel driver loaded." << std::endl;

    // load usermode DLL
    m_dll = LoadLibraryW( dll_path.c_str( ) );
    if ( !m_dll )
    {
        std::wcerr << L"[driver] LoadLibrary failed: " << GetLastError( ) << std::endl;
        return false;
    }

    m_map   = reinterpret_cast<fn_map_buffer>( GetProcAddress( m_dll, "CorMemMapBuffer" ) );
    m_unmap = reinterpret_cast<fn_unmap_buffer>( GetProcAddress( m_dll, "CorMemUnmapBuffer" ) );

    if ( !m_map || !m_unmap )
    {
        std::wcerr << L"[driver] failed to resolve DLL exports." << std::endl;
        FreeLibrary( m_dll );
        m_dll = nullptr;
        return false;
    }

    m_loaded = true;
    std::wcout << L"[driver] usermode DLL loaded. ready." << std::endl;
    return true;
}

void driver::unload( )
{
    if ( m_dll )
    {
        FreeLibrary( m_dll );
        m_dll = nullptr;
    }

    auto scm = OpenSCManagerW( nullptr, nullptr, SC_MANAGER_ALL_ACCESS );
    if ( !scm ) return;

    auto service = OpenServiceW( scm, m_service_name.c_str( ), SERVICE_ALL_ACCESS );
    if ( service )
    {
        SERVICE_STATUS status{};
        ControlService( service, SERVICE_CONTROL_STOP, &status );
        DeleteService( service );
        CloseServiceHandle( service );
    }

    CloseServiceHandle( scm );
    m_loaded = false;
    std::wcout << L"[driver] unloaded." << std::endl;
}

void* driver::map_physical( uint64_t phys_addr, uint32_t size )
{
    if ( !m_map )
        return nullptr;
    return m_map( phys_addr, size );
}

void driver::unmap_physical( void* mapped_va, uint32_t /* size */ )
{
    if ( m_unmap && mapped_va )
        m_unmap( mapped_va );
}

bool driver::read_physical( uint64_t phys_addr, void* buffer, uint32_t size )
{
    uint64_t page_base   = phys_addr & ~0xFFFULL;
    uint32_t page_offset = static_cast<uint32_t>( phys_addr & 0xFFF );
    uint32_t map_size    = ( page_offset + size + 0xFFF ) & ~0xFFFU;

    auto mapped = map_physical( page_base, map_size );
    if ( !mapped )
        return false;

    bool ok = true;
    __try
    {
        memcpy( buffer, static_cast<uint8_t*>( mapped ) + page_offset, size );
    }
    __except ( EXCEPTION_EXECUTE_HANDLER )
    {
        ok = false;
    }

    unmap_physical( mapped, map_size );
    return ok;
}

bool driver::write_physical( uint64_t phys_addr, const void* buffer, uint32_t size )
{
    uint64_t page_base   = phys_addr & ~0xFFFULL;
    uint32_t page_offset = static_cast<uint32_t>( phys_addr & 0xFFF );
    uint32_t map_size    = ( page_offset + size + 0xFFF ) & ~0xFFFU;

    auto mapped = map_physical( page_base, map_size );
    if ( !mapped )
        return false;

    bool ok = true;
    __try
    {
        memcpy( static_cast<uint8_t*>( mapped ) + page_offset, buffer, size );
    }
    __except ( EXCEPTION_EXECUTE_HANDLER )
    {
        ok = false;
    }

    unmap_physical( mapped, map_size );
    return ok;
}
