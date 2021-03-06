//***************************************************************************************
// d3dUtil.h by Frank Luna (C) 2015 All Rights Reserved.
//
// General helper code.
//***************************************************************************************

#pragma once

#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <list>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include <memory>
#include <algorithm>
#include <crtdbg.h>

#include <thread>

#include <random>

#include "d3dx12.h"
#include "DDSTextureLoader.h"
#include "MathHelper.h"

#include "FreeImage.h"
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "FreeImaged.lib")

// extern const int gNumFrameResources;

inline void d3dSetDebugName(IDXGIObject* obj, const char* name)
{
    if(obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}
inline void d3dSetDebugName(ID3D12Device* obj, const char* name)
{
    if(obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}
inline void d3dSetDebugName(ID3D12DeviceChild* obj, const char* name)
{
    if(obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

class d3dUtil
{
public:

    static bool IsKeyDown(int vkeyCode);

    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        // Constant buffers must be a multiple of the minimum hardware
        // allocation size (usually 256 bytes).  So round up to nearest
        // multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256.
        // Example: Suppose byteSize = 300.
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        return (byteSize + 255) & ~255;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	// <??????> CommandList?? ?????????? ???? ???? ?????? ??!!!!!!!!! 
	static void UpdateDefaultBuffer(
		ID3D12GraphicsCommandList* cmdList,						// DefaultBuffer?? ???????? ???? ?? ??????????
		const void* initData,									// DefaultBuffer?? ???????? ???? ??????
		UINT64 byteSize,										// ???? ???????? ??????
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer,	// ?????? ????
		Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer	// GPU ????
	);

	static Microsoft::WRL::ComPtr<ID3D12Resource> storeDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer );

	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);

	static std::string getName(std::string Path);

	static std::string getDDSFormat(std::string Path);

	static std::wstring String2Wstring(std::string str);

	static DirectX::XMMATRIX toMatrix(DirectX::XMFLOAT4X4 mat);
	static DirectX::XMFLOAT4X4 toFloat4X4(DirectX::XMMATRIX mat);

	static FIBITMAP* loadImage(
		std::string path, 
		std::string name, 
		std::string format,
		_Out_ int& width,
		_Out_ int& height
	);
	static FIBITMAP* loadImage(
		std::string name,
		_Out_ int& width,
		_Out_ int& height
	);

};

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

// Defines a subrange of geometry in a MeshGeometry.  This is for when multiple
// geometries are stored in one vertex and index buffer.  It provides the offsets
// and data needed to draw a subset of geometry stores in the vertex and index 
// buffers so that we can implement the technique described by Figure 6.3.
struct SubmeshGeometry
{
	std::string name;
	std::string textureName;

	// Index ??????
	UINT StartIndexLocation = 0;
	// Vertex ??????
	UINT BaseVertexLocation = 0;

	// Index ??????
	UINT IndexSize = 0;
	//  Vertex ??????
	UINT VertexSize = 0;

    // Bounding box of the geometry defined by this submesh. 
    // This is used in later chapters of the book.
	DirectX::BoundingBox Bounds;
};

struct MeshGeometry
{
public:
	// Give it a name so we can look it up by name.
	std::string Name;

	UINT IndexSize = 0;

	UINT VertexBufferByteSize = 0;
	UINT IndexBufferByteSize = 0;

	// System memory copies.  Use Blobs because the vertex/index format can be generic.
	// It is up to the client to cast appropriately.  
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU  = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    // Data about the buffers.
	UINT VertexByteStride = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;

	// A MeshGeometry may store multiple geometries in one vertex/index buffer.
	// Use this container to define the Submesh geometries so we can draw
	// the Submeshes individually.

	// ???????????? ???????????? SubMesh?? ????
	UINT subMeshCount = 0;
	// ???????????? ???????????? ?????????? ???????? ???????????? ????
	std::vector<std::string> meshNames;
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}

	// We can free this memory after we finish upload to the GPU.
	void DisposeUploaders()
	{
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}

public:
	
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 view = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Invview = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowViewProjNDC = MathHelper::Identity4x4();

	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	//DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	//DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
};

typedef enum LightType
{
	DIR_LIGHTS  = 0,
	POINT_LIGHT = 1,
	SPOT_LIGHT  = 2
}LightType;

struct Light
{
	int mLightType;
    DirectX::XMFLOAT3 mStrength = { 0.5f, 0.5f, 0.5f };
    float mFalloffStart = 1.0f;                          // point/spot light only
    DirectX::XMFLOAT3 mDirection = { 0.0f, -1.0f, 0.0f };// directional/spot light only
    float mFalloffEnd = 10.0f;                           // point/spot light only
    DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };  // point/spot light only
    float mSpotPower = 64.0f;                            // spot light only

	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;

	DirectX::XMFLOAT3 mLightPosW = { 0.0f, 0.0f, 0.0f };

	DirectX::XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();

	DirectX::XMFLOAT4X4 ShadowViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowViewProjNDC = MathHelper::Identity4x4();

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
};

struct LightDataConstants
{
	Light data[5];
};

struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

// Simple struct to represent a material for our demos.  A production 3D engine
// would likely create a class hierarchy of Materials.
struct Material
{
public:
	// Unique material name for lookup.
	std::string Name;

	// Index into constant buffer corresponding to this material.
	int MatCBIndex = -1;

	// ???????? ???????? Material Buffer?? ???????? ????, Material Buffer?? ?????? ???????????? ????????.
	int MatInstIndex = -1;

	// Index into SRV heap for diffuse texture.
	int DiffuseSrvHeapIndex = -1;
	// Index into SRV heap for normal texture.
	int NormalSrvHeapIndex = -1;
	// Index into SRV heap for normal texture.
	int MaskSrvHeapIndex = -1;
	// Index into SRV heap for normal texture.
	int NoiseSrvHeapIndex = -1;

	// Dirty flag indicating the material has changed and we need to update the constant buffer.
	// Because we have a material constant buffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify a material we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = 3;

	bool isSkyTexture = false;

	// Material constant buffer data used for shading.
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = .25f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct Animate
{
	int index;

	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

struct Texture
{
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;

	bool isCube;

public:
	Texture() : isCube(false) {}
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif