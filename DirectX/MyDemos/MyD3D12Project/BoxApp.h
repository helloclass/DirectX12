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
#include "ShadowMap.h"
#include "DrawTexture.h"
#include "Animation.h"
#include "Particle.h"

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

typedef struct BillBoardSpriteVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Size;
}BillBoardSpriteVertex;


struct InstanceData
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	UINT MaterialIndex=0;
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

struct RateOfAnimTimeConstants
{
	float rateOfAnimTime;
};

class ObjectData
{
public:
	ObjectData():
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
			throw std::runtime_error("");
		}
	}

	typedef enum RenderType {
		_OPAQUE_RENDER_TYPE,
		_ALPHA_RENDER_TYPE,
		_PMX_FORMAT_RENDER_TYPE,
		_OPAQUE_SHADOW_MAP_RENDER_TYPE,
		_OPAQUE_PICK_UP_MAP_RENDER_TYPE,
		_SKY_FORMAT_RENDER_TYPE,
		_OPAQUE_SKINNED_RENDER_TYPE,
		_DRAW_MAP_RENDER_TYPE,
		_POST_PROCESSING_PIPELINE,
		_BLUR_HORZ_COMPUTE_TYPE,
		_BLUR_VERT_COMPUTE_TYPE,
		_SOBEL_COMPUTE_TYPE,
		_COMPOSITE_COMPUTE_TYPE,
		_DEBUG_BOX_TYPE,
		_DRAW_MAP_TYPE,

		_SKILL_TONADO_SPLASH_TYPE
	}RenderType;

	std::string		mName;
	std::string		mFormat;
	RenderType		mRenderType;

	std::vector<struct Vertex> mVertices;
	std::vector<struct BillBoardSpriteVertex> mBillBoardVertices;
	std::vector<std::uint32_t> mIndices;

	// �ϳ��� ���� ������Ʈ ��, ����Ž����� �ش� ������Ʈ������ ���� �����Ѵ�.
	MeshGeometry mGeometry;

	// ��� ����޽��� �ε����� ����
	std::vector<std::set<int>> vertBySubmesh;
	// weight == 0.0
	std::vector<std::vector<int>> srcFixVertexSubmesh;
	std::vector<std::vector<DirectX::XMFLOAT3*>> dstFixVertexSubmesh;
	// weight != 0.0
	std::vector<std::vector<int>> srcDynamicVertexSubmesh;
	std::vector<std::vector<DirectX::XMFLOAT3*>> dstDynamicVertexSubmesh;

	// �������� ����޽��� �����Ƿ� ���� ��(����Ž� ���� ��ŭ)�� Texture, Material�� ǥ�� �� �� �ִ�. 
	std::vector<Material*> Mat;
	std::vector<Material*> SkyMat;

	// This BoundingBox will be used to checked that is in the FRUSTUM BOX or not.
	DirectX::XMFLOAT3 mColliderOffset;
	std::vector<BoundingBox> Bounds;

	UINT SubmeshCount = 0;
	UINT InstanceCount = 0;
	std::vector<InstanceData>		mInstances;

	std::vector<PhysResource>		mPhysResources;
	std::vector<PxRigidDynamic*>	mPhyxRigidBody;

	AnimationClip	mAnimationClip;
	Particle			mParticle;

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

	bool isDirty = false;
	bool isBillBoardDirty = false;

	bool isSky			= false;
	bool isDrawShadow	= false;
	bool isDrawTexture	= false;
	bool isBillBoard	= false;

