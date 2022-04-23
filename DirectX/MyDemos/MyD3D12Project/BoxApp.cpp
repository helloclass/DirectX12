#include "BoxApp.h"


bool StopThread = false;

// Thread Resource
DWORD hAnimThreadID;
HANDLE hAnimThread;

DWORD hClothPhysxThreadID;
HANDLE hClothPhysxThread;

BoxApp::BoxApp(HINSTANCE hInstance)
: D3DApp(hInstance) 
{
	mRenderTypeCount[RenderItem::RenderType::_OPAQUE_RENDER_TYPE]			= 0;
	mRenderTypeCount[RenderItem::RenderType::_OPAQUE_SKINNED_RENDER_TYPE]	= 0;
	mRenderTypeCount[RenderItem::RenderType::_PMX_FORMAT_RENDER_TYPE]		= 0;
	mRenderTypeCount[RenderItem::RenderType::_ALPHA_RENDER_TYPE]			= 0;
}

std::unordered_map<RenderItem::RenderType, ComPtr<ID3D12PipelineState>> BoxApp::mPSOs;
std::unique_ptr<RenderTarget> BoxApp::mOffscreenRT = nullptr;

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

HANDLE BoxApp::renderTargetEvent[3];
HANDLE BoxApp::recordingDoneEvents[3];
HANDLE BoxApp::drawThreads[3];
LPDWORD BoxApp::ThreadIndex[3] = { 0 };

static std::unique_ptr<UploadBuffer<PassConstants>>				PassCB;
static std::unique_ptr<UploadBuffer<RateOfAnimTimeConstants>>	RateOfAnimTimeCB;
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

// Physx Thread Section
// Cloth and Morph
DWORD WINAPI ThreadClothPhysxFunc(LPVOID prc)
{
	Physics mPhys;

	std::vector<PxClothParticle> vertices;
	std::vector<PxU32> primitives;

	UINT vOffset;
	UINT iOffset;

	UINT vSize;
	UINT iSize;

	// Update Vertex
	float* mClothBuffer = NULL;

	// 각 SubMesh에 Cloth Physx Obj 추가
	for (std::list<RenderItem*>::iterator& obj = mGameObjects.begin(); obj != mGameObjects.end(); obj++)
	{
		// 현 게임 오브젝트 포인터를 얻음.
		RenderItem* _RenderItem = *obj;
		ObjectData* _RenderData = mGameObjectDatas[_RenderItem->mName];

		_RenderData->mClothes.resize(_RenderData->SubmeshCount);
		for (UINT i = 0; i < _RenderData->SubmeshCount; i++)
			_RenderData->mClothes[i] = NULL;

		// Rigid Body Mesh를 추가
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

				// 현재 Material의 Vertex의 정보를 얻어온다.
				vertices.resize(vSize);
				primitives.resize(iSize);

				int mOffset = 0;
				int mIDXMin;

				DirectX::XMVECTOR posVector, resVector;
				DirectX::XMMATRIX originMatrix;
				for (std::set<int>::iterator& vIDX = _RenderData->vertBySubmesh[submesh].begin(); vIDX != _RenderData->vertBySubmesh[submesh].end(); vIDX++)
				{
					// Physx에 리지드바디를 업로드
					vertices[mOffset].pos[0] = _RenderData->vertices[*vIDX].Pos.x;
					vertices[mOffset].pos[1] = _RenderData->vertices[*vIDX].Pos.y;
					vertices[mOffset].pos[2] = _RenderData->vertices[*vIDX].Pos.z;
					vertices[mOffset].invWeight = 1.0f;

					// 실 게임에선 리지드바디를 보이면 안되므로 상쇄
					_RenderData->vertices[*vIDX].Pos.x = 0.0f;
					_RenderData->vertices[*vIDX].Pos.y = 0.0f;
					_RenderData->vertices[*vIDX].Pos.z = 0.0f;

					mOffset++;
				}

				mOffset = 0;
				mIDXMin = _RenderData->mModel.indices[iOffset];
				// 최소 인덱스를 찾는다.
				for (UINT i = iOffset; i < iOffset + iSize; i++)
				{
					if (mIDXMin > _RenderData->mModel.indices[i])
						mIDXMin = _RenderData->mModel.indices[i];
				}

				// 현재 Material의 Index 정보를 얻어온다.
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

		// Cloth Physx Mesh를 추가
		for (UINT submesh = 0; submesh < _RenderItem->SubmeshCount; submesh++)
		{
			// Cloth Physix를 사용하지 않는 서브메쉬면 스킵
			if (!_RenderData->isCloth[submesh])
			{
				continue;
			}

			if (_RenderData->mFormat == "FBX")
			{
				vOffset = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].BaseVertexLocation;
				iOffset = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].StartIndexLocation;

				vSize = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].VertexSize;
				iSize = _RenderItem->mGeometry.DrawArgs[_RenderItem->mGeometry.meshNames[submesh]].IndexSize;

				vertices.resize(vSize);
				primitives.resize(iSize);

				// 현재 Material의 Vertex의 정보를 얻어온다.
				for (UINT v = 0; v < vSize; v++) {
					vertices[v].pos[0] = _RenderData->vertices[vOffset + v].Pos.x;
					vertices[v].pos[1] = _RenderData->vertices[vOffset + v].Pos.y;
					vertices[v].pos[2] = _RenderData->vertices[vOffset + v].Pos.z;
					vertices[v].invWeight = _RenderData->mClothWeights[vOffset + v];
				}
				// 현재 Material의 Index 정보를 얻어온다.
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

				// 현재 Material의 Vertex의 정보를 얻어온다.
				vertices.clear();
				primitives.clear();
				vertices.resize(0);
				primitives.resize(0);

				bool hasZero(false), hasOne(false);

				/*if (!(hasZero && hasOne))
					throw std::runtime_error("cloth invWeight Exception. (InvWeight must has 0 and 1 least 1.)");*/

				int primCount = 0;
				for (UINT idx = iOffset; idx < iOffset + iSize; idx+=3)
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

				DirectX::XMVECTOR posVector, resVector;
				DirectX::XMMATRIX originMatrix;

				std::unordered_map<UINT, UINT> testest;

				// 서브메쉬에서 사용되는 버텍스 인덱스리스트를 담고있는 vertBySubmesh에서 weight가 0.0인 경우에 
				// vertBySubmesh에서 제외 할 것임.
				int mOffset = 0;
				int mRemover = -1;
				for (std::set<int>::iterator& vIDX = _RenderData->vertBySubmesh[submesh].begin(); vIDX != _RenderData->vertBySubmesh[submesh].end(); vIDX++)
				{
					if (mRemover != -1)
					{
						_RenderData->vertBySubmesh[submesh].erase(mRemover);
						mRemover = -1;
					}

					posVector = DirectX::XMLoadFloat3(&_RenderData->vertices[*vIDX].Pos);
					originMatrix = DirectX::XMLoadFloat4x4(&_RenderData->mOriginRevMatrix[2]);

					resVector = DirectX::XMVector3Transform(posVector, originMatrix);

					if (_RenderData->mClothWeights[*vIDX] == 0.0f) {
						hasZero = true;
						mRemover = *vIDX;

						continue;
					}
					else {
						hasOne = true;
					}

					physx::PxClothParticle part;
					part.pos[0] = resVector.m128_f32[0];
					part.pos[1] = resVector.m128_f32[1];
					part.pos[2] = resVector.m128_f32[2];
					part.invWeight = _RenderData->mClothWeights[*vIDX];

					vertices.push_back(part);

					testest[*vIDX] = mOffset;

					mOffset++;
				}

				for (int iIDX = 0; iIDX < primitives.size(); iIDX++)
				{
					primitives[iIDX] = testest[primitives[iIDX]];
				}

				//if (!hasZero)
					vertices[0].invWeight = 0.0f;
				if (!hasOne)
					vertices[0].invWeight = 1.0f;

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

		} // Loop Submesh
	} // Loop GameObjects

	// ClothParticleInfo가 업로드 되어질 공간
	PxClothParticleData* ppd = NULL;
	// Cloth Position 빠른 접근을 위한 공간
	Vertex* mClothOffset = NULL;
	UINT vertexSize;

	// 게임 초기에 Cloth Vertices Update가 먼저 시행 되어야 하기 때문에
	// ClothWriteEvent를 On
	ResetEvent(mClothReadEvent);
	SetEvent(mClothWriteEvent);

	int loop = 0;
	bool loopUpdate = false;

	while (!StopThread)
	{
		// Cloth Vertices가 Write Layer가 될 때 까지 대기
		WaitForSingleObject(mClothWriteEvent, INFINITE);

		if (StopThread) break;

		// Update Cloth Vertices
		for (std::unordered_map<std::string, ObjectData*>::iterator obj = mGameObjectDatas.begin(); obj != mGameObjectDatas.end(); obj++)
		{
			if (!loopUpdate)	break;

			ObjectData* _RenderData = obj->second;

			// new float[4 * vSize] 대신 submesh VBV에 바로 업로드 할 수 있을까?
			Vertex* clothPos = _RenderData->vertices.data();

			// 만일 애니메이션이 없는 오브젝트라면 스킵
			if (_RenderData->mAnimVertex.size() < 1 &&
				_RenderData->mBoneMatrix.size() < 1)
				return (DWORD)(-1);

			DirectX::XMFLOAT4X4 mRootF44;
			DirectX::XMMATRIX mRootMat;
			DirectX::XMVECTOR s, p, q;

			// 서브메쉬 단위로 애니메이션을 업데이트 하는 경우 (cloth)
			for (UINT submeshIDX = 0; submeshIDX < _RenderData->SubmeshCount; submeshIDX++)
			{
				if (_RenderData->isRigidBody[submeshIDX]) 
				{
					// Adapted Cloth Physx
					mRootF44 = _RenderData->mBoneMatrix[_RenderData->currentFrame][2];
					mRootMat = DirectX::XMLoadFloat4x4(&mRootF44);

					DirectX::XMMatrixDecompose(&s, &q, &p, mRootMat);

					physx::PxTransform mPose(
						physx::PxVec3(p.m128_f32[0], p.m128_f32[1], p.m128_f32[2]),
						physx::PxQuat(q.m128_f32[0], q.m128_f32[1], q.m128_f32[2], q.m128_f32[3])
					);

					_RenderData->mRigidbody[submeshIDX]->setKinematicTarget(mPose);
				}

				if (_RenderData->isCloth[submeshIDX]) 
				{
					// Adapted Cloth Physx
								// Adapted Cloth Physx
					mRootF44 = _RenderData->mBoneMatrix[_RenderData->currentFrame][2];
					mRootMat = DirectX::XMLoadFloat4x4(&mRootF44);

					DirectX::XMMatrixDecompose(&s, &q, &p, mRootMat);

					physx::PxTransform mPose(
						physx::PxVec3(p.m128_f32[0], p.m128_f32[1], p.m128_f32[2]),
						physx::PxQuat(q.m128_f32[0], q.m128_f32[1], q.m128_f32[2], q.m128_f32[3])
					);

					_RenderData->mClothes[submeshIDX]->setTargetPose(mPose);

					mClothOffset =
						clothPos +
						_RenderData->mDesc[submeshIDX].BaseVertexLocation;
					vertexSize =
						_RenderData->mDesc[submeshIDX].VertexSize;

					PxU32 nbParticles = _RenderData->mClothes[submeshIDX]->getNbParticles();

					ppd = _RenderData->mClothes[submeshIDX]->lockParticleData(PxDataAccessFlag::eREADABLE);

					std::set<int>::iterator& vIDX = _RenderData->vertBySubmesh[submeshIDX].begin();
					std::set<int>::iterator& vEnd = _RenderData->vertBySubmesh[submeshIDX].end();
					PxClothParticle* ppdPart = &ppd->particles[0];

					for (; vIDX != vEnd; vIDX++)
					{
						mClothOffset[*vIDX].Pos.x = ppdPart->pos[0];
						mClothOffset[*vIDX].Pos.y = ppdPart->pos[1];
						mClothOffset[*vIDX].Pos.z = ppdPart->pos[2];

						ppdPart++;
					}

					ppd->unlock();
				} // if (_RenderData->isCloth[submeshIDX])
			} // for Submesh
		} // for GameObject

		// Cloth Vertices를 Read Layer로 변경
		ResetEvent(mClothWriteEvent);
		SetEvent(mClothReadEvent);

		// 현 판을 갈아 엎고 다시 대기 
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
	std::unordered_map<std::string, ObjectData*>::iterator obj = mGameObjectDatas.begin();
	std::unordered_map<std::string, ObjectData*>::iterator objEnd = mGameObjectDatas.end();
	for (; obj != objEnd; obj++)
	{
		ObjectData* _RenderData = obj->second;

		for (int submeshIDX = 0; submeshIDX < _RenderData->SubmeshCount; submeshIDX++)
		{
			if (_RenderData->isCloth[submeshIDX])
			{
				_RenderData->mClothes[submeshIDX]->release();
			}
		}
	}

	return (DWORD)(0);
}

