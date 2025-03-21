#pragma once
#include "Renderer.h"
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class DirectX12Renderer : public Renderer
{
	DECLARE_SINGLETON(DirectX12Renderer)
public:
	void Initialize() override;
	void Render() override;
	void Shutdown() override;

private:
	// Ресурсы DirectX12
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> commandQueue;
};

