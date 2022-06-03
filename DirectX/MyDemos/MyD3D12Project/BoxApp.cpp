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
	: D3DApp(hInstance)
{
	mRenderTypeCount[RenderItem::RenderType::_OPAQUE_RENDER_TYPE] = 0;
	mRenderTypeCount[RenderItem::RenderType::_OPAQUE_SKINNED_RENDER_TYPE] = 0;
	mRenderTypeCount[RenderItem::RenderType::_PMX_FORMAT_RENDER_TYPE] = 0;
	mRenderTypeCount[RenderItem::RenderType::_ALPHA_RENDER_TYPE] = 0;
	mRenderTypeCount[RenderItem::RenderType::_SKY_FORMAT_RENDER_TYPE] = 0;
}

// Pipeline State Object Type List
std::unordered_map<RenderItem::RenderType, ComPtr<ID3D12PipelineState>> BoxApp::mPSOs;
// Post Process RTV ( Will use to Blur, Sobel Calc)
std::unique_ptr<RenderTarget> BoxApp::mOffscreenRT = nullptr;

// Degree of Blur
UINT BoxApp::mFilterCount = 0;

std::unique_ptr<BlurFilter> BoxApp::mBlurFilter;
std::unique_ptr<SobelFilter> BoxApp::mSobelFilter;

ComPtr<ID3D12DescriptorHeap> BoxApp::mSrvDescriptorHeap = nullptr;