// Physx Thread Section
DWORD WINAPI ThreadAnimFunc(LPVOID prc)
{
	// 게임 초기에 Cloth Vertices Update가 먼저 시행 되어야 하기 때문에
	// ClothWriteEvent를 On
	ResetEvent(mAnimationReadEvent);
	SetEvent(mAnimationWriteEvent);

	while (!StopThread) 
	{
		// Cloth Vertices가 Write Layer가 될 때 까지 대기
		WaitForSingleObject(mAnimationWriteEvent, INFINITE);
		if (StopThread) break;

		// Update Cloth Vertices
		std::unordered_map<std::string, ObjectData*>::iterator obj = mGameObjectDatas.begin();
		std::unordered_map<std::string, ObjectData*>::iterator objEnd = mGameObjectDatas.end();
		for (; obj != objEnd; obj++)
		{
			ObjectData* _RenderData = obj->second;

			// 만일 애니메이션이 없는 오브젝트라면 스킵
			if (_RenderData->mAnimVertex.size() < 1 &&
				_RenderData->mBoneMatrix.size() < 1)
				continue;

			// 애니메이션 업데이트
			_RenderData->isAnim = true;
			_RenderData->isLoop = true;

			// 현재 프레임을 계산하며, 프레임이 갱신 될 시 트리거를 작동한다.
			int currentFrame =
				(int)(_RenderData->currentDelayPerSec / _RenderData->durationOfFrame[0]);

			// 만일 현재 프레임이 애니메이션의 끝이라면?
			if ((_RenderData->endAnimIndex) <= currentFrame) {
				// 다시 시작 번지 프레임으로
				currentFrame = _RenderData->beginAnimIndex;
				_RenderData->currentDelayPerSec = (_RenderData->beginAnimIndex * _RenderData->durationOfFrame[0]);
			}
			// 만일 프레임이 갱신 되었다면?
			if (_RenderData->currentFrame != currentFrame) {
				_RenderData->currentFrame = currentFrame;
				_RenderData->updateCurrentFrame = true;
			}

			// 다음 프레임 까지의 잔분 시간을 업데이트 한다.
			float residueTime = _RenderData->currentDelayPerSec - (_RenderData->currentFrame * _RenderData->durationOfFrame[0]);

			// 잔분 시간 업데이트
			_RenderData->mAnimResidueTime = residueTime;

			////
			if (obj->second->mFormat == "FBX") {
				Vertex* VertexPos = obj->second->vertices.data();
				Vertex* mVertexOffset = NULL;
				UINT vertexSize = 0;

				for (int submeshIDX = 0; submeshIDX < (int)obj->second->SubmeshCount; submeshIDX++)
				{
					// 만일 애니메이션 Vertex에 고정되어 플레이 되는 Submesh라면
					if (!obj->second->isCloth[submeshIDX])
					{
						// 만일 현재 프레임의 애니메이션 정보가 NULL이면 0으로 공백을 채운다.
						if (obj->second->mAnimVertexSize[obj->second->currentFrame][submeshIDX] == 0)
							continue;

						// 현 게임 오브젝트의 VertexList 내 해당 Submesh가 적힌 오프셋을 불러옴
						mVertexOffset =
							VertexPos +
							obj->second->mDesc[submeshIDX].BaseVertexLocation;
						// Submesh에 해당하는 Vertex 개수
						vertexSize =
							obj->second->mDesc[submeshIDX].VertexSize;

						// 애니메이션이 최신화 된 버텍스 시작 주소
						float* animPos = obj->second->mAnimVertex[obj->second->currentFrame][submeshIDX];
						// 애니메이션 버텍스 사이즈
						int animVertSize = obj->second->mAnimVertexSize[obj->second->currentFrame][submeshIDX];

						int vcount = 0;
						for (UINT vertexIDX = 0; vertexIDX < vertexSize; vertexIDX++)
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
					DirectX::XMFLOAT4X4* BoneOriginOffset;
					DirectX::XMFLOAT4X4* BoneOffsetOfFrame;

					BoneOriginOffset = obj->second->mOriginRevMatrix.data();
					BoneOffsetOfFrame = obj->second->mBoneMatrix[obj->second->currentFrame].data();
					//BoneOffsetOfFrame = objData->mBoneMatrix[0].data();

					for (int i = 0; i < obj->second->mModel.bone_count; i++)
					{
						PmxAnimationData pmxAnimData;

						pmxAnimData.mOriginMatrix = BoneOriginOffset[i];
						pmxAnimData.mMatrix = BoneOffsetOfFrame[i];

						PmxAnimationBuffer->CopyData(i, pmxAnimData);
					}

					obj->second->updateCurrentFrame = false;
				}

				// 잔분 시간 업데이트
				RateOfAnimTimeConstants	mRateOfAnimTimeCB;
				mRateOfAnimTimeCB.rateOfAnimTime = obj->second->mAnimResidueTime;
				RateOfAnimTimeCB->CopyData(0, mRateOfAnimTimeCB);
			}
			////

			//////////////////////////////////////////////////////////////////////////////
			// Update Morph
			//////////////////////////////////////////////////////////////////////////////

			if (_RenderData->mMorphDirty.size() > 0) 
			{
				struct ObjectData::_VERTEX_MORPH_DESCRIPTOR mMorph;
				float weight = 0.0f;
				Vertex* DestPos = NULL;
				DirectX::XMFLOAT3* DesctinationPos = NULL;
				float* DefaultPos;

				for (int mIDX = 0; mIDX < _RenderData->mMorphDirty.size(); mIDX++) 
				{
					mMorph = _RenderData->mMorph[mIDX];
					weight = mMorph.mVertWeight;

					for (int i = 0; i < mMorph.mVertIndices.size(); i++)
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
		}

		// Cloth Vertices를 Read Layer로 변경
		ResetEvent(mAnimationWriteEvent);
		SetEvent(mAnimationReadEvent);
	}

	return (DWORD)(0);
}

// Client
bool BoxApp::Initialize()
{
	d3dUtil::loadImage(std::string(""), std::string(""), std::string(""));

    if(!D3DApp::Initialize())
		return false;
		
    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
 
	// _Awake (오브젝트 로드, 텍스쳐, 머테리얼 생성)
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

	// 여기 전까지 텍스쳐, 머테리얼 업로드 되어 있어야함.

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
	// Cloth가 업데이트 될 때 까지 대기
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

	//// 이후 캐릭터를 따라다니는 카메라가 나오면 사용

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

	// (게임 오브젝트 개수 * 게임 서브메쉬) 개수 만큼의 마테리얼 정보를 일렬로 나열 시키기 위한 인덱스 
	int MatIDX = 0;

	// UpdateMaterialBuffer
	// Material Count == Submesh Count
	// 인스턴스들을 정의하기 전에 인스턴스에 사용 될 Material을 정의한다.
	std::vector<std::pair<std::string, Material>>::iterator& matIDX = mMaterials.begin();
	std::vector<std::pair<std::string, Material>>::iterator& matEnd = mMaterials.end();
	for (; matIDX != matEnd; matIDX++)
	{
		XMMATRIX matTransform = XMLoadFloat4x4(&matIDX->second.MatTransform);

		// Initialize new MaterialDatas
		MaterialData matData;
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

	mRenderInstTasks.resize(mGameObjectDatas.size());

	RenderItem* obj = NULL;
	std::list<RenderItem*>::iterator objIDX = mGameObjects.begin();
	std::list<RenderItem*>::iterator objEnd = mGameObjects.end();
	for (; objIDX != objEnd; objIDX++) 
	{
		obj = *objIDX;

		// 프러스텀 내에 존재하는 오브젝트, 오브젝트 인스턴스 개수 초기화
		mRenderInstTasks[gIdx].resize(obj->InstanceCount);

		for (i = 0; i < obj->InstanceCount; ++i)
		{
			// getSyncDesc
			// 제발 지우지 좀 마라 
			//pr = phys.getTranspose(g->physxIdx[i]);

			world = XMLoadFloat4x4(&obj->_Instance[i].World);

			texTransform = XMLoadFloat4x4(&obj->_Instance[i].TexTransform);

			invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);
			viewToLocal = XMMatrixMultiply(invView, invWorld);
			mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

			// 만일 캐릭터가 카메라 영역에서 보이지 않는다면,
			// 캐릭터가 보여질 지를 결정하는 Bound의 위치값만 업데이트를 하고
			// 실 캐릭터는 움직이지 않는다.
			//g->Bounds.Center.x = pr->Position[0];
			//g->Bounds.Center.y = pr->Position[1];
			//g->Bounds.Center.z = pr->Position[2];

			XMFLOAT4X4 boundScale = MathHelper::Identity4x4();
			boundScale._11 = 1.5f;
			boundScale._22 = 1.5f;
			boundScale._33 = 1.5f;

			obj->Bounds.Transform(obj->Bounds, XMLoadFloat4x4(&boundScale));

			// 만일 프러스텀 박스내에 존재한다면
			if ((localSpaceFrustum.Contains(obj->Bounds) != DirectX::DISJOINT))
			{
				// 프러스텀 내에 존재하는 오브젝트만 보여줄 것이다
				XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
				data.MaterialIndex = obj->_Instance[i].MaterialIndex;

				// 제발 지우지 좀 마라 
				// 만일 캐릭터가 카메라 영역에 보인다면 ,
				// 캐릭터의 SRT를 업데이트 한다.
				//data.World._14 = pr->Position[0];
				//data.World._24 = pr->Position[1];
				//data.World._34 = pr->Position[2];

				//data.World._11 = pr->Scale[0];
				//data.World._22 = pr->Scale[1];
				//data.World._33 = pr->Scale[2];

				// renderInstCounts = {5, 4, 2, 6, 7, 11 .....} 일 경우의 인스턴스 Index는 
				// renderInstIndex = {{0, 1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10}, {11, 12, ..., 16}, ...}
				// 으로 순차적으로 늘어난다.
				InstanceBuffer->CopyData(visibleInstanceCount, data);

				// 해당 오브젝트의 렌더링 될 인스턴스 개수 증가 (상주성 때문에)
				// 이후 Draw에서는 "프러스텀 내에 존재하는" InstanceCounts 배열을 가지고 다음과 같은 작업을 수행한다.
				// renderInstCounts = {5, 4, 1, 2, 3, 6, 7, 9, 11 .....} 일 경우
				// Thread 0 = {5, 2, 7, ...} Thread 1 = {4, 3, 9, ...} Thread 2 = {1, 6, 11, ...}
				// 과 같이 분할하여, "프러스텀 내에 존재하는 오브젝트"만을 렌더링.
				mRenderInstTasks[gIdx][i] = visibleInstanceCount++;
			}
		}

		// 다음 오브젝트의 인스턴스 개수를 검사하기 위한 인덱스 스위칭
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
		mMainPassCB.AmbientLight = { 1.00f, 0.85f, 0.85f, 1.0f };
		mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
		mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
		mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
		mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
		mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
		mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

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

		// 현재 애니메이션 실행 시간을 업데이트 한다.
		objData->currentDelayPerSec += delta;
	}
}

void BoxApp::InitSwapChain(int numThread) 
{
	{
		RenderItem* beginItem = *(mGameObjects.begin());

		// commandAlloc, commandList를 재사용 하기 위한 리프레쉬
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

			d3dUtil::UpdateDefaultBuffer(
				mCommandList.Get(),
				mGameObjectDatas[obj->mName]->vertices.data(),
				obj->mGeometry.VertexBufferByteSize,
				obj->mGeometry.VertexBufferUploader,
				obj->mGeometry.VertexBufferGPU
			);
		}

		// Cloth를 Write모드로 변경
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
	D3D12_GPU_VIRTUAL_ADDRESS objectCB =			InstanceBuffer->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS matCB =				MaterialBuffer->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS rateOfAnimTimeCB =	RateOfAnimTimeCB->Resource()->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS pmxBoneCB =			PmxAnimationBuffer->Resource()->GetGPUVirtualAddress();

	// Stack Pointer
	D3D12_GPU_VIRTUAL_ADDRESS objCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS matCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS rateOfAnimTimeCBAddress;
	D3D12_GPU_VIRTUAL_ADDRESS pmxBoneCBAddress;

	// Instance Offset
	uint32_t _gInstOffset = 0;

	// loop Resource
	UINT j;

	// RenderInstTasks Offset
	UINT _taskOffset;
	// RenderInstTasks End Point
	UINT _taskEnd;

	// Extracted Render Items.
	RenderItem* obj = nullptr;
	int amountOfTask = 0;
	int continueIDX = -1;
	int taskIDX = 0;

	// GameObjectIndices
	std::vector<UINT> mGameObjectIndices;

	// 게임이 종료 될 때 까지 멀티 스레드 렌더링 대기
	while (ThreadIDX < numThread)
	{
		// CommandList가 프레젠테이션을 마치고, 렌더 타겟 이벤트를 호출 할 때 까지 대기.
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

		for (int loop = 0; loop < _taskEnd - _taskOffset; loop++)
		{
			mGameObjectIndices[loop] = -1;
			for (int i = 0; i < mRenderInstTasks.size(); i++)
			{
				int end = mRenderInstTasks[i].size() - 1;

				if ((_taskOffset + loop) <= mRenderInstTasks[i][end])
				{
					mGameObjectIndices[loop] = i;
					break;
				}
			}

			if (mGameObjectIndices[loop] == -1)
				throw std::runtime_error("잘못된 인덱싱..");
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

			// 디스크립터 바인딩
			ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
			mMultiCommandList[ThreadIDX]->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			// 시그니쳐 (디스크립터 셋) 바인딩
			mMultiCommandList[ThreadIDX]->SetGraphicsRootSignature(mRootSignature.Get());

			// Select Descriptor Buffer Index
			mMultiCommandList[ThreadIDX]->SetGraphicsRootConstantBufferView (
				4, 
				PassCB->Resource()->GetGPUVirtualAddress()
			);

			mMultiCommandList[ThreadIDX]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		// 스레드의 인덱스 단위로 Render Item을 분할하여 CommandList에 바인딩. 
		{
			continueIDX = -1;
			int MatIDX = 0;

			// 인스턴스 버퍼는 모든 게임 오브젝트에 대한 인스턴스 정보를 모두 가지고 있기에, 전역으로 할당.
			// 위에 Instance Update 할 때, Instance Buffer에다가 일렬로 Instance 정보를 바인딩 (CopyData) 하였기에
			// 한 번에 모든 게임 오브젝트의 인스턴스를 적재 해야 함. (물론 프러스텀에서 제외 된, 오브젝트 제외)

			// count of Objects
			// 인스턴스 카운트 X 게임 오브젝트 카운트만 적용
			for (taskIDX = 0; taskIDX < (_taskEnd - _taskOffset); taskIDX++)
			{
				// 렌더할 게임 오브젝트의 인덱스
				int gameIDX = mGameObjectIndices[taskIDX];
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
				objCBAddress = objectCB + _gInstOffset * sizeof(InstanceData);
				pmxBoneCBAddress = pmxBoneCB + 0 * sizeof(PmxAnimationData);
				rateOfAnimTimeCBAddress = rateOfAnimTimeCB + d3dUtil::CalcConstantBufferByteSize(gameIDX * sizeof(RateOfAnimTimeConstants));

				// Select Descriptor Buffer Index
				{
					mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
						1, 
						objCBAddress
					);
					mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
						3,
						pmxBoneCBAddress
					);

					if (obj->mFormat == "PMX")
					{
						mMultiCommandList[ThreadIDX]->SetGraphicsRootConstantBufferView(
							5,
							rateOfAnimTimeCBAddress
						);
					}
				}

				{
					// count of submesh
					for (j = 0; j < obj->SubmeshCount; j++)
					{
						// 각 서브메쉬당 각각의 텍스쳐, 마테리얼 정보를 가지고 있어야 합니다.
						if (obj->Mat[j] == NULL)
							throw std::runtime_error("Submesh 개수와 Material의 개수가 서로 다릅니다.");

						// Select Texture to Index
						CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

						// Move to Current Stack Pointer
						tex.Offset (
							obj->Mat[j]->DiffuseSrvHeapIndex,
							mCbvSrvUavDescriptorSize
						);

						matCBAddress = matCB + obj->Mat[j]->MatInstIndex * sizeof(MaterialData);

						// Select Descriptor Buffer Index
						{
							mMultiCommandList[ThreadIDX]->SetGraphicsRootDescriptorTable(
								0, 
								tex
							);
							mMultiCommandList[ThreadIDX]->SetGraphicsRootShaderResourceView(
								2, 
								matCBAddress
							);
						}

						// 여기서 Local 변경.
						// 현재 submesh Geometry를 선택
						sg = &obj->mGeometry.DrawArgs[
							obj->mGeometry.meshNames[
								j
							].c_str()
						];

						mMultiCommandList[ThreadIDX]->DrawIndexedInstanced(
							sg->IndexSize,							// 한 지오메트리의 인덱스 개수
							obj->InstanceCount,
							sg->StartIndexLocation,
							sg->BaseVertexLocation,
							0
						);

						// 서브매쉬 당 인스턴스는 존재하지 않는다.
						// 생성 단계에서 고정 SRT Mat을 바인딩 시킬 것.
					}
				}

				// 다음 Geometry면 offset 변경
				// _gInstOffset	+= mGameObjects[gameIDX]->InstanceCount;
				_gInstOffset += 1;
			}
			_gInstOffset	= 0;
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

	// 각 RenderItem들을 멀티 스레딩으로 그린다.
	{
		SetEvent(renderTargetEvent[0]);
		SetEvent(renderTargetEvent[1]);
		SetEvent(renderTargetEvent[2]);

		WaitForMultipleObjects(3, recordingDoneEvents, true, INFINITE);

		ResetEvent(recordingDoneEvents[0]);
		ResetEvent(recordingDoneEvents[1]);
		ResetEvent(recordingDoneEvents[2]);
	}
 
	// CommandAllocationList에 적재된 RenderItem들을 CommnadQ로 보낸다.
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

		// commandAlloc, commandList를 재사용 하기 위한 리프레쉬
		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs[RenderItem::RenderType::_POST_PROCESSING_PIPELINE].Get()));

		// 디스크립터 바인딩
		ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		//mCommandList->SetPipelineState(mPSOs[RenderItem::RenderType::_POST_PROCESSING_PIPELINE].Get());

		mCommandList->RSSetViewports(1, &mScreenViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		// Specify the buffers we are going to render to.
		mCommandList->OMSetRenderTargets(1, &mOffscreenRT->Rtv(), true, &DepthStencilView());

		// 시그니쳐 (디스크립터 셋) 바인딩
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		mCommandList->SetGraphicsRootConstantBufferView(4, PassCB->Resource()->GetGPUVirtualAddress());


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

		printf("");
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
    if((btnState & MK_LBUTTON) != 0)
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
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[6];

	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	// Instance SRV
	slotRootParameter[1].InitAsShaderResourceView(0, 1);
	// Material SRV
	slotRootParameter[2].InitAsShaderResourceView(1, 0);
	// PMX BONE CB
	slotRootParameter[3].InitAsShaderResourceView(1, 1);
	// PassCB
	slotRootParameter[4].InitAsConstantBufferView(0);
	// Animation Index and Time Space
	slotRootParameter[5].InitAsConstantBufferView(1);


	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		6, 
		slotRootParameter, 
		(UINT)staticSamplers.size(), 
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
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
	// 텍스쳐가 저장될 힙을 생성한다.
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
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	for (int texIDX = 0; texIDX < mTextures.size(); texIDX++)
	{
		srvDesc.Format = mTextures[mTextureList[texIDX]].Resource->GetDesc().Format;
		md3dDevice->CreateShaderResourceView(mTextures[mTextureList[texIDX]].Resource.Get(), &srvDesc, hDescriptor);

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
	RateOfAnimTimeCB = std::make_unique<UploadBuffer<RateOfAnimTimeConstants>>(md3dDevice.Get(), 1, true);
	InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(md3dDevice.Get(), InstanceNum, false);
	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(md3dDevice.Get(), mMaterials.size(), false);
	PmxAnimationBuffer = std::make_unique<UploadBuffer<PmxAnimationData>>(md3dDevice.Get(), BoneNum, false);
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
		if (go->mFormat == "FBX")
		{
			ObjectData* v = mGameObjectDatas[go->mName];
			// CPU Buffer를 할당하여 Vertices Data를 입력
			ThrowIfFailed(D3DCreateBlob(
				go->mGeometry.VertexBufferByteSize,
				&go->mGeometry.VertexBufferCPU
			));
			CopyMemory(
				go->mGeometry.VertexBufferCPU->GetBufferPointer(),
				mGameObjectDatas[go->mName]->vertices.data(),
				go->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer를 할당하여 Indices Data를 입력
			ThrowIfFailed(D3DCreateBlob(
				go->mGeometry.IndexBufferByteSize,
				&go->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				go->mGeometry.IndexBufferCPU->GetBufferPointer(),
				mGameObjectDatas[go->mName]->indices.data(),
				go->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer를 할당하여 Vertices Data를 입력
			go->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				mGameObjectDatas[go->mName]->vertices.data(),
				go->mGeometry.VertexBufferByteSize,
				go->mGeometry.VertexBufferUploader);

			// GPU Buffer를 할당하여 Indices Data를 입력
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

			// CPU Buffer를 할당하여 Vertices Data를 입력
			ThrowIfFailed(D3DCreateBlob(
				go->mGeometry.VertexBufferByteSize,
				&go->mGeometry.VertexBufferCPU
			));
			CopyMemory(
				go->mGeometry.VertexBufferCPU->GetBufferPointer(),
				v->vertices.data(),
				go->mGeometry.VertexBufferByteSize
			);

			// CPU Buffer를 할당하여 Indices Data를 입력
			ThrowIfFailed(D3DCreateBlob(
				go->mGeometry.IndexBufferByteSize,
				&go->mGeometry.IndexBufferCPU
			));
			CopyMemory(
				go->mGeometry.IndexBufferCPU->GetBufferPointer(),
				v->mModel.indices.get(),
				go->mGeometry.IndexBufferByteSize
			);

			// GPU Buffer를 할당하여 Vertices Data를 입력
			go->mGeometry.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
				md3dDevice.Get(),
				mCommandList.Get(),
				v->vertices.data(),
				go->mGeometry.VertexBufferByteSize,
				go->mGeometry.VertexBufferUploader);

			// GPU Buffer를 할당하여 Indices Data를 입력
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

RenderItem* BoxApp::CreateGameObject(std::string Name, int instance = 1)
{
	RenderItem* newGameObjects = new RenderItem();

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

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

		newGameObjects->_Instance.push_back(id);
	}

	mGameObjects.push_back(newGameObjects);
	mGameObjectDatas[Name] = new ObjectData();
	mGameObjectDatas[Name]->mName = Name;

	return mGameObjects.back();
}

RenderItem* BoxApp::CreateDynamicGameObject(std::string Name, int instance = 1)
{
	RenderItem* newGameObjects = new RenderItem();
	//PxRigidDynamic* dynamicObj = nullptr;

	// Allocated RenderItem ID
	newGameObjects->mName = Name;

	// Bind Phy
	/*for (int i = 0; i < instance; i++) {
		newGameObjects->physx.push_back(new PhyxResource());
		dynamicObj = phys.CreateDynamic(PxTransform(PxVec3(0, 0, 0)), PxSphereGeometry(3), PxVec3(0, 0, 0));

		newGameObjects->physxIdx.push_back(phys.BindObjColliber(dynamicObj, newGameObjects->physx[i]));
	}*/

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

		newGameObjects->_Instance.push_back(id);
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
	int subDividNum
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

		if (r->_Instance.size() == 0)
			throw std::runtime_error("Instance Size가 0입니다.");

		r->_Instance.at(0) = id;
	}


	// input subGeom
	GeometryGenerator::MeshData Box;
	Box = Geom.CreateBox(x, y, z, subDividNum);
	mGameObjectDatas[r->mName]->SubmeshCount += 1;
	mGameObjectDatas[r->mName]->mDesc.resize(mGameObjectDatas[r->mName]->SubmeshCount);

	// 하나의 오브젝트 스택 내, 해당 오브젝트의 구간(오프셋, 사이즈)을 저장
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation	= boxSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation	= boxSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize			= boxSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize			= boxSubmesh.VertexSize;

	// Submesh 저장
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

	// 게임 오브젝트의 전체 크기를 나타내기 위해 모든 서브메쉬 크기를 더한다.
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
	DirectX::XMFLOAT3 scale
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

		if (r->_Instance.size() == 0)
			throw std::runtime_error("Instance Size가 0입니다.");

		r->_Instance.at(0) = id;
	}

	// input subGeom
	GeometryGenerator::MeshData Sphere;
	Sphere = Geom.CreateSphere(rad, sliceCount, stackCount);
	mGameObjectDatas[r->mName]->SubmeshCount = 1;
	mGameObjectDatas[r->mName]->mDesc.resize(1);

	// 하나의 오브젝트 스택 내, 해당 오브젝트의 구간(오프셋, 사이즈)을 저장
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	sphereSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	sphereSubmesh.IndexSize = (UINT)Sphere.Indices32.size();
	sphereSubmesh.VertexSize = (UINT)Sphere.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation	= sphereSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation	= sphereSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize			= sphereSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize			= sphereSubmesh.VertexSize;

	// Submesh 저장
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

	// 게임 오브젝트의 전체 크기를 나타내기 위해 모든 서브메쉬 크기를 더한다.
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
	int subdivid
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

		if (r->_Instance.size() == 0)
			throw std::runtime_error("Instance Size가 0입니다.");

		r->_Instance.at(0) = id;
	}

	// input subGeom
	GeometryGenerator::MeshData Sphere;
	Sphere = Geom.CreateGeosphere(rad, subdivid);
	mGameObjectDatas[r->mName]->SubmeshCount = 1;
	mGameObjectDatas[r->mName]->mDesc.resize(1);

	// 하나의 오브젝트 스택 내, 해당 오브젝트의 구간(오프셋, 사이즈)을 저장
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	sphereSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	sphereSubmesh.IndexSize = (UINT)Sphere.Indices32.size();
	sphereSubmesh.VertexSize = (UINT)Sphere.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation = sphereSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation = sphereSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize = sphereSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize = sphereSubmesh.VertexSize;

	// Submesh 저장
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

	// 게임 오브젝트의 전체 크기를 나타내기 위해 모든 서브메쉬 크기를 더한다.
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
	DirectX::XMFLOAT3 scale
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

		if (r->_Instance.size() == 0)
			throw std::runtime_error("Instance Size가 0입니다.");

		r->_Instance.at(0) = id;
	}

	// input subGeom
	GeometryGenerator::MeshData Box;
	Box = Geom.CreateCylinder(bottomRad, topRad, height, sliceCount, stackCount);
	mGameObjectDatas[r->mName]->SubmeshCount = 1;
	mGameObjectDatas[r->mName]->mDesc.resize(1);

	// 하나의 오브젝트 스택 내, 해당 오브젝트의 구간(오프셋, 사이즈)을 저장
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation = boxSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation = boxSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize = boxSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize = boxSubmesh.VertexSize;

	// Submesh 저장
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

	// 게임 오브젝트의 전체 크기를 나타내기 위해 모든 서브메쉬 크기를 더한다.
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
	DirectX::XMFLOAT3 scale
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

		if (r->_Instance.size() == 0)
			throw std::runtime_error("Instance Size가 0입니다.");

		r->_Instance.at(0) = id;
	}

	// input subGeom
	GeometryGenerator::MeshData Box;
	Box = Geom.CreateGrid(w, h, wc, hc);
	mGameObjectDatas[r->mName]->SubmeshCount = 1;
	mGameObjectDatas[r->mName]->mDesc.resize(1);

	// 하나의 오브젝트 스택 내, 해당 오브젝트의 구간(오프셋, 사이즈)을 저장
	SubmeshGeometry boxSubmesh;
	boxSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
	boxSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

	boxSubmesh.IndexSize = (UINT)Box.Indices32.size();
	boxSubmesh.VertexSize = (UINT)Box.Vertices.size();

	mGameObjectDatas[r->mName]->mDesc[0].StartIndexLocation = boxSubmesh.StartIndexLocation;
	mGameObjectDatas[r->mName]->mDesc[0].BaseVertexLocation = boxSubmesh.BaseVertexLocation;

	mGameObjectDatas[r->mName]->mDesc[0].IndexSize = boxSubmesh.IndexSize;
	mGameObjectDatas[r->mName]->mDesc[0].VertexSize = boxSubmesh.VertexSize;

	// Submesh 저장
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

	// 게임 오브젝트의 전체 크기를 나타내기 위해 모든 서브메쉬 크기를 더한다.
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

		for (int i = 0; i < r->_Instance.size(); i++)
			r->_Instance.at(i) = id;
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

	mGameObjectDatas[r->mName]->SubmeshCount = meshData.size();
	mGameObjectDatas[r->mName]->mDesc.resize(meshData.size());

	size_t startV = mGameObjectDatas[r->mName]->vertices.size();

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	for (int subMeshCount = 0; subMeshCount < meshData.size(); ++subMeshCount)
	{
		SubmeshGeometry boxSubmesh;
		std::string submeshName = Name + std::to_string(subMeshCount);

		// 하나의 오브젝트 스택 내, 해당 오브젝트의 구간(오프셋, 사이즈)을 저장
		boxSubmesh.IndexSize = (UINT)meshData[subMeshCount].Indices32.size();
		boxSubmesh.VertexSize = (UINT)meshData[subMeshCount].Vertices.size();

		mGameObjectDatas[r->mName]->mDesc[subMeshCount].BaseVertexLocation = vertexOffset;
		mGameObjectDatas[r->mName]->mDesc[subMeshCount].StartIndexLocation = indexOffset;

		mGameObjectDatas[r->mName]->mDesc[subMeshCount].IndexSize = boxSubmesh.IndexSize;
		mGameObjectDatas[r->mName]->mDesc[subMeshCount].VertexSize = boxSubmesh.VertexSize;

		// 각 서브메쉬의 Cloth physix 인덱스를 초기화
		mGameObjectDatas[r->mName]->isCloth.push_back(false);
		mGameObjectDatas[r->mName]->isRigidBody.push_back(false);

		// Submesh 저장
		r->SubmeshCount += 1;

		r->mGeometry.subMeshCount += 1;
		r->mGeometry.DrawArgs[submeshName]				= boxSubmesh;
		r->mGeometry.DrawArgs[submeshName].name			= d3dUtil::getName(meshData[subMeshCount].texPath);
		r->mGeometry.DrawArgs[submeshName].textureName	= meshData[subMeshCount].texPath;

		r->mGeometry.DrawArgs[submeshName].BaseVertexLocation = vertexOffset;
		r->mGeometry.DrawArgs[submeshName].StartIndexLocation = indexOffset;

		// 해당 이름의 매쉬가 저장되어 있음을 알리기 위해 이름을 저장
		r->mGeometry.meshNames.push_back(submeshName);
		texturePath.push_back(r->mGeometry.DrawArgs[submeshName].textureName);

		// _Geom 공간을 공유하기에 버텍스 스택의 오프셋을 미리 정해둔다.
		startV = mGameObjectDatas[r->mName]->vertices.size();
		// 새로운 Submesh가 들어갈 공간을 마련한다.
		mGameObjectDatas[r->mName]->vertices.resize(startV + meshData[subMeshCount].Vertices.size());
		Vertex* v = mGameObjectDatas[r->mName]->vertices.data();
		// _Geometry를 하나의 공간에 물려 쓰기 때문에 지오메트리의 맨 뒤 부터 버텍스 값을 주기한다.
		// 이는 이후에 Deprecated 될 것.
		for (size_t i = 0; i < (meshData[subMeshCount].Vertices.size()); ++i)
		{
			v[i + startV].Pos		= meshData[subMeshCount].Vertices[i].Position;
			v[i + startV].Normal	= meshData[subMeshCount].Vertices[i].Normal;
			v[i + startV].Tangent	= meshData[subMeshCount].Vertices[i].TangentU;
			v[i + startV].TexC		= meshData[subMeshCount].Vertices[i].TexC;
		}

		mGameObjectDatas[r->mName]->indices.insert(
			mGameObjectDatas[r->mName]->indices.end(),
			std::begin(meshData[subMeshCount].Indices32),
			std::end(meshData[subMeshCount].Indices32)
		);

		// Texture, Material 자동 지정
		{
			// 만일 텍스쳐가 존재한다면
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

				// 새로운 머테리얼, 텍스쳐 추가
				this->BindMaterial(r, charTex.Name);
			}
			else
			{
				this->BindMaterial(r, "Default", "bricksTex");
			}

		}

		startV += boxSubmesh.VertexSize;

		// 게임 오브젝트의 전체 크기를 나타내기 위해 모든 서브메쉬 크기를 더한다.
		vertexOffset	+= boxSubmesh.VertexSize;
		indexOffset		+= boxSubmesh.IndexSize;

		r->mGeometry.VertexBufferByteSize	+= boxSubmesh.VertexSize * sizeof(Vertex);
		r->mGeometry.IndexBufferByteSize	+= boxSubmesh.IndexSize * sizeof(std::uint32_t);
	}
}

