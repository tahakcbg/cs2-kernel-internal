#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include "imgui/imgui_internal.h"

class DX11BlurEffect
{
public:
    DX11BlurEffect() = default;
    ~DX11BlurEffect();

    bool Initialize( ID3D11Device* dev, ID3D11DeviceContext* ctx );
    void BeginBlur( IDXGISwapChain* swapChain );
    void ApplyBlur( ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float radius, float rounding = 0.f, ImDrawFlags flags = 0 );
    void EndBlur();

private:
    bool CreateShaders();
    bool CreateSamplerState();
    bool CreateBlurTextures( int width, int height );
    bool CreateFullscreenQuadResources();
    void DrawFullscreenQuad();

    struct FullscreenQuadVertex;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    ID3D11PixelShader* blurShaderX = nullptr;
    ID3D11PixelShader* blurShaderY = nullptr;
    ID3D11SamplerState* samplerState = nullptr;

    ID3D11Texture2D* blurTexture = nullptr;
    ID3D11ShaderResourceView* blurSRV = nullptr;
    ID3D11RenderTargetView* blurRTV = nullptr;

    ID3D11Texture2D* blurTextureX = nullptr;
    ID3D11ShaderResourceView* blurSRVX = nullptr;
    ID3D11RenderTargetView* blurRTVX = nullptr;

    ID3D11Texture2D* blurTextureY = nullptr;
    ID3D11ShaderResourceView* blurSRVY = nullptr;
    ID3D11RenderTargetView* blurRTVY = nullptr;

    ID3D11Buffer* fullscreenQuadVertexBuffer = nullptr;
    ID3D11VertexShader* fullscreenQuadVertexShader = nullptr;
    ID3D11InputLayout* fullscreenQuadInputLayout = nullptr;

    ID3D11RenderTargetView* rtBackup = nullptr;
    int backbufferWidth = 0;
    int backbufferHeight = 0;
    DXGI_FORMAT backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    bool isInitialized = false;
};

inline DX11BlurEffect blurEffect = DX11BlurEffect();
