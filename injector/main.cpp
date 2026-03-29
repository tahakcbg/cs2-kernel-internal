#include "driver.hpp"
#include "phys_mem.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <TlHelp32.h>

static uint32_t find_pid( const wchar_t* process_name )
{
    HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if ( snap == INVALID_HANDLE_VALUE )
        return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof( pe );

    if ( Process32FirstW( snap, &pe ) )
    {
        do
        {
            if ( _wcsicmp( pe.szExeFile, process_name ) == 0 )
            {
                CloseHandle( snap );
                return pe.th32ProcessID;
            }
        }
        while ( Process32NextW( snap, &pe ) );
    }

    CloseHandle( snap );
    return 0;
}

static std::vector<uint8_t> read_file( const std::wstring& path )
{
    HANDLE file = CreateFileW( path.c_str( ), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr );
    if ( file == INVALID_HANDLE_VALUE )
        return {};

    DWORD size = GetFileSize( file, nullptr );
    std::vector<uint8_t> data( size );
    DWORD read = 0;
    ReadFile( file, data.data( ), size, &read, nullptr );
    CloseHandle( file );
    return data;
}

static bool inject_dll( phys_mem& mem, uint64_t dtb, uint32_t pid, const std::wstring& dll_path )
{
    auto dll_data = read_file( dll_path );
    if ( dll_data.empty( ) )
    {
        std::cerr << "[inject] failed to read DLL file." << std::endl;
        return false;
    }

    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>( dll_data.data( ) );
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>( dll_data.data( ) + dos->e_lfanew );
    auto image_size = nt->OptionalHeader.SizeOfImage;

    std::cout << "[inject] DLL image size: 0x" << std::hex << image_size << std::dec << std::endl;

    HANDLE proc = OpenProcess( PROCESS_ALL_ACCESS, FALSE, pid );
    if ( !proc )
    {
        std::cerr << "[inject] OpenProcess failed: " << GetLastError( ) << std::endl;
        return false;
    }

    auto remote_base = reinterpret_cast<uint64_t>(
        VirtualAllocEx( proc, nullptr, image_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE )
    );

    if ( !remote_base )
    {
        std::cerr << "[inject] VirtualAllocEx failed: " << GetLastError( ) << std::endl;
        CloseHandle( proc );
        return false;
    }

    std::cout << "[inject] allocated at: 0x" << std::hex << remote_base << std::dec << std::endl;

    std::vector<uint8_t> image( image_size, 0 );
    memcpy( image.data( ), dll_data.data( ), nt->OptionalHeader.SizeOfHeaders );

    auto section = IMAGE_FIRST_SECTION( nt );
    for ( uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section )
    {
        if ( section->SizeOfRawData == 0 )
            continue;

        memcpy(
            image.data( ) + section->VirtualAddress,
            dll_data.data( ) + section->PointerToRawData,
            min( section->SizeOfRawData, section->Misc.VirtualSize )
        );
    }

    auto delta = static_cast<int64_t>( remote_base - nt->OptionalHeader.ImageBase );
    if ( delta != 0 )
    {
        auto reloc_dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if ( reloc_dir->Size > 0 )
        {
            auto reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>( image.data( ) + reloc_dir->VirtualAddress );
            while ( reloc->VirtualAddress && reloc->SizeOfBlock )
            {
                auto count = ( reloc->SizeOfBlock - sizeof( IMAGE_BASE_RELOCATION ) ) / sizeof( uint16_t );
                auto entries = reinterpret_cast<uint16_t*>( reloc + 1 );

                for ( size_t j = 0; j < count; ++j )
                {
                    auto type = entries[j] >> 12;
                    auto offset = entries[j] & 0xFFF;

                    if ( type == IMAGE_REL_BASED_DIR64 )
                    {
                        auto ptr = reinterpret_cast<uint64_t*>( image.data( ) + reloc->VirtualAddress + offset );
                        *ptr += delta;
                    }
                }

                reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                    reinterpret_cast<uint8_t*>( reloc ) + reloc->SizeOfBlock
                );
            }
        }
    }

    auto import_dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if ( import_dir->Size > 0 )
    {
        auto import_desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>( image.data( ) + import_dir->VirtualAddress );
        while ( import_desc->Name )
        {
            auto module_name = reinterpret_cast<const char*>( image.data( ) + import_desc->Name );
            auto module_handle = GetModuleHandleA( module_name );

            if ( !module_handle )
            {
                module_handle = LoadLibraryA( module_name );
            }

            if ( !module_handle )
            {
                std::cerr << "[inject] failed to resolve module: " << module_name << std::endl;
                import_desc++;
                continue;
            }

            // Get module base in target process
            uint64_t remote_module = 0;
            {
                HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, pid );
                if ( snap != INVALID_HANDLE_VALUE )
                {
                    MODULEENTRY32W me{};
                    me.dwSize = sizeof( me );
                    if ( Module32FirstW( snap, &me ) )
                    {
                        do
                        {
                            char narrow[260];
                            WideCharToMultiByte( CP_ACP, 0, me.szModule, -1, narrow, 260, nullptr, nullptr );
                            if ( _stricmp( narrow, module_name ) == 0 )
                            {
                                remote_module = reinterpret_cast<uint64_t>( me.modBaseAddr );
                                break;
                            }
                        }
                        while ( Module32NextW( snap, &me ) );
                    }
                    CloseHandle( snap );
                }
            }

            if ( !remote_module )
            {
                remote_module = reinterpret_cast<uint64_t>( module_handle );
            }

            auto thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
                image.data( ) + ( import_desc->OriginalFirstThunk ? import_desc->OriginalFirstThunk : import_desc->FirstThunk )
            );
            auto iat = reinterpret_cast<IMAGE_THUNK_DATA*>( image.data( ) + import_desc->FirstThunk );

            while ( thunk->u1.AddressOfData )
            {
                uint64_t func_addr = 0;

                if ( thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64 )
                {
                    auto proc_addr = GetProcAddress( module_handle, MAKEINTRESOURCEA( IMAGE_ORDINAL64( thunk->u1.Ordinal ) ) );
                    func_addr = remote_module + ( reinterpret_cast<uint64_t>( proc_addr ) - reinterpret_cast<uint64_t>( module_handle ) );
                }
                else
                {
                    auto import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>( image.data( ) + thunk->u1.AddressOfData );
                    auto proc_addr = GetProcAddress( module_handle, import->Name );
                    func_addr = remote_module + ( reinterpret_cast<uint64_t>( proc_addr ) - reinterpret_cast<uint64_t>( module_handle ) );
                }

                iat->u1.Function = func_addr;
                thunk++;
                iat++;
            }

            import_desc++;
        }
    }

    std::cout << "[inject] writing image via physical memory..." << std::endl;
    for ( uint32_t off = 0; off < image_size; off += 0x1000 )
    {
        uint32_t chunk = min( image_size - off, 0x1000u );

        uint8_t zero = 0;
        WriteProcessMemory( proc, reinterpret_cast<void*>( remote_base + off ), &zero, 1, nullptr );

        if ( !mem.write_virtual( dtb, remote_base + off, image.data( ) + off, chunk ) )
        {
            std::cerr << "[inject] physical write failed at offset 0x" << std::hex << off << std::dec << std::endl;
            CloseHandle( proc );
            return false;
        }
    }
    // verify: read back first 2 bytes via ReadProcessMemory and check for MZ
    uint8_t verify[2]{};
    ReadProcessMemory( proc, reinterpret_cast<void*>( remote_base ), verify, 2, nullptr );
    std::cout << "[inject] verify: " << ( char )verify[0] << ( char )verify[1]
              << ( ( verify[0] == 'M' && verify[1] == 'Z' ) ? " OK" : " CORRUPT!" ) << std::endl;

    auto entry = remote_base + nt->OptionalHeader.AddressOfEntryPoint;
    std::cout << "[inject] entry point at: 0x" << std::hex << entry << std::dec << std::endl;

    auto rtl_add_ft = reinterpret_cast<uint64_t>( GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "RtlAddFunctionTable" ) );
    if ( !rtl_add_ft )
        rtl_add_ft = reinterpret_cast<uint64_t>( GetProcAddress( GetModuleHandleA( "ntdll.dll" ), "RtlAddFunctionTable" ) );

    // get exception directory (.pdata)
    auto& exception_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    uint64_t pdata_addr = remote_base + exception_dir.VirtualAddress;
    uint32_t pdata_count = exception_dir.Size / sizeof( IMAGE_RUNTIME_FUNCTION_ENTRY );

    std::cout << "[inject] registering exception handlers (" << pdata_count << " entries)..." << std::endl;

    // shellcode: RtlAddFunctionTable then DllMain
    uint8_t shellcode[128] = {};
    int p = 0;

    // sub rsp, 28h
    shellcode[p++] = 0x48; shellcode[p++] = 0x83; shellcode[p++] = 0xEC; shellcode[p++] = 0x28;

    // RtlAddFunctionTable(pdata_addr, pdata_count, remote_base)
    if ( rtl_add_ft && exception_dir.Size > 0 )
    {
        // mov rcx, pdata_addr
        shellcode[p++] = 0x48; shellcode[p++] = 0xB9;
        memcpy( shellcode + p, &pdata_addr, 8 ); p += 8;
        // mov edx, pdata_count
        shellcode[p++] = 0xBA;
        memcpy( shellcode + p, &pdata_count, 4 ); p += 4;
        // mov r8, remote_base
        shellcode[p++] = 0x49; shellcode[p++] = 0xB8;
        memcpy( shellcode + p, &remote_base, 8 ); p += 8;
        // mov rax, RtlAddFunctionTable
        shellcode[p++] = 0x48; shellcode[p++] = 0xB8;
        memcpy( shellcode + p, &rtl_add_ft, 8 ); p += 8;
        // call rax
        shellcode[p++] = 0xFF; shellcode[p++] = 0xD0;
    }

    // DllMain(hModule, DLL_PROCESS_ATTACH, NULL)
    // mov rcx, remote_base
    shellcode[p++] = 0x48; shellcode[p++] = 0xB9;
    memcpy( shellcode + p, &remote_base, 8 ); p += 8;
    // mov edx, 1
    shellcode[p++] = 0xBA; shellcode[p++] = 0x01; shellcode[p++] = 0x00; shellcode[p++] = 0x00; shellcode[p++] = 0x00;
    // xor r8d, r8d
    shellcode[p++] = 0x45; shellcode[p++] = 0x33; shellcode[p++] = 0xC0;
    // mov rax, entry
    shellcode[p++] = 0x48; shellcode[p++] = 0xB8;
    memcpy( shellcode + p, &entry, 8 ); p += 8;
    // call rax
    shellcode[p++] = 0xFF; shellcode[p++] = 0xD0;

    // add rsp, 28h
    shellcode[p++] = 0x48; shellcode[p++] = 0x83; shellcode[p++] = 0xC4; shellcode[p++] = 0x28;
    // ret
    shellcode[p++] = 0xC3;

    // write shellcode to the end of the image
    auto shellcode_addr = remote_base + image_size - 0x100;
    mem.write_virtual( dtb, shellcode_addr, shellcode, p );

    std::cout << "[inject] shellcode at: 0x" << std::hex << shellcode_addr << std::dec << std::endl;

    auto thread = CreateRemoteThread(
        proc,
        nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>( shellcode_addr ),
        nullptr,
        0, nullptr
    );

    if ( !thread )
    {
        std::cerr << "[inject] CreateRemoteThread failed: " << GetLastError( ) << std::endl;
        CloseHandle( proc );
        return false;
    }

    WaitForSingleObject( thread, 5000 );
    CloseHandle( thread );
    CloseHandle( proc );

    std::cout << "[inject] injection complete!" << std::endl;
    return true;
}

