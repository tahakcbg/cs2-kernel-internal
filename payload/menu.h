#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include "imgui/imgui.h"
#include "DX11BlurEffect.h"
#include "widgets.h"
#include "features/settings.h"

namespace menu
{
    inline bool open = true;
    inline int active_tab = 0;

    void render( IDXGISwapChain* swap );
}
