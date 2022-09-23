#pragma once

#define __UTIL_H_

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
#include <set>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include <memory>
#include <algorithm>
#include <crtdbg.h>

#include "fbxsdk.h"
#include "Pmx.h"
#include "EncodingHelper.h"
#include "../MyDemos/MyD3D12Project/_physx.h"

#include <thread>

#include <random>

#include "d3dx12.h"
#include "DDSTextureLoader.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

#include "FreeImage.h"
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "FreeImaged.lib")

// IMGUI

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

// ~IMGUI

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

	// <�ݵ���> CommandList�� �����ִ��� Ȯ�� �ϰ� ����� ��!!!!!!!!! 
	static void UpdateDefaultBuffer(
		ID3D12GraphicsCommandList* cmdList,						// DefaultBuffer�� ������Ʈ ���� �� ����������
		const void* initData,									// DefaultBuffer�� ������Ʈ ��ų ������
		UINT64 byteSize,										// Ÿ�� �������� ������
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer,	// ���ε� ����
		Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer	// GPU ����
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

	// Index ������
	UINT StartIndexLocation = 0;
	// Vertex ������
	UINT BaseVertexLocation = 0;

	// Index ������
	UINT IndexSize = 0;
	//  Vertex ������
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

	// ���� �� ����޽��� ���������� ������ �ϰ� ���� ���
	std::vector<UINT> IndexSizes;

	// System memory copies.  Use Blobs because the vertex/index format can be generic.
	// It is up to the client to cast appropriately.  
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU  = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// ���� �� ����޽��� ���������� ������ �ϰ� ���� ���
	std::vector<Microsoft::WRL::ComPtr<ID3DBlob>> VertexBufferCPUs;
	std::vector<Microsoft::WRL::ComPtr<ID3DBlob>> IndexBufferCPUs;

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> VertexBufferGPUs;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> IndexBufferGPUs;

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> VertexBufferUploaders;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> IndexBufferUploaders;

    // Data about the buffers.
	UINT VertexByteStride = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;

	// A MeshGeometry may store multiple geometries in one vertex/index buffer.
	// Use this container to define the Submesh geometries so we can draw
	// the Submeshes individually.

	// 서브메쉬 개수를 저장
	UINT subMeshCount = 0;
	// 오브젝트를 이루는 서브메쉬의 이름들을 저장.
	std::vector<std::string> meshNames;
	std::vector<SubmeshGeometry> DrawArgs;

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
	DirectX::XMFLOAT4X4 ViewProjTex = MathHelper::Identity4x4();
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

struct SsaoConstants
{
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 InvProj;
	DirectX::XMFLOAT4X4 ProjTex;
	DirectX::XMFLOAT4   OffsetVectors[14];

	// For SsaoBlur.hlsl
	DirectX::XMFLOAT4 BlurWeights[3];

	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

	// Coordinates given in view space.
	float OcclusionRadius = 0.5f;
	float OcclusionFadeStart = 0.2f;
	float OcclusionFadeEnd = 2.0f;
	float SurfaceEpsilon = 0.05f;
};

typedef enum LightType
{
	DIR_LIGHTS  = 0,
	POINT_LIGHT = 1,
	SPOT_LIGHT  = 2
}LightType;

struct Light
{
	DirectX::XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowViewProjNDC = MathHelper::Identity4x4();

	DirectX::XMFLOAT4 AmbientLight = { 1.0f, 0.0f, 0.0f, 1.0f };

	DirectX::XMFLOAT4 mStrength = { 0.5f, 0.5f, 0.5f, 0.0f };
	DirectX::XMFLOAT4 mDirection = { 0.0f, 1.0f, 0.0f, 0.0f };// direct/spot light only
	DirectX::XMFLOAT4 mPosition = { 1.0f, 0.0f, 0.0f, 0.0f };  // point/spot light only
	DirectX::XMFLOAT4 mLightPosW = { 0.0f, 0.0f, 0.0f, 0.0f };

	int mLightType;
    float mFalloffStart = 1.0f;                          // point/spot light only
    float mFalloffEnd = 10.0f;                           // point/spot light only
    
    float mSpotPower = 64.0f;                            // spot light only

	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;

	float	PADDING[2];
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

	// ����ؼ� �����Ǵ� Material Buffer�� �����ϱ� ����, Material Buffer�� ��� ��ġ���ִ��� �����Ѵ�.
	int MatInstIndex = -1;

	// Index into SRV heap for diffuse texture.
	int DiffuseSrvHeapIndex = 0;
	// Index into SRV heap for normal texture.
	int NormalSrvHeapIndex = 0;
	// Index into SRV heap for normal texture.
	int MaskSrvHeapIndex = 0;
	// Index into SRV heap for normal texture.
	int NoiseSrvHeapIndex = 0;

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
	DirectX::XMFLOAT2 DiffuseCount = { 1.0f, 1.0f };
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

////////////////////////////////////////////////////////////////
// Objects
////////////////////////////////////////////////////////////////

typedef struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT3 Tangent;
	DirectX::XMFLOAT2 TexC;

	DirectX::XMFLOAT4 BoneWeights;
	DirectX::XMINT4 BoneIndices;
}Vertex;

typedef struct BillBoardSpriteVertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Norm;
	DirectX::XMFLOAT2 Size;
}BillBoardSpriteVertex;