static bool is_admin( )
{
    BOOL admin = FALSE;
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    PSID group = nullptr;
    if ( AllocateAndInitializeSid( &auth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &group ) )
    {
        CheckTokenMembership( nullptr, group, &admin );
        FreeSid( group );
    }
    return admin;
}

static void pause_exit( int code )
{
    std::cout << "\npress enter to exit..." << std::endl;
    std::cin.get( );
    exit( code );
}

int main( )
{
    std::cout << "=== cs2-imgui injector ===" << std::endl;

    if ( !is_admin( ) )
    {
        std::cerr << "[!] run as administrator! (right click -> run as admin)" << std::endl;
        pause_exit( 1 );
    }

    // find cs2
    auto pid = find_pid( L"cs2.exe" );
    if ( !pid )
    {
        std::cerr << "[!] cs2.exe not found. launch the game first." << std::endl;
        pause_exit( 1 );
    }
    std::cout << "[+] cs2.exe PID: " << pid << std::endl;

    // load driver — place your .sys and .dll next to injector.exe
    driver drv;
    auto base_dir = std::filesystem::absolute( L"." ).wstring( );
    auto sys_path = base_dir + L"\\driver.sys";
    auto dll_path_drv = base_dir + L"\\driver_um.dll";
    if ( !drv.load( sys_path, dll_path_drv ) )
    {
        std::cerr << "[!] failed to load driver." << std::endl;
        pause_exit( 1 );
    }

    // find DTB via low stub + EPROCESS walk
    phys_mem mem( drv );
    auto dtb = mem.find_dtb( pid );
    if ( !dtb )
    {
        std::cerr << "[!] failed to find DTB for cs2." << std::endl;
        drv.unload( );
        pause_exit( 1 );
    }

    // inject payload DLL
    auto dll_path = std::filesystem::absolute( L"..\\bin\\payload.dll" ).wstring( );
    if ( !std::filesystem::exists( dll_path ) )
    {
        std::cerr << "[!] payload.dll not found. build the payload project first." << std::endl;
        drv.unload( );
        pause_exit( 1 );
    }

    inject_dll( mem, dtb, pid, dll_path );

    std::cout << "\npress enter to unload driver and exit..." << std::endl;
    std::cin.get( );

    drv.unload( );
    return 0;
}
