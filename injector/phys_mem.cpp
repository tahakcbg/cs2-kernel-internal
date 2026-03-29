#include "phys_mem.hpp"
#include <iostream>
#include <Psapi.h>
#include <TlHelp32.h>

#pragma comment(lib, "psapi.lib")

static uint64_t get_ntos_base( )
{
    LPVOID drivers[1024];
    DWORD needed = 0;
    if ( !EnumDeviceDrivers( drivers, sizeof( drivers ), &needed ) )
        return 0;
    return reinterpret_cast<uint64_t>( drivers[0] );
}

static uint64_t get_ntos_export_rva( const char* name )
{
    auto ntos = LoadLibraryExA( "ntoskrnl.exe", nullptr, DONT_RESOLVE_DLL_REFERENCES );
    if ( !ntos )
        return 0;

    auto addr = reinterpret_cast<uint64_t>( GetProcAddress( ntos, name ) );
    uint64_t rva = addr ? ( addr - reinterpret_cast<uint64_t>( ntos ) ) : 0;
    FreeLibrary( ntos );
    return rva;
}

static uint32_t get_pid_offset( )
{
    auto ntos = LoadLibraryExA( "ntoskrnl.exe", nullptr, DONT_RESOLVE_DLL_REFERENCES );
    if ( !ntos )
        return 0;

    auto func = reinterpret_cast<uint8_t*>( GetProcAddress( ntos, "PsGetProcessId" ) );
    uint32_t offset = 0;

    if ( func )
    {
        if ( func[0] == 0x48 && func[1] == 0x8B && func[2] == 0x81 )
            offset = *reinterpret_cast<uint32_t*>( func + 3 );
        else if ( func[0] == 0x48 && func[1] == 0x8B && func[2] == 0x41 )
            offset = func[3];
    }

    FreeLibrary( ntos );
    return offset;
}

uint64_t phys_mem::read_phys_u64( uint64_t phys_addr )
{
    uint64_t val = 0;
    m_drv.read_physical( phys_addr, &val, sizeof( val ) );
    return val;
}

uint64_t phys_mem::find_dtb( uint32_t pid )
{
    std::cout << "[phys_mem] finding DTB for PID " << pid << "..." << std::endl;

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof( ms );
    GlobalMemoryStatusEx( &ms );
    m_max_phys = ms.ullTotalPhys;
    std::cout << "[phys_mem] physical RAM ceiling: 0x" << std::hex << m_max_phys << std::dec << std::endl;

    auto ntos_base = get_ntos_base( );
    if ( !ntos_base )
    {
        std::cerr << "[phys_mem] could not get ntoskrnl base." << std::endl;
        return 0;
    }
    std::cout << "[phys_mem] ntoskrnl base: 0x" << std::hex << ntos_base << std::dec << std::endl;

    uint64_t system_cr3 = 0;

    for ( uint64_t pa = 0x1000; pa < 0x100000 && !system_cr3; pa += 0x1000 )
    {
        uint8_t page[0x1000];
        if ( !m_drv.read_physical( pa, page, sizeof( page ) ) )
            continue;

        if ( page[0] != 0xE9 )
            continue;

        std::cout << "[phys_mem] low stub at phys 0x" << std::hex << pa << std::dec << std::endl;

        std::cout << "[phys_mem] page-aligned QWORDs in stub:" << std::endl;
        for ( uint32_t off = 0x08; off < 0x200; off += 0x08 )
        {
            uint64_t val = *reinterpret_cast<uint64_t*>( page + off );
            if ( val && ( val & 0xFFF ) == 0 && val < 0x10000000ULL )
                std::cout << "  +0x" << std::hex << off << ": 0x" << val << std::dec << std::endl;
        }

        for ( uint32_t off = 0x08; off < 0x200; off += 0x08 )
        {
            uint64_t candidate = *reinterpret_cast<uint64_t*>( page + off );

            if ( !candidate || ( candidate & 0xFFF ) != 0 )
                continue;
            if ( candidate > 0x1000000ULL )  // 16MB
                continue;

            auto phys = translate( candidate, ntos_base );
            if ( !phys )
                continue;

            uint8_t mz[2]{};
            if ( !m_drv.read_physical( phys, mz, 2 ) )
                continue;

            if ( mz[0] == 'M' && mz[1] == 'Z' )
            {
                system_cr3 = candidate;
                std::cout << "[phys_mem] system CR3: 0x" << std::hex << candidate
                          << " (stub offset 0x" << off << ")" << std::dec << std::endl;
                break;
            }
        }
    }

    if ( !system_cr3 )
    {
        std::cerr << "[phys_mem] could not find system CR3 in low stub." << std::endl;
        return 0;
    }

    // resolve PsInitialSystemProcess -> System EPROCESS
    auto ps_initial_rva = get_ntos_export_rva( "PsInitialSystemProcess" );
    if ( !ps_initial_rva )
    {
        std::cerr << "[phys_mem] could not resolve PsInitialSystemProcess." << std::endl;
        return 0;
    }

    uint64_t system_eprocess = 0;
    if ( !read_virtual( system_cr3, ntos_base + ps_initial_rva, &system_eprocess, sizeof( system_eprocess ) ) )
    {
        std::cerr << "[phys_mem] could not read PsInitialSystemProcess." << std::endl;
        return 0;
    }
    std::cout << "[phys_mem] System EPROCESS: 0x" << std::hex << system_eprocess << std::dec << std::endl;

    // get EPROCESS offsets
    auto pid_offset = get_pid_offset( );
    if ( !pid_offset )
    {
        std::cerr << "[phys_mem] could not determine UniqueProcessId offset." << std::endl;
        return 0;
    }

    uint32_t links_offset = pid_offset + 8;
    constexpr uint32_t dtb_offset = 0x28;

    std::cout << "[phys_mem] offsets: PID=0x" << std::hex << pid_offset
              << " Links=0x" << links_offset
              << " DTB=0x" << dtb_offset << std::dec << std::endl;

    // walk EPROCESS linked list to find cs2 PID
    uint64_t current = system_eprocess;
    int count = 0;

    do
    {
        uint64_t proc_pid = 0;
        if ( !read_virtual( system_cr3, current + pid_offset, &proc_pid, sizeof( proc_pid ) ) )
            break;

        count++;

        if ( static_cast<uint32_t>( proc_pid ) == pid )
        {
            uint64_t dtb = 0;
            if ( !read_virtual( system_cr3, current + dtb_offset, &dtb, sizeof( dtb ) ) )
            {
                std::cerr << "[phys_mem] found EPROCESS but could not read DTB." << std::endl;
                return 0;
            }

            dtb &= ~0x3ULL;

            std::cout << "[phys_mem] found target EPROCESS: 0x" << std::hex << current
                      << ", DTB: 0x" << dtb << std::dec
                      << " (" << count << " processes checked)" << std::endl;
            return dtb;
        }

        uint64_t flink = 0;
        if ( !read_virtual( system_cr3, current + links_offset, &flink, sizeof( flink ) ) )
            break;

        current = flink - links_offset;

    } while ( current != system_eprocess && current != 0 && count < 1000 );

    std::cerr << "[phys_mem] PID " << pid << " not found (" << count << " processes checked)." << std::endl;
    return 0;
}