struct InstanceData
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	UINT MaterialIndex = 0;

	// Texture Animation Frame
	DirectX::XMFLOAT2 TopLeftTexCroodPos = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 BottomRightTexCroodPos = { 1.0f, 1.0f };
};

struct MaterialData
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 64.0f;

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;

	DirectX::XMFLOAT2 DiffuseCount = { 1.0f, 1.0f };
};

struct PmxAnimationData
{
	DirectX::XMFLOAT4X4 mOriginMatrix;
	DirectX::XMFLOAT4X4 mMatrix;
};

struct RateOfAnimTimeConstants
{
	float rateOfAnimTime;
};

class Translate
{
public:
	Translate()
	{
		position	= { 0.0f, 0.0f, 0.0f, 0.0f };
		rotation	= { 0.0f, 0.0f, 0.0f, 0.0f };
		scale		= { 1.0f, 1.0f, 1.0f, 1.0f };
		velocity	= { 0.0f, 0.0f, 0.0f, 0.0f };
		accelerate	= { 0.0f, 0.0f, 0.0f, 0.0f };
		torque		= { 0.0f, 0.0f, 0.0f, 0.0f };

		mUpdateWorldMat = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
	}

	Translate(
		DirectX::XMVECTOR mPosition,
		DirectX::XMVECTOR mRotation,
		DirectX::XMVECTOR mScale
	):	position(mPosition),
		rotation(mRotation),
		scale(mScale) 
	{
		velocity	= { 0.0f, 0.0f, 0.0f, 0.0f };
		accelerate	= { 0.0f, 0.0f, 0.0f, 0.0f };
		torque		= { 0.0f, 0.0f, 0.0f, 0.0f };

		mUpdateWorldMat = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
	}

	Translate(
		DirectX::XMVECTOR mPosition,
		DirectX::XMVECTOR mRotation,
		DirectX::XMVECTOR mScale,
		DirectX::XMMATRIX mWorld
	) : position(mPosition),
		rotation(mRotation),
		scale(mScale),
		mUpdateWorldMat(mWorld)
	{
		velocity	= { 0.0f, 0.0f, 0.0f, 0.0f };
		accelerate	= { 0.0f, 0.0f, 0.0f, 0.0f };
		torque		= { 0.0f, 0.0f, 0.0f, 0.0f };

		mUpdateWorldMat = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
	}

public:
	DirectX::XMVECTOR position	= {0.0f, 0.0f, 0.0f, 1.0f};
	DirectX::XMVECTOR rotation	= {0.0f, 0.0f, 0.0f, 1.0f};
	DirectX::XMVECTOR scale		= {1.0f, 1.0f, 1.0f, 1.0f};

