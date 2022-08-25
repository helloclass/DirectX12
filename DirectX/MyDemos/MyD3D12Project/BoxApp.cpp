#include "BoxApp.h"

// 시스템 종료시 전역 변수인 StopThread를 트리거하여 다른 스레드를 종료 시킬 수 있도록 도움을 줍니다. 
Physics mPhys;
bool StopThread = false;

// Animation Thread Resource
DWORD hAnimThreadID;
HANDLE hAnimThread;

// Cloth Thread Resource
DWORD hClothPhysxThreadID;
HANDLE hClothPhysxThread;

// Cloth Thread Resource
DWORD hTextureBrushThreadID;
HANDLE hTextureBrushThread;

std::list<InstanceData>::iterator	InstanceIterator;
std::list<BoundingBox>::iterator	BoundsIterator;

BoxApp::BoxApp(HINSTANCE hInstance)
	: D3DApp(hInstance){}

// Pipeline State Object Type List
std::unordered_map<ObjectData::RenderType, ComPtr<ID3D12PipelineState>> BoxApp::mPSOs;
// Post Process RTV ( Will use to Blur, Sobel Calc)
std::unique_ptr<RenderTarget> BoxApp::mOffscreenRT = nullptr;

// Degree of Blur
UINT BoxApp::mFilterCount = 0;

std::unique_ptr<BlurFilter>		BoxApp::mBlurFilter;
std::unique_ptr<SobelFilter>	BoxApp::mSobelFilter;
std::unique_ptr<ShadowMap>		BoxApp::mShadowMap;
std::unique_ptr<Ssao>			BoxApp::mSsao;
std::unique_ptr<DrawTexture>	BoxApp::mDrawTexture;

ComPtr<ID3D12DescriptorHeap> BoxApp::mSrvDescriptorHeap = nullptr;
ComPtr<ID3D12DescriptorHeap> BoxApp::mGUIDescriptorHeap = nullptr;

ComPtr<ID3D12RootSignature> BoxApp::mRootSignature = nullptr;
ComPtr<ID3D12RootSignature> BoxApp::mBlurRootSignature = nullptr;
ComPtr<ID3D12RootSignature> BoxApp::mSobelRootSignature = nullptr;
ComPtr<ID3D12RootSignature> BoxApp::mSsaoRootSignature = nullptr;
ComPtr<ID3D12RootSignature> BoxApp::mDrawMapSignature = nullptr;

CD3DX12_CPU_DESCRIPTOR_HANDLE BoxApp::mTextureHeapDescriptor;
CD3DX12_GPU_DESCRIPTOR_HANDLE BoxApp::mTextureHeapGPUDescriptor;

// Draw Thread Resource
UINT BoxApp::numGlobalThread = 8;

// Thread Trigger Event (Fence)
HANDLE BoxApp::shadowRenderTargetEvent[8];
HANDLE BoxApp::shadowRecordingDoneEvents[8];
HANDLE BoxApp::renderTargetEvent[8];
HANDLE BoxApp::recordingDoneEvents[8];

HANDLE BoxApp::shadowDrawThreads[8];
LPDWORD BoxApp::shadowThreadIndex[8];

HANDLE BoxApp::drawThreads[8];
LPDWORD BoxApp::ThreadIndex[8] = { 0 };

static std::unique_ptr<UploadBuffer<PassConstants>>				PassCB;
static std::unique_ptr<UploadBuffer<SsaoConstants>>				SsaoCB;
static std::unique_ptr<UploadBuffer<RateOfAnimTimeConstants>>	RateOfAnimTimeCB;
static std::unique_ptr<UploadBuffer<LightDataConstants>>		LightBufferCB;
static std::unique_ptr<UploadBuffer<InstanceData>>				InstanceBuffer;
static std::unique_ptr<UploadBuffer<MaterialData>>				MaterialBuffer;
static std::unique_ptr<UploadBuffer<PmxAnimationData>>			PmxAnimationBuffer;

// Cloth Update SynchronizationEvent;
static HANDLE mClothReadEvent;
static HANDLE mClothWriteEvent;
static HANDLE mAnimationReadEvent;
static HANDLE mAnimationWriteEvent;

//bool isMousePressed = false;
POINT mLastMousePos;

BoxApp::~BoxApp()
{
	StopThread = true;

	SetEvent(mClothWriteEvent);
	SetEvent(mAnimationWriteEvent);

	mODBC.OnDestroy();

	std::unordered_map<std::string, ObjectData*>::iterator& it = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator& itEnd = mGameObjectDatas.end();
	for (; it != itEnd; it++)
	{
		ObjectData* obj = it->second;
		for (int animName = 0; animName < obj->animNameLists.Size(); animName++)
			delete(obj->animNameLists[animName]);

		for (int animV = 0; animV < obj->mAnimVertex.size(); animV++)
			for (int animH = 0; animH < obj->mAnimVertex[animV].size(); animH++)
				delete[](obj->mAnimVertex[animV][animH]);

		delete(obj);
	}

	while (mGameObjects.size() > 0)
	{
		RenderItem* obj = *mGameObjects.begin();

		delete(obj);
		mGameObjects.pop_front();
	}

#ifdef DEBUG
	_CrtDumpMemoryLeaks();
#endif
}

bool isUseGlobalPose = false;

ImVec4 mGlobalVec = ImVec4();
ImVec4 mLocalScale = ImVec4();

/*
	ThreadClothPhysxFunc
	It is a thread that is calculated Cloth Physx and Morph Operation and stored to Result Data on vertices each of GameObjects.
	The thread use Cloth Physx that is version Physx 3.4.
*/
DWORD WINAPI ThreadClothPhysxFunc(LPVOID prc)
{
	std::vector<PxClothParticle> vertices;
	std::vector<PxU32> primitives;

	UINT vOffset;
	UINT iOffset;

	UINT vSize;
	UINT iSize;

	// Update Vertex
	float* mClothBuffer = NULL;

	// 64MB
	FIBITMAP* mWeightImage = NULL;
	int width, height;

	std::list<RenderItem*>::iterator& obj		= mGameObjects.begin();
	std::list<RenderItem*>::iterator& objEnd	= mGameObjects.end();

	// Traversal each GameObject, than updated and got a new Cloth Data of Vertex
	for (; obj != objEnd; obj++)
	{
		RenderItem* _RenderItem = *obj;
		// The new Cloth Vertex Data will be stored in here.
		ObjectData* _RenderData = mGameObjectDatas[_RenderItem->mName];

		if (!_RenderData)	
			continue;

		_RenderData->mClothes.resize(_RenderData->SubmeshCount);
		_RenderData->mClothBinedBoneIDX.resize(_RenderData->SubmeshCount);

		// Initialize 
		for (UINT i = 0; i < _RenderData->SubmeshCount; i++)
		{
			_RenderData->mClothes[i] = NULL;
			_RenderData->mClothBinedBoneIDX[i] = -1;
		}

		_RenderData->mRigidbody.resize(_RenderItem->SubmeshCount);
		for (UINT submesh = 0; submesh < _RenderItem->SubmeshCount; submesh++)
		{
			if (!_RenderData->isRigidBody[submesh])
			{
				continue;
			}

			if (_RenderData->mFormat == "PMX")
			{
				vOffset = _RenderData->mGeometry.DrawArgs[submesh].BaseVertexLocation;
				iOffset = _RenderData->mGeometry.DrawArgs[submesh].StartIndexLocation;

				vSize = _RenderData->mGeometry.DrawArgs[submesh].VertexSize;
				iSize = _RenderData->mGeometry.DrawArgs[submesh].IndexSize;

				vertices.clear();
				vertices.resize(vSize);
				primitives.clear();
				primitives.resize(iSize);

				int mOffset = 0;
				int mIDXMin;

				std::set<int>::iterator& vIDX = _RenderData->vertBySubmesh[submesh].begin();
				std::set<int>::iterator& vEndIDX = _RenderData->vertBySubmesh[submesh].end();

				// Physx의 PxClothFabric를 생성하기 위해 기본 cloth의 버텍스를 로드하고,
				// 이제 이 cloth가 적용된 오브젝트는 Physx에서 전해주는 Vertex 정보에 따라
				// 유동적으로 움직이기 때문에 기존의 Vertex 값을 0.0으로 초기화 하고
				// 이후 유동된 값을 다시 받을 준비를 한다.
				while (vIDX != vEndIDX)
				{
					vertices[mOffset].pos[0] = _RenderData->mVertices[*vIDX].Pos.x;
					vertices[mOffset].pos[1] = _RenderData->mVertices[*vIDX].Pos.y;
					vertices[mOffset].pos[2] = _RenderData->mVertices[*vIDX].Pos.z;
					vertices[mOffset].invWeight = 1.0f;

					_RenderData->mVertices[*vIDX].Pos.x = 0.0f;
					_RenderData->mVertices[*vIDX].Pos.y = 0.0f;
					_RenderData->mVertices[*vIDX].Pos.z = 0.0f;

					vIDX++;
					mOffset++;
				}

				// PxClothFabric을 생성하기 위해 인덱스를 저장한다.
				mOffset = 0;
				mIDXMin = _RenderData->mModel.indices[iOffset];
				// 
				for (UINT i = iOffset; i < iOffset + iSize; i++)
				{
					if (mIDXMin > _RenderData->mModel.indices[i])
						mIDXMin = _RenderData->mModel.indices[i];
				}

				// weight가 0.0인 Vertex를 제외하고 남은 Vertex 끼리 재결합을 시키기 위하여
				// 만일 기존의 인덱스에서 weight가 0.0이 아닌 버텍스가 발견이 되었다면,
				// 중간에 weight가 0.0이였던 인덱스를 모두 제외하고 Weight가 0.0 초과인 버텍스
				// 끼리 순열을 만들기 위해 mIDXMin을 제거한다.
				bool isSuccessedScaled = false;
				for (UINT i = iOffset; i < iOffset + iSize; i++)
				{
					// 위에서 잘못된 인덱스를 로드하였던가, 부분집합이 전체 인덱스 범위에서 벗어났을 경우
					if (_RenderData->mModel.indices[i] - mIDXMin < 0)
						throw std::runtime_error("Invaild Indices..");
					else if (_RenderData->mModel.indices[i] - mIDXMin == 0)
						isSuccessedScaled = true;

					primitives[mOffset++] = _RenderData->mModel.indices[i] - mIDXMin;
				}

				if (!isSuccessedScaled)
					throw std::runtime_error("Invaild Indices..");

				// Cloth 데이터를 생성
				PxTriangleMeshDesc meshDesc;
				meshDesc.points.data = (void*)&vertices.data()->pos;
				meshDesc.points.count = static_cast<PxU32>(vertices.size());
				meshDesc.points.stride = sizeof(PxClothParticle);

				meshDesc.triangles.data = (void*)primitives.data();
				meshDesc.triangles.count = static_cast<PxU32>(primitives.size() / 3);
				meshDesc.triangles.stride = sizeof(PxU32) * 3;

				PxShape* mRigidMesh = mPhys.CreateTriangleMeshToShape(meshDesc);
				PxTransform t(physx::PxVec3(0.0f, 0.0f, 0.0f));

				_RenderData->mRigidbody[submesh] = mPhys.CreateKinematic(t, mRigidMesh, 1.0f);
			}
		}

		// 중력에 영향을 받지 않을, 즉 옷감 모델을 원본 모델과 고정시켜주는 역할을 하는 버텍스 리스트
		_RenderData->srcFixVertexSubmesh.resize(_RenderItem->SubmeshCount);
		_RenderData->dstFixVertexSubmesh.resize(_RenderItem->SubmeshCount);
		// 여려 힘에 의해 동적으로 위치가 변하는 버텍스 리스트 
		_RenderData->srcDynamicVertexSubmesh.resize(_RenderItem->SubmeshCount);
		_RenderData->dstDynamicVertexSubmesh.resize(_RenderItem->SubmeshCount);


		for (UINT submesh = 0; submesh < _RenderItem->SubmeshCount; submesh++)
		{
			// 옷감 서브메쉬가 아니면 스킵
			if (!_RenderData->isCloth[submesh])	continue;

			//if (_RenderData->mFormat == "FBX")
			//{
			//	vOffset = _RenderData->mGeometry.DrawArgs[submesh].BaseVertexLocation;
			//	iOffset = _RenderData->mGeometry.DrawArgs[submesh].StartIndexLocation;

			//	vSize = _RenderData->mGeometry.DrawArgs[submesh].VertexSize;
			//	iSize = _RenderData->mGeometry.DrawArgs[submesh].IndexSize;

			//	vertices.resize(vSize);
			//	primitives.resize(iSize);

			//	for (UINT v = 0; v < vSize; v++) {
			//		vertices[v].pos[0] = _RenderData->mVertices[vOffset + v].Pos.x;
			//		vertices[v].pos[1] = _RenderData->mVertices[vOffset + v].Pos.y;
			//		vertices[v].pos[2] = _RenderData->mVertices[vOffset + v].Pos.z;
			//		vertices[v].invWeight = _RenderData->mClothWeights[vOffset + v];
			//	}

			//	// 
			//	for (UINT i = 0; i < iSize; i++) {
			//		primitives[i] = _RenderData->mIndices[iOffset + i];
			//	}

			//	primitives.pop_back();

			//	PxClothMeshDesc meshDesc;
			//	meshDesc.points.data = (void*)&vertices.data()->pos;
			//	meshDesc.points.count = static_cast<PxU32>(vertices.size());
			//	meshDesc.points.stride = sizeof(PxClothParticle);

			//	meshDesc.invMasses.data = (void*)&vertices.data()->invWeight;
			//	meshDesc.invMasses.count = static_cast<PxU32>(vertices.size());
			//	meshDesc.invMasses.stride = sizeof(PxClothParticle);

			//	meshDesc.triangles.data = (void*)primitives.data();
			//	meshDesc.triangles.count = static_cast<PxU32>(primitives.size() / 3);
			//	meshDesc.triangles.stride = sizeof(PxU32) * 3;

			//	_RenderData->mClothes[submesh] = mPhys.LoadCloth(vertices.data(), meshDesc);
			//}
			//else if (_RenderData->mFormat == "PMX")
			//{
				vOffset = _RenderData->mGeometry.DrawArgs[submesh].BaseVertexLocation;
				iOffset = _RenderData->mGeometry.DrawArgs[submesh].StartIndexLocation;

				vSize = _RenderData->mGeometry.DrawArgs[submesh].VertexSize;
				iSize = _RenderData->mGeometry.DrawArgs[submesh].IndexSize;

				// 
				vertices.clear();
				primitives.clear();
				vertices.resize(0);
				primitives.resize(0);

				bool hasZero(false), hasOne(false);
				int primCount = 0;

				/////////////////////////////////////////////////////////////////////////
				// Make new submesh Indices with vertices that is not weight zero.
				/////////////////////////////////////////////////////////////////////////
				for (UINT idx = iOffset; idx < iOffset + iSize; idx += 3)
				{
					// Skip Vertex that is weight zero.
					if (_RenderData->mClothWeights[_RenderData->mModel.indices[idx]] == 0.0f ||
						_RenderData->mClothWeights[_RenderData->mModel.indices[idx + 1]] == 0.0f ||
						_RenderData->mClothWeights[_RenderData->mModel.indices[idx + 2]] == 0.0f)
					{
						continue;
					}

					primitives.push_back(_RenderData->mModel.indices[idx]);
					primitives.push_back(_RenderData->mModel.indices[idx + 1]);
					primitives.push_back(_RenderData->mModel.indices[idx + 2]);

					primCount++;
				}

				/////////////////////////////////////////////////////////////////////////

				DirectX::XMVECTOR posVector;

				std::unordered_map<UINT, UINT> testest;

				RGBQUAD color;
				int uvX, uvY;

				/////////////////////////////////////////////////////////////////////////
				// Load Weight Image
				// The Weight Image has a information about weight by vertex and Fixed Vertex from Gravity.
				/////////////////////////////////////////////////////////////////////////
				int texIDX = _RenderData->mModel.materials[submesh].diffuse_texture_index;
				std::wstring wname = _RenderData->mModel.textures[texIDX];

				int extIDX = (int)wname.find(L".png");
				wname = wname.substr(0, extIDX);
				wname.append(L"_Weight.png");

				std::string name;
				name.assign(wname.begin(), wname.end());

				mWeightImage = d3dUtil::loadImage(
					std::string(""),
					name,
					std::string(""),
					width,
					height
				);

				if (!mWeightImage)
				{
					throw std::runtime_error("Can't Found Weight Images!!");
				}

				///////////////////////////////////////////////////////////////////////

				int mOffset = 0;
				int mRemover = -1;

				for (
					std::set<int>::iterator& vIDX = _RenderData->vertBySubmesh[submesh].begin();
					vIDX != _RenderData->vertBySubmesh[submesh].end();
					vIDX++
					)
				{
					if (mRemover != -1)
					{
						_RenderData->vertBySubmesh[submesh].erase(mRemover);
						mRemover = -1;
					}

					posVector = DirectX::XMLoadFloat3(&_RenderData->mVertices[*vIDX].Pos);

					if (_RenderData->mClothWeights[*vIDX] == 0.0f) {
						mRemover = *vIDX;

						continue;
					}

					uvX = (int)((float)width  * _RenderData->mVertices[*vIDX].TexC.x);
					uvY = (int)((float)height * (1.0f - _RenderData->mVertices[*vIDX].TexC.y));

					FreeImage_GetPixelColor(mWeightImage, uvX, uvY, &color);

					if (
						color.rgbGreen == 0 &&
						color.rgbBlue == 0
						)
					{
						_RenderData->mClothWeights[*vIDX] = (float)(255 - color.rgbRed) / 255.0f;
					}

					///////////////////////////

					physx::PxClothParticle part;
					part.pos[0] = posVector.m128_f32[0];
					part.pos[1] = posVector.m128_f32[1];
					part.pos[2] = posVector.m128_f32[2];
					part.invWeight = _RenderData->mClothWeights[*vIDX];

					vertices.push_back(part);

					///////
					// PPD2Vertices MAP
					///////
					if (_RenderData->mClothWeights[*vIDX] != 0.0f)
					{
						_RenderData->srcDynamicVertexSubmesh[submesh].push_back(mOffset);
						_RenderData->dstDynamicVertexSubmesh[submesh].push_back(&_RenderData->mVertices[*vIDX].Pos);
					}
					else
					{
						_RenderData->srcFixVertexSubmesh[submesh].push_back(mOffset);
						_RenderData->dstFixVertexSubmesh[submesh].push_back(&_RenderData->mVertices[*vIDX].Pos);
					}
					///////
					testest[*vIDX] = mOffset;

					mOffset++;
				}

				FreeImage_Unload(mWeightImage);
				//FreeImage_DeInitialise();

				for (int iIDX = 0; iIDX < primitives.size(); iIDX++)
				{
					primitives[iIDX] = testest[primitives[iIDX]];
				}

				// 
				size_t vertSize = vertices.size();

				if (vertices.size() > 0)
				{
					PxClothMeshDesc meshDesc;
					meshDesc.points.data = (void*)&vertices.data()->pos;
					meshDesc.points.count = static_cast<PxU32>(vertices.size());
					meshDesc.points.stride = sizeof(PxClothParticle);

					meshDesc.invMasses.data = (void*)&vertices.data()->invWeight;
					meshDesc.invMasses.count = static_cast<PxU32>(vertices.size());
					meshDesc.invMasses.stride = sizeof(PxClothParticle);

					meshDesc.triangles.data = (void*)primitives.data();
					meshDesc.triangles.count = static_cast<PxU32>(primCount);
					meshDesc.triangles.stride = sizeof(PxU32) * 3;

					_RenderData->mClothes[submesh] = mPhys.LoadCloth(vertices.data(), meshDesc);
				}
				else
				{
					_RenderData->isCloth[submesh] = false;
					_RenderData->mClothes[submesh] = NULL;
				}
			//}
		} // Loop Submesh
	} // Loop GameObjects

	// ClothParticleInfo
	PxClothParticleData* ppd = NULL;
	// Cloth Position 
	Vertex* mClothOffset = NULL;
	UINT vertexSize;

	// 초기 베리어(533)를 우선 통과시키기 위해 강제로 이벤트를 킨다.
	ResetEvent(mClothReadEvent);
	SetEvent(mClothWriteEvent);

	int loop = 0;
	bool loopUpdate = false;

	ObjectData* _RenderData = NULL;
	Vertex* clothPos = NULL;

	std::unordered_map<std::string, ObjectData*>::iterator mObj;
	std::unordered_map<std::string, ObjectData*>::iterator mEndObj = mGameObjectDatas.end();

	DirectX::XMFLOAT4X4 mRootF44;
	DirectX::XMMATRIX mRootMat;
	DirectX::XMVECTOR s, p, q;
	DirectX::XMVECTOR op, oq;
	DirectX::XMVECTOR delta, resP, resQ;

	UINT submeshIDX = 0;

	std::vector<int>::iterator mSrcFixIter;
	std::vector<DirectX::XMFLOAT3*>::iterator mDstFixIter;
	std::vector<DirectX::XMFLOAT3*>::iterator mDstEndFixIter;

	std::vector<int>::iterator mSrcDynamicIter;
	std::vector<DirectX::XMFLOAT3*>::iterator mDstDynamicIter;
	std::vector<DirectX::XMFLOAT3*>::iterator mDstEndDynamicIter;

	physx::PxVec3*		mSrcIterPos = NULL;

	while (!StopThread)
	{
		// Cloth Vertices가 최신화 되었으니, Draw에서 쓰도록 하자
		WaitForSingleObject(mClothWriteEvent, INFINITE);

		if (StopThread) break;

		mObj = mGameObjectDatas.begin();

		// Update Cloth Vertices
		while (mObj != mEndObj)
		{
			if (!loopUpdate)	break;

			_RenderData = mObj->second;

			clothPos = _RenderData->mVertices.data();

			// 애니메이션, 물리 시뮬레이션이 없는 오브젝트 스킵
			if (_RenderData->mAnimVertex.size() < 1 &&
				_RenderData->mBoneMatrix.size() < 1)
			{
				_RenderData->isDirty = false;
				mObj++;
				continue;
			}

			_RenderData->isDirty = true;

			for (submeshIDX = 0; submeshIDX < _RenderData->SubmeshCount; submeshIDX++)
			{
				// 리지드 바디의 위치를 최신화
				if (_RenderData->isRigidBody[submeshIDX])
				{
					// pelvis 뼈를 기준으로 속도를 측정.
					// Calc O
					mRootF44 = _RenderData->mBoneMatrix[0][8];
					mRootMat = DirectX::XMLoadFloat4x4(&mRootF44);

					DirectX::XMMatrixDecompose(&s, &oq, &op, mRootMat);

					// Calc Q
					mRootF44 = _RenderData->mBoneMatrix[_RenderData->currentFrame][8];
					mRootMat = DirectX::XMLoadFloat4x4(&mRootF44);

					DirectX::XMMatrixDecompose(&s, &q, &p, mRootMat);

					// Calc new T
					resP = p - op;

					// Calc new Q
					delta = q - oq;

					resQ.m128_f32[0] = 0.0f;
					resQ.m128_f32[1] = -1.0f;
					resQ.m128_f32[2] = 0.0f;
					resQ.m128_f32[3] = 1.0f;

					delta = DirectX::XMQuaternionNormalize(delta);
					resQ = DirectX::XMQuaternionNormalize(resQ);

					resQ = resQ + delta;

					resQ = DirectX::XMQuaternionNormalize(resQ);

					/////////////////////////////////////

					// 다음 포즈를 계산
					physx::PxTransform mPose(
						physx::PxVec3(
							resP.m128_f32[0],
							resP.m128_f32[1],
							resP.m128_f32[2]
						),
						physx::PxQuat(
							resQ.m128_f32[0],
							resQ.m128_f32[1],
							resQ.m128_f32[2],
							resQ.m128_f32[3]
						)
					);

					_RenderData->mRigidbody[submeshIDX]->setKinematicTarget(mPose);
				}

				// 옷감의 연속적인 모핑, 위치 최신화 
				if (_RenderData->isCloth[submeshIDX])
				{
					// Adapted Cloth Physx
					// pelvis 뼈를 기준으로 속도를 측정.
					// Calc O
					mRootF44 = _RenderData->mBoneMatrix[0][8];
					mRootMat = DirectX::XMLoadFloat4x4(&mRootF44);

					DirectX::XMMatrixDecompose(&s, &oq, &op, mRootMat);

					// Calc Q
					mRootF44 = _RenderData->mBoneMatrix[_RenderData->currentFrame][8];
					mRootMat = DirectX::XMLoadFloat4x4(&mRootF44);

					DirectX::XMMatrixDecompose(&s, &q, &p, mRootMat);

					// Calc new T
					resP = p - op;

					// Calc new Q
					delta = q - oq;

					//delta = DirectX::XMQuaternionNormalize(delta);

					delta.m128_f32[0] *= mLocalScale.x;
					delta.m128_f32[1] *= mLocalScale.y;
					delta.m128_f32[2] *= mLocalScale.z;

					resQ = delta;

					resQ = DirectX::XMQuaternionNormalize(resQ);

					resP.m128_f32[1] = -10.0f;

					physx::PxTransform mPose(
						physx::PxVec3(
							resP.m128_f32[0],
							resP.m128_f32[1],
							resP.m128_f32[2]
						),
						physx::PxQuat(
							resQ.m128_f32[0],
							resQ.m128_f32[1],
							resQ.m128_f32[2],
							resQ.m128_f32[3]
						)
					);

					_RenderData->mClothes[submeshIDX]->setTargetPose(mPose);

					if (isUseGlobalPose)
					{
						physx::PxTransform mGlobalPose(
							physx::PxVec3(
								mGlobalVec.x,
								mGlobalVec.y,
								mGlobalVec.z
							)
						);

						_RenderData->mClothes[submeshIDX]->setGlobalPose(mGlobalPose);
					}

					mClothOffset =
						clothPos;
					vertexSize =
						_RenderData->mDesc[submeshIDX].VertexSize;

					mSrcFixIter = _RenderData->srcFixVertexSubmesh[submeshIDX].begin();
					mDstFixIter = _RenderData->dstFixVertexSubmesh[submeshIDX].begin();
					mDstEndFixIter = _RenderData->dstFixVertexSubmesh[submeshIDX].end();

					// 결과 옷감 Vertices List를 ObjectData에 전사한다. (고정 버텍스)
					ppd = _RenderData->mClothes[submeshIDX]->lockParticleData(PxDataAccessFlag::eWRITABLE);

					while (mDstFixIter != mDstEndFixIter)
					{
						mSrcIterPos = &ppd->particles[*mSrcFixIter].pos;

						memcpy(mSrcIterPos, (*mDstFixIter), sizeof(float) * 3);

						mSrcFixIter++;
						mDstFixIter++;
					}

					ppd->unlock();

					mSrcDynamicIter = _RenderData->srcDynamicVertexSubmesh[submeshIDX].begin();
					mDstDynamicIter = _RenderData->dstDynamicVertexSubmesh[submeshIDX].begin();
					mDstEndDynamicIter = _RenderData->dstDynamicVertexSubmesh[submeshIDX].end();
					 
					// 결과 옷감 Vertices List를 ObjectData에 전사한다. (유동 버텍스)
					ppd = _RenderData->mClothes[submeshIDX]->lockParticleData(PxDataAccessFlag::eREADABLE);

					while (mDstDynamicIter != mDstEndDynamicIter)
					{
						mSrcIterPos = &ppd->particles[*mSrcDynamicIter].pos;

						memcpy((*mDstDynamicIter), mSrcIterPos, sizeof(float) * 3);

						mSrcDynamicIter++;
						mDstDynamicIter++;
					}

					ppd->unlock();

				} // if (_RenderData->isCloth[submeshIDX])
			} // for Submesh

			mObj++;
		} // for GameObject

		// Cloth Vertices가 최신화 되었으므로 Draw에서 사용을 할 수 있도록 이벤트를 잠근다.
		ResetEvent(mClothWriteEvent);
		SetEvent(mClothReadEvent);

		// 빈번한 옷감 시뮬레이션의 최신화는 FPS에 치명적이므로, 프레임에 무리가 없을 만큼만
		// 최신화 빈도를 줄인다.
		if (loop++ == 5)
		{
			loop = 0;
			mPhys.Update();
			loopUpdate = true;
		}
		else
		{
			loopUpdate = false;
		}

	}

	// Destroier
	std::unordered_map<std::string, ObjectData*>::iterator objDest = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator objDestEnd = mGameObjectDatas.end();
	for (; objDest != objDestEnd; objDest++)
	{
		ObjectData* _RenderData = objDest->second;

		for (UINT submeshIDX = 0; submeshIDX < _RenderData->SubmeshCount; submeshIDX++)
		{
			objDest->second->vertBySubmesh[submeshIDX].clear();
			objDest->second->srcFixVertexSubmesh[submeshIDX].clear();
			objDest->second->dstFixVertexSubmesh[submeshIDX].clear();
			objDest->second->srcDynamicVertexSubmesh[submeshIDX].clear();
			objDest->second->dstDynamicVertexSubmesh[submeshIDX].clear();

			if (_RenderData->isCloth[submeshIDX])
			{
				_RenderData->mClothes[submeshIDX]->release();
			}
		}

		objDest->second->vertBySubmesh.clear();
		objDest->second->srcFixVertexSubmesh.clear();
		objDest->second->dstFixVertexSubmesh.clear();
		objDest->second->srcDynamicVertexSubmesh.clear();
		objDest->second->dstDynamicVertexSubmesh.clear();
	}

	return (DWORD)(0);
}

// Physx Thread Section
DWORD WINAPI ThreadAnimFunc(LPVOID prc)
{
	// ���� �ʱ⿡ Cloth Vertices Update�� ���� ���� �Ǿ�� �ϱ� ������
	// ClothWriteEvent�� On
	ResetEvent(mAnimationReadEvent);
	SetEvent(mAnimationWriteEvent);

	std::unordered_map<std::string, ObjectData*>::iterator obj;
	std::unordered_map<std::string, ObjectData*>::iterator objEnd;

	ObjectData* _RenderData = NULL;

	int currentFrame;
	float residueTime;

	Vertex* VertexPos = NULL;
	Vertex* mVertexOffset = NULL;
	UINT vertexSize = 0;

	float* animPos = NULL;
	int animVertSize = 0;
	int vcount = 0;

	int submeshIDX = 0;
	int vertexIDX = 0;
	int i = 0;

	DirectX::XMFLOAT4X4* BoneOriginOffset = NULL;
	DirectX::XMFLOAT4X4* BoneOffsetOfFrame = NULL;

	PmxAnimationData pmxAnimData;
	RateOfAnimTimeConstants	mRateOfAnimTimeCB;

	struct ObjectData::_VERTEX_MORPH_DESCRIPTOR mMorph;
	float weight = 0.0f;
	Vertex* DestPos = NULL;
	DirectX::XMFLOAT3* DesctinationPos = NULL;
	float* DefaultPos = NULL;

	int mIDX = 0;

	while (!StopThread)
	{
		WaitForSingleObject(mAnimationWriteEvent, INFINITE);
		if (StopThread) break;

		// Update Cloth Vertices
		obj = mGameObjectDatas.begin();
		objEnd = mGameObjectDatas.end();

		while (obj != objEnd)
		{
			_RenderData = obj->second;

			// 애니메이션 정보가 없다면 업데이트 패스
			if (_RenderData->mAnimVertex.size() < 1 &&
				_RenderData->mBoneMatrix.size() < 1)
			{
				_RenderData->isDirty = false;
				obj++;
				continue;
			}

			// 애니메이션을 통해 vert가 업데이트 되었으므로 디스크립터에 업데이트 할 것을 요청
			_RenderData->isDirty = true;

			// 
			_RenderData->isAnim = true;
			_RenderData->isLoop = true;

			// 애니메이션의 인덱스를 업데이트
			currentFrame =
			(int)(_RenderData->currentDelayPerSec / _RenderData->durationOfFrame[0]);

			// 애니메이션 인덱스가 엔드 인덱스보다 크다면 다시 시작 인덱스로 반환
			if ((_RenderData->endAnimIndex) <= currentFrame) {
				currentFrame = (int)_RenderData->beginAnimIndex;
				_RenderData->currentDelayPerSec = (_RenderData->beginAnimIndex * _RenderData->durationOfFrame[0]);
			}
			// 인덱스가 변형될 때 마다, 현재 프레임이 업데이트 되었음을 알려준다.
			if (_RenderData->currentFrame != currentFrame) {
				_RenderData->currentFrame = currentFrame;
				_RenderData->updateCurrentFrame = true;
			}

			// 다음 프레임 까지 남은 시간을 계산.
			residueTime = _RenderData->currentDelayPerSec - (_RenderData->currentFrame * _RenderData->durationOfFrame[0]);
			_RenderData->mAnimResidueTime = residueTime;

			if (obj->second->mFormat == "FBX") {
				VertexPos = obj->second->mVertices.data();
				mVertexOffset = NULL;
				vertexSize = 0;

				for (submeshIDX = 0; submeshIDX < (int)obj->second->SubmeshCount; submeshIDX++)
				{
					// 옷 감 메쉬가 아닌 경우에만 애니메이션 처리를 할 것 
					if (!obj->second->isCloth[submeshIDX])
					{
						if (obj->second->mAnimVertexSize[submeshIDX] == 0)
							continue;

						mVertexOffset =
							VertexPos +
							obj->second->mDesc[submeshIDX].BaseVertexLocation;

						animPos = obj->second->mAnimVertex[submeshIDX][obj->second->currentFrame];
						animVertSize = obj->second->mAnimVertexSize[submeshIDX];

						vcount = 0;
						for (vertexIDX = 0; vertexIDX < animVertSize; vertexIDX++)
						{
							mVertexOffset[vertexIDX].Pos.x = animPos[vcount++];
							mVertexOffset[vertexIDX].Pos.y = animPos[vcount++];
							mVertexOffset[vertexIDX].Pos.z = animPos[vcount++];
							vcount++;	// skip w
						}

					} // !mGameObjects[gameIdx]->cloth[submeshIDX
				} // Loop of GameObject Submesh

				obj->second->updateCurrentFrame = false;
			}

			if (obj->second->mFormat == "PMX") {
				if (obj->second->updateCurrentFrame) {
					BoneOriginOffset = obj->second->mOriginRevMatrix.data();
					BoneOffsetOfFrame = obj->second->mBoneMatrix[obj->second->currentFrame].data();

					for (i = 0; i < obj->second->mModel.bone_count; i++)
					{
						pmxAnimData.mOriginMatrix = BoneOriginOffset[i];
						pmxAnimData.mMatrix = BoneOffsetOfFrame[i];

						PmxAnimationBuffer->CopyData(i, pmxAnimData);
					}

					obj->second->updateCurrentFrame = false;
				}

				// �ܺ� �ð� ������Ʈ
				mRateOfAnimTimeCB.rateOfAnimTime = obj->second->mAnimResidueTime;
				RateOfAnimTimeCB->CopyData(0, mRateOfAnimTimeCB);
			}
			////

			//////////////////////////////////////////////////////////////////////////////
			// Update Morph
			//////////////////////////////////////////////////////////////////////////////

			for (mIDX = 0; mIDX < _RenderData->mMorphDirty.size(); mIDX++)
			{
				if (_RenderData->mMorphDirty[mIDX] == 0)
					continue;

				_RenderData->mMorphDirty[mIDX] = 0;

				mMorph = _RenderData->mMorph[mIDX];
				weight = mMorph.mVertWeight;

				for (i = 0; i < mMorph.mVertIndices.size(); i++)
				{
					DesctinationPos = &_RenderData->mVertices[mMorph.mVertIndices[i]].Pos;
					DefaultPos = _RenderData->mModel.vertices[mMorph.mVertIndices[i]].position;

					DesctinationPos->x = DefaultPos[0] + mMorph.mVertOffset[i][0] * weight;
					DesctinationPos->y = DefaultPos[1] + mMorph.mVertOffset[i][1] * weight;
					DesctinationPos->z = DefaultPos[2] + mMorph.mVertOffset[i][2] * weight;
				}
			}

			obj++;
		}

		// Cloth Vertices�� Read Layer�� ����
		ResetEvent(mAnimationWriteEvent);
		SetEvent(mAnimationReadEvent);
	}

	return (DWORD)(0);
}

//void Brushing(LPVOID temp)
DWORD Brushing(LPVOID threadIDX)
{
	UINT idx = (UINT)threadIDX;

	float vx, vy;

	while (!StopThread)
	{
		WaitForSingleObject(mBrushEvent[idx], INFINITE);

		// Compute picking ray in view space.
		// [0, 600] -> [-1, 1] [0, 800] -> [-1, 1]
		vx =
			(mClickedPosX[idx] - mLastMousePos.x) /
			(float)(mScreenWidth[idx]);
		vy =
			(mClickedPosY[idx] - mLastMousePos.y) /
			(float)(mScreenHeight[idx]);

		mBrushPosition[idx].x =
			mBrushOrigin[idx].x +
			vx;
		mBrushPosition[idx].y =
			mBrushOrigin[idx].y -
			vy;

		ResetEvent(mBrushEvent[idx]);
	}

	return (DWORD)(0);
}

// Client
bool BoxApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

 	mInstanceIsUpdated = 0;

	mODBC.OnInitDialog();

	mMapGene.GenerateMap(
		"Map1",									// MapName
		{ 0.0f, 0.0f, 0.0f },					// MapPosition
		30,										// MapSize
		GeneratorParameter::Random,				// HoleCount
		GeneratorParameter::Random,				// HoleSize
		GeneratorParameter::VeryHigh,			// HightSize
		GeneratorParameter::VeryHigh,			// MaxHight
		GeneratorParameter::Random,				// HeightSmoothing
		EnableDisable::Enabled,
		EnableDisable::Enabled,
		3,
		EnableDisable::Enabled,
		25,
		25,
		50,
		EnableDisable::Enabled,
		25
	);

	//mSSAOCommandList->Close();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Preprocessing Main _Awake
	_Awake(this);

	mCamera.SetPosition(0.0f, 0.0f, -15.0f);

	mBlurFilter = std::make_unique<BlurFilter>(md3dDevice.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	mSobelFilter = std::make_unique<SobelFilter>(md3dDevice.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	mOffscreenRT = std::make_unique<RenderTarget>(md3dDevice.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(), 6000, 6000);
	mSsao = std::make_unique<Ssao>(md3dDevice.Get(), mCommandList.Get(), mClientWidth, mClientHeight);
	mDrawTexture = std::make_unique<DrawTexture>(md3dDevice.Get(), 2048, 2048, DXGI_FORMAT_R8G8B8A8_UNORM);

	HWND mHandle = MainWnd();
	InitGUI(mHandle, md3dDevice, mGUIDescriptorHeap);

	BuildRootSignature();
	BuildBlurRootSignature();
	BuildSobelRootSignature();
	BuildSsaoRootSignature();
	BuildDrawMapSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();

	BuildPSO();

	BuildRenderItem();
	BuildFrameResource();

	BuildGUIFrame();

	mSsao->SetPSOs(
		mPSOs[ObjectData::RenderType::_SSAO_COMPUTE_TYPE].Get(), 
		mPSOs[ObjectData::RenderType::_SSAO_BLUR_COMPUTE_TYPE].Get()
	);

	// Create Physx Synchronization Thread
	hAnimThread = CreateThread(NULL, 0, ThreadAnimFunc, NULL, 0, &hAnimThreadID);
	hClothPhysxThread = CreateThread(NULL, 0, ThreadClothPhysxFunc, NULL, 0, &hClothPhysxThreadID);

	mClothReadEvent = CreateEvent(nullptr, false, false, nullptr);
	mClothWriteEvent = CreateEvent(nullptr, false, false, nullptr);
	mAnimationReadEvent = CreateEvent(nullptr, false, false, nullptr);
	mAnimationWriteEvent = CreateEvent(nullptr, false, false, nullptr);

	for (UINT i = 0; i < numGlobalThread; i++) {
		shadowRecordingDoneEvents[i] = CreateEvent(nullptr, false, false, nullptr);
		shadowRenderTargetEvent[i] = CreateEvent(nullptr, false, false, nullptr);

		recordingDoneEvents[i] = CreateEvent(nullptr, false, false, nullptr);
		renderTargetEvent[i] = CreateEvent(nullptr, false, false, nullptr);

		shadowDrawThreads[i] = CreateThread(
			nullptr,
			0,
			this->DrawShadowThread,
			reinterpret_cast<LPVOID>(i),
			0,
			shadowThreadIndex[i]
		);

		drawThreads[i] = CreateThread(
			nullptr,
			0,
			this->DrawThread,
			reinterpret_cast<LPVOID>(i),
			0,
			ThreadIndex[i]
		);
	}

	for (UINT i = 0; i < mBrushThreadNum; i++) {
		mBrushEvent[i] = CreateEvent(nullptr, false, false, nullptr);
		hTextureBrushThread = CreateThread(NULL, 0, Brushing, (LPVOID)i, 0, &hTextureBrushThreadID);
	}

	return true;
}

bool BoxApp::CloseCommandList()
{
	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();
}

void BoxApp::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 4;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())
	));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 3;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())
	));
}

void BoxApp::OnResize()
{
	D3DApp::OnResize();

	mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	BoundingFrustum::CreateFromMatrix(mCamFrustum, mCamera.GetProj());

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);

	if (mBlurFilter != nullptr)
	{
		mBlurFilter->OnResize(mClientWidth, mClientHeight);
	}
	if (mSobelFilter != nullptr)
	{
		mSobelFilter->OnResize(mClientWidth, mClientHeight);
	}
	if (mOffscreenRT != nullptr)
	{
		mOffscreenRT->OnResize(mClientWidth, mClientHeight);
	}
	if (mSsao != nullptr)
	{
		mSsao->OnResize(mClientWidth, mClientHeight);

		mSsao->RebuildDescriptors(mDepthStencilBuffer.Get());
	}
}

void BoxApp::Update(const GameTimer& gt)
{
	if (mInstanceIsUpdated > 0)
	{
		mInstanceIsUpdated -= 1;

		UpdateInstanceBuffer();
		UpdateInstanceDataWithBaked(gt);
	}

	DrawGUI(gt);

	WaitForSingleObject(mClothReadEvent, INFINITE);
	WaitForSingleObject(mAnimationReadEvent, INFINITE);

	if (isBegining > 10)
		_Update(gt);

	UpdateMaterialBuffer(gt);

	OnKeyboardInput(gt);
	AnimationMaterials(gt);
	UpdateMainPassCB(gt);
	UpdateAnimation(gt);

	UpdateInstanceData(gt);

	UpdateQuestUI(gt);

	//UpdateSsaoCB(gt);
}

void BoxApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime() * 50.0f;

	std::unordered_map<char, char>::iterator mInputVectorIterator = mInputVector.begin();

	for (int i = 0; i < mInputVector.size(); i++)
	{
		if (GetAsyncKeyState((*mInputVectorIterator).first) & 0x8000)
			(*mInputVectorIterator).second = 1;

		mInputVectorIterator++;
	}

	if (GetAsyncKeyState('Z') & 0x8000)
		mFilterCount -= 1;
	else if (GetAsyncKeyState('X') & 0x8000)
		mFilterCount += 1;

	// Convert Spherical to Cartesian coordinates.
	float x = sinf(mPhi)*cosf(mTheta);
	float z = mRadius * sinf(mPhi)*sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	mCamera.UpdateViewMatrix();
}

void BoxApp::AnimationMaterials(const GameTimer& gt)
{
	std::unordered_map<std::string, ObjectData*>::iterator iter = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator end = mGameObjectDatas.end();
	ObjectData* obj = nullptr;

	while (iter != end)
	{
		obj = (*iter).second;

		if (obj->InstanceCount == 0)
		{
			iter++;
			continue;
		}

		InstanceIterator = obj->mInstances.begin();

		(*InstanceIterator).TexTransform._41 += 0.0f;
		(*InstanceIterator).TexTransform._42 += gt.DeltaTime();
		(*InstanceIterator).TexTransform._43 += 0.0f;

		iter++;
	}
}

XMMATRIX view;
XMMATRIX invView;

XMMATRIX world, texTransform, invWorld, viewToLocal;
InstanceData data;

//const PhysResource* pr = nullptr;
UINT visibleInstanceCount = 0;

// Game Index
int mGameIDX;

int MatIDX;
int LightIDX;

// UpdateMaterialBuffer
// Material Count == Submesh Count
MaterialData matData;
XMMATRIX matTransform;

ObjectData* obj = NULL;
PhysResource* pr = nullptr;
std::vector<ThreadDrawRenderItem>::iterator objIDX;
std::vector<ThreadDrawRenderItem>::iterator objEnd;
std::vector<LightDataConstants>::iterator lightDataIDX;
std::vector<Light>::iterator lightIDX;
std::vector<Light>::iterator lightEnd;

// 오브젝트가 프러스텀에 걸쳐있는지 여부를 확인하는 콜라이더 크기
XMFLOAT4X4 boundScale = MathHelper::Identity4x4();

Light* l;
float mObjectPos[3];
float distance = 0.0f;

DirectX::XMFLOAT3 lightVec;
DirectX::XMVECTOR lightData;

XMVECTOR lightDir;
XMVECTOR lightPos;
XMVECTOR targetPos;
XMVECTOR lightUp;
// Light View Matrix
XMMATRIX lightView;

// World View에서 Light View 방향으로 변형된 경계구의 위치
XMFLOAT3 sphereCenterL;

float rad;

XMMATRIX lightProj;
// NDC 매트릭스
XMMATRIX T(
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, -0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.0f, 1.0f
);

XMMATRIX S;

XMMATRIX mLightViewProj;
XMMATRIX mLightInvProj;
XMMATRIX mLightInvView;
XMMATRIX mLightInvViewProj;

UINT w;
UINT h;

UINT mThreadNum = 0;

void BoxApp::UpdateMaterialBuffer(const GameTimer& gt)
{
	ObjectData* obj = NULL;

	std::unordered_map<std::string, ObjectData*>::iterator iter = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator end = mGameObjectDatas.end();

	UINT count = 0;
	while (iter != end)
	{
		obj = (*iter).second;

		if (obj->isDiffuseDirty)
		{
			for (int i = 0; i < (*iter).second->Mat.size(); i++)
			{
				mMaterials[obj->Mat[i].MatCBIndex].DiffuseAlbedo = obj->Mat[i].DiffuseAlbedo;
			}
		}

		count++;
		iter++;
	}

	std::vector<Material>::iterator& matIDX = mMaterials.begin();
	std::vector<Material>::iterator& matEnd = mMaterials.end();
	while (matIDX != matEnd)
	{
		matTransform = XMLoadFloat4x4(&matIDX->MatTransform);

		// Initialize new MaterialDatas
		matData.DiffuseAlbedo = matIDX->DiffuseAlbedo;
		matData.FresnelR0 = matIDX->FresnelR0;
		matData.Roughness = matIDX->Roughness;
		matData.DiffuseCount = matIDX->DiffuseCount;

		XMStoreFloat4x4(
			&matData.MatTransform,
			XMMatrixTranspose(matTransform)
		);

		matIDX->MatInstIndex = MatIDX;

		// copy on Descriptor Set
		MaterialBuffer->CopyData(MatIDX++, matData);

		matIDX++;
	}
}

void BoxApp::UpdateInstanceData(const GameTimer& gt)
{
	view = mCamera.GetView();
	invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	UINT i, j;
	// Game Index
	mGameIDX = 0;

	MatIDX = 0;
	LightIDX = 0;

	lightEnd = mLights.end();

	// 오브젝트가 프러스텀에 걸쳐있는지 여부를 확인하는 콜라이더 크기
	boundScale = MathHelper::Identity4x4();
	distance = 0.0f;
	mThreadNum = 0;

	DirectX::XMVECTOR mParentPosition;
	DirectX::XMMATRIX mParentWorld;

	for (mThreadNum = 0; mThreadNum < numGlobalThread; mThreadNum++)
	{
		objIDX = mThreadDrawRenderItems[mThreadNum].begin();
		objEnd = mThreadDrawRenderItems[mThreadNum].end();

		for (; objIDX != objEnd; objIDX++)
		{
			obj = (*objIDX).mObject;


			if (obj->isBaked || (obj->mVertices.size() == 0 && obj->mBillBoardVertices.size() == 0))
			{
				mGameIDX++;
				continue;
			}

			if (obj->mParentName == "")
			{
				mParentPosition = { 0.0f, 0.0f, 0.0f, 1.0f };
				mParentWorld = {
					1, 0, 0, 0,
					0, 1, 0, 0,
					0, 0, 1, 0,
					0, 0, 0, 1
				};
			}
			else
			{
				mParentPosition = mGameObjectDatas[obj->mParentName]->mTranslate[0].position;
				mParentPosition.m128_f32[0] *= 10.0f;
				mParentPosition.m128_f32[1] *= 10.0f;
				mParentPosition.m128_f32[2] *= 10.0f;

				mParentWorld = mGameObjectDatas[obj->mParentName]->mTranslate[0].Matrix();
			}

			InstanceIterator = obj->mInstances.begin();
			BoundsIterator = obj->Bounds.begin();

			if (obj->isTextureSheetAnimation)
			{
				data.TopLeftTexCroodPos = (*InstanceIterator).TopLeftTexCroodPos;
				data.BottomRightTexCroodPos = (*InstanceIterator).BottomRightTexCroodPos;
			}

			if (obj->isBillBoard)
			{
				DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&(*InstanceIterator).World);
				world = mParentWorld * world;

				DirectX::XMStoreFloat4x4(&data.World, world);
				data.TexTransform = (*InstanceIterator).TexTransform;

				InstanceBuffer->CopyData((*objIDX).mInstanceIDX, data);

				mGameIDX++;
				continue;
			}

			bool isUpdate = false;
			for (i = 0; i < obj->InstanceCount; ++i)
			{
				if (obj->mParentName != "")
					obj->mTranslate[i].position = mParentPosition;

				obj->mTranslate[i].Update(gt.DeltaTime());

				world = obj->mTranslate[i].Matrix();

				XMStoreFloat4x4(&(*InstanceIterator).World, world);

				mObjectPos[0] = world.r[3].m128_f32[0];
				mObjectPos[1] = world.r[3].m128_f32[1];
				mObjectPos[2] = world.r[3].m128_f32[2];

				(*BoundsIterator).Center.x = world.r[3].m128_f32[0];
				(*BoundsIterator).Center.y = world.r[3].m128_f32[1];
				(*BoundsIterator).Center.z = world.r[3].m128_f32[2];

				texTransform = XMLoadFloat4x4(&(*InstanceIterator).TexTransform);
				
				XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
				data.MaterialIndex = (*InstanceIterator).MaterialIndex;

				// (현재 보여지고 있는 오브젝트의 개수) == (인스턴스 인덱스)
				InstanceBuffer->CopyData((*objIDX).mInstanceIDX + i, data);

				// Light Update
				if (mLightDatas.size() > 0)
				{
					lightIDX = mLights.begin();
					lightDataIDX = mLightDatas.begin();

					distance = 0.0f;

					// 충돌 라이트를 최신화 하기 위해 clear한다.
					int LightSize = 0;
					while (lightIDX != lightEnd)
					{
						// 만일 라이트가 Dir이면
						if ((*lightIDX).mLightType == LightType::DIR_LIGHTS)
						{
							// 무조건 포함
							(*lightDataIDX).data[LightSize] = *lightIDX;
						}
						// 만일 라이트가 Point이면
						else if ((*lightIDX).mLightType == LightType::POINT_LIGHT)
						{
							// ||(lightPos - Pos)|| < light.FalloffEnd 를 충족하면 포함
							lightVec.x = mObjectPos[0] - (*lightIDX).mPosition.x;
							lightVec.y = mObjectPos[1] - (*lightIDX).mPosition.y;
							lightVec.z = mObjectPos[2] - (*lightIDX).mPosition.z;

							lightData = DirectX::XMLoadFloat3(&lightVec);

							XMStoreFloat(&distance, DirectX::XMVector3Length(lightData));

							if (distance < (*lightIDX).mFalloffEnd)
							{
								(*lightDataIDX).data[LightSize] = *lightIDX;
							}
						}
						// 만일 라이트가 Spot이면
						else if ((*lightIDX).mLightType == LightType::SPOT_LIGHT)
						{
							// ||(lightPos - Pos)|| < light.FalloffEnd 를 충족하면 포함
							lightVec.x = mObjectPos[0] - (*lightIDX).mPosition.x;
							lightVec.y = mObjectPos[1] - (*lightIDX).mPosition.y;
							lightVec.z = mObjectPos[2] - (*lightIDX).mPosition.z;

							lightData = DirectX::XMLoadFloat3(&lightVec);

							XMStoreFloat(&distance, DirectX::XMVector3Length(lightData));

							if (distance < (*lightIDX).mFalloffEnd)
							{
								(*lightDataIDX).data[LightSize] = *lightIDX;
							}
						}

						//lightDir = XMLoadFloat3(&(*lightIDX).mDirection);
						lightDir.m128_f32[0] = 0.57735f;
						lightDir.m128_f32[1] = -0.57735f;
						lightDir.m128_f32[2] = 0.57735f;
						lightPos = -2.0f * lightDir;
						targetPos = XMLoadFloat3(&mSceneBounds.Center);
						lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
						// Light View Matrix
						lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);
						// Store Light Pos
						XMStoreFloat4(&(*lightIDX).mLightPosW, lightPos);

						// World View에서 Light View 방향으로 변형된 경계구의 위치
						XMStoreFloat3(&sphereCenterL, XMVector3TransformCoord(targetPos, lightView));

						rad = mSceneBounds.Radius;

						// Ortho 프러스텀 생성
						(*lightIDX).mLightNearZ = sphereCenterL.z - rad;
						(*lightIDX).mLightFarZ = sphereCenterL.z + rad;

						lightProj =
							XMMatrixOrthographicOffCenterLH(
								sphereCenterL.x - rad,
								sphereCenterL.x + rad,
								sphereCenterL.y - rad,
								sphereCenterL.y + rad,
								sphereCenterL.z - rad,
								sphereCenterL.z + rad
							);

						S = lightView * lightProj * T;
						XMStoreFloat4x4(&(*lightIDX).mLightView, lightView);
						XMStoreFloat4x4(&(*lightIDX).mLightProj, lightProj);

						mLightViewProj = XMMatrixMultiply(lightView, lightProj);
						mLightInvView = XMMatrixInverse(&XMMatrixDeterminant(lightView), lightView);
						mLightInvProj = XMMatrixInverse(&XMMatrixDeterminant(lightProj), lightProj);
						mLightInvViewProj = XMMatrixInverse(&XMMatrixDeterminant(mLightViewProj), mLightViewProj);

						XMStoreFloat4x4(&(*lightIDX).ShadowViewProj, XMMatrixTranspose(mLightViewProj));
						XMStoreFloat4x4(&(*lightIDX).ShadowViewProjNDC, XMMatrixTranspose(S));
						XMStoreFloat4x4(&mMainPassCB.ShadowViewProj, XMMatrixTranspose(mLightViewProj));
						XMStoreFloat4x4(&mMainPassCB.ShadowViewProjNDC, XMMatrixTranspose(S));

						(*lightIDX).AmbientLight = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

						w = mShadowMap->Width();
						h = mShadowMap->Height();

						LightSize += 1;
						lightIDX++;
					}// while (lightIDX != lightEnd)

					LightBufferCB->CopyData((*objIDX).mInstanceIDX + i, (*lightDataIDX));

					lightDataIDX++;
				}// if (mLightDatas.size() > 0)
				InstanceIterator++;
				BoundsIterator++;
			}

			// 다음 게임 오브젝트의 인덱싱
			mGameIDX++;
		}
	}
}

void BoxApp::UpdateInstanceDataWithBaked(const GameTimer& gt)
{
	view = mCamera.GetView();
	invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	//const PhysResource* pr = nullptr;
	visibleInstanceCount = 0;

	UINT i, j;
	// Game Index
	mGameIDX = 0;

	MatIDX = 0;
	LightIDX = 0;


	//////
	// 이후 각 라이트마다의 경계구로 변형 요망
	//////
	mSceneBounds.Center = XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	mSceneBounds.Radius = 100;

	std::vector<Material>::iterator& matIDX = mMaterials.begin();
	std::vector<Material>::iterator& matEnd = mMaterials.end();
	while (matIDX != matEnd)
	{
		matTransform = XMLoadFloat4x4(&matIDX->MatTransform);

		// Initialize new MaterialDatas
		matData.DiffuseAlbedo = matIDX->DiffuseAlbedo;
		matData.FresnelR0 = matIDX->FresnelR0;
		matData.Roughness = matIDX->Roughness;
		matData.DiffuseCount = matIDX->DiffuseCount;

		XMStoreFloat4x4(
			&matData.MatTransform,
			XMMatrixTranspose(matTransform)
		);

		matIDX->MatInstIndex = MatIDX;

		// copy on Descriptor Set
		MaterialBuffer->CopyData(MatIDX++, matData);

		matIDX++;
	}

	lightEnd = mLights.end();

	// 오브젝트가 프러스텀에 걸쳐있는지 여부를 확인하는 콜라이더 크기
	boundScale = MathHelper::Identity4x4();

	distance		= 0.0f;

	mThreadDrawRenderItem.resize(0);
	mRenderTasks.resize(mGameObjectDatas.size());

	std::unordered_map<std::string, ObjectData*>::iterator objIDX = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator objEnd = mGameObjectDatas.end();

	for (; objIDX != objEnd; objIDX++)
	{
		obj = (*objIDX).second;
		mRenderTasks[mGameIDX] = obj;

		InstanceIterator = obj->mInstances.begin();
		BoundsIterator = obj->Bounds.begin();

		if (obj->isTextureSheetAnimation)
		{
			data.TopLeftTexCroodPos = (*InstanceIterator).TopLeftTexCroodPos;
			data.BottomRightTexCroodPos = (*InstanceIterator).BottomRightTexCroodPos;
		}

		if (obj->isBillBoard)
		{
			data.World = (*InstanceIterator).World;
			data.TexTransform = (*InstanceIterator).TexTransform;

			InstanceBuffer->CopyData(visibleInstanceCount, data);

			mThreadDrawRenderItem.push_back(
				ThreadDrawRenderItem(
					obj,
					visibleInstanceCount,
					0,
					obj->InstanceCount
				)
			);

			mGameIDX++;
			visibleInstanceCount++;
			continue;
		}

		int isOnTheFrustumCount = 0;
		UINT mInstanceIDX = 0;

		for (i = 0; i < obj->InstanceCount; ++i)
		{
			world = XMLoadFloat4x4(&(*InstanceIterator).World);

			mObjectPos[0] = world.r[3].m128_f32[0];
			mObjectPos[1] = world.r[3].m128_f32[1];
			mObjectPos[2] = world.r[3].m128_f32[2];

			(*BoundsIterator).Center.x = world.r[3].m128_f32[0];
			(*BoundsIterator).Center.y = world.r[3].m128_f32[1];
			(*BoundsIterator).Center.z = world.r[3].m128_f32[2];

			texTransform = XMLoadFloat4x4(&(*InstanceIterator).TexTransform);

			invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);
			if (!DirectX::XMMatrixIsNaN(invWorld))
			{
				viewToLocal = XMMatrixMultiply(invView, invWorld);
			}
			else
			{
				viewToLocal = invView;
			}

			BoundingFrustum localSpaceFrustum;

			// 카메라의 시야에 ((worldMat)^-1 * (viewMat)^-1)를 곱하여, 오브젝트 관점에서의 프러스텀으로 변형
			// 후, 최종 결과의 프러스텀 내 오브젝트가 존재한다면 실행
			mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

			if (i == 0)
			{
				// Store Offset Index of Instance Buffer
				mInstanceIDX = visibleInstanceCount;
			}

			isOnTheFrustumCount++;

			XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
			data.MaterialIndex = (*InstanceIterator).MaterialIndex;

			// (현재 보여지고 있는 오브젝트의 개수) == (인스턴스 인덱스)
			InstanceBuffer->CopyData(visibleInstanceCount, data);

			// Light Update
			if (mLightDatas.size() > 0)
			{
				lightIDX = mLights.begin();
				lightDataIDX = mLightDatas.begin();

				distance = 0.0f;

				// 충돌 라이트를 최신화 하기 위해 clear한다.
				int LightSize = 0;
				while (lightIDX != lightEnd)
				{
					// 만일 라이트가 Dir이면
					if ((*lightIDX).mLightType == LightType::DIR_LIGHTS)
					{
						// 무조건 포함
						(*lightDataIDX).data[LightSize] = *lightIDX;
					}
					// 만일 라이트가 Point이면
					else if ((*lightIDX).mLightType == LightType::POINT_LIGHT)
					{
						// ||(lightPos - Pos)|| < light.FalloffEnd 를 충족하면 포함
						lightVec.x = mObjectPos[0] - (*lightIDX).mPosition.x;
						lightVec.y = mObjectPos[1] - (*lightIDX).mPosition.y;
						lightVec.z = mObjectPos[2] - (*lightIDX).mPosition.z;

						lightData = DirectX::XMLoadFloat3(&lightVec);

						XMStoreFloat(&distance, DirectX::XMVector3Length(lightData));

						if (distance < (*lightIDX).mFalloffEnd)
						{
							(*lightDataIDX).data[LightSize] = *lightIDX;
						}
					}
					// 만일 라이트가 Spot이면
					else if ((*lightIDX).mLightType == LightType::SPOT_LIGHT)
					{
						// ||(lightPos - Pos)|| < light.FalloffEnd 를 충족하면 포함
						lightVec.x = mObjectPos[0] - (*lightIDX).mPosition.x;
						lightVec.y = mObjectPos[1] - (*lightIDX).mPosition.y;
						lightVec.z = mObjectPos[2] - (*lightIDX).mPosition.z;

						lightData = DirectX::XMLoadFloat3(&lightVec);

						XMStoreFloat(&distance, DirectX::XMVector3Length(lightData));

						if (distance < (*lightIDX).mFalloffEnd)
						{
							(*lightDataIDX).data[LightSize] = *lightIDX;
						}
					}

					//lightDir = XMLoadFloat3(&(*lightIDX).mDirection);
					lightDir.m128_f32[0] = 0.57735f;
					lightDir.m128_f32[1] = -0.57735f;
					lightDir.m128_f32[2] = 0.57735f;
					lightPos = -2.0f * lightDir;
					targetPos = XMLoadFloat3(&mSceneBounds.Center);
					lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
					// Light View Matrix
					lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);
					// Store Light Pos
					XMStoreFloat4(&(*lightIDX).mLightPosW, lightPos);

					// World View에서 Light View 방향으로 변형된 경계구의 위치
					XMStoreFloat3(&sphereCenterL, XMVector3TransformCoord(targetPos, lightView));

					rad = mSceneBounds.Radius;

					// Ortho 프러스텀 생성
					(*lightIDX).mLightNearZ = sphereCenterL.z - rad;
					(*lightIDX).mLightFarZ = sphereCenterL.z + rad;

					lightProj =
						XMMatrixOrthographicOffCenterLH(
							sphereCenterL.x - rad,
							sphereCenterL.x + rad,
							sphereCenterL.y - rad,
							sphereCenterL.y + rad,
							sphereCenterL.z - rad,
							sphereCenterL.z + rad
						);

					S = lightView * lightProj * T;
					XMStoreFloat4x4(&(*lightIDX).mLightView, lightView);
					XMStoreFloat4x4(&(*lightIDX).mLightProj, lightProj);

					mLightViewProj = XMMatrixMultiply(lightView, lightProj);
					mLightInvView = XMMatrixInverse(&XMMatrixDeterminant(lightView), lightView);
					mLightInvProj = XMMatrixInverse(&XMMatrixDeterminant(lightProj), lightProj);
					mLightInvViewProj = XMMatrixInverse(&XMMatrixDeterminant(mLightViewProj), mLightViewProj);

					XMStoreFloat4x4(&(*lightIDX).ShadowViewProj, XMMatrixTranspose(mLightViewProj));
					XMStoreFloat4x4(&(*lightIDX).ShadowViewProjNDC, XMMatrixTranspose(S));
					XMStoreFloat4x4(&mMainPassCB.ShadowViewProj, XMMatrixTranspose(mLightViewProj));
					XMStoreFloat4x4(&mMainPassCB.ShadowViewProjNDC, XMMatrixTranspose(S));

					(*lightIDX).AmbientLight = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

					w = mShadowMap->Width();
					h = mShadowMap->Height();

					LightSize += 1;
					lightIDX++;
				}// while (lightIDX != lightEnd)

				LightBufferCB->CopyData(visibleInstanceCount, (*lightDataIDX));
			}// if (mLightDatas.size() > 0)

			// 전체 오브젝트 인스턴스 카운트 (Default OBJ + BillBoard OBJ)
			visibleInstanceCount++;
			// InstanceBuffer가 지정되어 있는 오브젝트'만' 카운트
			lightDataIDX++;
			InstanceIterator++;
			BoundsIterator++;
		}

		mThreadDrawRenderItem.push_back(
			ThreadDrawRenderItem(
				obj,
				mInstanceIDX,
				0,
				isOnTheFrustumCount
			)
		);

		// 다음 게임 오브젝트의 인덱싱
		mGameIDX++;
	}

	for (i = 0; i < numGlobalThread; i++)
		mThreadDrawRenderItems[i].resize(0);

	// mThreadDrawRenderItem를 쓰레드 개수로 나눈다.
	UINT loop = 0;
	for (i = 0; i < mThreadDrawRenderItem.size(); i++)
	{
		if (mThreadDrawRenderItem[i].mOnTheFrustumCount < numGlobalThread)
		{
			mThreadDrawRenderItems[loop].push_back(mThreadDrawRenderItem[i]);
			loop = (loop + 1) < numGlobalThread ? (loop + 1) : 0;
		}
		else
		{
			UINT mPieceOfOnTheFrustumCount = mThreadDrawRenderItem[i].mOnTheFrustumCount / numGlobalThread;

			for (j = 0; j < (numGlobalThread - 1); j++)
			{
				mThreadDrawRenderItems[j].push_back(
					ThreadDrawRenderItem(
						mThreadDrawRenderItem[i].mObject,
						mThreadDrawRenderItem[i].mInstanceIDX + (mPieceOfOnTheFrustumCount * j),
						mThreadDrawRenderItem[i].mOnlySubmeshIDX,
						mPieceOfOnTheFrustumCount
					)
				);
			}
			mThreadDrawRenderItems[numGlobalThread - 1].push_back(
				ThreadDrawRenderItem(
					mThreadDrawRenderItem[i].mObject,
					mThreadDrawRenderItem[i].mInstanceIDX + (mPieceOfOnTheFrustumCount * (numGlobalThread - 1)),
					mThreadDrawRenderItem[i].mOnlySubmeshIDX,
					mThreadDrawRenderItem[i].mOnTheFrustumCount - (mPieceOfOnTheFrustumCount * (numGlobalThread - 1))
				)
			);
		}
	}
}

void BoxApp::UpdateMainPassCB(const GameTimer& gt)
{
	{
		XMMATRIX view = mCamera.GetView();
		XMMATRIX proj = mCamera.GetProj();
		XMMATRIX viewProj = XMMatrixMultiply(view, proj);
		XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
		XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
		XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

		XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

		XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);

		XMStoreFloat4x4(&mMainPassCB.view, XMMatrixTranspose(view));
		XMStoreFloat4x4(&mMainPassCB.Invview, XMMatrixTranspose(invView));
		XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
		XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
		XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
		XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));

		mMainPassCB.EyePosW = mCamera.GetPosition3f();
		mMainPassCB.NearZ = 1.0f;
		mMainPassCB.FarZ = 1000.0f;
		mMainPassCB.TotalTime = gt.TotalTime();
		mMainPassCB.DeltaTime = gt.DeltaTime();

		PassCB->CopyData(0, mMainPassCB);
	}
}

void BoxApp::UpdateAnimation(const GameTimer& gt)
{
	ObjectData* obj = NULL;

	float delta = gt.DeltaTime();
	std::unordered_map<std::string, ObjectData*>::iterator objIDX = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator objEnd = mGameObjectDatas.end();

	XMVECTOR up;
	XMVECTOR right;
	XMVECTOR front;

	while (objIDX != objEnd)
	{
		obj = (*objIDX).second;

		obj->currentDelayPerSec += delta;

		for (int i = 0; i < obj->mBillBoardVertices.size(); i++)
		{
			up = DirectX::XMLoadFloat3(&obj->mBillBoardVertices[i].Norm);
			right = mCamera.GetLook();

			front = DirectX::XMVector3Cross(up, right);
			DirectX::XMStoreFloat3(&obj->mBillBoardVertices[i].Norm, front);
		}

		objIDX++;
	}
}

void BoxApp::UpdateSsaoCB(const GameTimer& gt)
{
	SsaoConstants ssaoCB;

	XMMATRIX P = mCamera.GetProj();

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	ssaoCB.Proj = mMainPassCB.Proj;
	ssaoCB.InvProj = mMainPassCB.InvProj;
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P*T));

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);

	std::vector<float> blurWeights = mSsao->CalcGaussWeights(2.5f);
	ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / mSsao->SsaoMapWidth(), 1.0f / mSsao->SsaoMapHeight());

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 1.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	SsaoCB->CopyData(0, ssaoCB);
}

void BoxApp::UpdateInstanceBuffer()
{
	// Calc Instance Size
	int InstanceNum = 0;
	// Calc Submesh Size
	int SubmeshNum = 0;
	// Calc Bone Size
	int BoneNum = 0;

	RenderItem* obj = NULL;
	std::list<RenderItem*>::iterator objIDX = mGameObjects.begin();
	std::list<RenderItem*>::iterator objEnd = mGameObjects.end();
	while (objIDX != objEnd) {
		obj = (*objIDX);
		RecursionFrameResource(obj, InstanceNum, SubmeshNum, BoneNum);

		objIDX++;
	}

	if (InstanceNum > 0)
	{
		LightBufferCB =
			std::make_unique<UploadBuffer<LightDataConstants>>(md3dDevice.Get(), InstanceNum, true);
		InstanceBuffer =
			std::make_unique<UploadBuffer<InstanceData>>(md3dDevice.Get(), InstanceNum, false);
	}
}

void BoxApp::UpdateQuestUI(const GameTimer& gt)
{
	//mODBC.GetTuple(0, data);
	//printf("");

	//mODBC.GetCurrentQuest(data);
	//printf("");

	//mODBC.SetComplete(data.id);

	//mODBC.GetCurrentQuest(data);
	//printf("");

	//questParamComp->pushToggleComponent(isUseGlobalPose, "isUseGlobalPoseToggle", "isUseGlobalPoseToggle");

	//questParamComp->pushSliderFloatComponent(mGlobalVec.x, "GlobalVecX", "GlobalVecX", -1.0f, 1.0f);
	//questParamComp->pushSliderFloatComponent(mGlobalVec.y, "GlobalVecY", "GlobalVecY", -1.0f, 1.0f);
	//questParamComp->pushSliderFloatComponent(mGlobalVec.z, "GlobalVecZ", "GlobalVecZ", -1.0f, 1.0f);

	//questParamComp->pushSliderFloatComponent(mLocalScale.x, "LocalScaleX", "LocalScaleX", -0.01f, 0.01f);
	//questParamComp->pushSliderFloatComponent(mLocalScale.y, "LocalScaleY", "LocalScaleY", -0.1f, 0.1f);
	//questParamComp->pushSliderFloatComponent(mLocalScale.z, "LocalScaleZ", "LocalScaleZ", -0.01f, 0.01f);
}

