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
std::unique_ptr<DrawTexture>	BoxApp::mDrawTexture;

DirectX::BoundingSphere mSceneBounds;				// 그림자가 그려지는 경계 구

ComPtr<ID3D12DescriptorHeap> BoxApp::mSrvDescriptorHeap = nullptr;

ComPtr<ID3D12RootSignature> BoxApp::mRootSignature = nullptr;
ComPtr<ID3D12RootSignature> BoxApp::mBlurRootSignature = nullptr;
ComPtr<ID3D12RootSignature> BoxApp::mSobelRootSignature = nullptr;
ComPtr<ID3D12RootSignature> BoxApp::mDrawMapSignature = nullptr;

CD3DX12_CPU_DESCRIPTOR_HANDLE BoxApp::mTextureHeapDescriptor;
CD3DX12_GPU_DESCRIPTOR_HANDLE BoxApp::mTextureHeapGPUDescriptor;

// Draw Thread Resource
UINT BoxApp::numThread = 3;

// Thread Trigger Event (Fence)
HANDLE BoxApp::renderTargetEvent[3];
HANDLE BoxApp::recordingDoneEvents[3];
HANDLE BoxApp::drawThreads[3];
LPDWORD BoxApp::ThreadIndex[3] = { 0 };

static std::unique_ptr<UploadBuffer<PassConstants>>				PassCB;
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

BoxApp::~BoxApp()
{
	StopThread = true;

	SetEvent(mClothWriteEvent);
	SetEvent(mAnimationWriteEvent);

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

		// Rigid Body Mesh�� �߰�
		_RenderData->mRigidbody.resize(_RenderItem->SubmeshCount);
		for (UINT submesh = 0; submesh < _RenderItem->SubmeshCount; submesh++)
		{
			if (!_RenderData->isRigidBody[submesh])
			{
				continue;
			}

			if (_RenderData->mFormat == "PMX")
			{
				vOffset = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].BaseVertexLocation;
				iOffset = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].StartIndexLocation;

				vSize = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].VertexSize;
				iSize = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].IndexSize;

				// ���� Material�� Vertex�� ������ ���´�.
				vertices.clear();
				vertices.resize(vSize);
				primitives.clear();
				primitives.resize(iSize);

				int mOffset = 0;
				int mIDXMin;

				std::set<int>::iterator& vIDX = _RenderData->vertBySubmesh[submesh].begin();
				std::set<int>::iterator& vEndIDX = _RenderData->vertBySubmesh[submesh].end();

				while (vIDX != vEndIDX)
				{
					// Physx�� ������ٵ� ���ε�
					vertices[mOffset].pos[0] = _RenderData->vertices[*vIDX].Pos.x;
					vertices[mOffset].pos[1] = _RenderData->vertices[*vIDX].Pos.y;
					vertices[mOffset].pos[2] = _RenderData->vertices[*vIDX].Pos.z;
					vertices[mOffset].invWeight = 1.0f;

					// �� ���ӿ��� ������ٵ� ���̸� �ȵǹǷ� ���
					_RenderData->vertices[*vIDX].Pos.x = 0.0f;
					_RenderData->vertices[*vIDX].Pos.y = 0.0f;
					_RenderData->vertices[*vIDX].Pos.z = 0.0f;

					vIDX++;
					mOffset++;
				}

				mOffset = 0;
				mIDXMin = _RenderData->mModel.indices[iOffset];
				// �ּ� �ε����� ã�´�.
				for (UINT i = iOffset; i < iOffset + iSize; i++)
				{
					if (mIDXMin > _RenderData->mModel.indices[i])
						mIDXMin = _RenderData->mModel.indices[i];
				}

				// ���� Material�� Index ������ ���´�.
				bool isSuccessedScaled = false;
				for (UINT i = iOffset; i < iOffset + iSize; i++)
				{
					if (_RenderData->mModel.indices[i] - mIDXMin < 0)
						throw std::runtime_error("Invaild Indices..");
					else if (_RenderData->mModel.indices[i] - mIDXMin == 0)
						isSuccessedScaled = true;

					primitives[mOffset++] = _RenderData->mModel.indices[i] - mIDXMin;
				}

				if (!isSuccessedScaled)
					throw std::runtime_error("Invaild Indices..");

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

		_RenderData->srcFixVertexSubmesh.resize(_RenderItem->SubmeshCount);
		_RenderData->dstFixVertexSubmesh.resize(_RenderItem->SubmeshCount);
		_RenderData->srcDynamicVertexSubmesh.resize(_RenderItem->SubmeshCount);
		_RenderData->dstDynamicVertexSubmesh.resize(_RenderItem->SubmeshCount);

		for (UINT submesh = 0; submesh < _RenderItem->SubmeshCount; submesh++)
		{
			// Cloth Physix�� ������� �ʴ� ����޽��� ��ŵ
			if (!_RenderData->isCloth[submesh])	continue;

			if (_RenderData->mFormat == "FBX")
			{
				vOffset = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].BaseVertexLocation;
				iOffset = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].StartIndexLocation;

				vSize = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].VertexSize;
				iSize = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].IndexSize;

				vertices.resize(vSize);
				primitives.resize(iSize);

				for (UINT v = 0; v < vSize; v++) {
					vertices[v].pos[0] = _RenderData->vertices[vOffset + v].Pos.x;
					vertices[v].pos[1] = _RenderData->vertices[vOffset + v].Pos.y;
					vertices[v].pos[2] = _RenderData->vertices[vOffset + v].Pos.z;
					vertices[v].invWeight = _RenderData->mClothWeights[vOffset + v];
				}

				// ���� Material�� Index ������ ���´�.
				for (UINT i = 0; i < iSize; i++) {
					primitives[i] = _RenderData->indices[iOffset + i];
				}

				primitives.pop_back();

				PxClothMeshDesc meshDesc;
				meshDesc.points.data = (void*)&vertices.data()->pos;
				meshDesc.points.count = static_cast<PxU32>(vertices.size());
				meshDesc.points.stride = sizeof(PxClothParticle);

				meshDesc.invMasses.data = (void*)&vertices.data()->invWeight;
				meshDesc.invMasses.count = static_cast<PxU32>(vertices.size());
				meshDesc.invMasses.stride = sizeof(PxClothParticle);

				meshDesc.triangles.data = (void*)primitives.data();
				meshDesc.triangles.count = static_cast<PxU32>(primitives.size() / 3);
				meshDesc.triangles.stride = sizeof(PxU32) * 3;

				_RenderData->mClothes[submesh] = mPhys.LoadCloth(vertices.data(), meshDesc);
			}
			else if (_RenderData->mFormat == "PMX")
			{
				vOffset = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].BaseVertexLocation;
				iOffset = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].StartIndexLocation;

				vSize = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].VertexSize;
				iSize = _RenderData->mGeometry.DrawArgs[_RenderData->mGeometry.meshNames[submesh]].IndexSize;

				// ���� Material�� Vertex�� ������ ���´�.
				vertices.clear();
				primitives.clear();
				vertices.resize(0);
				primitives.resize(0);

				bool hasZero(false), hasOne(false);
				int primCount = 0;

				/////////////////////////////////////////////////////////////////////////
				// �켱 Cloth�� ������� �ʴ� �κ��� ���� indices���� �����Ѵ�
				/////////////////////////////////////////////////////////////////////////
				for (UINT idx = iOffset; idx < iOffset + iSize; idx += 3)
				{
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

				// ����޽����� ���Ǵ� ���ؽ� �ε�������Ʈ�� ����ִ� vertBySubmesh���� weight�� 0.0�� ��쿡 
				// vertBySubmesh���� ���� �� ����.
				// ���� Material�� Vertex�� ������ ���´�.
				RGBQUAD color;
				int uvX, uvY;

				/////////////////////////////////////////////////////////////////////////
				// Cloth�� Attatch�� ���ؽ� �̹����� ���´�
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
						// �ش� ����޽��� ���ؽ� ����Ʈ
						_RenderData->vertBySubmesh[submesh].erase(mRemover);
						mRemover = -1;
					}

					posVector = DirectX::XMLoadFloat3(&_RenderData->vertices[*vIDX].Pos);

					if (_RenderData->mClothWeights[*vIDX] == 0.0f) {
						mRemover = *vIDX;

						continue;
					}

					uvX = (int)((float)width  * _RenderData->vertices[*vIDX].TexC.x);
					uvY = (int)((float)height * (1.0f - _RenderData->vertices[*vIDX].TexC.y));

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
					// ���⿡ PPD2Vertices MAP�� ������
					///////
					if (_RenderData->mClothWeights[*vIDX] != 0.0f)
					{
						_RenderData->srcDynamicVertexSubmesh[submesh].push_back(mOffset);
						_RenderData->dstDynamicVertexSubmesh[submesh].push_back(&_RenderData->vertices[*vIDX].Pos);
					}
					else
					{
						_RenderData->srcFixVertexSubmesh[submesh].push_back(mOffset);
						_RenderData->dstFixVertexSubmesh[submesh].push_back(&_RenderData->vertices[*vIDX].Pos);
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

				// ��� ���ؽ��� ������ ��찡 �ƴϸ�
				size_t vertSize = vertices.size();

				if (vertices.size() > 0)
				{
					// �ּ� ���� ���ؽ�
					// vertices[0].invWeight = 0.0f;

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
				// ��� ���ؽ��� ������ ��� (Cloth�� ���� ��ų �ʿ䰡 ����)
				else
				{
					_RenderData->isCloth[submesh] = false;
					_RenderData->mClothes[submesh] = NULL;
				}
			}
		} // Loop Submesh
	} // Loop GameObjects

	// ClothParticleInfo�� ���ε� �Ǿ��� ����
	PxClothParticleData* ppd = NULL;
	// Cloth Position ���� ������ ���� ����
	Vertex* mClothOffset = NULL;
	UINT vertexSize;

	// ���� �ʱ⿡ Cloth Vertices Update�� ���� ���� �Ǿ�� �ϱ� ������
	// ClothWriteEvent�� On
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
		// Cloth Vertices�� Write Layer�� �� �� ���� ���
		WaitForSingleObject(mClothWriteEvent, INFINITE);

		if (StopThread) break;

		mObj = mGameObjectDatas.begin();

		// Update Cloth Vertices
		while (mObj != mEndObj)
		{
			if (!loopUpdate)	break;

			_RenderData = mObj->second;

			// new float[4 * vSize] ��� submesh VBV�� �ٷ� ���ε� �� �� ������?
			clothPos = _RenderData->vertices.data();

			// ���� �ִϸ��̼��� ���� ������Ʈ��� ��ŵ
			if (_RenderData->mAnimVertex.size() < 1 &&
				_RenderData->mBoneMatrix.size() < 1)
			{
				_RenderData->isDirty = false;
				mObj++;
				continue;
			}

			_RenderData->isDirty = true;

			// ����޽� ������ �ִϸ��̼��� ������Ʈ �ϴ� ��� (cloth)
			for (submeshIDX = 0; submeshIDX < _RenderData->SubmeshCount; submeshIDX++)
			{
				if (_RenderData->isRigidBody[submeshIDX])
				{
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

				if (_RenderData->isCloth[submeshIDX])
				{
					// Adapted Cloth Physx
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

					delta.m128_f32[0] *= 0.003f;
					delta.m128_f32[1] *= 0.003f;
					delta.m128_f32[2] *= 0.003f;

					resQ = delta;

					resQ = DirectX::XMQuaternionNormalize(resQ);

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

					mClothOffset =
						clothPos;
					//clothPos +
					//_RenderData->mDesc[submeshIDX].BaseVertexLocation;
					vertexSize =
						_RenderData->mDesc[submeshIDX].VertexSize;

					mSrcFixIter = _RenderData->srcFixVertexSubmesh[submeshIDX].begin();
					mDstFixIter = _RenderData->dstFixVertexSubmesh[submeshIDX].begin();
					mDstEndFixIter = _RenderData->dstFixVertexSubmesh[submeshIDX].end();

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

					ppd = _RenderData->mClothes[submeshIDX]->lockParticleData(PxDataAccessFlag::eREADABLE);

					while (mDstDynamicIter != mDstEndDynamicIter)
					{
						mSrcIterPos = &ppd->particles[*mSrcDynamicIter].pos;

						//// DELTA
						//{
						//	(*mDstDynamicIter)->x - mSrcIterPos->x;
						//	(*mDstDynamicIter)->y - mSrcIterPos->y;
						//	(*mDstDynamicIter)->z - mSrcIterPos->z;
						//}

						memcpy((*mDstDynamicIter), mSrcIterPos, sizeof(float) * 3);

						mSrcDynamicIter++;
						mDstDynamicIter++;
					}

					ppd->unlock();

				} // if (_RenderData->isCloth[submeshIDX])
			} // for Submesh

			mObj++;
		} // for GameObject

		// Cloth Vertices�� Read Layer�� ����
		ResetEvent(mClothWriteEvent);
		SetEvent(mClothReadEvent);

		// �� ���� ���� ���� �ٽ� ��� 
		if (loop++ == 3)
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
	UINT vertexIDX = 0;
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
		// Cloth Vertices�� Write Layer�� �� �� ���� ���
		WaitForSingleObject(mAnimationWriteEvent, INFINITE);
		if (StopThread) break;

		// Update Cloth Vertices
		obj = mGameObjectDatas.begin();
		objEnd = mGameObjectDatas.end();
		while (obj != objEnd)
		{
			_RenderData = obj->second;

			// ���� �ִϸ��̼��� ���� ������Ʈ��� ��ŵ
			if (_RenderData->mAnimVertex.size() < 1 &&
				_RenderData->mBoneMatrix.size() < 1)
			{
				_RenderData->isDirty = false;
				obj++;
				continue;
			}

			_RenderData->isDirty = true;

			// �ִϸ��̼� ������Ʈ
			_RenderData->isAnim = true;
			_RenderData->isLoop = true;

			// ���� �������� ����ϸ�, �������� ���� �� �� Ʈ���Ÿ� �۵��Ѵ�.
			currentFrame =
				(int)(_RenderData->currentDelayPerSec / _RenderData->durationOfFrame[0]);
			//currentFrame =
			//	1;

			// ���� ���� �������� �ִϸ��̼��� ���̶��?
			if ((_RenderData->endAnimIndex) <= currentFrame) {
				// �ٽ� ���� ���� ����������
				currentFrame = (int)_RenderData->beginAnimIndex;
				_RenderData->currentDelayPerSec = (_RenderData->beginAnimIndex * _RenderData->durationOfFrame[0]);
			}
			// ���� �������� ���� �Ǿ��ٸ�?
			if (_RenderData->currentFrame != currentFrame) {
				_RenderData->currentFrame = currentFrame;
				_RenderData->updateCurrentFrame = true;
			}

			// ���� ������ ������ �ܺ� �ð��� ������Ʈ �Ѵ�.
			residueTime = _RenderData->currentDelayPerSec - (_RenderData->currentFrame * _RenderData->durationOfFrame[0]);

			// �ܺ� �ð� ������Ʈ
			_RenderData->mAnimResidueTime = residueTime;

			////
			if (obj->second->mFormat == "FBX") {
				VertexPos = obj->second->vertices.data();
				mVertexOffset = NULL;
				vertexSize = 0;

				for (submeshIDX = 0; submeshIDX < (int)obj->second->SubmeshCount; submeshIDX++)
				{
					// ���� �ִϸ��̼� Vertex�� �����Ǿ� �÷��� �Ǵ� Submesh���
					if (!obj->second->isCloth[submeshIDX])
					{
						// ���� ���� �������� �ִϸ��̼� ������ NULL�̸� 0���� ������ ä���.
						if (obj->second->mAnimVertexSize[obj->second->currentFrame][submeshIDX] == 0)
							continue;

						// �� ���� ������Ʈ�� VertexList �� �ش� Submesh�� ���� �������� �ҷ���
						mVertexOffset =
							VertexPos +
							obj->second->mDesc[submeshIDX].BaseVertexLocation;
						// Submesh�� �ش��ϴ� Vertex ����
						vertexSize =
							obj->second->mDesc[submeshIDX].VertexSize;

						// �ִϸ��̼��� �ֽ�ȭ �� ���ؽ� ���� �ּ�
						animPos = obj->second->mAnimVertex[obj->second->currentFrame][submeshIDX];
						// �ִϸ��̼� ���ؽ� ������
						animVertSize = obj->second->mAnimVertexSize[obj->second->currentFrame][submeshIDX];

						vcount = 0;
						for (vertexIDX = 0; vertexIDX < vertexSize; vertexIDX++)
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

			if (_RenderData->mMorphDirty.size() > 0)
			{
				for (mIDX = 0; mIDX < _RenderData->mMorphDirty.size(); mIDX++)
				{
					mMorph = _RenderData->mMorph[_RenderData->mMorphDirty[mIDX]];
					weight = mMorph.mVertWeight;

					for (i = 0; i < mMorph.mVertIndices.size(); i++)
					{
						DesctinationPos = &_RenderData->vertices[mMorph.mVertIndices[i]].Pos;
						DefaultPos = _RenderData->mModel.vertices[mMorph.mVertIndices[i]].position;

						DesctinationPos->x = DefaultPos[0] + mMorph.mVertOffset[i][0] * weight;
						DesctinationPos->y = DefaultPos[1] + mMorph.mVertOffset[i][1] * weight;
						DesctinationPos->z = DefaultPos[2] + mMorph.mVertOffset[i][2] * weight;
					}
				}
				
				_RenderData->mMorphDirty.resize(0);
			}

			obj++;
		}

		// Cloth Vertices�� Read Layer�� ����
		ResetEvent(mAnimationWriteEvent);
		SetEvent(mAnimationReadEvent);
	}

	return (DWORD)(0);
}

// Client
bool BoxApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// _Awake (������Ʈ �ε�, �ؽ���, ���׸��� ����)
	_Awake(this);

	mCamera.SetPosition(0.0f, 0.0f, -15.0f);

	mBlurFilter = std::make_unique<BlurFilter>(md3dDevice.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	mSobelFilter = std::make_unique<SobelFilter>(md3dDevice.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	mOffscreenRT = std::make_unique<RenderTarget>(md3dDevice.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(), 2048, 2048);
	mDrawTexture = std::make_unique<DrawTexture>(md3dDevice.Get(), 2048, 2048, DXGI_FORMAT_R8G8B8A8_UNORM);

	BuildRootSignature();
	BuildBlurRootSignature();
	BuildSobelRootSignature();
	BuildDrawMapSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();

	BuildPSO();

	// ���� ������ �ؽ���, ���׸��� ���ε� �Ǿ� �־����.

	BuildRenderItem();
	BuildFrameResource();

	// Create Physx Synchronization Thread
	hAnimThread = CreateThread(NULL, 0, ThreadAnimFunc, NULL, 0, &hAnimThreadID);
	hClothPhysxThread = CreateThread(NULL, 0, ThreadClothPhysxFunc, NULL, 0, &hClothPhysxThreadID);

	mClothReadEvent = CreateEvent(nullptr, false, false, nullptr);
	mClothWriteEvent = CreateEvent(nullptr, false, false, nullptr);
	mAnimationReadEvent = CreateEvent(nullptr, false, false, nullptr);
	mAnimationWriteEvent = CreateEvent(nullptr, false, false, nullptr);

	for (UINT i = 0; i < numThread; i++) {
		recordingDoneEvents[i] = CreateEvent(nullptr, false, false, nullptr);
		renderTargetEvent[i] = CreateEvent(nullptr, false, false, nullptr);

		drawThreads[i] = CreateThread(
			nullptr,
			0,
			this->DrawThread,
			reinterpret_cast<LPVOID>(i),
			0,
			ThreadIndex[i]
		);

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
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 2;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())
	));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
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
}

void BoxApp::Update(const GameTimer& gt)
{
	// Cloth�� ������Ʈ �� �� ���� ���
	WaitForSingleObject(mClothReadEvent, INFINITE);
	WaitForSingleObject(mAnimationReadEvent, INFINITE);

	_Update(gt);

	OnKeyboardInput(gt);
	AnimationMaterials(gt);
	UpdateInstanceData(gt);
	// UpdateMaterialBuffer(gt);
	UpdateMainPassCB(gt);

	UpdateAnimation(gt);
}

void BoxApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime() * 50.0f;

	if (GetAsyncKeyState('Q') & 0x8000)
		mCamera.Fly(-dt);
	else if (GetAsyncKeyState('E') & 0x8000)
		mCamera.Fly(dt);

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(dt);
	else if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-dt);
	else if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(dt);

	if (GetAsyncKeyState('Z') & 0x8000)
		mFilterCount -= 1;
	else if (GetAsyncKeyState('X') & 0x8000)
		mFilterCount += 1;

	//// ���� ĳ���͸� ����ٴϴ� ī�޶� ������ ���

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

}