	DirectX::XMVECTOR velocity;
	DirectX::XMVECTOR accelerate;

	DirectX::XMVECTOR torque;

	DirectX::XMVECTOR mOrientationVec = { 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMVECTOR mQuaternionVec;

	DirectX::XMMATRIX mUpdateWorldMat;

public:
	inline void Update(float delta)
	{
		velocity.m128_f32[0] += accelerate.m128_f32[0] * delta;
		velocity.m128_f32[1] += accelerate.m128_f32[1] * delta;
		velocity.m128_f32[2] += accelerate.m128_f32[2] * delta;

		position.m128_f32[0] += velocity.m128_f32[0] * delta;
		position.m128_f32[1] += velocity.m128_f32[1] * delta;
		position.m128_f32[2] += velocity.m128_f32[2] * delta;

		rotation.m128_f32[0] += torque.m128_f32[0] * delta;
		rotation.m128_f32[1] += torque.m128_f32[1] * delta;
		rotation.m128_f32[2] += torque.m128_f32[2] * delta;
	}

	inline DirectX::XMMATRIX Matrix()
	{
		//rotation.m128_f32[0] = DirectX::XMConvertToRadians(rotation.m128_f32[0]);
		//rotation.m128_f32[1] = DirectX::XMConvertToRadians(rotation.m128_f32[1]);
		//rotation.m128_f32[2] = DirectX::XMConvertToRadians(rotation.m128_f32[2]);

		mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(rotation);

		//mQuaternionVec = rotation;

		return(
			DirectX::XMMatrixAffineTransformation(
				scale,
				mOrientationVec,
				mQuaternionVec,
				position
			)
		);
	}

	inline void setVelocity(DirectX::XMVECTOR mVelocity)
	{
		DirectX::XMVECTOR normalizedVector = DirectX::XMVector3Transform(
			mVelocity,
			DirectX::XMMatrixRotationRollPitchYaw(
				rotation.m128_f32[0],
				rotation.m128_f32[1],
				rotation.m128_f32[2]
			)
		);

		velocity = normalizedVector;
	}

	inline void setVelocityX(float X)
	{
		velocity.m128_f32[0] = X;
	}

	inline void setVelocityY(float Y)
	{
		velocity.m128_f32[1] = Y;
	}

	inline void setVelocityZ(float Z)
	{
		velocity.m128_f32[2] = Z;
	}

	inline void addVelocityX(float X)
	{
		velocity.m128_f32[0] += X;
	}

	inline void addVelocityY(float Y)
	{
		velocity.m128_f32[1] += Y;
	}

	inline void addVelocityZ(float Z)
	{
		velocity.m128_f32[2] += Z;
	}

	inline void addVelocity(DirectX::XMVECTOR mVelocity)
	{
		velocity.m128_f32[0] += mVelocity.m128_f32[0];
		velocity.m128_f32[1] += mVelocity.m128_f32[1];
		velocity.m128_f32[2] += mVelocity.m128_f32[2];
	}
};

class ObjectData
{
public:
	ObjectData() :
		isDebugBox(false)
	{}
	~ObjectData() {
		try {
			Mat.clear();

			mVertices.resize(0);
			mBillBoardVertices.resize(0);
			mIndices.resize(0);

			mVertices.clear();
			mBillBoardVertices.clear();
			mIndices.clear();
		}
		catch (std::exception&)
		{
			abort();
		}
	}

