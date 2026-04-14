#include "Renderer/CircleRenderer.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    constexpr uint32_t kCircleSegments = 64;
    constexpr float kCircleRadius = 0.35f;

    struct Vertex
    {
        float position[3];
        float color[4];
    };

    std::string GetShaderPath()
    {
        wchar_t modulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
        {
            return {};
        }

        const std::filesystem::path shaderPath = std::filesystem::path(modulePath).parent_path() / L".." / L".." / L"Shaders" / L"Circle2D.hlsl";
        return shaderPath.lexically_normal().string();
    }
}

CircleRenderer::~CircleRenderer()
{
    Shutdown();
}

bool CircleRenderer::Initialize(ID3D11Device* device)
{
    if (!device)
    {
        return false;
    }

    if (!CreateShaders(device))
    {
        Shutdown();
        return false;
    }

    if (!CreateVertexBuffer(device))
    {
        Shutdown();
        return false;
    }

    if (!CreateConstantBuffer(device))
    {
        Shutdown();
        return false;
    }

    return true;
}

void CircleRenderer::Shutdown()
{
    if (constantBuffer_)
    {
        constantBuffer_->Release();
        constantBuffer_ = nullptr;
    }
    if (vertexBuffer_)
    {
        vertexBuffer_->Release();
        vertexBuffer_ = nullptr;
    }
    if (inputLayout_)
    {
        inputLayout_->Release();
        inputLayout_ = nullptr;
    }
    if (pixelShader_)
    {
        pixelShader_->Release();
        pixelShader_ = nullptr;
    }
    if (vertexShader_)
    {
        vertexShader_->Release();
        vertexShader_ = nullptr;
    }

    vertexCount_ = 0;
}

void CircleRenderer::Render(ID3D11DeviceContext* deviceContext, float viewportWidth, float viewportHeight) const
{
    if (!deviceContext || !vertexShader_ || !pixelShader_ || !inputLayout_ || !vertexBuffer_ || !constantBuffer_ || vertexCount_ == 0 || viewportWidth <= 0.0f || viewportHeight <= 0.0f)
    {
        return;
    }

    CircleConstants constants = {};
    constants.aspectScaleX = viewportHeight / viewportWidth;
    deviceContext->UpdateSubresource(const_cast<ID3D11Buffer*>(constantBuffer_), 0, nullptr, &constants, 0, 0);

    constexpr UINT stride = sizeof(Vertex);
    constexpr UINT offset = 0;
    deviceContext->IASetInputLayout(inputLayout_);
    deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->VSSetConstantBuffers(0, 1, &constantBuffer_);
    deviceContext->VSSetShader(vertexShader_, nullptr, 0);
    deviceContext->PSSetShader(pixelShader_, nullptr, 0);
    deviceContext->Draw(vertexCount_, 0);
}

bool CircleRenderer::CreateShaders(ID3D11Device* device)
{
    const std::string shaderPath = GetShaderPath();
    if (shaderPath.empty())
    {
        return false;
    }

    ID3DBlob* vertexShaderBlob = nullptr;
    ID3DBlob* pixelShaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    const std::wstring shaderPathWide = std::filesystem::path(shaderPath).wstring();

    HRESULT result = D3DCompileFromFile(
        shaderPathWide.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "VSMain",
        "vs_5_0",
        0,
        0,
        &vertexShaderBlob,
        &errorBlob);
    if (FAILED(result))
    {
        if (errorBlob)
        {
            errorBlob->Release();
        }
        return false;
    }

    result = D3DCompileFromFile(
        shaderPathWide.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PSMain",
        "ps_5_0",
        0,
        0,
        &pixelShaderBlob,
        &errorBlob);
    if (FAILED(result))
    {
        if (errorBlob)
        {
            errorBlob->Release();
        }
        if (vertexShaderBlob)
        {
            vertexShaderBlob->Release();
        }
        return false;
    }

    result = device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, &vertexShader_);
    if (FAILED(result))
    {
        vertexShaderBlob->Release();
        pixelShaderBlob->Release();
        return false;
    }

    result = device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &pixelShader_);
    if (FAILED(result))
    {
        vertexShaderBlob->Release();
        pixelShaderBlob->Release();
        return false;
    }

    const std::array<D3D11_INPUT_ELEMENT_DESC, 2> inputElements =
    {
        D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        D3D11_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    result = device->CreateInputLayout(
        inputElements.data(),
        static_cast<UINT>(inputElements.size()),
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        &inputLayout_);

    vertexShaderBlob->Release();
    pixelShaderBlob->Release();

    return SUCCEEDED(result);
}

bool CircleRenderer::CreateConstantBuffer(ID3D11Device* device)
{
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(CircleConstants);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    return SUCCEEDED(device->CreateBuffer(&bufferDesc, nullptr, &constantBuffer_));
}

bool CircleRenderer::CreateVertexBuffer(ID3D11Device* device)
{
    std::vector<Vertex> vertices;
    vertices.reserve(kCircleSegments * 3);

    const Vertex center = { { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.65f, 1.0f, 1.0f } };
    constexpr float pi = 3.1415926535f;

    for (uint32_t segment = 0; segment < kCircleSegments; ++segment)
    {
        const float angle0 = (2.0f * pi * static_cast<float>(segment)) / static_cast<float>(kCircleSegments);
        const float angle1 = (2.0f * pi * static_cast<float>(segment + 1)) / static_cast<float>(kCircleSegments);

        const Vertex outer0 = { { static_cast<float>(std::cos(angle0)) * kCircleRadius, static_cast<float>(std::sin(angle0)) * kCircleRadius, 0.0f }, { 0.25f, 0.65f, 1.0f, 1.0f } };
        const Vertex outer1 = { { static_cast<float>(std::cos(angle1)) * kCircleRadius, static_cast<float>(std::sin(angle1)) * kCircleRadius, 0.0f }, { 0.25f, 0.65f, 1.0f, 1.0f } };

        vertices.push_back(center);
        vertices.push_back(outer1);
        vertices.push_back(outer0);
    }

    vertexCount_ = static_cast<UINT>(vertices.size());

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = vertices.data();

    return SUCCEEDED(device->CreateBuffer(&bufferDesc, &initialData, &vertexBuffer_));
}