void BoxApp::InitSwapChain(int numThread)
{
	{
		// commandAlloc, commandList의 재사용을 위한 리셋
		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		for (UINT i = 0; i < numGlobalThread; i++)
		{
			ThrowIfFailed(mMultiCmdListAlloc[i]->Reset());
			ThrowIfFailed(mMultiShadowCmdListAlloc[i]->Reset());
		}

		ThrowIfFailed(
			mCommandList->Reset(
				mDirectCmdListAlloc.Get(),
				mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE].Get()
			)
		);
		for (UINT i = 0; i < numGlobalThread; i++)
		{
			ThrowIfFailed(mMultiCommandList[i]->Reset(
				mMultiCmdListAlloc[i].Get(),
				mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE].Get()
			));
			ThrowIfFailed(mMultiShadowCommandList[i]->Reset(
				mMultiShadowCmdListAlloc[i].Get(),
				mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE].Get()
			));
		}

		// Indicate a state transition on the resource usage.
		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mOffscreenRT->Resource(),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			)
		);

		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			)
		);

		// Clear the back buffer and depth buffer.
		mCommandList->ClearRenderTargetView(
			mOffscreenRT->Rtv(), 
			Colors::LightSteelBlue, 
			0, 
			nullptr
		);
		mCommandList->ClearDepthStencilView(
			mDsvHeap->GetCPUDescriptorHandleForHeapStart(), 
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 
			1.0f, 
			0, 
			0, 
			nullptr
		);

	}

	// 이전 프레임에서 계산한 Cloth Vertices 결과를 업데이트.
	{
		ObjectData* obj = NULL;
		std::unordered_map<std::string, ObjectData*>::iterator objIDX = mGameObjectDatas.begin();
		std::unordered_map<std::string, ObjectData*>::iterator objEnd = mGameObjectDatas.end();
		while (objIDX != objEnd)
		{
			obj = (*objIDX).second;
			objIDX++;

			if (!obj->isDirty)
				continue;

			obj->isDirty = false;

			d3dUtil::UpdateDefaultBuffer(
				mCommandList.Get(),
				obj->mVertices.data(),
				obj->mGeometry.VertexBufferByteSize,
				obj->mGeometry.VertexBufferUploader,
				obj->mGeometry.VertexBufferGPU
			);
		}

		// 옷 버텍스를 업데이트 하였다면, 다시 쓰기모드로 바꾸기
		ResetEvent(mClothReadEvent);
		SetEvent(mClothWriteEvent);
		ResetEvent(mAnimationReadEvent);
		SetEvent(mAnimationWriteEvent);
	}

	{
		for (UINT i = 0; i < numGlobalThread; i++)
		{
			SetEvent(shadowRenderTargetEvent[i]);
		}

		WaitForMultipleObjects(numGlobalThread, shadowRecordingDoneEvents, true, INFINITE);

		for (UINT i = 0; i < numGlobalThread; i++)
		{
			ResetEvent(shadowRecordingDoneEvents[i]);
		}
	}

	// Compute Draw Texture
	if (mDrawTexture->isDirty)
	{
		mDrawTexture->isDirty = false;

		for (int i = 0; i < 3; i++)
		{
			mDrawTexture->Position[i].x = mBrushPosition[i].x;
			mDrawTexture->Position[i].y = mBrushPosition[i].y;
		}

		mDrawTexture->Execute(
			mCommandList.Get(),
			mDrawMapSignature.Get(),
			mPSOs[ObjectData::RenderType::_DRAW_MAP_TYPE].Get()
		);
	}

	// 이전 프레임에서 계산한 Cloth Vertices 결과를 업데이트.
	{
		ObjectData* obj = NULL;
		std::unordered_map<std::string, ObjectData*>::iterator objIDX = mGameObjectDatas.begin();
		std::unordered_map<std::string, ObjectData*>::iterator objEnd = mGameObjectDatas.end();
		while (objIDX != objEnd)
		{
			obj = (*objIDX).second;
			objIDX++;

			if (obj->isDirty)
			{
				obj->isDirty = false;

				d3dUtil::UpdateDefaultBuffer(
					mCommandList.Get(),
					obj->mVertices.data(),
					obj->mGeometry.VertexBufferByteSize,
					obj->mGeometry.VertexBufferUploader,
					obj->mGeometry.VertexBufferGPU
				);
			}
			else if (obj->isBillBoardDirty)
			{
				obj->isBillBoardDirty = false;

				if (obj->mBillBoardVertices.size() > 0)
				{
					d3dUtil::UpdateDefaultBuffer(
						mCommandList.Get(),
						obj->mBillBoardVertices.data(),
						obj->mGeometry.VertexBufferByteSize,
						obj->mGeometry.VertexBufferUploader,
						obj->mGeometry.VertexBufferGPU
					);
				}
			}
		}

		// 옷 버텍스를 업데이트 하였다면, 다시 쓰기모드로 바꾸기
		ResetEvent(mClothReadEvent);
		SetEvent(mClothWriteEvent);
		ResetEvent(mAnimationReadEvent);
		SetEvent(mAnimationWriteEvent);
	}

	{
		mCommandList->Close();

		mMultiShadowCommandList[0]->Close();
		mMultiShadowCommandList[1]->Close();
		mMultiShadowCommandList[2]->Close();
		mMultiShadowCommandList[3]->Close();
		mMultiShadowCommandList[4]->Close();
		mMultiShadowCommandList[5]->Close();
		mMultiShadowCommandList[6]->Close();
		mMultiShadowCommandList[7]->Close();

		ID3D12CommandList* commands[] = {
			mCommandList.Get(),
			mMultiShadowCommandList[0].Get(),
			mMultiShadowCommandList[1].Get(),
			mMultiShadowCommandList[2].Get(),
			mMultiShadowCommandList[3].Get(),
			mMultiShadowCommandList[4].Get(),
			mMultiShadowCommandList[5].Get(),
			mMultiShadowCommandList[6].Get(),
			mMultiShadowCommandList[7].Get()
		};
		mCommandQueue->ExecuteCommandLists(_countof(commands), commands);

		mCurrentFence++;

		ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

		if (mFence->GetCompletedValue() <= mCurrentFence) {
			HANDLE _event = CreateEvent(
				nullptr,
				false,
				false,
				L""
			);

			mFence->SetEventOnCompletion(mCurrentFence, _event);

			WaitForSingleObject(_event, INFINITE);
			CloseHandle(_event);
		}
	}
}

DWORD WINAPI BoxApp::DrawShadowThread(LPVOID temp)
{
	UINT ThreadIDX = (UINT)(temp);

	SubmeshGeometry* sg = nullptr;

	// PreMake offset of Descriptor Resource Buffer.
	// 프레임 버퍼의 시작 주소 (Base Pointer)
	D3D12_GPU_VIRTUAL_ADDRESS instanceCB = InstanceBuffer->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS lightCB = LightBufferCB->Resource()->GetGPUVirtualAddress();

	// 프레임 버퍼 (Stack Pointer)
	D3D12_GPU_VIRTUAL_ADDRESS objCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress;

	// For each render item...
	ThreadDrawRenderItem* mRenderItem = nullptr;
	ObjectData* obj = nullptr;

	// loop Resource
	size_t i, k;

	while (ThreadIDX < numGlobalThread)
	{
		// Draw에서 Update를 마치고 Draw를 시작하라고 지시 할 때 까지 대기
		WaitForSingleObject(shadowRenderTargetEvent[ThreadIDX], INFINITE);

		instanceCB = InstanceBuffer->Resource()->GetGPUVirtualAddress();
		lightCB = LightBufferCB->Resource()->GetGPUVirtualAddress();

		// Opaque 아이템을 렌더 하여 DepthMap을 그린다.
		{
			mMultiShadowCommandList[ThreadIDX]->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mShadowMap->Resource(),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					D3D12_RESOURCE_STATE_DEPTH_WRITE
				)
			);

			mMultiShadowCommandList[ThreadIDX]->RSSetViewports(1, &mShadowMap->Viewport());
			mMultiShadowCommandList[ThreadIDX]->RSSetScissorRects(1, &mShadowMap->ScissorRect());

			UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

			if (ThreadIDX == 0)
			{
				mMultiShadowCommandList[ThreadIDX]->ClearDepthStencilView(
					mShadowMap->Dsv(),
					D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
					1.0f,
					0,
					0,
					nullptr
				);
			}

			// Set null render target because we are only going to draw to
			// depth buffer.  Setting a null render target will disable color writes.
			// Note the active PSO also must specify a render target count of 0.
			mMultiShadowCommandList[ThreadIDX]->OMSetRenderTargets(
				0,
				nullptr,
				false,
				&mShadowMap->Dsv()
			);

			ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
			mMultiShadowCommandList[ThreadIDX]->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			mMultiShadowCommandList[ThreadIDX]->SetGraphicsRootSignature(mRootSignature.Get());

			// Select Descriptor Buffer Index
			mMultiShadowCommandList[ThreadIDX]->SetGraphicsRootConstantBufferView(
				0,
				PassCB->Resource()->GetGPUVirtualAddress()
			);

			mMultiShadowCommandList[ThreadIDX]->SetPipelineState(mPSOs[ObjectData::RenderType::_OPAQUE_SHADOW_MAP_RENDER_TYPE].Get());

			for (i = 0; i < mThreadDrawRenderItems[ThreadIDX].size(); ++i)
			{
				mRenderItem = &mThreadDrawRenderItems[ThreadIDX][i];
				obj = mRenderItem->mObject;

				if ( obj->mVertices.size() == 0			|| 
					 !obj->isDrawShadow					||
					 !mRenderItem->mOnTheFrustumCount)
				{
					continue;
				}

				// Instance
				objCBAddress = 
					instanceCB + mRenderItem->mInstanceIDX * sizeof(InstanceData);
				lightCBAddress = 
					lightCB + mRenderItem->mInstanceIDX * d3dUtil::CalcConstantBufferByteSize(sizeof(LightDataConstants));

				SubmeshGeometry* sg = nullptr;

				mMultiShadowCommandList[ThreadIDX]->IASetVertexBuffers(
					0, 
					1, 
					&obj->mGeometry.VertexBufferView()
				);
				mMultiShadowCommandList[ThreadIDX]->IASetIndexBuffer(
					&obj->mGeometry.IndexBufferView()
				);
				mMultiShadowCommandList[ThreadIDX]->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				mMultiShadowCommandList[ThreadIDX]->SetGraphicsRootConstantBufferView(
					1,
					lightCBAddress
				);
				mMultiShadowCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
					3,
					objCBAddress
				);

				for (k = 0; k < obj->SubmeshCount; k++)
				{
					sg = &obj->mGeometry.DrawArgs[k];

					mMultiShadowCommandList[ThreadIDX]->DrawIndexedInstanced(
						sg->IndexSize,
						mRenderItem->mOnTheFrustumCount,
						sg->StartIndexLocation,
						sg->BaseVertexLocation,
						0
					);
				}
			}

			// Change back to GENERIC_READ so we can read the texture in a shader.
			mMultiShadowCommandList[ThreadIDX]->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mShadowMap->Resource(),
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					D3D12_RESOURCE_STATE_GENERIC_READ
				)
			);
		}

		// Done recording commands.
		//ThrowIfFailed(mMultiShadowCommandList[ThreadIDX]->Close());

		ResetEvent(shadowRenderTargetEvent[ThreadIDX]);
		SetEvent(shadowRecordingDoneEvents[ThreadIDX]);
	}

	return (DWORD)0;
}

DWORD WINAPI BoxApp::DrawThread(LPVOID temp)
{
	UINT ThreadIDX = (UINT)(temp);

	SubmeshGeometry* sg = nullptr;

	UINT i, j;

	// PreMake offset of Descriptor Resource Buffer.
	// 프레임 버퍼의 시작 주소 (Base Pointer)
	D3D12_GPU_VIRTUAL_ADDRESS instanceCB = InstanceBuffer->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS matCB = MaterialBuffer->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS lightCB = LightBufferCB->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS rateOfAnimTimeCB = RateOfAnimTimeCB->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS pmxBoneCB = PmxAnimationBuffer->Resource()->GetGPUVirtualAddress();

	// 프레임 버퍼 (Stack Pointer)
	D3D12_GPU_VIRTUAL_ADDRESS instanceCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS matCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS rateOfAnimTimeCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS pmxBoneCBAddress;

	// Extracted Render Items.
	ObjectData* obj = nullptr;

	UINT loop = 0;
	UINT end = 0;

	int MatIDX = 0;
	int gameIDX = 0;

	while (ThreadIDX < numGlobalThread)
	{
		// Draw에서 Update를 마치고 Draw를 시작하라고 지시 할 때 까지 대기
		WaitForSingleObject(renderTargetEvent[ThreadIDX], INFINITE);

		lightCB = LightBufferCB->Resource()->GetGPUVirtualAddress();
		instanceCB = InstanceBuffer->Resource()->GetGPUVirtualAddress();

#ifdef _USE_UBER_SHADER
		// Recording CommandList
		{
			mMultiCommandList[ThreadIDX]->RSSetViewports(1, &mScreenViewport);
			mMultiCommandList[ThreadIDX]->RSSetScissorRects(1, &mScissorRect);

			// Specify the buffers we are going to render to.
			mMultiCommandList[ThreadIDX]->OMSetRenderTargets(
				1, 
				&mOffscreenRT->Rtv(), 
				true, 
				&mDsvHeap->GetCPUDescriptorHandleForHeapStart()
);
		}

#else// _USE_UBER_SHADER
		// Recording CommandList
		{
			mMultiCommandList[ThreadIDX]->RSSetViewports(1, &mScreenViewport);
			mMultiCommandList[ThreadIDX]->RSSetScissorRects(1, &mScissorRect);

			// Specify the buffers we are going to render to.
			mMultiCommandList[ThreadIDX]->OMSetRenderTargets(
				1,
				&mOffscreenRT->Rtv(),
				true,
				&mDsvHeap->GetCPUDescriptorHandleForHeapStart()
			);

			// 디스크립션 할당
			ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
			mMultiCommandList[ThreadIDX]->SetDescriptorHeaps(
				_countof(descriptorHeaps), 
				descriptorHeaps
			);

			// 루트 시그니쳐 할당
			mMultiCommandList[ThreadIDX]->SetGraphicsRootSignature(mRootSignature.Get());

			// Select Descriptor Buffer Index
			{
				// PassCB
				mMultiCommandList[ThreadIDX]->SetGraphicsRootConstantBufferView(
					0,
					PassCB->Resource()->GetGPUVirtualAddress()
				);
				// ShadowMapSRV
				mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
					10,
					mShadowMap->Srv()
				);
				// SsaoMap
				mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
					11,
					mSsao->AmbientMapSrv()
				);
				// DrawTexture
				mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
					12,
					mDrawTexture->OutputSrv()
				);
			}

			mMultiCommandList[ThreadIDX]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		ThreadDrawRenderItem* mRenderItem = nullptr;

		{
			for (UINT taskIDX = 0; taskIDX < mThreadDrawRenderItems[ThreadIDX].size(); taskIDX++)
			{
				mRenderItem = &mThreadDrawRenderItems[ThreadIDX][taskIDX];
				obj	= mRenderItem->mObject;

				if (mRenderItem->mOnTheFrustumCount == 0)
					continue;

				if (!obj->isBillBoard)
				{
					size_t vertSize = obj->mVertices.size();
					if (!obj || !vertSize) continue;

					mMultiCommandList[ThreadIDX]->SetPipelineState(
						mPSOs[(ObjectData::RenderType)obj->mRenderType].Get()
					);

					mMultiCommandList[ThreadIDX]->IASetVertexBuffers(
						0,
						1,
						&obj->mGeometry.VertexBufferView()
					);
					mMultiCommandList[ThreadIDX]->IASetIndexBuffer(
						&obj->mGeometry.IndexBufferView()
					);

					// Move to Current Stack Pointer
					instanceCBAddress = 
						instanceCB + mRenderItem->mInstanceIDX * sizeof(InstanceData);
					lightCBAddress = 
						lightCB + mRenderItem->mInstanceIDX * d3dUtil::CalcConstantBufferByteSize(sizeof(LightDataConstants));
					pmxBoneCBAddress = pmxBoneCB;
					rateOfAnimTimeCBAddress = rateOfAnimTimeCB;

					// Select Descriptor Buffer Index
					{
						mMultiCommandList[ThreadIDX]->SetGraphicsRootConstantBufferView(
							1,
							lightCBAddress
						);
						if (obj->mFormat == "PMX")
						{
							mMultiCommandList[ThreadIDX]->SetGraphicsRootConstantBufferView(
								2,
								rateOfAnimTimeCBAddress
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
								5,
								pmxBoneCBAddress
							);
						}
						mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
							3,
							instanceCBAddress
						);
					}

					// count of submesh
					if (obj->Mat.size() > 0)
					{
						for (j = 0; j < obj->SubmeshCount; j++)
						{
							// Select Texture to Index
							CD3DX12_GPU_DESCRIPTOR_HANDLE tex(
								mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
							);
							CD3DX12_GPU_DESCRIPTOR_HANDLE maskTex(
								mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
							);
							CD3DX12_GPU_DESCRIPTOR_HANDLE noiseTex(
								mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
							);
							CD3DX12_GPU_DESCRIPTOR_HANDLE skyTex(
								mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
							);

							matCBAddress = matCB + obj->Mat[j].MatCBIndex * sizeof(MaterialData);

							// Move to Current Stack Pointer
							tex.Offset(
								obj->Mat[j].DiffuseSrvHeapIndex,
								mCbvSrvUavDescriptorSize
							);

							maskTex.Offset(
								obj->Mat[j].MaskSrvHeapIndex,
								mCbvSrvUavDescriptorSize
							);

							noiseTex.Offset(
								obj->Mat[j].NoiseSrvHeapIndex,
								mCbvSrvUavDescriptorSize
							);

							// Select Descriptor Buffer Index
							mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
								4,
								matCBAddress
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								6,
								tex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								7,
								maskTex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								8,
								noiseTex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								9,
								skyTex
							);

							sg = &obj->mGeometry.DrawArgs[j];

							mMultiCommandList[ThreadIDX]->DrawIndexedInstanced(
								sg->IndexSize,
								mRenderItem->mOnTheFrustumCount,
								sg->StartIndexLocation,
								sg->BaseVertexLocation,
								0
							);
						}
					}// if (obj->Mat.size() > 0)
					else
					{
						// Select Texture to Index
						CD3DX12_GPU_DESCRIPTOR_HANDLE tex(
							mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
						);
						CD3DX12_GPU_DESCRIPTOR_HANDLE maskTex(
							mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
						);
						CD3DX12_GPU_DESCRIPTOR_HANDLE noiseTex(
							mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
						);
						CD3DX12_GPU_DESCRIPTOR_HANDLE skyTex(
							mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
						);

						for (j = 0; j < obj->SubmeshCount; j++)
						{
							matCBAddress = matCB;

							// Select Descriptor Buffer Index
							mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
								4,
								matCBAddress
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								6,
								tex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								7,
								maskTex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								8,
								noiseTex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								9,
								skyTex
							);

							sg = &obj->mGeometry.DrawArgs[j];

							mMultiCommandList[ThreadIDX]->DrawIndexedInstanced(
								sg->IndexSize,
								mRenderItem->mOnTheFrustumCount,
								sg->StartIndexLocation,
								sg->BaseVertexLocation,
								0
							);
						}
					}
				} // if (!obj->isBillBoard)
				else
				{
					size_t vertSize = obj->mBillBoardVertices.size();
					if (!obj || !vertSize) continue;

					mMultiCommandList[ThreadIDX]->SetPipelineState(
						mPSOs[(ObjectData::RenderType)obj->mRenderType].Get()
					);

					mMultiCommandList[ThreadIDX]->IASetVertexBuffers(
						0,
						1,
						&obj->mGeometry.VertexBufferView()
					);
					mMultiCommandList[ThreadIDX]->IASetIndexBuffer(
						&obj->mGeometry.IndexBufferView()
					);


					// Move to Current Stack Pointer
					instanceCBAddress = 
						instanceCB + mRenderItem->mInstanceIDX * sizeof(InstanceData);
					mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
						3,
						instanceCBAddress
					);

					// count of submesh
					for (j = 0; j < obj->SubmeshCount; j++)
					{
						// Select Texture to Index
						CD3DX12_GPU_DESCRIPTOR_HANDLE tex(
							mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
						);
						CD3DX12_GPU_DESCRIPTOR_HANDLE maskTex(
							mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
						);
						CD3DX12_GPU_DESCRIPTOR_HANDLE noiseTex(
							mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
						);

						if (obj->Mat.size() > 0)
						{
							matCBAddress = matCB + obj->Mat[j].MatCBIndex * sizeof(MaterialData);

							// Move to Current Stack Pointer
							tex.Offset(
								obj->Mat[j].DiffuseSrvHeapIndex,
								mCbvSrvUavDescriptorSize
							);

							maskTex.Offset(
								obj->Mat[j].MaskSrvHeapIndex/* >= 0 ? obj->Mat[j].MaskSrvHeapIndex : 0*/,
								mCbvSrvUavDescriptorSize
							);

							noiseTex.Offset(
								obj->Mat[j].NoiseSrvHeapIndex/* >= 0 ? obj->Mat[j].NoiseSrvHeapIndex : 0*/,
								mCbvSrvUavDescriptorSize
							);
						}
						else
						{
							matCBAddress = matCB;

							// Move to Current Stack Pointer
							tex.Offset(
								0,
								mCbvSrvUavDescriptorSize
							);

							maskTex.Offset(
								0,
								mCbvSrvUavDescriptorSize
							);

							noiseTex.Offset(
								0,
								mCbvSrvUavDescriptorSize
							);
						}

						// Select Descriptor Buffer Index
						{
							mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
								4,
								matCBAddress
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								6,
								tex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								7,
								maskTex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								8,
								noiseTex
							);
						}

						sg = &obj->mGeometry.DrawArgs[j];

						mMultiCommandList[ThreadIDX]->DrawIndexedInstanced(
							sg->IndexSize,
							mRenderItem->mOnTheFrustumCount,
							sg->StartIndexLocation,
							sg->BaseVertexLocation,
							0
						);
					}
				} // (obj->isBillBoard)
			}
		}

		printf("");

		//// DebugBox를 그리는 구간.
		//{
		//	continueIDX = -1;
		//	MatIDX = 0;

		//	for (taskIDX = 0; taskIDX < (_taskEnd - _taskOffset); taskIDX++)
		//	{
		//		obj = mGameObjectIndices[taskIDX];

		//		if (!obj || !obj->isDebugBox)	continue;

		//		mMultiCommandList[ThreadIDX]->SetPipelineState(
		//			mPSOs[ObjectData::RenderType::_DEBUG_BOX_TYPE].Get()
		//		);

		//		mMultiCommandList[ThreadIDX]->IASetVertexBuffers(
		//			0,
		//			1,
		//			&obj->mDebugBoxData->mGeometry.VertexBufferView()
		//		);
		//		mMultiCommandList[ThreadIDX]->IASetIndexBuffer(
		//			&obj->mDebugBoxData->mGeometry.IndexBufferView()
		//		);

		//		// Move to Current Stack Pointer
		//		instanceCBAddress = instanceCB + obj->InstanceIDX * sizeof(InstanceData);
		//		mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
		//			3,
		//			instanceCBAddress
		//		);

		//		sg = &obj->mDebugBoxData->mGeometry.DrawArgs[0];

		//		mMultiCommandList[ThreadIDX]->DrawIndexedInstanced(
		//			sg->IndexSize,
		//			obj->mDebugBoxData->InstanceCount,
		//			sg->StartIndexLocation,
		//			sg->BaseVertexLocation,
		//			0
		//		);
		//	}
		//}

#endif //!_USE_UBER_SHADER

		// Done recording commands.
		ThrowIfFailed(mMultiCommandList[ThreadIDX]->Close());

		ResetEvent(renderTargetEvent[ThreadIDX]);
		SetEvent(recordingDoneEvents[ThreadIDX]);
	}

	return (DWORD)0;
}

void BoxApp::Draw(const GameTimer& gt)
{
	isBegining++;

	InitSwapChain(numGlobalThread);

	// Render Item on Multi Renderer.
	{
		for (UINT i = 0; i < numGlobalThread; i++)
		{
			SetEvent(renderTargetEvent[i]);
		}

		WaitForMultipleObjects(numGlobalThread, recordingDoneEvents, true, INFINITE);

		for (UINT i = 0; i < numGlobalThread; i++)
		{
			ResetEvent(recordingDoneEvents[i]);
		}
	}

	{
		// Add the command list to the queue for execution.
		ID3D12CommandList* cmdsLists[] = { 
			mMultiCommandList[0].Get(), 
			mMultiCommandList[1].Get(), 
			mMultiCommandList[2].Get(),
			mMultiCommandList[3].Get(),
			mMultiCommandList[4].Get(),
			mMultiCommandList[5].Get(),
			mMultiCommandList[6].Get(),
			mMultiCommandList[7].Get()
		};
		mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		mCurrentFence++;

		mCommandQueue->Signal(mFence.Get(), mCurrentFence);

		if (mFence->GetCompletedValue() <= mCurrentFence) {
			HANDLE _event = CreateEvent(
				nullptr, 
				false, 
				false, 
				nullptr
			);

			mFence->SetEventOnCompletion(mCurrentFence, _event);

			WaitForSingleObject(_event, INFINITE);
			CloseHandle(_event);
		}
	}

	// Post Processing
	{
		//
		// _Non_Skinned_Opaque SobelFilter
		//

		// commandAlloc, commandList
		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		//ThrowIfFailed(mSSAOCmdListAlloc->Reset());

		ThrowIfFailed(mCommandList->Reset(
			mDirectCmdListAlloc.Get(), 
			mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE].Get())
		);
		//ThrowIfFailed(mSSAOCommandList->Reset(
		//	mSSAOCmdListAlloc.Get(),
		//	mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE].Get())
		//);

		ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		//mCommandList->SetPipelineState(mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE].Get());

		mCommandList->RSSetViewports(1, &mScreenViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		// Specify the buffers we are going to render to.
		mCommandList->OMSetRenderTargets(
			1, 
			&mOffscreenRT->Rtv(), 
			true, 
			&DepthStencilView()
		);

		// �ñ״��� (��ũ���� ��) ���ε�
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		mCommandList->SetGraphicsRootConstantBufferView(
			0, 
			PassCB->Resource()->GetGPUVirtualAddress()
		);


		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mOffscreenRT->Resource(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_GENERIC_READ
			)
		);

		//
		mSobelFilter->Execute(
			mCommandList.Get(),
			mSobelRootSignature.Get(),
			mPSOs[ObjectData::RenderType::_SOBEL_COMPUTE_TYPE].Get(),
			mOffscreenRT->Srv()
		);

		mCommandList->OMSetRenderTargets(
			1,
			&CurrentBackBufferView(),
			true,
			&DepthStencilView()
		);


		mCommandList->SetGraphicsRootSignature(mSobelRootSignature.Get());
		mCommandList->SetPipelineState(mPSOs[ObjectData::RenderType::_COMPOSITE_COMPUTE_TYPE].Get());

		// Post Processing 전 이미지
		mCommandList->SetGraphicsRootDescriptorTable(
			0, 
			mOffscreenRT->Srv()
		);
		// Sobel Contour line 결과 이미지
		mCommandList->SetGraphicsRootDescriptorTable(
			1, 
			mSobelFilter->OutputSrv()
		);

		mCommandList->IASetVertexBuffers(0, 1, nullptr);
		mCommandList->IASetIndexBuffer(nullptr);
		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		mCommandList->DrawInstanced(
			6,
			1,
			0,
			0
		);

		//
		// BlurFilter
		//

		mBlurFilter->Execute(
			mCommandList.Get(),
			mBlurRootSignature.Get(),
			mPSOs[ObjectData::RenderType::_BLUR_HORZ_COMPUTE_TYPE].Get(),
			mPSOs[ObjectData::RenderType::_BLUR_VERT_COMPUTE_TYPE].Get(),
			CurrentBackBuffer(),
			mFilterCount
		);

		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_COPY_SOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST
			)
		);

		mCommandList->CopyResource(CurrentBackBuffer(), mBlurFilter->Output());

		// Indicate a state transition on the resource usage.
		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PRESENT
			)
		);
	}

	// ImGui
	mCommandList->SetDescriptorHeaps(1, mGUIDescriptorHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

	//// SSAO
	//{
	//	ID3D12Resource* normalMap = mSsao->NormalMap();
	//	CD3DX12_CPU_DESCRIPTOR_HANDLE normalMapRtv = mSsao->NormalMapRtv();

	//	mCommandList->SetPipelineState(mPSOs[ObjectData::RenderType::_DRAW_NORMAL_COMPUTE_TYPE].Get());

	//	// Get a Normal SRV for calculating SSAO 
	//	mCommandList->RSSetViewports(1, &mScreenViewport);
	//	mCommandList->RSSetScissorRects(1, &mScissorRect);

	//	// Change to RENDER_TARGET.
	//	mCommandList->ResourceBarrier(
	//		1,
	//		&CD3DX12_RESOURCE_BARRIER::Transition(
	//			normalMap,
	//			D3D12_RESOURCE_STATE_GENERIC_READ,
	//			D3D12_RESOURCE_STATE_RENDER_TARGET
	//		)
	//	);

	//	// Clear the screen normal map and depth buffer.
	//	float clearValue[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	//	mCommandList->ClearRenderTargetView(
	//		normalMapRtv,
	//		clearValue,
	//		0,
	//		nullptr
	//	);
	//	mCommandList->ClearDepthStencilView(
	//		DepthStencilView(),
	//		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
	//		1.0f,
	//		0,
	//		0,
	//		nullptr
	//	);

	//	// Specify the buffers we are going to render to.
	//	mCommandList->OMSetRenderTargets(
	//		1,
	//		&normalMapRtv,
	//		true,
	//		&DepthStencilView()
	//	);

	//	ID3D12DescriptorHeap* descriptorHeaps1[] = { mSrvDescriptorHeap.Get() };
	//	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps1), descriptorHeaps1);
	//	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	//	// Bind the constant buffer for this pass.
	//	mCommandList->SetGraphicsRootConstantBufferView(
	//		0,
	//		PassCB->Resource()->GetGPUVirtualAddress()
	//	);
	//	mCommandList->SetGraphicsRootShaderResourceView(
	//		4,
	//		MaterialBuffer->Resource()->GetGPUVirtualAddress()
	//	);
	//	mCommandList->SetGraphicsRootShaderResourceView(
	//		5,
	//		PmxAnimationBuffer->Resource()->GetGPUVirtualAddress()
	//	);

	//	D3D12_GPU_VIRTUAL_ADDRESS objectCB = InstanceBuffer->Resource()->GetGPUVirtualAddress();

	//	std::unordered_map<std::string, ObjectData*>::iterator& iter = mGameObjectDatas.begin();
	//	std::unordered_map<std::string, ObjectData*>::iterator& end = mGameObjectDatas.end();

	//	ObjectData* obj = nullptr;
	//	while(iter != end)
	//	{
	//		obj = iter->second;

	//		if (
	//			!obj->isOnTheFrustumCount	||
	//			obj->isDrawTexture			||
	//			obj->mFormat == "PMX"		||
	//			obj->Bounds.size() == 0
	//			)
	//		{
	//			iter++;
	//			continue;
	//		}

	//		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB + obj->InstanceIDX * sizeof(InstanceData);

	//		mCommandList->SetGraphicsRootShaderResourceView(3, objCBAddress);

	//		SubmeshGeometry* sg = nullptr;
	//		size_t k;

	//		if (!obj->isOnlySubmesh)
	//		{
	//			mCommandList->IASetVertexBuffers(0, 1, &obj->mGeometry.VertexBufferView());
	//			mCommandList->IASetIndexBuffer(&obj->mGeometry.IndexBufferView());
	//			mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//			for (k = 0; k < obj->SubmeshCount; k++)
	//			{
	//				sg = &obj->mGeometry.DrawArgs[k];

	//				mCommandList->DrawIndexedInstanced(
	//					sg->IndexSize,
	//					obj->isOnTheFrustumCount,
	//					sg->StartIndexLocation,
	//					sg->BaseVertexLocation,
	//					0
	//				);
	//			}
	//		}
	//		else
	//		{
	//			for (int kIter = 0; kIter < obj->onlySubmeshIndices.size(); kIter++)
	//			{
	//				k = obj->onlySubmeshIndices[kIter];

	//				mCommandList->IASetVertexBuffers(0, 1, &obj->mGeometry.VertexBufferViewBySubmesh(k));
	//				mCommandList->IASetIndexBuffer(&obj->mGeometry.IndexBufferViewBySubmesh(k));
	//				mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//				sg = &obj->mGeometry.DrawArgs[k];

	//				mCommandList->DrawIndexedInstanced(
	//					sg->IndexSize,
	//					obj->isOnTheFrustumCount,
	//					sg->StartIndexLocation,
	//					sg->BaseVertexLocation,
	//					0
	//				);
	//			}
	//		}

	//		iter++;
	//	}

	//	// Change back to GENERIC_READ so we can read the texture in a shader.
	//	mCommandList->ResourceBarrier(
	//		1,
	//		&CD3DX12_RESOURCE_BARRIER::Transition(
	//			normalMap,
	//			D3D12_RESOURCE_STATE_RENDER_TARGET,
	//			D3D12_RESOURCE_STATE_GENERIC_READ)
	//	);

	//	// calculate SSAO
	//	mCommandList->SetGraphicsRootSignature(mSsaoRootSignature.Get());
	//	mSsao->ComputeSsao(mCommandList.Get(), &SsaoCB, 5);
	//}

	// Close and fly to GPU
	mCommandList->Close();
	//mSSAOCommandList->Close();

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { 
		mCommandList.Get()
		// mSSAOCommandList.Get()
	};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrentFence++;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

	if (mFence->GetCompletedValue() <= mCurrentFence) {
		HANDLE _event = CreateEvent(nullptr, false, false, nullptr);

		mFence->SetEventOnCompletion(mCurrentFence, _event);

		WaitForSingleObject(_event, INFINITE);
		CloseHandle(_event);
	}
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		if (GetAsyncKeyState(VK_MENU) & 0x8000)
		{
			PickBrush(x, y);
		}

		mLastMousePos.x = x;
		mLastMousePos.y = y;
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		Pick(x, y);
	}
	SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		if (GetAsyncKeyState(VK_MENU) & 0x8000)
		{
			mClickedPosX[mThreadIDX] = (float)x;
			mClickedPosY[mThreadIDX] = (float)y;

			int threadIDX = mThreadIDX;
			mDrawTexture->isDirty = true;

			SetEvent(mBrushEvent[mThreadIDX]);

			mThreadIDX = 
				(mThreadIDX + 1) < mBrushThreadNum ? 
				(mThreadIDX + 1) : 0;
		}
		else
		{
			// Make each pixel correspond to a quarter of a degree.
			float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
			float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

			// Update angles based on input to orbit camera around box.
			mTheta += dx;
			mPhi += dy;

			// Restrict the angle mPhi.
			mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);

			mCamera.Pitch(dy);
			mCamera.RotateY(dx);

			mMainCameraDeltaRotationY = mTheta;

			mLastMousePos.x = x;
			mLastMousePos.y = y;
		}
	}
}