	typedef enum RenderType {
		_OPAQUE_RENDER_TYPE,
		_ALPHA_RENDER_TYPE,
		_PMX_FORMAT_RENDER_TYPE,
		_OPAQUE_SHADOW_MAP_RENDER_TYPE,
		_OPAQUE_PICK_UP_MAP_RENDER_TYPE,
		_SKY_FORMAT_RENDER_TYPE,
		_MAP_BASE_RENDER_TYPE,
		_OPAQUE_SKINNED_RENDER_TYPE,
		_DRAW_MAP_RENDER_TYPE,
		_POST_PROCESSING_PIPELINE,
		_BLUR_HORZ_COMPUTE_TYPE,
		_BLUR_VERT_COMPUTE_TYPE,
		_SOBEL_COMPUTE_TYPE,
		_DRAW_NORMAL_COMPUTE_TYPE,
		_SSAO_COMPUTE_TYPE,
		_SSAO_BLUR_COMPUTE_TYPE,
		_COMPOSITE_COMPUTE_TYPE,
		_DEBUG_BOX_TYPE,
		_DRAW_MAP_TYPE,

		_SKILL_TONADO_SPLASH_TYPE,
		_SKILL_TONADO_SPLASH_3_TYPE,
		_SKILL_TONADO_MAIN_TONADO_TYPE,
		_SKILL_TONADO_WATER_TORUS_TYPE,
		_SKILL_PUNCH_SPARKS_TYPE,
		_SKILL_PUNCH_ENDL_DESBIS_TYPE,
		_SKILL_LASER_TRAILS_TYPE
	}RenderType;

	std::string		mName;
	std::string		mFormat;
	RenderType		mRenderType;

	std::string		mParentName;

	std::vector<struct Vertex> mVertices;
	std::vector<struct BillBoardSpriteVertex> mBillBoardVertices;
	std::vector<std::uint32_t> mIndices;

	MeshGeometry mGeometry;

	// For Cloth Physx or Animation
	std::vector<std::set<int>> vertBySubmesh;
	// weight == 0.0
	std::vector<std::vector<int>> srcFixVertexSubmesh;
	std::vector<std::vector<DirectX::XMFLOAT3*>> dstFixVertexSubmesh;
	// weight != 0.0
	std::vector<std::vector<int>> srcDynamicVertexSubmesh;
	std::vector<std::vector<DirectX::XMFLOAT3*>> dstDynamicVertexSubmesh;

	// 만일 특정 버텍스가 잡혔을 경우 해당 버텍스의 위치는 마우스의 위치에 의해 결정 됩니다.
	bool isPicked = false;
	UINT mPickedSubmesh;
	UINT mPickedIndex;

	// Materials
	std::vector<Material> Mat;
	std::vector<Material> SkyMat;

	// This BoundingBox will be used to checked that is in the FRUSTUM BOX or not.
	DirectX::XMFLOAT3 mColliderOffset;
	std::list<DirectX::BoundingBox> Bounds;

	UINT SubmeshCount = 0;
	UINT InstanceCount = 0;
	std::list<InstanceData>		mInstances;
	std::vector<Translate>		mTranslate;

	// if the objectData is only rendering one submesh.
	std::vector<UINT> onlySubmeshIDXs;
	std::vector<std::vector<UINT>> onlySubmeshInstanceIDXs;

	//std::vector<PhysResource>		mPhysResources;
	std::vector<physx::PxRigidDynamic*>	mPhysRigidBody;

	struct _OBJECT_DATA_DESCRIPTOR {
		// Offset of Vertex
		UINT BaseVertexLocation = 0;
		// Offset of Index
		UINT StartIndexLocation = 0;

		// Size of Vertex by Submesh
		UINT VertexSize = 0;
		// Size of Index by Submesh
		UINT IndexSize = 0;
	};

	struct _VERTEX_MORPH_DESCRIPTOR {
		std::wstring	Name;
		std::wstring	NickName;
		float			mVertWeight;

		std::vector<int>							mVertIndices;
		std::vector<std::array<float, 3>>			mVertOffset;
	};