// Debug Box
public:
	bool isDebugBox = false;
	std::unique_ptr<ObjectData> mDebugBoxData = nullptr;

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
	RenderItem():
		mFormat(""),
		ObjCBIndex(0),
		PrimitiveType(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
		InstanceCount(0) 
	{}

	RenderItem(std::string mName, UINT instance):
		mName(mName),
		mFormat(""),
		ObjCBIndex(0),
		PrimitiveType(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
		InstanceCount(instance)
	{
		isDirty.resize(instance);
	}

	~RenderItem() {}

	std::string mName;
	std::string mFormat;

	// Physx�� ����ȭ�� �Ǿ����.
	// Instance �� �ϳ��� PhtxResource�� �ο����� �� ����.
	//std::vector<int>				physxIdx;
	//std::vector<PhysResource*>	physx;

	XMFLOAT4X4 mTexTransform = MathHelper::Identity4x4();

	int NumFrameDirty = 3;

	UINT ObjCBIndex = -1;
	UINT MatCBIndex = -1;

	UINT SubmeshCount = 0;
	UINT InstanceCount = 0;

	UINT offset = 0;

	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;

	// �ش� ������Ʈ�� ī�޶� �������� ���� �ִٸ� true.
	std::vector<bool> isDirty;

	bool isSky = false;
	bool isDrawShadow = false;
	bool isDrawTexture = false;

	UINT mSkyCubeIndex = -1;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

public:
	RenderItem* mParent;
	std::unordered_map<std::string, RenderItem*> mChilds;

public:
	inline RenderItem* getParentRenderItem()
	{
		return mParent;
	}
	inline RenderItem* getChildRenderItem(std::string mName)
	{
		return mChilds[mName];
	}
	inline void setParentRenderItem(RenderItem* model)
	{
		mParent = model;
	}
	inline void appendChildRenderItem(RenderItem* model)
	{
		mChilds[model->mName] = model;
	}
	inline void removeChildRenderItem(RenderItem* model)
	{
		mChilds.erase(model->mName);
	}

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

	void setAnimClip(_In_ std::string mClipName);
	const std::string getAnimClip() const;

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
		void ParticleUpdate(float delta);
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
};

// GameObjects Resources
static std::list<RenderItem*> mGameObjects;
// The variable Has a Vertex, Index datas of each GameObjects
static std::unordered_map<std::string, ObjectData*> mGameObjectDatas;

// A Tasks list that is on the Frustum.
static std::vector<ObjectData*>			mRenderTasks;
static std::vector<std::vector<UINT>>	mRenderInstTasks;
// ��ü �ν��Ͻ� ����
static UINT mInstanceCount = 0;

// ��Ƽ �������� �̿��Ͽ� ������Ʈ�� �׸� ��,
// �ϳ��� �����尡 �׷��� �� ���� ������Ʈ�� �ν��Ͻ��� ����, �� �ε����� ����
struct PieceOfRenderItemByThread
{
	ObjectData* mObjPTR;
	UINT mInstanceOffset;
};

/*
	mGameObejcts�� List�� ���� �� ������
	1. ����Ʈ ���� ��� ���ҵ��� ���������� ���� ������ ������ ���ҵ��� �������� ������ �־�� ��.
	2. ����Ʈ ���� �����ϴ� ���Ұ� ���� ���� �ǰ� ������. (�����̾� �������� ������ ������, 
	�߰��� �ִ� ���Ұ� ���ŵǴ� ��쿡�� ����Ʈ ���� �ڷᱸ���� ����.) 
*/

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

	void RecursionChildItem(
		_In_ RenderItem* child
	);
	void RecursionFrameResource(
		_In_ RenderItem* obj,
		_Out_ int& InstanceNum,
		_Out_ int& SubmeshNum,
		_Out_ int& BoneNum
	);

	void BuildDescriptorHeaps(void);
	void BuildRootSignature(void);
	void BuildBlurRootSignature(void);
	void BuildSobelRootSignature(void);
	void BuildDrawMapSignature(void);
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
	void Pick(int sx, int sy);
	void PickBrush(int sx, int sy);

public:
	void _Awake(_In_ BoxApp* app);
	void _Start(void);
	void _Update(_In_ const GameTimer& gt);
	void _Exit(void);

	bool CloseCommandList(void);

	// Manipulate GameObject Function
public:
	RenderItem* CreateGameObject(_In_ std::string Name, _In_ int instance=1);
	RenderItem* CreateStaticGameObject(_In_ std::string Name, _In_ int instance=1);
	RenderItem* CreateKinematicGameObject(_In_ std::string Name, _In_ int instance=1);
	RenderItem* CreateDynamicGameObject(_In_ std::string Name, _In_ int instance=1);

	void CreateBoxObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_In_ std::string Format,
		_Out_ RenderItem* r, 
		_In_ float x, 
		_In_ float y, 
		_In_ float z, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale, 
		_In_ int subDividNum,
		ObjectData::RenderType renderType,
		bool isDrawShadow = true,
		bool isDrawTexture = false
	);
	void CreateSphereObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_In_ std::string Format,
		_Out_ RenderItem* r, 
		_In_ float rad, 
		_In_ int sliceCount, 
		_In_ int stackCount, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale,
		ObjectData::RenderType renderType,
		bool isDrawShadow = true,
		bool isDrawTexture = false
	);
	void CreateGeoSphereObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_In_ std::string Format,
		_Out_ RenderItem* r, 
		_In_ float rad, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale, 
		_In_ int subdivid,
		ObjectData::RenderType renderType,
		bool isDrawShadow = true,
		bool isDrawTexture = false
	);
	void CreateCylinberObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_In_ std::string Format,
		_Out_ RenderItem* r, 
		_In_ float bottomRad, 
		_In_ float topRad, 
		_In_ float height, 
		_In_ int sliceCount, 
		_In_ int stackCount, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale,
		ObjectData::RenderType renderType,
		bool isDrawShadow = true,
		bool isDrawTexture = false
	);
	void CreateGridObject(
		_In_ std::string Name, 
		_In_ std::string textuerName, 
		_In_ std::string Format,
		_Out_ RenderItem* r, 
		_In_ float w, 
		_In_ float h, 
		_In_ int wc, 
		_In_ int hc, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale,
		ObjectData::RenderType renderType,
		bool isDrawShadow = true,
		bool isDrawTexture = false
	);
	void BoxApp::CreateBillBoardObject(
		_In_ std::string Name,
		_In_ std::string textuerName,
		_In_ std::string Format,
		_Out_ RenderItem* r,
		_In_ UINT particleCount,
		_In_ DirectX::XMFLOAT3 position,
		_In_ DirectX::XMFLOAT2 extends,
		_In_ ObjectData::RenderType renderType,
		_In_ bool isDrawShadow = false
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
		_In_ bool uvMode = true,
		bool isDrawShadow = true,
		bool isDrawTexture = false
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
		_In_ bool uvMode = true,
		bool isDrawShadow = true,
		bool isDrawTexture = false
	);
	void CreatePMXObject(
		_In_ std::string Name, 
		_In_ std::string Path, 
		_In_ std::string FileName, 
		_Out_ std::vector<std::string>& Textures, 
		_Out_ RenderItem* r, 
		_In_ DirectX::XMFLOAT3 position, 
		_In_ DirectX::XMFLOAT3 rotation, 
		_In_ DirectX::XMFLOAT3 scale,
		bool isDrawShadow = true,
		bool isDrawTexture = false
	);
	void CreateDebugBoxObject(
		_In_ RenderItem* r
	);
	void ExtractAnimBones(
		_In_ std::string Path,
		_In_ std::string FileName,
		_In_ std::string targetPath,
		_In_ std::string targetFileName,
		_Out_ RenderItem* r
	);
	void SetBoundBoxScale(
		_Out_ RenderItem* r,
		_In_ XMFLOAT3	scale
	);

