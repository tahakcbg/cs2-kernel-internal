#include "DX11BlurEffect.h"
#include <DirectXMath.h>
#include "imgui/imgui_internal.h"
#include <memory>
#include <array>

namespace blur_ps_x {
    #include "blur_x.h"
}
#undef g_main

namespace blur_ps_y {
    #include "blur_y.h"
}
#undef g_main

namespace blur_vs_quad {
    #include "blur_quad_vs.h"
}
#undef g_main


bool DX11BlurEffect::CreateShaders()
{
    HRESULT hr;

    hr = device->CreatePixelShader( blur_ps_x::g_main, sizeof( blur_ps_x::g_main ), nullptr, &blurShaderX );
    if ( FAILED( hr ) ) return false;

    hr = device->CreatePixelShader( blur_ps_y::g_main, sizeof( blur_ps_y::g_main ), nullptr, &blurShaderY );
    if ( FAILED( hr ) ) return false;

    return true;
}

bool DX11BlurEffect::CreateSamplerState()
{
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    return SUCCEEDED( device->CreateSamplerState( &samplerDesc, &samplerState ) );
}


bool DX11BlurEffect::CreateBlurTextures( int width, int height )
{
    auto create_texture_resources = [&]( ID3D11Texture2D** texture, ID3D11ShaderResourceView** srv, ID3D11RenderTargetView** rtv ) -> bool
    {
        if ( *texture ) ( *texture )->Release(); *texture = nullptr;
        if ( *srv ) ( *srv )->Release(); *srv = nullptr;
        if ( *rtv ) ( *rtv )->Release(); *rtv = nullptr;

        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = backbufferFormat;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device->CreateTexture2D( &textureDesc, nullptr, texture );
        if ( FAILED( hr ) ) return false;
        hr = device->CreateShaderResourceView( *texture, nullptr, srv );
        if ( FAILED( hr ) ) return false;
        hr = device->CreateRenderTargetView( *texture, nullptr, rtv );
        if ( FAILED( hr ) ) return false;
        return true;
    };

    if ( !create_texture_resources( &blurTextureX, &blurSRVX, &blurRTVX ) ) return false;
    if ( !create_texture_resources( &blurTextureY, &blurSRVY, &blurRTVY ) ) return false;
    if ( !create_texture_resources( &blurTexture, &blurSRV, &blurRTV ) ) return false;

    return true;
}


bool DX11BlurEffect::Initialize( ID3D11Device* dev, ID3D11DeviceContext* ctx )
{
    if ( !dev || !ctx ) return false;

    device = dev;
    context = ctx;

    if ( !CreateShaders() || !CreateSamplerState() || !CreateBlurTextures( 1, 1 ) || !CreateFullscreenQuadResources() )
        return false;

    isInitialized = true;
    return true;
}

void DX11BlurEffect::BeginBlur( IDXGISwapChain* swapChain )
{
    if ( !isInitialized || !swapChain ) return;

    ID3D11Texture2D* backbuffer = nullptr;
    if ( FAILED( swapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &backbuffer ) ) ) )
        return;

    D3D11_TEXTURE2D_DESC desc;
    backbuffer->GetDesc( &desc );

    if ( backbufferWidth != (int)desc.Width || backbufferHeight != (int)desc.Height || backbufferFormat != desc.Format )
    {
        backbufferFormat = desc.Format;
        if ( !CreateBlurTextures( desc.Width, desc.Height ) )
        {
            backbuffer->Release();
            return;
        }
        backbufferWidth = desc.Width;
        backbufferHeight = desc.Height;
    }

    context->OMGetRenderTargets( 1, &rtBackup, nullptr );
    context->CopyResource( blurTexture, backbuffer );

    backbuffer->Release();
}

void DX11BlurEffect::ApplyBlur( ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float radius, float rounding, ImDrawFlags flags )
{
    if ( !isInitialized ) return;

    struct BlurConstants { float pixelSize[4]; };

    ID3D11Buffer* constantBuffer = nullptr;
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof( BlurConstants );
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if ( FAILED( device->CreateBuffer( &desc, nullptr, &constantBuffer ) ) )
            return;
    }

    ID3D11RenderTargetView* rtvX[] = { blurRTVX };
    context->OMSetRenderTargets( 1, rtvX, nullptr );
    context->PSSetShader( blurShaderX, nullptr, 0 );
    context->PSSetConstantBuffers( 0, 1, &constantBuffer );
    context->PSSetSamplers( 0, 1, &samplerState );
    context->PSSetShaderResources( 0, 1, &blurSRV );

    {
        BlurConstants constants = { { 1.0f / backbufferWidth, 0.0f, 0.0f, 0.0f } };
        D3D11_MAPPED_SUBRESOURCE mapped;
        context->Map( constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
        memcpy( mapped.pData, &constants, sizeof( constants ) );
        context->Unmap( constantBuffer, 0 );
    }
    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>( backbufferWidth ), static_cast<float>( backbufferHeight ), 0.0f, 1.0f };
    context->RSSetViewports( 1, &viewport );
    DrawFullscreenQuad();

    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    context->PSSetShaderResources( 0, 1, nullSRV );
    ID3D11RenderTargetView* nullRTV[] = { nullptr };
    context->OMSetRenderTargets( 1, nullRTV, nullptr );

    ID3D11RenderTargetView* rtvY[] = { blurRTVY };
    context->OMSetRenderTargets( 1, rtvY, nullptr );
    context->PSSetShader( blurShaderY, nullptr, 0 );
    context->PSSetConstantBuffers( 0, 1, &constantBuffer );
    context->PSSetSamplers( 0, 1, &samplerState );
    context->PSSetShaderResources( 0, 1, &blurSRVX );

    {
        BlurConstants constants = { { 1.0f / backbufferHeight, 0.0f, 0.0f, 0.0f } };
        D3D11_MAPPED_SUBRESOURCE mapped;
        context->Map( constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
        memcpy( mapped.pData, &constants, sizeof( constants ) );
        context->Unmap( constantBuffer, 0 );
    }
    context->RSSetViewports( 1, &viewport );
    DrawFullscreenQuad();

    context->OMSetRenderTargets( 1, &rtBackup, nullptr );
    D3D11_VIEWPORT imguiViewport = { 0.0f, 0.0f, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0f, 1.0f };
    context->RSSetViewports( 1, &imguiViewport );

    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 uv0( pos.x / screenSize.x, pos.y / screenSize.y );
    ImVec2 uv1( ( pos.x + size.x ) / screenSize.x, ( pos.y + size.y ) / screenSize.y );

    drawList->AddImageRounded(
        reinterpret_cast<ImTextureID>( blurSRVY ),
        pos,
        ImVec2( pos.x + size.x, pos.y + size.y ),
        uv0, uv1,
        ImColor( ImVec4( 1.f, 1.f, 1.f, 1.f ) ),
        rounding, flags
    );

    if ( constantBuffer ) constantBuffer->Release();
}

void DX11BlurEffect::EndBlur()
{
    if ( !isInitialized ) return;

    if ( rtBackup )
    {
        context->OMSetRenderTargets( 1, &rtBackup, nullptr );
        rtBackup->Release();
        rtBackup = nullptr;
    }

    context->PSSetShader( nullptr, nullptr, 0 );
    ID3D11SamplerState* nullSampler = nullptr;
    context->PSSetSamplers( 0, 1, &nullSampler );
}


struct DX11BlurEffect::FullscreenQuadVertex { float position[2]; };

bool DX11BlurEffect::CreateFullscreenQuadResources()
{
    FullscreenQuadVertex vertices[] = {
        { -1.0f,  1.0f }, {  1.0f,  1.0f },
        { -1.0f, -1.0f }, {  1.0f, -1.0f }
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof( vertices );
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = { vertices, 0, 0 };
    if ( FAILED( device->CreateBuffer( &vbDesc, &vbData, &fullscreenQuadVertexBuffer ) ) ) return false;

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if ( FAILED( device->CreateVertexShader( blur_vs_quad::g_main, sizeof( blur_vs_quad::g_main ), nullptr, &fullscreenQuadVertexShader ) ) )
        return false;

    if ( FAILED( device->CreateInputLayout( layout, 1, blur_vs_quad::g_main, sizeof( blur_vs_quad::g_main ), &fullscreenQuadInputLayout ) ) )
        return false;

    return true;
}


void DX11BlurEffect::DrawFullscreenQuad()
{
    if ( !fullscreenQuadVertexBuffer || !fullscreenQuadVertexShader ) return;

    UINT stride = sizeof( FullscreenQuadVertex );
    UINT offset = 0;

    context->IASetVertexBuffers( 0, 1, &fullscreenQuadVertexBuffer, &stride, &offset );
    context->IASetInputLayout( fullscreenQuadInputLayout );
    context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
    context->VSSetShader( fullscreenQuadVertexShader, nullptr, 0 );
    context->Draw( 4, 0 );
}


DX11BlurEffect::~DX11BlurEffect()
{
    if ( blurShaderX ) blurShaderX->Release();
    if ( blurShaderY ) blurShaderY->Release();
    if ( samplerState ) samplerState->Release();
    if ( blurTexture ) blurTexture->Release();
    if ( blurSRV ) blurSRV->Release();
    if ( blurRTV ) blurRTV->Release();
    if ( blurTextureX ) blurTextureX->Release();
    if ( blurSRVX ) blurSRVX->Release();
    if ( blurRTVX ) blurRTVX->Release();
    if ( blurTextureY ) blurTextureY->Release();
    if ( blurSRVY ) blurSRVY->Release();
    if ( blurRTVY ) blurRTVY->Release();
    if ( fullscreenQuadVertexBuffer ) fullscreenQuadVertexBuffer->Release();
    if ( fullscreenQuadVertexShader ) fullscreenQuadVertexShader->Release();
    if ( fullscreenQuadInputLayout ) fullscreenQuadInputLayout->Release();
}