	std::vector<struct _OBJECT_DATA_DESCRIPTOR>		mDesc;
	std::vector<bool>								isCloth;
	std::vector<bool>								isRigidBody;
	std::vector<physx::PxCloth*>					mClothes;
	std::vector<physx::PxRigidDynamic*>				mRigidbody;
	std::vector<float>								mClothWeights;
	std::vector<int>								mClothBinedBoneIDX;

	std::vector<int>								mMorphDirty;
	std::vector<int>								mClothDirty;
	std::vector<struct _VERTEX_MORPH_DESCRIPTOR>	mMorph;

	bool isDirty = false;
	bool isDiffuseDirty = false;
	bool isBillBoardDirty = false;
	bool isBaked = false;
	bool isSky = false;
	bool isDrawShadow = false;
	bool isDrawTexture = false;
	bool isBillBoard = false;
	bool isTextureSheetAnimation = false;
	bool isOnlySubmesh = false;

	// Debug Box
public:
	bool isDebugBox = false;
	std::unique_ptr<ObjectData> mDebugBoxData = nullptr;

	// Animation Kit
public:
	// Is the animation playing?
	bool isAnim = false;
	// Is the animation Loop?
	bool isLoop = true;
	// Current Animation Clip Index
	float currentAnimIdx = 0;

	// Begin Anim Index
	float beginAnimIndex = 0;
	// End Anim index
	float endAnimIndex = 0;

	bool isAnimDone = false;

	/*
		The Residue Value of next frame.
	 */
	float mAnimResidueTime = 0.0f;

	FbxArray<FbxString*> animNameLists;
	std::vector<FbxTime> mStart, mStop, mFrame;
	std::vector<long long> countOfFrame;

	// FBX
	// Animation Kit (Per Submesh(Per AnimationClip(Per Frame)))
	std::vector<std::vector<float*>>	mAnimVertex;
	// Vertex Count information of each Submeshs.
	std::vector<FbxUInt> mAnimVertexSize;

	// PMX
	// 
	std::vector<DirectX::XMFLOAT4X4>		mOriginRevMatrix;
	std::vector<std::vector<DirectX::XMFLOAT4X4>>	mBoneMatrix;


	int currentFrame = -1;
	bool updateCurrentFrame = false;
	//
	float currentDelayPerSec = 0;
	// 
	std::vector<float> durationPerSec;
	//
	std::vector<float> durationOfFrame;

public:
	pmx::PmxModel mModel;
};

// The variable Has a Vertex, Index datas of each GameObjects
static std::unordered_map<std::string, ObjectData*> mGameObjectDatas;

class RenderItem
{
public:
	RenderItem() :
		mFormat("")
	{}

	RenderItem(std::string mName, UINT instance) :
		mName(mName),
		InstanceCount(instance),
		mFormat("")
	{

	}

	~RenderItem() {}

	std::string mName;
	std::string mFormat;

	UINT InstanceCount = 0;
	UINT SubmeshCount = 0;

	ObjectData* mData = nullptr;

public:
	RenderItem* mParent = nullptr;
	std::unordered_map<std::string, RenderItem*> mChilds;

public:
	inline RenderItem* getParentRenderItem()
	{
		return mParent;
	}
	inline RenderItem* getChildRenderItem(std::string mName)
	{
		if (!mChilds[mName])
			throw std::runtime_error("Can't find Chile Object!");

		return mChilds[mName];
	}
	inline void setParentRenderItem(RenderItem* model)
	{
		mParent = model;
	}
	inline void appendChildRenderItem(RenderItem* model)
	{
		mChilds[model->mName] = model;
		model->mParent = this;
	}
	inline void removeChildRenderItem(RenderItem* model)
	{
		mChilds.erase(model->mName);
	}

public:
	//inline void setBoundaryPosition(_In_ bool isShadow)
	//{
	//	if (!this->mData)
	//		throw std::runtime_error("");

