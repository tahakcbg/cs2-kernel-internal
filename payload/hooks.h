#pragma once
#include <atomic>

namespace hooks
{
    inline std::atomic<bool> running{ true };

    void init( HMODULE self );
}