// Load Skinned Object
void BoxApp::CreateFBXSkinnedObject (
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

		for (int i = 0; i < r->_Instance.size(); i++)
			r->_Instance.at(i) = id;
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

	assert(!res && "애니메이션 스택이 존재하지 않는 오브젝트는 CreateFBXObject로");
 
	// 각 애니메이션의 하나의 프레임 당 시간(초 단위)과 애니메이션의 전체 듀레이션 (초 단위)를 얻어옵니다.
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

	mGameObjectDatas[r->mName]->SubmeshCount = meshData.size();
	mGameObjectDatas[r->mName]->mDesc.resize(meshData.size());

	// 각 Submesh의 Offset을 저장하는 용도
	size_t startV = mGameObjectDatas[r->mName]->vertices.size();

	UINT indexOffset = 0;
	UINT vertexOffset = 0;

	for (int subMeshCount = 0; subMeshCount < meshData.size(); ++subMeshCount)
	{
		SubmeshGeometry boxSubmesh;
		std::string submeshName = Name + std::to_string(subMeshCount);

		// 하나의 오브젝트 스택 내, 해당 오브젝트의 구간(오프셋, 사이즈)을 저장
		boxSubmesh.StartIndexLocation = (UINT)r->mGeometry.IndexBufferByteSize;
		boxSubmesh.BaseVertexLocation = (UINT)r->mGeometry.VertexBufferByteSize;

		boxSubmesh.IndexSize = (UINT)meshData[subMeshCount].Indices32.size();
		boxSubmesh.VertexSize = (UINT)meshData[subMeshCount].Vertices.size();

		mGameObjectDatas[r->mName]->mDesc[subMeshCount].BaseVertexLocation = vertexOffset;
		mGameObjectDatas[r->mName]->mDesc[subMeshCount].StartIndexLocation = indexOffset;

		mGameObjectDatas[r->mName]->mDesc[subMeshCount].IndexSize = boxSubmesh.IndexSize;
		mGameObjectDatas[r->mName]->mDesc[subMeshCount].VertexSize = boxSubmesh.VertexSize;

		// 각 서브메쉬의 Cloth physix 인덱스를 초기화
		mGameObjectDatas[r->mName]->isCloth.push_back(false);
		mGameObjectDatas[r->mName]->isRigidBody.push_back(false);

		// Submesh 저장
		r->SubmeshCount += 1;

		r->mGeometry.subMeshCount += 1;
		r->mGeometry.DrawArgs[submeshName] = boxSubmesh;
		r->mGeometry.DrawArgs[submeshName].name = d3dUtil::getName(meshData[subMeshCount].texPath);
		r->mGeometry.DrawArgs[submeshName].textureName = meshData[subMeshCount].texPath;

		r->mGeometry.DrawArgs[submeshName].BaseVertexLocation = vertexOffset;
		r->mGeometry.DrawArgs[submeshName].StartIndexLocation = indexOffset;

		////////////////

		// 해당 이름의 매쉬가 저장되어 있음을 알리기 위해 이름을 저장
		r->mGeometry.meshNames.push_back(submeshName);
		texturePath.push_back(r->mGeometry.DrawArgs[submeshName].textureName);

		// _Geom 공간을 공유하기에 버텍스 스택의 오프셋을 미리 정해둔다.
		startV = (UINT)mGameObjectDatas[r->mName]->vertices.size();
		// 새로운 Submesh가 들어갈 공간을 마련한다.
		mGameObjectDatas[r->mName]->vertices.resize(startV + meshData[subMeshCount].Vertices.size());
		Vertex* v = mGameObjectDatas[r->mName]->vertices.data();

		// _Geometry를 하나의 공간에 물려 쓰기 때문에 지오메트리의 맨 뒤 부터 버텍스 값을 주기한다.
		// 이는 이후에 Deprecated 될 것.
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

		// Texture, Material 자동 지정
		{
			// 만일 텍스쳐가 존재한다면
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

				// 새로운 머테리얼, 텍스쳐 추가
				this->BindMaterial(r, charTex.Name);
			}
			else
			{
				this->BindMaterial(r, "Default", "bricksTex");
			}

		}

		startV += meshData[subMeshCount].Vertices.size();
		// 게임 오브젝트의 전체 크기를 나타내기 위해 모든 서브메쉬 크기를 더한다.
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
	// meshData의 0번지는 Vertices 배열
	// meshData의 1번지 이후 부터는 각 Submesh에 대한 Indices
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

		for (int i = 0; i < r->_Instance.size(); i++)
			r->_Instance.at(i) = id;
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

	// GameObjects Data를 넣을 공간 생성
	mGameObjectDatas[r->mName]->SubmeshCount = _SubMeshCount;
	mGameObjectDatas[r->mName]->mDesc.resize(_SubMeshCount);

	//////////////////////////////////////////////////////////////
	// 외부 애니메이션 본을 읽어 Vertex를 업데이트
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

	mGameObjectDatas[r->mName]->endAnimIndex = mAnimFrameCount;

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

			mMorph.Name.push_back((wchar_t)morph->morph_name.c_str());
			mMorph.NickName.push_back((wchar_t)morph->morph_english_name.c_str());

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

	mGameObjectDatas[r->mName]->mMorph[0].mVertWeight = 1.0f;
	mGameObjectDatas[r->mName]->mMorphDirty.push_back(0);

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

	// 역행렬 벡터
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

	// 애니메이션 벡터
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
	// Model의 Cloth Weight와 Submesh 단위의 isCloth 정보를 저장
	//////////////////////////////////////////////////////////////

	//std::vector<std::wstring> mWeightBonesRoot;
	//mWeightBonesRoot.push_back(std::wstring(L"WaistString1_2"));
	//mWeightBonesRoot.push_back(std::wstring(L"WaistString2_2"));
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
	//	// 우선 버텍스의 인덱스를 저장
	//	outFile.write((char*)&i, sizeof(int));

	//	// 다음 웨이트를 설정한 뒤
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

	//	// 웨이트 저장
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
	bool mIsRigid;

	mGameObjectDatas[r->mName]->mClothWeights.resize(model->vertex_count);
	mGameObjectDatas[r->mName]->isCloth.resize(model->material_count);
	while (true)
	{
		inFile.read((char*)&vertIDX, sizeof(int));
		if (vertIDX == -1)	break;
		inFile.read((char*)&vertWeight, sizeof(float));

		mGameObjectDatas[r->mName]->mClothWeights[vertIDX] = vertWeight;
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
		int textureSize = model->textures[i].size() * 2;

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
		for (int j = mIDXAcculation; j < mCeil; j++)
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

		// 현 마테리얼의 텍스쳐 인덱스 로드
		diffuseTextureIDX = model->materials[subMeshIDX].diffuse_texture_index;
		sphereTextureIDX = model->materials[subMeshIDX].sphere_texture_index;
		toonTextureIDX = model->materials[subMeshIDX].toon_texture_index;

		// vertex, index의 오프셋 로드
		// Draw에서 모든 Vertices를 한번에 DescriptorSet의 VBV에 바인딩 할 것 (SubMesh 단위로 바인딩 X)
		boxSubmesh.BaseVertexLocation = (UINT)vertexOffset;
		boxSubmesh.StartIndexLocation = (UINT)indexOffset;

		// 현 submesh에 업로드 할 버텍스, 인덱스 개수
		boxSubmesh.VertexSize = (UINT)mGameObjectDatas[r->mName]->vertBySubmesh[subMeshIDX].size();
		boxSubmesh.IndexSize = (UINT)model->materials[subMeshIDX].index_count;

		std::string matName;
		matName.assign (
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

		// Submesh 저장
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

		// 해당 이름의 매쉬가 저장되어 있음을 알리기 위해 이름을 저장
		// DrawArgs에서 다시 해당 Submesh를 로드할 수 있도록 이름을 저장
		r->mGeometry.meshNames.push_back(submeshName);

		// Texture, Material 자동 지정
		{
			// 만일 텍스쳐가 존재한다면
			if (r->mGeometry.DrawArgs[submeshName].textureName != "")
			{
				std::string texName = d3dUtil::getName(texturePath[diffuseTextureIDX]);
				this->BindMaterial(r, texName);
			}
			else
			{
				this->BindMaterial(r, "Default", "bricksTex");
			}
		}

		_ModelIndexOffset += model->materials[subMeshIDX].index_count;

		vertexOffset += boxSubmesh.VertexSize;
		indexOffset += boxSubmesh.IndexSize;
	}

	// 게임 오브젝트의 전체 크기를 나타내기 위해 모든 서브메쉬 크기를 더한다.
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

	assert(!res && "애니메이션 스택이 존재하지 않는 오브젝트는 CreateFBXObject로");
}

void BoxApp::uploadTexture(_In_ Texture& tex) {
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

			// unordered_map에 Index를 부여하기 위해 이름 리스트 벡터 삽입.
			mTextureList.push_back(tex.Name);
			mTextures[tex.Name] = tex;
		}
	}
	catch (std::exception e) {
		MessageBoxA(nullptr, (LPCSTR)L"DDS 텍스쳐를 찾지 못하였습니다.", (LPCSTR)L"Error", MB_OK);
	}
}