	//	this->mData->isDrawShadow = isShadow;
	//}
	inline void setBoundaryScale(_In_ DirectX::XMFLOAT3 mExtents)
	{
		if (!this->mData)
			throw std::runtime_error("");

		std::list<DirectX::BoundingBox>::iterator iter = this->mData->Bounds.begin();
		std::list<DirectX::BoundingBox>::iterator end = this->mData->Bounds.end();

		while (iter != end)
		{
			(*iter).Extents = mExtents;

			iter++;
		}
		
	}

public:
	inline void setIsDrawShadow(_In_ bool isShadow)
	{
		if (!this->mData)
			throw std::runtime_error("");

		this->mData->isDrawShadow = isShadow;
	}
	inline void setIsBaked(_In_ bool isBaked)
	{
		if (!this->mData)
			throw std::runtime_error("");

		this->mData->isBaked = isBaked;
	}

public:
	void setPosition(_In_ DirectX::XMFLOAT3 pos);
	void setRotation(_In_ DirectX::XMFLOAT3 rot);

	void setVelocity(_In_ DirectX::XMFLOAT3 vel);
	void setTorque(_In_ DirectX::XMFLOAT3 torq);

	void setInstancePosition(_In_ DirectX::XMFLOAT3 pos, _In_ UINT idx = 0);
	void setInstanceRotation(_In_ DirectX::XMFLOAT3 rot, _In_ UINT idx = 0);

	void setInstanceVelocity(_In_ DirectX::XMFLOAT3 vel, _In_ UINT idx = 0);
	void setInstanceTorque(_In_ DirectX::XMFLOAT3 torq, _In_ UINT idx = 0);

	void setAnimIndex(_In_ int animIndex);
	float getAnimIndex();
	void setAnimBeginIndex(_In_ int animBeginIndex);
	float getAnimBeginIndex();
	void setAnimEndIndex(_In_ int animEndIndex);
	float getAnimEndIndex();
	void setAnimIsLoop(_In_ bool animLoop);
	int getAnimIsLoop();

	void setAnimClip(
		_In_ std::string mClipName,
		_In_ int mInstanceOffset,
		_In_ bool isLoop,
		_In_ bool isCompression = false
	);
	const std::string getAnimClip(
		_In_ int mInstanceOffset
	) const;

public:
	void InitParticleSystem(
		_In_ float mDurationTime,
		_In_ bool mOnPlayAwake = true
	);

	void InitParticleSystem(
		_In_ float mDurationTime,
		_In_ DirectX::XMFLOAT3 mMinAcc,
		_In_ DirectX::XMFLOAT3 mMaxAcc,
		_In_ DirectX::XMFLOAT3 mMinVelo,
		_In_ DirectX::XMFLOAT3 mMaxVelo,
		_In_ bool mOnPlayAwake = true
	);

	void ParticleGene();
	void ParticleUpdate(
		_In_ float delta, 
		_In_ float time
	);
	void ParticleReset();

	void setDurationTime(
		_In_ float duration
	);
	void setIsLoop(
		_In_ bool isLoop
	);
	void setIsFilled(
		_In_ bool isFilled
	);
	void setStartDelay(
		_In_ float startDelay
	);
	void setStartLifeTime(
		_In_ float startLifeTime
	);
	void setOnPlayAwake(
		_In_ bool onPlayAwake
	);
	void setMinAcc(
		_In_ DirectX::XMFLOAT3 minAcc
	);
	void setMaxAcc(
		_In_ DirectX::XMFLOAT3 maxAcc
	);
	void setMinVelo(
		_In_ DirectX::XMFLOAT3 minVelo
	);
	void setMaxVelo(
		_In_ DirectX::XMFLOAT3 maxVelo
	);
	void setTextureSheetAnimationXY(
		_In_ UINT x,
		_In_ UINT y
	);
	void setTextureSheetAnimationFrame(
		_In_ float mFrame
	);
	void setIsUsedErrorScale(
		_In_ DirectX::XMFLOAT3 mErrorScale
	);

