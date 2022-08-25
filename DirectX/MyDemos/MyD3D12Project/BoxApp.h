#pragma once

#define no_init_all

#include "../../Common/d3dApp.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/MathHelper.h"

#include "_physx.h"

#include "BlurFilter.h"
#include "SobelFilter.h"
#include "RenderTarget.h"
#include "CubeRenderTarget.h"
#include "ShadowMap.h"
#include "Ssao.h"
#include "DrawTexture.h"
#include "Animation.h"
#include "Particle.h"
#include "MapGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

// A Tasks list that is on the Frustum.
static std::vector<ObjectData*>			mRenderTasks;

// AnimationClip, Particle Storage
static std::unordered_map<std::string, AnimationClip*>	mAnimationClips;
static std::unordered_map<std::string, Particle*>		mParticles;

// Alter Instance is Updated
static int mInstanceIsUpdated;

// Stop Object movement while calculating initializing window.
static int isBegining = 0;

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
	static DWORD WINAPI DrawShadowThread(_In_ LPVOID temp);
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
	void BuildSsaoRootSignature(void);
	void BuildDrawMapSignature(void);
	void BuildShadersAndInputLayout(void);
	void BuildRenderItem(void);
	void BuildFrameResource(void);
	void BuildPSO(void);
	void BuildGUIFrame(void);

	void OnKeyboardInput(_In_ const GameTimer& gt);
	void AnimationMaterials(_In_ const GameTimer& gt);
	void UpdateMaterialBuffer(_In_ const GameTimer& gt);
	void UpdateInstanceData(_In_ const GameTimer& gt);
	void UpdateInstanceDataWithBaked(_In_ const GameTimer& gt);
	void UpdateMainPassCB(_In_ const GameTimer& gt);
	void UpdateAnimation(_In_ const GameTimer& gt);
	void UpdateSsaoCB(_In_ const GameTimer& gt);

	void UpdateInstanceBuffer(void);

	void UpdateQuestUI(_In_ const GameTimer& gt);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