uint64_t phys_mem::translate( uint64_t dtb, uint64_t virtual_addr )
{
    const uint64_t pml4_idx = ( virtual_addr >> 39 ) & 0x1FF;
    const uint64_t pdpt_idx = ( virtual_addr >> 30 ) & 0x1FF;
    const uint64_t pd_idx   = ( virtual_addr >> 21 ) & 0x1FF;
    const uint64_t pt_idx   = ( virtual_addr >> 12 ) & 0x1FF;
    const uint64_t offset   = virtual_addr & 0xFFF;

    uint64_t pml4e = read_phys_u64( ( dtb & PAGE_MASK_4KB ) + pml4_idx * 8 );
    if ( !( pml4e & PAGE_PRESENT ) )
        return 0;

    uint64_t pdpte = read_phys_u64( ( pml4e & PAGE_MASK_4KB ) + pdpt_idx * 8 );
    if ( !( pdpte & PAGE_PRESENT ) )
        return 0;

    if ( pdpte & PAGE_LARGE )
        return ( pdpte & PAGE_MASK_1GB ) + ( virtual_addr & 0x3FFFFFFF );

    uint64_t pde = read_phys_u64( ( pdpte & PAGE_MASK_4KB ) + pd_idx * 8 );
    if ( !( pde & PAGE_PRESENT ) )
        return 0;

    if ( pde & PAGE_LARGE )
        return ( pde & PAGE_MASK_2MB ) + ( virtual_addr & 0x1FFFFF );

    uint64_t pte = read_phys_u64( ( pde & PAGE_MASK_4KB ) + pt_idx * 8 );
    if ( !( pte & PAGE_PRESENT ) )
        return 0;

    return ( pte & PAGE_MASK_4KB ) + offset;
}

bool phys_mem::read_virtual( uint64_t dtb, uint64_t virt_addr, void* buffer, size_t size )
{
    auto buf = static_cast<uint8_t*>( buffer );
    size_t bytes_read = 0;

    while ( bytes_read < size )
    {
        uint64_t current_va = virt_addr + bytes_read;
        uint64_t page_offset = current_va & 0xFFF;
        size_t chunk = min( size - bytes_read, 0x1000 - page_offset );

        uint64_t phys = translate( dtb, current_va );
        if ( !phys )
            return false;

        if ( !m_drv.read_physical( phys, buf + bytes_read, static_cast<uint32_t>( chunk ) ) )
            return false;

        bytes_read += chunk;
    }

    return true;
}

bool phys_mem::write_virtual( uint64_t dtb, uint64_t virt_addr, const void* buffer, size_t size )
{
    auto buf = static_cast<const uint8_t*>( buffer );
    size_t bytes_written = 0;

    while ( bytes_written < size )
    {
        uint64_t current_va = virt_addr + bytes_written;
        uint64_t page_offset = current_va & 0xFFF;
        size_t chunk = min( size - bytes_written, 0x1000 - page_offset );

        uint64_t phys = translate( dtb, current_va );
        if ( !phys )
            return false;

        if ( !m_drv.write_physical( phys, buf + bytes_written, static_cast<uint32_t>( chunk ) ) )
            return false;

        bytes_written += chunk;
    }

    return true;
}