void BoxApp::UpdateInstanceData(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	XMMATRIX world, texTransform, invWorld, viewToLocal;
	InstanceData data;

	//const PhyxResource* pr = nullptr;
	UINT visibleInstanceCount = 0;

	UINT i;
	// Game Index
	int gIdx = 0;

	// (���� ������Ʈ ���� * ���� ����޽�) ���� ��ŭ�� ���׸��� ������ �Ϸķ� ���� ��Ű�� ���� �ε��� 
	int MatIDX = 0;
	int LightIDX = 0;

	// UpdateMaterialBuffer
	// Material Count == Submesh Count
	// �ν��Ͻ����� �����ϱ� ���� �ν��Ͻ��� ��� �� Material�� �����Ѵ�.
	MaterialData matData;
	XMMATRIX matTransform;

	//////
	// 이후 각 라이트마다의 경계구로 변형 요망
	//////
	mSceneBounds.Center = XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	mSceneBounds.Radius = 100;

	std::vector<std::pair<std::string, Material>>::iterator& matIDX = mMaterials.begin();
	std::vector<std::pair<std::string, Material>>::iterator& matEnd = mMaterials.end();
	for (; matIDX != matEnd; matIDX++)
	{
		matTransform = XMLoadFloat4x4(&matIDX->second.MatTransform);

		// Initialize new MaterialDatas
		matData.DiffuseAlbedo = matIDX->second.DiffuseAlbedo;
		matData.FresnelR0 = matIDX->second.FresnelR0;
		matData.Roughness = matIDX->second.Roughness;

		XMStoreFloat4x4(
			&matData.MatTransform,
			XMMatrixTranspose(matTransform)
		);

		matIDX->second.MatInstIndex = MatIDX;

		// copy on Descriptor Set
		MaterialBuffer->CopyData(MatIDX++, matData);

		// matIDX->second.NumFramesDirty--;
	}

	ObjectData* obj = NULL;
	PhyxResource* pr = nullptr;
	std::unordered_map<std::string, ObjectData*>::iterator objIDX;
	std::unordered_map<std::string, ObjectData*>::iterator objEnd;
	std::vector<LightDataConstants>::iterator lightDataIDX;
	std::vector<Light>::iterator lightIDX;
	std::vector<Light>::iterator lightEnd = mLights.end();

	// 오브젝트가 프러스텀에 걸쳐있는지 여부를 확인하는 콜라이더 크기
	XMFLOAT4X4 boundScale = MathHelper::Identity4x4();

	Light* l = nullptr;
	float* mObjectPos = pr->Position;
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

	mRenderTasks.resize(mGameObjectDatas.size());
	mRenderInstTasks.resize(mGameObjectDatas.size());

	objIDX = mGameObjectDatas.begin();
	objEnd = mGameObjectDatas.end();
	for (; objIDX != objEnd; objIDX++)
	{
		obj = (*objIDX).second;
		mRenderTasks[gIdx] = obj;

		mRenderInstTasks[gIdx].clear();

		for (i = 0; i < obj->InstanceCount; ++i)
		{
			// getSyncDesc
			pr = &obj->mPhyxResources[i];

			// Collider Box 또한 이동
			obj->Bounds[i].Center.x = pr->Position[0];
			obj->Bounds[i].Center.y = pr->Position[1];
			obj->Bounds[i].Center.z = pr->Position[2];

			world = XMLoadFloat4x4(&obj->mInstances[i].World);

			texTransform = XMLoadFloat4x4(&obj->mInstances[i].TexTransform);

			invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);
			viewToLocal = XMMatrixMultiply(invView, invWorld);

			BoundingFrustum localSpaceFrustum;
			// 카메라의 시야에 ((worldMat)^-1 * (viewMat)^-1)를 곱하여, 오브젝트 관점에서의 프러스텀으로 변형
			// 후, 최종 결과의 프러스텀 내 오브젝트가 존재한다면 실행
			mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

			// 프러스텀 내에 존재하는 오브젝트인지 확인 후, true라면 오브젝트 정보를 적재
			if (
					(obj->mRenderType == (UINT)ObjectData::RenderType::_SKY_FORMAT_RENDER_TYPE) ||
					(obj->mRenderType == (UINT)ObjectData::RenderType::_DEBUG_BOX_TYPE) ||
					(localSpaceFrustum.Contains(obj->Bounds[i]) != DirectX::DISJOINT)
				)
			{
				XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
				data.MaterialIndex = obj->mInstances[i].MaterialIndex;

				// renderInstCounts = {5, 4, 2, 6, 7, 11 .....} �� ����� �ν��Ͻ� Index�� 
				// renderInstIndex = {{0, 1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10}, {11, 12, ..., 16}, ...}
				// ���� ���������� �þ��.
				InstanceBuffer->CopyData(visibleInstanceCount, data);

				if (mLightDatas.size() > 0)
				{
					// Light Update
					lightIDX = mLights.begin();
					lightDataIDX = mLightDatas.begin();

					mObjectPos = pr->Position;
					distance = 0.0f;

					// 충돌 라이트를 최신화 하기 위해 clear한다.
					int LightSize = 0;
					while (lightIDX != lightEnd)
					{
						// 라이트 무브먼트 테스트
						{
							/*(*lightIDX).Position.x += gt.DeltaTime() * 0.5f;
							(*lightIDX).Position.z += gt.DeltaTime() * 0.5f;
							(*lightIDX).Position.y += gt.DeltaTime() * 0.5f;*/
						}

						// 오브젝트의 포지션을 얻어온다.

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
						XMStoreFloat3(&(*lightIDX).mLightPosW, lightPos);

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

				mRenderInstTasks[gIdx].push_back(visibleInstanceCount);

				visibleInstanceCount++;
				lightDataIDX++;
			}
		}

		// ���� ������Ʈ�� �ν��Ͻ� ������ �˻��ϱ� ���� �ε��� ����Ī
		gIdx++;
	}

	mInstanceCount = visibleInstanceCount;
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

		XMStoreFloat4x4(&mMainPassCB.view, XMMatrixTranspose(view));
		XMStoreFloat4x4(&mMainPassCB.Invview, XMMatrixTranspose(invView));
		XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
		XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
		XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

		mMainPassCB.EyePosW = mCamera.GetPosition3f();
		//mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
		//mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
		mMainPassCB.NearZ = 1.0f;
		mMainPassCB.FarZ = 1000.0f;
		mMainPassCB.TotalTime = gt.TotalTime();
		mMainPassCB.DeltaTime = gt.DeltaTime();

		PassCB->CopyData(0, mMainPassCB);
	}
}

