#pragma once

#include "../../Common/d3dUtil.h"

// static Brush Param
const UINT					mBrushThreadNum = 3;

static UINT					mThreadIDX = 0;

static float				mClickedPosX[mBrushThreadNum], mClickedPosY[mBrushThreadNum];
static DirectX::XMFLOAT2	mBrushPosition[mBrushThreadNum];
static DirectX::XMFLOAT2	mBrushOrigin[mBrushThreadNum];
static float				mScreenWidth[mBrushThreadNum], mScreenHeight[mBrushThreadNum];

static HANDLE				mBrushEvent[mBrushThreadNum];

class DrawTexture
{
public:
	DrawTexture(
		ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format
	);

	DrawTexture(const DrawTexture& rhs) = delete;
	DrawTexture& operator=(const DrawTexture& rhs) = delete;
	~DrawTexture() = default;

	CD3DX12_GPU_DESCRIPTOR_HANDLE OutputSrv();

	UINT DescriptorCount()const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
		UINT descriptorSize);

	void OnResize(UINT newWidth, UINT newHeight);

	void Execute(
		ID3D12GraphicsCommandList* cmdList,
		ID3D12RootSignature* rootSig,
		ID3D12PipelineState* pso
	);

	bool								isDirty = false;
	DirectX::XMFLOAT2					leftTop;
	DirectX::XMFLOAT2					rightBottom;
	DirectX::XMFLOAT2					Origin;
	std::array<DirectX::XMFLOAT2, mBrushThreadNum>	Position;
	DirectX::XMFLOAT4					Color;

private:
	void BuildDescriptors();
	void BuildResource();

private:
	ID3D12Device* md3dDevice = nullptr;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuUav;
	//CD3DX12_CPU_DESCRIPTOR_HANDLE mBrushShapeCpuSrv;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuUav;
	//CD3DX12_GPU_DESCRIPTOR_HANDLE mBrushShapeGpuSrv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mOutput = nullptr;
};