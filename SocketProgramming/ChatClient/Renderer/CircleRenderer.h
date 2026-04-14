#pragma once

#include <d3d11.h>

class CircleRenderer
{
public:
    CircleRenderer() = default;
    ~CircleRenderer();

    bool Initialize(ID3D11Device* device);
    void Shutdown();
    void Render(ID3D11DeviceContext* deviceContext, float viewportWidth, float viewportHeight) const;

private:
    struct CircleConstants
    {
        float aspectScaleX = 1.0f;
        float padding[3] = {};
    };

    bool CreateShaders(ID3D11Device* device);
    bool CreateVertexBuffer(ID3D11Device* device);
    bool CreateConstantBuffer(ID3D11Device* device);

private:
    ID3D11VertexShader* vertexShader_ = nullptr;
    ID3D11PixelShader* pixelShader_ = nullptr;
    ID3D11InputLayout* inputLayout_ = nullptr;
    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* constantBuffer_ = nullptr;
    UINT vertexCount_ = 0;
};
