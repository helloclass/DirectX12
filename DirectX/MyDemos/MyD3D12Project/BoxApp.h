#pragma once

#define no_init_all

#include "../../Common/d3dApp.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/Camera.h"

#include "_physx.h"

#include "BlurFilter.h"
#include "SobelFilter.h"

#include "RenderTarget.h"
#include "CubeRenderTarget.h"

#include <vector>
#include <list>
#include <algorithm>
#include <crtdbg.h>

#include <thread>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;


typedef struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT3 Tangent;
	XMFLOAT2 TexC;

	XMFLOAT4 BoneWeights;
	XMINT4 BoneIndices;
}Vertex;


struct InstanceData
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	UINT MaterialIndex;
};

struct MaterialData
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 64.0f;

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;
};

struct PmxAnimationData
{
	DirectX::XMFLOAT4X4 mOriginMatrix;
	DirectX::XMFLOAT4X4 mMatrix;
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 view = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Invview = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();

	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };

	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
};

//struct DeformConstants
//{
//	float Pose[4096 * 4];
//};

struct RateOfAnimTimeConstants
{
	float rateOfAnimTime;
};

class ObjectData
{
public:
	ObjectData() {}
	~ObjectData() {
		vertices.resize(0);
		indices.resize(0);

		vertices.clear();
		indices.clear();
	}
	std::string mName;
	std::string mFormat;

	std::vector<struct Vertex> vertices;
	std::vector<std::uint32_t> indices;

	// ��� ����޽��� �ε����� ����
	std::vector<std::set<int>> vertBySubmesh;
	// weight == 0.0
	std::vector<std::vector<int>> srcFixVertexSubmesh;
	std::vector<std::vector<DirectX::XMFLOAT3*>> dstFixVertexSubmesh;
	// weight != 0.0
	std::vector<std::vector<int>> srcDynamicVertexSubmesh;
	std::vector<std::vector<DirectX::XMFLOAT3*>> dstDynamicVertexSubmesh;

	UINT SubmeshCount = 0;

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

		std::vector<int>						mVertIndices;
		std::vector<std::array<float, 3>>		mVertOffset;
	};

	std::vector<struct _OBJECT_DATA_DESCRIPTOR> mDesc;
	std::vector<bool>							isCloth;
	std::vector<bool>							isRigidBody;
	std::vector<PxCloth*>						mClothes;
	std::vector< PxRigidDynamic*>				mRigidbody;
	std::vector<float>							mClothWeights;
	std::vector<int>							mClothBinedBoneIDX;

	std::vector<int>								mMorphDirty;
	std::vector<struct _VERTEX_MORPH_DESCRIPTOR>	mMorph;

	bool isDirty;

// Animation Kit
public:
	// �ִϸ��̼��� ���� ���ΰ�
	bool isAnim = false;
	// �ִϸ��̼��� ���� ���� �ΰ�?
	bool isLoop = false;
	// ���� �ִϸ��̼� �ε���
	float currentAnimIdx = 0;

	// Begin Anim Index
	float beginAnimIndex = 0;
	// End Anim index
	float endAnimIndex = 0;

	/*
		���� �ִϸ��̼� �ε������� ���� �ε����� �Ѿ�� ������ �����,
		���� ������ ���� ����
	 */
	float mAnimResidueTime = 0.0f;

	// �ش� �������� �ִϸ��̼� �̸� ����Ʈ
	FbxArray<FbxString*> animNameLists;
	// �� �ִϸ��̼��� ����, �� �ð�
	std::vector<FbxTime> mStart, mStop, mFrame;
	// �ִϸ��̼��� ��ü ������ ����
	std::vector<long long> countOfFrame;

	// FBX
	// Animation Kit (Per Frame(Per Bone))
	std::vector<std::vector<float*>>	mAnimVertex;

	// PMX
	// ���� �ִϸ��̼��� ���� ���� ���� �� ���
	std::vector<DirectX::XMFLOAT4X4>		mOriginRevMatrix;
	std::vector<std::vector<DirectX::XMFLOAT4X4>>	mBoneMatrix;

	// [Current Frame] [Deform(Submesh)IDX]
	std::vector<std::vector<FbxUInt>> mAnimVertexSize;

	int currentFrame = -1;
	bool updateCurrentFrame = false;
	// �ִϸ��̼��� ���� �ð� (�� ����)
	float currentDelayPerSec = 0;
	// �ִϸ��̼��� ��ü �෹�̼� (�� ����)
	std::vector<float> durationPerSec;
	// �� ������ �� �෹�̼� (�� ����)
	std::vector<float> durationOfFrame;

