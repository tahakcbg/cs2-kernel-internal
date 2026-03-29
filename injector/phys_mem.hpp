#pragma once
#include "driver.hpp"
#include <cstdint>

// x64 paging structure constants
constexpr uint64_t PAGE_PRESENT    = 1 << 0;
constexpr uint64_t PAGE_LARGE      = 1 << 7;
constexpr uint64_t PAGE_MASK_4KB   = 0x000FFFFFFFFFF000ULL;
constexpr uint64_t PAGE_MASK_2MB   = 0x000FFFFFFFE00000ULL;
constexpr uint64_t PAGE_MASK_1GB   = 0x000FFFFFC0000000ULL;

class phys_mem
{
public:
    phys_mem( driver& drv ) : m_drv( drv ) {}

    uint64_t find_dtb( uint32_t pid );

    uint64_t translate( uint64_t dtb, uint64_t virtual_addr );

    bool read_virtual( uint64_t dtb, uint64_t virt_addr, void* buffer, size_t size );
    bool write_virtual( uint64_t dtb, uint64_t virt_addr, const void* buffer, size_t size );

    template<typename T>
    T read( uint64_t dtb, uint64_t virt_addr )
    {
        T val{};
        read_virtual( dtb, virt_addr, &val, sizeof( T ) );
        return val;
    }

private:
    uint64_t read_phys_u64( uint64_t phys_addr );

    driver& m_drv;
    uint64_t m_max_phys{ 0 };
};