ComPtr<ID3D12RootSignature> BoxApp::mRootSignature = nullptr;
ComPtr<ID3D12RootSignature> BoxApp::mBlurRootSignature = nullptr;
ComPtr<ID3D12RootSignature> BoxApp::mSobelRootSignature = nullptr;

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
static std::unique_ptr<UploadBuffer<InstanceData>>				InstanceBuffer;
static std::unique_ptr<UploadBuffer<MaterialData>>				MaterialBuffer;
static std::unique_ptr<UploadBuffer<LightData>>					LightBuffer;
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

	std::list<RenderItem*>::iterator& obj = mGameObjects.begin();
	std::list<RenderItem*>::iterator& objEnd = mGameObjects.end();

	// Traversal each GameObject, than updated and got a new Cloth Data of Vertex
	for (; obj != objEnd; obj++)
	{
		RenderItem* _RenderItem = *obj;
		// The new Cloth Vertex Data will be stored in here.
		ObjectData* _RenderData = mGameObjectDatas[_RenderItem->mName];

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
				vOffset = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].BaseVertexLocation;
				iOffset = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].StartIndexLocation;

				vSize = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].VertexSize;
				iSize = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].IndexSize;

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
				vOffset = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].BaseVertexLocation;
				iOffset = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].StartIndexLocation;

				vSize = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].VertexSize;
				iSize = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].IndexSize;

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
				vOffset = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].BaseVertexLocation;
				iOffset = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].StartIndexLocation;

				vSize = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].VertexSize;
				iSize = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].IndexSize;

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

	BuildRootSignature();
	BuildBlurRootSignature();
	BuildSobelRootSignature();
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
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 1;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())
	));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
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
	BoundingFrustum localSpaceFrustum;
	InstanceData data;

	//const PhyxResource* pr = nullptr;
	UINT visibleInstanceCount = 0;

	UINT i;
	int gIdx = 0;

	// (���� ������Ʈ ���� * ���� ����޽�) ���� ��ŭ�� ���׸��� ������ �Ϸķ� ���� ��Ű�� ���� �ε��� 
	int MatIDX = 0;
	int LightIDX = 0;

	// UpdateMaterialBuffer
	// Material Count == Submesh Count
	// �ν��Ͻ����� �����ϱ� ���� �ν��Ͻ��� ��� �� Material�� �����Ѵ�.
	MaterialData matData;
	XMMATRIX matTransform;

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

	RenderItem* obj = NULL;
	PhyxResource* pr = nullptr;
	std::list<RenderItem*>::iterator objIDX;
	std::list<RenderItem*>::iterator objEnd;
	std::vector<LightData>::iterator lightDataIDX;
	std::vector<Light>::iterator lightIDX;
	std::vector<Light>::iterator lightEnd = mLights.end();

	mRenderInstTasks.resize(mGameObjectDatas.size());

	objIDX = mGameObjects.begin();
	objEnd = mGameObjects.end();
	for (; objIDX != objEnd; objIDX++)
	{
		obj = *objIDX;

		// �������� ���� �����ϴ� ������Ʈ, ������Ʈ �ν��Ͻ� ���� �ʱ�ȭ
		mRenderInstTasks[gIdx].resize(obj->InstanceCount);

		for (i = 0; i < obj->InstanceCount; ++i)
		{
			// getSyncDesc
			pr = &obj->mPhyxResources[i];

			world = XMLoadFloat4x4(&obj->mInstances[i].World);

			texTransform = XMLoadFloat4x4(&obj->mInstances[i].TexTransform);

			invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);
			viewToLocal = XMMatrixMultiply(invView, invWorld);
			mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

			// ���� ĳ���Ͱ� ī�޶� �������� ������ �ʴ´ٸ�,
			// ĳ���Ͱ� ������ ���� �����ϴ� Bound�� ��ġ���� ������Ʈ�� �ϰ�
			// �� ĳ���ʹ� �������� �ʴ´�.
			obj->Bounds.Center.x = pr->Position[0];
			obj->Bounds.Center.y = pr->Position[1];
			obj->Bounds.Center.z = pr->Position[2];

			XMFLOAT4X4 boundScale = MathHelper::Identity4x4();
			boundScale._11 = 1.5f;
			boundScale._22 = 1.5f;
			boundScale._33 = 1.5f;

			obj->Bounds.Transform(obj->Bounds, XMLoadFloat4x4(&boundScale));

			// ���� �������� �ڽ����� �����Ѵٸ�
			if ((localSpaceFrustum.Contains(obj->Bounds) != DirectX::DISJOINT))
			{
				// �������� ���� �����ϴ� ������Ʈ�� ������ ���̴�
				XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
				data.MaterialIndex = obj->mInstances[i].MaterialIndex;

				// ���� ������ �� ���� 
				// ���� ĳ���Ͱ� ī�޶� ������ ���δٸ� ,
				// ĳ������ SRT�� ������Ʈ �Ѵ�.
				data.World._41 = pr->Position[0];
				data.World._42 = pr->Position[1];
				data.World._43 = pr->Position[2];

				//data.World._11 = pr->Scale[0];
				//data.World._22 = pr->Scale[1];
				//data.World._33 = pr->Scale[2];

				// renderInstCounts = {5, 4, 2, 6, 7, 11 .....} �� ����� �ν��Ͻ� Index�� 
				// renderInstIndex = {{0, 1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10}, {11, 12, ..., 16}, ...}
				// ���� ���������� �þ��.
				InstanceBuffer->CopyData(visibleInstanceCount, data);

				// Light Update
				lightIDX = mLights.begin();
				lightDataIDX = mLightDatas.begin();
				Light* l = nullptr;
				float* mObjectPos = pr->Position;
				float distance = 0.0f;

				DirectX::XMFLOAT3 lightVec;
				DirectX::XMVECTOR lightData;

				// 충돌 라이트를 최신화 하기 위해 clear한다.
				(*lightDataIDX).LightSize = 0;
				for (; lightIDX != lightEnd; lightIDX++)
				{
					// 오브젝트의 포지션을 얻어온다.

					// 만일 라이트가 Dir이면
					if ((*lightIDX).LightType == 0)
					{
						// 무조건 포함
						(*lightDataIDX).data[(*lightDataIDX).LightSize++] = *lightIDX;
					}
					// 만일 라이트가 Point이면
					else if ((*lightIDX).LightType == 1)
					{
						// ||(lightPos - Pos)|| < light.FalloffEnd 를 충족하면 포함
						lightVec.x = mObjectPos[0] - (*lightIDX).Position.x;
						lightVec.y = mObjectPos[1] - (*lightIDX).Position.y;
						lightVec.z = mObjectPos[2] - (*lightIDX).Position.z;

						lightData = DirectX::XMLoadFloat3(&lightVec);

						XMStoreFloat(&distance, DirectX::XMVector3Length(lightData));

						if (distance < (*lightIDX).FalloffEnd)
						{
							(*lightDataIDX).data[(*lightDataIDX).LightSize++] = *lightIDX;
						}
					}
					// 만일 라이트가 Spot이면
					else if ((*lightIDX).LightType == 2)
					{
						// ||(lightPos - Pos)|| < light.FalloffEnd 를 충족하면 포함
						lightVec.x = mObjectPos[0] - (*lightIDX).Position.x;
						lightVec.y = mObjectPos[1] - (*lightIDX).Position.y;
						lightVec.z = mObjectPos[2] - (*lightIDX).Position.z;

						lightData = DirectX::XMLoadFloat3(&lightVec);

						XMStoreFloat(&distance, DirectX::XMVector3Length(lightData));

						if (distance < (*lightIDX).FalloffEnd)
						{
							(*lightDataIDX).data[(*lightDataIDX).LightSize++] = *lightIDX;
						}
					}
					else
					{

					}
				}

				LightBuffer->CopyData(visibleInstanceCount, *lightDataIDX++);

				// �ش� ������Ʈ�� ������ �� �ν��Ͻ� ���� ���� (���ּ� ������)
				// ���� Draw������ "�������� ���� �����ϴ�" InstanceCounts �迭�� ������ ������ ���� �۾��� �����Ѵ�.
				// renderInstCounts = {5, 4, 1, 2, 3, 6, 7, 9, 11 .....} �� ���
				// Thread 0 = {5, 2, 7, ...} Thread 1 = {4, 3, 9, ...} Thread 2 = {1, 6, 11, ...}
				// �� ���� �����Ͽ�, "�������� ���� �����ϴ� ������Ʈ"���� ������.
				mRenderInstTasks[gIdx][i] = visibleInstanceCount++;
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
		mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
		mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
		mMainPassCB.NearZ = 1.0f;
		mMainPassCB.FarZ = 1000.0f;
		mMainPassCB.TotalTime = gt.TotalTime();
		mMainPassCB.DeltaTime = gt.DeltaTime();

		mMainPassCB.AmbientLight = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

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
		RenderItem* beginItem = *(mGameObjects.begin());

		// commandAlloc, commandList�� ���� �ϱ� ���� ��������
		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		for (int i = 0; i < 3; i++)
			ThrowIfFailed(mMultiCmdListAlloc[i]->Reset());

		ThrowIfFailed(
			mCommandList->Reset(mDirectCmdListAlloc.Get(),
				mPSOs[RenderItem::RenderType::_POST_PROCESSING_PIPELINE].Get())
		);
		for (int i = 0; i < 3; i++)
			ThrowIfFailed(mMultiCommandList[i]->Reset(mMultiCmdListAlloc[i].Get(), mPSOs[RenderItem::RenderType::_POST_PROCESSING_PIPELINE].Get()));

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
		mCommandList->ClearRenderTargetView(mOffscreenRT->Rtv(), Colors::LightSteelBlue, 0, nullptr);
		mCommandList->ClearDepthStencilView(mDsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	}

	{
		RenderItem* obj = NULL;
		std::list<RenderItem*>::iterator objIDX = mGameObjects.begin();
		std::list<RenderItem*>::iterator objEnd = mGameObjects.end();
		for (; objIDX != objEnd; objIDX++)
		{
			obj = *objIDX;

			if (!mGameObjectDatas[obj->mName]->isDirty)
			{
				continue;
			}

			mGameObjectDatas[obj->mName]->isDirty = false;

			d3dUtil::UpdateDefaultBuffer(
				mCommandList.Get(),
				mGameObjectDatas[obj->mName]->vertices.data(),
				obj->mGeometry.VertexBufferByteSize,
				obj->mGeometry.VertexBufferUploader,
				obj->mGeometry.VertexBufferGPU
			);
		}

		// Cloth�� Write���� ����
		ResetEvent(mClothReadEvent);
		SetEvent(mClothWriteEvent);
		ResetEvent(mAnimationReadEvent);
		SetEvent(mAnimationWriteEvent);

		mCommandList->Close();
	}

	{
		ID3D12CommandList* commands[] = { mCommandList.Get() };
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
	// Base Pointer
	D3D12_GPU_VIRTUAL_ADDRESS instanceCB = InstanceBuffer->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS matCB = MaterialBuffer->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS lightCB = LightBuffer->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS rateOfAnimTimeCB = RateOfAnimTimeCB->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS pmxBoneCB = PmxAnimationBuffer->Resource()->GetGPUVirtualAddress();

	// Stack Pointer
	D3D12_GPU_VIRTUAL_ADDRESS instanceCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS matCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS rateOfAnimTimeCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS pmxBoneCBAddress;

	// Instance Offset
	uint32_t _gInstOffset = 0;

	// loop Resource
	UINT i;
	UINT j;

	// RenderInstTasks Offset
	UINT _taskOffset;
	// RenderInstTasks End Point
	UINT _taskEnd;

	// Extracted Render Items.
	RenderItem* obj = nullptr;
	int amountOfTask = 0;
	int continueIDX = -1;
	UINT taskIDX = 0;

	// GameObjectIndices
	std::vector<UINT> mGameObjectIndices;

	UINT loop = 0;
	UINT end = 0;

	int MatIDX = 0;
	int gameIDX = 0;

	// ������ ���� �� �� ���� ��Ƽ ������ ������ ���
	while (ThreadIDX < numThread)
	{
		// CommandList�� ���������̼��� ��ġ��, ���� Ÿ�� �̺�Ʈ�� ȣ�� �� �� ���� ���.
		WaitForSingleObject(renderTargetEvent[ThreadIDX], INFINITE);

		// Initialization
		continueIDX = -1;
		taskIDX = 0;
		instanceIDX = 0;

		// Divid RenderItem Geometrys by ThreadIDX
		// This Situation will be make Minimalized saving of VBV buffer.

		_taskOffset = (mInstanceCount / 3) * ThreadIDX;

		if (ThreadIDX != 2)
			_taskEnd = (mInstanceCount / 3) * (ThreadIDX + 1);
		else
			_taskEnd = mInstanceCount;

		mGameObjectIndices.resize(_taskEnd - _taskOffset);

		for (loop = 0; loop < _taskEnd - _taskOffset; loop++)
		{
			mGameObjectIndices[loop] = -1;
			for (i = 0; i < mRenderInstTasks.size(); i++)
			{
				end = (UINT)mRenderInstTasks[i].size() - 1;

				if ((_taskOffset + loop) <= mRenderInstTasks[i][end])
				{
					mGameObjectIndices[loop] = i;
					break;
				}
			}

			if (mGameObjectIndices[loop] == -1)
				throw std::runtime_error("Failed to Create Obejct of Index list..");
		}

#ifdef _USE_UBER_SHADER
		// Recording CommandList
		{
			mMultiCommandList[ThreadIDX]->RSSetViewports(1, &mScreenViewport);
			mMultiCommandList[ThreadIDX]->RSSetScissorRects(1, &mScissorRect);

			// Specify the buffers we are going to render to.
			mMultiCommandList[ThreadIDX]->OMSetRenderTargets(1, &mOffscreenRT->Rtv(), true, &mDsvHeap->GetCPUDescriptorHandleForHeapStart());
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
			mMultiCommandList[ThreadIDX]->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			// �ñ״��� (��ũ���� ��) ���ε�
			mMultiCommandList[ThreadIDX]->SetGraphicsRootSignature(mRootSignature.Get());

			// Select Descriptor Buffer Index
			mMultiCommandList[ThreadIDX]->SetGraphicsRootConstantBufferView(
				0,
				PassCB->Resource()->GetGPUVirtualAddress()
			);

			mMultiCommandList[ThreadIDX]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		// �������� �ε��� ������ Render Item�� �����Ͽ� CommandList�� ���ε�. 
		{
			continueIDX = -1;
			MatIDX = 0;

			// �ν��Ͻ� ���۴� ��� ���� ������Ʈ�� ���� �ν��Ͻ� ������ ��� ������ �ֱ⿡, �������� �Ҵ�.
			// ���� Instance Update �� ��, Instance Buffer���ٰ� �Ϸķ� Instance ������ ���ε� (CopyData) �Ͽ��⿡
			// �� ���� ��� ���� ������Ʈ�� �ν��Ͻ��� ���� �ؾ� ��. (���� �������ҿ��� ���� ��, ������Ʈ ����)

			// count of Objects
			// �ν��Ͻ� ī��Ʈ X ���� ������Ʈ ī��Ʈ�� ����
			for (taskIDX = 0; taskIDX < (_taskEnd - _taskOffset); taskIDX++)
			{
				// ������ ���� ������Ʈ�� �ε���
				gameIDX = mGameObjectIndices[taskIDX];
				_gInstOffset = taskIDX + _taskOffset;

				obj = *std::next(mGameObjects.begin(), gameIDX);
				mMultiCommandList[ThreadIDX]->SetPipelineState(mPSOs[obj->mRenderType].Get());

				mMultiCommandList[ThreadIDX]->IASetVertexBuffers(
					0,
					1,
					&obj->mGeometry.VertexBufferView()
				);
				mMultiCommandList[ThreadIDX]->IASetIndexBuffer(
					&obj->mGeometry.IndexBufferView()
				);

				// Move to Current Stack Pointer
				instanceCBAddress		= instanceCB + _gInstOffset * sizeof(InstanceData);
				lightCBAddress			= lightCB + _gInstOffset * sizeof(LightData);
				pmxBoneCBAddress		= pmxBoneCB;
				rateOfAnimTimeCBAddress = rateOfAnimTimeCB;

				// Select Descriptor Buffer Index
				{
					mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
						2,
						instanceCBAddress
					);
					mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
						4,
						lightCBAddress
					);
					mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
						5,
						pmxBoneCBAddress
					);

					if (obj->mFormat == "PMX")
					{
						mMultiCommandList[ThreadIDX]->SetGraphicsRootConstantBufferView(
							1,
							rateOfAnimTimeCBAddress
						);
					}
				}

				{
					// count of submesh
					for (j = 0; j < obj->SubmeshCount; j++)
					{
						if (obj->Mat.size() == 0 && obj->SkyMat.size() == 0)
							throw std::runtime_error("");

						// Select Texture to Index
						CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
						CD3DX12_GPU_DESCRIPTOR_HANDLE skyTex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

						if (obj->Mat.size() > 0)
						{
							/*matCBAddress = matCB + obj->Mat[j]->MatInstIndex * sizeof(MaterialData);*/
							matCBAddress = matCB;

							// Move to Current Stack Pointer
							tex.Offset(
								obj->Mat[j]->DiffuseSrvHeapIndex,
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

							skyTex.Offset(
								0,
								mCbvSrvUavDescriptorSize
							);
						}

						// Select Descriptor Buffer Index
						{
							mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
								3,
								matCBAddress
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								6,
								tex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								7,
								skyTex
							);
						}

						// ���⼭ Local ����.
						// ���� submesh Geometry�� ����
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

						// ����Ž� �� �ν��Ͻ��� �������� �ʴ´�.
						// ���� �ܰ迡�� ���� SRT Mat�� ���ε� ��ų ��.
					}
				}

				// ���� Geometry�� offset ����
				// _gInstOffset	+= mGameObjects[gameIDX]->InstanceCount;
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
		ID3D12CommandList* cmdsLists[] = { mMultiCommandList[0].Get(), mMultiCommandList[1].Get(), mMultiCommandList[2].Get() };
		mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		mCurrentFence++;

		mCommandQueue->Signal(mFence.Get(), mCurrentFence);

		if (mFence->GetCompletedValue() <= mCurrentFence) {
			HANDLE _event = CreateEvent(nullptr, false, false, nullptr);

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
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs[RenderItem::RenderType::_POST_PROCESSING_PIPELINE].Get()));

		// ��ũ���� ���ε�
		ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		//mCommandList->SetPipelineState(mPSOs[RenderItem::RenderType::_POST_PROCESSING_PIPELINE].Get());

		mCommandList->RSSetViewports(1, &mScreenViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		// Specify the buffers we are going to render to.
		mCommandList->OMSetRenderTargets(1, &mOffscreenRT->Rtv(), true, &DepthStencilView());

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
			mPSOs[RenderItem::RenderType::_SOBEL_COMPUTE_TYPE].Get(),
			mOffscreenRT->Srv()
		);

		mCommandList->OMSetRenderTargets(
			1,
			&CurrentBackBufferView(),
			true,
			&DepthStencilView()
		);

		mCommandList->SetGraphicsRootSignature(mSobelRootSignature.Get());
		mCommandList->SetPipelineState(mPSOs[RenderItem::RenderType::_COMPOSITE_COMPUTE_TYPE].Get());
		mCommandList->SetGraphicsRootDescriptorTable(0, mOffscreenRT->Srv());
		mCommandList->SetGraphicsRootDescriptorTable(1, mSobelFilter->OutputSrv());

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
			mPSOs[RenderItem::RenderType::_BLUR_HORZ_COMPUTE_TYPE].Get(),
			mPSOs[RenderItem::RenderType::_BLUR_VERT_COMPUTE_TYPE].Get(),
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

		mCommandList->Close();
	}

	{
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
	mLastMousePos.x = x;
	mLastMousePos.y = y;

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
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
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
	CD3DX12_DESCRIPTOR_RANGE skyTexTable;
	skyTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 2, 0);

	// Root parameter can be a table, root descriptor or root constants.
	std::array<CD3DX12_ROOT_PARAMETER, 8> slotRootParameter;

	// PassCB
	slotRootParameter[0].InitAsConstantBufferView(0);
	// Animation Index and Time Space
	slotRootParameter[1].InitAsConstantBufferView(1);
	// Instance SRV
	slotRootParameter[2].InitAsShaderResourceView(0, 0);
	// Material SRV
	slotRootParameter[3].InitAsShaderResourceView(0, 1);
	// Light SRV
	slotRootParameter[4].InitAsShaderResourceView(0, 2);
	// PMX BONE CB
	slotRootParameter[5].InitAsShaderResourceView(0, 3);
	// Main Textures
	slotRootParameter[6].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	// Sky Textures
	slotRootParameter[7].InitAsDescriptorTable(1, &skyTexTable, D3D12_SHADER_VISIBILITY_PIXEL);


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
	CD3DX12_DESCRIPTOR_RANGE srvTable0;
	srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

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

void BoxApp::BuildDescriptorHeaps()
{
	const int texCount = (const int)mTextures.size();
	const int blurDescriptorCount = 4;
	const int sobelDescriptorCount = mSobelFilter->DescriptorCount();
	const int postProcessDescriptorCount = 4;

	D3D12_DESCRIPTOR_HEAP_DESC SrvUavHeapDesc;
	SrvUavHeapDesc.NumDescriptors = texCount + blurDescriptorCount + sobelDescriptorCount + postProcessDescriptorCount;
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

	// Build Descriptors 
	mBlurFilter->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			(int)texCount,
			mCbvSrvUavDescriptorSize
		),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			(int)texCount,
			mCbvSrvUavDescriptorSize
		),
		mCbvSrvUavDescriptorSize
	);

	mSobelFilter->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			(int)(texCount + blurDescriptorCount),
			mCbvSrvUavDescriptorSize
		),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			(int)(texCount + blurDescriptorCount),
			mCbvSrvUavDescriptorSize
		),
		mCbvSrvUavDescriptorSize
	);

	mOffscreenRT->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			(int)(texCount + blurDescriptorCount + sobelDescriptorCount),
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			(int)(texCount + blurDescriptorCount + sobelDescriptorCount),
			(UINT)mCbvSrvUavDescriptorSize
		),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
			SwapChainBufferCount,
			(UINT)mRtvDescriptorSize
		)
	);

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

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", alphaTestDefines, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["pmxFormatVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", pmxFormatDefines, "VS", "vs_5_1");

	mShaders["pix"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["horzBlurCS"] = d3dUtil::CompileShader(L"Shaders\\Blur.hlsl", nullptr, "HorzBlurCS", "cs_5_0");
	mShaders["vertBlurCS"] = d3dUtil::CompileShader(L"Shaders\\Blur.hlsl", nullptr, "VertBlurCS", "cs_5_0");

	mShaders["compositeVS"] = d3dUtil::CompileShader(L"Shaders\\Composite.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["compositePS"] = d3dUtil::CompileShader(L"Shaders\\Composite.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["sobelCS"] = d3dUtil::CompileShader(L"Shaders\\Sobel.hlsl", nullptr, "SobelCS", "cs_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

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
		obj = *objIDX;

		InstanceNum += (2 * (obj->InstanceCount));
		SubmeshNum += obj->SubmeshCount;

		if (mGameObjectDatas[obj->mName]->mFormat == "PMX")
			BoneNum = mGameObjectDatas[obj->mName]->mModel.bone_count;
	}

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(md3dDevice.Get(), 1, true);
	RateOfAnimTimeCB	= 
		std::make_unique<UploadBuffer<RateOfAnimTimeConstants>>(md3dDevice.Get(), 1, true);
	InstanceBuffer		= 
		std::make_unique<UploadBuffer<InstanceData>>(md3dDevice.Get(), InstanceNum, false);
	MaterialBuffer		= 
		std::make_unique<UploadBuffer<MaterialData>>(md3dDevice.Get(), mMaterials.size(), false);
	LightBuffer =
		std::make_unique<UploadBuffer<LightData>>(md3dDevice.Get(), InstanceNum, false);
	PmxAnimationBuffer	= std::make_unique<UploadBuffer<PmxAnimationData>>(md3dDevice.Get(), BoneNum, false);
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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[RenderItem::RenderType::_OPAQUE_RENDER_TYPE])));


	//
	// PSO for _POST_PROCESSING_PIPELINE
	//
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[RenderItem::RenderType::_POST_PROCESSING_PIPELINE])));

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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyDesc, IID_PPV_ARGS(&mPSOs[RenderItem::RenderType::_SKY_FORMAT_RENDER_TYPE])));

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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedDesc, IID_PPV_ARGS(&mPSOs[RenderItem::RenderType::_OPAQUE_SKINNED_RENDER_TYPE])));

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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&pmxFormatDesc, IID_PPV_ARGS(&mPSOs[RenderItem::RenderType::_PMX_FORMAT_RENDER_TYPE])));

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
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&horzBlurPSO, IID_PPV_ARGS(&mPSOs[RenderItem::RenderType::_BLUR_HORZ_COMPUTE_TYPE])));

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
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&vertBlurPSO, IID_PPV_ARGS(&mPSOs[RenderItem::RenderType::_BLUR_VERT_COMPUTE_TYPE])));

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
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&sobelPSO, IID_PPV_ARGS(&mPSOs[RenderItem::RenderType::_SOBEL_COMPUTE_TYPE])));

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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&compositePSO, IID_PPV_ARGS(&mPSOs[RenderItem::RenderType::_COMPOSITE_COMPUTE_TYPE])));
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
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp, shadow };
}