public:
	pmx::PmxModel mModel;
};

class RenderItem
{
public:
	RenderItem() {}
	~RenderItem() {
		for (int i = 0; i < Mat.size(); i++)
			delete(Mat[i]);
	}

	typedef enum RenderType {
		_OPAQUE_RENDER_TYPE,
		_ALPHA_RENDER_TYPE,
		_PMX_FORMAT_RENDER_TYPE,
		_SKY_FORMAT_RENDER_TYPE,
		_OPAQUE_SKINNED_RENDER_TYPE,
		_POST_PROCESSING_PIPELINE,
		_BLUR_HORZ_COMPUTE_TYPE,
		_BLUR_VERT_COMPUTE_TYPE,
		_SOBEL_COMPUTE_TYPE,
		_COMPOSITE_COMPUTE_TYPE,
	}RenderType;

	std::string mName;
	std::string mFormat;
	enum RenderType mRenderType;

	// Physx�� ����ȭ�� �Ǿ����.
	// Instance �� �ϳ��� PhtxResource�� �ο����� �� ����.
	//std::vector<int> physxIdx;
	//std::vector<PhyxResource*> physx;

	// �ϳ��� ���� ������Ʈ ��, ����Ž����� �ش� ������Ʈ������ ���� �����Ѵ�.
	MeshGeometry mGeometry;

	XMFLOAT4X4 mTexTransform = MathHelper::Identity4x4();

	int NumFrameDirty = 3;

	UINT ObjCBIndex = -1;
	UINT MatCBIndex = -1;

	// �������� ����޽��� �����Ƿ� ���� ��(����Ž� ���� ��ŭ)�� Texture, Material�� ǥ�� �� �� �ִ�. 
	std::vector<Material*> Mat;
	std::vector<Material*> SkyMat;

	// This BoundingBox will be used to checked that is in the FRUSTUM BOX or not.
	BoundingBox Bounds;

	UINT SubmeshCount = 0;
	UINT InstanceCount = 0;
	std::vector<InstanceData> _Instance;

	UINT offset = 0;

	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;

	bool isSky = false;
	UINT mSkyCubeIndex = -1;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//std::vector<physx::PxRigidDynamic*> test;

public:
	void setPosition(_In_ XMFLOAT3 pos);
	void setRotation(_In_ XMFLOAT3 rot);

	void setVelocity(_In_ XMFLOAT3 vel);
	void setTorque(_In_ XMFLOAT3 torq);

	void setInstancePosition(_In_ XMFLOAT3 pos, _In_ UINT idx = 0);
	void setInstanceRotation(_In_ XMFLOAT3 rot, _In_ UINT idx = 0);

	void setInstanceVelocity(_In_ XMFLOAT3 vel, _In_ UINT idx = 0);
	void setInstanceTorque(_In_ XMFLOAT3 torq, _In_ UINT idx = 0);

	void setAnimIndex(_In_ int animIndex);
	float getAnimIndex();
	void setAnimBeginIndex(_In_ int animBeginIndex);
	float getAnimBeginIndex();
	void setAnimEndIndex(_In_ int animEndIndex);
	float getAnimEndIndex();
	void setAnimIsLoop(_In_ bool animLoop);
	int getAnimIsLoop();
};

/*
	mGameObejcts�� List�� ���� �� ������
	1. ����Ʈ ���� ��� ���ҵ��� ���������� ���� ������ ������ ���ҵ��� �������� ������ �־�� ��.
	2. ����Ʈ ���� �����ϴ� ���Ұ� ���� ���� �ǰ� ������. (�����̾� �������� ������ ������, 
	�߰��� �ִ� ���Ұ� ���ŵǴ� ��쿡�� ����Ʈ ���� �ڷᱸ���� ����.) 
*/

