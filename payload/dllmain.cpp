#include <Windows.h>
#include "hooks.h"

BOOL APIENTRY DllMain( HMODULE hModule, DWORD reason, LPVOID )
{
    if ( reason == DLL_PROCESS_ATTACH )
    {
        auto thread = CreateThread( nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>( hooks::init ),
            hModule, 0, nullptr );
        if ( thread )
            CloseHandle( thread );
    }
    return TRUE;
}