void BoxApp::BuildRenderItem()
{
	for (RenderItem* go : mGameObjects)
	{
		if (go->mFormat != "PMX")
		{
			ObjectData* v = mGameObjectDatas[go->mName];
			// CPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				go->mGeometry.VertexBufferByteSize,
				&go->mGeometry.VertexBufferCPU
			));
			CopyMemory(
				go->mGeometry.VertexBufferCPU->GetBufferPointer(),
				mGameObjectDatas[go->mName]->vertices.data(),
				go->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				go->mGeometry.IndexBufferByteSize,
				&go->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				go->mGeometry.IndexBufferCPU->GetBufferPointer(),
				mGameObjectDatas[go->mName]->indices.data(),
				go->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			go->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				mGameObjectDatas[go->mName]->vertices.data(),
				go->mGeometry.VertexBufferByteSize,
				go->mGeometry.VertexBufferUploader);

			// GPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			go->mGeometry.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				mGameObjectDatas[go->mName]->indices.data(),
				go->mGeometry.IndexBufferByteSize,
				go->mGeometry.IndexBufferUploader);

			go->mGeometry.VertexByteStride = sizeof(Vertex);
			go->mGeometry.IndexFormat = DXGI_FORMAT_R32_UINT;
		}
		else if (go->mFormat == "PMX")
		{
			pmx::PmxVertexSkinningBDEF1* BDEF1 = NULL;
			pmx::PmxVertexSkinningBDEF2* BDEF2 = NULL;
			pmx::PmxVertexSkinningBDEF4* BDEF4 = NULL;

			ObjectData* v = mGameObjectDatas[go->mName];

			pmx::PmxVertex* mFromVert = v->mModel.vertices.get();
			Vertex* mToVert;

			int vSize = v->mModel.vertex_count;
			v->vertices.resize(vSize);

			for (int vLoop = 0; vLoop < vSize; vLoop++)
			{
				mToVert = &v->vertices[vLoop];

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

				if (v->mModel.vertices[vLoop].skinning_type == pmx::PmxVertexSkinningType::BDEF1)
				{
					BDEF1 = (pmx::PmxVertexSkinningBDEF1*)v->mModel.vertices[vLoop].skinning.get();

					mToVert->BoneIndices.x = BDEF1->bone_index;
					mToVert->BoneWeights.x = 1.0f;

					mToVert->BoneIndices.y = -1;
					mToVert->BoneWeights.y = 0.0f;

					mToVert->BoneIndices.z = -1;
					mToVert->BoneWeights.z = 0.0f;

					mToVert->BoneIndices.w = -1;
					mToVert->BoneWeights.w = 0.0f;

				}
				else if (v->mModel.vertices[vLoop].skinning_type == pmx::PmxVertexSkinningType::BDEF2)
				{
					BDEF2 = (pmx::PmxVertexSkinningBDEF2*)v->mModel.vertices[vLoop].skinning.get();

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
					BDEF4 = (pmx::PmxVertexSkinningBDEF4*)v->mModel.vertices[vLoop].skinning.get();

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
				go->mGeometry.VertexBufferByteSize,
				&go->mGeometry.VertexBufferCPU
			));
			CopyMemory(
				go->mGeometry.VertexBufferCPU->GetBufferPointer(),
				v->vertices.data(),
				go->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			ThrowIfFailed(D3DCreateBlob(
				go->mGeometry.IndexBufferByteSize,
				&go->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				go->mGeometry.IndexBufferCPU->GetBufferPointer(),
				v->mModel.indices.get(),
				go->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer�� �Ҵ��Ͽ� Vertices Data�� �Է�
			go->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				v->vertices.data(),
				go->mGeometry.VertexBufferByteSize,
				go->mGeometry.VertexBufferUploader);

			// GPU Buffer�� �Ҵ��Ͽ� Indices Data�� �Է�
			go->mGeometry.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				v->mModel.indices.get(),
				go->mGeometry.IndexBufferByteSize,
				go->mGeometry.IndexBufferUploader);

			go->mGeometry.VertexByteStride = sizeof(Vertex);
			go->mGeometry.IndexFormat = DXGI_FORMAT_R32_UINT;

			//mGameObjectDatas[go->mName]->vertices.clear();
			//mGameObjectDatas[go->mName]->vertices.resize(0);
		}
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

	// Bind Phy
	newGameObjects->mPhyxResources.resize(instance);
	for (int i = 0; i < instance; i++) {
		staticObj = mPhys.CreateStatic(PxTransform(PxVec3(0, 0, 0)), sphere);

		mPhys.BindObjColliber(
			staticObj,
			&newGameObjects->mPhyxResources[i]
		);
	}

	// Create Collider Body
	//phys.CreateBox(1, 1, 1);

	newGameObjects->ObjCBIndex = 0;
	newGameObjects->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	newGameObjects->InstanceCount = instance;

	// Push Instance
	for (UINT i = 0; i < newGameObjects->InstanceCount; i++)
	{
		InstanceData id;
		id.World = MathHelper::Identity4x4();
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		newGameObjects->mInstances.push_back(id);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = new ObjectData();
	mGameObjectDatas[Name]->mName = Name;

	return mGameObjects.back();
}


RenderItem* BoxApp::CreateKinematicGameObject(std::string Name, int instance)
{
	RenderItem* newGameObjects = new RenderItem();
	PxRigidDynamic* dynamicObj = nullptr;
	PxShape* sphere = nullptr;

	sphere = mPhys.CreateSphere(1.0f);

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

	// Bind Phy
	newGameObjects->mPhyxResources.resize(instance);
	for (int i = 0; i < instance; i++) {
		dynamicObj = mPhys.CreateKinematic(PxTransform(PxVec3(0, 0, 0)), sphere, 1, PxVec3(0, 0, 0));
		newGameObjects->mPhyxRigidBody.push_back(dynamicObj);

		mPhys.BindObjColliber(
			dynamicObj,
			&newGameObjects->mPhyxResources[i]
		);
	}

	// Create Collider Body
	//phys.CreateBox(1, 1, 1);

	newGameObjects->ObjCBIndex = 0;
	newGameObjects->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	newGameObjects->InstanceCount = instance;

	// Push Instance
	for (UINT i = 0; i < newGameObjects->InstanceCount; i++)
	{
		InstanceData id;
		id.World = MathHelper::Identity4x4();
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		newGameObjects->mInstances.push_back(id);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = new ObjectData();
	mGameObjectDatas[Name]->mName = Name;

	return mGameObjects.back();
}

RenderItem* BoxApp::CreateDynamicGameObject(std::string Name, int instance)
{
	RenderItem* newGameObjects = new RenderItem();
	PxRigidDynamic* dynamicObj = nullptr;

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

	// Bind Phy
	newGameObjects->mPhyxResources.resize(instance);
	for (int i = 0; i < instance; i++) {
		dynamicObj = mPhys.CreateDynamic(PxTransform(PxVec3(0, 0, 0)), PxSphereGeometry(3), PxVec3(0, 0, 0));
		newGameObjects->mPhyxRigidBody.push_back(dynamicObj);

		mPhys.BindObjColliber(
			dynamicObj,
			&newGameObjects->mPhyxResources[i]
		);
	}

	newGameObjects->ObjCBIndex = 0;
	newGameObjects->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	newGameObjects->InstanceCount = instance;

	// Push Instance
	for (UINT i = 0; i < newGameObjects->InstanceCount; i++)
	{
		InstanceData id;
		id.World = MathHelper::Identity4x4();
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		newGameObjects->mInstances.push_back(id);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = new ObjectData();
	mGameObjectDatas[Name]->mName = Name;

	return mGameObjects.back();
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
	RenderItem::RenderType renderType
)
{
	GeometryGenerator Geom;
	{
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

		InstanceData id;
		DirectX::XMStoreFloat4x4(&id.World, mWorldMat);
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		if (r->mInstances.size() == 0)
			throw std::runtime_error("Instance Size must bigger than 0.");

		r->mInstances.at(0) = id;
	}

	r->mFormat = "";
	r->mRenderType = renderType;

	// input subGeom
	GeometryGenerator::MeshData Box;
	Box = Geom.CreateBox(x, y, z, subDividNum);
	mGameObjectDatas[r->mName]->SubmeshCount += 1;
	mGameObjectDatas[r->mName]->mDesc.resize(mGameObjectDatas[r->mName]->SubmeshCount);

	mGameObjectDatas[r->mName]->isCloth.push_back(false);
	mGameObjectDatas[r->mName]->isRigidBody.push_back(false);

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation = boxSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation = boxSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize = boxSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize = boxSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	r->mGeometry.subMeshCount += 1;
	r->mGeometry.DrawArgs[Name.c_str()] = boxSubmesh;
	r->mGeometry.DrawArgs[Name.c_str()].textureName = (textuerName.c_str());
	r->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = mGameObjectDatas[r->mName]->vertices.size();
	XMVECTOR posV;

	mGameObjectDatas[r->mName]->vertices.resize(startV + Box.Vertices.size());

	Vertex* v = mGameObjectDatas[r->mName]->vertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	mGameObjectDatas[r->mName]->indices.insert(
		mGameObjectDatas[r->mName]->indices.end(),
		std::begin(Box.Indices32),
		std::end(Box.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	r->mGeometry.VertexBufferByteSize += (UINT)Box.Vertices.size() * sizeof(Vertex);
	r->mGeometry.IndexBufferByteSize += (UINT)Box.Indices32.size() * sizeof(std::uint32_t);
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
	RenderItem::RenderType renderType
)
{
	GeometryGenerator Geom;
	{
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

		InstanceData id;
		DirectX::XMStoreFloat4x4(&id.World, mWorldMat);
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		if (r->mInstances.size() == 0)
			throw std::runtime_error("Instance Size must bigger than 0.");

		r->mInstances.at(0) = id;
	}

	r->mFormat = "";
	r->mRenderType = renderType;

	// input subGeom
	GeometryGenerator::MeshData Sphere;
	Sphere = Geom.CreateSphere(rad, sliceCount, stackCount);
	mGameObjectDatas[r->mName]->SubmeshCount = 1;
	mGameObjectDatas[r->mName]->mDesc.resize(1);

	mGameObjectDatas[r->mName]->isCloth.push_back(false);
	mGameObjectDatas[r->mName]->isRigidBody.push_back(false);

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	sphereSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	sphereSubmesh.IndexSize = (UINT)Sphere.Indices32.size();
	sphereSubmesh.VertexSize = (UINT)Sphere.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation = sphereSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation = sphereSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize = sphereSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize = sphereSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	r->mGeometry.subMeshCount += 1;
	r->mGeometry.DrawArgs[Name.c_str()] = sphereSubmesh;
	r->mGeometry.DrawArgs[Name.c_str()].textureName = textuerName.c_str();
	r->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = mGameObjectDatas[r->mName]->vertices.size();
	XMVECTOR posV;

	mGameObjectDatas[r->mName]->vertices.resize(startV + Sphere.Vertices.size());

	Vertex* v = mGameObjectDatas[r->mName]->vertices.data();
	for (size_t i = 0; i < (Sphere.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Sphere.Vertices[i].Position);

		v[i + startV].Normal = Sphere.Vertices[i].Normal;
		v[i + startV].TexC = Sphere.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	mGameObjectDatas[r->mName]->indices.insert(mGameObjectDatas[r->mName]->indices.end(), std::begin(
		Sphere.Indices32),
		std::end(Sphere.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	r->mGeometry.VertexBufferByteSize += (UINT)Sphere.Vertices.size() * sizeof(Vertex);
	r->mGeometry.IndexBufferByteSize += (UINT)Sphere.Indices32.size() * sizeof(std::uint32_t);
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
	RenderItem::RenderType renderType
)
{
	GeometryGenerator Geom;
	{
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

		InstanceData id;
		DirectX::XMStoreFloat4x4(&id.World, mWorldMat);
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		if (r->mInstances.size() == 0)
			throw std::runtime_error("Instance Size must bigger than 0.");

		r->mInstances.at(0) = id;
	}

	r->mFormat = "";
	r->mRenderType = renderType;

	// input subGeom
	GeometryGenerator::MeshData Sphere;
	Sphere = Geom.CreateGeosphere(rad, subdivid);
	mGameObjectDatas[r->mName]->SubmeshCount = 1;
	mGameObjectDatas[r->mName]->mDesc.resize(1);

	mGameObjectDatas[r->mName]->isCloth.push_back(false);
	mGameObjectDatas[r->mName]->isRigidBody.push_back(false);

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	sphereSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	sphereSubmesh.IndexSize = (UINT)Sphere.Indices32.size();
	sphereSubmesh.VertexSize = (UINT)Sphere.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation = sphereSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation = sphereSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize = sphereSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize = sphereSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	r->mGeometry.subMeshCount += 1;
	r->mGeometry.DrawArgs[Name.c_str()] = sphereSubmesh;
	r->mGeometry.DrawArgs[Name.c_str()].textureName = textuerName.c_str();
	r->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = mGameObjectDatas[r->mName]->vertices.size();
	XMVECTOR posV;

	mGameObjectDatas[r->mName]->vertices.resize(startV + Sphere.Vertices.size());

	Vertex* v = mGameObjectDatas[r->mName]->vertices.data();
	for (size_t i = 0; i < (Sphere.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Sphere.Vertices[i].Position);

		v[i + startV].Normal = Sphere.Vertices[i].Normal;
		v[i + startV].TexC = Sphere.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	mGameObjectDatas[r->mName]->indices.insert(
		mGameObjectDatas[r->mName]->indices.end(),
		std::begin(Sphere.Indices32),
		std::end(Sphere.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	r->mGeometry.VertexBufferByteSize += (UINT)Sphere.Vertices.size() * sizeof(Vertex);
	r->mGeometry.IndexBufferByteSize += (UINT)Sphere.Indices32.size() * sizeof(std::uint32_t);
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
	RenderItem::RenderType renderType
)
{
	GeometryGenerator Geom;
	{
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

		InstanceData id;
		DirectX::XMStoreFloat4x4(&id.World, mWorldMat);
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		if (r->mInstances.size() == 0)
			throw std::runtime_error("Instance Size must bigger than 0.");

		r->mInstances.at(0) = id;
	}

	r->mFormat = "";
	r->mRenderType = renderType;

	// input subGeom
	GeometryGenerator::MeshData Box;
	Box = Geom.CreateCylinder(bottomRad, topRad, height, sliceCount, stackCount);
	mGameObjectDatas[r->mName]->SubmeshCount = 1;
	mGameObjectDatas[r->mName]->mDesc.resize(1);

	mGameObjectDatas[r->mName]->isCloth.push_back(false);
	mGameObjectDatas[r->mName]->isRigidBody.push_back(false);

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation = boxSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation = boxSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize = boxSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize = boxSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	r->mGeometry.subMeshCount += 1;
	r->mGeometry.DrawArgs[Name.c_str()] = boxSubmesh;
	r->mGeometry.DrawArgs[Name.c_str()].textureName = textuerName.c_str();
	r->mGeometry.meshNames.push_back(Name.c_str());


	size_t startV = mGameObjectDatas[r->mName]->vertices.size();
	XMVECTOR posV;

	mGameObjectDatas[r->mName]->vertices.resize(startV + Box.Vertices.size());

	Vertex* v = mGameObjectDatas[r->mName]->vertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	mGameObjectDatas[r->mName]->indices.insert(
		mGameObjectDatas[r->mName]->indices.end(),
		std::begin(Box.Indices32),
		std::end(Box.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	r->mGeometry.VertexBufferByteSize += (UINT)Box.Vertices.size() * sizeof(Vertex);
	r->mGeometry.IndexBufferByteSize += (UINT)Box.Indices32.size() * sizeof(std::uint32_t);
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
	RenderItem::RenderType renderType
)
{
	GeometryGenerator Geom;
	{
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

		InstanceData id;
		DirectX::XMStoreFloat4x4(&id.World, mWorldMat);
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		if (r->mInstances.size() == 0)
			throw std::runtime_error("Instance Size must bigger than 0.");

		r->mInstances.at(0) = id;
	}

	r->mFormat = "";
	r->mRenderType = renderType;

	// input subGeom
	GeometryGenerator::MeshData Box;
	Box = Geom.CreateGrid(w, h, wc, hc);
	mGameObjectDatas[r->mName]->SubmeshCount = 1;
	mGameObjectDatas[r->mName]->mDesc.resize(1);

	mGameObjectDatas[r->mName]->isCloth.push_back(false);
	mGameObjectDatas[r->mName]->isRigidBody.push_back(false);

	// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation = boxSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation = boxSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize = boxSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize = boxSubmesh.VertexSize;

	// Submesh ����
	r->SubmeshCount += 1;

	r->mGeometry.subMeshCount += 1;
	r->mGeometry.DrawArgs[Name.c_str()] = boxSubmesh;
	r->mGeometry.DrawArgs[Name.c_str()].textureName = textuerName.c_str();
	r->mGeometry.meshNames.push_back(Name.c_str());

	size_t startV = mGameObjectDatas[r->mName]->vertices.size();
	XMVECTOR posV;

	mGameObjectDatas[r->mName]->vertices.resize(startV + Box.Vertices.size());

	Vertex* v = mGameObjectDatas[r->mName]->vertices.data();
	for (size_t i = 0; i < (Box.Vertices.size()); ++i)
	{
		posV = XMLoadFloat3(&Box.Vertices[i].Position);

		v[i + startV].Normal = Box.Vertices[i].Normal;
		v[i + startV].TexC = Box.Vertices[i].TexC;

		XMStoreFloat3(&v[i + startV].Pos, posV);
	}

	mGameObjectDatas[r->mName]->indices.insert(
		mGameObjectDatas[r->mName]->indices.end(),
		std::begin(Box.Indices32),
		std::end(Box.Indices32)
	);

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	r->mGeometry.VertexBufferByteSize += (UINT)Box.Vertices.size() * sizeof(Vertex);
	r->mGeometry.IndexBufferByteSize += (UINT)Box.Indices32.size() * sizeof(std::uint32_t);
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
	bool uvMode
)
{
	GeometryGenerator Geom;
	mGameObjectDatas[r->mName]->mFormat = "FBX";

	{
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

		InstanceData id;
		DirectX::XMStoreFloat4x4(&id.World, mWorldMat);
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		for (int i = 0; i < r->mInstances.size(); i++)
			r->mInstances.at(i) = id;
	}

	r->mFormat = "FBX";
	r->mRenderType = RenderItem::RenderType::_OPAQUE_RENDER_TYPE;

	mRenderTypeCount[r->mRenderType] += 1;

	// input subGeom
	std::vector<GeometryGenerator::MeshData> meshData;
	Geom.CreateFBXModel(
		meshData,
		(Path + "\\" + FileName),
		uvMode
	);

	mGameObjectDatas[r->mName]->SubmeshCount = (UINT)meshData.size();
	mGameObjectDatas[r->mName]->mDesc.resize(meshData.size());

	size_t startV = mGameObjectDatas[r->mName]->vertices.size();

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	for (int subMeshCount = 0; subMeshCount < meshData.size(); ++subMeshCount)
	{
		SubmeshGeometry boxSubmesh;
		std::string submeshName = Name + std::to_string(subMeshCount);

		// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
		boxSubmesh.IndexSize = (UINT)meshData[subMeshCount].Indices32.size();
		boxSubmesh.VertexSize = (UINT)meshData[subMeshCount].Vertices.size();

		mGameObjectDatas[r->mName]->mDesc[subMeshCount].BaseVertexLocation = vertexOffset;
		mGameObjectDatas[r->mName]->mDesc[subMeshCount].StartIndexLocation = indexOffset;

		mGameObjectDatas[r->mName]->mDesc[subMeshCount].IndexSize = boxSubmesh.IndexSize;
		mGameObjectDatas[r->mName]->mDesc[subMeshCount].VertexSize = boxSubmesh.VertexSize;

		// �� ����޽��� Cloth physix �ε����� �ʱ�ȭ
		mGameObjectDatas[r->mName]->isCloth.push_back(false);
		mGameObjectDatas[r->mName]->isRigidBody.push_back(false);

		// Submesh ����
		r->SubmeshCount += 1;

		r->mGeometry.subMeshCount += 1;
		r->mGeometry.DrawArgs[submeshName] = boxSubmesh;
		r->mGeometry.DrawArgs[submeshName].name = d3dUtil::getName(meshData[subMeshCount].texPath);
		r->mGeometry.DrawArgs[submeshName].textureName = meshData[subMeshCount].texPath;

		r->mGeometry.DrawArgs[submeshName].BaseVertexLocation = vertexOffset;
		r->mGeometry.DrawArgs[submeshName].StartIndexLocation = indexOffset;

		// �ش� �̸��� �Ž��� ����Ǿ� ������ �˸��� ���� �̸��� ����
		r->mGeometry.meshNames.push_back(submeshName);
		texturePath.push_back(r->mGeometry.DrawArgs[submeshName].textureName);

		// _Geom ������ �����ϱ⿡ ���ؽ� ������ �������� �̸� ���صд�.
		startV = mGameObjectDatas[r->mName]->vertices.size();
		// ���ο� Submesh�� �� ������ �����Ѵ�.
		mGameObjectDatas[r->mName]->vertices.resize(startV + meshData[subMeshCount].Vertices.size());
		Vertex* v = mGameObjectDatas[r->mName]->vertices.data();
		// _Geometry�� �ϳ��� ������ ���� ���� ������ ������Ʈ���� �� �� ���� ���ؽ� ���� �ֱ��Ѵ�.
		// �̴� ���Ŀ� Deprecated �� ��.
		for (size_t i = 0; i < (meshData[subMeshCount].Vertices.size()); ++i)
		{
			v[i + startV].Pos = meshData[subMeshCount].Vertices[i].Position;
			v[i + startV].Normal = meshData[subMeshCount].Vertices[i].Normal;
			v[i + startV].Tangent = meshData[subMeshCount].Vertices[i].TangentU;
			v[i + startV].TexC = meshData[subMeshCount].Vertices[i].TexC;
		}

		mGameObjectDatas[r->mName]->indices.insert(
			mGameObjectDatas[r->mName]->indices.end(),
			std::begin(meshData[subMeshCount].Indices32),
			std::end(meshData[subMeshCount].Indices32)
		);

		// Texture, Material �ڵ� ����
		{
			// ���� �ؽ��İ� �����Ѵٸ�
			if (r->mGeometry.DrawArgs[submeshName].textureName != "")
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

		startV += boxSubmesh.VertexSize;

		// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
		vertexOffset += boxSubmesh.VertexSize;
		indexOffset += boxSubmesh.IndexSize;

		r->mGeometry.VertexBufferByteSize += boxSubmesh.VertexSize * sizeof(Vertex);
		r->mGeometry.IndexBufferByteSize += boxSubmesh.IndexSize * sizeof(std::uint32_t);
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
	bool uvMode
)
{
	// input subGeom
	std::vector<GeometryGenerator::MeshData> meshData;

	GeometryGenerator Geom;
	mGameObjectDatas[r->mName]->mFormat = "FBX";

	{
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

		InstanceData id;
		DirectX::XMStoreFloat4x4(&id.World, mWorldMat);
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		for (int i = 0; i < r->mInstances.size(); i++)
			r->mInstances.at(i) = id;
	}


	r->mFormat = "FBX";
	r->mRenderType = RenderItem::RenderType::_OPAQUE_SKINNED_RENDER_TYPE;

	mRenderTypeCount[r->mRenderType] += 1;

	int res = Geom.CreateFBXSkinnedModel(
		meshData,
		(Path + "\\" + FileName),
		mGameObjectDatas[r->mName]->animNameLists,
		mGameObjectDatas[r->mName]->mStart,
		mGameObjectDatas[r->mName]->mStop,
		mGameObjectDatas[r->mName]->countOfFrame,
		mGameObjectDatas[r->mName]->mAnimVertex,
		mGameObjectDatas[r->mName]->mAnimVertexSize,
		uvMode
	);

	assert(!res && "(CreateFBXObject)Failed to create FBX Model on CreateFBXObject.");

	// �� �ִϸ��̼��� �ϳ��� ������ �� �ð�(�� ����)�� �ִϸ��̼��� ��ü �෹�̼� (�� ����)�� ���ɴϴ�.
	for (int animCount = 0; animCount < mGameObjectDatas[r->mName]->countOfFrame.size(); animCount++)
	{
		mGameObjectDatas[r->mName]->durationPerSec.push_back((float)mGameObjectDatas[r->mName]->mStop[animCount].GetSecondDouble());
		mGameObjectDatas[r->mName]->durationOfFrame.push_back(mGameObjectDatas[r->mName]->durationPerSec[animCount] / mGameObjectDatas[r->mName]->countOfFrame[animCount]);
	}

	mGameObjectDatas[r->mName]->beginAnimIndex = 0;
	mGameObjectDatas[r->mName]->currentFrame = 0;
	mGameObjectDatas[r->mName]->currentDelayPerSec = 0;

	mGameObjectDatas[r->mName]->endAnimIndex =
		(float)mGameObjectDatas[r->mName]->durationPerSec[0] /
		mGameObjectDatas[r->mName]->durationOfFrame[0];

	std::vector<PxClothParticle> vertices;
	std::vector<PxU32> primitives;

	mGameObjectDatas[r->mName]->SubmeshCount = (UINT)meshData.size();
	mGameObjectDatas[r->mName]->mDesc.resize(meshData.size());

	// �� Submesh�� Offset�� �����ϴ� �뵵
	size_t startV = mGameObjectDatas[r->mName]->vertices.size();

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	for (int subMeshCount = 0; subMeshCount < meshData.size(); ++subMeshCount)
	{
		SubmeshGeometry boxSubmesh;
		std::string submeshName = Name + std::to_string(subMeshCount);

		// �ϳ��� ������Ʈ ���� ��, �ش� ������Ʈ�� ����(������, ������)�� ����
		boxSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
		boxSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

		boxSubmesh.IndexSize = (UINT)meshData[subMeshCount].Indices32.size();
		boxSubmesh.VertexSize = (UINT)meshData[subMeshCount].Vertices.size();

		mGameObjectDatas[r->mName]->mDesc[subMeshCount].BaseVertexLocation = vertexOffset;
		mGameObjectDatas[r->mName]->mDesc[subMeshCount].StartIndexLocation = indexOffset;

		mGameObjectDatas[r->mName]->mDesc[subMeshCount].IndexSize = boxSubmesh.IndexSize;
		mGameObjectDatas[r->mName]->mDesc[subMeshCount].VertexSize = boxSubmesh.VertexSize;

		// �� ����޽��� Cloth physix �ε����� �ʱ�ȭ
		mGameObjectDatas[r->mName]->isCloth.push_back(false);
		mGameObjectDatas[r->mName]->isRigidBody.push_back(false);

		// Submesh ����
		r->SubmeshCount += 1;

		r->mGeometry.subMeshCount += 1;
		r->mGeometry.DrawArgs[submeshName] = boxSubmesh;
		r->mGeometry.DrawArgs[submeshName].name = d3dUtil::getName(meshData[subMeshCount].texPath);
		r->mGeometry.DrawArgs[submeshName].textureName = meshData[subMeshCount].texPath;

		r->mGeometry.DrawArgs[submeshName].BaseVertexLocation = vertexOffset;
		r->mGeometry.DrawArgs[submeshName].StartIndexLocation = indexOffset;

		////////////////

		// �ش� �̸��� �Ž��� ����Ǿ� ������ �˸��� ���� �̸��� ����
		r->mGeometry.meshNames.push_back(submeshName);
		texturePath.push_back(r->mGeometry.DrawArgs[submeshName].textureName);

		// _Geom ������ �����ϱ⿡ ���ؽ� ������ �������� �̸� ���صд�.
		startV = (UINT)mGameObjectDatas[r->mName]->vertices.size();
		// ���ο� Submesh�� �� ������ �����Ѵ�.
		mGameObjectDatas[r->mName]->vertices.resize(startV + meshData[subMeshCount].Vertices.size());
		Vertex* v = mGameObjectDatas[r->mName]->vertices.data();

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

		mGameObjectDatas[r->mName]->indices.insert(
			mGameObjectDatas[r->mName]->indices.end(),
			std::begin(meshData[subMeshCount].Indices32),
			std::end(meshData[subMeshCount].Indices32)
		);

		// Texture, Material �ڵ� ����
		{
			// ���� �ؽ��İ� �����Ѵٸ�
			if (r->mGeometry.DrawArgs[submeshName].textureName != "")
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

		r->mGeometry.VertexBufferByteSize += (UINT)meshData[subMeshCount].Vertices.size() * sizeof(Vertex);
		r->mGeometry.IndexBufferByteSize += (UINT)meshData[subMeshCount].Indices32.size() * sizeof(std::uint32_t);
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
	DirectX::XMFLOAT3 scale
)
{
	// meshData�� 0������ Vertices �迭
	// meshData�� 1���� ���� ���ʹ� �� Submesh�� ���� Indices
	std::vector<PxClothParticle> vertices;
	std::vector<PxU32> primitives;

	pmx::PmxModel* model = &mGameObjectDatas[r->mName]->mModel;
	{
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

		InstanceData id;
		DirectX::XMStoreFloat4x4(&id.World, mWorldMat);
		id.TexTransform = MathHelper::Identity4x4();
		id.MaterialIndex = 0;

		for (int i = 0; i < r->mInstances.size(); i++)
			r->mInstances.at(i) = id;
	}


	// Read PMX
	model->Init();

	std::string _FullFilePath = Path + "\\" + FileName;
	std::ifstream stream(_FullFilePath.c_str(), std::ifstream::binary);
	model->Read(&stream);

	r->mFormat = "PMX";
	mGameObjectDatas[r->mName]->mFormat = "PMX";
	r->mRenderType = RenderItem::RenderType::_PMX_FORMAT_RENDER_TYPE;

	mRenderTypeCount[r->mRenderType] += 1;

	// Load Submesh Count
	int _SubMeshCount = model->material_count;

	// GameObjects Data�� ���� ���� ����
	mGameObjectDatas[r->mName]->SubmeshCount = _SubMeshCount;
	mGameObjectDatas[r->mName]->mDesc.resize(_SubMeshCount);

	//////////////////////////////////////////////////////////////
	// �ܺ� �ִϸ��̼� ���� �о� Vertex�� ������Ʈ
	//////////////////////////////////////////////////////////////
	std::ifstream animBuffer(std::string("resFile"), std::ios::in | std::ios::binary);

	int mAnimFrameCount = 0;
	animBuffer.read((char*)&mAnimFrameCount, sizeof(int));

	std::vector<std::vector<float*>> AssistCalcVar(mAnimFrameCount + 1);

	mGameObjectDatas[r->mName]->mBoneMatrix.resize(mAnimFrameCount + 1);

	for (int i = 0; i < mAnimFrameCount; i++)
	{
		AssistCalcVar[i].resize(model->bone_count);
		mGameObjectDatas[r->mName]->mBoneMatrix[i].resize(model->bone_count);
		for (int j = 0; j < model->bone_count; j++)
		{
			// Mat
			AssistCalcVar[i][j] = new float[8];
		}
	}

	mGameObjectDatas[r->mName]->beginAnimIndex = 0;
	mGameObjectDatas[r->mName]->currentFrame = 0;
	mGameObjectDatas[r->mName]->currentDelayPerSec = 0;

	mGameObjectDatas[r->mName]->endAnimIndex = (float)mAnimFrameCount;

	// Base Origin Bone SRT Matrix
	mGameObjectDatas[r->mName]->mOriginRevMatrix.resize(model->bone_count);

	float* mBoneOriginPositionBuffer = NULL;
	float* mBonePositionBuffer = NULL;

	for (int i = 0; i < mAnimFrameCount; i++)
	{
		for (int j = 0; j < model->bone_count; j++)
		{
			animBuffer.read((char*)AssistCalcVar[i][j], sizeof(float) * 8);
		}
	}

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

			mGameObjectDatas[r->mName]->mMorph.push_back(mMorph);
		}
	}

	std::vector<struct ObjectData::_VERTEX_MORPH_DESCRIPTOR> test = mGameObjectDatas[r->mName]->mMorph;

	mGameObjectDatas[r->mName]->mMorph[19].mVertWeight = 1.0f;
	mGameObjectDatas[r->mName]->mMorphDirty.push_back(9);

	mGameObjectDatas[r->mName]->mMorph[19].mVertWeight = 1.0f;
	mGameObjectDatas[r->mName]->mMorphDirty.push_back(19);

	mGameObjectDatas[r->mName]->mMorph[27].mVertWeight = 1.0f;
	mGameObjectDatas[r->mName]->mMorphDirty.push_back(27);

	mGameObjectDatas[r->mName]->mMorph[41].mVertWeight = 1.0f;
	mGameObjectDatas[r->mName]->mMorphDirty.push_back(41);

	mGameObjectDatas[r->mName]->mMorph[52].mVertWeight = 1.0f;
	mGameObjectDatas[r->mName]->mMorphDirty.push_back(41);

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

		DirectX::XMStoreFloat4x4(&mGameObjectDatas[r->mName]->mOriginRevMatrix[i], M);
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

			DirectX::XMStoreFloat4x4(&mGameObjectDatas[r->mName]->mBoneMatrix[i][j], M);
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

	mGameObjectDatas[r->mName]->mClothWeights.resize(model->vertex_count);
	mGameObjectDatas[r->mName]->isCloth.resize(model->material_count);
	while (true)
	{
		inFile.read((char*)&vertIDX, sizeof(int));
		if (vertIDX == -1)	break;
		inFile.read((char*)&vertWeight, sizeof(float));

		/*mGameObjectDatas[r->mName]->mClothWeights[vertIDX] = vertWeight;*/
		if (vertWeight > 0.6f)
			mGameObjectDatas[r->mName]->mClothWeights[vertIDX] = 0.1f;
		else
			mGameObjectDatas[r->mName]->mClothWeights[vertIDX] = vertWeight * 0.02f;
	}

	int count = 0;
	while (count < model->material_count)
	{
		inFile.read((char*)&mIsCloth, sizeof(bool));
		mGameObjectDatas[r->mName]->isCloth[count++] = mIsCloth;
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

	mGameObjectDatas[r->mName]->durationPerSec.push_back(mAnimFrameCount);
	mGameObjectDatas[r->mName]->durationOfFrame.push_back((float)pTime);

	////
	UINT32 mCeil = 0;
	UINT32 mIDXAcculation = 0;

	mGameObjectDatas[r->mName]->vertBySubmesh.resize(model->material_count);
	for (int i = 0; i < model->material_count; i++)
	{
		mCeil = mIDXAcculation + model->materials[i].index_count;
		for (UINT j = mIDXAcculation; j < mCeil; j++)
		{
			mGameObjectDatas[r->mName]->vertBySubmesh[i].insert(model->indices[j]);
		}

		mIDXAcculation += model->materials[i].index_count;
	}

	////
	mGameObjectDatas[r->mName]->isRigidBody.resize(_SubMeshCount);
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
		boxSubmesh.VertexSize = (UINT)mGameObjectDatas[r->mName]->vertBySubmesh[subMeshIDX].size();
		boxSubmesh.IndexSize = (UINT)model->materials[subMeshIDX].index_count;

		std::string matName;
		matName.assign(
			model->materials[subMeshIDX].material_english_name.begin(),
			model->materials[subMeshIDX].material_english_name.end()
		);
		if (matName.find("_Rigid_Body") != std::string::npos) {
			mGameObjectDatas[r->mName]->isRigidBody[subMeshIDX] = true;
		}
		else {
			mGameObjectDatas[r->mName]->isRigidBody[subMeshIDX] = false;
		}

		mGameObjectDatas[r->mName]->mDesc[subMeshIDX].BaseVertexLocation = boxSubmesh.BaseVertexLocation;
		mGameObjectDatas[r->mName]->mDesc[subMeshIDX].StartIndexLocation = boxSubmesh.StartIndexLocation;

		mGameObjectDatas[r->mName]->mDesc[subMeshIDX].VertexSize = boxSubmesh.VertexSize;
		mGameObjectDatas[r->mName]->mDesc[subMeshIDX].IndexSize = boxSubmesh.IndexSize;

		// Submesh ����
		r->SubmeshCount += 1;

		r->mGeometry.subMeshCount += 1;
		r->mGeometry.DrawArgs[submeshName] = boxSubmesh;
		if (diffuseTextureIDX >= 0) {
			r->mGeometry.DrawArgs[submeshName].name = d3dUtil::getName(texturePath[diffuseTextureIDX]);
			r->mGeometry.DrawArgs[submeshName].textureName = texturePath[diffuseTextureIDX];
		}
		else {
			r->mGeometry.DrawArgs[submeshName].name = "";
			r->mGeometry.DrawArgs[submeshName].textureName = "";
		}

		r->mGeometry.DrawArgs[submeshName].BaseVertexLocation = 0;
		r->mGeometry.DrawArgs[submeshName].StartIndexLocation = indexOffset;

		// �ش� �̸��� �Ž��� ����Ǿ� ������ �˸��� ���� �̸��� ����
		// DrawArgs���� �ٽ� �ش� Submesh�� �ε��� �� �ֵ��� �̸��� ����
		r->mGeometry.meshNames.push_back(submeshName);

		// Texture, Material �ڵ� ����
		{
			// ���� �ؽ��İ� �����Ѵٸ�
			if (r->mGeometry.DrawArgs[submeshName].textureName != "")
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

	// ���� ������Ʈ�� ��ü ũ�⸦ ��Ÿ���� ���� ��� ����޽� ũ�⸦ ���Ѵ�.
	r->mGeometry.VertexBufferByteSize = model->vertex_count * sizeof(Vertex);
	r->mGeometry.IndexBufferByteSize = model->index_count * sizeof(uint32_t);
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
	r->mRenderType = RenderItem::RenderType::_OPAQUE_SKINNED_RENDER_TYPE;

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

void BoxApp::uploadTexture(_In_ Texture& tex, _In_ bool isSky) {
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
					tex.UploadHeap
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

void BoxApp::uploadMaterial(_In_ std::string matName, _In_ std::string texName, _In_ bool isSkyTexture)
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
	mat.NormalSrvHeapIndex = diffuseIDX;

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

	if (!isCubeMap)
	{
		if (r->Mat.size() <= idx)
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

		r->Mat[idx]->DiffuseSrvHeapIndex = diffuseIDX;
		r->Mat[idx]->NormalSrvHeapIndex = diffuseIDX;
	}
	else
	{
		if (r->SkyMat.size() <= idx)
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

		r->SkyMat[idx]->DiffuseSrvHeapIndex = diffuseIDX;
		r->SkyMat[idx]->NormalSrvHeapIndex = diffuseIDX;
	}
}

void BoxApp::BindMaterial(RenderItem* r, std::string name, bool isCubeMap) {
	assert(r && "The RenderItem is NULL");

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
		r->Mat.push_back(m);
	}
	else
	{
		r->SkyMat.push_back(m);
	}
}

void BoxApp::BindMaterial(RenderItem* r, std::string matName, std::string texName, bool isCubeMap) {
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

	r->Mat.push_back(m);
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

		memcpy(mPhyxResources[i].Position, vec.m128_f32, sizeof(float) * 3);

		mPhys.setPosition(mPhyxRigidBody[i], pos.x, pos.y, pos.z);
	}
}
void RenderItem::setRotation(_In_ XMFLOAT3 rot) {
	XMVECTOR vec = DirectX::XMLoadFloat3(&rot);

	for (UINT i = 0; i < InstanceCount; i++) {
		//mPhyxResources[i].Rotation[0] = rot.x;
		//mPhyxResources[i].Rotation[1] = rot.y;
		//mPhyxResources[i].Rotation[2] = rot.z;

		memcpy(mPhyxResources[i].Rotation, vec.m128_f32, sizeof(float) * 3);

		mPhys.setRotation(mPhyxRigidBody[i], rot.x, rot.y, rot.z);
	}
}

void RenderItem::setVelocity(_In_ XMFLOAT3 vel) {
	for (UINT i = 0; i < InstanceCount; i++)
		mPhys.setVelocity(mPhyxRigidBody[i], vel.x, vel.y, vel.z);
}
void RenderItem::setTorque(_In_ XMFLOAT3 torq) {
	for (UINT i = 0; i < InstanceCount; i++)
		mPhys.setTorque(mPhyxRigidBody[i], torq.x, torq.y, torq.z);
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