// GameObjects Resources
static std::list<RenderItem*> mGameObjects;
// The variable Has a Vertex, Index datas of each GameObjects
static std::unordered_map<std::string, ObjectData*> mGameObjectDatas;
// The Count of Object of separated to RenderType
static std::unordered_map<RenderItem::RenderType, UINT> mRenderTypeCount;

// A Tasks list that is on the Frustum.
static std::vector<std::vector<UINT>> mRenderInstTasks;
// ��ü �ν��Ͻ� ����
static UINT mInstanceCount = 0;

class BoxApp : public D3DApp
{
public:
	BoxApp(_In_ HINSTANCE hInstance);
	BoxApp(const BoxApp& rhs) = delete;
	BoxApp& operator=(const BoxApp& rhs) = delete;
	~BoxApp();

	virtual bool Initialize(void)override;

private:
	virtual void CreateRtvAndDsvDescriptorHeaps()override;
	virtual void OnResize()override;
	virtual void Update(_In_ const GameTimer& gt)override;
	virtual void Draw(_In_ const GameTimer& gt)override;

	virtual void OnMouseDown(_In_ WPARAM btnState, _In_ int x, _In_ int y)override;
	virtual void OnMouseUp(_In_ WPARAM btnState, _In_ int x, _In_ int y)override;
	virtual void OnMouseMove(_In_ WPARAM btnState, _In_ int x, _In_ int y)override;

	static void InitSwapChain(_In_ int numThread);
	static DWORD WINAPI DrawThread(_In_ LPVOID temp);

	void BuildDescriptorHeaps(void);
	void BuildRootSignature(void);
	void BuildBlurRootSignature(void);
	void BuildSobelRootSignature(void);
	void BuildShadersAndInputLayout(void);
	void BuildRenderItem(void);
	void BuildFrameResource(void);
	void BuildPSO(void);

	void OnKeyboardInput(_In_ const GameTimer& gt);
	void AnimationMaterials(_In_ const GameTimer& gt);
	void UpdateInstanceData(_In_ const GameTimer& gt);
	//void UpdateMaterialBuffer(_In_ const GameTimer& gt);
	void UpdateMainPassCB(_In_ const GameTimer& gt);
	void UpdateAnimation(_In_ const GameTimer& gt);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

public:
	void _Awake(_In_ BoxApp* app);
	void _Start(void);
	void _Update(_In_ const GameTimer& gt);
	void _Exit(void);

	bool CloseCommandList(void);

	// Manipulate GameObject Function
public:
	RenderItem* CreateStaticGameObject(_In_ std::string Name, _In_ int instance);
	RenderItem* CreateDynamicGameObject(_In_ std::string Name, _In_ int instance);

	void CreateBoxObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_Out_ RenderItem* r, 
		_In_ float x, 
		_In_ float y, 
		_In_ float z, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale, 
		_In_ int subDividNum,
		RenderItem::RenderType renderType
	);
	void CreateSphereObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_Out_ RenderItem* r, 
		_In_ float rad, 
		_In_ int sliceCount, 
		_In_ int stackCount, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale,
		RenderItem::RenderType renderType
	);
	void CreateGeoSphereObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_Out_ RenderItem* r, 
		_In_ float rad, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale, 
		_In_ int subdivid,
		RenderItem::RenderType renderType
	);
	void CreateCylinberObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_Out_ RenderItem* r, 
		_In_ float bottomRad, 
		_In_ float topRad, 
		_In_ float height, 
		_In_ int sliceCount, 
		_In_ int stackCount, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale,
		RenderItem::RenderType renderType
	);
	void CreateGridObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_Out_ RenderItem* r, 
		_In_ float w, 
		_In_ float h, 
		_In_ int wc, 
		_In_ int hc, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale,
		RenderItem::RenderType renderType
	);
	void CreateFBXObject(
		_In_ std::string Name, 
		_In_ std::string Path,
		_In_ std::string FileName, 
		_Out_ std::vector<std::string>& Textures, 
		_Out_ RenderItem* r, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale,
		_In_ bool uvMode = true
	);
	void CreateFBXSkinnedObject(
		_In_ std::string Name, 
		_In_ std::string Path,
		_In_ std::string FileName, 
		_Out_ std::vector<std::string>& Textures, 
		_Out_ RenderItem* r, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale,
		_In_ bool uvMode = true
	);
	void CreatePMXObject(
		_In_ std::string Name, 
		_In_ std::string Path, 
		_In_ std::string FileName, 
		_Out_ std::vector<std::string>& Textures, 
		_Out_ RenderItem* r, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale
	);
	void ExtractAnimBones(
		_In_ std::string Path,
		_In_ std::string FileName,
		_In_ std::string targetPath,
		_In_ std::string targetFileName,
		_Out_ RenderItem* r
	);