void BoxApp::BuildRootSignature()
{
	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	CD3DX12_DESCRIPTOR_RANGE maskTexTable;
	maskTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 1);
	CD3DX12_DESCRIPTOR_RANGE noiseTexTable;
	noiseTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 2);
	CD3DX12_DESCRIPTOR_RANGE skyTexTable;
	skyTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 2, 0);
	CD3DX12_DESCRIPTOR_RANGE shadowTexTable;
	shadowTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 1);
	CD3DX12_DESCRIPTOR_RANGE ssaoTexTable;
	ssaoTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 2);
	CD3DX12_DESCRIPTOR_RANGE drawTexTable;
	drawTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 3);

	// Root parameter can be a table, root descriptor or root constants.
	std::array<CD3DX12_ROOT_PARAMETER, 13> slotRootParameter;

	// PassCB
	slotRootParameter[0].InitAsConstantBufferView(0);
	// Light SRV
	slotRootParameter[1].InitAsConstantBufferView(1);
	// Animation Index and Time Space
	slotRootParameter[2].InitAsConstantBufferView(2);
	// Instance SRV
	slotRootParameter[3].InitAsShaderResourceView(0, 0);
	// Material SRV
	slotRootParameter[4].InitAsShaderResourceView(0, 1);
	// PMX BONE CB
	slotRootParameter[5].InitAsShaderResourceView(0, 2);
	// Main Textures
	slotRootParameter[6].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	// Mask Textures
	slotRootParameter[7].InitAsDescriptorTable(1, &maskTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	// Noise Textures
	slotRootParameter[8].InitAsDescriptorTable(1, &noiseTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	// Sky Textures
	slotRootParameter[9].InitAsDescriptorTable(1, &skyTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	// Shadow Textures
	slotRootParameter[10].InitAsDescriptorTable(1, &shadowTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	// Ssao Textures
	slotRootParameter[11].InitAsDescriptorTable(1, &ssaoTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	// Draw Textures
	slotRootParameter[12].InitAsDescriptorTable(1, &drawTexTable, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		(UINT)slotRootParameter.size(),
		slotRootParameter.data(),
		(UINT)staticSamplers.size(),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::BuildBlurRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// Weight
	slotRootParameter[0].InitAsConstants(12, 0);
	// SRV for Input
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable);
	// UAV for Output
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rooSigDesc(
		3,
		slotRootParameter,
		(UINT)staticSamplers.size(),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rooSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mBlurRootSignature.GetAddressOf())
	));
}

void BoxApp::BuildSobelRootSignature()
{
	// Post Processing 전 이미지
	CD3DX12_DESCRIPTOR_RANGE srvTable0;
	srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// Sobel Contour line 결과 이미지
	CD3DX12_DESCRIPTOR_RANGE srvTable1;
	srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE uavTable0;
	uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &srvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable1);
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable0);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		3,
		slotRootParameter,
		(UINT)staticSamplers.size(),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mSobelRootSignature.GetAddressOf())));
}

void BoxApp::BuildSsaoRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstants(1, 1);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,
		0,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers =
	{
		pointClamp, linearClamp, depthMapSam, linearWrap
	};

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())));
}

void BoxApp::BuildDrawMapSignature()
{
	// Post Processing 전 이미지
	CD3DX12_DESCRIPTOR_RANGE srvTable0;
	srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable0;
	uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstants(sizeof(float) * 6, 0);
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable0);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		4,
		slotRootParameter,
		(UINT)staticSamplers.size(),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mDrawMapSignature.GetAddressOf())
		)
	);
}

void BoxApp::BuildDescriptorHeaps()
{
	const int texCount = (const int)mTextures.size();
	const int blurDescriptorCount			= 2;
	const int sobelDescriptorCount			= mSobelFilter->DescriptorCount();
	const int ssaoDescriptorCount			= 10;
	const int postProcessDescriptorCount	= 2;
	const int shdowMapDescriptorCount		= 2;
	const int drawTextureDescriptorCount	= mDrawTexture->DescriptorCount();
	

	D3D12_DESCRIPTOR_HEAP_DESC SrvUavHeapDesc;
	SrvUavHeapDesc.NumDescriptors = 
		texCount					+ 
		blurDescriptorCount			+ 
		sobelDescriptorCount		+ 
		ssaoDescriptorCount			+
		postProcessDescriptorCount	+ 
		shdowMapDescriptorCount		+ 
		drawTextureDescriptorCount  + 
		30;

	SrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	SrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	SrvUavHeapDesc.NodeMask = 0;
	ThrowIfFailed(
		md3dDevice->CreateDescriptorHeap(
			&SrvUavHeapDesc,
			IID_PPV_ARGS(&mSrvDescriptorHeap)
		)
	);

	int mDescOffset = texCount;

	// Build Descriptors 
	mBlurFilter->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		mCbvSrvUavDescriptorSize
	);

	mDescOffset += blurDescriptorCount;

	mSobelFilter->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		mCbvSrvUavDescriptorSize
	);

	mDescOffset += sobelDescriptorCount;

	mOffscreenRT->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
			SwapChainBufferCount,
			(UINT)mRtvDescriptorSize
		)
	);

	mDescOffset += postProcessDescriptorCount;

	mShadowMap->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mDsvHeap->GetCPUDescriptorHandleForHeapStart(),
			1,
			(UINT)mDsvDescriptorSize
		)
	);

	mDescOffset += shdowMapDescriptorCount;

	mSsao->BuildDescriptors(
		mDepthStencilBuffer.Get(),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
			SwapChainBufferCount + 1,
			(UINT)mRtvDescriptorSize
		),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize
	);

	mDescOffset += ssaoDescriptorCount;

	mDrawTexture->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			mDescOffset,
			(UINT)mCbvSrvUavDescriptorSize
		),
		mCbvSrvUavDescriptorSize
	);

	mDescOffset += drawTextureDescriptorCount;

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
		mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		0,
		(UINT)mRtvDescriptorSize
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	for (int texIDX = 0; texIDX < mTextures.size(); texIDX++)
	{
		if (!mTextures[texIDX].isCube)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = -1;
			srvDesc.Format = mTextures[texIDX].Resource->GetDesc().Format;
			md3dDevice->CreateShaderResourceView(mTextures[texIDX].Resource.Get(), &srvDesc, hDescriptor);
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = mTextures[texIDX].Resource->GetDesc().MipLevels;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			srvDesc.Format = mTextures[texIDX].Resource->GetDesc().Format;
			md3dDevice->CreateShaderResourceView(mTextures[texIDX].Resource.Get(), &srvDesc, hDescriptor);
		}
		// next descriptor
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}
}

void BoxApp::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	const D3D_SHADER_MACRO pmxFormatDefines[] =
	{
		"_PMX_FORMAT", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO debugDefines[] =
	{
		"DEBUG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO drawTexDefines[] =
	{
		"DRAW_TEX", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", alphaTestDefines, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["drawNormalsVS"] = d3dUtil::CompileShader(L"Shaders\\DrawNormals.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoVS"] = d3dUtil::CompileShader(L"Shaders\\Ssao.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoBlurVS"] = d3dUtil::CompileShader(L"Shaders\\SsaoBlur.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["pmxFormatVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", pmxFormatDefines, "VS", "vs_5_1");
	mShaders["mapBaseVS"] = d3dUtil::CompileShader(L"Shaders\\MapBase.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["_SKILL_TONADO_SPLASH_VS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Splash.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["_SKILL_TONADO_SPLASH_3_VS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Splash3.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["_SKILL_TONADO_MAIN_TONADO_VS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Main_Tonado.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["_SKILL_TONADO_WATER_TORUS_VS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Water_Torus.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["_SKILL_PUNCH_SPARKS_VS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Punch_Sparks.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["_SKILL_PUNCH_ENDL_DESBIS_VS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Punch_Endl_Desbis.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["_SKILL_LASER_TRAILS_VS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Laser_Trails.hlsl", nullptr, "VS", "vs_5_1");

	mShaders["_SKILL_TONADO_SPLASH_GS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Splash.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["_SKILL_TONADO_SPLASH_3_GS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Splash3.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["_SKILL_PUNCH_SPARKS_GS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Punch_Sparks.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["_SKILL_PUNCH_ENDL_DESBIS_GS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Punch_Endl_Desbis.hlsl", nullptr, "GS", "gs_5_1");

	mShaders["pix"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["drawNormalsPS"] = d3dUtil::CompileShader(L"Shaders\\DrawNormals.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["ssaoPS"] = d3dUtil::CompileShader(L"Shaders\\Ssao.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["ssaoBlurPS"] = d3dUtil::CompileShader(L"Shaders\\SsaoBlur.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["debugPS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", debugDefines, "PS", "ps_5_1");
	mShaders["drawTexPS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", drawTexDefines, "PS", "ps_5_1");
	mShaders["mapBasePS"] = d3dUtil::CompileShader(L"Shaders\\MapBase.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["_SKILL_TONADO_SPLASH_PS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Splash.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["_SKILL_TONADO_SPLASH_3_PS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Splash3.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["_SKILL_TONADO_MAIN_TONADO_PS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Main_Tonado.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["_SKILL_TONADO_WATER_TORUS_PS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Tonado_Water_Torus.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["_SKILL_PUNCH_SPARKS_PS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Punch_Sparks.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["_SKILL_PUNCH_ENDL_DESBIS_PS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Punch_Endl_Desbis.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["_SKILL_LASER_TRAILS_PS"] = d3dUtil::CompileShader(L"Shaders\\Skill_Laser_Trails.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["horzBlurCS"] = d3dUtil::CompileShader(L"Shaders\\Blur.hlsl", nullptr, "HorzBlurCS", "cs_5_0");
	mShaders["vertBlurCS"] = d3dUtil::CompileShader(L"Shaders\\Blur.hlsl", nullptr, "VertBlurCS", "cs_5_0");

	mShaders["compositeVS"] = d3dUtil::CompileShader(L"Shaders\\Composite.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["compositePS"] = d3dUtil::CompileShader(L"Shaders\\Composite.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["sobelCS"] = d3dUtil::CompileShader(L"Shaders\\Sobel.hlsl", nullptr, "SobelCS", "cs_5_1");
	mShaders["DrawTextureCS"] = d3dUtil::CompileShader(L"Shaders\\DrawTexture.hlsl", nullptr, "DrawTextureCS", "cs_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mSkinnedInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 60, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mBillBoardInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void BoxApp::RecursionFrameResource(
	RenderItem* obj, 
	int& InstanceNum,
	int& SubmeshNum,
	int& BoneNum
)
{
	ObjectData* mData = mGameObjectDatas[obj->mName];

	InstanceNum += mData->InstanceCount + 3;
	SubmeshNum += mData->SubmeshCount;

	if (obj->mFormat == "PMX")
		BoneNum = mGameObjectDatas[obj->mName]->mModel.bone_count;

	std::unordered_map<std::string, RenderItem*>::iterator& iter = obj->mChilds.begin();
	std::unordered_map<std::string, RenderItem*>::iterator& end = obj->mChilds.end();
	while (iter != end) {
		RecursionFrameResource(
			(*iter).second, 
			InstanceNum, 
			SubmeshNum, 
			BoneNum
		);

		iter++;
	}
}

void BoxApp::BuildFrameResource()
{
	// Calc Instance Size
	int InstanceNum = 0;
	// Calc Submesh Size
	int SubmeshNum = 0;
	// Calc Bone Size
	int BoneNum = 0;

	RenderItem* obj = NULL;
	std::list<RenderItem*>::iterator objIDX = mGameObjects.begin();
	std::list<RenderItem*>::iterator objEnd = mGameObjects.end();
	for (; objIDX != objEnd; objIDX++) {
		obj = (*objIDX);
		RecursionFrameResource(obj, InstanceNum, SubmeshNum, BoneNum);
	}

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(md3dDevice.Get(), 1, true);
	SsaoCB = std::make_unique<UploadBuffer<SsaoConstants>>(md3dDevice.Get(), 1, true);
	RateOfAnimTimeCB	= 
		std::make_unique<UploadBuffer<RateOfAnimTimeConstants>>(md3dDevice.Get(), 1, true);
	if (InstanceNum > 0)
		LightBufferCB =
		std::make_unique<UploadBuffer<LightDataConstants>>(md3dDevice.Get(), InstanceNum, true);
	if (InstanceNum > 0)
		InstanceBuffer		= 
			std::make_unique<UploadBuffer<InstanceData>>(md3dDevice.Get(), InstanceNum, false);
	if (mMaterials.size() > 0)
		MaterialBuffer		= 
			std::make_unique<UploadBuffer<MaterialData>>(md3dDevice.Get(), mMaterials.size(), false);
	if (BoneNum > 0)
		PmxAnimationBuffer	= 
			std::make_unique<UploadBuffer<PmxAnimationData>>(md3dDevice.Get(), BoneNum, false);
}

void BoxApp::BuildPSO()
{
	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RENDER_TARGET_BLEND_DESC cullingBlendDesc;
	cullingBlendDesc.BlendEnable = true;
	cullingBlendDesc.LogicOpEnable = false;
	cullingBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	cullingBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	cullingBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	cullingBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	cullingBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	cullingBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	cullingBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	cullingBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//
	// PSO for Opaque
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["pix"]->GetBufferPointer()),
		mShaders["pix"]->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_OPAQUE_RENDER_TYPE])));


	//
	// PSO for _POST_PROCESSING_PIPELINE
	//
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE])));

	//
	// PSO for Sky
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyDesc = psoDesc;
	skyDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	skyDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyDesc.pRootSignature = mRootSignature.Get();

	skyDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SKY_FORMAT_RENDER_TYPE])));

	//
	// PSO for SkinnedOpaque
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedDesc = psoDesc;
	skinnedDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	skinnedDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["pix"]->GetBufferPointer()),
		mShaders["pix"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_OPAQUE_SKINNED_RENDER_TYPE])));

	//
	// PSO for pmxFormatVS
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pmxFormatDesc = psoDesc;
	pmxFormatDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	pmxFormatDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["pmxFormatVS"]->GetBufferPointer()),
		mShaders["pmxFormatVS"]->GetBufferSize()
	};
	pmxFormatDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["pix"]->GetBufferPointer()),
		mShaders["pix"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&pmxFormatDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_PMX_FORMAT_RENDER_TYPE])));

	//
	// PSO for MapBase
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC mapBaseDesc = psoDesc;
	mapBaseDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

	mapBaseDesc.pRootSignature = mRootSignature.Get();

	mapBaseDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["mapBaseVS"]->GetBufferPointer()),
		mShaders["mapBaseVS"]->GetBufferSize()
	};
	mapBaseDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["mapBasePS"]->GetBufferPointer()),
		mShaders["mapBasePS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&mapBaseDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_MAP_BASE_RENDER_TYPE])));

	//
	// PSO for horizontal blur
	// 
	D3D12_COMPUTE_PIPELINE_STATE_DESC horzBlurPSO = {};
	horzBlurPSO.pRootSignature = mBlurRootSignature.Get();
	horzBlurPSO.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["horzBlurCS"]->GetBufferPointer()),
		mShaders["horzBlurCS"]->GetBufferSize()
	};
	horzBlurPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&horzBlurPSO, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_BLUR_HORZ_COMPUTE_TYPE])));

	//
	// PSO for vertical blur
	// 
	D3D12_COMPUTE_PIPELINE_STATE_DESC vertBlurPSO = {};
	vertBlurPSO.pRootSignature = mBlurRootSignature.Get();
	vertBlurPSO.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["vertBlurCS"]->GetBufferPointer()),
		mShaders["vertBlurCS"]->GetBufferSize()
	};
	vertBlurPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&vertBlurPSO, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_BLUR_VERT_COMPUTE_TYPE])));

	//
	// PSO for sobel
	// 
	D3D12_COMPUTE_PIPELINE_STATE_DESC sobelPSO = {};
	sobelPSO.pRootSignature = mSobelRootSignature.Get();
	sobelPSO.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["sobelCS"]->GetBufferPointer()),
		mShaders["sobelCS"]->GetBufferSize()
	};
	sobelPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&sobelPSO, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SOBEL_COMPUTE_TYPE])));

	//
	// PSO for Composite
	// 
	D3D12_GRAPHICS_PIPELINE_STATE_DESC compositePSO = psoDesc;
	compositePSO.pRootSignature = mSobelRootSignature.Get();

	compositePSO.DepthStencilState.DepthEnable = false;
	compositePSO.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	compositePSO.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	compositePSO.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["compositeVS"]->GetBufferPointer()),
		mShaders["compositeVS"]->GetBufferSize()
	};
	compositePSO.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["compositePS"]->GetBufferPointer()),
		mShaders["compositePS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&compositePSO, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_COMPOSITE_COMPUTE_TYPE])));

	//
	// PSO for shadow map pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowMapPsoDesc = psoDesc;
	shadowMapPsoDesc.RasterizerState.DepthBias = 100000;
	shadowMapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	shadowMapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	shadowMapPsoDesc.pRootSignature = mRootSignature.Get();
	shadowMapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	shadowMapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};

	// Shadow map pass does not have a render target.
	shadowMapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowMapPsoDesc.NumRenderTargets = 0;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&shadowMapPsoDesc,
		IID_PPV_ARGS(
			&mPSOs[ObjectData::RenderType::_OPAQUE_SHADOW_MAP_RENDER_TYPE]
		)
	));

	//
	// PSO for drawing normals.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = psoDesc;
	drawNormalsPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsVS"]->GetBufferPointer()),
		mShaders["drawNormalsVS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsPS"]->GetBufferPointer()),
		mShaders["drawNormalsPS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.RTVFormats[0] = Ssao::NormalMapFormat;
	drawNormalsPsoDesc.SampleDesc.Count = 1;
	drawNormalsPsoDesc.SampleDesc.Quality = 0;
	drawNormalsPsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_DRAW_NORMAL_COMPUTE_TYPE])));

	//
	// PSO for SSAO.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = psoDesc;
	ssaoPsoDesc.InputLayout = { nullptr, 0 };
	ssaoPsoDesc.pRootSignature = mSsaoRootSignature.Get();
	ssaoPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoVS"]->GetBufferPointer()),
		mShaders["ssaoVS"]->GetBufferSize()
	};
	ssaoPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoPS"]->GetBufferPointer()),
		mShaders["ssaoPS"]->GetBufferSize()
	};

	// SSAO effect does not need the depth buffer.
	ssaoPsoDesc.DepthStencilState.DepthEnable = false;
	ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ssaoPsoDesc.RTVFormats[0] = Ssao::AmbientMapFormat;
	ssaoPsoDesc.SampleDesc.Count = 1;
	ssaoPsoDesc.SampleDesc.Quality = 0;
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&ssaoPsoDesc, 
		IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SSAO_COMPUTE_TYPE]))
	);

	//
	// PSO for SSAO blur.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc = ssaoPsoDesc;
	ssaoBlurPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoBlurVS"]->GetBufferPointer()),
		mShaders["ssaoBlurVS"]->GetBufferSize()
	};
	ssaoBlurPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoBlurPS"]->GetBufferPointer()),
		mShaders["ssaoBlurPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&ssaoBlurPsoDesc, 
		IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SSAO_BLUR_COMPUTE_TYPE]))
	);

	//
	// PSO for DebugBox
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugBoxDesc = psoDesc;
	debugBoxDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	shadowMapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
		mShaders["debugPS"]->GetBufferSize()
	};

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugBoxDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_DEBUG_BOX_TYPE])));

	//
	// PSO for DebugBox
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawMapDesc = psoDesc;
	drawMapDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawTexPS"]->GetBufferPointer()),
		mShaders["drawTexPS"]->GetBufferSize()
	};

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawMapDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_DRAW_MAP_RENDER_TYPE])));

	//
	// PSO for Draw Map CS
	// 
	D3D12_COMPUTE_PIPELINE_STATE_DESC drawMapPSO = {};
	drawMapPSO.pRootSignature = mDrawMapSignature.Get();
	drawMapPSO.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["DrawTextureCS"]->GetBufferPointer()),
		mShaders["DrawTextureCS"]->GetBufferSize()
	};
	drawMapPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(
		md3dDevice->CreateComputePipelineState(
			&drawMapPSO,
			IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_DRAW_MAP_TYPE])
		)
	);

	//
	// PSO for TONADO_SPLASH_SHADER
	// 

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skillDesc = psoDesc;
	skillDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	skillDesc.InputLayout = { mBillBoardInputLayout.data(), (UINT)mBillBoardInputLayout.size() };
	skillDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skillDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_SPLASH_VS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_SPLASH_VS"]->GetBufferSize()
	};
	skillDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_SPLASH_GS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_SPLASH_GS"]->GetBufferSize()
	};
	skillDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_SPLASH_PS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_SPLASH_PS"]->GetBufferSize()
	};

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skillDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SKILL_TONADO_SPLASH_TYPE])));

	//
	// PSO for TONADO_SPLASH_SHADER
	// 
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skillTonadoSplashDesc = skillDesc;

	skillTonadoSplashDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_SPLASH_3_VS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_SPLASH_3_VS"]->GetBufferSize()
	};
	skillTonadoSplashDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_SPLASH_3_GS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_SPLASH_3_GS"]->GetBufferSize()
	};
	skillTonadoSplashDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_SPLASH_3_PS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_SPLASH_3_PS"]->GetBufferSize()
	};

	skillTonadoSplashDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skillTonadoSplashDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SKILL_TONADO_SPLASH_3_TYPE])));

	//
	// PSO for _POST_PROCESSING_PIPELINE
	// 
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skillPunchDesc = skillDesc;

	skillPunchDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_PUNCH_SPARKS_VS"]->GetBufferPointer()),
		mShaders["_SKILL_PUNCH_SPARKS_VS"]->GetBufferSize()
	};
	skillPunchDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_PUNCH_SPARKS_GS"]->GetBufferPointer()),
		mShaders["_SKILL_PUNCH_SPARKS_GS"]->GetBufferSize()
	};
	skillPunchDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_PUNCH_SPARKS_PS"]->GetBufferPointer()),
		mShaders["_SKILL_PUNCH_SPARKS_PS"]->GetBufferSize()
	};

	skillPunchDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skillPunchDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SKILL_PUNCH_SPARKS_TYPE])));

	//
	// PSO for _SKILL_PUNCH_ENDL_DESBIS
	// 
	skillDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_PUNCH_ENDL_DESBIS_VS"]->GetBufferPointer()),
		mShaders["_SKILL_PUNCH_ENDL_DESBIS_VS"]->GetBufferSize()
	};
	skillDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_PUNCH_ENDL_DESBIS_GS"]->GetBufferPointer()),
		mShaders["_SKILL_PUNCH_ENDL_DESBIS_GS"]->GetBufferSize()
	};
	skillDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_PUNCH_ENDL_DESBIS_PS"]->GetBufferPointer()),
		mShaders["_SKILL_PUNCH_ENDL_DESBIS_PS"]->GetBufferSize()
	};

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skillDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SKILL_PUNCH_ENDL_DESBIS_TYPE])));

	//
	// PSO for TONADO_WATER_TORUS
	//
	skillDesc = psoDesc;
	skillDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_WATER_TORUS_VS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_WATER_TORUS_VS"]->GetBufferSize()
	};
	skillDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_WATER_TORUS_PS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_WATER_TORUS_PS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skillDesc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SKILL_TONADO_WATER_TORUS_TYPE])));

	//
	// PSO for _POST_PROCESSING_PIPELINE
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skill_Main_Tonado_Desc = psoDesc;
	skill_Main_Tonado_Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skill_Main_Tonado_Desc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_MAIN_TONADO_VS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_MAIN_TONADO_VS"]->GetBufferSize()
	};
	skill_Main_Tonado_Desc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_MAIN_TONADO_PS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_MAIN_TONADO_PS"]->GetBufferSize()
	};

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skill_Main_Tonado_Desc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE])));

	//
	// PSO for _POST_PROCESSING_PIPELINE
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skill_Laser_Ray_Waves_Desc = psoDesc;
	skill_Laser_Ray_Waves_Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skill_Laser_Ray_Waves_Desc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_MAIN_TONADO_VS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_MAIN_TONADO_VS"]->GetBufferSize()
	};
	skill_Laser_Ray_Waves_Desc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_TONADO_MAIN_TONADO_PS"]->GetBufferPointer()),
		mShaders["_SKILL_TONADO_MAIN_TONADO_PS"]->GetBufferSize()
	};

	skill_Laser_Ray_Waves_Desc.BlendState.RenderTarget[0] = transparencyBlendDesc;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skill_Laser_Ray_Waves_Desc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE])));

	//
	// PSO for _POST_PROCESSING_PIPELINE
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skill_Laser_Trails_Desc = psoDesc;
	skill_Laser_Trails_Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skill_Laser_Trails_Desc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_LASER_TRAILS_VS"]->GetBufferPointer()),
		mShaders["_SKILL_LASER_TRAILS_VS"]->GetBufferSize()
	};
	skill_Laser_Trails_Desc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["_SKILL_LASER_TRAILS_PS"]->GetBufferPointer()),
		mShaders["_SKILL_LASER_TRAILS_PS"]->GetBufferSize()
	};

	skill_Laser_Trails_Desc.BlendState.RenderTarget[0] = transparencyBlendDesc;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skill_Laser_Trails_Desc, IID_PPV_ARGS(&mPSOs[ObjectData::RenderType::_SKILL_LASER_TRAILS_TYPE])));

}

float hpGuage = 0.3f;

void BoxApp::BuildGUIFrame() {
	//////////////////////
	// Load HP, MP Resource
	//////////////////////
	int width, height;
	Texture HP, MP, Storm, Thunder, Earthquake;

	HP.Filename = L"D:\\Portfolio\\source\\DX12\\DirectX\\MyDemos\\MyD3D12Project\\textures\\UI\\HP.dds";
	HP.isCube = false;
	HP.Name = "HP";

	MP.Filename = L"D:\\Portfolio\\source\\DX12\\DirectX\\MyDemos\\MyD3D12Project\\textures\\UI\\MP.dds";
	MP.isCube = false;
	MP.Name = "MP";

	Storm.Filename = L"D:\\Portfolio\\source\\DX12\\DirectX\\MyDemos\\MyD3D12Project\\textures\\UI\\Storm.dds";
	Storm.isCube = false;
	Storm.Name = "STORM";

	Thunder.Filename = L"D:\\Portfolio\\source\\DX12\\DirectX\\MyDemos\\MyD3D12Project\\textures\\UI\\Thunder.dds";
	Thunder.isCube = false;
	Thunder.Name = "THUNDER";

	Earthquake.Filename = L"D:\\Portfolio\\source\\DX12\\DirectX\\MyDemos\\MyD3D12Project\\textures\\UI\\earthquake.dds";
	Earthquake.isCube = false;
	Earthquake.Name = "EARTHQUAKE";
	
	uploadTexture(HP, width, height);
	uploadTexture(MP, width, height);
	uploadTexture(Storm);
	uploadTexture(Thunder);
	uploadTexture(Earthquake);

	//////////////////////
	// Upload Resource
	//////////////////////
	D3D12_DESCRIPTOR_HEAP_DESC GUISRVHeapDesc;

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
		mGUIDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		1,
		(UINT)mRtvDescriptorSize
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = -1;
		srvDesc.Format = HP.Resource->GetDesc().Format;
		md3dDevice->CreateShaderResourceView(HP.Resource.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

		srvDesc.Format = MP.Resource->GetDesc().Format;
		md3dDevice->CreateShaderResourceView(MP.Resource.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

		srvDesc.Format = Storm.Resource->GetDesc().Format;
		md3dDevice->CreateShaderResourceView(Storm.Resource.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

		srvDesc.Format = Thunder.Resource->GetDesc().Format;
		md3dDevice->CreateShaderResourceView(Thunder.Resource.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

		srvDesc.Format = Earthquake.Resource->GetDesc().Format;
		md3dDevice->CreateShaderResourceView(Earthquake.Resource.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE HPTex(
		mGUIDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);
	CD3DX12_GPU_DESCRIPTOR_HANDLE MPTex(
		mGUIDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);
	CD3DX12_GPU_DESCRIPTOR_HANDLE StormTex(
		mGUIDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);
	CD3DX12_GPU_DESCRIPTOR_HANDLE ThunderTex(
		mGUIDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);
	CD3DX12_GPU_DESCRIPTOR_HANDLE EarthquakeTex(
		mGUIDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);
	HPTex.Offset(
		1,
		mCbvSrvUavDescriptorSize
	);
	MPTex.Offset(
		2,
		mCbvSrvUavDescriptorSize
	);
	StormTex.Offset(
		3,
		mCbvSrvUavDescriptorSize
	);
	ThunderTex.Offset(
		4,
		mCbvSrvUavDescriptorSize
	); 
	EarthquakeTex.Offset(
		5,
		mCbvSrvUavDescriptorSize
	);

	mGUIResources["HP"] = HPTex.ptr;
	mGUIResources["MP"] = MPTex.ptr;
	mGUIResources["STORM"] = StormTex.ptr;
	mGUIResources["Thunder"] = ThunderTex.ptr;
	mGUIResources["Earthquake"] = EarthquakeTex.ptr;

	// Append GUI Frame
	std::shared_ptr<ImGuiFrameComponent> clothParamComp = pushFrame("ClothParamGUI");

	clothParamComp->pushToggleComponent(isUseGlobalPose, "isUseGlobalPoseToggle", "isUseGlobalPoseToggle");

	clothParamComp->pushSliderFloatComponent(mGlobalVec.x, "GlobalVecX", "GlobalVecX", -1.0f, 1.0f);
	clothParamComp->pushSliderFloatComponent(mGlobalVec.y, "GlobalVecY", "GlobalVecY", -1.0f, 1.0f);
	clothParamComp->pushSliderFloatComponent(mGlobalVec.z, "GlobalVecZ", "GlobalVecZ", -1.0f, 1.0f);

	clothParamComp->pushSliderFloatComponent(mLocalScale.x, "LocalScaleX", "LocalScaleX", -0.01f, 0.01f);
	clothParamComp->pushSliderFloatComponent(mLocalScale.y, "LocalScaleY", "LocalScaleY", -0.1f, 0.1f);
	clothParamComp->pushSliderFloatComponent(mLocalScale.z, "LocalScaleZ", "LocalScaleZ", -0.01f, 0.01f);

	//////////////////////////////////////
	// Initialize Quest Info
	//////////////////////////////////////
	mQuestParamComp = pushFrame("Quest");

	mODBC.GetCurrentQuest(mQuestData);

	std::wstring targetWName(mQuestData.target_name);
	std::string targetName;

	targetName.assign(targetWName.begin(), targetWName.end());

	mQuestParamComp->pushTextComponent("TargetHint", "Target: " + targetName);
	mQuestParamComp->pushTextComponent("NumberHint", 
		"Number: " + 
		std::to_string(mQuestData.target_number) + 
		" / " + 
		std::to_string(mQuestData.target_max_number)
	);

	mQuestParamComp->pushTextComponent("DestinationHint", 
		"Destination : <" + 
		std::to_string(mQuestData.destination_pos_x) + 
		", " + 
		std::to_string(mQuestData.destination_pos_y) + 
		">"
	);

	mQuestParamComp->pushTextComponent("LimitTimeHint",
		"LimitTime :" +
		std::to_string(mQuestData.limit_time)
	);
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> BoxApp::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6,
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f,
		16,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
	);

	return {
		pointWrap, 
		pointClamp,
		linearWrap, 
		linearClamp,
		anisotropicWrap, 
		anisotropicClamp, 
		shadow 
	};
}

void BoxApp::RecursionChildItem(RenderItem* go)
{
	ObjectData* obj = mGameObjectDatas[go->mName];

	if (go->mParent)
		obj->mParentName = go->mParent->mName;

	// 비어있는 객체일 시 패스
	if (obj->SubmeshCount > 0)
	{
		if (obj->mFormat == "PMX")
		{
			pmx::PmxVertexSkinningBDEF1* BDEF1 = NULL;
			pmx::PmxVertexSkinningBDEF2* BDEF2 = NULL;
			pmx::PmxVertexSkinningBDEF4* BDEF4 = NULL;

			pmx::PmxVertex* mFromVert = obj->mModel.vertices.get();
			Vertex* mToVert;

			int vSize = obj->mModel.vertex_count;
			obj->mVertices.resize(vSize);

			for (int vLoop = 0; vLoop < vSize; vLoop++)
			{
				mToVert = &obj->mVertices[vLoop];

				mToVert->Pos.x = mFromVert[vLoop].position[0];
				mToVert->Pos.y = mFromVert[vLoop].position[1];
				mToVert->Pos.z = mFromVert[vLoop].position[2];

				mToVert->Normal.x = mFromVert[vLoop].normal[0];
				mToVert->Normal.y = mFromVert[vLoop].normal[1];
				mToVert->Normal.z = mFromVert[vLoop].normal[2];

				mToVert->Tangent.x = mFromVert[vLoop].tangent[0];
				mToVert->Tangent.y = mFromVert[vLoop].tangent[1];
				mToVert->Tangent.z = mFromVert[vLoop].tangent[2];

				mToVert->TexC.x = mFromVert[vLoop].uv[0];
				mToVert->TexC.y = mFromVert[vLoop].uv[1];

				if (obj->mModel.vertices[vLoop].skinning_type == pmx::PmxVertexSkinningType::BDEF1)
				{
					BDEF1 = (pmx::PmxVertexSkinningBDEF1*)obj->mModel.vertices[vLoop].skinning.get();

					mToVert->BoneIndices.x = BDEF1->bone_index;
					mToVert->BoneWeights.x = 1.0f;

					mToVert->BoneIndices.y = -1;
					mToVert->BoneWeights.y = 0.0f;

					mToVert->BoneIndices.z = -1;
					mToVert->BoneWeights.z = 0.0f;

					mToVert->BoneIndices.w = -1;
					mToVert->BoneWeights.w = 0.0f;

				}
				else if (obj->mModel.vertices[vLoop].skinning_type == pmx::PmxVertexSkinningType::BDEF2)
				{
					BDEF2 = (pmx::PmxVertexSkinningBDEF2*)obj->mModel.vertices[vLoop].skinning.get();

					mToVert->BoneIndices.x = BDEF2->bone_index1;
					mToVert->BoneWeights.x = BDEF2->bone_weight;

					mToVert->BoneIndices.y = BDEF2->bone_index2;
					mToVert->BoneWeights.y = 1.0f - BDEF2->bone_weight;

					mToVert->BoneIndices.z = -1;
					mToVert->BoneWeights.z = 0.0f;

					mToVert->BoneIndices.w = -1;
					mToVert->BoneWeights.w = 0.0f;
				}
				else
				{
					BDEF4 = (pmx::PmxVertexSkinningBDEF4*)obj->mModel.vertices[vLoop].skinning.get();

					mToVert->BoneIndices.x = BDEF4->bone_index1;
					mToVert->BoneWeights.x = BDEF4->bone_weight1;

					mToVert->BoneIndices.y = BDEF4->bone_index2;
					mToVert->BoneWeights.y = BDEF4->bone_weight2;

					mToVert->BoneIndices.z = BDEF4->bone_index3;
					mToVert->BoneWeights.z = BDEF4->bone_weight3;

					mToVert->BoneIndices.w = BDEF4->bone_index4;
					mToVert->BoneWeights.w = BDEF4->bone_weight4;
				}

			}

			// CPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				obj->mGeometry.VertexBufferByteSize,
				&obj->mGeometry.VertexBufferCPU
			));
			CopyMemory(
				obj->mGeometry.VertexBufferCPU->GetBufferPointer(),
				obj->mVertices.data(),
				obj->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				obj->mGeometry.IndexBufferByteSize,
				&obj->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				obj->mGeometry.IndexBufferCPU->GetBufferPointer(),
				obj->mModel.indices.get(),
				obj->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			obj->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				obj->mVertices.data(),
				obj->mGeometry.VertexBufferByteSize,
				obj->mGeometry.VertexBufferUploader);

			// GPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			obj->mGeometry.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				obj->mModel.indices.get(),
				obj->mGeometry.IndexBufferByteSize,
				obj->mGeometry.IndexBufferUploader);

			obj->mGeometry.VertexByteStride = sizeof(Vertex);
			obj->mGeometry.IndexFormat = DXGI_FORMAT_R32_UINT;

			//mGameObjectDatas[go->mName]->vertices.clear();
			//mGameObjectDatas[go->mName]->vertices.resize(0);
		} // (go->mFormat == "PMX")
		else if (obj->isBillBoard)
		{
			// CPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				obj->mGeometry.VertexBufferByteSize,
				&obj->mGeometry.VertexBufferCPU
			));
			CopyMemory(
				obj->mGeometry.VertexBufferCPU->GetBufferPointer(),
				obj->mBillBoardVertices.data(),
				obj->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				obj->mGeometry.IndexBufferByteSize,
				&obj->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				obj->mGeometry.IndexBufferCPU->GetBufferPointer(),
				obj->mIndices.data(),
				obj->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			obj->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				obj->mBillBoardVertices.data(),
				obj->mGeometry.VertexBufferByteSize,
				obj->mGeometry.VertexBufferUploader);

			// GPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			obj->mGeometry.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				obj->mIndices.data(),
				obj->mGeometry.IndexBufferByteSize,
				obj->mGeometry.IndexBufferUploader);

			obj->mGeometry.VertexByteStride = sizeof(BillBoardSpriteVertex);
			obj->mGeometry.IndexFormat = DXGI_FORMAT_R32_UINT;
		} // else if (obj->isBillBoard)
		else 
		{
			// CPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				obj->mGeometry.VertexBufferByteSize,
				&obj->mGeometry.VertexBufferCPU
			));
			CopyMemory(
				obj->mGeometry.VertexBufferCPU->GetBufferPointer(),
				obj->mVertices.data(),
				obj->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				obj->mGeometry.IndexBufferByteSize,
				&obj->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				obj->mGeometry.IndexBufferCPU->GetBufferPointer(),
				obj->mIndices.data(),
				obj->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			obj->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				obj->mVertices.data(),
				obj->mGeometry.VertexBufferByteSize,
				obj->mGeometry.VertexBufferUploader);

			// GPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			obj->mGeometry.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				obj->mIndices.data(),
				obj->mGeometry.IndexBufferByteSize,
				obj->mGeometry.IndexBufferUploader);

			obj->mGeometry.VertexByteStride = sizeof(Vertex);
			obj->mGeometry.IndexFormat = DXGI_FORMAT_R32_UINT;
		} // (go->mFormat != "PMX")

		if (obj->isDebugBox)
		{
			ObjectData* debugBoxData = obj->mDebugBoxData.get();

			// CPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				debugBoxData->mGeometry.VertexBufferByteSize,
				&debugBoxData->mGeometry.VertexBufferCPU
			));
			CopyMemory(
				debugBoxData->mGeometry.VertexBufferCPU->GetBufferPointer(),
				debugBoxData->mVertices.data(),
				debugBoxData->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				debugBoxData->mGeometry.IndexBufferByteSize,
				&debugBoxData->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				debugBoxData->mGeometry.IndexBufferCPU->GetBufferPointer(),
				debugBoxData->mIndices.data(),
				debugBoxData->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			debugBoxData->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				debugBoxData->mVertices.data(),
				debugBoxData->mGeometry.VertexBufferByteSize,
				debugBoxData->mGeometry.VertexBufferUploader
			);

			// GPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			debugBoxData->mGeometry.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				debugBoxData->mIndices.data(),
				debugBoxData->mGeometry.IndexBufferByteSize,
				debugBoxData->mGeometry.IndexBufferUploader
			);

			debugBoxData->mGeometry.VertexByteStride = sizeof(Vertex);
			debugBoxData->mGeometry.IndexFormat = DXGI_FORMAT_R32_UINT;
		}
	}

	auto& iter = go->mChilds.begin();
	auto& endPtr = go->mChilds.end();

	while (iter != endPtr)
	{
		// 자식 노드를 렌더 업데이트
		if (go->mChilds.size() > 0)
			RecursionChildItem((*iter).second);

		iter++;
	}
}