void BoxApp::UpdateAnimation(const GameTimer& gt)
{
	RenderItem* obj = NULL;
	ObjectData* objData = NULL;

	float delta = gt.DeltaTime();
	std::list<RenderItem*>::iterator objIDX = mGameObjects.begin();
	std::list<RenderItem*>::iterator objEnd = mGameObjects.end();
	for (; objIDX != objEnd; objIDX++)
	{
		obj = *objIDX;
		objData = mGameObjectDatas[obj->mName];

		// ���� �ִϸ��̼� ���� �ð��� ������Ʈ �Ѵ�.
		objData->currentDelayPerSec += delta;
	}
}

void BoxApp::InitSwapChain(int numThread)
{
	{
		// commandAlloc, commandList의 재사용을 위한 리셋
		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		for (int i = 0; i < 3; i++)
			ThrowIfFailed(mMultiCmdListAlloc[i]->Reset());

		ThrowIfFailed(
			mCommandList->Reset(
				mDirectCmdListAlloc.Get(),
				mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE].Get()
			)
		);
		for (int i = 0; i < 3; i++)
			ThrowIfFailed(mMultiCommandList[i]->Reset(
				mMultiCmdListAlloc[i].Get(), 
				mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE].Get()
			)
		);

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
				obj->vertices.data(),
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

	// Opaque 아이템을 렌더 하여 DepthMap을 그린다.
	{
		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mShadowMap->Resource(),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_STATE_DEPTH_WRITE
			)
		);

		mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
		mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

		UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

		mCommandList->ClearDepthStencilView (
			mShadowMap->Dsv(),
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f,
			0,
			0,
			nullptr
		);

		// Set null render target because we are only going to draw to
		// depth buffer.  Setting a null render target will disable color writes.
		// Note the active PSO also must specify a render target count of 0.
		mCommandList->OMSetRenderTargets(
			0,
			nullptr,
			false,
			&mShadowMap->Dsv()
		);

		ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		// Select Descriptor Buffer Index
		mCommandList->SetGraphicsRootConstantBufferView (
			0,
			PassCB->Resource()->GetGPUVirtualAddress()
		);

		mCommandList->SetPipelineState(mPSOs[ObjectData::RenderType::_OPAQUE_SHADOW_MAP_RENDER_TYPE].Get());

		// Instance Count
		D3D12_GPU_VIRTUAL_ADDRESS objectCB = InstanceBuffer->Resource()->GetGPUVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS lightCB = LightBufferCB->Resource()->GetGPUVirtualAddress();

		// For each render item...
		size_t instAcc = 0;
		ObjectData* obj = NULL;
		for (size_t i = 0; i < mGameObjectDatas.size(); ++i)
		{
			obj = (std::next(mGameObjectDatas.begin(), i))->second;

			if (!obj->isDrawShadow)
			{
				instAcc++;
				continue;
			}

			for (size_t j = 0; j < obj->InstanceCount; j++)
			{
				// Instance
				D3D12_GPU_VIRTUAL_ADDRESS objCBAddress =
					objectCB + instAcc * sizeof(InstanceData);
				D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress =
					lightCB + instAcc * d3dUtil::CalcConstantBufferByteSize(sizeof(LightDataConstants));

				mCommandList->IASetVertexBuffers(0, 1, &obj->mGeometry.VertexBufferView());
				mCommandList->IASetIndexBuffer(&obj->mGeometry.IndexBufferView());
				mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				mCommandList->SetGraphicsRootConstantBufferView(
					1,
					lightCBAddress
				);
				mCommandList->SetGraphicsRootShaderResourceView (
					3,
					objCBAddress
				);

				SubmeshGeometry* sg = nullptr;
				for (size_t k = 0; k < obj->SubmeshCount; k++)
				{
					sg = &obj->mGeometry.DrawArgs[
						obj->mGeometry.meshNames[k].c_str()
					];

					mCommandList->DrawIndexedInstanced(
						sg->IndexSize,
						obj->InstanceCount,
						sg->StartIndexLocation,
						sg->BaseVertexLocation,
						0
					);
				}

				instAcc++;
			}
		}

		// Change back to GENERIC_READ so we can read the texture in a shader.
		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mShadowMap->Resource(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_GENERIC_READ
			)
		);
	}

	// Compute Draw Texture
	if (mDrawTexture->isDirty)
	{
		mDrawTexture->isDirty = false;

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

			if (!obj->isDirty)
				continue;

			obj->isDirty = false;

			d3dUtil::UpdateDefaultBuffer(
				mCommandList.Get(),
				obj->vertices.data(),
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
		mCommandList->Close();

		ID3D12CommandList* commands[] = {
			mCommandList.Get()
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

DWORD WINAPI BoxApp::DrawThread(LPVOID temp)
{
	UINT ThreadIDX = reinterpret_cast<UINT>(temp);

	UINT instanceIDX = 0;
	SubmeshGeometry* sg = nullptr;

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

	// Instance Offset
	uint32_t _gInstOffset = 0;

	// loop Resource
	UINT i, j;

	// RenderInstTasks Offset
	UINT _taskOffset;
	// RenderInstTasks End Point
	UINT _taskEnd;

	// Extracted Render Items.
	ObjectData* obj = nullptr;
	int amountOfTask = 0;
	int continueIDX = -1;
	UINT taskIDX = 0;

	// GameObjectIndices
	std::vector<struct PieceOfRenderItemByThread> mGameObjectIndices;

	UINT loop = 0;
	UINT end = 0;

	int MatIDX = 0;
	int gameIDX = 0;

	while (ThreadIDX < numThread)
	{
		// Draw에서 Update를 마치고 Draw를 시작하라고 지시 할 때 까지 대기
		WaitForSingleObject(renderTargetEvent[ThreadIDX], INFINITE);

		// Initialization
		continueIDX = -1;
		taskIDX = 0;
		instanceIDX = 0;

		// Divid RenderItem Geometrys by ThreadIDX
		// This Situation will be make Minimalized saving to VBV buffer.
		
		// 그려야 할 인스턴스 리스트를 삼등분 하여 나누어 렌더링

		// 시작 인스턴스 인덱스 
		_taskOffset = (mInstanceCount / 3) * ThreadIDX;

		// 끝 인스턴스 인덱스
		if (ThreadIDX != 2)
			_taskEnd = (mInstanceCount / 3) * (ThreadIDX + 1);
		else
			_taskEnd = mInstanceCount;

		// 렌더링 할 오브젝트가 없다면 스킵
		if (_taskEnd - _taskOffset == 0)
		{
			ThrowIfFailed(mMultiCommandList[ThreadIDX]->Close());

			ResetEvent(renderTargetEvent[ThreadIDX]);
			SetEvent(recordingDoneEvents[ThreadIDX]);

			return 1;
		}

		// 하나의 스레드가 그려야 할 인스턴스의 개수만큼 공간을 할당함.
		mGameObjectIndices.resize(_taskEnd - _taskOffset);

		UINT count = 0;
		UINT index = 0;
		// _taskEnd
		for (i = 0; i < mRenderInstTasks.size(); i++)
		{
			for (j = 0; j < mRenderInstTasks[i].size(); j++)
			{
				if (_taskOffset <= count && count < _taskEnd)
				{
					mGameObjectIndices[index].mObjPTR = mRenderTasks[i];
					mGameObjectIndices[index].mInstanceOffset = mRenderInstTasks[i][j];

					index++;
				}
				count++;
			}
		}

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

			// ��ũ���� ���ε�
			ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
			mMultiCommandList[ThreadIDX]->SetDescriptorHeaps(
				_countof(descriptorHeaps), 
				descriptorHeaps
			);

			// �ñ״��� (��ũ���� ��) ���ε�
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
				// DrawTexture
				mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
					11,
					mDrawTexture->OutputSrv()
				);
			}

			mMultiCommandList[ThreadIDX]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		// �������� �ε��� ������ Render Item�� �����Ͽ� CommandList�� ���ε�. 
		{
			continueIDX = -1;
			MatIDX = 0;

			for (taskIDX = 0; taskIDX < (_taskEnd - _taskOffset); taskIDX++)
			{
				obj				= mGameObjectIndices[taskIDX].mObjPTR;
				_gInstOffset	= mGameObjectIndices[taskIDX].mInstanceOffset;

				UINT vertSize = obj->vertices.size();
				if (!obj || !vertSize) break;

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
				instanceCBAddress		= instanceCB	+ _gInstOffset * sizeof(InstanceData);
				lightCBAddress			= lightCB		+ _gInstOffset * d3dUtil::CalcConstantBufferByteSize(sizeof(LightDataConstants));
				pmxBoneCBAddress		= pmxBoneCB;
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
					}
					mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
						3,
						instanceCBAddress
					);
					mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
						5,
						pmxBoneCBAddress
					);
				}

				{
					// count of submesh
					for (j = 0; j < obj->SubmeshCount; j++)
					{
						if (obj->Mat.size() == 0 && obj->SkyMat.size() == 0)
							throw std::runtime_error("");

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

						if (obj->Mat.size() > 0)
						{
							/*matCBAddress = matCB + obj->Mat[j]->MatInstIndex * sizeof(MaterialData);*/
							matCBAddress = matCB;

							// Move to Current Stack Pointer
							tex.Offset(
								obj->Mat[j]->DiffuseSrvHeapIndex,
								mCbvSrvUavDescriptorSize
							);

							maskTex.Offset(
								obj->Mat[j]->MaskSrvHeapIndex >= 0 ? obj->Mat[j]->MaskSrvHeapIndex : 0,
								mCbvSrvUavDescriptorSize
							);

							noiseTex.Offset(
								obj->Mat[j]->NoiseSrvHeapIndex >= 0 ? obj->Mat[j]->NoiseSrvHeapIndex : 0,
								mCbvSrvUavDescriptorSize
							);

							skyTex.Offset(
								0,
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

							skyTex.Offset(
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
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								9,
								skyTex
							);
						}

						sg = &obj->mGeometry.DrawArgs[
							obj->mGeometry.meshNames[
								j
							].c_str()
						];

						mMultiCommandList[ThreadIDX]->DrawIndexedInstanced(
							sg->IndexSize,							// �� ������Ʈ���� �ε��� ����
							obj->InstanceCount,
							sg->StartIndexLocation,
							sg->BaseVertexLocation,
							0
						);
					}
				}

				// ���� Geometry�� offset ����
				// _gInstOffset	+= mGameObjects[gameIDX]->InstanceCount;
				_gInstOffset += 1;
			}
			_gInstOffset = 0;
		}

		// DebugBox를 그리는 구간.
		{
			continueIDX = -1;
			MatIDX = 0;

			for (taskIDX = 0; taskIDX < (_taskEnd - _taskOffset); taskIDX++)
			{
				obj = mGameObjectIndices[taskIDX].mObjPTR;
				_gInstOffset = mGameObjectIndices[taskIDX].mInstanceOffset;

				if (!obj || !obj->isDebugBox)	continue;

				mMultiCommandList[ThreadIDX]->SetPipelineState(
					mPSOs[ObjectData::RenderType::_DEBUG_BOX_TYPE].Get()
				);

				mMultiCommandList[ThreadIDX]->IASetVertexBuffers(
					0,
					1,
					&obj->mDebugBoxData->mGeometry.VertexBufferView()
				);
				mMultiCommandList[ThreadIDX]->IASetIndexBuffer(
					&obj->mDebugBoxData->mGeometry.IndexBufferView()
				);

				// Move to Current Stack Pointer
				instanceCBAddress = instanceCB + _gInstOffset * sizeof(InstanceData);
				mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
					3,
					instanceCBAddress
				);

				sg = &obj->mDebugBoxData->mGeometry.DrawArgs[
					obj->mDebugBoxData->mGeometry.meshNames[0]
				];

				mMultiCommandList[ThreadIDX]->DrawIndexedInstanced(
					sg->IndexSize,
					obj->mDebugBoxData->InstanceCount,
					sg->StartIndexLocation,
					sg->BaseVertexLocation,
					0
				);

				_gInstOffset += 1;
			}
			_gInstOffset = 0;
		}

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
	InitSwapChain(numThread);

	// �� RenderItem���� ��Ƽ ���������� �׸���.
	{
		SetEvent(renderTargetEvent[0]);
		SetEvent(renderTargetEvent[1]);
		SetEvent(renderTargetEvent[2]);

		WaitForMultipleObjects(3, recordingDoneEvents, true, INFINITE);

		ResetEvent(recordingDoneEvents[0]);
		ResetEvent(recordingDoneEvents[1]);
		ResetEvent(recordingDoneEvents[2]);
	}

	// CommandAllocationList�� ����� RenderItem���� CommnadQ�� ������.
	{
		// Add the command list to the queue for execution.
		ID3D12CommandList* cmdsLists[] = { 
			mMultiCommandList[0].Get(), 
			mMultiCommandList[1].Get(), 
			mMultiCommandList[2].Get() 
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

		// commandAlloc, commandList�� ���� �ϱ� ���� ��������
		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(
			mDirectCmdListAlloc.Get(), 
			mPSOs[ObjectData::RenderType::_POST_PROCESSING_PIPELINE].Get())
		);

		// ��ũ���� ���ε�
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

	{
		mCommandList->Close();

		// Add the command list to the queue for execution.
		ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
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
			XMFLOAT4X4 P = mCamera.GetProj4x4f();

			// Compute picking ray in view space.
			// [0, 600] -> [-1, 1] [0, 800] -> [-1, 1]
			float vx = (x - mLastMousePos.x) / (float)(mClientWidth);
			float vy = (y - mLastMousePos.y) / (float)(mClientHeight);

			//// z축을 노말라이즈
			//vx = vx / P(0, 0);
			//vy = vy / P(1, 1);

			mDrawTexture->isDirty = true;
			
			mDrawTexture->Position.x =
				mDrawTexture->Origin.x +
				vx;
			mDrawTexture->Position.y =
				mDrawTexture->Origin.y -
				vy;
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
	CD3DX12_DESCRIPTOR_RANGE drawTexTable;
	drawTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 2);

	// Root parameter can be a table, root descriptor or root constants.
	std::array<CD3DX12_ROOT_PARAMETER, 12> slotRootParameter;

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
	// Draw Textures
	slotRootParameter[11].InitAsDescriptorTable(1, &drawTexTable, D3D12_SHADER_VISIBILITY_PIXEL);

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

void BoxApp::BuildDrawMapSignature()
{
	// Post Processing 전 이미지
	CD3DX12_DESCRIPTOR_RANGE srvTable0;
	srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable0;
	uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstants(12, 0);
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
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
	const int postProcessDescriptorCount	= 2;
	const int shdowMapDescriptorCount		= 2;
	const int drawTextureDescriptorCount	= mDrawTexture->DescriptorCount();

	D3D12_DESCRIPTOR_HEAP_DESC SrvUavHeapDesc;
	SrvUavHeapDesc.NumDescriptors = 
		texCount					+ 
		blurDescriptorCount			+ 
		sobelDescriptorCount		+ 
		postProcessDescriptorCount	+ 
		shdowMapDescriptorCount		+ 
		drawTextureDescriptorCount;

	SrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	SrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	SrvUavHeapDesc.NodeMask = 0;
	ThrowIfFailed(
		md3dDevice->CreateDescriptorHeap(
			&SrvUavHeapDesc,
			IID_PPV_ARGS(&mSrvDescriptorHeap)
		)
	);

	//
	// �ؽ��İ� ����� ���� �����Ѵ�.
	//

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

	mDescOffset += drawTextureDescriptorCount;

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

	mDescOffset += shdowMapDescriptorCount;

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
		mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		0,
		(UINT)mRtvDescriptorSize
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	for (int texIDX = 0; texIDX < mTextures.size(); texIDX++)
	{
		if (!mTextures[mTextureList[texIDX]].isCube)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = -1;
			srvDesc.Format = mTextures[mTextureList[texIDX]].Resource->GetDesc().Format;
			md3dDevice->CreateShaderResourceView(mTextures[mTextureList[texIDX]].Resource.Get(), &srvDesc, hDescriptor);
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = mTextures[mTextureList[texIDX]].Resource->GetDesc().MipLevels;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			srvDesc.Format = mTextures[mTextureList[texIDX]].Resource->GetDesc().Format;
			md3dDevice->CreateShaderResourceView(mTextures[mTextureList[texIDX]].Resource.Get(), &srvDesc, hDescriptor);
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
	mShaders["pmxFormatVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", pmxFormatDefines, "VS", "vs_5_1");

	mShaders["pix"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["debugPS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", debugDefines, "PS", "ps_5_1");
	mShaders["drawTexPS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", drawTexDefines, "PS", "ps_5_1");

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
}

void BoxApp::RecursionFrameResource(
	RenderItem* obj, 
	int& InstanceNum,
	int& SubmeshNum,
	int& BoneNum
)
{
	InstanceNum += (2 * (obj->InstanceCount));
	SubmeshNum += obj->SubmeshCount;

	if (obj->mFormat == "PMX")
		BoneNum = mGameObjectDatas[obj->mName]->mModel.bone_count;

	auto& objIDX = obj->mChilds.begin();
	auto& objEnd = obj->mChilds.end();
	for (; objIDX != objEnd; objIDX++) {
		RecursionFrameResource(
			(*objIDX).second, 
			InstanceNum, 
			SubmeshNum, 
			BoneNum
		);
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

	// 비어있는 객체일 시 패스
	if (obj->SubmeshCount > 0)
	{
		if (obj->mFormat != "PMX")
		{
			// CPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				obj->mGeometry.VertexBufferByteSize,
				&obj->mGeometry.VertexBufferCPU
			));
			CopyMemory(
				obj->mGeometry.VertexBufferCPU->GetBufferPointer(),
				obj->vertices.data(),
				obj->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				obj->mGeometry.IndexBufferByteSize,
				&obj->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				obj->mGeometry.IndexBufferCPU->GetBufferPointer(),
				obj->indices.data(),
				obj->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			obj->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				obj->vertices.data(),
				obj->mGeometry.VertexBufferByteSize,
				obj->mGeometry.VertexBufferUploader);

			// GPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			obj->mGeometry.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				obj->indices.data(),
				obj->mGeometry.IndexBufferByteSize,
				obj->mGeometry.IndexBufferUploader);

			obj->mGeometry.VertexByteStride = sizeof(Vertex);
			obj->mGeometry.IndexFormat = DXGI_FORMAT_R32_UINT;
		} // (go->mFormat != "PMX")

		else if (obj->mFormat == "PMX")
		{
			pmx::PmxVertexSkinningBDEF1* BDEF1 = NULL;
			pmx::PmxVertexSkinningBDEF2* BDEF2 = NULL;
			pmx::PmxVertexSkinningBDEF4* BDEF4 = NULL;

			pmx::PmxVertex* mFromVert = obj->mModel.vertices.get();
			Vertex* mToVert;

			int vSize = obj->mModel.vertex_count;
			obj->vertices.resize(vSize);

			for (int vLoop = 0; vLoop < vSize; vLoop++)
			{
				mToVert = &obj->vertices[vLoop];

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
				obj->vertices.data(),
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
				obj->vertices.data(),
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
				debugBoxData->vertices.data(),
				debugBoxData->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				debugBoxData->mGeometry.IndexBufferByteSize,
				&debugBoxData->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				debugBoxData->mGeometry.IndexBufferCPU->GetBufferPointer(),
				debugBoxData->indices.data(),
				debugBoxData->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			debugBoxData->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				debugBoxData->vertices.data(),
				debugBoxData->mGeometry.VertexBufferByteSize,
				debugBoxData->mGeometry.VertexBufferUploader
			);

			// GPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			debugBoxData->mGeometry.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				debugBoxData->indices.data(),
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

	newGameObjects->ObjCBIndex = 0;
	newGameObjects->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	newGameObjects->InstanceCount = instance;

	newGameObjects->isDirty.resize(instance);

	ObjectData* obj = new ObjectData();

	obj->mName = Name;
	obj->mPhyxResources.resize(instance);
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

		mPhys.BindObjColliber(
			staticObj,
			&obj->mPhyxResources[i]
		);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = obj;

	return newGameObjects;
}


RenderItem* BoxApp::CreateKinematicGameObject(std::string Name, int instance)
{
	RenderItem* newGameObjects = new RenderItem();
	PxRigidDynamic* dynamicObj = nullptr;
	PxShape* sphere = nullptr;

	sphere = mPhys.CreateSphere(1.0f);

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

	// Create Collider Body
	//phys.CreateBox(1, 1, 1);

	newGameObjects->ObjCBIndex = 0;
	newGameObjects->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	newGameObjects->InstanceCount = instance;

	newGameObjects->isDirty.resize(instance);

	ObjectData* obj = new ObjectData();

	obj->mName = Name;
	obj->mPhyxResources.resize(instance);
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

		dynamicObj = mPhys.CreateKinematic(PxTransform(PxVec3(0, 0, 0)), sphere, 1, PxVec3(0, 0, 0));
		obj->mPhyxRigidBody.push_back(dynamicObj);

		mPhys.BindObjColliber(
			dynamicObj,
			&obj->mPhyxResources[i]
		);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = obj;

	return newGameObjects;
}

RenderItem* BoxApp::CreateDynamicGameObject(std::string Name, int instance)
{
	RenderItem* newGameObjects = new RenderItem();
	PxRigidDynamic* dynamicObj = nullptr;

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

	newGameObjects->ObjCBIndex = 0;
	newGameObjects->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	newGameObjects->InstanceCount = instance;

	newGameObjects->isDirty.resize(instance);

	ObjectData* obj = new ObjectData();

	obj->mName = Name;
	obj->mPhyxResources.resize(instance);
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
		obj->mPhyxRigidBody.push_back(dynamicObj);

		mPhys.BindObjColliber(
			dynamicObj,
			&obj->mPhyxResources[i]
		);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = obj;

	mGameObjectDatas[Name]->InstanceCount = instance;

	return newGameObjects;
}

void BoxApp::CreateBoxObject(
	std::string Name,
	std::string textuerName,
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

	r->mFormat = "";
	r->isDrawShadow = isDrawShadow;
	r->isDrawTexture = isDrawTexture;

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

		obj->mPhyxResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	if (obj->mInstances.size() == 0)
		throw std::runtime_error("Instance Size must bigger than 0.");

	for (UINT inst = 0; inst < r->InstanceCount; inst++)
	{
		obj->Bounds[inst].Transform(obj->Bounds[inst], mWorldMat);

		obj->mPhyxResources[inst].Position[0]	= position.x;
		obj->mPhyxResources[inst].Position[1]	= position.y;
		obj->mPhyxResources[inst].Position[2]	= position.z;
		obj->mPhyxResources[inst].Rotation[0]	= rotation.x;
		obj->mPhyxResources[inst].Rotation[1]	= rotation.y;
		obj->mPhyxResources[inst].Rotation[2]	= rotation.z;
		obj->mPhyxResources[inst].Scale[0]		= scale.x;
		obj->mPhyxResources[inst].Scale[1]		= scale.y;
		obj->mPhyxResources[inst].Scale[2]		= scale.z;

		//memcpy(obj->mPhyxResources[inst].Position, &position, sizeof(float) * 3);
	}

	obj->mName = r->mName;
	obj->mFormat = r->mFormat;
	obj->mRenderType = renderType;

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount += 1;
	obj->mDesc.resize(obj->SubmeshCount);

	obj->isCloth.push_back(false);
	obj->isRigidBody.push_back(false);

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	obj->mDesc[0].StartIndexLocation = boxSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = boxSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = boxSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = boxSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs[Name.c_str()] = boxSubmesh;
	obj->mGeometry.DrawArgs[Name.c_str()].textureName = (textuerName.c_str());
	obj->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = obj->vertices.size();
	XMVECTOR posV;

	obj->vertices.resize(startV + Box.Vertices.size());

	Vertex* v = obj->vertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->indices.insert(
		obj->indices.end(),
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

	r->mFormat = "";
	r->isDrawShadow = isDrawShadow;
	r->isDrawTexture = isDrawTexture;

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

		obj->mPhyxResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	if (obj->mInstances.size() == 0)
		throw std::runtime_error("Instance Size must bigger than 0.");

	for (UINT inst = 0; inst < r->InstanceCount; inst++)
	{
		obj->Bounds[inst].Transform(obj->Bounds[inst], mWorldMat);

		obj->mPhyxResources[inst].Position[0]	= position.x;
		obj->mPhyxResources[inst].Position[1]	= position.y;
		obj->mPhyxResources[inst].Position[2]	= position.z;
		obj->mPhyxResources[inst].Rotation[0]	= rotation.x;
		obj->mPhyxResources[inst].Rotation[1]	= rotation.y;
		obj->mPhyxResources[inst].Rotation[2]	= rotation.z;
		obj->mPhyxResources[inst].Scale[0]		= scale.x;
		obj->mPhyxResources[inst].Scale[1]		= scale.y;
		obj->mPhyxResources[inst].Scale[2]		= scale.z;

		//memcpy(obj->mPhyxResources[inst].Position, &position, sizeof(float) * 3);
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
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	sphereSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	sphereSubmesh.IndexSize = (UINT)Sphere.Indices32.size();
	sphereSubmesh.VertexSize = (UINT)Sphere.Vertices.size();

	obj->mDesc[0].StartIndexLocation = sphereSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = sphereSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = sphereSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = sphereSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs[Name.c_str()] = sphereSubmesh;
	obj->mGeometry.DrawArgs[Name.c_str()].textureName = textuerName.c_str();
	obj->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = obj->vertices.size();
	XMVECTOR posV;

	obj->vertices.resize(startV + Sphere.Vertices.size());

	Vertex* v = obj->vertices.data();
	for (size_t i = 0; i < (Sphere.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Sphere.Vertices[i].Position);

		v[i + startV].Normal = Sphere.Vertices[i].Normal;
		v[i + startV].TexC = Sphere.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->indices.insert(
		obj->indices.end(), 
		std::begin(Sphere.Indices32),
		std::end(Sphere.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	obj->mGeometry.IndexSize			+= (UINT)Sphere.Indices32.size();

	obj->mGeometry.VertexBufferByteSize	+= (UINT)Sphere.Vertices.size() * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize	+= (UINT)Sphere.Indices32.size() * sizeof(std::uint32_t);
}

void BoxApp::CreateGeoSphereObject(
	std::string Name,
	std::string textuerName,
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

	r->mFormat = "";
	r->isDrawShadow = isDrawShadow;
	r->isDrawTexture = isDrawTexture;

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

		obj->mPhyxResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	if (obj->mInstances.size() == 0)
		throw std::runtime_error("Instance Size must bigger than 0.");

	for (UINT inst = 0; inst < obj->InstanceCount; inst++)
	{
		obj->Bounds[inst].Transform(obj->Bounds[inst], mWorldMat);

		obj->mPhyxResources[inst].Position[0]	= position.x;
		obj->mPhyxResources[inst].Position[1]	= position.y;
		obj->mPhyxResources[inst].Position[2]	= position.z;
		obj->mPhyxResources[inst].Rotation[0]	= rotation.x;
		obj->mPhyxResources[inst].Rotation[1]	= rotation.y;
		obj->mPhyxResources[inst].Rotation[2]	= rotation.z;
		obj->mPhyxResources[inst].Scale[0]		= scale.x;
		obj->mPhyxResources[inst].Scale[1]		= scale.y;
		obj->mPhyxResources[inst].Scale[2]		= scale.z;

		//memcpy(obj->mPhyxResources[inst].Position, &position, sizeof(float) * 3);
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
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	sphereSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	sphereSubmesh.IndexSize = (UINT)Sphere.Indices32.size();
	sphereSubmesh.VertexSize = (UINT)Sphere.Vertices.size();

	obj->mDesc[0].StartIndexLocation = sphereSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = sphereSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = sphereSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = sphereSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs[Name.c_str()] = sphereSubmesh;
	obj->mGeometry.DrawArgs[Name.c_str()].textureName = textuerName.c_str();
	obj->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = obj->vertices.size();
	XMVECTOR posV;

	obj->vertices.resize(startV + Sphere.Vertices.size());

	Vertex* v = obj->vertices.data();
	for (size_t i = 0; i < (Sphere.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Sphere.Vertices[i].Position);

		v[i + startV].Normal = Sphere.Vertices[i].Normal;
		v[i + startV].TexC = Sphere.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->indices.insert(
		obj->indices.end(),
		std::begin(Sphere.Indices32),
		std::end(Sphere.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	obj->mGeometry.IndexSize			+= (UINT)Sphere.Indices32.size();

	obj->mGeometry.VertexBufferByteSize	+= (UINT)Sphere.Vertices.size() * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize	+= (UINT)Sphere.Indices32.size() * sizeof(std::uint32_t);
}

void BoxApp::CreateCylinberObject(
	std::string Name,
	std::string textuerName,
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

	r->mFormat = "";
	r->isDrawShadow = isDrawShadow;
	r->isDrawTexture = isDrawTexture;

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

		obj->mPhyxResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	if (obj->mInstances.size() == 0)
		throw std::runtime_error("Instance Size must bigger than 0.");
	
	for (UINT inst = 0; inst < obj->InstanceCount; inst++)
	{
		obj->Bounds[inst].Transform(obj->Bounds[inst], mWorldMat);

		obj->mPhyxResources[inst].Position[0]	= position.x;
		obj->mPhyxResources[inst].Position[1]	= position.y;
		obj->mPhyxResources[inst].Position[2]	= position.z;
		obj->mPhyxResources[inst].Rotation[0]	= rotation.x;
		obj->mPhyxResources[inst].Rotation[1]	= rotation.y;
		obj->mPhyxResources[inst].Rotation[2]	= rotation.z;
		obj->mPhyxResources[inst].Scale[0]		= scale.x;
		obj->mPhyxResources[inst].Scale[1]		= scale.y;
		obj->mPhyxResources[inst].Scale[2]		= scale.z;

		//memcpy(obj->mPhyxResources[inst].Position, &position, sizeof(float) * 3);
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
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	obj->mDesc[0].StartIndexLocation = boxSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = boxSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = boxSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = boxSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs[Name.c_str()] = boxSubmesh;
	obj->mGeometry.DrawArgs[Name.c_str()].textureName = textuerName.c_str();
	obj->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = obj->vertices.size();
	XMVECTOR posV;

	obj->vertices.resize(startV + Box.Vertices.size());

	Vertex* v = obj->vertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->indices.insert(
		obj->indices.end(),
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

	r->mFormat = "";
	r->isDrawShadow = isDrawShadow;
	r->isDrawTexture = isDrawTexture;

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

		obj->mPhyxResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	for (UINT i = 0; i < r->InstanceCount; i++)
	{
		DirectX::XMStoreFloat4x4(&obj->mInstances[i].World, mWorldMat);
		
		obj->mPhyxResources[i].Position[0]	= position.x;
		obj->mPhyxResources[i].Position[1]	= position.y;
		obj->mPhyxResources[i].Position[2]	= position.z;
		obj->mPhyxResources[i].Rotation[0]	= rotation.x;
		obj->mPhyxResources[i].Rotation[1]	= rotation.y;
		obj->mPhyxResources[i].Rotation[2]	= rotation.z;
		obj->mPhyxResources[i].Scale[0]		= scale.x;
		obj->mPhyxResources[i].Scale[1]		= scale.y;
		obj->mPhyxResources[i].Scale[2]		= scale.z;

		memcpy(obj->mPhyxResources[i].Position, &position, sizeof(float) * 3);
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
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	// init Boundary(Collider) Box
	if (obj->Bounds.size() > 0)
	{
		obj->Bounds[0].Extents.x = scale.x * w * 0.5f;
		obj->Bounds[0].Extents.y = scale.y;
		obj->Bounds[0].Extents.z = scale.z * h * 0.5f;
	}

	obj->mDesc[0].StartIndexLocation = boxSubmesh.StartIndexLocation;
	obj->mDesc[0].BaseVertexLocation = boxSubmesh.BaseVertexLocation;

	obj->mDesc[0].IndexSize = boxSubmesh.IndexSize;
	obj->mDesc[0].VertexSize = boxSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	obj->mGeometry.subMeshCount += 1;
	obj->mGeometry.DrawArgs[Name.c_str()] = boxSubmesh;
	obj->mGeometry.DrawArgs[Name.c_str()].textureName = textuerName.c_str();
	obj->mGeometry.meshNames.push_back(Name.c_str());

	size_t startV = obj->vertices.size();
	XMVECTOR posV;

	obj->vertices.resize(startV + Box.Vertices.size());

	Vertex* v = obj->vertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->indices.insert(
		obj->indices.end(),
		std::begin(Box.Indices32),
		std::end(Box.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	obj->mGeometry.IndexSize			+= (UINT)Box.Indices32.size();

	obj->mGeometry.VertexBufferByteSize	+= (UINT)Box.Vertices.size() * sizeof(Vertex);
	obj->mGeometry.IndexBufferByteSize	+= (UINT)Box.Indices32.size() * sizeof(std::uint32_t);
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
	bool uvMode,
	bool isDrawShadow,
	bool isDrawTexture
)
{
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

		obj->mPhyxResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

	obj->mName = r->mName;
	obj->mFormat = "FBX";
	obj->mRenderType = ObjectData::RenderType::_OPAQUE_RENDER_TYPE;

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
	r->isDrawShadow = isDrawShadow;
	r->isDrawTexture = isDrawTexture;

	// init Boundary(Collider) Box
	obj->Bounds[0].Center = position;
	obj->Bounds[0].Extents = scale;

	for (UINT inst = 0; inst < r->InstanceCount; inst++)
	{
		DirectX::XMStoreFloat4x4(&obj->mInstances[inst].World, mWorldMat);

		obj->mPhyxResources[inst].Position[0]	= position.x;
		obj->mPhyxResources[inst].Position[1]	= position.y;
		obj->mPhyxResources[inst].Position[2]	= position.z;
		obj->mPhyxResources[inst].Rotation[0]	= rotation.x;
		obj->mPhyxResources[inst].Rotation[1]	= rotation.y;
		obj->mPhyxResources[inst].Rotation[2]	= rotation.z;
		obj->mPhyxResources[inst].Scale[0]		= scale.x;
		obj->mPhyxResources[inst].Scale[1]		= scale.y;
		obj->mPhyxResources[inst].Scale[2]		= scale.z;

		// memcpy(obj->mPhyxResources[inst].Position, &position, sizeof(float) * 3);
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

	size_t startV = obj->vertices.size();

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	for (int subMeshCount = 0; subMeshCount < meshData.size(); ++subMeshCount)
	{
		SubmeshGeometry boxSubmesh;
		std::string submeshName = Name + std::to_string(subMeshCount);

		// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
		boxSubmesh.IndexSize = (UINT)meshData[subMeshCount].Indices32.size();
		boxSubmesh.VertexSize = (UINT)meshData[subMeshCount].Vertices.size();

		obj->mDesc[subMeshCount].BaseVertexLocation = vertexOffset;
		obj->mDesc[subMeshCount].StartIndexLocation = indexOffset;

		obj->mDesc[subMeshCount].IndexSize = boxSubmesh.IndexSize;
		obj->mDesc[subMeshCount].VertexSize = boxSubmesh.VertexSize;

		// �� ����޽��� Cloth physix �ε����� �ʱ�ȭ
		obj->isCloth.push_back(false);
		obj->isRigidBody.push_back(false);

		// Submesh ����
		r->SubmeshCount += 1;

		obj->mGeometry.subMeshCount += 1;
		obj->mGeometry.DrawArgs[submeshName] = boxSubmesh;
		obj->mGeometry.DrawArgs[submeshName].name = d3dUtil::getName(meshData[subMeshCount].texPath);
		obj->mGeometry.DrawArgs[submeshName].textureName = meshData[subMeshCount].texPath;

		obj->mGeometry.DrawArgs[submeshName].BaseVertexLocation = vertexOffset;
		obj->mGeometry.DrawArgs[submeshName].StartIndexLocation = indexOffset;

		// �ش� �̸��� �Ž��� ����Ǿ� ������ �˸��� ���� �̸��� ����
		obj->mGeometry.meshNames.push_back(submeshName);
		texturePath.push_back(obj->mGeometry.DrawArgs[submeshName].textureName);

		// _Geom ������ �����ϱ⿡ ���ؽ� ������ �������� �̸� ���صд�.
		startV = obj->vertices.size();
		// ���ο� Submesh�� �� ������ �����Ѵ�.
		obj->vertices.resize(startV + meshData[subMeshCount].Vertices.size());
		Vertex* v = obj->vertices.data();
		// _Geometry�� �ϳ��� ������ ���� ���� ������ ������Ʈ���� �� �� ���� ���ؽ� ���� �ֱ��Ѵ�.
		// �̴� ���Ŀ� Deprecated �� ��.
		for (size_t i = 0; i < (meshData[subMeshCount].Vertices.size()); ++i)
		{
			v[i + startV].Pos = meshData[subMeshCount].Vertices[i].Position;
			v[i + startV].Normal = meshData[subMeshCount].Vertices[i].Normal;
			v[i + startV].Tangent = meshData[subMeshCount].Vertices[i].TangentU;
			v[i + startV].TexC = meshData[subMeshCount].Vertices[i].TexC;
		}

		obj->indices.insert(
			obj->indices.end(),
			std::begin(meshData[subMeshCount].Indices32),
			std::end(meshData[subMeshCount].Indices32)
		);

		// Texture, Material �ڵ� ����
		{
			// ���� �ؽ��İ� �����Ѵٸ�
			if (obj->mGeometry.DrawArgs[submeshName].textureName != "")
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
				this->BindMaterial(r, "Default", "", "", "bricksTex");
			}

		}

		startV += boxSubmesh.VertexSize;

		// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
		vertexOffset += boxSubmesh.VertexSize;
		indexOffset += boxSubmesh.IndexSize;

		obj->mGeometry.IndexSize += (UINT)boxSubmesh.IndexSize;

		obj->mGeometry.VertexBufferByteSize += boxSubmesh.VertexSize * sizeof(Vertex);
		obj->mGeometry.IndexBufferByteSize += boxSubmesh.IndexSize * sizeof(std::uint32_t);
	}
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

		obj->mPhyxResources.resize(r->InstanceCount);

		mGameObjectDatas[r->mName] = obj;
		obj = mGameObjectDatas[r->mName];
	}

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
	r->isDrawShadow = isDrawShadow;
	r->isDrawTexture = isDrawTexture;

	// init Boundary(Collider) Box
	obj->Bounds[0].Center = position;
	obj->Bounds[0].Extents = scale;

	for (UINT inst = 0; inst < r->InstanceCount; inst++)
	{
		DirectX::XMStoreFloat4x4(&obj->mInstances[inst].World, mWorldMat);

		obj->mPhyxResources[inst].Position[0] = position.x;
		obj->mPhyxResources[inst].Position[1] = position.y;
		obj->mPhyxResources[inst].Position[2] = position.z;
		obj->mPhyxResources[inst].Rotation[0] = rotation.x;
		obj->mPhyxResources[inst].Rotation[1] = rotation.y;
		obj->mPhyxResources[inst].Rotation[2] = rotation.z;
		obj->mPhyxResources[inst].Scale[0] = scale.x;
		obj->mPhyxResources[inst].Scale[1] = scale.y;
		obj->mPhyxResources[inst].Scale[2] = scale.z;

		// memcpy(obj->mPhyxResources[inst].Position, &position, sizeof(float) * 3);
	}

	int res = Geom.CreateFBXSkinnedModel(
		meshData,
		(Path + "\\" + FileName),
		obj->animNameLists,
		obj->mStart,
		obj->mStop,
		obj->countOfFrame,
		obj->mAnimVertex,
		obj->mAnimVertexSize,
		uvMode
	);

	assert(!res && "(CreateFBXObject)Failed to create FBX Model on CreateFBXObject.");

	// �� �ִϸ��̼��� �ϳ��� ������ �� �ð�(�� ����)�� �ִϸ��̼��� ��ü �෹�̼� (�� ����)�� ���ɴϴ�.
	for (int animCount = 0; animCount < obj->countOfFrame.size(); animCount++)
	{
		obj->durationPerSec.push_back((float)obj->mStop[animCount].GetSecondDouble());
		obj->durationOfFrame.push_back(obj->durationPerSec[animCount] / obj->countOfFrame[animCount]);
	}

	obj->beginAnimIndex = 0;
	obj->currentFrame = 0;
	obj->currentDelayPerSec = 0;

	obj->endAnimIndex =
		(float)obj->durationPerSec[0] /
		obj->durationOfFrame[0];

	std::vector<PxClothParticle> vertices;
	std::vector<PxU32> primitives;

	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = (UINT)meshData.size();
	obj->mDesc.resize(meshData.size());

	// �� Submesh�� Offset�� �����ϴ� �뵵
	size_t startV = obj->vertices.size();

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	for (int subMeshCount = 0; subMeshCount < meshData.size(); ++subMeshCount)
	{
		SubmeshGeometry boxSubmesh;
		std::string submeshName = Name + std::to_string(subMeshCount);

		// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
		boxSubmesh.StartIndexLocation = (UINT)obj->mGeometry.IndexBufferByteSize;
		boxSubmesh.BaseVertexLocation = (UINT)obj->mGeometry.VertexBufferByteSize;

		boxSubmesh.IndexSize = (UINT)meshData[subMeshCount].Indices32.size();
		boxSubmesh.VertexSize = (UINT)meshData[subMeshCount].Vertices.size();

		obj->mDesc[subMeshCount].BaseVertexLocation = vertexOffset;
		obj->mDesc[subMeshCount].StartIndexLocation = indexOffset;

		obj->mDesc[subMeshCount].IndexSize = boxSubmesh.IndexSize;
		obj->mDesc[subMeshCount].VertexSize = boxSubmesh.VertexSize;

		// �� ����޽��� Cloth physix �ε����� �ʱ�ȭ
		obj->isCloth.push_back(false);
		obj->isRigidBody.push_back(false);

		// Submesh ����
		r->SubmeshCount += 1;

		obj->mGeometry.subMeshCount += 1;
		obj->mGeometry.DrawArgs[submeshName] = boxSubmesh;
		obj->mGeometry.DrawArgs[submeshName].name = d3dUtil::getName(meshData[subMeshCount].texPath);
		obj->mGeometry.DrawArgs[submeshName].textureName = meshData[subMeshCount].texPath;

		obj->mGeometry.DrawArgs[submeshName].BaseVertexLocation = vertexOffset;
		obj->mGeometry.DrawArgs[submeshName].StartIndexLocation = indexOffset;

		////////////////

		// �ش� �̸��� �Ž��� ����Ǿ� ������ �˸��� ���� �̸��� ����
		obj->mGeometry.meshNames.push_back(submeshName);
		texturePath.push_back(obj->mGeometry.DrawArgs[submeshName].textureName);

		//memcpy(r->mPhyxResources[subMeshCount].Position, &position, sizeof(float) * 3);

		// _Geom ������ �����ϱ⿡ ���ؽ� ������ �������� �̸� ���صд�.
		startV = (UINT)obj->vertices.size();
		// ���ο� Submesh�� �� ������ �����Ѵ�.
		obj->vertices.resize(startV + meshData[subMeshCount].Vertices.size());
		Vertex* v = obj->vertices.data();

		// _Geometry�� �ϳ��� ������ ���� ���� ������ ������Ʈ���� �� �� ���� ���ؽ� ���� �ֱ��Ѵ�.
		// �̴� ���Ŀ� Deprecated �� ��.
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

		obj->indices.insert(
			obj->indices.end(),
			std::begin(meshData[subMeshCount].Indices32),
			std::end(meshData[subMeshCount].Indices32)
		);

		// Texture, Material �ڵ� ����
		{
			// ���� �ؽ��İ� �����Ѵٸ�
			if (obj->mGeometry.DrawArgs[submeshName].textureName != "")
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
		vertexOffset += boxSubmesh.VertexSize;
		indexOffset += boxSubmesh.IndexSize;

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
	// meshData�� 0������ Vertices �迭
	// meshData�� 1���� ���� ���ʹ� �� Submesh�� ���� Indices
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
	r->isDrawShadow = isDrawShadow;
	r->isDrawTexture = isDrawTexture;

	// init Boundary(Collider) Box
	obj->Bounds[0].Center = position;
	//obj->Bounds[0].Extents = scale;
	obj->Bounds[0].Extents = { 3.0f, 10.0f, 3.0f };

	for (UINT inst = 0; inst < r->InstanceCount; inst++)
	{
		DirectX::XMStoreFloat4x4(&obj->mInstances[inst].World, mWorldMat);

		obj->mPhyxResources[inst].Position[0]	= position.x;
		obj->mPhyxResources[inst].Position[1]	= position.y;
		obj->mPhyxResources[inst].Position[2]	= position.z;
		obj->mPhyxResources[inst].Rotation[0]	= rotation.x;
		obj->mPhyxResources[inst].Rotation[1]	= rotation.y;
		obj->mPhyxResources[inst].Rotation[2]	= rotation.z;
		obj->mPhyxResources[inst].Scale[0]		= scale.x;
		obj->mPhyxResources[inst].Scale[1]		= scale.y;
		obj->mPhyxResources[inst].Scale[2]		= scale.z;

		// memcpy(obj->mPhyxResources[inst].Position, &position, sizeof(float) * 3);
	}

	// Load Submesh Count
	int _SubMeshCount = model->material_count;

	// GameObjects Data�� ���� ���� ����
	obj->InstanceCount = r->InstanceCount;
	obj->SubmeshCount = _SubMeshCount;
	obj->mDesc.resize(_SubMeshCount);

	//////////////////////////////////////////////////////////////
	// �ܺ� �ִϸ��̼� ���� �о� Vertex�� ������Ʈ
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

	obj->mAnimationClip.appendClip("IDLE2", 15.0f, 90.0f);
	obj->mAnimationClip.appendClip("IDLE2toIDLE1", 91.0f, 131.0f);
	obj->mAnimationClip.appendClip("IDLE1", 132.0f, 232.0f);
	obj->mAnimationClip.appendClip("IDLE1toIDLE2", 233.0f, 273.0f);

	obj->mAnimationClip.appendClip("JUMP_BACK", 274.0f, 299.0f);
	obj->mAnimationClip.appendClip("JUMP_LEFT", 300.0f, 325.0f);
	obj->mAnimationClip.appendClip("JUMP_RIGHT", 326.0f, 351.0f);

	obj->mAnimationClip.appendClip("KNOCKED_BACKWARD", 352.0f, 393.0f);
	obj->mAnimationClip.appendClip("DOWN_BACK", 394.0f, 434.0f);
	obj->mAnimationClip.appendClip("RECOVER_FROM_KNOCKED_BACKWARD", 435.0f, 475.0f);

	obj->mAnimationClip.appendClip("KNOCKED_FORWARD", 476.0f, 504.0f);
	obj->mAnimationClip.appendClip("DOWN_FORWARD", 505.0f, 545.0f);
	obj->mAnimationClip.appendClip("RECOVER_FROM_KNOCKED_FORWARD", 546.0f, 578.0f);

	obj->mAnimationClip.appendClip("MAGIC_SHOT_STRIGHT", 666.0f, 701.0f);

	obj->mAnimationClip.appendClip("DODGE_BACKWARD", 885.0f, 920.0f);
	obj->mAnimationClip.appendClip("DODGE_TO_LEFT", 921.0f, 956.0f);
	obj->mAnimationClip.appendClip("DODGE_TO_RIGHT", 957.0f, 992.0f);
	obj->mAnimationClip.appendClip("DODGE_TO_BACK", 993.0f, 1028.0f);
	obj->mAnimationClip.appendClip("DODGE_TO_FRONT", 1029.0f, 1064.0f);

	obj->mAnimationClip.appendClip("EARTHQUAKE_SPELL", 1172.0f, 1207.0f);
	obj->mAnimationClip.appendClip("HIT_STRIGHT_DOWN", 1208.0f, 1243.0f);
	obj->mAnimationClip.appendClip("HIT_SWING_RIGHT", 1244.0f, 1279.0f);

	obj->mAnimationClip.appendClip("DYING_A", 2220.0f, 2260.0f);
	obj->mAnimationClip.appendClip("DYING_B", 1280.0f, 1345.0f);

	obj->mAnimationClip.appendClip("DRINKING_POTION", 1568.0f, 1623.0f);

	obj->mAnimationClip.appendClip("SPELL_CAST_1", 2082.0f, 2117.0f);
	obj->mAnimationClip.appendClip("SPELL_CAST_2", 2118.0f, 2178.0f);

	obj->mAnimationClip.appendClip("WALK", 2309.0f, 2344.0f);
	obj->mAnimationClip.appendClip("RUN", 2796.0f, 2831.0f);
	obj->mAnimationClip.appendClip("RUN_A", 2492.0f, 2536.0f);
	obj->mAnimationClip.appendClip("RUN_B", 2537.0f, 2581.0f);
	obj->mAnimationClip.appendClip("RUN_C", 2582.0f, 2626.0f);
	obj->mAnimationClip.appendClip("RUN_D", 2627.0f, 2671.0f);

	obj->mAnimationClip.appendClip("DIAGONAL_LEFT", 2677.0f, 2711.0f);
	obj->mAnimationClip.appendClip("DIAGONAL_RIGHT", 2717.0f, 2751.0f);
	obj->mAnimationClip.appendClip("STRIFE_LEFT", 3239.0f, 3273.0f);
	obj->mAnimationClip.appendClip("STRIFE_RIGHT", 3279.0f, 3313.0f);


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

	std::vector<struct ObjectData::_VERTEX_MORPH_DESCRIPTOR> test = obj->mMorph;

	obj->mMorph[9].mVertWeight = 1.0f;
	obj->mMorphDirty.push_back(9);

	obj->mMorph[19].mVertWeight = 1.0f;
	obj->mMorphDirty.push_back(19);

	obj->mMorph[27].mVertWeight = 1.0f;
	obj->mMorphDirty.push_back(27);

	obj->mMorph[41].mVertWeight = 1.0f;
	obj->mMorphDirty.push_back(41);

	obj->mMorph[52].mVertWeight = 1.0f;
	obj->mMorphDirty.push_back(52);

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
		SubmeshGeometry boxSubmesh;
		std::string submeshName = Name + std::to_string(subMeshIDX);

		// �� ���׸����� �ؽ��� �ε��� �ε�
		diffuseTextureIDX = model->materials[subMeshIDX].diffuse_texture_index;
		sphereTextureIDX = model->materials[subMeshIDX].sphere_texture_index;
		toonTextureIDX = model->materials[subMeshIDX].toon_texture_index;

		// vertex, index�� ������ �ε�
		// Draw���� ��� Vertices�� �ѹ��� DescriptorSet�� VBV�� ���ε� �� �� (SubMesh ������ ���ε� X)
		boxSubmesh.BaseVertexLocation = (UINT)vertexOffset;
		boxSubmesh.StartIndexLocation = (UINT)indexOffset;

		// �� submesh�� ���ε� �� ���ؽ�, �ε��� ����
		boxSubmesh.VertexSize = (UINT)obj->vertBySubmesh[subMeshIDX].size();
		boxSubmesh.IndexSize = (UINT)model->materials[subMeshIDX].index_count;

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

		obj->mDesc[subMeshIDX].BaseVertexLocation = boxSubmesh.BaseVertexLocation;
		obj->mDesc[subMeshIDX].StartIndexLocation = boxSubmesh.StartIndexLocation;

		obj->mDesc[subMeshIDX].VertexSize = boxSubmesh.VertexSize;
		obj->mDesc[subMeshIDX].IndexSize = boxSubmesh.IndexSize;

		// Submesh ����
		r->SubmeshCount += 1;

		obj->mGeometry.subMeshCount += 1;
		obj->mGeometry.DrawArgs[submeshName] = boxSubmesh;
		if (diffuseTextureIDX >= 0) {
			obj->mGeometry.DrawArgs[submeshName].name = d3dUtil::getName(texturePath[diffuseTextureIDX]);
			obj->mGeometry.DrawArgs[submeshName].textureName = texturePath[diffuseTextureIDX];
		}
		else {
			obj->mGeometry.DrawArgs[submeshName].name = "";
			obj->mGeometry.DrawArgs[submeshName].textureName = "";
		}

		obj->mGeometry.DrawArgs[submeshName].BaseVertexLocation = 0;
		obj->mGeometry.DrawArgs[submeshName].StartIndexLocation = indexOffset;

		// �ش� �̸��� �Ž��� ����Ǿ� ������ �˸��� ���� �̸��� ����
		// DrawArgs���� �ٽ� �ش� Submesh�� �ε��� �� �ֵ��� �̸��� ����
		obj->mGeometry.meshNames.push_back(submeshName);

		// Texture, Material �ڵ� ����
		{
			// ���� �ؽ��İ� �����Ѵٸ�
			if (obj->mGeometry.DrawArgs[submeshName].textureName != "")
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

		vertexOffset += boxSubmesh.VertexSize;
		indexOffset += boxSubmesh.IndexSize;
	}

	//memcpy(r->mPhyxResources[0].Position, &position, sizeof(float) * 3);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
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
	Box = Geom.CreateBox(
		obj->Bounds[0].Extents.x * 2.0f, 
		obj->Bounds[0].Extents.y,
		obj->Bounds[0].Extents.z * 2.0f,
		0
	);

	obj->mDebugBoxData->InstanceCount += 1;

	obj->mDebugBoxData->SubmeshCount += 1;
	obj->mDebugBoxData->mDesc.resize(obj->mDebugBoxData->SubmeshCount);

	obj->mDebugBoxData->isCloth.push_back(false);
	obj->mDebugBoxData->isRigidBody.push_back(false);

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)obj->mDebugBoxData->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)obj->mDebugBoxData->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	obj->mDebugBoxData->mDesc[0].StartIndexLocation = boxSubmesh.StartIndexLocation;
	obj->mDebugBoxData->mDesc[0].BaseVertexLocation = boxSubmesh.BaseVertexLocation;

	obj->mDebugBoxData->mDesc[0].IndexSize = boxSubmesh.IndexSize;
	obj->mDebugBoxData->mDesc[0].VertexSize = boxSubmesh.VertexSize;

	// Submesh ����
	std::string submeshName = "_DEBUG_" + r->mName;

	obj->mDebugBoxData->mGeometry.subMeshCount							+= 1;
	obj->mDebugBoxData->mGeometry.DrawArgs[submeshName].StartIndexLocation	= boxSubmesh.StartIndexLocation;
	obj->mDebugBoxData->mGeometry.DrawArgs[submeshName].BaseVertexLocation	= boxSubmesh.BaseVertexLocation;
	obj->mDebugBoxData->mGeometry.DrawArgs[submeshName].IndexSize				= boxSubmesh.IndexSize;
	obj->mDebugBoxData->mGeometry.DrawArgs[submeshName].VertexSize			= boxSubmesh.VertexSize;
	obj->mDebugBoxData->mGeometry.DrawArgs[submeshName].textureName			= "";
	obj->mDebugBoxData->mGeometry.meshNames.push_back(submeshName);

	size_t startV = obj->mDebugBoxData->vertices.size();
	XMVECTOR posV;

	obj->mDebugBoxData->vertices.resize(startV + Box.Vertices.size());

	Vertex* v = obj->mDebugBoxData->vertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	obj->mDebugBoxData->indices.insert(
		obj->mDebugBoxData->indices.end(),
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
	for (UINT inst = 0; inst < r->InstanceCount; inst++)
	{
		mGameObjectDatas[r->mName]->Bounds[inst].Extents = scale;
	}
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

			// unordered_map�� Index�� �ο��ϱ� ���� �̸� ����Ʈ ���� ����.
			mTextureList.push_back(tex.Name);
			mTextures[tex.Name] = tex;
			mTextures[tex.Name].isCube = isSky;
		}
	}
	catch (std::exception e) {
		MessageBoxA(nullptr, (LPCSTR)L"DDS �ؽ��ĸ� ã�� ���Ͽ����ϴ�.", (LPCSTR)L"Error", MB_OK);
	}
}

void BoxApp::uploadMaterial(_In_ std::string name, _In_ bool isSkyTexture) {
	std::vector<std::pair<std::string, Material>>::iterator& matIDX = mMaterials.begin();
	std::vector<std::pair<std::string, Material>>::iterator& matEnd = mMaterials.end();
	for (; matIDX != matEnd; matIDX++)
	{
		if (matIDX->second.Name == name)
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

		std::pair<std::string, Material> res(name.c_str(), mat);
		mMaterials.push_back(res);
	}
}

void BoxApp::uploadMaterial(
	_In_ std::string matName, 
	_In_ std::string texName, 
	_In_ bool isSkyTexture
)
{
	std::vector<std::pair<std::string, Material>>::iterator it = mMaterials.begin();
	std::vector<std::pair<std::string, Material>>::iterator itEnd = mMaterials.end();
	for (; it != itEnd; it++)
	{
		if (it->second.Name == matName)
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

	// DiffuseSrvHeapIndex�� ������ ���� �Ҵ��� �ް� �Ǹ�, �������� �ִ� mTexture�� Ư������ ���Ͽ� ���� �ؽ��ĸ� ĳ���� �ϰ� �˴ϴ�.
	mat.DiffuseSrvHeapIndex = diffuseIDX;
	mat.NormalSrvHeapIndex	= diffuseIDX;
	mat.MaskSrvHeapIndex	= 0;
	mat.NoiseSrvHeapIndex	= 0;

	std::pair<std::string, Material> res(matName, mat);
	mMaterials.push_back(res);
}

void BoxApp::uploadMaterial(
	_In_ std::string matName, 
	_In_ std::string tex_Diffuse_Name, 
	_In_ std::string tex_Mask_Name,
	_In_ std::string tex_Noise_Name,
	_In_ bool isSkyTexture
)
{
	std::vector<std::pair<std::string, Material>>::iterator it = mMaterials.begin();
	std::vector<std::pair<std::string, Material>>::iterator itEnd = mMaterials.end();
	for (; it != itEnd; it++)
	{
		if (it->second.Name == matName)
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

	bool isFound	= false;
	int diffuseIDX	= -1;
	int MaskIDX		= -1;
	int NoiseIDX	= -1;

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

	// DiffuseSrvHeapIndex�� ������ ���� �Ҵ��� �ް� �Ǹ�, �������� �ִ� mTexture�� Ư������ ���Ͽ� ���� �ؽ��ĸ� ĳ���� �ϰ� �˴ϴ�.
	mat.DiffuseSrvHeapIndex = diffuseIDX;
	mat.NormalSrvHeapIndex	= diffuseIDX;
	mat.MaskSrvHeapIndex	= MaskIDX;
	mat.NoiseSrvHeapIndex	= NoiseIDX;

	std::pair<std::string, Material> res(matName, mat);
	mMaterials.push_back(res);
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

		obj->Mat[idx]->DiffuseSrvHeapIndex = diffuseIDX;
		obj->Mat[idx]->NormalSrvHeapIndex = diffuseIDX;
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

		obj->SkyMat[idx]->DiffuseSrvHeapIndex = diffuseIDX;
		obj->SkyMat[idx]->NormalSrvHeapIndex = diffuseIDX;
	}
}

void BoxApp::BindMaterial(
	RenderItem* r,
	std::string name,
	bool isCubeMap
) {
	assert(r && "The RenderItem is NULL");

	ObjectData* obj = mGameObjectDatas[r->mName];

	Material* m = new Material;
	r->isSky = isCubeMap;
	m->isSkyTexture = isCubeMap;

	for (auto& i = mMaterials.begin(); i != mMaterials.end(); i++) {
		if (i->second.Name == name) {
			//m = &i->second;

			m->Name = i->second.Name;
			m->DiffuseAlbedo = i->second.DiffuseAlbedo;
			m->DiffuseSrvHeapIndex = i->second.DiffuseSrvHeapIndex;
			m->FresnelR0 = i->second.FresnelR0;
			m->isSkyTexture = i->second.isSkyTexture;
			m->MatCBIndex = i->second.MatCBIndex;
			m->MatInstIndex = i->second.MatInstIndex;
			m->MatTransform = i->second.MatTransform;
			m->NormalSrvHeapIndex = i->second.NormalSrvHeapIndex;
			m->NumFramesDirty = i->second.NumFramesDirty;
			m->Roughness = i->second.Roughness;

			break;
		}
	}

	assert(m && "Can't find Material which same with NAME!!");

	if (!isCubeMap)
	{
		obj->Mat.push_back(m);
	}
	else
	{
		obj->SkyMat.push_back(m);
	}
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

	Material* m = new Material;
	r->isSky = isCubeMap;
	m->isSkyTexture = isCubeMap;

	for (auto& i = mMaterials.begin(); i != mMaterials.end(); i++) {
		if (i->second.Name == name) {
			//m = &i->second;

			m->Name = i->second.Name;
			m->DiffuseAlbedo = i->second.DiffuseAlbedo;
			m->DiffuseSrvHeapIndex = i->second.DiffuseSrvHeapIndex;
			m->FresnelR0 = i->second.FresnelR0;
			m->isSkyTexture = i->second.isSkyTexture;
			m->MatCBIndex = i->second.MatCBIndex;
			m->MatInstIndex = i->second.MatInstIndex;
			m->MatTransform = i->second.MatTransform;
			m->NormalSrvHeapIndex = i->second.NormalSrvHeapIndex;
			m->NumFramesDirty = i->second.NumFramesDirty;
			m->Roughness = i->second.Roughness;

			break;
		}
	}

	assert(m && "Can't find Material which same with NAME!!");

	if (!isCubeMap)
	{
		obj->Mat.push_back(m);
	}
	else
	{
		obj->SkyMat.push_back(m);
	}
}

void BoxApp::BindMaterial(
	RenderItem* r,
	std::string matName,
	std::string texName,
	bool isCubeMap
) {
	assert(r && "The RenderItem is NULL");

	r->isSky = isCubeMap;

	// Define of Material
	Material* m = NULL;
	for (auto& i = mMaterials.begin(); i != mMaterials.end(); i++) {
		if (i->second.Name == matName) {
			m = &i->second;
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

	mGameObjectDatas[r->mName]->Mat.push_back(m);
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

	r->isSky = isCubeMap;

	// Define of Material
	Material* m = NULL;
	for (auto& i = mMaterials.begin(); i != mMaterials.end(); i++) {
		if (i->second.Name == matName) {
			m = &i->second;
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

	mGameObjectDatas[r->mName]->Mat.push_back(m);
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
				obj->mRenderType	== ObjectData::RenderType::_SKY_FORMAT_RENDER_TYPE	||
				obj->mRenderType	== ObjectData::RenderType::_DEBUG_BOX_TYPE			||
				obj->mName			== "BottomGeo"
			)
		{
			iter++;

			continue;
		}

		for (UINT inst = 0; inst < obj->InstanceCount; inst++)
		{
			XMMATRIX W = XMLoadFloat4x4(&obj->mInstances[inst].World);
			XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);

			// Tranform ray to vi space of Mesh.
			// inv ViewWorld
			XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);

			// Camera View World에서 Origin View World로 변경하는
			rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

			rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
			rayDir = XMVector3TransformNormal(rayDir, toLocal);

			//rayOrigin = XMVector3TransformCoord(rayOrigin, invView);
			//rayDir = XMVector3TransformNormal(rayDir, invView);

			// Make the ray direction unit length for the intersection tests.
			rayDir = XMVector3Normalize(rayDir);

			float tmin = 0.0f;
			if (obj->Bounds[inst].Intersects(rayOrigin, rayDir, tmin))
			{
				// 경계 박스에 광선이 접촉한다면, 해당 오브젝트를 선택한다.
				// tmin는 접촉한 경계 박스 중 가장 거리가 작은 박스의 거리를 저장한다.
				// 만일 접촉한 박스가 하나도 없다면 tmin은 0.0이다.

				printf("");
			}
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

		for (UINT inst = 0; inst < obj->InstanceCount; inst++)
		{
			XMMATRIX W = XMLoadFloat4x4(&obj->mInstances[inst].World);
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
			if (obj->Bounds[inst].Intersects(rayOrigin, rayDir, tmin))
			{
				XMVECTOR hitPos = rayOrigin + rayDir * tmin;

				XMVECTOR mBoundPos[2] = {};

				mBoundPos[0].m128_f32[0] = obj->Bounds[inst].Center.x - obj->Bounds[inst].Extents.x;
				mBoundPos[0].m128_f32[1] = 0.0;
				mBoundPos[0].m128_f32[2] = obj->Bounds[inst].Center.z - obj->Bounds[inst].Extents.z;
				mBoundPos[0].m128_f32[3] = 1.0f;

				mBoundPos[1].m128_f32[0] = obj->Bounds[inst].Center.x + obj->Bounds[inst].Extents.x;
				mBoundPos[1].m128_f32[1] = 0.0;
				mBoundPos[1].m128_f32[2] = obj->Bounds[inst].Center.z + obj->Bounds[inst].Extents.z;
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
				mDrawTexture->isDirty = true;

				mDrawTexture->Color = { 1.0f, 0.0f, 0.0f, 1.0f };
				mDrawTexture->Origin =
				{
					mTexCroodX,
					1.0f - mTexCroodY
				};
				mDrawTexture->Position = 
				{ 
					mTexCroodX, 
					1.0f - mTexCroodY 
				};
			}
		}

		iter++;
	}
}

//////////////////////////////////
// RenderItem
//////////////////////////////////

void RenderItem::setPosition(_In_ XMFLOAT3 pos) {
	XMVECTOR vec = DirectX::XMLoadFloat3(&pos);

	for (UINT i = 0; i < InstanceCount; i++) {
		//mPhyxResources[i].Position[0] = pos.x;
		//mPhyxResources[i].Position[1] = pos.y;
		//mPhyxResources[i].Position[2] = pos.z;

		memcpy(mGameObjectDatas[mName]->mPhyxResources[i].Position, vec.m128_f32, sizeof(float) * 3);

		mPhys.setPosition(mGameObjectDatas[mName]->mPhyxRigidBody[i], pos.x, pos.y, pos.z);
	}
}
void RenderItem::setRotation(_In_ XMFLOAT3 rot) {
	XMVECTOR vec = DirectX::XMLoadFloat3(&rot);

	for (UINT i = 0; i < InstanceCount; i++) {
		//mPhyxResources[i].Rotation[0] = rot.x;
		//mPhyxResources[i].Rotation[1] = rot.y;
		//mPhyxResources[i].Rotation[2] = rot.z;

		memcpy(mGameObjectDatas[mName]->mPhyxResources[i].Rotation, vec.m128_f32, sizeof(float) * 3);

		mPhys.setRotation(mGameObjectDatas[mName]->mPhyxRigidBody[i], rot.x, rot.y, rot.z);
	}
}

void RenderItem::setVelocity(_In_ XMFLOAT3 vel) {
	for (UINT i = 0; i < InstanceCount; i++)
		mPhys.setVelocity(mGameObjectDatas[mName]->mPhyxRigidBody[i], vel.x, vel.y, vel.z);
}
void RenderItem::setTorque(_In_ XMFLOAT3 torq) {
	for (UINT i = 0; i < InstanceCount; i++)
		mPhys.setTorque(mGameObjectDatas[mName]->mPhyxRigidBody[i], torq.x, torq.y, torq.z);
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

void RenderItem::setAnimClip(_In_ std::string mClipName) {
	int res = mGameObjectDatas[mName]->mAnimationClip.setCurrentClip(mClipName);

	// 애니메이션 클립 정보를 업데이트
	if (!res)
	{
		mGameObjectDatas[mName]->mAnimationClip.getCurrentClip(
			mGameObjectDatas[mName]->beginAnimIndex,
			mGameObjectDatas[mName]->endAnimIndex
		);

		mGameObjectDatas[mName]->currentDelayPerSec =
			mGameObjectDatas[mName]->beginAnimIndex;
	}
}

const std::string RenderItem::getAnimClip() const 
{
	return mGameObjectDatas[mName]->mAnimationClip.getCurrentClipName();
}