private:

	static ComPtr<ID3D12RootSignature> mRootSignature;
	static ComPtr<ID3D12RootSignature> mBlurRootSignature;
	static ComPtr<ID3D12RootSignature> mSobelRootSignature;

	static ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	bool mFrustumCullingEnabled = true;

	float mTheta = 1.5f*XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	static std::unique_ptr<RenderTarget> mOffscreenRT;

	static std::unique_ptr<BlurFilter> mBlurFilter;
	static std::unique_ptr<SobelFilter> mSobelFilter;

	BoundingFrustum				mCamFrustum;
	PassConstants				mMainPassCB;
	Camera						mCamera;

	POINT mLastMousePos;

	static UINT mFilterCount;

private:
	// �ؽ��� CPU �� ����
	static CD3DX12_CPU_DESCRIPTOR_HANDLE mTextureHeapDescriptor;
	static CD3DX12_GPU_DESCRIPTOR_HANDLE mTextureHeapGPUDescriptor;

	static std::unordered_map<RenderItem::RenderType, ComPtr<ID3D12PipelineState>> mPSOs;

public:
	std::vector<std::string>							mTextureList;
	std::unordered_map<std::string, Texture>			mTextures;
	std::vector<std::pair<std::string, Material>>		mMaterials;
	std::unordered_map<std::string, ComPtr<ID3DBlob>>	mShaders;
	std::vector<Light>									mLights;
	std::vector<LightData>								mLightDatas;

public:
	std::vector<Texture> texList;
	// ���ο� DDS �ؽ��ĸ� �ε�
	void uploadTexture(_In_ Texture& t, _In_ bool isCube=false);
	// ���ο� ���׸����� �ε�
	void uploadMaterial(_In_ std::string name, _In_ bool isSkyTexture=false);
	// ���ο� ���׸����� �ε��ϰ� �ؽ��ĸ� ���ε�
	void uploadMaterial(_In_ std::string matName, _In_ std::string texName, _In_ bool isSkyTexture=false);
	// ���ο� ����Ʈ ���ε�
	void uploadLight(_In_ Light light);
	// ������Ʈ�� �ؽ��ĸ� ���ε�
	void BindTexture(_Out_ RenderItem* r, _In_ std::string name, int idx, _In_ bool isCubeMap = false);
	// ������Ʈ�� ���׸����� ���ε�
	void BindMaterial(_Out_ RenderItem* r, _In_ std::string name, _In_ bool isCubeMap=false);
	// ������Ʈ�� ���׸���� �ؽ��ĸ� ���ε�
	void BindMaterial(_Out_ RenderItem* r, _In_ std::string matName, _In_ std::string texName, _In_ bool isCubeMap=false);

private:
	// Draw Thread Resource
	static UINT numThread;

	static HANDLE renderTargetEvent[3];
	static HANDLE recordingDoneEvents[3];
	static HANDLE drawThreads[3];
	static LPDWORD ThreadIndex[3];
}; 

// Corroutine
class Corroutine {
private:
	HANDLE Thread;
	DWORD ThreadID;

	void* _ThreadFunc;

public:
	Corroutine(void* func) {
		_ThreadFunc = func;
	}

	void Start() {
		// Multi Command List makes tasks.
		Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_ThreadFunc, NULL, 0, &ThreadID);
	}
	void Suspend() {
		SuspendThread(Thread);
	}
	void Resume() {
		ResumeThread(Thread);
	}
	void End() {
		ExitThread(ThreadID);
	}
};