void BoxApp::BuildRenderItem()
{
	for (RenderItem* go : mGameObjects)
	{
		RecursionChildItem(go);
	}
}

///////////////////////////////////////////
// Create or Modified Model Part
///////////////////////////////////////////


RenderItem* BoxApp::CreateGameObject(std::string Name, int instance)
{
	RenderItem* newGameObjects = new RenderItem();
	ObjectData* obj = new ObjectData();

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

	// Create Collider Body
	//phys.CreateBox(1, 1, 1);

	newGameObjects->InstanceCount = instance;

	obj->mName = Name;
	obj->InstanceCount = instance;

	newGameObjects->mData = obj;

	for (UINT i = 0; i < newGameObjects->InstanceCount; i++)
	{
		InstanceData id;
		id.World = MathHelper::Identity4x4();
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		obj->mInstances.push_back(id);

		BoundingBox bb;
		bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

		obj->Bounds.push_back(bb);

		Translate t;
		obj->mTranslate.push_back(t);

		//PhysResource mPhysRes;
		//obj->mPhysResources.push_back(mPhysRes);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = obj;

	return newGameObjects;
}

RenderItem* BoxApp::CreateStaticGameObject(std::string Name, int instance)
{
	RenderItem* newGameObjects	= new RenderItem();
	PxRigidStatic* staticObj	= nullptr;
	PxShape* sphere				= nullptr;

	sphere = mPhys.CreateSphere(1.0f);

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

	// Create Collider Body
	//phys.CreateBox(1, 1, 1);

	newGameObjects->InstanceCount = instance;

	ObjectData* obj = new ObjectData();

	obj->mName = Name;
	//obj->mPhysResources.resize(instance);
	for (UINT i = 0; i < newGameObjects->InstanceCount; i++)
	{
		InstanceData id;
		id.World = MathHelper::Identity4x4();
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		obj->mInstances.push_back(id);

		BoundingBox bb;
		bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

		obj->Bounds.push_back(bb);

		staticObj = mPhys.CreateStatic(PxTransform(PxVec3(0, 0, 0)), sphere);

		//mPhys.BindObjColliber(
		//	staticObj,
		//	&obj->mPhysResources[i]
		//);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = obj;
	newGameObjects->mData = obj;

	return newGameObjects;
}


RenderItem* BoxApp::CreateKinematicGameObject(std::string Name, int instance)
{
	RenderItem* newGameObjects = new RenderItem();
	PxRigidDynamic* dynamicObj = nullptr;
	PxShape* sphere = nullptr;

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

	// Create Collider Body
	//phys.CreateBox(1, 1, 1);

	newGameObjects->InstanceCount = instance;

	ObjectData* obj = new ObjectData();

	obj->mName = Name;
	//obj->mPhysResources.resize(instance);
	for (UINT i = 0; i < obj->InstanceCount; i++)
	{
		InstanceData id;
		id.World = MathHelper::Identity4x4();
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		obj->mInstances.push_back(id);

		BoundingBox bb;
		bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

		obj->Bounds.push_back(bb);

		sphere = mPhys.CreateSphere(1.0f);

		dynamicObj = mPhys.CreateKinematic(PxTransform(PxVec3(0, 0, 0)), sphere, 1, PxVec3(0, 0, 0));
		obj->mPhysRigidBody.push_back(dynamicObj);

		//mPhys.BindObjColliber(
		//	dynamicObj,
		//	&obj->mPhysResources[i]
		//);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = obj;
	newGameObjects->mData = obj;

	return newGameObjects;
}

RenderItem* BoxApp::CreateDynamicGameObject(std::string Name, int instance)
{
	RenderItem* newGameObjects = new RenderItem();
	PxRigidDynamic* dynamicObj = nullptr;

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

	newGameObjects->InstanceCount = instance;

	ObjectData* obj = new ObjectData();

	obj->mName = Name;
	//obj->mPhysResources.resize(instance);
	for (UINT i = 0; i < newGameObjects->InstanceCount; i++)
	{
		InstanceData id;
		id.World = MathHelper::Identity4x4();
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		obj->mInstances.push_back(id);

		BoundingBox bb;
		bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

		// Bind Phy
		obj->Bounds.push_back(bb);

		dynamicObj = mPhys.CreateDynamic(PxTransform(PxVec3(0, 0, 0)), PxSphereGeometry(3), PxVec3(0, 0, 0));
		obj->mPhysRigidBody.push_back(dynamicObj);

		//mPhys.BindObjColliber(
		//	dynamicObj,
		//	&obj->mPhysResources[i]
		//);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = obj;

	mGameObjectDatas[Name]->InstanceCount = instance;
	newGameObjects->mData = obj;

	return newGameObjects;
}

void BoxApp::CreateBoxObject(
	std::string Name,
	std::string textuerName,
	std::string Format,
	RenderItem* r,
	float x,
	float y,
	float z,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	int subDividNum,
	ObjectData::RenderType renderType,
	bool isDrawShadow,
	bool isDrawTexture
)
{
	GeometryGenerator Geom;

	rotation.x = DirectX::XMConvertToRadians(rotation.x);
	rotation.y = DirectX::XMConvertToRadians(rotation.y);
	rotation.z = DirectX::XMConvertToRadians(rotation.z);

	DirectX::XMVECTOR mPositionVec = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR mRotationVec = DirectX::XMLoadFloat3(&rotation);
	DirectX::XMVECTOR mScaleVec = DirectX::XMLoadFloat3(&scale);

	DirectX::XMVECTOR mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);
	DirectX::XMVECTOR mOrientationVec;

	mOrientationVec.m128_f32[0] = 0.0f;
	mOrientationVec.m128_f32[1] = 0.0f;
	mOrientationVec.m128_f32[2] = 0.0f;
	mOrientationVec.m128_f32[3] = 1.0f;

	DirectX::XMMATRIX mWorldMat = DirectX::XMMatrixAffineTransformation(
		mScaleVec,
		mOrientationVec,
		mQuaternionVec,
		mPositionVec
	);

	r->mFormat = Format;

	// init Boundary(Collider) Box
	DirectX::XMMATRIX mBoundScaleMat = {};
	mBoundScaleMat.r[0].m128_f32[0] = x;
	mBoundScaleMat.r[1].m128_f32[1] = y;
	mBoundScaleMat.r[2].m128_f32[2] = z;
	mBoundScaleMat.r[3].m128_f32[3] = 1.0f;

	mWorldMat *= mBoundScaleMat;

	// input subGeom
	GeometryGenerator::MeshData Box;
	Box = Geom.CreateBox(x, y, z, subDividNum);

	ObjectData* obj = mGameObjectDatas[r->mName];
	if (!obj)
	{
		obj = new ObjectData();

		for (UINT i = 0; i < r->InstanceCount; i++)
		{
			InstanceData id;
			id.World = MathHelper::Identity4x4();
			id.TexTransform = MathHelper::Identity4x4();
			id.MaterialIndex = 0;

			obj->mInstances.push_back(id);

			BoundingBox bb;
			bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

			// Bind Phy
			obj->Bounds.push_back(bb);
		}

		//obj->mPhysResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	//r->mData = obj;

	if (obj->mInstances.size() == 0)
		throw std::runtime_error("Instance Size must bigger than 0.");

	size_t resourceSize = obj->mInstances.size();
	InstanceIterator = obj->mInstances.begin();
	BoundsIterator = obj->Bounds.begin();

	for (UINT inst = 0; inst < resourceSize; inst++)
	{
		(*BoundsIterator).Transform((*BoundsIterator), mWorldMat);
		DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

		obj->mTranslate.push_back(
			Translate(
				mPositionVec,
				mRotationVec,
				mScaleVec,
				mWorldMat
			)
		);

		InstanceIterator++;
		BoundsIterator++;
		//memcpy(obj->mPhysResources[inst].Position, &position, sizeof(float) * 3);
	}

	obj->mName = r->mName;
	obj->mFormat = r->mFormat;
	obj->mRenderType = renderType;

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount += 1;
	obj->mDesc.resize(obj->SubmeshCount);

	obj->isCloth.push_back(false);
	obj->isRigidBody.push_back(false);


	SubmeshGeometry mSubmesh;
	mSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	mSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	mSubmesh.IndexSize = (UINT)Box.Indices32.size();
	mSubmesh.VertexSize = (UINT)Box.Vertices.size();

	obj->mDesc[0].StartIndexLocation = mSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = mSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = mSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = mSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs.push_back(mSubmesh);
	obj->mGeometry.DrawArgs[obj->mGeometry.DrawArgs.size() - 1].textureName = (textuerName.c_str());
	obj->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = obj->mVertices.size();
	XMVECTOR posV;

	obj->mVertices.resize(startV + Box.Vertices.size());

	Vertex* v = obj->mVertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->mIndices.insert(
		obj->mIndices.end(),
		std::begin(Box.Indices32),
		std::end(Box.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	obj->mGeometry.IndexSize			+= (UINT)Box.Indices32.size();

	obj->mGeometry.VertexBufferByteSize	+= (UINT)Box.Vertices.size() * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize	+= (UINT)Box.Indices32.size() * sizeof(std::uint32_t);
}

void BoxApp::CreateSphereObject(
	std::string Name,
	std::string textuerName,
	std::string Format,
	RenderItem* r,
	float rad,
	int sliceCount,
	int stackCount,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	ObjectData::RenderType renderType,
	bool isDrawShadow,
	bool isDrawTexture
)
{
	GeometryGenerator Geom;

	rotation.x = DirectX::XMConvertToRadians(rotation.x);
	rotation.y = DirectX::XMConvertToRadians(rotation.y);
	rotation.z = DirectX::XMConvertToRadians(rotation.z);

	scale.x *= rad;
	scale.y *= rad;
	scale.z *= rad;

	DirectX::XMVECTOR mPositionVec = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR mRotationVec = DirectX::XMLoadFloat3(&rotation);
	DirectX::XMVECTOR mScaleVec = DirectX::XMLoadFloat3(&scale);

	DirectX::XMVECTOR mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);
	DirectX::XMVECTOR mOrientationVec;

	mOrientationVec.m128_f32[0] = 0.0f;
	mOrientationVec.m128_f32[1] = 0.0f;
	mOrientationVec.m128_f32[2] = 0.0f;
	mOrientationVec.m128_f32[3] = 1.0f;

	DirectX::XMMATRIX mWorldMat = DirectX::XMMatrixAffineTransformation(
		mScaleVec,
		mOrientationVec,
		mQuaternionVec,
		mPositionVec
	);

	r->mFormat = Format;

	// input subGeom
	GeometryGenerator::MeshData Sphere;
	Sphere = Geom.CreateSphere(rad, sliceCount, stackCount);

	ObjectData* obj = mGameObjectDatas[r->mName];
	if (!obj)
	{
		obj = new ObjectData();

		for (UINT i = 0; i < r->InstanceCount; i++)
		{
			InstanceData id;
			id.World = MathHelper::Identity4x4();
			id.TexTransform = MathHelper::Identity4x4();
			id.MaterialIndex = 0;

			obj->mInstances.push_back(id);

			BoundingBox bb;
			bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

			// Bind Phy
			obj->Bounds.push_back(bb);
		}

		//obj->mPhysResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	//r->mData = obj;

	if (obj->mInstances.size() == 0)
		throw std::runtime_error("Instance Size must bigger than 0.");

	InstanceIterator = obj->mInstances.begin();
	BoundsIterator = obj->Bounds.begin();
	size_t resourceSize = obj->mInstances.size();
	for (UINT inst = 0; inst < resourceSize; inst++)
	{
		(*BoundsIterator).Transform((*BoundsIterator), mWorldMat);
		DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

		obj->mTranslate.push_back(
			Translate(
				mPositionVec,
				mRotationVec,
				mScaleVec,
				mWorldMat
			)
		);

		InstanceIterator++;
		BoundsIterator++;
		//memcpy(obj->mPhysResources[inst].Position, &position, sizeof(float) * 3);
	}

	obj->mName = r->mName;
	obj->mFormat = r->mFormat;
	obj->mRenderType = renderType;

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = 1;
	obj->mDesc.resize(1);

	obj->isCloth.push_back(false);
	obj->isRigidBody.push_back(false);

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry mSubmesh;
	mSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	mSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	mSubmesh.IndexSize = (UINT)Sphere.Indices32.size();
	mSubmesh.VertexSize = (UINT)Sphere.Vertices.size();

	obj->mDesc[0].StartIndexLocation = mSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = mSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = mSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = mSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs.push_back(mSubmesh);
	obj->mGeometry.DrawArgs[obj->mGeometry.DrawArgs.size() - 1].textureName = textuerName.c_str();
	obj->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = obj->mVertices.size();
	XMVECTOR posV;

	obj->mVertices.resize(startV + Sphere.Vertices.size());

	Vertex* v = obj->mVertices.data();
	for (size_t i = 0; i < (Sphere.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Sphere.Vertices[i].Position);

		v[i + startV].Normal = Sphere.Vertices[i].Normal;
		v[i + startV].TexC = Sphere.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->mIndices.insert(
		obj->mIndices.end(),
		std::begin(Sphere.Indices32),
		std::end(Sphere.Indices32)
	);

	obj->mGeometry.IndexSize			+= (UINT)Sphere.Indices32.size();

	obj->mGeometry.VertexBufferByteSize	+= (UINT)Sphere.Vertices.size() * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize	+= (UINT)Sphere.Indices32.size() * sizeof(std::uint32_t);
}

void BoxApp::CreateGeoSphereObject(
	std::string Name,
	std::string textuerName,
	std::string Format,
	RenderItem* r,
	float rad,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	int subdivid,
	ObjectData::RenderType renderType,
	bool isDrawShadow,
	bool isDrawTexture
)
{
	GeometryGenerator Geom;

	rotation.x = DirectX::XMConvertToRadians(rotation.x);
	rotation.y = DirectX::XMConvertToRadians(rotation.y);
	rotation.z = DirectX::XMConvertToRadians(rotation.z);

	DirectX::XMVECTOR mPositionVec = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR mRotationVec = DirectX::XMLoadFloat3(&rotation);
	DirectX::XMVECTOR mScaleVec = DirectX::XMLoadFloat3(&scale);

	DirectX::XMVECTOR mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);
	DirectX::XMVECTOR mOrientationVec;

	mOrientationVec.m128_f32[0] = 0.0f;
	mOrientationVec.m128_f32[1] = 0.0f;
	mOrientationVec.m128_f32[2] = 0.0f;
	mOrientationVec.m128_f32[3] = 1.0f;

	DirectX::XMMATRIX mWorldMat = DirectX::XMMatrixAffineTransformation(
		mScaleVec,
		mOrientationVec,
		mQuaternionVec,
		mPositionVec
	);

	r->mFormat = Format;

	// input subGeom
	GeometryGenerator::MeshData Sphere;
	Sphere = Geom.CreateGeosphere(rad, subdivid);

	ObjectData* obj = mGameObjectDatas[r->mName];
	if (!obj)
	{
		obj = new ObjectData();

		for (UINT i = 0; i < r->InstanceCount; i++)
		{
			InstanceData id;
			id.World = MathHelper::Identity4x4();
			id.TexTransform = MathHelper::Identity4x4();
			id.MaterialIndex = 0;

			obj->mInstances.push_back(id);

			BoundingBox bb;
			bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

			// Bind Phy
			obj->Bounds.push_back(bb);
		}

		//obj->mPhysResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	//r->mData = obj;

	if (obj->mInstances.size() == 0)
		throw std::runtime_error("Instance Size must bigger than 0.");

	InstanceIterator = obj->mInstances.begin();
	BoundsIterator = obj->Bounds.begin();
	size_t resourceSize = obj->mInstances.size();
	for (UINT inst = 0; inst < resourceSize; inst++)
	{
		(*BoundsIterator).Transform((*BoundsIterator), mWorldMat);
		DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

		obj->mTranslate.push_back(
			Translate(
				mPositionVec,
				mRotationVec,
				mScaleVec,
				mWorldMat
			)
		);

		InstanceIterator++;
		BoundsIterator++;
		// memcpy(obj->mPhysResources[inst].Position, &position, sizeof(float) * 3);
	}

	obj->mName = r->mName;
	obj->mFormat = r->mFormat;
	obj->mRenderType = renderType;

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = 1;
	obj->mDesc.resize(1);

	obj->isCloth.push_back(false);
	obj->isRigidBody.push_back(false);

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry mSubmesh;
	mSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	mSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	mSubmesh.IndexSize = (UINT)Sphere.Indices32.size();
	mSubmesh.VertexSize = (UINT)Sphere.Vertices.size();

	obj->mDesc[0].StartIndexLocation = mSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = mSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = mSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = mSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs.push_back(mSubmesh);
	obj->mGeometry.DrawArgs[obj->mGeometry.DrawArgs.size() - 1].textureName = textuerName.c_str();
	obj->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = obj->mVertices.size();
	XMVECTOR posV;

	obj->mVertices.resize(startV + Sphere.Vertices.size());

	Vertex* v = obj->mVertices.data();
	for (size_t i = 0; i < (Sphere.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Sphere.Vertices[i].Position);

		v[i + startV].Normal = Sphere.Vertices[i].Normal;
		v[i + startV].TexC = Sphere.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->mIndices.insert(
		obj->mIndices.end(),
		std::begin(Sphere.Indices32),
		std::end(Sphere.Indices32)
	);

	obj->mGeometry.IndexSize			+= (UINT)Sphere.Indices32.size();

	obj->mGeometry.VertexBufferByteSize	+= (UINT)Sphere.Vertices.size() * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize	+= (UINT)Sphere.Indices32.size() * sizeof(std::uint32_t);
}

void BoxApp::CreateCylinberObject(
	std::string Name,
	std::string textuerName,
	std::string Format,
	RenderItem* r,
	float bottomRad,
	float topRad,
	float height,
	int sliceCount,
	int stackCount,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	ObjectData::RenderType renderType,
	bool isDrawShadow,
	bool isDrawTexture
)
{
	GeometryGenerator Geom;

	rotation.x = DirectX::XMConvertToRadians(rotation.x);
	rotation.y = DirectX::XMConvertToRadians(rotation.y);
	rotation.z = DirectX::XMConvertToRadians(rotation.z);

	DirectX::XMVECTOR mPositionVec = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR mRotationVec = DirectX::XMLoadFloat3(&rotation);
	DirectX::XMVECTOR mScaleVec = DirectX::XMLoadFloat3(&scale);

	DirectX::XMVECTOR mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);
	DirectX::XMVECTOR mOrientationVec;

	mOrientationVec.m128_f32[0] = 0.0f;
	mOrientationVec.m128_f32[1] = 0.0f;
	mOrientationVec.m128_f32[2] = 0.0f;
	mOrientationVec.m128_f32[3] = 1.0f;

	DirectX::XMMATRIX mWorldMat = DirectX::XMMatrixAffineTransformation(
		mScaleVec,
		mOrientationVec,
		mQuaternionVec,
		mPositionVec
	);

	r->mFormat = Format;

	// input subGeom
	GeometryGenerator::MeshData Box;
	Box = Geom.CreateCylinder(bottomRad, topRad, height, sliceCount, stackCount);

	ObjectData* obj = mGameObjectDatas[r->mName];
	if (!obj)
	{
		obj = new ObjectData();

		for (UINT i = 0; i < r->InstanceCount; i++)
		{
			InstanceData id;
			id.World = MathHelper::Identity4x4();
			id.TexTransform = MathHelper::Identity4x4();
			id.MaterialIndex = 0;

			obj->mInstances.push_back(id);

			BoundingBox bb;
			bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

			// Bind Phy
			obj->Bounds.push_back(bb);
		}

		//obj->mPhysResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	//r->mData = obj;

	if (obj->mInstances.size() == 0)
		throw std::runtime_error("Instance Size must bigger than 0.");
	
	InstanceIterator = obj->mInstances.begin();
	BoundsIterator = obj->Bounds.begin();
	size_t resourceSize = obj->mInstances.size();
	for (UINT inst = 0; inst < resourceSize; inst++)
	{
		(*BoundsIterator).Transform((*BoundsIterator), mWorldMat);
		DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

		obj->mTranslate.push_back(
			Translate(
				mPositionVec,
				mRotationVec,
				mScaleVec,
				mWorldMat
			)
		);

		InstanceIterator++;
		BoundsIterator++;
		//memcpy(obj->mPhysResources[inst].Position, &position, sizeof(float) * 3);
	}

	obj->mName = r->mName;
	obj->mFormat = r->mFormat;
	obj->mRenderType = renderType;

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = 1;
	obj->mDesc.resize(1);

	obj->isCloth.push_back(false);
	obj->isRigidBody.push_back(false);

	SubmeshGeometry mSubmesh;
	mSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	mSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	mSubmesh.IndexSize = (UINT)Box.Indices32.size();
	mSubmesh.VertexSize = (UINT)Box.Vertices.size();

	obj->mDesc[0].StartIndexLocation = mSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = mSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = mSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = mSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs.push_back(mSubmesh);
	obj->mGeometry.DrawArgs[obj->mGeometry.DrawArgs.size() - 1].textureName = textuerName.c_str();
	obj->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = obj->mVertices.size();
	XMVECTOR posV;

	obj->mVertices.resize(startV + Box.Vertices.size());

	Vertex* v = obj->mVertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->mIndices.insert(
		obj->mIndices.end(),
		std::begin(Box.Indices32),
		std::end(Box.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	obj->mGeometry.IndexSize			+= (UINT)Box.Indices32.size();

	obj->mGeometry.VertexBufferByteSize	+= (UINT)Box.Vertices.size() * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize	+= (UINT)Box.Indices32.size() * sizeof(std::uint32_t);
}

void BoxApp::CreateGridObject(
	std::string Name,
	std::string textuerName,
	std::string Format,
	RenderItem* r,
	float w, float h,
	int wc, int hc,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	ObjectData::RenderType renderType,
	bool isDrawShadow,
	bool isDrawTexture
)
{
	GeometryGenerator Geom;

	DirectX::XMMATRIX mWorldMat;
	InstanceData id;

	DirectX::XMVECTOR mPositionVec;
	DirectX::XMVECTOR mRotationVec;
	DirectX::XMVECTOR mScaleVec;

	DirectX::XMVECTOR mQuaternionVec;
	DirectX::XMVECTOR mOrientationVec;

	{
		mOrientationVec.m128_f32[0] = 0.0f;
		mOrientationVec.m128_f32[1] = 0.0f;
		mOrientationVec.m128_f32[2] = 0.0f;
		mOrientationVec.m128_f32[3] = 1.0f;

		rotation.x = DirectX::XMConvertToRadians(rotation.x);
		rotation.y = DirectX::XMConvertToRadians(rotation.y);
		rotation.z = DirectX::XMConvertToRadians(rotation.z);

		mPositionVec	= DirectX::XMLoadFloat3(&position);
		mRotationVec	= DirectX::XMLoadFloat3(&rotation);
		mScaleVec		= DirectX::XMLoadFloat3(&scale);

		mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);

		mWorldMat = DirectX::XMMatrixAffineTransformation(
			mScaleVec,
			mOrientationVec,
			mQuaternionVec,
			mPositionVec
		);

		if (r->InstanceCount == 0)
			throw std::runtime_error("Instance Size must bigger than 0.");
	}

	r->mFormat = Format;

	// input subGeom
	GeometryGenerator::MeshData Box;
	Box = Geom.CreateGrid(w, h, wc, hc);

	ObjectData* obj = mGameObjectDatas[r->mName];
	if (!obj)
	{
		obj = new ObjectData();

		for (UINT i = 0; i < r->InstanceCount; i++)
		{
			InstanceData id;
			id.World = MathHelper::Identity4x4();
			id.TexTransform = MathHelper::Identity4x4();
			id.MaterialIndex = 0;

			obj->mInstances.push_back(id);

			BoundingBox bb;
			bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

			// Bind Phy
			obj->Bounds.push_back(bb);
		}

		//obj->mPhysResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	//r->mData = obj;

	InstanceIterator = obj->mInstances.begin();
	size_t resourceSize = obj->mInstances.size();
	for (UINT inst = 0; inst < resourceSize; inst++)
	{
		DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

		obj->mTranslate.push_back(
			Translate(
				mPositionVec,
				mRotationVec,
				mScaleVec,
				mWorldMat
			)
		);

		InstanceIterator++;
		//memcpy(obj->mPhysResources[inst].Position, &position, sizeof(float) * 3);
	}

	obj->mName = r->mName;
	obj->mFormat = r->mFormat;
	obj->mRenderType = renderType;

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = 1;
	obj->mDesc.resize(1);

	obj->isCloth.push_back(false);
	obj->isRigidBody.push_back(false);

	if (renderType == ObjectData::RenderType::_DRAW_MAP_RENDER_TYPE)
		obj->isDrawTexture = true;

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry mSubmesh;
	mSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	mSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	mSubmesh.IndexSize = (UINT)Box.Indices32.size();
	mSubmesh.VertexSize = (UINT)Box.Vertices.size();

	// init Boundary(Collider) Box
	BoundsIterator = obj->Bounds.begin();
	if (obj->Bounds.size() > 0)
	{
		(*BoundsIterator).Extents.x = scale.x * w * 0.5f;
		(*BoundsIterator).Extents.y = scale.y;
		(*BoundsIterator).Extents.z = scale.z * h * 0.5f;
	}

	obj->mDesc[0].StartIndexLocation = mSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = mSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = mSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = mSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs.push_back(mSubmesh);
	obj->mGeometry.DrawArgs[obj->mGeometry.DrawArgs.size() - 1].textureName = textuerName.c_str();
	obj->mGeometry.meshNames.push_back(Name.c_str());

	size_t startV = obj->mVertices.size();
	XMVECTOR posV;

	obj->mVertices.resize(startV + Box.Vertices.size());

	Vertex* v = obj->mVertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->mIndices.insert(
		obj->mIndices.end(),
		std::begin(Box.Indices32),
		std::end(Box.Indices32)
	);

	obj->mGeometry.IndexSize			+= (UINT)Box.Indices32.size();

	obj->mGeometry.VertexBufferByteSize	+= (UINT)Box.Vertices.size() * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize	+= (UINT)Box.Indices32.size() * sizeof(std::uint32_t);
}

void BoxApp::CreateBillBoardObject(
	std::string Name,
	std::string textuerName,
	std::string Format,
	RenderItem* r,
	UINT particleCount,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT2 extends,
	ObjectData::RenderType renderType,
	bool isDrawShadow
)
{
	GeometryGenerator Geom;
	InstanceData id;

	r->mFormat = Format;

	ObjectData* obj = mGameObjectDatas[r->mName];
	mParticles[r->mName] = new Particle;

	if (!obj)
	{
		obj = new ObjectData();

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	if (!r->mData)
		r->mData = obj;

	obj->mName = r->mName;
	obj->mFormat = r->mFormat;
	obj->mRenderType = renderType;

	obj->InstanceCount = r->InstanceCount;
	obj->mInstances.resize(1);
	obj->SubmeshCount = 1;
	obj->mDesc.resize(1);

	obj->isCloth.push_back(false);
	obj->isRigidBody.push_back(false);

	if (renderType == ObjectData::RenderType::_DRAW_MAP_RENDER_TYPE)
		obj->isDrawTexture = true;

	obj->isAnim = false;
	obj->isBillBoard = true;
	obj->isDebugBox = false;

	// 각 서브메쉬에 대한 저장소 디스크립터 생성
	SubmeshGeometry mSubmesh;
	mSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	mSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	obj->mBillBoardVertices.resize(particleCount);
	obj->mIndices.resize(particleCount);

	//obj->mPhysResources.resize(particleCount);
	for (UINT partIDX = 0; partIDX < particleCount; partIDX++)
	{
		obj->mBillBoardVertices[partIDX].Pos	= position;
		obj->mBillBoardVertices[partIDX].Size	= extends;

		obj->mTranslate.push_back(
			Translate(
				{ position.x, position.y, position.z, 1.0f },
				{0.0f, 0.0f, 0.0f, 1.0f},
				{ extends.x, extends.y, 0.0f, 1.0f }
			)
		);

		obj->mIndices[partIDX] = partIDX;
	}

	// BillBoard는 1개의 포지션 포인트와 스케일 정보로 이루어져 있음.
	mSubmesh.VertexSize = (UINT)obj->mBillBoardVertices.size();
	mSubmesh.IndexSize = (UINT)obj->mIndices.size();

	obj->mDesc[0].BaseVertexLocation	= mSubmesh.BaseVertexLocation;
	obj->mDesc[0].StartIndexLocation	= mSubmesh.StartIndexLocation;

	obj->mDesc[0].VertexSize			= mSubmesh.VertexSize;
	obj->mDesc[0].IndexSize				= mSubmesh.IndexSize;

	// Submesh 개수 추가
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs.push_back(mSubmesh);
	obj->mGeometry.DrawArgs[obj->mGeometry.DrawArgs.size() - 1].textureName = textuerName.c_str();
	obj->mGeometry.meshNames.push_back(Name.c_str());

	// 다음 서브메쉬를 담기 위하여 SP(Stack Pointer)를 최신화 한다
	obj->mGeometry.IndexSize				+= mSubmesh.IndexSize;

	obj->mGeometry.VertexBufferByteSize		+= mSubmesh.VertexSize * sizeof(struct BillBoardSpriteVertex);
	obj->mGeometry.IndexBufferByteSize		+= mSubmesh.IndexSize * sizeof(std::uint32_t);
}

// Load Non-Skinned Object
void BoxApp::CreateFBXObject(
	std::string Name,
	std::string Path,
	std::string FileName,
	std::vector<std::string>& texturePath,
	RenderItem* r,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	ObjectData::RenderType renderType,
	bool uvMode,
	bool isDrawShadow,
	bool isDrawTexture
)
{
	GeometryGenerator Geom;

	// 게임 데이터 리스트에서 오브젝트를 찾고, 
	ObjectData* obj = mGameObjectDatas[r->mName];
	mParticles[r->mName] = new Particle;

	// 만일 존재하지 않는다면, 새로 생성
	if (!obj)
	{
		obj = new ObjectData();

		for (UINT i = 0; i < r->InstanceCount; i++)
		{
			InstanceData id;
			id.World = MathHelper::Identity4x4();
			id.TexTransform = MathHelper::Identity4x4();
			id.MaterialIndex = 0;

			obj->mInstances.push_back(id);

			BoundingBox bb;
			bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

			// Bind Phy
			obj->Bounds.push_back(bb);
		}

		//obj->mPhysResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	if (!r->mData)
		r->mData = obj;

	obj->mName = r->mName;
	obj->mFormat = "FBX";
	obj->mRenderType = renderType;

	rotation.x = DirectX::XMConvertToRadians(rotation.x);
	rotation.y = DirectX::XMConvertToRadians(rotation.y);
	rotation.z = DirectX::XMConvertToRadians(rotation.z);

	DirectX::XMVECTOR mPositionVec = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR mRotationVec = DirectX::XMLoadFloat3(&rotation);
	DirectX::XMVECTOR mScaleVec = DirectX::XMLoadFloat3(&scale);

	DirectX::XMVECTOR mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);
	DirectX::XMVECTOR mOrientationVec;

	mOrientationVec.m128_f32[0] = 0.0f;
	mOrientationVec.m128_f32[1] = 0.0f;
	mOrientationVec.m128_f32[2] = 0.0f;
	mOrientationVec.m128_f32[3] = 1.0f;

	DirectX::XMMATRIX mWorldMat = DirectX::XMMatrixAffineTransformation(
		mScaleVec,
		mOrientationVec,
		mQuaternionVec,
		mPositionVec
	);

	r->mFormat = "FBX";

	// init Boundary(Collider) Box
	InstanceIterator = obj->mInstances.begin();
	BoundsIterator = obj->Bounds.begin();
	(*BoundsIterator).Center = position;
	(*BoundsIterator).Extents = scale;

	// Physx와 연동시키기 위해, Transform 초기화
	size_t resourceSize = obj->mInstances.size();
	for (UINT inst = 0; inst < resourceSize; inst++)
	{
		DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

		obj->mTranslate.push_back(
			Translate(
				mPositionVec,
				mRotationVec,
				mScaleVec,
				mWorldMat
			)
		);

		InstanceIterator++;
		//memcpy(obj->mPhysResources[inst].Position, &position, sizeof(float) * 3);
	}

	// input subGeom
	std::vector<GeometryGenerator::MeshData> meshData;
	Geom.CreateFBXModel(
		meshData,
		(Path + "\\" + FileName),
		uvMode
	);

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = (UINT)meshData.size();
	obj->mDesc.resize(meshData.size());

	size_t startV = obj->mVertices.size();

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	// 서브메쉬 초기화
	for (int subMeshCount = 0; subMeshCount < meshData.size(); ++subMeshCount)
	{
		SubmeshGeometry mSubmesh;
		std::string submeshName = meshData[subMeshCount].mName;

		// 하나의 서브메쉬의 Vertex, Index 크기 저장
		mSubmesh.IndexSize = (UINT)meshData[subMeshCount].Indices32.size();
		mSubmesh.VertexSize = (UINT)meshData[subMeshCount].Vertices.size();

		// 현재 서브메쉬가 저장된 Offset 저장
		obj->mDesc[subMeshCount].BaseVertexLocation = vertexOffset;
		obj->mDesc[subMeshCount].StartIndexLocation = indexOffset;

		obj->mDesc[subMeshCount].IndexSize = mSubmesh.IndexSize;
		obj->mDesc[subMeshCount].VertexSize = mSubmesh.VertexSize;

		// 서브메쉬 당 Cloth Physx, RigidBody를 적용 시킬 것인가에 대한 Boolean
		obj->isCloth.push_back(false);
		obj->isRigidBody.push_back(false);

		// Submesh 개수 증가
		r->SubmeshCount += 1;

		obj->mGeometry.subMeshCount += 1;
		obj->mGeometry.DrawArgs.push_back(mSubmesh);

		obj->mGeometry.DrawArgs[subMeshCount].name = d3dUtil::getName(meshData[subMeshCount].texPath);
		obj->mGeometry.DrawArgs[subMeshCount].textureName = meshData[subMeshCount].texPath;

		obj->mGeometry.DrawArgs[subMeshCount].BaseVertexLocation = vertexOffset;
		obj->mGeometry.DrawArgs[subMeshCount].StartIndexLocation = indexOffset;

		// 서브메쉬 정보를 스택에 저장
		obj->mGeometry.meshNames.push_back(submeshName);
		texturePath.push_back(obj->mGeometry.DrawArgs[subMeshCount].textureName);

		// 서브메쉬의 Vertex, Index를 스택에 Push
		startV = obj->mVertices.size();
		
		obj->mVertices.resize(startV + meshData[subMeshCount].Vertices.size());
		Vertex* v = obj->mVertices.data();
		for (size_t i = 0; i < (meshData[subMeshCount].Vertices.size()); ++i)
		{
			v[i + startV].Pos = meshData[subMeshCount].Vertices[i].Position;
			v[i + startV].Normal = meshData[subMeshCount].Vertices[i].Normal;
			v[i + startV].Tangent = meshData[subMeshCount].Vertices[i].TangentU;
			v[i + startV].TexC = meshData[subMeshCount].Vertices[i].TexC;
		}

		obj->mIndices.insert(
			obj->mIndices.end(),
			std::begin(meshData[subMeshCount].Indices32),
			std::end(meshData[subMeshCount].Indices32)
		);

		// Texture, Material 바인딩
		{
			if (obj->mGeometry.DrawArgs[subMeshCount].textureName != "")
			{
				Texture charTex;
				std::string TexPath;
				std::wstring wTexPath;

				charTex.Name = d3dUtil::getName(meshData[subMeshCount].texPath);
				TexPath = d3dUtil::getDDSFormat(Path + "\\textures\\" + charTex.Name);
				wTexPath.assign(TexPath.begin(), TexPath.end());
				charTex.Filename = wTexPath;

				this->uploadTexture(charTex);
				this->uploadMaterial(charTex.Name, charTex.Name);

				this->BindMaterial(r, charTex.Name);
			}
			else
			{
				// 텍스쳐가 없거나 텍스쳐를 찾지 못하였을 경우 Default Texture, Materiai
				this->BindMaterial(r, "Default", "", "", "bricksTex");
			}

		}

		startV += mSubmesh.VertexSize;

		vertexOffset += mSubmesh.VertexSize;
		indexOffset += mSubmesh.IndexSize;

		obj->mGeometry.IndexSize += (UINT)mSubmesh.IndexSize;

		obj->mGeometry.VertexBufferByteSize += mSubmesh.VertexSize * sizeof(Vertex);
		obj->mGeometry.IndexBufferByteSize += mSubmesh.IndexSize * sizeof(std::uint32_t);
	} // for (int subMeshCount = 0; subMeshCount < meshData.size(); ++subMeshCount)
}

// Load Non-Skinned Object
void BoxApp::CreateFBXObjectSplitSubmeshs(
	std::string Name,
	std::string Path,
	std::string FileName,
	RenderItem* r,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	ObjectData::RenderType renderType,
	UINT submeshIDX,
	bool uvMode,
	bool isDrawShadow,
	bool isDrawTexture
)
{
	GeometryGenerator Geom;

	// 게임 데이터 리스트에서 오브젝트를 찾고, 
	ObjectData* obj = mGameObjectDatas[r->mName];
	// 만일 존재하지 않는다면, 새로 생성
	if (!obj)
	{
		obj = new ObjectData();

		for (UINT i = 0; i < r->InstanceCount; i++)
		{
			InstanceData id;
			id.World = MathHelper::Identity4x4();
			id.TexTransform = MathHelper::Identity4x4();
			id.MaterialIndex = 0;

			obj->mInstances.push_back(id);

			BoundingBox bb;
			bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

			// Bind Phy
			obj->Bounds.push_back(bb);
		}

		//obj->mPhysResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	//r->mData = obj;

	obj->mName = r->mName;
	obj->mFormat = "FBX";
	obj->mRenderType = renderType;

	obj->isOnlySubmesh = true;

	rotation.x = DirectX::XMConvertToRadians(rotation.x);
	rotation.y = DirectX::XMConvertToRadians(rotation.y);
	rotation.z = DirectX::XMConvertToRadians(rotation.z);

	DirectX::XMVECTOR mPositionVec = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR mRotationVec = DirectX::XMLoadFloat3(&rotation);
	DirectX::XMVECTOR mScaleVec = DirectX::XMLoadFloat3(&scale);

	DirectX::XMVECTOR mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);
	DirectX::XMVECTOR mOrientationVec;

	mOrientationVec.m128_f32[0] = 0.0f;
	mOrientationVec.m128_f32[1] = 0.0f;
	mOrientationVec.m128_f32[2] = 0.0f;
	mOrientationVec.m128_f32[3] = 1.0f;

	DirectX::XMMATRIX mWorldMat = DirectX::XMMatrixAffineTransformation(
		mScaleVec,
		mOrientationVec,
		mQuaternionVec,
		mPositionVec
	);

	r->mFormat = "FBX";

	// init Boundary(Collider) Box
	if (obj->InstanceCount > 0)
	{
		InstanceIterator = obj->mInstances.begin();
		BoundsIterator = obj->Bounds.begin();
		(*BoundsIterator).Center = position;
		(*BoundsIterator).Extents.x = scale.x;
		(*BoundsIterator).Extents.y = scale.y;
		(*BoundsIterator).Extents.z = scale.z;

		// Physx와 연동시키기 위해, Transform 초기화
		size_t resourceSize = obj->mInstances.size();
		for (UINT inst = 0; inst < resourceSize; inst++)
		{
			DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

			obj->mTranslate.push_back(
				Translate(
					mPositionVec,
					mRotationVec,
					mScaleVec,
					mWorldMat
				)
			);

			InstanceIterator++;
			//memcpy(obj->mPhysResources[inst].Position, &position, sizeof(float) * 3);
		}
	}

	// input subGeom
	std::vector<GeometryGenerator::MeshData> meshData;
	Geom.CreateFBXModel(
		meshData,
		(Path + "\\" + FileName),
		uvMode
	);

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = 1;
	obj->mDesc.resize(meshData.size());

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	// Push Submesh
	SubmeshGeometry mSubmesh;
	std::string submeshName = meshData[submeshIDX].mName;

	// 하나의 서브메쉬의 Vertex, Index 크기 저장
	mSubmesh.IndexSize = (UINT)meshData[submeshIDX].Indices32.size();
	mSubmesh.VertexSize = (UINT)meshData[submeshIDX].Vertices.size();

	// 현재 서브메쉬가 저장된 Offset 저장
	obj->mDesc[0].BaseVertexLocation = vertexOffset;
	obj->mDesc[0].StartIndexLocation = indexOffset;

	obj->mDesc[0].IndexSize = mSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = mSubmesh.VertexSize;

	// 서브메쉬 당 Cloth Physx, RigidBody를 적용 시킬 것인가에 대한 Boolean
	obj->isCloth.push_back(false);
	obj->isRigidBody.push_back(false);

	// Submesh 개수 증가
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs.push_back(mSubmesh);

	obj->mGeometry.DrawArgs[0].name = d3dUtil::getName(meshData[0].texPath);
	obj->mGeometry.DrawArgs[0].textureName = meshData[0].texPath;
	obj->mGeometry.DrawArgs[0].BaseVertexLocation = vertexOffset;
	obj->mGeometry.DrawArgs[0].StartIndexLocation = indexOffset;

	// 서브메쉬 정보를 스택에 저장
	obj->mGeometry.meshNames.push_back(submeshName);

	// 서브메쉬의 Vertex, Index를 스택에 Push
	obj->mVertices.resize(meshData[submeshIDX].Vertices.size());
	Vertex* v = obj->mVertices.data();
	for (size_t i = 0; i < (meshData[submeshIDX].Vertices.size()); ++i)
	{
		v[i].Pos = meshData[submeshIDX].Vertices[i].Position;
		v[i].Normal = meshData[submeshIDX].Vertices[i].Normal;
		v[i].Tangent = meshData[submeshIDX].Vertices[i].TangentU;
		v[i].TexC = meshData[submeshIDX].Vertices[i].TexC;
	}

	obj->mIndices.insert(
		obj->mIndices.end(),
		std::begin(meshData[submeshIDX].Indices32),
		std::end(meshData[submeshIDX].Indices32)
	);

	// Texture, Material 바인딩
	if (obj->mGeometry.DrawArgs[0].textureName != "")
	{
		Texture charTex;
		std::string TexPath;
		std::wstring wTexPath;

		charTex.Name = d3dUtil::getName(meshData[0].texPath);
		TexPath = d3dUtil::getDDSFormat(Path + "\\textures\\" + charTex.Name);
		wTexPath.assign(TexPath.begin(), TexPath.end());
		charTex.Filename = wTexPath;

		this->uploadTexture(charTex);
		this->uploadMaterial(charTex.Name, charTex.Name);

		this->BindMaterial(r, charTex.Name);
	}
	else
	{
		// 텍스쳐가 없거나 텍스쳐를 찾지 못하였을 경우 Default Texture, Materiai
		this->BindMaterial(r, "Default", "", "", "bricksTex");
	}

	vertexOffset += mSubmesh.VertexSize;
	indexOffset += mSubmesh.IndexSize;

	obj->mGeometry.IndexSizes.push_back((UINT)mSubmesh.IndexSize);

	obj->mGeometry.VertexBufferByteSize = mSubmesh.VertexSize * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize = mSubmesh.IndexSize * sizeof(std::uint32_t);
}

// Load Skinned Object
void BoxApp::CreateFBXSkinnedObject(
	std::string Name,
	std::string Path,
	std::string FileName,
	std::vector<std::string>& texturePath,
	RenderItem* r,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	AnimationClip& mAnimClips,
	bool uvMode,
	bool isDrawShadow,
	bool isDrawTexture
)
{
	// input subGeom
	std::vector<GeometryGenerator::MeshData> meshData;

	GeometryGenerator Geom;

	ObjectData* obj = mGameObjectDatas[r->mName];
	if (!obj)
	{
		obj = new ObjectData();

		for (UINT i = 0; i < r->InstanceCount; i++)
		{
			InstanceData id;
			id.World = MathHelper::Identity4x4();
			id.TexTransform = MathHelper::Identity4x4();
			id.MaterialIndex = 0;

			obj->mInstances.push_back(id);

			BoundingBox bb;
			bb.Center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			bb.Extents = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

			// Bind Phy
			obj->Bounds.push_back(bb);
		}

		//obj->mPhysResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	//r->mData = obj;

	obj->mName = r->mName;
	obj->mFormat = "FBX";
	obj->mRenderType = ObjectData::RenderType::_OPAQUE_SKINNED_RENDER_TYPE;

	rotation.x = DirectX::XMConvertToRadians(rotation.x);
	rotation.y = DirectX::XMConvertToRadians(rotation.y);
	rotation.z = DirectX::XMConvertToRadians(rotation.z);

	DirectX::XMVECTOR mPositionVec = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR mRotationVec = DirectX::XMLoadFloat3(&rotation);
	DirectX::XMVECTOR mScaleVec = DirectX::XMLoadFloat3(&scale);

	DirectX::XMVECTOR mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);
	DirectX::XMVECTOR mOrientationVec;

	mOrientationVec.m128_f32[0] = 0.0f;
	mOrientationVec.m128_f32[1] = 0.0f;
	mOrientationVec.m128_f32[2] = 0.0f;
	mOrientationVec.m128_f32[3] = 1.0f;

	DirectX::XMMATRIX mWorldMat = DirectX::XMMatrixAffineTransformation(
		mScaleVec,
		mOrientationVec,
		mQuaternionVec,
		mPositionVec
	);

	r->mFormat = "FBX";

	// init Boundary(Collider) Box
	InstanceIterator = obj->mInstances.begin();
	BoundsIterator = obj->Bounds.begin();
	(*BoundsIterator).Center = position;
	(*BoundsIterator).Extents = scale;

	size_t resourceSize = obj->mInstances.size();
	for (UINT inst = 0; inst < resourceSize; inst++)
	{
		DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

		obj->mTranslate.push_back(
			Translate(
				mPositionVec,
				mRotationVec,
				mScaleVec,
				mWorldMat
			)
		);

		InstanceIterator++;
		//memcpy(obj->mPhysResources[inst].Position, &position, sizeof(float) * 3);
	}

	AnimationClip* clip = new AnimationClip;
	clip->mClips.resize(mAnimClips.mClips.size());
	std::copy(mAnimClips.mClips.begin(), mAnimClips.mClips.end(), clip->mClips.begin());
	mAnimationClips[Name] = clip;

	//mMeshName = obj->mName;
	int res = Geom.CreateFBXSkinnedModel(
		meshData,
		obj->mName,
		(Path + "\\" + FileName),
		obj->animNameLists,
		obj->mStart,
		obj->mStop,
		obj->countOfFrame,
		obj->mAnimVertex,
		obj->mAnimVertexSize,
		mAnimClips
	);

	assert(!res && "(CreateFBXObject)Failed to create FBX Model on CreateFBXObject.");

	for (int animCount = 0; animCount < obj->countOfFrame.size(); animCount++)
	{
		obj->durationPerSec.push_back((float)obj->mStop[animCount].GetSecondDouble());
		obj->durationOfFrame.push_back(obj->durationPerSec[animCount] / obj->countOfFrame[animCount]);
	}

	obj->currentFrame = 0;
	obj->currentDelayPerSec = 0;
	
	obj->beginAnimIndex = 0;
	obj->endAnimIndex = (float)obj->mAnimVertex[0].size();

	std::vector<PxClothParticle> vertices;
	std::vector<PxU32> primitives;

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = (UINT)meshData.size();
	obj->mDesc.resize(meshData.size());

	size_t startV = obj->mVertices.size();

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	for (int subMeshCount = 0; subMeshCount < meshData.size(); ++subMeshCount)
	{
		SubmeshGeometry mSubmesh;
		std::string submeshName = Name + std::to_string(subMeshCount);

		mSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
		mSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

		mSubmesh.IndexSize = (UINT)meshData[subMeshCount].Indices32.size();
		mSubmesh.VertexSize = (UINT)meshData[subMeshCount].Vertices.size();

		obj->mDesc[subMeshCount].BaseVertexLocation = vertexOffset;
		obj->mDesc[subMeshCount].StartIndexLocation = indexOffset;

		obj->mDesc[subMeshCount].IndexSize = mSubmesh.IndexSize;
		obj->mDesc[subMeshCount].VertexSize = mSubmesh.VertexSize;

		obj->isCloth.push_back(false);
		obj->isRigidBody.push_back(false);

		r->SubmeshCount += 1;

		obj->mGeometry.subMeshCount += 1;
		obj->mGeometry.DrawArgs.push_back(mSubmesh);

		obj->mGeometry.DrawArgs[subMeshCount].name = d3dUtil::getName(meshData[subMeshCount].texPath);
		obj->mGeometry.DrawArgs[subMeshCount].textureName = meshData[subMeshCount].texPath;
		obj->mGeometry.DrawArgs[subMeshCount].BaseVertexLocation = vertexOffset;
		obj->mGeometry.DrawArgs[subMeshCount].StartIndexLocation = indexOffset;

		////////////////

		obj->mGeometry.meshNames.push_back(submeshName);
		texturePath.push_back(obj->mGeometry.DrawArgs[subMeshCount].textureName);

		//memcpy(r->mPhysResources[subMeshCount].Position, &position, sizeof(float) * 3);

		startV = (UINT)obj->mVertices.size();

		obj->mVertices.resize(startV + meshData[subMeshCount].Vertices.size());
		Vertex* v = obj->mVertices.data();

		for (size_t i = 0; i < (meshData[subMeshCount].Vertices.size()); ++i)
		{
			v[i + startV].Pos = meshData[subMeshCount].Vertices[i].Position;
			v[i + startV].Normal = meshData[subMeshCount].Vertices[i].Normal;
			v[i + startV].Tangent = meshData[subMeshCount].Vertices[i].TangentU;
			v[i + startV].TexC = meshData[subMeshCount].Vertices[i].TexC;

			// Bone Kit
			v[i + startV].BoneWeights.x = 0.0f;
			v[i + startV].BoneWeights.y = 0.0f;
			v[i + startV].BoneWeights.z = 0.0f;
			v[i + startV].BoneWeights.w = 0.0f;

			v[i + startV].BoneIndices.x = 0;
			v[i + startV].BoneIndices.y = 0;
			v[i + startV].BoneIndices.z = 0;
			v[i + startV].BoneIndices.w = 0;
		}

		obj->mIndices.insert(
			obj->mIndices.end(),
			std::begin(meshData[subMeshCount].Indices32),
			std::end(meshData[subMeshCount].Indices32)
		);

		// Texture, Material �ڵ� ����
		{
			if (obj->mGeometry.DrawArgs[subMeshCount].textureName != "")
			{
				Texture charTex;
				std::string TexPath;
				std::wstring wTexPath;

				charTex.Name = d3dUtil::getName(meshData[subMeshCount].texPath);
				TexPath = d3dUtil::getDDSFormat(Path + "\\textures\\" + charTex.Name);
				wTexPath.assign(TexPath.begin(), TexPath.end());
				charTex.Filename = wTexPath;

				this->uploadTexture(charTex);
				this->uploadMaterial(charTex.Name, charTex.Name);

				// ���ο� ���׸���, �ؽ��� �߰�
				this->BindMaterial(r, charTex.Name);
			}
			else
			{
				this->BindMaterial(r, "Default", "bricksTex");
			}

		}

		startV += meshData[subMeshCount].Vertices.size();
		// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
		vertexOffset += mSubmesh.VertexSize;
		indexOffset += mSubmesh.IndexSize;

		obj->mGeometry.IndexSize += (UINT)meshData[subMeshCount].Indices32.size();

		obj->mGeometry.VertexBufferByteSize += (UINT)meshData[subMeshCount].Vertices.size() * sizeof(Vertex);
		obj->mGeometry.IndexBufferByteSize += (UINT)meshData[subMeshCount].Indices32.size() * sizeof(std::uint32_t);
	}
}

void BoxApp::CreatePMXObject(
	std::string Name,
	std::string Path,
	std::string FileName,
	std::vector<std::string>& texturePath,
	RenderItem* r,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	bool isDrawShadow,
	bool isDrawTexture
)
{
	std::vector<PxClothParticle> vertices;
	std::vector<PxU32> primitives;

	ObjectData* obj = mGameObjectDatas[r->mName];

	obj->mName = r->mName;
	obj->mFormat = "PMX";
	obj->mRenderType = ObjectData::RenderType::_PMX_FORMAT_RENDER_TYPE;

	pmx::PmxModel* model = &obj->mModel;

	rotation.x = DirectX::XMConvertToRadians(rotation.x);
	rotation.y = DirectX::XMConvertToRadians(rotation.y);
	rotation.z = DirectX::XMConvertToRadians(rotation.z);

	DirectX::XMVECTOR mPositionVec = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR mRotationVec = DirectX::XMLoadFloat3(&rotation);
	DirectX::XMVECTOR mScaleVec = DirectX::XMLoadFloat3(&scale);

	DirectX::XMVECTOR mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);
	DirectX::XMVECTOR mOrientationVec;

	mOrientationVec.m128_f32[0] = 0.0f;
	mOrientationVec.m128_f32[1] = 0.0f;
	mOrientationVec.m128_f32[2] = 0.0f;
	mOrientationVec.m128_f32[3] = 1.0f;

	DirectX::XMMATRIX mWorldMat = DirectX::XMMatrixAffineTransformation(
		mScaleVec,
		mOrientationVec,
		mQuaternionVec,
		mPositionVec
	);

	// Read PMX
	model->Init();

	std::string _FullFilePath = Path + "\\" + FileName;
	std::ifstream stream(_FullFilePath.c_str(), std::ifstream::binary);
	model->Read(&stream);

	r->mFormat = "PMX";

	// init Boundary(Collider) Box
	InstanceIterator = obj->mInstances.begin();
	BoundsIterator = obj->Bounds.begin();
	(*BoundsIterator).Center = position;
	//obj->Bounds[0].Extents = scale;
	(*BoundsIterator).Extents = { 10.0f, 9.0f, 10.0f };

	size_t resourceSize = obj->mInstances.size();
	for (UINT inst = 0; inst < resourceSize; inst++)
	{
		DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

		obj->mTranslate.push_back(
			Translate(
				mPositionVec,
				mRotationVec,
				mScaleVec,
				mWorldMat
			)
		);

		InstanceIterator++;
	}

	// Load Submesh Count
	int _SubMeshCount = model->material_count;

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = _SubMeshCount;
	obj->mDesc.resize(_SubMeshCount);

	//////////////////////////////////////////////////////////////
	// 애니메이션 뼈대 버퍼를 로드
	//////////////////////////////////////////////////////////////
	std::ifstream animBuffer(std::string("resFile"), std::ios::in | std::ios::binary);

	int mAnimFrameCount = 0;
	animBuffer.read((char*)&mAnimFrameCount, sizeof(int));

	std::vector<std::vector<float*>> AssistCalcVar(mAnimFrameCount + 1);

	obj->mBoneMatrix.resize(mAnimFrameCount + 1);

	for (int i = 0; i < mAnimFrameCount; i++)
	{
		AssistCalcVar[i].resize(model->bone_count);
		obj->mBoneMatrix[i].resize(model->bone_count);
		for (int j = 0; j < model->bone_count; j++)
		{
			// Mat
			AssistCalcVar[i][j] = new float[8];
		}
	}

	obj->beginAnimIndex = 0;
	obj->currentFrame = 0;
	obj->currentDelayPerSec = 0;

	obj->endAnimIndex = (float)mAnimFrameCount;

	// Base Origin Bone SRT Matrix
	obj->mOriginRevMatrix.resize(model->bone_count);

	float* mBoneOriginPositionBuffer = NULL;
	float* mBonePositionBuffer = NULL;

	for (int i = 0; i < mAnimFrameCount; i++)
	{
		for (int j = 0; j < model->bone_count; j++)
		{
			animBuffer.read((char*)AssistCalcVar[i][j], sizeof(float) * 8);
		}
	}

	AnimationClip* clip = new AnimationClip;

	clip->appendClip("IDLE2", 15.0f, 90.0f);
	clip->appendClip("IDLE2toIDLE1", 91.0f, 131.0f);
	clip->appendClip("IDLE1", 132.0f, 232.0f);
	clip->appendClip("IDLE1toIDLE2", 233.0f, 273.0f);

	clip->appendClip("JUMP_BACK", 274.0f, 299.0f);
	clip->appendClip("JUMP_LEFT", 300.0f, 325.0f);
	clip->appendClip("JUMP_RIGHT", 326.0f, 351.0f);

	clip->appendClip("KNOCKED_BACKWARD", 352.0f, 393.0f);
	clip->appendClip("DOWN_BACK", 394.0f, 434.0f);
	clip->appendClip("RECOVER_FROM_KNOCKED_BACKWARD", 435.0f, 475.0f);

	clip->appendClip("KNOCKED_FORWARD", 476.0f, 504.0f);
	clip->appendClip("DOWN_FORWARD", 505.0f, 545.0f);
	clip->appendClip("RECOVER_FROM_KNOCKED_FORWARD", 546.0f, 578.0f);

	clip->appendClip("MAGIC_SHOT_STRIGHT", 666.0f, 701.0f);

	clip->appendClip("DODGE_BACKWARD", 885.0f, 920.0f);
	clip->appendClip("DODGE_TO_LEFT", 921.0f, 956.0f);
	clip->appendClip("DODGE_TO_RIGHT", 957.0f, 992.0f);
	clip->appendClip("DODGE_TO_BACK", 993.0f, 1028.0f);
	clip->appendClip("DODGE_TO_FRONT", 1029.0f, 1064.0f);

	clip->appendClip("EARTHQUAKE_SPELL", 1172.0f, 1207.0f);
	clip->appendClip("HIT_STRIGHT_DOWN", 1208.0f, 1243.0f);
	clip->appendClip("HIT_SWING_RIGHT", 1244.0f, 1279.0f);

	clip->appendClip("DYING_A", 2220.0f, 2260.0f);
	clip->appendClip("DYING_B", 1280.0f, 1345.0f);

	clip->appendClip("DRINKING_POTION", 1568.0f, 1623.0f);

	clip->appendClip("SPELL_CAST_1", 2082.0f, 2117.0f);
	clip->appendClip("SPELL_CAST_2", 2118.0f, 2178.0f);

	clip->appendClip("WALK", 2309.0f, 2344.0f);
	clip->appendClip("RUN", 2796.0f, 2831.0f);
	clip->appendClip("RUN_A", 2492.0f, 2536.0f);
	clip->appendClip("RUN_B", 2537.0f, 2581.0f);
	clip->appendClip("RUN_C", 2582.0f, 2626.0f);
	clip->appendClip("RUN_D", 2627.0f, 2671.0f);

	clip->appendClip("DIAGONAL_LEFT", 2677.0f, 2711.0f);
	clip->appendClip("DIAGONAL_RIGHT", 2717.0f, 2751.0f);
	clip->appendClip("STRIFE_LEFT", 3239.0f, 3273.0f);
	clip->appendClip("STRIFE_RIGHT", 3279.0f, 3313.0f);

	mAnimationClips[Name] = clip;

	//////////////////////////////////////
	// Morph
	//////////////////////////////////////

	pmx::PmxMorph* morph = NULL;
	pmx::MorphType type;
	pmx::PmxMorphVertexOffset* vertOff = NULL;

	for (int i = 0; i < model->morph_count; i++)
	{
		morph = &model->morphs[i];
		type = morph->morph_type;

		if (type == pmx::MorphType::Vertex)
		{
			struct ObjectData::_VERTEX_MORPH_DESCRIPTOR mMorph;

			mMorph.Name = morph->morph_name;
			mMorph.NickName = morph->morph_english_name;

			mMorph.mVertWeight = 0.0f;

			vertOff = morph->vertex_offsets.get();

			mMorph.mVertIndices.resize(morph->offset_count);
			mMorph.mVertOffset.resize(morph->offset_count);

			for (int j = 0; j < morph->offset_count; j++)
			{
				mMorph.mVertIndices[j] = vertOff[j].vertex_index;

				mMorph.mVertOffset[j][0] = vertOff[j].position_offset[0];
				mMorph.mVertOffset[j][1] = vertOff[j].position_offset[1];
				mMorph.mVertOffset[j][2] = vertOff[j].position_offset[2];
			}

			obj->mMorph.push_back(mMorph);
		}
	}

	std::shared_ptr<ImGuiFrameComponent> mMorphFrame = pushFrame("Morph");

	obj->mMorphDirty.resize(obj->mMorph.size());
	for (int i = 0; i < obj->mMorph.size(); i++)
	{
		obj->mMorph[i].mVertWeight = 0.0f;
		obj->mMorphDirty[i] = 0;

		std::string name = "Morph" + std::to_string(i);
		std::string hint(obj->mMorph[i].NickName.begin(), obj->mMorph[i].NickName.end());

		mMorphFrame->pushSliderFloatComponent(obj->mMorph[i].mVertWeight, name, hint);

		mMorphFrame->mComponents[i]->bindFence(&obj->mMorphDirty.data()[i]);
	}

	//for (int i = 0; i < obj->mMorph.size(); i++)
	//{
	//	std::string name = "Morph" + std::to_string(i);

	//	mMorphFrame->mComponents[name]->bindFence(&obj->mMorphDirty.data()[i]);
	//}

	//////////////////////////////////////

	DirectX::XMVECTOR S;
	DirectX::XMVECTOR T;
	DirectX::XMVECTOR O;
	DirectX::XMVECTOR Q;
	DirectX::XMMATRIX M, OM;

	S.m128_f32[0] = 1.0f;
	S.m128_f32[1] = 1.0f;
	S.m128_f32[2] = 1.0f;
	S.m128_f32[3] = 1.0f;

	O.m128_f32[0] = 0.0f;
	O.m128_f32[1] = 0.0f;
	O.m128_f32[2] = 0.0f;
	O.m128_f32[3] = 1.0f;

	Q.m128_f32[0] = 0.0f;
	Q.m128_f32[1] = 0.0f;
	Q.m128_f32[2] = 0.0f;
	Q.m128_f32[3] = 1.0f;

	// ����� ����
	DirectX::XMVECTOR det;
	for (int i = 0; i < model->bone_count; i++)
	{
		mBonePositionBuffer = AssistCalcVar[0][i];

		T.m128_f32[0] = mBonePositionBuffer[0];
		T.m128_f32[1] = mBonePositionBuffer[1];
		T.m128_f32[2] = mBonePositionBuffer[2];
		T.m128_f32[3] = 1.0f;

		Q.m128_f32[0] = mBonePositionBuffer[4];
		Q.m128_f32[1] = mBonePositionBuffer[5];
		Q.m128_f32[2] = mBonePositionBuffer[6];
		Q.m128_f32[3] = mBonePositionBuffer[7];

		OM = XMMatrixAffineTransformation(S, O, Q, T);
		det = XMMatrixDeterminant(OM);
		M = DirectX::XMMatrixInverse(&det, OM);
		M = DirectX::XMMatrixTranspose(M);

		DirectX::XMStoreFloat4x4(&obj->mOriginRevMatrix[i], M);
	}

	// �ִϸ��̼� ����
	for (int i = 0; i < mAnimFrameCount; i++)
	{
		for (int j = 0; j < model->bone_count; j++)
		{
			mBoneOriginPositionBuffer = AssistCalcVar[0][j];
			mBonePositionBuffer = AssistCalcVar[i][j];

			T.m128_f32[0] = mBonePositionBuffer[0];
			T.m128_f32[1] = mBonePositionBuffer[1];
			T.m128_f32[2] = mBonePositionBuffer[2];
			T.m128_f32[3] = 1.0f;

			O.m128_f32[0] = mBoneOriginPositionBuffer[4];
			O.m128_f32[1] = mBoneOriginPositionBuffer[5];
			O.m128_f32[2] = mBoneOriginPositionBuffer[6];
			O.m128_f32[3] = mBoneOriginPositionBuffer[7];

			Q.m128_f32[0] = mBonePositionBuffer[4];
			Q.m128_f32[1] = mBonePositionBuffer[5];
			Q.m128_f32[2] = mBonePositionBuffer[6];
			Q.m128_f32[3] = mBonePositionBuffer[7];

			M = XMMatrixAffineTransformation(S, O, Q, T);
			M = DirectX::XMMatrixTranspose(M);

			DirectX::XMStoreFloat4x4(&obj->mBoneMatrix[i][j], M);
		}
	}

	// Delete Assist Var to Calculated Matrix
	for (int i = 0; i < AssistCalcVar.size(); i++)
	{
		for (int j = 0; j < AssistCalcVar[i].size(); j++)
		{
			delete(AssistCalcVar[i][j]);
		}
		AssistCalcVar[i].clear();
	}
	AssistCalcVar.clear();

	animBuffer.close();

	//////////////////////////////////////////////////////////////
	// Model�� Cloth Weight�� Submesh ������ isCloth ������ ����
	//////////////////////////////////////////////////////////////

	//std::vector<std::wstring> mWeightBonesRoot;
	//mWeightBonesRoot.push_back(std::wstring(L"WaistString1_2"));
	//mWeightBonesRoot.push_back(std::wstring(L"WaistString2_2")); 
	//mWeightBonesRoot.push_back(std::wstring(L"UpperBody2"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_1_1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_2_1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_1_2"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_2_2"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_3_2"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_4_2"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_5_2"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_6_1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_7_1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_8_1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_9_1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_10_1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_11_1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Skirt_12_1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Front_Hair1"));
	//mWeightBonesRoot.push_back(std::wstring(L"Front_Hair2"));
	//mWeightBonesRoot.push_back(std::wstring(L"Front_Hair3"));
	//mWeightBonesRoot.push_back(std::wstring(L"SideHair1-1.L"));
	//mWeightBonesRoot.push_back(std::wstring(L"Sideburns1.L"));
	//mWeightBonesRoot.push_back(std::wstring(L"SideHair2.L"));
	//mWeightBonesRoot.push_back(std::wstring(L"SideHair3-1.L"));
	//mWeightBonesRoot.push_back(std::wstring(L"SideHair1-1.R"));
	//mWeightBonesRoot.push_back(std::wstring(L"Sideburns1.R"));
	//mWeightBonesRoot.push_back(std::wstring(L"SideHair2.R"));
	//mWeightBonesRoot.push_back(std::wstring(L"SideHair3-1.R"));

	//std::vector<std::wstring> mWeightBones;
	//mWeightBones.push_back(std::wstring(L"WaistString1_3"));
	//mWeightBones.push_back(std::wstring(L"WaistString1_4"));
	//mWeightBones.push_back(std::wstring(L"WaistString1_5"));
	//mWeightBones.push_back(std::wstring(L"WaistString1_6"));
	//mWeightBones.push_back(std::wstring(L"WaistString1_7"));
	//mWeightBones.push_back(std::wstring(L"WaistString1_8"));
	//mWeightBones.push_back(std::wstring(L"WaistString2_3"));
	//mWeightBones.push_back(std::wstring(L"WaistString2_4"));
	//mWeightBones.push_back(std::wstring(L"WaistString2_5"));
	//mWeightBones.push_back(std::wstring(L"WaistString2_6"));
	//mWeightBones.push_back(std::wstring(L"WaistString2_7"));
	//mWeightBones.push_back(std::wstring(L"WaistString2_8"));
	//mWeightBones.push_back(std::wstring(L"Breast_Root"));
	//mWeightBones.push_back(std::wstring(L"Breast_Base.L"));
	//mWeightBones.push_back(std::wstring(L"Breast_Base.R"));
	//mWeightBones.push_back(std::wstring(L"Breast.L"));
	//mWeightBones.push_back(std::wstring(L"Breast.R"));
	//mWeightBones.push_back(std::wstring(L"Breast_Support.L"));
	//mWeightBones.push_back(std::wstring(L"Breast_Support.R"));
	//mWeightBones.push_back(std::wstring(L"Skirt_1_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_1_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_1_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_2_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_2_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_2_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_3_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_3_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_3_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_4_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_4_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_4_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_5_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_5_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_5_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_6_2"));
	//mWeightBones.push_back(std::wstring(L"Skirt_6_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_7_2"));
	//mWeightBones.push_back(std::wstring(L"Skirt_7_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_8_2"));
	//mWeightBones.push_back(std::wstring(L"Skirt_8_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_8_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_8_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_9_2"));
	//mWeightBones.push_back(std::wstring(L"Skirt_9_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_9_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_9_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_9_6"));
	//mWeightBones.push_back(std::wstring(L"Skirt_9_7"));
	//mWeightBones.push_back(std::wstring(L"Skirt_10_2"));
	//mWeightBones.push_back(std::wstring(L"Skirt_10_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_10_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_10_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_10_6"));
	//mWeightBones.push_back(std::wstring(L"Skirt_10_7"));
	//mWeightBones.push_back(std::wstring(L"Skirt_11_2"));
	//mWeightBones.push_back(std::wstring(L"Skirt_11_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_11_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_11_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_11_6"));
	//mWeightBones.push_back(std::wstring(L"Skirt_11_7"));
	//mWeightBones.push_back(std::wstring(L"Skirt_12_2"));
	//mWeightBones.push_back(std::wstring(L"Skirt_12_3"));
	//mWeightBones.push_back(std::wstring(L"Skirt_12_4"));
	//mWeightBones.push_back(std::wstring(L"Skirt_12_5"));
	//mWeightBones.push_back(std::wstring(L"Skirt_12_6"));
	//mWeightBones.push_back(std::wstring(L"SideHair1-2.L"));
	//mWeightBones.push_back(std::wstring(L"SideHair1-3.L"));
	//mWeightBones.push_back(std::wstring(L"Sideburns2.L"));
	//mWeightBones.push_back(std::wstring(L"Sideburns3.L"));
	//mWeightBones.push_back(std::wstring(L"Sideburns4.L"));
	//mWeightBones.push_back(std::wstring(L"SideHair3-2.L"));
	//mWeightBones.push_back(std::wstring(L"SideHair1-2.R"));
	//mWeightBones.push_back(std::wstring(L"SideHair1-3.R"));
	//mWeightBones.push_back(std::wstring(L"Sideburns2.R"));
	//mWeightBones.push_back(std::wstring(L"Sideburns3.R"));
	//mWeightBones.push_back(std::wstring(L"Sideburns4.R"));
	//mWeightBones.push_back(std::wstring(L"SideHair3-2.R"));


	//std::ifstream checkExistsHello(std::string("Weights").c_str());
	//if (checkExistsHello.good())
	//	std::remove("Weights");

	//std::ofstream outFile(std::string("Weights"), std::ios::out | std::ios::binary);
	//if (!outFile.is_open())
	//	throw std::runtime_error("");

	//// Transformation Model By Param (Pos, Scale, Rot)
	//float* vOffset = NULL;

	//mGameObjectDatas[r->mName]->mClothWeights.resize(model->vertex_count);

	//bool hasZero(false), hasOne(false);

	//float dWeight = 0.0f;
	//pmx::PmxVertexSkinningBDEF1* bdef1 = NULL;
	//pmx::PmxVertexSkinningBDEF2* bdef2 = NULL;
	//pmx::PmxVertexSkinningBDEF4* bdef4 = NULL;

	//for (int i = 0; i < model->vertex_count; i++)
	//{
	//	// �켱 ���ؽ��� �ε����� ����
	//	outFile.write((char*)&i, sizeof(int));

	//	// ���� ����Ʈ�� ������ ��
	//	dWeight = 0.0f;

	//	if (model->vertices[i].skinning_type == pmx::PmxVertexSkinningType::BDEF1)
	//	{
	//		bdef1 = (pmx::PmxVertexSkinningBDEF1*)model->vertices[i].skinning.get();

	//		std::wstring name1 = model->bones[bdef1->bone_index].bone_english_name;

	//		for (int j = 0; j < mWeightBonesRoot.size(); j++)
	//		{
	//			if (mWeightBonesRoot[j] == name1)
	//			{
	//				dWeight = 0.0f;
	//				break;
	//			}
	//		}

	//		for (int j = 0; j < mWeightBones.size(); j++)
	//		{
	//			if (mWeightBones[j] == name1)
	//			{
	//				dWeight = 1.0f;
	//				break;
	//			}
	//		}
	//	}
	//	else if (model->vertices[i].skinning_type == pmx::PmxVertexSkinningType::BDEF2)
	//	{
	//		bdef2 = (pmx::PmxVertexSkinningBDEF2*)model->vertices[i].skinning.get();

	//		std::wstring name1 = model->bones[bdef2->bone_index1].bone_english_name;
	//		std::wstring name2 = model->bones[bdef2->bone_index2].bone_english_name;

	//		for (int j = 0; j < mWeightBonesRoot.size(); j++)
	//		{
	//			if (mWeightBonesRoot[j] == name1 ||
	//				mWeightBonesRoot[j] == name2)
	//			{
	//				dWeight = 0.0f;
	//				break;
	//			}
	//		}

	//		for (int j = 0; j < mWeightBones.size(); j++)
	//		{
	//			if (mWeightBones[j] == name1)
	//			{
	//				dWeight = bdef2->bone_weight;
	//				break;
	//			}
	//			else if (mWeightBones[j] == name2)
	//			{
	//				dWeight = 1.0f - bdef2->bone_weight;
	//				break;
	//			}
	//		}
	//	}
	//	else if (model->vertices[i].skinning_type == pmx::PmxVertexSkinningType::BDEF4)
	//	{
	//		bdef4 = (pmx::PmxVertexSkinningBDEF4*)model->vertices[i].skinning.get();

	//		std::wstring name1 = model->bones[bdef4->bone_index1].bone_english_name;
	//		std::wstring name2 = model->bones[bdef4->bone_index2].bone_english_name;
	//		std::wstring name3 = model->bones[bdef4->bone_index3].bone_english_name;
	//		std::wstring name4 = model->bones[bdef4->bone_index4].bone_english_name;

	//		for (int j = 0; j < mWeightBonesRoot.size(); j++)
	//		{
	//			if (mWeightBonesRoot[j] == name1 ||
	//				mWeightBonesRoot[j] == name2 ||
	//				mWeightBonesRoot[j] == name3 ||
	//				mWeightBonesRoot[j] == name4)
	//			{
	//				dWeight = 0.0f;
	//				break;
	//			}
	//		}

	//		for (int j = 0; j < mWeightBones.size(); j++)
	//		{
	//			if (mWeightBones[j] == name1)
	//			{
	//				dWeight = bdef4->bone_weight1;
	//				break;
	//			}
	//			else if (mWeightBones[j] == name2)
	//			{
	//				dWeight = bdef4->bone_weight2;
	//				break;
	//			}
	//			else if (mWeightBones[j] == name3)
	//			{
	//				dWeight = bdef4->bone_weight3;
	//				break;
	//			}
	//			else if (mWeightBones[j] == name4)
	//			{
	//				dWeight = bdef4->bone_weight4;
	//				break;
	//			}
	//		}
	//	}

	//	// ����Ʈ ����
	//	outFile.write((char*)&dWeight, sizeof(float));
	//}

	//int delimiter = -1;
	//outFile.write((char*)&delimiter, sizeof(int));

	//std::vector<std::string> isClothSubmeshNames;
	//bool clothValue;
	//isClothSubmeshNames.push_back(std::string("Hair"));
	//isClothSubmeshNames.push_back(std::string("FrontCloth"));
	//isClothSubmeshNames.push_back(std::string("FlontCloth_Metal"));
	//isClothSubmeshNames.push_back(std::string("Cloth"));
	//isClothSubmeshNames.push_back(std::string("Cloth_Blue"));
	//isClothSubmeshNames.push_back(std::string("BodySuit"));
	////isClothSubmeshNames.push_back(std::string("Cloth_Metal"));
	////isClothSubmeshNames.push_back(std::string("Cloth_Jwel"));
	//isClothSubmeshNames.push_back(std::string("WaistString"));
	//isClothSubmeshNames.push_back(std::string("WaistString_Metal"));

	//// Cloth Submesh List
	//for (int i = 0; i < model->material_count; i++)
	//{
	//	clothValue = false;

	//	std::wstring wname = model->materials[i].material_english_name;
	//	std::string name;
	//	name.assign(wname.begin(), wname.end());

	//	for (int j = 0; j < isClothSubmeshNames.size(); j++)
	//	{
	//		if (name == isClothSubmeshNames[j])
	//		{
	//			clothValue = true;
	//			break;
	//		}
	//	}

	//	outFile.write((char*)&clothValue, sizeof(bool));
	//}

	//outFile.close();

	//throw std::runtime_error("");

	////////////////////////////////////////////////////////////

	std::ifstream inFile(std::string("Weights"), std::ios::in | std::ios::binary);
	if (!inFile.is_open())
		throw std::runtime_error("");

	int vertIDX;
	float vertWeight;
	bool mIsCloth;

	obj->mClothWeights.resize(model->vertex_count);
	obj->isCloth.resize(model->material_count);
	while (true)
	{
		inFile.read((char*)&vertIDX, sizeof(int));
		if (vertIDX == -1)	break;
		inFile.read((char*)&vertWeight, sizeof(float));

		/*mGameObjectDatas[r->mName]->mClothWeights[vertIDX] = vertWeight;*/
		if (vertWeight > 0.6f)
			obj->mClothWeights[vertIDX] = 0.1f;
		else
			obj->mClothWeights[vertIDX] = vertWeight * 0.02f;
	}

	int count = 0;
	while (count < model->material_count)
	{
		inFile.read((char*)&mIsCloth, sizeof(bool));
		obj->isCloth[count++] = mIsCloth;
	}

	inFile.close();

	//////////////////////////////////////////////////////////////

	// Load Texture Path
	for (int i = 0; i < model->texture_count; i++)
	{
		int textureSize = (int)model->textures[i].size() * 2;

		Texture charTex;
		std::string rawTextureName = std::string((char*)model->textures[i].c_str(), textureSize);
		std::string textureName;
		std::wstring wTextureName;

		for (int eraseEmptyIndex = 0; eraseEmptyIndex < rawTextureName.size(); eraseEmptyIndex += 2) {
			textureName.append(&rawTextureName.at(eraseEmptyIndex));
		}

		charTex.Name = d3dUtil::getName(textureName);
		textureName = Path + "\\" + d3dUtil::getDDSFormat(textureName);
		wTextureName.assign(textureName.begin(), textureName.end());
		charTex.Filename = wTextureName;

		this->uploadTexture(charTex);
		this->uploadMaterial(charTex.Name, charTex.Name);

		texturePath.push_back(textureName);
	}

	int _ModelIndexOffset = 0;

	int diffuseTextureIDX = -1;
	int sphereTextureIDX = -1;
	int toonTextureIDX = -1;

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	double pTime;
	pTime = FbxTime::GetFrameRate(FbxTime::EMode::eFrames30);
	pTime = 1.0 / pTime;

	obj->durationPerSec.push_back(mAnimFrameCount);
	obj->durationOfFrame.push_back((float)pTime);

	////
	UINT32 mCeil = 0;
	UINT32 mIDXAcculation = 0;

	obj->vertBySubmesh.resize(model->material_count);
	for (int i = 0; i < model->material_count; i++)
	{
		mCeil = mIDXAcculation + model->materials[i].index_count;
		for (UINT j = mIDXAcculation; j < mCeil; j++)
		{
			obj->vertBySubmesh[i].insert(model->indices[j]);
		}

		mIDXAcculation += model->materials[i].index_count;
	}

	////
	obj->isRigidBody.resize(_SubMeshCount);
	for (int subMeshIDX = 0; subMeshIDX < _SubMeshCount; ++subMeshIDX)
	{
		SubmeshGeometry mSubmesh;
		std::string submeshName = Name + std::to_string(subMeshIDX);

		// �� ���׸����� �ؽ��� �ε��� �ε�
		diffuseTextureIDX = model->materials[subMeshIDX].diffuse_texture_index;
		sphereTextureIDX = model->materials[subMeshIDX].sphere_texture_index;
		toonTextureIDX = model->materials[subMeshIDX].toon_texture_index;

		// vertex, index�� ������ �ε�
		// Draw���� ��� Vertices�� �ѹ��� DescriptorSet�� VBV�� ���ε� �� �� (SubMesh ������ ���ε� X)
		mSubmesh.BaseVertexLocation = (UINT)vertexOffset;
		mSubmesh.StartIndexLocation = (UINT)indexOffset;

		// �� submesh�� ���ε� �� ���ؽ�, �ε��� ����
		mSubmesh.VertexSize = (UINT)obj->vertBySubmesh[subMeshIDX].size();
		mSubmesh.IndexSize = (UINT)model->materials[subMeshIDX].index_count;

		std::string matName;
		matName.assign(
			model->materials[subMeshIDX].material_english_name.begin(),
			model->materials[subMeshIDX].material_english_name.end()
		);
		if (matName.find("_Rigid_Body") != std::string::npos) {
			obj->isRigidBody[subMeshIDX] = true;
		}
		else {
			obj->isRigidBody[subMeshIDX] = false;
		}

		obj->mDesc[subMeshIDX].BaseVertexLocation = mSubmesh.BaseVertexLocation;
		obj->mDesc[subMeshIDX].StartIndexLocation = mSubmesh.StartIndexLocation;

		obj->mDesc[subMeshIDX].VertexSize = mSubmesh.VertexSize;
		obj->mDesc[subMeshIDX].IndexSize = mSubmesh.IndexSize;

		// Submesh ����
		r->SubmeshCount += 1;

		obj->mGeometry.subMeshCount += 1;
		obj->mGeometry.DrawArgs.push_back(mSubmesh);

		if (diffuseTextureIDX >= 0) {
			obj->mGeometry.DrawArgs[subMeshIDX].name = d3dUtil::getName(texturePath[diffuseTextureIDX]);
			obj->mGeometry.DrawArgs[subMeshIDX].textureName = texturePath[diffuseTextureIDX];
		}
		else {
			obj->mGeometry.DrawArgs[subMeshIDX].name = "";
			obj->mGeometry.DrawArgs[subMeshIDX].textureName = "";
		}

		obj->mGeometry.DrawArgs[subMeshIDX].BaseVertexLocation = 0;
		obj->mGeometry.DrawArgs[subMeshIDX].StartIndexLocation = indexOffset;

		obj->mGeometry.meshNames.push_back(submeshName);

		// Bind Texture, Material 
		{
			if (obj->mGeometry.DrawArgs[subMeshIDX].textureName != "")
			{
				std::string texName = d3dUtil::getName(texturePath[diffuseTextureIDX]);
				this->BindMaterial(r, texName, false);
			}
			else
			{
				this->BindMaterial(r, "Default", "bricksTex", false);
			}
		}

		_ModelIndexOffset += model->materials[subMeshIDX].index_count;

		vertexOffset += mSubmesh.VertexSize;
		indexOffset += mSubmesh.IndexSize;
	}

	obj->mGeometry.IndexSize += (UINT)model->index_count;

	obj->mGeometry.VertexBufferByteSize = model->vertex_count * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize = model->index_count * sizeof(uint32_t);
}

void BoxApp::CreateDebugBoxObject (
	_In_ RenderItem* r
	) 
{
	ObjectData* obj = mGameObjectDatas[r->mName];

	obj->isDebugBox = true;

	obj->mDebugBoxData = std::make_unique<ObjectData>();

	obj->mDebugBoxData->mFormat = "Debug";
	obj->mDebugBoxData->mRenderType = ObjectData::RenderType::_DEBUG_BOX_TYPE;
	obj->mDebugBoxData->isDrawShadow = false;
	obj->mDebugBoxData->isDrawTexture = false;

	// input subGeom
	GeometryGenerator Geom;
	GeometryGenerator::MeshData Box;

	BoundsIterator = obj->Bounds.begin();
	Box = Geom.CreateBox(
		(*BoundsIterator).Extents.x * 2.0f,
		(*BoundsIterator).Extents.y,
		(*BoundsIterator).Extents.z * 2.0f,
		0
	);

	obj->mDebugBoxData->InstanceCount += 1;

	obj->mDebugBoxData->SubmeshCount += 1;
	obj->mDebugBoxData->mDesc.resize(obj->mDebugBoxData->SubmeshCount);

	obj->mDebugBoxData->isCloth.push_back(false);
	obj->mDebugBoxData->isRigidBody.push_back(false);

	SubmeshGeometry mSubmesh;
	mSubmesh.StartIndexLocation = (UINT)obj->mDebugBoxData->mGeometry.IndexBufferByteSize;
	mSubmesh.BaseVertexLocation = (UINT)obj->mDebugBoxData->mGeometry.VertexBufferByteSize;

	mSubmesh.IndexSize = (UINT)Box.Indices32.size();
	mSubmesh.VertexSize = (UINT)Box.Vertices.size();

	obj->mDebugBoxData->mDesc[0].StartIndexLocation = mSubmesh.StartIndexLocation;
	obj->mDebugBoxData->mDesc[0].BaseVertexLocation = mSubmesh.BaseVertexLocation;

	obj->mDebugBoxData->mDesc[0].IndexSize = mSubmesh.IndexSize;
	obj->mDebugBoxData->mDesc[0].VertexSize = mSubmesh.VertexSize;

	// Submesh
	std::string submeshName = "_DEBUG_" + r->mName;

	SubmeshGeometry mDebugSubmesh;
	mDebugSubmesh.StartIndexLocation	= mSubmesh.StartIndexLocation;
	mDebugSubmesh.BaseVertexLocation	= mSubmesh.BaseVertexLocation;
	mDebugSubmesh.IndexSize				= mSubmesh.IndexSize;
	mDebugSubmesh.VertexSize			= mSubmesh.VertexSize;
	mDebugSubmesh.textureName			= "";
	obj->mDebugBoxData->mGeometry.DrawArgs.push_back(mDebugSubmesh);
	obj->mDebugBoxData->mGeometry.meshNames.push_back(submeshName);

	size_t startV = obj->mDebugBoxData->mVertices.size();
	XMVECTOR posV;

	obj->mDebugBoxData->mVertices.resize(startV + Box.Vertices.size());

	Vertex* v = obj->mDebugBoxData->mVertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->mDebugBoxData->mIndices.insert(
		obj->mDebugBoxData->mIndices.end(),
		std::begin(Box.Indices32),
		std::end(Box.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	obj->mDebugBoxData->mGeometry.IndexSize += (UINT)Box.Indices32.size();

	obj->mDebugBoxData->mGeometry.VertexBufferByteSize += (UINT)Box.Vertices.size() * sizeof(Vertex);
	obj->mDebugBoxData->mGeometry.IndexBufferByteSize += (UINT)Box.Indices32.size() * sizeof(std::uint32_t);
}

// Load Skinned Object
void BoxApp::ExtractAnimBones(
	std::string Path,
	std::string FileName,
	std::string targetPath,
	std::string targetFileName,
	RenderItem* r
)
{
	GeometryGenerator Geom;

	r->mFormat = "FBX";

	// SRT
	DirectX::XMFLOAT4X4 SRT = MathHelper::Identity4x4();
	// SRT Matrix
	DirectX::XMMATRIX SRTM;

	SRTM = XMLoadFloat4x4(&SRT);

	int res = Geom.ExtractedAnimationBone
	(
		(Path + "\\" + FileName),
		(targetPath + "\\" + targetFileName),
		mGameObjectDatas[r->mName]->animNameLists,
		mGameObjectDatas[r->mName]->mStart,
		mGameObjectDatas[r->mName]->mStop,
		mGameObjectDatas[r->mName]->countOfFrame
	);

	assert(!res &&  "(CreateFBXObject)Failed to extract FBX Data on ExtractedAnimationBone.");
}

// Load Skinned Object
void BoxApp::SetBoundBoxScale(
	_Out_ RenderItem* r,
	_In_ XMFLOAT3	scale
)
{
	BoundsIterator = mGameObjectDatas[r->mName]->Bounds.begin();
	for (UINT inst = 0; inst < r->InstanceCount; inst++)
	{
		(*BoundsIterator).Extents = scale;
		BoundsIterator++;
	}
}

ObjectData* BoxApp::GetData(
	_In_ std::string mName
)
{
	return mGameObjectDatas[mName];
}

void BoxApp::uploadTexture(_In_ Texture& tex, _In_ bool isSky) {
	try {
		for (int i = 0; i < mTextureList.size(); i++)
		{
			if (mTextureList[i] == tex.Name)
			{
				return;
			}
		}

		int width, height;

		// Define Texture
		{
			ThrowIfFailed(
				DirectX::CreateDDSTextureFromFile12(
					md3dDevice.Get(),
					mCommandList.Get(),
					tex.Filename.c_str(),
					tex.Resource,
					tex.UploadHeap,
					&width, 
					&height
				)
			);

			tex.isCube = isSky;

			// Push New TextureName, TextureInfo
			mTextureList.push_back(tex.Name);
			mTextures.push_back(tex);
			mTextures[mTextures.size() - 1].isCube = isSky;
		}
	}
	catch (std::exception e) {
		MessageBoxA(nullptr, (LPCSTR)L"Failed to Load DDS File!!", (LPCSTR)L"Error", MB_OK);
	}
}

void BoxApp::uploadTexture(_In_ Texture& tex, _In_ int& width, _In_ int& height, _In_ bool isSky) {
	try {
		for (int i = 0; i < mTextureList.size(); i++)
		{
			if (mTextureList[i] == tex.Name)
			{
				return;
			}
		}

		// Define Texture
		{
			ThrowIfFailed(
				DirectX::CreateDDSTextureFromFile12(
					md3dDevice.Get(),
					mCommandList.Get(),
					tex.Filename.c_str(),
					tex.Resource,
					tex.UploadHeap,
					&width,
					&height
				)
			);

			tex.isCube = isSky;

			// Push New TextureName, TextureInfo
			mTextureList.push_back(tex.Name);
			mTextures.push_back(tex);
			mTextures[mTextures.size() - 1].isCube = isSky;
		}
	}
	catch (std::exception e) {
		MessageBoxA(nullptr, (LPCSTR)L"Failed to Load DDS File!!", (LPCSTR)L"Error", MB_OK);
	}
}

void BoxApp::uploadMaterial(_In_ std::string name, _In_ bool isSkyTexture) {
	std::vector<Material>::iterator& matIDX = mMaterials.begin();
	std::vector<Material>::iterator& matEnd = mMaterials.end();
	// When The Material that is getting same name with new Material is existing, creating is will be skipped.
	for (; matIDX != matEnd; matIDX++)
	{
		if (matIDX->Name == name)
			return;
	}

	// Define Material
	{
		Material mat;
		mat.Name = name.c_str();
		mat.MatCBIndex = (int)mMaterials.size();
		mat.DiffuseSrvHeapIndex = -1;
		mat.NormalSrvHeapIndex = -1;
		mat.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mat.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
		mat.Roughness = 0.1f;
		mat.isSkyTexture = isSkyTexture;

		mMaterials.push_back(mat);
	}
}

void BoxApp::uploadMaterial(
	_In_ std::string matName, 
	_In_ std::string texName, 
	_In_ bool isSkyTexture
)
{
	std::vector<Material>::iterator it = mMaterials.begin();
	std::vector<Material>::iterator itEnd = mMaterials.end();
	for (; it != itEnd; it++)
	{
		if (it->Name == matName)
			return;
	}

	// Define Material

	Material mat;
	mat.Name = matName.c_str();
	mat.MatCBIndex = (int)mMaterials.size();
	mat.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mat.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	mat.Roughness = 0.1f;
	mat.isSkyTexture = isSkyTexture;

	// Define Texture

	bool isFound = false;
	int diffuseIDX = 0;

	for (int idx = 0; idx < mTextureList.size(); idx++) {
		if (mTextureList[idx] == texName) {
			isFound = true;
			break;
		}

		diffuseIDX++;
	}

	assert(isFound && "Can't find Texture which same with NAME!!");

	// Fill in the DiffuseSrvHeapIndex to Diffuse Texture Index.
	mat.DiffuseSrvHeapIndex = diffuseIDX;
	mat.NormalSrvHeapIndex	= diffuseIDX;
	mat.MaskSrvHeapIndex	= 0;
	mat.NoiseSrvHeapIndex	= 0;

	mMaterials.push_back(mat);
}

void BoxApp::uploadMaterial(
	_In_ std::string matName, 
	_In_ std::string tex_Diffuse_Name, 
	_In_ std::string tex_Mask_Name,
	_In_ std::string tex_Noise_Name,
	_In_ bool isSkyTexture,
	_In_ DirectX::XMFLOAT2 diffuseCount,
	_In_ DirectX::XMFLOAT4 diffuseAlbedo,
	_In_ DirectX::XMFLOAT3 fresnelR0,
	_In_ float roughness
)
{
	std::vector<Material>::iterator it = mMaterials.begin();
	std::vector<Material>::iterator itEnd = mMaterials.end();
	for (; it != itEnd; it++)
	{
		if (it->Name == matName)
			return;
	}

	// Define Material
	Material mat;
	mat.Name			= matName.c_str();
	mat.MatCBIndex		= (int)mMaterials.size();
	mat.DiffuseAlbedo	= diffuseAlbedo;
	mat.FresnelR0		= fresnelR0;
	mat.Roughness		= roughness;
	mat.DiffuseCount	= diffuseCount;
	mat.isSkyTexture	= isSkyTexture;

	// Define Texture
	bool isFound		= false;
	int diffuseIDX		= -1;
	int MaskIDX			= -1;
	int NoiseIDX		= -1;

	for (int idx = 0; idx < mTextureList.size(); idx++) {
		if (mTextureList[idx] == tex_Diffuse_Name)
			diffuseIDX	= idx;
		if (mTextureList[idx] == tex_Mask_Name)
			MaskIDX		= idx;
		if (mTextureList[idx] == tex_Noise_Name)
			NoiseIDX	= idx;
	}

	if (
		diffuseIDX	== -1 ||
		MaskIDX		== -1 ||
		NoiseIDX	== -1
		)
	{
		throw std::runtime_error("이미지 파일을 찾지 못하였습니다.");
	}

	// Fill in the DiffuseSrvHeapIndex to Diffuse Texture Index.
	mat.DiffuseSrvHeapIndex = diffuseIDX;
	mat.NormalSrvHeapIndex	= diffuseIDX;
	mat.MaskSrvHeapIndex	= MaskIDX;
	mat.NoiseSrvHeapIndex	= NoiseIDX;

	mMaterials.push_back(mat);
}

void BoxApp::uploadLight(Light light)
{
	mLights.push_back(light);
	mLightDatas.resize(mLights.size());
}

void BoxApp::BindTexture(RenderItem* r, std::string name, int idx, bool isCubeMap) {
	assert(r  && "The RenderItem is NULL!");

	ObjectData* obj = mGameObjectDatas[r->mName];

	if (!isCubeMap)
	{
		if (obj->Mat.size() <= idx)
			throw std::runtime_error("Index is equal or over than r->Mat size.");

		bool isFound = false;
		int diffuseIDX = 0;
		for (int idx = 0; idx < mTextureList.size(); idx++) {
			if (mTextureList[idx] == name) {
				isFound = true;
				break;
			}

			diffuseIDX++;
		}

		assert(isFound && "Can't find Texture which same with NAME!!");

		obj->Mat[idx].DiffuseSrvHeapIndex = diffuseIDX;
		obj->Mat[idx].NormalSrvHeapIndex = diffuseIDX;
	}
	else
	{
		if (obj->SkyMat.size() <= idx)
			throw std::runtime_error("Index is equal or over than r->SkyMat size.");

		bool isFound = false;
		int diffuseIDX = 0;
		for (int idx = 0; idx < mTextureList.size(); idx++) {
			if (mTextureList[idx] == name) {
				isFound = true;
				break;
			}

			diffuseIDX++;
		}

		assert(isFound && "Can't find Texture which same with NAME!!");

		obj->SkyMat[idx].DiffuseSrvHeapIndex = diffuseIDX;
		obj->SkyMat[idx].NormalSrvHeapIndex = diffuseIDX;
	}
}

void BoxApp::BindMaterial(
	RenderItem* r,
	std::string name,
	bool isCubeMap
) {
	assert(r && "The RenderItem is NULL");

	ObjectData* obj = mGameObjectDatas[r->mName];
	bool isFind = false;

	for (auto& iter = mMaterials.begin(); iter != mMaterials.end(); iter++) {
		if (iter->Name == name) {
			isFind = true;

			UINT matSize = 0;
			if (!isCubeMap)
			{
				matSize = (UINT)obj->Mat.size();
				obj->Mat.resize(matSize + 1);

				obj->Mat[matSize].isSkyTexture = isCubeMap;

				obj->Mat[matSize].Name = iter->Name;
				obj->Mat[matSize].DiffuseAlbedo = iter->DiffuseAlbedo;
				obj->Mat[matSize].DiffuseSrvHeapIndex = iter->DiffuseSrvHeapIndex;
				obj->Mat[matSize].FresnelR0 = iter->FresnelR0;
				obj->Mat[matSize].isSkyTexture = iter->isSkyTexture;
				obj->Mat[matSize].MatCBIndex = iter->MatCBIndex;
				obj->Mat[matSize].MatInstIndex = iter->MatInstIndex;
				obj->Mat[matSize].MatTransform = iter->MatTransform;
				obj->Mat[matSize].NormalSrvHeapIndex = iter->NormalSrvHeapIndex;
				obj->Mat[matSize].NoiseSrvHeapIndex = iter->NoiseSrvHeapIndex;
				obj->Mat[matSize].MaskSrvHeapIndex = iter->MaskSrvHeapIndex;
				obj->Mat[matSize].NumFramesDirty = iter->NumFramesDirty;
				obj->Mat[matSize].Roughness = iter->Roughness;
			}
			else
			{
				matSize = (UINT)obj->SkyMat.size();
				obj->SkyMat.resize(matSize + 1);

				obj->SkyMat[matSize].isSkyTexture = isCubeMap;

				obj->SkyMat[matSize].Name = iter->Name;
				obj->SkyMat[matSize].DiffuseAlbedo = iter->DiffuseAlbedo;
				obj->SkyMat[matSize].DiffuseSrvHeapIndex = iter->DiffuseSrvHeapIndex;
				obj->SkyMat[matSize].FresnelR0 = iter->FresnelR0;
				obj->SkyMat[matSize].isSkyTexture = iter->isSkyTexture;
				obj->SkyMat[matSize].MatCBIndex = iter->MatCBIndex;
				obj->SkyMat[matSize].MatInstIndex = iter->MatInstIndex;
				obj->SkyMat[matSize].MatTransform = iter->MatTransform;
				obj->SkyMat[matSize].NormalSrvHeapIndex = iter->NormalSrvHeapIndex;
				obj->SkyMat[matSize].NoiseSrvHeapIndex = iter->NoiseSrvHeapIndex;
				obj->SkyMat[matSize].MaskSrvHeapIndex = iter->MaskSrvHeapIndex;
				obj->SkyMat[matSize].NumFramesDirty = iter->NumFramesDirty;
				obj->SkyMat[matSize].Roughness = iter->Roughness;
			}

			break;
		}
	}

	assert(isFind && "Can't find Material which same with NAME!!");
}

void BoxApp::BindMaterial(
	RenderItem* r, 
	std::string name,
	std::string maskTexName,
	std::string noiseTexName,
	bool isCubeMap
) {
	assert(r && "The RenderItem is NULL");

	ObjectData* obj = mGameObjectDatas[r->mName];
	bool isFind = false;

	for (auto& iter = mMaterials.begin(); iter != mMaterials.end(); iter++) {
		if (iter->Name == name) {
			isFind = true;

			UINT matSize = 0;
			if (!isCubeMap)
			{
				matSize = (UINT)obj->Mat.size();
				obj->Mat.resize(matSize + 1);

				obj->Mat[matSize].isSkyTexture = isCubeMap;

				obj->Mat[matSize].Name = iter->Name;
				obj->Mat[matSize].DiffuseAlbedo = iter->DiffuseAlbedo;
				obj->Mat[matSize].DiffuseSrvHeapIndex = iter->DiffuseSrvHeapIndex;
				obj->Mat[matSize].FresnelR0 = iter->FresnelR0;
				obj->Mat[matSize].isSkyTexture = iter->isSkyTexture;
				obj->Mat[matSize].MatCBIndex = iter->MatCBIndex;
				obj->Mat[matSize].MatInstIndex = iter->MatInstIndex;
				obj->Mat[matSize].MatTransform = iter->MatTransform;
				obj->Mat[matSize].NormalSrvHeapIndex = iter->NormalSrvHeapIndex;
				obj->Mat[matSize].NoiseSrvHeapIndex = iter->NoiseSrvHeapIndex;
				obj->Mat[matSize].MaskSrvHeapIndex = iter->MaskSrvHeapIndex;
				obj->Mat[matSize].NumFramesDirty = iter->NumFramesDirty;
				obj->Mat[matSize].Roughness = iter->Roughness;
			}
			else
			{
				matSize = (UINT)obj->SkyMat.size();
				obj->SkyMat.resize(matSize + 1);

				obj->SkyMat[matSize].isSkyTexture = isCubeMap;

				obj->SkyMat[matSize].Name = iter->Name;
				obj->SkyMat[matSize].DiffuseAlbedo = iter->DiffuseAlbedo;
				obj->SkyMat[matSize].DiffuseSrvHeapIndex = iter->DiffuseSrvHeapIndex;
				obj->SkyMat[matSize].FresnelR0 = iter->FresnelR0;
				obj->SkyMat[matSize].isSkyTexture = iter->isSkyTexture;
				obj->SkyMat[matSize].MatCBIndex = iter->MatCBIndex;
				obj->SkyMat[matSize].MatInstIndex = iter->MatInstIndex;
				obj->SkyMat[matSize].MatTransform = iter->MatTransform;
				obj->SkyMat[matSize].NormalSrvHeapIndex = iter->NormalSrvHeapIndex;
				obj->SkyMat[matSize].NoiseSrvHeapIndex = iter->NoiseSrvHeapIndex;
				obj->SkyMat[matSize].MaskSrvHeapIndex = iter->MaskSrvHeapIndex;
				obj->SkyMat[matSize].NumFramesDirty = iter->NumFramesDirty;
				obj->SkyMat[matSize].Roughness = iter->Roughness;
			}

			break;
		}
	}

	assert(isFind && "Can't find Material which same with NAME!!");
}

void BoxApp::BindMaterial(
	RenderItem* r,
	std::string matName,
	std::string texName,
	bool isCubeMap
) {
	assert(r && "The RenderItem is NULL");

	// Define of Material
	Material* m = NULL;
	for (auto& i = mMaterials.begin(); i != mMaterials.end(); i++) {
		if (i->Name == matName) {
			m = i._Ptr;
			break;
		}
	}

	assert(m && "Can't find Material which same with NAME!!");

	// Define of Texture
	bool isFound = false;
	int diffuseIDX = 0;
	for (int idx = 0; idx < mTextureList.size(); idx++) {
		if (mTextureList[idx] == texName) {
			isFound = true;
			break;
		}

		diffuseIDX++;
	}

	assert(isFound && "Can't find Texture which same with NAME!!");

	m->isSkyTexture = isCubeMap;
	m->DiffuseSrvHeapIndex = diffuseIDX;
	m->NormalSrvHeapIndex = diffuseIDX;

	mGameObjectDatas[r->mName]->Mat.push_back(*m);
}

void BoxApp::BindMaterial(
	RenderItem* r, 
	std::string matName, 
	std::string texName, 
	std::string maskTexName,
	std::string noiseTexName,
	bool isCubeMap
) {
	assert(r && "The RenderItem is NULL");

	// Define of Material
	Material* m = NULL;
	for (auto& i = mMaterials.begin(); i != mMaterials.end(); i++) {
		if (i->Name == matName) {
			m = i._Ptr;
			break;
		}
	}

	assert(m && "Can't find Material which same with NAME!!");

	// Define of Texture
	int diffuseIDX	= -1;
	int maskIDX		= -1;
	int noiseIDX	= -1;

	for (int idx = 0; idx < mTextureList.size(); idx++) {
		if (mTextureList[idx] == texName)
			diffuseIDX	= idx;
		else if (mTextureList[idx] == maskTexName)
			maskIDX		= idx;
		else if (mTextureList[idx] == noiseTexName)
			noiseIDX	= idx;
	}

	if (
		diffuseIDX	== -1 ||
		maskIDX		== -1 ||
		noiseIDX	== -1
		) 
	{
		throw std::runtime_error("텍스쳐를 찾지 못하였습니다.");
	}

	m->isSkyTexture			= isCubeMap;
	m->DiffuseSrvHeapIndex	= diffuseIDX;
	m->NormalSrvHeapIndex	= diffuseIDX;
	m->MaskSrvHeapIndex		= maskIDX;
	m->NoiseSrvHeapIndex	= noiseIDX;

	mGameObjectDatas[r->mName]->Mat.push_back(*m);
}

void RenderItem::setOnlySubmesh(
	std::string mName,
	int mSubmeshIDX
) {
	ObjectData* mData = mGameObjectDatas[mName];

	if (!mData)
		throw std::runtime_error("");

	if (!mData->onlySubmeshIDXs.size())
	{
		mData->InstanceCount = 0;
	}

	int onlySubmeshInstanceIDX = -1;
	for (int i = 0; i < mData->onlySubmeshIDXs.size(); i++)
	{
		if (mData->onlySubmeshIDXs[i] == mSubmeshIDX)
			onlySubmeshInstanceIDX = i;
	}

	if (onlySubmeshInstanceIDX == -1)
	{
		mData->onlySubmeshIDXs.push_back(mSubmeshIDX);
		mData->onlySubmeshInstanceIDXs.resize(mData->onlySubmeshIDXs.size());
		onlySubmeshInstanceIDX = (int)mData->onlySubmeshIDXs.size() - 1;
	}

	mData->isOnlySubmesh = true;
	mData->onlySubmeshInstanceIDXs[onlySubmeshInstanceIDX].push_back(mData->InstanceCount);

	// 인스턴스 추가
	mData->InstanceCount++;
	mData->mInstances.resize(mData->InstanceCount);
	mData->Bounds.resize(mData->InstanceCount);
	//mData->mPhysResources.resize(mData->InstanceCount);

	mInstanceIsUpdated = 1;
}

void RenderItem::setOnlySubmesh(
	std::string mName,
	std::string mSubmeshName
) {
	ObjectData* mData = mGameObjectDatas[mName];

	if (!mData)
		throw std::runtime_error("");

	int index = -1;
	for (int i = 0; i < mData->mGeometry.meshNames.size(); i++)
	{
		if (mData->mGeometry.meshNames[i] == mSubmeshName)
		{
			index = i;
			break;
		}
	}

	if (!mData->onlySubmeshIDXs.size())
	{
		mData->InstanceCount = 0;
	}

	int onlySubmeshInstanceIDX = -1;
	for (int i = 0; i < mData->onlySubmeshIDXs.size(); i++)
	{
		if (mData->onlySubmeshIDXs[i] == index)
			onlySubmeshInstanceIDX = i;
	}

	if (onlySubmeshInstanceIDX == -1)
	{
		mData->onlySubmeshIDXs.push_back(index);
		mData->onlySubmeshInstanceIDXs.resize(mData->onlySubmeshIDXs.size());
		onlySubmeshInstanceIDX = (int)mData->onlySubmeshIDXs.size() - 1;
	}

	mData->isOnlySubmesh = true;
	mData->onlySubmeshInstanceIDXs[onlySubmeshInstanceIDX].push_back(mData->InstanceCount);

	// 인스턴스 추가
	mData->InstanceCount++;
	mData->mInstances.resize(mData->InstanceCount);
	mData->Bounds.resize(mData->InstanceCount);
	//mData->mPhysResources.resize(mData->InstanceCount);

	mInstanceIsUpdated = 1;
}

void RenderItem::Instantiate(
	DirectX::XMFLOAT3 mPosition,
	DirectX::XMFLOAT3 mRotation,
	DirectX::XMFLOAT3 mScale,
	DirectX::XMFLOAT3 mBoundScale
) {
	ObjectData* mData = mGameObjectDatas[this->mName];

	UINT mInstanceIndex = mData->InstanceCount;

	// 인스턴스 추가
	mData->InstanceCount++;
	mData->mInstances.resize(mData->InstanceCount);
	mData->Bounds.resize(mData->InstanceCount);
	//mData->mPhysResources.resize(mData->InstanceCount);

	BoundsIterator = std::next(mData->Bounds.begin(), mInstanceIndex);

	(*BoundsIterator).Center.x = mPosition.x;
	(*BoundsIterator).Center.y = mPosition.y;
	(*BoundsIterator).Center.z = mPosition.z;

	(*BoundsIterator).Extents.x = mBoundScale.x;
	(*BoundsIterator).Extents.y = mBoundScale.y;
	(*BoundsIterator).Extents.z = mBoundScale.z;

	// World 변경
	mRotation.x = DirectX::XMConvertToRadians(mRotation.x);
	mRotation.y = DirectX::XMConvertToRadians(mRotation.y);
	mRotation.z = DirectX::XMConvertToRadians(mRotation.z);

	DirectX::XMVECTOR mPositionVec = DirectX::XMLoadFloat3(&mPosition);
	DirectX::XMVECTOR mRotationVec = DirectX::XMLoadFloat3(&mRotation);
	DirectX::XMVECTOR mScaleVec = DirectX::XMLoadFloat3(&mScale);

	DirectX::XMVECTOR mQuaternionVec = DirectX::XMQuaternionRotationRollPitchYawFromVector(mRotationVec);
	DirectX::XMVECTOR mOrientationVec;

	mOrientationVec.m128_f32[0] = 0.0f;
	mOrientationVec.m128_f32[1] = 0.0f;
	mOrientationVec.m128_f32[2] = 0.0f;
	mOrientationVec.m128_f32[3] = 1.0f;

	DirectX::XMMATRIX mWorldMat = DirectX::XMMatrixAffineTransformation(
		mScaleVec,
		mOrientationVec,
		mQuaternionVec,
		mPositionVec
	);

	InstanceIterator = std::next(mData->mInstances.begin(), mInstanceIndex);
	DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

	mData->mTranslate.push_back(
		Translate(
			mPositionVec,
			mRotationVec,
			mScaleVec,
			mWorldMat
		)
	);

	mInstanceIsUpdated = 1;
}

void RenderItem::Destroy (
	UINT InstanceIDX
) {
	ObjectData* mData = mGameObjectDatas[this->mName];

	if (!mData)
		throw std::runtime_error("");

	UINT mInstanceIndex = mData->InstanceCount;

	mData->InstanceCount--;

	InstanceIterator = std::next(mData->mInstances.begin(), InstanceIDX);
	BoundsIterator   = std::next(mData->Bounds.begin(), InstanceIDX);

	mData->mInstances.erase(InstanceIterator);
	mData->Bounds.erase(BoundsIterator);

	mInstanceIsUpdated = 1;
}

void RenderItem::InitParticleSystem(
	_In_ float mDurationTime,
	_In_ bool mOnPlayAwake
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setInstanceCount(InstanceCount);

	mParticle->setDurationTime(mDurationTime);
	mParticle->setOnPlayAwake(mOnPlayAwake);
}

void RenderItem::InitParticleSystem(
	_In_ float mDurationTime,
	_In_ DirectX::XMFLOAT3 mMinAcc,
	_In_ DirectX::XMFLOAT3 mMaxAcc,
	_In_ DirectX::XMFLOAT3 mMinVelo,
	_In_ DirectX::XMFLOAT3 mMaxVelo,
	_In_ bool mOnPlayAwake
) {
	Particle* mParticle = mParticles[mName];

	if ((UINT)mGameObjectDatas[mName]->mBillBoardVertices.size() > 0)
		mParticle->setInstanceCount((UINT)mGameObjectDatas[mName]->mBillBoardVertices.size());
	else
		mParticle->setInstanceCount((UINT)this->InstanceCount);

	mParticle->setDurationTime(mDurationTime);
	mParticle->setMinAcc(mMinAcc);
	mParticle->setMaxAcc(mMaxAcc);
	mParticle->setMinVelo(mMinVelo);
	mParticle->setMaxVelo(mMaxVelo);
	mParticle->setOnPlayAwake(mOnPlayAwake);


}

void RenderItem::ParticleGene()
{
	ObjectData* obj = mGameObjectDatas[mName];
	Particle* mParticle = mParticles[mName];

	std::random_device rd;

	DirectX::XMFLOAT3 scaleError = mParticle->mScaleError;

	std::uniform_real_distribution<float> scaleErrorX(-scaleError.x, scaleError.x);
	std::uniform_real_distribution<float> scaleErrorY(-scaleError.y, scaleError.y);
	std::uniform_real_distribution<float> scaleErrorZ(-scaleError.z, scaleError.z);

	mParticle->mTime = 0.0f;

	mParticle->Generator();

	for (int i = 0; i < obj->mInstances.size(); i++)
	{
		std::mt19937 gen(rd());

		mParticle->mErrorScale[i].m128_f32[0] = scaleErrorX(gen);
		mParticle->mErrorScale[i].m128_f32[1] = scaleErrorY(gen);
		mParticle->mErrorScale[i].m128_f32[2] = scaleErrorZ(gen);
	}
}

void RenderItem::ParticleUpdate(float delta, float time)
{
	ObjectData* obj = mGameObjectDatas[mName];
	Particle* mParticle = mParticles[mName];

	mParticle->mTime += delta;

	float mStartDelay = mParticle->getStartDelay();
	if (time < mStartDelay)
		return;

	bool isScaleUpdated = false;
	bool isDiffuseUpdated = false;
	DirectX::XMFLOAT3 position = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 rotation = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	std::string mDiffuseMaterialName = "";

	// 스케일 애니메이션 프레임 키가 존재할 경우
	if (mParticle->mScaleAnimation.size() > 0)
	{
		// 현재 진행 시간이 현재 프레임 시간보다 커진다면 프레임과 스케일을 업데이트.
		if (mParticle->mScaleAnimation[mParticle->mScaleAnimIndex].time <= time)
		{
			mParticle->mPreScale = mParticle->mScaleAnimation[mParticle->mScaleAnimIndex].scale;
			mParticle->mScaleAnimIndex++;
		}

		// ScaleAnimationIndex가 오버플로 되지 않았으면
		if (mParticle->mScaleAnimIndex < mParticle->mScaleAnimation.size())
		{
			isScaleUpdated = true;

			float timeScale = 
				(time - mParticle->mScaleAnimation[mParticle->mScaleAnimIndex - 1].time) /
				(mParticle->mScaleAnimation[mParticle->mScaleAnimIndex].time - mParticle->mScaleAnimation[mParticle->mScaleAnimIndex - 1].time);

			scale = mParticle->mPreScale;
			DirectX::XMFLOAT3 scale2 = mParticle->mScaleAnimation[mParticle->mScaleAnimIndex].scale;

			scale.x += (scale2.x - scale.x) * timeScale;
			scale.y += (scale2.y - scale.y) * timeScale;
			scale.z += (scale2.z - scale.z) * timeScale;
		}
		// 오버플로 된다면
		else
		{
			// 오버플로 나지 않도록 프레임을 다시 보간
			mParticle->mScaleAnimIndex = (int)mParticle->mScaleAnimation.size() - 1;
		}
	}

	// 디퓨즈 애니메이션 프레임 키가 존재할 경우
	if (mParticle->mDiffuseAnimation.size() > 0)
	{
		// 현재 진행 시간이 현재 프레임 시간보다 커진다면 프레임과 디퓨즈를 업데이트.
		if (mParticle->mDiffuseAnimation[mParticle->mDiffuseAnimIndex].time <= time)
		{
			mParticle->mPreDiffuse = mParticle->mDiffuseAnimation[mParticle->mDiffuseAnimIndex].color;
			mParticle->mDiffuseAnimIndex++;
		}

		// mDiffuseAnimIndex가 오버플로 되지 않았으면
		if (mParticle->mDiffuseAnimIndex < mParticle->mDiffuseAnimation.size())
		{
			isDiffuseUpdated = true;

			float timeDiffuse =
				(time - mParticle->mDiffuseAnimation[mParticle->mDiffuseAnimIndex - 1].time) /
				(mParticle->mDiffuseAnimation[mParticle->mDiffuseAnimIndex].time - mParticle->mDiffuseAnimation[mParticle->mDiffuseAnimIndex - 1].time);

			diffuse = mParticle->mPreDiffuse;
			DirectX::XMFLOAT4 color2 = mParticle->mDiffuseAnimation[mParticle->mDiffuseAnimIndex].color;

			diffuse.x += (color2.x - diffuse.x) * timeDiffuse;
			diffuse.y += (color2.y - diffuse.y) * timeDiffuse;
			diffuse.z += (color2.z - diffuse.z) * timeDiffuse;
			diffuse.w += (color2.w - diffuse.w) * timeDiffuse;
		}
		// 오버플로 된다면
		else
		{
			// 오버플로 나지 않도록 프레임을 다시 보간
			mParticle->mDiffuseAnimIndex = (int)mParticle->mDiffuseAnimation.size() - 1;
		}
	}

	if (obj->isTextureSheetAnimation)
	{
		DirectX::XMFLOAT4 sprite = mParticle->getSprite();
		InstanceIterator = obj->mInstances.begin();

		(*InstanceIterator).TopLeftTexCroodPos = { sprite.x, sprite.y };
		(*InstanceIterator).BottomRightTexCroodPos = { sprite.z, sprite.w };

		// particle->setTextureSheetAnimationFrame

		if (mParticle->mTime > mParticle->getTextureSheetAnimationFrame())
		{
			mParticle->mTime = 0.0f;
			mParticle->mCurrFrame += 1;
		}
	}

	std::vector<BillBoardSpriteVertex>::iterator iter = obj->mBillBoardVertices.begin();
	std::vector<BillBoardSpriteVertex>::iterator mEnd = obj->mBillBoardVertices.end();
	UINT partIDX = 0;

	if (obj->mBillBoardVertices.size() > 0)
	{
		obj->isBillBoardDirty = true;
		mParticle->ParticleUpdate(delta);

		DirectX::XMVECTOR mParentPosition = mParent->mData->mTranslate[0].position;
		mParentPosition.m128_f32[0] *= 10.0f;
		mParentPosition.m128_f32[1] *= 10.0f;
		mParentPosition.m128_f32[2] *= 10.0f;

		while (iter != mEnd)
		{
			(*iter).Pos.x =

				mParentPosition.m128_f32[0] +
				mParticle->mDist[partIDX].m128_f32[0];
			(*iter).Pos.y =
				mParentPosition.m128_f32[1] +
				mParticle->mDist[partIDX].m128_f32[1];
			(*iter).Pos.z =
				mParentPosition.m128_f32[2] +
				mParticle->mDist[partIDX].m128_f32[2];

			(*iter).Norm.x = mParticle->mDeltaDist[partIDX].m128_f32[0];
			(*iter).Norm.y = mParticle->mDeltaDist[partIDX].m128_f32[1];
			(*iter).Norm.z = mParticle->mDeltaDist[partIDX].m128_f32[2];

			partIDX++;
			iter++;
		}
	}
	
	// 스케일 애니메이션이 업데이트 되었다면
	if (isScaleUpdated)
	{
		if (obj->isBillBoard)
		{
			partIDX = 0;

			iter = obj->mBillBoardVertices.begin();
			mEnd = obj->mBillBoardVertices.end();

			while (iter != mEnd)
			{
				if (!mParticle->isUseErrorScale)
				{
					(*iter).Size.x = scale.x;
					(*iter).Size.y = scale.y;
				}
				else
				{
					(*iter).Size.x = scale.x * mParticle->mErrorScale[partIDX].m128_f32[0];
					(*iter).Size.y = scale.y * mParticle->mErrorScale[partIDX].m128_f32[1];
				}

				partIDX++;
				iter++;
			}
		}
		else
		{
			for (int i = 0; i < obj->mInstances.size(); i++)
			{
				if (!mParticle->isUseErrorScale)
				{
					obj->mTranslate[i].scale.m128_f32[0] = scale.x;
					obj->mTranslate[i].scale.m128_f32[1] = scale.y;
					obj->mTranslate[i].scale.m128_f32[2] = scale.z;
				}
				else
				{
					obj->mTranslate[i].scale.m128_f32[0] =
						scale.x * mParticle->mErrorScale[partIDX].m128_f32[0];
					obj->mTranslate[i].scale.m128_f32[1] =
						scale.y * mParticle->mErrorScale[partIDX].m128_f32[1];
					obj->mTranslate[i].scale.m128_f32[2] =
						scale.z;
				}
			}
		}
	}
	else
	{
		DirectX::XMMATRIX mWorldMat;

		InstanceIterator = obj->mInstances.begin();
		for (int i = 0; i < obj->mInstances.size(); i++)
		{
			mParticle->ParticleUpdate(delta);

			mWorldMat = DirectX::XMLoadFloat4x4(&(*InstanceIterator).World);

			//mWorldMat.r[3].m128_f32[0] = mParent->mData->mTranslate[0].position.m128_f32[0];
			//mWorldMat.r[3].m128_f32[1] = mParent->mData->mTranslate[0].position.m128_f32[1];
			//mWorldMat.r[3].m128_f32[2] = mParent->mData->mTranslate[0].position.m128_f32[2];

			obj->mTranslate[i].position.m128_f32[0] = mParticle->mDeltaDist[i].m128_f32[0];
			obj->mTranslate[i].position.m128_f32[1] = mParticle->mDeltaDist[i].m128_f32[1];
			obj->mTranslate[i].position.m128_f32[2] = mParticle->mDeltaDist[i].m128_f32[2];

			if (!isScaleUpdated && mParticle->isUseErrorScale)
			{
				mWorldMat.r[0].m128_f32[0] *= mParticle->mErrorScale[i].m128_f32[0];
				mWorldMat.r[1].m128_f32[1] *= mParticle->mErrorScale[i].m128_f32[1];
				mWorldMat.r[2].m128_f32[2] *= mParticle->mErrorScale[i].m128_f32[2];
			}

			DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, mWorldMat);

			InstanceIterator++;
		}
	}

	if (isDiffuseUpdated)
	{
		obj->isDiffuseDirty = true;

		for (int i = 0; i < obj->Mat.size(); i++)
		{
			obj->Mat[i].DiffuseAlbedo.x = diffuse.x;
			obj->Mat[i].DiffuseAlbedo.y = diffuse.y;
			obj->Mat[i].DiffuseAlbedo.z = diffuse.z;
			obj->Mat[i].DiffuseAlbedo.w = diffuse.w;
		}
	}
}

void RenderItem::ParticleReset()
{
	ObjectData* obj = mGameObjectDatas[mName];
	Particle* mParticle = mParticles[mName];

	obj->isBillBoardDirty = true;
	mParticle->mTime = 0.0f;
	mParticle->mCurrFrame = 0;
	mParticle->mScaleAnimIndex = 0;
	mParticle->mDiffuseAnimIndex = 0;

	std::vector<BillBoardSpriteVertex>::iterator iter = obj->mBillBoardVertices.begin();
	std::vector<BillBoardSpriteVertex>::iterator mEnd = obj->mBillBoardVertices.end();

	while (iter != mEnd)
	{
		(*iter).Pos.x = 0.0f;
		(*iter).Pos.y = 0.0f;
		(*iter).Pos.z = 0.0f;

		iter++;
	}

	for (int i = 0; i < mParticle->mDist.size(); i++)
	{
		mParticle->mDist[i].m128_f32[0] = 0.0f;
		mParticle->mDist[i].m128_f32[1] = 0.0f;
		mParticle->mDist[i].m128_f32[2] = 0.0f;
		mParticle->mDist[i].m128_f32[3] = 0.0f;
	}

	// 파티클 월드 매트릭스 초기화
	InstanceIterator = obj->mInstances.begin();
	for (UINT i = 0; i < obj->InstanceCount; i++)
	{
		DirectX::XMStoreFloat4x4(&(*InstanceIterator).World, obj->mTranslate[i].mUpdateWorldMat);

		InstanceIterator++;
	}

	if (mParticle->isUseErrorScale)
	{
		std::random_device rd;
		std::mt19937 gen(rd());

		DirectX::XMFLOAT3 scaleError = mParticle->mScaleError;

		std::uniform_real_distribution<float> scaleErrorX(0.0f, scaleError.x);
		std::uniform_real_distribution<float> scaleErrorY(0.0f, scaleError.y);
		std::uniform_real_distribution<float> scaleErrorZ(0.0f, scaleError.z);

		for (UINT i = 0; i < obj->InstanceCount; i++)
		{
			mParticle->mErrorScale[i].m128_f32[0] = scaleError.x != 0.0f ? scaleErrorX(gen) : 1.0f;
			mParticle->mErrorScale[i].m128_f32[1] = scaleError.y != 0.0f ? scaleErrorY(gen) : 1.0f;
			mParticle->mErrorScale[i].m128_f32[2] = scaleError.z != 0.0f ? scaleErrorZ(gen) : 1.0f;
		}
	}

	mParticle->Generator();
}

void RenderItem::setDurationTime(
	_In_ float duration
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setDurationTime(duration);
}
void RenderItem::setIsLoop(
	_In_ bool isLoop
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setIsLoop(isLoop);
}
void RenderItem::setIsFilled(
	_In_ bool isFilled
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setIsFilled(isFilled);
}
void RenderItem::setStartDelay(
	_In_ float startDelay
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setStartDelay(startDelay);
}
void RenderItem::setStartLifeTime(
	_In_ float startLifeTime
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setStartLifeTime(startLifeTime);
}
void RenderItem::setOnPlayAwake(
	_In_ bool onPlayAwake
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setOnPlayAwake(onPlayAwake);
}
void RenderItem::setMinAcc(
	_In_ DirectX::XMFLOAT3 minAcc
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setMinVelo(minAcc);
}
void RenderItem::setMaxAcc(
	_In_ DirectX::XMFLOAT3 maxAcc
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setMaxVelo(maxAcc);
}
void RenderItem::setMinVelo(
	_In_ DirectX::XMFLOAT3 minVelo
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setMinVelo(minVelo);
}
void RenderItem::setMaxVelo(
	_In_ DirectX::XMFLOAT3 maxVelo
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setMaxVelo(maxVelo);
}
void RenderItem::setTextureSheetAnimationXY(
	_In_ UINT x,
	_In_ UINT y
) {
	Particle* mParticle = mParticles[mName];
	mGameObjectDatas[mName]->isTextureSheetAnimation = true;

	mParticle->setTextureSheetAnimationXY(x, y);
}
void RenderItem::setTextureSheetAnimationFrame(
	_In_ float mFrame
) {
	Particle* mParticle = mParticles[mName];

	mParticle->setTextureSheetAnimationFrame(mFrame);
}
void RenderItem::setIsUsedErrorScale(
	_In_ DirectX::XMFLOAT3 mErrorScale
) {
	Particle* mParticle = mParticles[mName];

	mParticle->isUseErrorScale = true;
	mParticle->mScaleError = mErrorScale;
}

void RenderItem::appendScaleAnimation(
	_In_ float mTime,
	_In_ DirectX::XMFLOAT3 mScale
) {
	Particle* mParticle = mParticles[mName];

	mParticle->mScaleAnimation.push_back (
		Particle::ScaleAnimation(
			mTime, 
			mScale
		)
	);
}
void RenderItem::appendDiffuseAnimation(
	_In_ float mTime,
	_In_ DirectX::XMFLOAT4 mDiffuse
) {
	Particle* mParticle = mParticles[mName];

	mParticle->mDiffuseAnimation.push_back(
		Particle::DiffuseAnimation(
			mTime,
			mDiffuse
		)
	);
}

void BoxApp::Pick(int sx, int sy)
{
	XMFLOAT4X4 P = mCamera.GetProj4x4f();

	// Compute picking ray in view space.
	// [0, 600] -> [-1, 1] [0, 800] -> [-1, 1]
	float vx = (+2.0f * sx / mClientWidth - 1.0f);
	float vy = (-2.0f * sy / mClientHeight + 1.0f);

	// z축을 노말라이즈
	vx = vx / P(0, 0);
	vy = vy / P(1, 1);

	// Ray definition in view space.
	XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

	XMMATRIX V = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(V), V);

	// Check if we picked an opaque render item.  A real app might keep a separate "picking list"
	// of objects that can be selected.   

	std::unordered_map<std::string, ObjectData*>::iterator iter = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator end = mGameObjectDatas.end();

	ObjectData* obj = nullptr;

	//XMMATRIX W;
	//XMMATRIX invWorld;

	/*XMMATRIX toLocal;*/
	//float tmin = 0.0f;

	while (iter != end)
	{
		obj = (*iter).second;

		if (
				obj->Bounds.size()	== 0												||
				obj->mRenderType	== ObjectData::RenderType::_SKY_FORMAT_RENDER_TYPE	||
				obj->mRenderType	== ObjectData::RenderType::_DEBUG_BOX_TYPE			||
				obj->mName			== "BottomGeo"										||
				obj->isDrawTexture
			)
		{
			iter++;

			continue;
		}

		InstanceIterator = obj->mInstances.begin();
		BoundsIterator = obj->Bounds.begin();
		for (UINT inst = 0; inst < obj->InstanceCount; inst++)
		{
			XMMATRIX W = XMLoadFloat4x4(&(*InstanceIterator).World);
			XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);

			// Tranform ray to vi space of Mesh.
			// inv ViewWorld
			XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);

			// Camera View World에서 Origin View World로 변경하는
			rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

			rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
			rayDir = XMVector3TransformNormal(rayDir, toLocal);

			// Make the ray direction unit length for the intersection tests.
			rayDir = XMVector3Normalize(rayDir);

			float tmin = 0.0f;
			if ((*BoundsIterator).Intersects(rayOrigin, rayDir, tmin))
			{
				// 경계 박스에 광선이 접촉한다면, 해당 오브젝트를 선택한다.
				// tmin는 접촉한 경계 박스 중 가장 거리가 작은 박스의 거리를 저장한다.
				// 만일 접촉한 박스가 하나도 없다면 tmin은 0.0이다.

				printf("");
			}

			InstanceIterator++;
			BoundsIterator++;
		}

		iter++;
	}
}

void BoxApp::PickBrush(int sx, int sy)
{
	XMFLOAT4X4 P = mCamera.GetProj4x4f();

	// Compute picking ray in view space.
	// [0, 600] -> [-1, 1] [0, 800] -> [-1, 1]
	float vx = (+2.0f * sx / mClientWidth - 1.0f);
	float vy = (-2.0f * sy / mClientHeight + 1.0f);

	// z축을 노말라이즈
	vx = vx / P(0, 0);
	vy = vy / P(1, 1);

	// Ray definition in view space.
	XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

	XMMATRIX V = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(V), V);

	// Check if we picked an opaque render item.  A real app might keep a separate "picking list"
	// of objects that can be selected.   

	std::unordered_map<std::string, ObjectData*>::iterator iter = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator end = mGameObjectDatas.end();

	ObjectData* obj = nullptr;

	//XMMATRIX W;
	//XMMATRIX invWorld;

	/*XMMATRIX toLocal;*/
	//float tmin = 0.0f;

	while (iter != end)
	{
		obj = (*iter).second;

		if (!obj->isDrawTexture)
		{
			iter++;

			continue;
		}

		InstanceIterator = obj->mInstances.begin();
		BoundsIterator = obj->Bounds.begin();
		for (UINT inst = 0; inst < obj->InstanceCount; inst++)
		{
			XMMATRIX W = XMLoadFloat4x4(&(*InstanceIterator).World);
			XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);

			// Tranform ray to vi space of Mesh.
			// inv ViewWorld
			XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);

			// Camera View World에서 Origin View World로 변경하는
			rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

			rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
			rayDir = XMVector3TransformNormal(rayDir, toLocal);

			// Make the ray direction unit length for the intersection tests.
			rayDir = XMVector3Normalize(rayDir);

			float tmin = 0.0f;
			if ((*BoundsIterator).Intersects(rayOrigin, rayDir, tmin))
			{
				XMVECTOR hitPos = rayOrigin + rayDir * tmin;

				XMVECTOR mBoundPos[2];

				mBoundPos[0].m128_f32[0] = (*BoundsIterator).Center.x - (*BoundsIterator).Extents.x;
				mBoundPos[0].m128_f32[1] = 0.0;
				mBoundPos[0].m128_f32[2] = (*BoundsIterator).Center.z - (*BoundsIterator).Extents.z;
				mBoundPos[0].m128_f32[3] = 1.0f;

				mBoundPos[1].m128_f32[0] = (*BoundsIterator).Center.x + (*BoundsIterator).Extents.x;
				mBoundPos[1].m128_f32[1] = 0.0;
				mBoundPos[1].m128_f32[2] = (*BoundsIterator).Center.z + (*BoundsIterator).Extents.z;
				mBoundPos[1].m128_f32[3] = 1.0f;

				float mTexCroodX, mTexCroodY;

				mDrawTexture->leftTop		= { mBoundPos[0].m128_f32[0], mBoundPos[0].m128_f32[2] };
				mDrawTexture->rightBottom	= { mBoundPos[1].m128_f32[0], mBoundPos[1].m128_f32[2] };

				mTexCroodX = 
					(hitPos.m128_f32[0] - mBoundPos[0].m128_f32[0]) / 
					(mDrawTexture->rightBottom.x - mDrawTexture->leftTop.x);
				mTexCroodY = 
					(hitPos.m128_f32[2] - mBoundPos[0].m128_f32[2]) / 
					(mDrawTexture->rightBottom.y - mDrawTexture->leftTop.y);

				// Let's Draw!!
				mDrawTexture->Color = { 1.0f, 0.0f, 0.0f, 1.0f };

				mDrawTexture->Origin =
				{
					mTexCroodX,
					1.0f - mTexCroodY
				};

				for (UINT i = 0; i < mBrushThreadNum; i++)
				{
					mScreenWidth[i] = (float)mClientWidth;
					mScreenHeight[i] = (float)mClientHeight;

					mClickedPosX[i] = hitPos.m128_f32[0];
					mClickedPosY[i] = hitPos.m128_f32[2];

					mBrushOrigin[i] = 
					{
						mTexCroodX,
						1.0f - mTexCroodY
					};

					mBrushPosition[i] = 
					{
						mTexCroodX,
						1.0f - mTexCroodY
					};
				}
			}

			InstanceIterator++;
			BoundsIterator++;
		}

		iter++;
	}
}

//////////////////////////////////
// RenderItem
//////////////////////////////////

void RenderItem::setPosition(_In_ XMFLOAT3 pos) {
	//XMVECTOR vec = DirectX::XMLoadFloat3(&pos);

	//for (UINT i = 0; i < InstanceCount; i++) {
	//	//mPhysResources[i].Position[0] = pos.x;
	//	//mPhysResources[i].Position[1] = pos.y;
	//	//mPhysResources[i].Position[2] = pos.z;

	//	memcpy(mGameObjectDatas[mName]->mPhysResources[i].Position, vec.m128_f32, sizeof(float) * 3);

	//	mPhys.setPosition(mGameObjectDatas[mName]->mPhysRigidBody[i], pos.x, pos.y, pos.z);
	//}
}
void RenderItem::setRotation(_In_ XMFLOAT3 rot) {
	//XMVECTOR vec = DirectX::XMLoadFloat3(&rot);

	//for (UINT i = 0; i < InstanceCount; i++) {
	//	//mPhysResources[i].Rotation[0] = rot.x;
	//	//mPhysResources[i].Rotation[1] = rot.y;
	//	//mPhysResources[i].Rotation[2] = rot.z;

	//	memcpy(mGameObjectDatas[mName]->mPhysResources[i].Rotation, vec.m128_f32, sizeof(float) * 3);

	//	mPhys.setRotation(mGameObjectDatas[mName]->mPhysRigidBody[i], rot.x, rot.y, rot.z);
	//}
}

void RenderItem::setVelocity(_In_ XMFLOAT3 vel) {
	//for (UINT i = 0; i < InstanceCount; i++)
	//	mPhys.setVelocity(mGameObjectDatas[mName]->mPhysRigidBody[i], vel.x, vel.y, vel.z);
}
void RenderItem::setTorque(_In_ XMFLOAT3 torq) {
	//for (UINT i = 0; i < InstanceCount; i++)
	//	mPhys.setTorque(mGameObjectDatas[mName]->mPhysRigidBody[i], torq.x, torq.y, torq.z);
}

void RenderItem::setInstancePosition(_In_ XMFLOAT3 pos, _In_ UINT idx) {
	/*physx[idx]->Position[0] = pos.x;
	physx[idx]->Position[1] = pos.y;
	physx[idx]->Position[2] = pos.z;

	phys.setPosition(physxIdx[idx], pos.x, pos.y, pos.z);*/
}
void RenderItem::setInstanceRotation(_In_ XMFLOAT3 rot, _In_ UINT idx) {
	/*physx[idx]->Rotation[0] = rot.x;
	physx[idx]->Rotation[1] = rot.y;
	physx[idx]->Rotation[2] = rot.z;

	phys.setRotation(physxIdx[idx], rot.x, rot.y, rot.z);*/
}

void RenderItem::setInstanceVelocity(_In_ XMFLOAT3 vel, _In_ UINT idx) {
	//phys.setVelocity(physxIdx[idx], vel.x, vel.y, vel.z);
}
void RenderItem::setInstanceTorque(_In_ XMFLOAT3 torq, _In_ UINT idx) {
	//phys.setTorque(physxIdx[idx], torq.x, torq.y, torq.z);
}

void RenderItem::setAnimIndex(_In_ int animIndex) {
	mGameObjectDatas[mName]->currentAnimIdx = (float)animIndex;
}

float RenderItem::getAnimIndex() {
	return mGameObjectDatas[mName]->currentAnimIdx;
}

void RenderItem::setAnimBeginIndex(_In_ int animBeginIndex) {
	mGameObjectDatas[mName]->beginAnimIndex = (float)animBeginIndex;
	mGameObjectDatas[mName]->currentFrame = animBeginIndex;
	mGameObjectDatas[mName]->currentDelayPerSec = (mGameObjectDatas[mName]->beginAnimIndex * mGameObjectDatas[mName]->durationOfFrame[0]);
}

float RenderItem::getAnimBeginIndex() {
	return mGameObjectDatas[mName]->beginAnimIndex;
}

void RenderItem::setAnimEndIndex(_In_ int animEndIndex) {
	mGameObjectDatas[mName]->endAnimIndex = (float)animEndIndex;
}

float RenderItem::getAnimEndIndex() {
	return mGameObjectDatas[mName]->endAnimIndex;
}

void RenderItem::setAnimIsLoop(_In_ bool animLoop) {
	mGameObjectDatas[mName]->isLoop = animLoop;
}

int RenderItem::getAnimIsLoop() {
	return mGameObjectDatas[mName]->isLoop;
}

void RenderItem::setAnimClip (
	_In_ std::string mClipName, 
	_In_  bool isCompression) 
{
	int res = mAnimationClips[mName]->setCurrentClip(mClipName);

	// 애니메이션 클립 정보를 업데이트
	if (!res)
	{
		mAnimationClips[mName]->getCurrentClip(
			mGameObjectDatas[mName]->beginAnimIndex,
			mGameObjectDatas[mName]->endAnimIndex,
			isCompression
		);

		mGameObjectDatas[mName]->currentDelayPerSec =
			mGameObjectDatas[mName]->beginAnimIndex;
	}
}

const std::string RenderItem::getAnimClip() const 
{
	return  mAnimationClips[mName]->getCurrentClipName();
}