	void appendScaleAnimation(
		_In_ float mTime,
		_In_ DirectX::XMFLOAT3 mScale
	);
	void appendDiffuseAnimation(
		_In_ float mTime,
		_In_ DirectX::XMFLOAT4 mDiffuse
	);

public:
	void setOnlySubmesh(
		_In_ std::string mName,
		_In_ int mSubmeshIDX
	);
	void setOnlySubmesh(
		_In_ std::string mName,
		_In_ std::string mSubmeshName
	);

	void Instantiate(
		_In_ DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f },
		_In_ DirectX::XMFLOAT3 mRotation = { 0.0f, 0.0f, 0.0f },
		_In_ DirectX::XMFLOAT3 mScale = { 1.0f, 1.0f, 1.0f },
		_In_ DirectX::XMFLOAT3 mBoundScale = { 1.0f, 1.0f, 1.0f }
	);
	void Destroy(
		_In_ UINT InstanceIDX = 0
	);

};

// GameObjects Resources
static std::list<RenderItem*> mGameObjects;

////////////////////////////////////////////////////////////////
// ETC Util
////////////////////////////////////////////////////////////////

static std::random_device rd;

class Random {
public:
	static float Rangefloat(float x, float y)
	{
		if (x > y)
			return 0.0f;

		std::mt19937 gen(rd());

		std::uniform_real_distribution<float> dis(x, y);

		return dis(gen);
	}

	static int RangeInt(int x, int y)
	{
		y = y - 1;

		if (x > y)
			return 0;

		std::mt19937 gen(rd());

		std::uniform_int_distribution<int> dis(x, y);

		return dis(gen);
	}
};

//Physics.Raycast
static bool RayCast(DirectX::XMVECTOR rayOrigin, DirectX::XMVECTOR rayDir, float& tmin, std::vector<std::string>& mHitNames, DirectX::XMFLOAT3& hitPos)
{
	std::list<DirectX::BoundingBox>::iterator BoundsIterator;

	std::unordered_map<std::string, ObjectData*>::iterator iter = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator end = mGameObjectDatas.end();
	ObjectData* obj = nullptr;

	tmin = -1.0f;

	while (iter != end)
	{
		obj = (*iter).second;
		BoundsIterator = obj->Bounds.begin();

		for (UINT instanceIDX = 0; instanceIDX < obj->InstanceCount; instanceIDX++)
		{
			if ((*BoundsIterator).Intersects(rayOrigin, rayDir, tmin))
			{
				mHitNames.push_back(obj->mName + std::to_string(instanceIDX));
			}
			BoundsIterator++;
		}
	}

	if (tmin > 0.0f)
	{
		hitPos.x = rayOrigin.m128_f32[0] + rayDir.m128_f32[0] * tmin;
		hitPos.y = rayOrigin.m128_f32[1] + rayDir.m128_f32[1] * tmin;
		hitPos.z = rayOrigin.m128_f32[2] + rayDir.m128_f32[2] * tmin;

		return true;
	}

	return false;
}

static bool isTargetHitRay(
	DirectX::XMVECTOR rayOrigin, 
	DirectX::XMVECTOR rayDir, 
	DirectX::XMFLOAT3& hitPos, 
	std::list<DirectX::BoundingBox>& boundBoxs
)
{
	float tmin = -1.0f;

	std::list<DirectX::BoundingBox>::iterator iter = boundBoxs.begin();

	for (UINT i = 0; i < boundBoxs.size(); i++)
	{
		if ((*iter).Intersects(rayOrigin, rayDir, tmin))
		{
			hitPos.x = rayOrigin.m128_f32[0] + rayDir.m128_f32[0] * tmin;
			hitPos.y = rayOrigin.m128_f32[1] + rayDir.m128_f32[1] * tmin;
			hitPos.z = rayOrigin.m128_f32[2] + rayDir.m128_f32[2] * tmin;

			return true;
		}

		iter++;
	}
	return false;
}