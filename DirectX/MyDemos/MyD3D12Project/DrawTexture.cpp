#include "DrawTexture.h"

DrawTexture::DrawTexture(	ID3D12Device* device,
							UINT width, 
							UINT height,
							DXGI_FORMAT format)
{
	md3dDevice	= device;

	mWidth		= width;
	mHeight		= height;
	mFormat		= format;

	Position	= DirectX::XMFLOAT2(0.0f, 0.0f);
	Color		= DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	BuildResource();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DrawTexture::OutputSrv()
{
	return mhGpuSrv;
}

UINT DrawTexture::DescriptorCount()const
{
	return 2;
}

void DrawTexture::BuildDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
	UINT descriptorSize
)
{
	mhCpuSrv = hCpuDescriptor;
	mhCpuUav = hCpuDescriptor.Offset(1, descriptorSize);
	mhGpuSrv = hGpuDescriptor;
	mhGpuUav = hGpuDescriptor.Offset(1, descriptorSize);

	BuildDescriptors();
}

void DrawTexture::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResource();

		BuildDescriptors();
	}
}

void DrawTexture::Execute(
	ID3D12GraphicsCommandList* cmdList,
	ID3D12RootSignature* rootSig,
	ID3D12PipelineState* pso
)
{
	/*std::vector<DirectX::XMFLOAT4> test(2);*/
	std::vector<float> test(8);

	// PASS
	test[0] = Position.x;
	test[1] = Position.y;
	test[2] = 0.0f;
	test[3] = 0.0f;

	test[4] = Color.x;
	test[5] = Color.y;
	test[6] = Color.z;
	test[7] = Color.w;

	cmdList->SetComputeRootSignature(rootSig);
	cmdList->SetPipelineState(pso);

	cmdList->SetComputeRoot32BitConstants(
		0, 
		12, 
		test.data(), 
		1
	);

	// Sobel Signature의 2번째 디스크립터에 RW가 가능한 UAV를 삽입.
	cmdList->SetComputeRootDescriptorTable(
		2, 
		mhGpuUav
	);

	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mOutput.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		)
	);

	UINT numGroupsX = (UINT)ceilf(mWidth / 16.0f);
	UINT numGroupsY = (UINT)ceilf(mHeight / 16.0f);

	cmdList->Dispatch(numGroupsX, numGroupsY, 1);

	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mOutput.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_GENERIC_READ
		)
	);
}

void DrawTexture::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = mFormat;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	md3dDevice->CreateShaderResourceView(mOutput.Get(), &srvDesc, mhCpuSrv);
	md3dDevice->CreateUnorderedAccessView(mOutput.Get(), nullptr, &uavDesc, mhCpuUav);
}

void DrawTexture::BuildResource()
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, 
		IID_PPV_ARGS(&mOutput)
	));
}