private:
	static ComPtr<ID3D12RootSignature> mRootSignature;
	static ComPtr<ID3D12RootSignature> mBlurRootSignature;
	static ComPtr<ID3D12RootSignature> mSobelRootSignature;
	static ComPtr<ID3D12RootSignature> mDrawMapSignature;

	static ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mBillBoardInputLayout;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	bool mFrustumCullingEnabled = true;

	float mTheta = 1.5f*XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	static std::unique_ptr<RenderTarget> mOffscreenRT;

	static std::unique_ptr<BlurFilter>	mBlurFilter;
	static std::unique_ptr<SobelFilter> mSobelFilter;
	static std::unique_ptr<ShadowMap>	mShadowMap;
	static std::unique_ptr<DrawTexture> mDrawTexture;

	BoundingFrustum				mCamFrustum;
	PassConstants				mMainPassCB;
	Camera						mCamera;

	static UINT mFilterCount;

private:
	// �ؽ��� CPU �� ����
	static CD3DX12_CPU_DESCRIPTOR_HANDLE mTextureHeapDescriptor;
	static CD3DX12_GPU_DESCRIPTOR_HANDLE mTextureHeapGPUDescriptor;

	static std::unordered_map<ObjectData::RenderType, ComPtr<ID3D12PipelineState>> mPSOs;

public:
	std::vector<std::string>							mTextureList;
	std::unordered_map<std::string, Texture>			mTextures;
	std::vector<std::pair<std::string, Material>>		mMaterials;
	std::unordered_map<std::string, ComPtr<ID3DBlob>>	mShaders;
	std::vector<Light>									mLights;
	std::vector<LightDataConstants>						mLightDatas;

public:
	std::vector<Texture> texList;
	// ���ο� DDS �ؽ��ĸ� �ε�
	void uploadTexture(
		_In_ Texture& t, 
		_In_ bool isCube=false
	);
	// ���ο� ���׸����� �ε�
	void uploadMaterial(
		_In_ std::string name, 
		_In_ bool isSkyTexture=false
	);
	// ���ο� ���׸����� �ε��ϰ� �ؽ��ĸ� ���ε�
	void uploadMaterial(
		_In_ std::string matName, 
		_In_ std::string texName, 
		_In_ bool isSkyTexture=false
	);
	void BoxApp::uploadMaterial(
		_In_ std::string matName,
		_In_ std::string tex_Diffuse_Name,
		_In_ std::string tex_Mask_Name,
		_In_ std::string tex_Noise_Name,
		_In_ bool isSkyTexture
	);
	// ���ο� ����Ʈ ���ε�
	void uploadLight(
		_In_ Light light
	);
	// ������Ʈ�� �ؽ��ĸ� ���ε�
	void BindTexture(
		_Out_ RenderItem* r, 
		_In_ std::string name, 
		int idx, 
		_In_ bool isCubeMap = false
	);
	// ������Ʈ�� ���׸����� ���ε�
	void BindMaterial(
		_Out_ RenderItem* r,
		_In_ std::string name,
		_In_ bool isCubeMap = false
	);
	// ������Ʈ�� ���׸����� ���ε�
	void BindMaterial(
		_Out_ RenderItem* r, 
		_In_ std::string name, 
		_In_ std::string maskTexName,
		_In_ std::string noiseTexName, 
		_In_ bool isCubeMap				= false
	);
	// ������Ʈ�� ���׸���� �ؽ��ĸ� ���ε�
	void BindMaterial(
		_Out_ RenderItem* r,
		_In_ std::string matName,
		_In_ std::string texName,
		_In_ bool isCubeMap = false
	);
	// ������Ʈ�� ���׸���� �ؽ��ĸ� ���ε�
	void BindMaterial(
		_Out_ RenderItem* r, 
		_In_ std::string matName, 
		_In_ std::string texName,
		_In_ std::string maskTexName,
		_In_ std::string noiseTexName,
		_In_ bool isCubeMap				= false
	);

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