public:
	// InputVector
	std::unordered_map<char, char> mInputVector;
	// If Object is on the this Bound, The Object will be drawn shadow.
	DirectX::BoundingSphere mSceneBounds;

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
	RenderItem* CreateGameObject(_In_ std::string Name, _In_ int instance = 1);
	RenderItem* CreateStaticGameObject(_In_ std::string Name, _In_ int instance = 1);
	RenderItem* CreateKinematicGameObject(_In_ std::string Name, _In_ int instance = 1);
	RenderItem* CreateDynamicGameObject(_In_ std::string Name, _In_ int instance = 1);

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
		_In_ ObjectData::RenderType renderType,
		_In_ bool uvMode = true,
		_In_ bool isDrawShadow = true,
		_In_ bool isDrawTexture = false
	);
	void CreateFBXObjectSplitSubmeshs(
		_In_ std::string Name,
		_In_ std::string Path,
		_In_ std::string FileName,
		_Out_ RenderItem* r,
		_In_ DirectX::XMFLOAT3 position,
		_In_ DirectX::XMFLOAT3 rotation,
		_In_ DirectX::XMFLOAT3 scale,
		_In_ ObjectData::RenderType renderType,
		_In_ UINT submeshIDX,
		_In_ bool uvMode = true,
		_In_ bool isDrawShadow = true,
		_In_ bool isDrawTexture = false
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
		_In_ AnimationClip& mAnimClips,
		_In_ bool uvMode = true,
		_In_ bool isDrawShadow = true,
		_In_ bool isDrawTexture = false
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
		_In_ bool isDrawShadow = true,
		_In_ bool isDrawTexture = false
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

public:
	ObjectData* GetData(
		_In_ std::string mName
	);

private:
	static ComPtr<ID3D12RootSignature> mRootSignature;
	static ComPtr<ID3D12RootSignature> mBlurRootSignature;
	static ComPtr<ID3D12RootSignature> mSobelRootSignature;
	static ComPtr<ID3D12RootSignature> mSsaoRootSignature;
	static ComPtr<ID3D12RootSignature> mDrawMapSignature;

	static ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap;
	static ComPtr<ID3D12DescriptorHeap> mGUIDescriptorHeap;

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
	static std::unique_ptr<Ssao>		mSsao;
	static std::unique_ptr<DrawTexture> mDrawTexture;

public:
	BoundingFrustum				mCamFrustum;
	PassConstants				mMainPassCB;
	Camera						mCamera;

	float mMainCameraDeltaRotationY = 0.0f;

	static UINT mFilterCount;

private:
	static CD3DX12_CPU_DESCRIPTOR_HANDLE mTextureHeapDescriptor;
	static CD3DX12_GPU_DESCRIPTOR_HANDLE mTextureHeapGPUDescriptor;

	static std::unordered_map<ObjectData::RenderType, ComPtr<ID3D12PipelineState>> mPSOs;

public:
	std::vector<std::string>							mTextureList;
	std::vector<Texture>								mTextures;
	std::unordered_map<std::string, UINT64>				mGUIResources;
	std::vector<Material>								mMaterials;
	std::unordered_map<std::string, ComPtr<ID3DBlob>>	mShaders;
	std::vector<Light>									mLights;
	std::vector<LightDataConstants>						mLightDatas;

public:
	// Generator Map
	MapGenerator mMapGene;
	// Genetator DB
	ODBC		 mODBC;

	// GUI for Quest
	std::shared_ptr<ImGuiFrameComponent> mQuestParamComp;
	ODBC::UserData mQuestData;

public:
	std::vector<Texture> texList;

	void uploadTexture(
		_In_ Texture& t,
		_In_ bool isCube = false
	);
	void uploadTexture(
		_In_ Texture& tex, 
		_In_ int& width, 
		_In_ int& height, 
		_In_ bool isCube = false
	);
	void uploadMaterial(
		_In_ std::string name,
		_In_ bool isSkyTexture = false
	);
	void uploadMaterial(
		_In_ std::string matName,
		_In_ std::string texName,
		_In_ bool isSkyTexture = false
	);
	void BoxApp::uploadMaterial(
		_In_ std::string matName,
		_In_ std::string tex_Diffuse_Name,
		_In_ std::string tex_Mask_Name,
		_In_ std::string tex_Noise_Name,
		_In_ bool isSkyTexture = false,
		_In_ DirectX::XMFLOAT2 diffuseCount = { 1.0f, 1.0f },
		_In_ DirectX::XMFLOAT4 diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f },
		_In_ DirectX::XMFLOAT3 fresnelR0 = { 0.02f, 0.02f, 0.02f },
		_In_ float roughness = 0.1f
	);
	void uploadLight(
		_In_ Light light
	);
	void BindTexture(
		_Out_ RenderItem* r, 
		_In_ std::string name, 
		int idx, 
		_In_ bool isCubeMap = false
	);
	void BindMaterial(
		_Out_ RenderItem* r,
		_In_ std::string name,
		_In_ bool isCubeMap = false
	);
	void BindMaterial(
		_Out_ RenderItem* r, 
		_In_ std::string name, 
		_In_ std::string maskTexName,
		_In_ std::string noiseTexName, 
		_In_ bool isCubeMap				= false
	);
	void BindMaterial(
		_Out_ RenderItem* r,
		_In_ std::string matName,
		_In_ std::string texName,
		_In_ bool isCubeMap = false
	);
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
	static UINT numGlobalThread;

	static HANDLE shadowRenderTargetEvent[8];
	static HANDLE shadowRecordingDoneEvents[8];
	static HANDLE renderTargetEvent[8];
	static HANDLE recordingDoneEvents[8];

	static HANDLE shadowDrawThreads[8];
	static LPDWORD shadowThreadIndex[8];

	static HANDLE drawThreads[8]; 
	static LPDWORD ThreadIndex[8];
}; 

typedef struct ThreadDrawRenderItem
{
	ThreadDrawRenderItem() : 
		mObject(nullptr),
		mInstanceIDX(0),
		mOnlySubmeshIDX(0),
		mOnTheFrustumCount(0)
	{}

	ThreadDrawRenderItem(
		ObjectData* Object,
		UINT InstanceIDX,
		UINT OnlySubmeshIDX,
		UINT OnTheFrustumCount
	) : mObject(Object), 
		mInstanceIDX(InstanceIDX), 
		mOnlySubmeshIDX(OnlySubmeshIDX), 
		mOnTheFrustumCount(OnTheFrustumCount)
	{}

	ObjectData* mObject = nullptr;
	UINT mInstanceIDX = 0;
	UINT mOnlySubmeshIDX = 0;
	UINT mOnTheFrustumCount = 0;

}ThreadDrawRenderItem;

static std::vector<struct ThreadDrawRenderItem> mThreadDrawRenderItem;
static std::vector<struct ThreadDrawRenderItem> mThreadDrawRenderItems[8];

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