void BoxApp::uploadMaterial(_In_ std::string name) {
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

		std::pair<std::string, Material> res(name.c_str(), mat);
		mMaterials.push_back(res);
	}
}

void BoxApp::uploadMaterial(_In_ std::string matName, _In_ std::string texName) 
{
	mMaterials;
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

	// DiffuseSrvHeapIndex를 다음과 같이 할당을 받게 되면, 변동성이 있는 mTexture의 특성으로 인하여 엄한 텍스쳐를 캐스팅 하게 됩니다.
	mat.DiffuseSrvHeapIndex = diffuseIDX;
	mat.NormalSrvHeapIndex = diffuseIDX;

	std::pair<std::string, Material> res(matName, mat);
	mMaterials.push_back(res);
}

void BoxApp::BindTexture(RenderItem* r, std::string name, int idx) {
	assert(r  && "The RenderItem is NULL!");
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

void BoxApp::BindMaterial(RenderItem* r, std::string name) {
	assert(r && "The RenderItem is NULL");

	Material* m = NULL;
	for (auto& i = mMaterials.begin(); i != mMaterials.end(); i++) {
		if (i->second.Name == name) {
			m = &i->second;
			break;
		}
	}

	assert(m && "Can't find Material which same with NAME!!");

	r->Mat.push_back(m);
}

void BoxApp::BindMaterial(RenderItem* r, std::string matName, std::string texName) {
	assert(r && "The RenderItem is NULL");

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

	m->DiffuseSrvHeapIndex = diffuseIDX;
	m->NormalSrvHeapIndex = diffuseIDX;

	r->Mat.push_back(m);
}

//////////////////////////////////
// RenderItem
//////////////////////////////////

void RenderItem::setPosition(_In_ XMFLOAT3 pos) {
	/*for (UINT i = 0; i < InstanceCount; i++) {
		physx[i]->Position[0] = pos.x;
		physx[i]->Position[1] = pos.y;
		physx[i]->Position[2] = pos.z;

		phys.setPosition(physxIdx[i], pos.x, pos.y, pos.z);
	}*/
}
void RenderItem::setRotation(_In_ XMFLOAT3 rot) {
	/*for (UINT i = 0; i < InstanceCount; i++) {
		physx[i]->Rotation[0] = rot.x;
		physx[i]->Rotation[1] = rot.y;
		physx[i]->Rotation[2] = rot.z;

		phys.setRotation(physxIdx[i], rot.x, rot.y, rot.z);
	}*/
}

void RenderItem::setVelocity(_In_ XMFLOAT3 vel) {
	/*for (UINT i = 0; i < InstanceCount; i++)
		phys.setVelocity(physxIdx[i], vel.x, vel.y, vel.z);*/
}
void RenderItem::setTorque(_In_ XMFLOAT3 torq) {
	/*for (UINT i = 0; i < InstanceCount; i++) 
		phys.setTorque(physxIdx[i], torq.x, torq.y, torq.z);*/
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

void RenderItem::setTestPosition(RenderItem* r, XMFLOAT3 pos, UINT idx) {
	r->_Instance[idx].World._41 = pos.x;
	r->_Instance[idx].World._42 = pos.y;
	r->_Instance[idx].World._43 = pos.z;
}

void RenderItem::setAnimIndex(_In_ int animIndex) {
	mGameObjectDatas[mName]->currentAnimIdx = animIndex;
}

int RenderItem::getAnimIndex() {
	return mGameObjectDatas[mName]->currentAnimIdx;
}

void RenderItem::setAnimBeginIndex(_In_ int animBeginIndex) {
	mGameObjectDatas[mName]->beginAnimIndex = animBeginIndex;
	mGameObjectDatas[mName]->currentFrame = animBeginIndex;
	mGameObjectDatas[mName]->currentDelayPerSec = (mGameObjectDatas[mName]->beginAnimIndex * mGameObjectDatas[mName]->durationOfFrame[0]);
}

int RenderItem::getAnimBeginIndex() {
	return mGameObjectDatas[mName]->beginAnimIndex;
}

void RenderItem::setAnimEndIndex(_In_ int animEndIndex) {
	mGameObjectDatas[mName]->endAnimIndex = animEndIndex;
}

int RenderItem::getAnimEndIndex() {
	return mGameObjectDatas[mName]->endAnimIndex;
}

void RenderItem::setAnimIsLoop(_In_ bool animLoop) {
	mGameObjectDatas[mName]->isLoop = animLoop;
}

int RenderItem::getAnimIsLoop() {
	return mGameObjectDatas[mName]->isLoop;
}