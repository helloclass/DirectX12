//***************************************************************************************
// GeometryGenerator.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "GeometryGenerator.h"
#include <algorithm>
#include <omp.h>

using namespace DirectX;

std::string mStaticName;
std::string mStaticNumber;

GeometryGenerator::MeshData GeometryGenerator::CreateBox(float width, float height, float depth, uint32 numSubdivisions)
{
	MeshData meshData;

	//
	// Create the vertices.
	//

	Vertex v[24];

	float w2 = 0.5f*width;
	float h2 = 0.5f*height;
	float d2 = 0.5f*depth;

	// Fill in the front face vertex data.
	v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the back face vertex data.
	v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the top face vertex data.
	v[8] = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[9] = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the bottom face vertex data.
	v[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the left face vertex data.
	v[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
	v[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
	v[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

	// Fill in the right face vertex data.
	v[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	v[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	v[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	meshData.Vertices.assign(&v[0], &v[24]);

	//
	// Create the indices.
	//

	uint32 i[36];

	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Fill in the back face index data
	i[6] = 4; i[7] = 5; i[8] = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// Fill in the top face index data
	i[12] = 8; i[13] = 9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// Fill in the bottom face index data
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// Fill in the left face index data
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// Fill in the right face index data
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	meshData.Indices32.assign(&i[0], &i[36]);

	// Put a cap on the number of subdivisions.
	numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

	for (uint32 i = 0; i < numSubdivisions; ++i)
		Subdivide(meshData);

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateSphere(float radius, uint32 sliceCount, uint32 stackCount)
{
	MeshData meshData;

	//
	// Compute the vertices stating at the top pole and moving down the stacks.
	//

	// Poles: note that there will be texture coordinate distortion as there is
	// not a unique point on the texture map to assign to the pole when mapping
	// a rectangular texture onto a sphere.
	Vertex topVertex(0.0f, +radius, 0.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	Vertex bottomVertex(0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	meshData.Vertices.push_back(topVertex);

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f*XM_PI / sliceCount;

	// Compute vertices for each stack ring (do not count the poles as rings).
	for (uint32 i = 1; i <= stackCount - 1; ++i)
	{
		float phi = i * phiStep;

		// Vertices of ring.
		for (uint32 j = 0; j <= sliceCount; ++j)
		{
			float theta = j * thetaStep;

			Vertex v;

			// spherical to cartesian
			v.Position.x = radius * sinf(phi)*cosf(theta);
			v.Position.y = radius * cosf(phi);
			v.Position.z = radius * sinf(phi)*sinf(theta);

			// Partial derivative of P with respect to theta
			v.TangentU.x = -radius * sinf(phi)*sinf(theta);
			v.TangentU.y = 0.0f;
			v.TangentU.z = +radius * sinf(phi)*cosf(theta);

			XMVECTOR T = XMLoadFloat3(&v.TangentU);
			DirectX::XMStoreFloat3(&v.TangentU, XMVector3Normalize(T));

			XMVECTOR p = XMLoadFloat3(&v.Position);
			DirectX::XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

			v.TexC.x = theta / XM_2PI;
			v.TexC.y = phi / XM_PI;

			meshData.Vertices.push_back(v);
		}
	}

	meshData.Vertices.push_back(bottomVertex);

	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//

	for (uint32 i = 1; i <= sliceCount; ++i)
	{
		meshData.Indices32.push_back(0);
		meshData.Indices32.push_back(i + 1);
		meshData.Indices32.push_back(i);
	}

	//
	// Compute indices for inner stacks (not connected to poles).
	//

	// Offset the indices to the index of the first vertex in the first ring.
	// This is just skipping the top pole vertex.
	uint32 baseIndex = 1;
	uint32 ringVertexCount = sliceCount + 1;
	for (uint32 i = 0; i < stackCount - 2; ++i)
	{
		for (uint32 j = 0; j < sliceCount; ++j)
		{
			meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j);
			meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
			meshData.Indices32.push_back(baseIndex + (i + 1)*ringVertexCount + j);

			meshData.Indices32.push_back(baseIndex + (i + 1)*ringVertexCount + j);
			meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
			meshData.Indices32.push_back(baseIndex + (i + 1)*ringVertexCount + j + 1);
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	uint32 southPoleIndex = (uint32)meshData.Vertices.size() - 1;

	// Offset the indices to the index of the first vertex in the last ring.
	baseIndex = southPoleIndex - ringVertexCount;

	for (uint32 i = 0; i < sliceCount; ++i)
	{
		meshData.Indices32.push_back(southPoleIndex);
		meshData.Indices32.push_back(baseIndex + i);
		meshData.Indices32.push_back(baseIndex + i + 1);
	}

	return meshData;
}

int GeometryGenerator::CreateFBXModel(
	std::vector<GeometryGenerator::MeshData>& meshData,
	std::string Path,
	bool uvMode
)
{
	// Fbx Kit
	FbxManager* manager = NULL;
	FbxScene* scene = NULL;
	FbxIOSettings* ios = NULL;
	FbxNode* node = NULL;

	manager = FbxManager::Create();
	scene = FbxScene::Create(manager, "scene");

	ios = FbxIOSettings::Create(manager, "");
	manager->SetIOSettings(ios);

	FbxImporter* importer = FbxImporter::Create(manager, "");

	bool loadRes = false;
	loadRes = importer->Initialize(Path.c_str(), -1, manager->GetIOSettings());

	if (!loadRes)
		throw std::runtime_error("FBX 파일을 찾지 못하였습니다.");

	importer->Import(scene);
	importer->Destroy();
	ios->Destroy();

	node = scene->GetRootNode();

	{
		DrawNodeRecursive(
			node,
			meshData,
			uvMode
		);

	}

	return 0;
}

int GeometryGenerator::CreateFBXSkinnedModel(
	std::vector<GeometryGenerator::MeshData>& meshData,
	std::string mName,
	std::string Path,
	FbxArray<FbxString*>& animNameLists,
	std::vector<FbxTime>& mStarts,
	std::vector<FbxTime>& mStops,
	std::vector<long long>& countOfFrame,
	std::vector<std::vector<float*>>& animVertexArrays,
	std::vector<FbxUInt>& mAnimVertexSizes,
	AnimationClip& mAnimClips
)
{
	// Fbx Kit
	FbxIOSettings* ios = NULL;
	FbxNode* node = NULL;

	// Animation Kit
	FbxArray<FbxPose*> pPoseArray;
	FbxTime time;

	FbxAnimStack* currAnimStack = NULL;
	FbxAnimLayer* currAnimLayer = NULL;

	FbxTakeInfo* currTakeInfo = NULL;

	int size = sizeof(fbxsdk::FbxVector4);

	mStaticName = mName;

	fbxsdk::FbxManager* manager = FbxManager::Create();
	fbxsdk::FbxScene* scene = FbxScene::Create(manager, "scene");

	ios = FbxIOSettings::Create(manager, "");
	manager->SetIOSettings(ios);

	FbxImporter* importer = FbxImporter::Create(manager, "");

	bool loadRes = false;
	loadRes = importer->Initialize(Path.c_str(), -1, manager->GetIOSettings());

	if (!loadRes)
		throw std::runtime_error("Path를 찾을 수 없습니다.");

	importer->Import(scene);

	importer->Destroy();
	ios->Destroy();

	node = scene->GetRootNode();

	// LoadFile
	scene->FillAnimStackNameArray(animNameLists);

	const int poseCount = scene->GetPoseCount();
	for (int i = 0; i < poseCount; i++)
		pPoseArray.Add(scene->GetPose(i));

	time.SetTime(0, 0, 0, 1, 0, scene->GetGlobalSettings().GetTimeMode());

	if (animNameLists.Size() > 0)
		currAnimStack = scene->FindMember<FbxAnimStack>(animNameLists[0]->Buffer());

	if (currAnimStack == NULL)
		return 1;

	currAnimLayer = currAnimStack->GetMember<FbxAnimLayer>();

	scene->SetCurrentAnimationStack(currAnimStack);

	// Get Anim Time
	FbxTime mStart, mStop;

	for (int i = 0; i < animNameLists.Size(); i++)
	{
		currTakeInfo = scene->GetTakeInfo(*(animNameLists[i]));
		if (currTakeInfo)
		{
			mStart = currTakeInfo->mLocalTimeSpan.GetStart();
			mStop = currTakeInfo->mLocalTimeSpan.GetStop();
		}
		else
		{
			continue;
		}

		mStarts.push_back(mStart);
		mStops.push_back(mStop);

		// countOfFrame.push_back(mStops[i].GetFrameCount() / 2);
		countOfFrame.push_back(mStops[i].GetFrameCount());
	}

	/*long long halfFrame = mStops[0].GetFrameCount() / 2;*/
	long long halfFrame = mStops[0].GetFrameCount();

	FbxTime pTime;
	std::vector<FbxAMatrix> mBoneHierarchy;

	DrawNodeRecursive(
		node,
		mStarts,
		mStops,
		pTime,
		mBoneHierarchy,
		NULL,
		meshData,
		animVertexArrays,
		mAnimVertexSizes,
		mAnimClips
	);

	return 0;
}

// PMX Loader
int GeometryGenerator::CreatePMXModel(
	std::vector<GeometryGenerator::MeshData>& meshData,
	std::string Path,
	std::vector<std::wstring>& texturePaths)
{
	// Read PMX
	pmx::PmxModel model;
	model.Init();

	std::ifstream stream(Path.c_str(), std::ifstream::binary);
	model.Read(&stream);
	// Load Texture Path
	for (int i = 0; i < model.texture_count; i++) {
		texturePaths.push_back(model.textures[i]);
	}

	// Vertex 정보가 담긴 Mesh를 저장하는 공간
	GeometryGenerator::MeshData mesh;
	pmx::PmxVertex* cv = NULL;

	mesh.Vertices.resize(model.vertex_count);
	for (int i = 0; i < model.vertex_count; i++) {
		cv = &model.vertices[i];

		mesh.Vertices[i] =
			GeometryGenerator::Vertex(
				cv->position[0],
				cv->position[1],
				cv->position[2],
				cv->normal[0],
				cv->normal[1],
				cv->normal[2],
				0.0f,
				0.0f,
				0.0f,
				cv->uv[0],
				cv->uv[1]
			);
	}
	meshData.push_back(mesh);

	int stack = 0;
	for (int i = 0; i < model.material_count; i++) {
		//model.materials[i].diffuse_texture_index;
		//model.materials[i].sphere_texture_index;
		//model.materials[i].toon_texture_index;

		mesh.Indices32.resize(model.materials[i].index_count);
		for (int j = 0; j < model.materials[i].index_count; j++) {
			mesh.Indices32[j] = model.indices[stack++];
		}

		meshData.push_back(mesh);
	}

	return 0;
}

// Extract Animation Bone
int GeometryGenerator::ExtractedAnimationBone(
	std::string Path,
	std::string targetPath,
	FbxArray<FbxString*>& animNameLists,
	std::vector<FbxTime>& mStarts,
	std::vector<FbxTime>& mStops,
	std::vector<long long>& countOfFrame
)
{
	// Init Bone List
	std::vector<std::string> mBoneStack;

	transSkeletonPairs[std::string("Center")].push_back(std::string("Center"));
	transSkeletonPairs[std::string("UpperBody")].push_back(std::string("LowerBody"));
	transSkeletonPairs[std::string("UpperBody")].push_back(std::string("groove"));
	transSkeletonPairs[std::string("UpperBody")].push_back(std::string("waist"));

	transSkeletonPairs[std::string("UpperBody")].push_back(std::string("UpperBody"));
	transSkeletonPairs[std::string("UpperBody2")].push_back(std::string("UpperBody2"));
	transSkeletonPairs[std::string("Breast.L")].push_back(std::string("Breast.L"));
	transSkeletonPairs[std::string("Breast.R")].push_back(std::string("Breast.R"));
	transSkeletonPairs[std::string("Neck")].push_back(std::string("Neck"));
	transSkeletonPairs[std::string("Head")].push_back(std::string("Head"));

	transSkeletonPairs[std::string("LegD.L")].push_back(std::string("LegD.L"));
	//transSkeletonPairs[std::string("LegD.L")].push_back(std::string("Leg.L"));
	transSkeletonPairs[std::string("KneeD.L")].push_back(std::string("KneeD.L"));
	//transSkeletonPairs[std::string("KneeD.L")].push_back(std::string("Knee.L"));
	//transSkeletonPairs[std::string("FootD.L")].push_back(std::string("FootD.L"));
	//transSkeletonPairs[std::string("FootD.L")].push_back(std::string("Foot.L"));
	//transSkeletonPairs[std::string("FootD.L")].push_back(std::string("FootIK.L"));
	//transSkeletonPairs[std::string("FootD.L")].push_back(std::string("leg IKP_L"));
	//transSkeletonPairs[std::string("FootD.L")].push_back(std::string("toe.L"));
	//transSkeletonPairs[std::string("FootD.L")].push_back(std::string("toeIK.L"));

	transSkeletonPairs[std::string("LegD.R")].push_back(std::string("LegD.R"));
	//transSkeletonPairs[std::string("LegD.R")].push_back(std::string("Leg.R"));
	transSkeletonPairs[std::string("KneeD.R")].push_back(std::string("KneeD.R"));
	//transSkeletonPairs[std::string("KneeD.R")].push_back(std::string("Knee.R"));
	//transSkeletonPairs[std::string("FootD.R")].push_back(std::string("FootD.R"));
	//transSkeletonPairs[std::string("FootD.R")].push_back(std::string("Foot.R"));
	//transSkeletonPairs[std::string("FootD.R")].push_back(std::string("FootIK.R"));
	//transSkeletonPairs[std::string("FootD.R")].push_back(std::string("leg IKP_R"));
	//transSkeletonPairs[std::string("FootD.R")].push_back(std::string("toe.R"));
	//transSkeletonPairs[std::string("FootD.R")].push_back(std::string("toeIK.R"));

	//transSkeletonPairs[std::string("Neck")].push_back(std::string("shoulderP.L"));
	transSkeletonPairs[std::string("Shoulder.L")].push_back(std::string("Shoulder.L"));
	transSkeletonPairs[std::string("Bip02 L UpperArm")].push_back(std::string("Arm.L"));
	transSkeletonPairs[std::string("Bip02 L UpperArm")].push_back(std::string("Bip02 L UpperArm"));
	//transSkeletonPairs[std::string("Bip02 LUpArmTwist")].push_back(std::string("arm twist_L"));
	transSkeletonPairs[std::string("Bip02 L Forearm")].push_back(std::string("Elbow.L"));
	//transSkeletonPairs[std::string("Bip02 L Forearm")].push_back(std::string("+Elbow.L"));
	//transSkeletonPairs[std::string("Bip02 L ForeTwist")].push_back(std::string("wrist twist_L"));
	transSkeletonPairs[std::string("Elbow.L")].push_back(std::string("Wrist.L"));
	//transSkeletonPairs[std::string("Elbow.L")].push_back(std::string("wrist twist_L"));

	//transSkeletonPairs[std::string("Neck")].push_back(std::string("shoulderP.R"));
	transSkeletonPairs[std::string("Shoulder.R")].push_back(std::string("Shoulder.R"));
	transSkeletonPairs[std::string("Bip02 R UpperArm")].push_back(std::string("Arm.R"));
	transSkeletonPairs[std::string("Bip02 R UpperArm")].push_back(std::string("Bip02 R UpperArm"));
	//transSkeletonPairs[std::string("Bip02 RUpArmTwist")].push_back(std::string("arm twist_R"));
	transSkeletonPairs[std::string("Elbow.R")].push_back(std::string("Elbow.R"));
	//transSkeletonPairs[std::string("Bip02 R Forearm")].push_back(std::string("+Elbow.R"));
	//transSkeletonPairs[std::string("Bip02 R ForeTwist")].push_back(std::string("wrist twist_R"));
	transSkeletonPairs[std::string("Elbow.R")].push_back(std::string("Wrist.R"));
	//transSkeletonPairs[std::string("Elbow.R")].push_back(std::string("wrist twist_R"));
	//transSkeletonPairs[std::string("Bip02 Rhand_Weapon")].push_back(std::string("Wrist.R"));


	// Fbx Kit
	FbxIOSettings* ios = NULL;
	FbxNode* node = NULL;

	// Animation Kit
	FbxArray<FbxPose*> pPoseArray;
	FbxTime time;

	FbxAnimStack* currAnimStack = NULL;

	FbxTakeInfo* currTakeInfo = NULL;

	int size = sizeof(fbxsdk::FbxVector4);


	fbxsdk::FbxManager* manager = FbxManager::Create();
	fbxsdk::FbxScene* scene = FbxScene::Create(manager, "scene");

	ios = FbxIOSettings::Create(manager, "");
	manager->SetIOSettings(ios);

	FbxImporter* importer = FbxImporter::Create(manager, "");

	bool loadRes = false;
	loadRes = importer->Initialize(Path.c_str(), -1, manager->GetIOSettings());

	if (!loadRes)
		throw std::runtime_error("Path를 찾지 못하였습니다.");

	importer->Import(scene);

	importer->Destroy();
	ios->Destroy();

	node = scene->GetRootNode();

	// Pmx Kit

	pmx::PmxModel target;
	target.Init();

	std::string _FullFilePath = targetPath;
	std::ifstream stream(_FullFilePath.c_str(), std::ifstream::binary);
	target.Read(&stream);

	{
		// LoadFile
		scene->FillAnimStackNameArray(animNameLists);

		const int poseCount = scene->GetPoseCount();
		for (int i = 0; i < poseCount; i++)
			pPoseArray.Add(scene->GetPose(i));

		time.SetTime(0, 0, 0, 1, 0, scene->GetGlobalSettings().GetTimeMode());

		// Curr Anim Stack
		FbxAnimStack* currAnimStack = NULL;
		FbxAnimLayer* currAnimLayer = NULL;

		if (animNameLists.Size() > 0)
			currAnimStack = scene->FindMember<FbxAnimStack>(animNameLists[0]->Buffer());

		if (currAnimStack == NULL)
			return 1;

		currAnimLayer = currAnimStack->GetMember<FbxAnimLayer>();

		scene->SetCurrentAnimationStack(currAnimStack);

		// Get Anim Time
		FbxTime mStart, mStop;

		for (int i = 0; i < animNameLists.Size(); i++) {
			currTakeInfo = scene->GetTakeInfo(*(animNameLists[i]));
			if (currTakeInfo)
			{
				mStart = currTakeInfo->mLocalTimeSpan.GetStart();
				mStop = currTakeInfo->mLocalTimeSpan.GetStop();
			}
			else
			{
				continue;
			}

			mStarts.push_back(mStart);
			mStops.push_back(mStop);

			countOfFrame.push_back(mStops[i].GetFrameCount());
		}
		long long halfFrame = mStops[0].GetFrameCount();
		long long perFrame = mStops[0].Get() / mStops[0].GetFrameCount(mStops[0].eFrames30);

		std::ifstream checkExistsHello(std::string("Hello").c_str());
		if (checkExistsHello.good())
			std::remove("Hello");

		std::ofstream outFile(std::string("Hello"), std::ios::out | std::ios::binary);
		if (!outFile.is_open())
			throw std::runtime_error("");

		// GetRootTransform
		pRootGlobalPosition = GetGlobalPosition(node->GetChild(0), 0);

		DrawBoneRecursive
		(
			node,
			mStops,
			mCurrentTime,
			pRootGlobalPosition,
			pParentGlobalPosition,
			NULL,
			perFrame,
			outFile
		);

		const int FinSize = 1;

		outFile.write((const char*)&FinSize, sizeof(int));
		outFile.write("FIN\n", 4);
		outFile.close();
	}

	{
		// 다음 파일에 애니메이션 정보 저장
		std::ifstream inFile(std::string("Hello"), std::ios::in | std::ios::binary);
		if (!inFile.is_open())
			throw std::runtime_error("");

		long long mAnimFrameCount = mStops[0].GetFrameCount(fbxsdk::FbxTime::eFrames30);

		std::vector<std::vector<std::array<DirectX::XMFLOAT4, 2>>> bones(target.bone_count);

		int targetCount = 0;
		int nameIDX = 0;
		char Name[10][30];
		char Buffer;

		int i = 0;

		std::vector<int> targetBoneIDX;

		std::string findBoneName;

		int bIDX = 0;
		int scaling = 0;

		// 카피하고자 하는 본만 추출하여 해당 본에만 카피
		while (true)
		{
			inFile.read((char*)&targetCount, sizeof(int));

			for (i = 0; i < targetCount; i++)
			{
				nameIDX = 0;

				do {
					if (nameIDX > 30)
						throw std::runtime_error("Failed to Read");

					inFile.read(&Buffer, 1);
					Name[i][nameIDX++] = Buffer;
				} while (Buffer != '\n');
			}

			if (
				Name[0][0] == 'F' &&
				Name[0][1] == 'I' &&
				Name[0][2] == 'N' &&
				Name[0][3] == '\n'
				) break;

			for (i = 0; i < targetCount; i++)
			{
				std::string findTargetBoneName(Name[i], nameIDX - 1);

				// find bone by name
				for (i = 0; i < target.bone_count; i++)
				{
					findBoneName.clear();

					findBoneName.assign(
						target.bones[i].bone_english_name.begin(),
						target.bones[i].bone_english_name.end()
					);

					if (findTargetBoneName == findBoneName)
					{
						targetBoneIDX.push_back(i);

						bones[i].resize(mAnimFrameCount);
					}
				}
			} // for (int i = 0; i < targetCount; i++)

			std::vector<DirectX::XMFLOAT4> _CopyData(mAnimFrameCount * 2);
			inFile.read(
				(char*)_CopyData.data(),
				// 1 raw => Pos, Quat (* 2)
				(sizeof(DirectX::XMFLOAT4) * 2) * mAnimFrameCount
			);

			if (targetBoneIDX.size() > 0)
			{
				i = targetBoneIDX[targetBoneIDX.size() - 1];

				for (scaling = 0; scaling < bones[i].size(); scaling++) {
					bones[i][scaling][0].x = _CopyData[scaling * 2].x;
					bones[i][scaling][0].y = _CopyData[scaling * 2].y;
					bones[i][scaling][0].z = _CopyData[scaling * 2].z;
					bones[i][scaling][0].w = 1.0f;

					bones[i][scaling][1].x = _CopyData[scaling * 2 + 1].x;
					bones[i][scaling][1].y = _CopyData[scaling * 2 + 1].y;
					bones[i][scaling][1].z = _CopyData[scaling * 2 + 1].z;
					bones[i][scaling][1].w = _CopyData[scaling * 2 + 1].w;
				}
			}
		}

		inFile.close();

		int rootIDX = 0;
		bool isUpdate = false;

		// Root index 설정
		// 카피 된 본 중 가장 조상인 본을 찾아 루트본으로 한다.
		for (int i = 0; i < bones.size(); i++)
		{
			if (bones[i].size() > 2)
			{
				rootIDX = i;
				break;
			}
		}

		// 우선 루트 본을 가져온다 (Frame = 0)
		bones[0].resize(mAnimFrameCount);
		for (int anim = 0; anim < mAnimFrameCount; anim++)
		{
			bones[0][anim][0].x =
				bones[rootIDX][anim][0].x;
			bones[0][anim][0].y =
				bones[rootIDX][anim][0].y;
			bones[0][anim][0].z =
				bones[rootIDX][anim][0].z;
			bones[0][anim][0].w = 1.0f;

			bones[0][anim][1].x =
				bones[rootIDX][anim][1].x;
			bones[0][anim][1].y =
				bones[rootIDX][anim][1].y;
			bones[0][anim][1].z =
				bones[rootIDX][anim][1].z;
			bones[0][anim][1].w =
				bones[rootIDX][anim][1].w;
		}

		i = 1;
		int j = 0;
		int parentIDX = 0;
		int anim = 0;
		// Frame 1 부터는 루트 본 -> 애니메이션 본의 델타를 구해서 저장.
		for (i = 1; i < target.bone_count; i++)
		{
			// 만일 현재 뼈대가 업데이트 되지 않은 뼈대라면 
			if (bones[i].size() < 2)
			{
				bones[i].resize(mAnimFrameCount);

				// 부모(바로 이전 뼈대)의 뼈대가 업데이트 되었다면
				if (isUpdate)
				{
					rootIDX = target.bones[i].parent_index;
					parentIDX = target.bones[i].parent_index;

					for (anim = 0; anim < bones[parentIDX].size(); anim++)
					{
						bones[i][anim][0].x =
							bones[parentIDX][anim][0].x;
						bones[i][anim][0].y =
							bones[parentIDX][anim][0].y;
						bones[i][anim][0].z =
							bones[parentIDX][anim][0].z;
						bones[i][anim][0].w = 1.0f;

						bones[i][anim][1].x =
							bones[parentIDX][anim][1].x;
						bones[i][anim][1].y =
							bones[parentIDX][anim][1].y;
						bones[i][anim][1].z =
							bones[parentIDX][anim][1].z;
						bones[i][anim][1].w =
							bones[parentIDX][anim][1].w;
					}
				}
				// 부모 뼈대가 업데이트 상태가 아니라면
				else
				{
					for (anim = 0; anim < mAnimFrameCount; anim++)
					{
						bones[i][anim][0].x =
							bones[rootIDX][anim][0].x;
						bones[i][anim][0].y =
							bones[rootIDX][anim][0].y;
						bones[i][anim][0].z =
							bones[rootIDX][anim][0].z;
						bones[i][anim][0].w = 1.0f;

						bones[i][anim][1].x =
							bones[rootIDX][anim][1].x;
						bones[i][anim][1].y =
							bones[rootIDX][anim][1].y;
						bones[i][anim][1].z =
							bones[rootIDX][anim][1].z;
						bones[i][anim][1].w =
							bones[rootIDX][anim][1].w;
					}
				}
			}
			else {
				isUpdate = true;
			}
		}

		// Transpose Bones
		std::vector<std::vector<std::array<DirectX::XMFLOAT4, 2>>> transposeBones(mAnimFrameCount);
		for (i = 0; i < mAnimFrameCount; i++)
			transposeBones[i].resize(target.bone_count);

		float testy(0.0f);
		for (i = 0; i < target.bone_count; i++)
		{
			testy = 0.0f;
			//if (target.bones[i].bone_english_name == L"Shoulder.L" ||
			//	target.bones[i].bone_english_name == L"Arm.L")
			//{
			//	testy = -0.5f;
			//}
			//if (target.bones[i].bone_english_name == L"Shoulder.R" ||
			//	target.bones[i].bone_english_name == L"Arm.R")
			//{
			//	testy = 0.5f;
			//}

			for (j = 0; j < mAnimFrameCount; j++)
			{
				// Transpose Mat
				bones[i][j][0].y += testy;

				transposeBones[j][i][0] = bones[i][j][0];
				transposeBones[j][i][1] = bones[i][j][1];
			}
		}

		std::ifstream checkExistsResFile(std::string("resFile").c_str());
		if (checkExistsResFile.good())
			std::remove("resFile");

		std::ofstream outFile(std::string("resFile"), std::ios::out | std::ios::binary);

		outFile.write((char*)&mAnimFrameCount, sizeof(int));

		for (i = 0; i < mAnimFrameCount; i++)
		{
			for (j = 0; j < target.bone_count; j++)
			{
				outFile.write((char*)&transposeBones[i][j][0].x, sizeof(float));
				outFile.write((char*)&transposeBones[i][j][0].y, sizeof(float));
				outFile.write((char*)&transposeBones[i][j][0].z, sizeof(float));
				outFile.write((char*)&transposeBones[i][j][0].w, sizeof(float));

				outFile.write((char*)&transposeBones[i][j][1].x, sizeof(float));
				outFile.write((char*)&transposeBones[i][j][1].y, sizeof(float));
				outFile.write((char*)&transposeBones[i][j][1].z, sizeof(float));
				outFile.write((char*)&transposeBones[i][j][1].w, sizeof(float));
			}
		}

		bones.clear();
		transposeBones.clear();

		outFile.close();
		throw std::runtime_error("WELL DONE!!");
	}

	return 0;
}

void GeometryGenerator::Subdivide(MeshData& meshData)
{
	// Save a copy of the input geometry.
	MeshData inputCopy = meshData;


	meshData.Vertices.resize(0);
	meshData.Indices32.resize(0);

	//       v1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// v0    m2     v2

	uint32 i = 0;
	uint32 numTris = (uint32)inputCopy.Indices32.size() / 3;

	Vertex v0;
	Vertex v1;
	Vertex v2;

	Vertex m0;
	Vertex m1;
	Vertex m2;

	for (i = 0; i < numTris; ++i)
	{
		v0 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 0]];
		v1 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 1]];
		v2 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 2]];

		m0 = MidPoint(v0, v1);
		m1 = MidPoint(v1, v2);
		m2 = MidPoint(v0, v2);

		meshData.Vertices.push_back(v0); // 0
		meshData.Vertices.push_back(v1); // 1
		meshData.Vertices.push_back(v2); // 2
		meshData.Vertices.push_back(m0); // 3
		meshData.Vertices.push_back(m1); // 4
		meshData.Vertices.push_back(m2); // 5

		meshData.Indices32.push_back(i * 6 + 0);
		meshData.Indices32.push_back(i * 6 + 3);
		meshData.Indices32.push_back(i * 6 + 5);

		meshData.Indices32.push_back(i * 6 + 3);
		meshData.Indices32.push_back(i * 6 + 4);
		meshData.Indices32.push_back(i * 6 + 5);

		meshData.Indices32.push_back(i * 6 + 5);
		meshData.Indices32.push_back(i * 6 + 4);
		meshData.Indices32.push_back(i * 6 + 2);

		meshData.Indices32.push_back(i * 6 + 3);
		meshData.Indices32.push_back(i * 6 + 1);
		meshData.Indices32.push_back(i * 6 + 4);
	}
}

GeometryGenerator::Vertex GeometryGenerator::MidPoint(const Vertex& v0, const Vertex& v1)
{
	XMVECTOR p0 = XMLoadFloat3(&v0.Position);
	XMVECTOR p1 = XMLoadFloat3(&v1.Position);

	XMVECTOR n0 = XMLoadFloat3(&v0.Normal);
	XMVECTOR n1 = XMLoadFloat3(&v1.Normal);

	XMVECTOR tan0 = XMLoadFloat3(&v0.TangentU);
	XMVECTOR tan1 = XMLoadFloat3(&v1.TangentU);

	XMVECTOR tex0 = XMLoadFloat2(&v0.TexC);
	XMVECTOR tex1 = XMLoadFloat2(&v1.TexC);

	// Compute the midpoints of all the attributes.  Vectors need to be normalized
	// since linear interpolating can make them not unit length.  
	XMVECTOR pos = 0.5f*(p0 + p1);
	XMVECTOR normal = XMVector3Normalize(0.5f*(n0 + n1));
	XMVECTOR tangent = XMVector3Normalize(0.5f*(tan0 + tan1));
	XMVECTOR tex = 0.5f*(tex0 + tex1);

	Vertex v;
	DirectX::XMStoreFloat3(&v.Position, pos);
	DirectX::XMStoreFloat3(&v.Normal, normal);
	DirectX::XMStoreFloat3(&v.TangentU, tangent);
	DirectX::XMStoreFloat2(&v.TexC, tex);

	return v;
}

GeometryGenerator::MeshData GeometryGenerator::CreateGeosphere(float radius, uint32 numSubdivisions)
{
	MeshData meshData;

	// Put a cap on the number of subdivisions.
	numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

	// Approximate a sphere by tessellating an icosahedron.

	const float X = 0.525731f;
	const float Z = 0.850651f;

	XMFLOAT3 pos[12] =
	{
		{ -X, 0.0f, Z },  { X, 0.0f, Z },
		{ -X, 0.0f, -Z }, { X, 0.0f, -Z },
		{ 0.0f, Z, X }, { 0.0f, Z, -X },
		{ 0.0f, -Z, X },  { 0.0f, -Z, -X },
		{ Z, X, 0.0f }, { -Z, X, 0.0f },
		{ Z, -X, 0.0f }, { -Z, -X, 0.0f }
	};

	uint32 k[60] =
	{
		1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
		1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
		3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
		10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
	};

	meshData.Vertices.resize(12);
	meshData.Indices32.assign(&k[0], &k[60]);

	uint32 i = 0;

	for (i = 0; i < 12; ++i)
		meshData.Vertices[i].Position = pos[i];

	for (i = 0; i < numSubdivisions; ++i)
		Subdivide(meshData);

	// Project vertices onto sphere and scale.
	XMVECTOR n, p, T;
	float theta, phi;

	for (i = 0; i < meshData.Vertices.size(); ++i)
	{
		// Project onto unit sphere.
		n = XMVector3Normalize(XMLoadFloat3(&meshData.Vertices[i].Position));

		// Project onto sphere.
		p = radius * n;

		DirectX::XMStoreFloat3(&meshData.Vertices[i].Position, p);
		DirectX::XMStoreFloat3(&meshData.Vertices[i].Normal, n);

		// Derive texture coordinates from spherical coordinates.
		theta = atan2f(meshData.Vertices[i].Position.z, meshData.Vertices[i].Position.x);

		// Put in [0, 2pi].
		if (theta < 0.0f)
			theta += XM_2PI;

		phi = acosf(meshData.Vertices[i].Position.y / radius);

		meshData.Vertices[i].TexC.x = theta / XM_2PI;
		meshData.Vertices[i].TexC.y = phi / XM_PI;

		// Partial derivative of P with respect to theta
		meshData.Vertices[i].TangentU.x = -radius * sinf(phi)*sinf(theta);
		meshData.Vertices[i].TangentU.y = 0.0f;
		meshData.Vertices[i].TangentU.z = +radius * sinf(phi)*cosf(theta);

		T = XMLoadFloat3(&meshData.Vertices[i].TangentU);
		DirectX::XMStoreFloat3(&meshData.Vertices[i].TangentU, XMVector3Normalize(T));
	}

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount)
{
	MeshData meshData;

	//
	// Build Stacks.
	// 

	float stackHeight = height / stackCount;

	// Amount to increment radius as we move up each stack level from bottom to top.
	float radiusStep = (topRadius - bottomRadius) / stackCount;

	uint32 i = 0;
	uint32 j = 0;
	uint32 ringCount = stackCount + 1;

	// Compute vertices for each stack ring starting at the bottom and moving up.
	float y;
	float r;
	float dTheta;
	Vertex vertex;
	float c, s;
	float dr;
	XMFLOAT3 bitangent;

	XMVECTOR T;
	XMVECTOR B;
	XMVECTOR N;

	for (i = 0; i < ringCount; ++i)
	{
		y = -0.5f*height + i * stackHeight;
		r = bottomRadius + i * radiusStep;

		// vertices of ring
		dTheta = 2.0f*XM_PI / sliceCount;
		for (j = 0; j <= sliceCount; ++j)
		{
			c = cosf(j*dTheta);
			s = sinf(j*dTheta);

			vertex.Position = { r*c, y, r*s };

			vertex.TexC.x = (float)j / sliceCount;
			vertex.TexC.y = 1.0f - (float)i / stackCount;

			// Cylinder can be parameterized as follows, where we introduce v
			// parameter that goes in the same direction as the v tex-coord
			// so that the bitangent goes in the same direction as the v tex-coord.
			//   Let r0 be the bottom radius and let r1 be the top radius.
			//   y(v) = h - hv for v in [0,1].
			//   r(v) = r1 + (r0-r1)v
			//
			//   x(t, v) = r(v)*cos(t)
			//   y(t, v) = h - hv
			//   z(t, v) = r(v)*sin(t)
			// 
			//  dx/dt = -r(v)*sin(t)
			//  dy/dt = 0
			//  dz/dt = +r(v)*cos(t)
			//
			//  dx/dv = (r0-r1)*cos(t)
			//  dy/dv = -h
			//  dz/dv = (r0-r1)*sin(t)

			// This is unit length.
			vertex.TangentU = XMFLOAT3(-s, 0.0f, c);

			dr = bottomRadius - topRadius;
			bitangent = { dr*c, -height, dr*s };

			T = XMLoadFloat3(&vertex.TangentU);
			B = XMLoadFloat3(&bitangent);
			N = XMVector3Normalize(XMVector3Cross(T, B));
			DirectX::XMStoreFloat3(&vertex.Normal, N);

			meshData.Vertices.push_back(vertex);
		}
	}

	// Add one because we duplicate the first and last vertex per ring
	// since the texture coordinates are different.
	uint32 ringVertexCount = sliceCount + 1;

	// Compute indices for each stack.
	for (i = 0; i < stackCount; ++i)
	{
		for (j = 0; j < sliceCount; ++j)
		{
			meshData.Indices32.push_back(i*ringVertexCount + j);
			meshData.Indices32.push_back((i + 1)*ringVertexCount + j);
			meshData.Indices32.push_back((i + 1)*ringVertexCount + j + 1);

			meshData.Indices32.push_back(i*ringVertexCount + j);
			meshData.Indices32.push_back((i + 1)*ringVertexCount + j + 1);
			meshData.Indices32.push_back(i*ringVertexCount + j + 1);
		}
	}

	BuildCylinderTopCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
	BuildCylinderBottomCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);

	return meshData;
}

void GeometryGenerator::BuildCylinderTopCap(float bottomRadius, float topRadius, float height,
	uint32 sliceCount, uint32 stackCount, MeshData& meshData)
{
	uint32 baseIndex = (uint32)meshData.Vertices.size();

	float y = 0.5f*height;
	float dTheta = 2.0f*XM_PI / sliceCount;

	uint32 i = 0;

	float x;
	float z;
	float u;
	float v;

	// Duplicate cap ring vertices because the texture coordinates and normals differ.
	for (i = 0; i <= sliceCount; ++i)
	{
		x = topRadius * cosf(i*dTheta);
		z = topRadius * sinf(i*dTheta);

		// Scale down by the height to try and make top cap texture coord area
		// proportional to base.
		u = x / height + 0.5f;
		v = z / height + 0.5f;

		meshData.Vertices.push_back(Vertex(x, y, z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
	}

	// Cap center vertex.
	meshData.Vertices.push_back(Vertex(0.0f, y, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f));

	// Index of center vertex.
	uint32 centerIndex = (uint32)meshData.Vertices.size() - 1;

	for (i = 0; i < sliceCount; ++i)
	{
		meshData.Indices32.push_back(centerIndex);
		meshData.Indices32.push_back(baseIndex + i + 1);
		meshData.Indices32.push_back(baseIndex + i);
	}
}

void GeometryGenerator::BuildCylinderBottomCap(float bottomRadius, float topRadius, float height,
	uint32 sliceCount, uint32 stackCount, MeshData& meshData)
{
	// 
	// Build bottom cap.
	//

	uint32 baseIndex = (uint32)meshData.Vertices.size();
	float y = -0.5f*height;

	uint32 i = 0;
	uint32 j = 0;

	float x;
	float z;
	float u;
	float v;

	// vertices of ring
	float dTheta = 2.0f*XM_PI / sliceCount;
	for (i = 0; i <= sliceCount; ++i)
	{
		x = bottomRadius * cosf(i*dTheta);
		z = bottomRadius * sinf(i*dTheta);

		// Scale down by the height to try and make top cap texture coord area
		// proportional to base.
		u = x / height + 0.5f;
		v = z / height + 0.5f;

		meshData.Vertices.push_back({ x, y, z, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v });
	}

	// Cap center vertex.
	meshData.Vertices.push_back({ 0.0f, y, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f
		});

	// Cache the index of center vertex.
	uint32 centerIndex = (uint32)meshData.Vertices.size() - 1;

	for (i = 0; i < sliceCount; ++i)
	{
		meshData.Indices32.push_back(centerIndex);
		meshData.Indices32.push_back(baseIndex + i);
		meshData.Indices32.push_back(baseIndex + i + 1);
	}
}

GeometryGenerator::MeshData GeometryGenerator::CreateGrid(float width, float depth, uint32 m, uint32 n)
{
	MeshData meshData;

	uint32 vertexCount = m * n;
	uint32 faceCount = (m - 1)*(n - 1) * 2;

	//
	// Create the vertices.
	//

	uint32 i = 0;
	uint32 j = 0;

	float x;
	float z;

	float halfWidth = 0.5f*width;
	float halfDepth = 0.5f*depth;

	float dx = width / (n - 1);
	float dz = depth / (m - 1);

	float du = 1.0f / (n - 1);
	float dv = 1.0f / (m - 1);

	meshData.Vertices.resize(vertexCount);
	for (i = 0; i < m; ++i)
	{
		z = halfDepth - i * dz;
		for (j = 0; j < n; ++j)
		{
			x = -halfWidth + j * dx;

			meshData.Vertices[i*n + j].Position = XMFLOAT3(x, 0.0f, z);
			meshData.Vertices[i*n + j].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
			meshData.Vertices[i*n + j].TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);

			// Stretch texture over grid.
			meshData.Vertices[i*n + j].TexC.x = j * du;
			meshData.Vertices[i*n + j].TexC.y = i * dv;
		}
	}

	//
	// Create the indices.
	//

	meshData.Indices32.resize(faceCount * 3); // 3 indices per face

	// Iterate over each quad and compute indices.
	uint32 k = 0;
	for (i = 0; i < m - 1; ++i)
	{
		for (j = 0; j < n - 1; ++j)
		{
			meshData.Indices32[k] = i * n + j;
			meshData.Indices32[k + 1] = i * n + j + 1;
			meshData.Indices32[k + 2] = (i + 1)*n + j;

			meshData.Indices32[k + 3] = (i + 1)*n + j;
			meshData.Indices32[k + 4] = i * n + j + 1;
			meshData.Indices32[k + 5] = (i + 1)*n + j + 1;

			k += 6; // next quad
		}
	}

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateQuad(float x, float y, float w, float h, float depth)
{
	MeshData meshData;

	meshData.Vertices.resize(4);
	meshData.Indices32.resize(6);

	// Position coordinates specified in NDC space.
	meshData.Vertices[0] = Vertex(
		x, y - h, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f);

	meshData.Vertices[1] = Vertex(
		x, y, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f);

	meshData.Vertices[2] = Vertex(
		x + w, y, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f);

	meshData.Vertices[3] = Vertex(
		x + w, y - h, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f);

	meshData.Indices32[0] = 0;
	meshData.Indices32[1] = 1;
	meshData.Indices32[2] = 2;

	meshData.Indices32[3] = 0;
	meshData.Indices32[4] = 2;
	meshData.Indices32[5] = 3;

	return meshData;
}


/////////////////////////////////////////////////////

// Get the matrix of the given pose
FbxAMatrix GetPoseMatrix(
	FbxPose* pPose,
	int pNodeIndex
)
{
	FbxAMatrix lPoseMatrix;
	FbxMatrix lMatrix = pPose->GetMatrix(pNodeIndex);

	memcpy((double*)lPoseMatrix, (double*)lMatrix, sizeof(lMatrix.mData));

	return lPoseMatrix;
}

FbxAMatrix GetGlobalPosition(
	FbxNode* pNode,
	const FbxTime& pTime,
	FbxPose* pPose,
	FbxAMatrix* pParentGlobalPosition
)
{
	FbxAMatrix lGlobalPosition;
	bool        lPositionFound = false;

	if (pPose)
	{
		int lNodeIndex = pPose->Find(pNode);

		if (lNodeIndex > -1)
		{
			// The bind pose is always a global matrix.
			// If we have a rest pose, we need to check if it is
			// stored in global or local space.
			if (pPose->IsBindPose() || !pPose->IsLocalMatrix(lNodeIndex))
			{
				lGlobalPosition = GetPoseMatrix(pPose, lNodeIndex);
			}
			else
			{
				// We have a local matrix, we need to convert it to
				// a global space matrix.
				FbxAMatrix lParentGlobalPosition;

				if (pParentGlobalPosition)
				{
					lParentGlobalPosition = *pParentGlobalPosition;
				}
				else
				{
					if (pNode->GetParent())
					{
						lParentGlobalPosition = GetGlobalPosition(pNode->GetParent(), pTime, pPose);
					}
				}

				FbxAMatrix lLocalPosition = GetPoseMatrix(pPose, lNodeIndex);
				lGlobalPosition = lParentGlobalPosition * lLocalPosition;
			}

			lPositionFound = true;
		}
	}

	if (!lPositionFound)
	{
		// There is no pose entry for that node, get the current global position instead.

		// Ideally this would use parent global position and local position to compute the global position.
		// Unfortunately the equation 
		//    lGlobalPosition = pParentGlobalPosition * lLocalPosition
		// does not hold when inheritance type is other than "Parent" (RSrs).
		// To compute the parent rotation and scaling is tricky in the RrSs and Rrs cases.
		lGlobalPosition = pNode->EvaluateGlobalTransform(pTime);
	}

	return lGlobalPosition;
}

/////////////////////////////////////////////////////

void DrawNodeRecursive(
	FbxNode* pNode,
	std::vector<GeometryGenerator::MeshData>& meshData,
	bool uvMode
) {
	if (pNode->GetNodeAttribute())
	{
		DrawNode(
			pNode,
			meshData,
			uvMode
		);
	}

	const int childCount = pNode->GetChildCount();
	for (int childIndex = 0; childIndex < childCount; ++childIndex)
	{
		DrawNodeRecursive(
			pNode->GetChild(childIndex),
			meshData,
			uvMode
		);
	}
}

void DrawNodeRecursive(
	FbxNode* pNode,
	std::vector<FbxTime> mStarts,
	std::vector<FbxTime> mStops,
	FbxTime& pTime,
	std::vector<FbxAMatrix>& pParentGlobalPositions,
	FbxPose* pPose,
	std::vector<GeometryGenerator::MeshData>& meshData,
	std::vector<std::vector<float*>>& animVertexArrays,
	std::vector<FbxUInt>& mAnimVertexSizes,
	AnimationClip& mAnimClips
) {
	FbxAMatrix pParentGlobalPosition;

	FbxAMatrix globalPosition =
		GetGlobalPosition(
			pNode,
			pTime,
			pPose,
			&pParentGlobalPosition
		);

	pParentGlobalPositions.push_back(pParentGlobalPosition);

	if (pNode->GetNodeAttribute())
	{
		FbxAMatrix geomOffset;

		const FbxVector4 IT = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
		const FbxVector4 IR = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
		const FbxVector4 IS = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

		geomOffset = FbxAMatrix(IT, IR, IS);

		FbxAMatrix gOffPosition = globalPosition * geomOffset;

		DrawNode(
			pNode,
			mStarts,
			mStops,
			gOffPosition,
			meshData,
			animVertexArrays,
			mAnimVertexSizes,
			mAnimClips
		);
	}

	const int childCount = pNode->GetChildCount();
	for (int childIndex = 0; childIndex < childCount; ++childIndex)
	{
		DrawNodeRecursive(
			pNode->GetChild(childIndex),
			mStarts,
			mStops,
			pTime,
			pParentGlobalPositions,
			pPose,
			meshData,
			animVertexArrays,
			mAnimVertexSizes,
			mAnimClips
		);
	}
}

void DrawBoneRecursive(
	FbxNode* pNode,
	std::vector<FbxTime> mStops,
	FbxTime& pTime,
	FbxAMatrix& pGlobalRootPositions,
	std::vector<FbxAMatrix>& pParentGlobalPositions,
	FbxPose* pPose,
	long long perFrame,
	std::ofstream& outFile
)
{
	FbxAMatrix pParentGlobalPosition;

	FbxAMatrix globalPosition = 
		GetGlobalPosition(
			pNode, 
			pTime, 
			pPose, 
			&pParentGlobalPosition
		);
	pParentGlobalPositions.push_back(globalPosition);

	if (pNode->GetNodeAttribute())
	{
		DrawBone(
			pNode,
			mStops,
			pGlobalRootPositions,
			pParentGlobalPosition,
			perFrame,
			outFile
		);
	}

	const int childCount = pNode->GetChildCount();
	for (int childIndex = 0; childIndex < childCount; ++childIndex)
	{
		DrawBoneRecursive(
			pNode->GetChild(childIndex),
			mStops,
			pTime,
			pGlobalRootPositions,
			pParentGlobalPositions,
			pPose,
			perFrame,
			outFile
		);
	}
}

// Draw Skeleton
void DrawSkeleton(
	FbxNode* pNode,
	FbxSkeleton* skeleton,
	int targetBoneCount,
	pmx::PmxBone* targetBone,
	std::vector<FbxTime> mStarts,
	std::vector<FbxTime> mStops,
	FbxTime& pTime,
	FbxAnimLayer* pAnimLayer,
	FbxAMatrix& pParentGlobalPosition,
	FbxAMatrix& pGlobalPosition,
	long long perFrame,
	std::ofstream& outFile
)
{
	if (skeleton->GetSkeletonType() == FbxSkeleton::eLimbNode &&
		pNode->GetParent() &&
		pNode->GetParent()->GetNodeAttribute() &&
		pNode->GetParent()->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		std::string boneName(pNode->GetName());

		try {
			int transSkeletonPairSize = (int)transSkeletonPairs.at(boneName).size();
			if (transSkeletonPairSize == 0)
				return;
		}
		catch (std::exception&) {
			return;
		}

		// The Bones that is Non-Adapted Animation

		// Global Base Position
		FbxVector4 head = pParentGlobalPosition.GetT();
		// Global End Position
		FbxVector4 tail = pGlobalPosition.GetT();

		// The Bones that is Adapted Animation
		FbxAMatrix fbxAMatrix =
			FbxAMatrix(
				pNode->GetGeometricTranslation(FbxNode::eSourcePivot),
				pNode->GetGeometricRotation(FbxNode::eSourcePivot),
				pNode->GetGeometricScaling(FbxNode::eSourcePivot)
			);

		FbxAMatrix global;
		FbxVector4 globalT;
		FbxVector4 globalR;
		FbxVector4 fbxMatT;

		// Pmx에게 옮길 뼈대의 개수
		int transSkeletonPairsSize = (int)transSkeletonPairs.at(boneName).size();

		outFile.write((char*)&transSkeletonPairsSize, sizeof(int));

		for (int i = 0; i < transSkeletonPairsSize; i++)
		{
			outFile.write((const char*)transSkeletonPairs.at(boneName)[i].c_str(), transSkeletonPairs.at(boneName)[i].size());
			outFile.write("\n", 1);
		}

		int frameCount = (int)mStops[0].GetFrameCount(FbxTime::eFrames30);

		float convBuf;
		for (int animFrame = 0; animFrame < frameCount; animFrame++)
		{
			global =
				GetGlobalPosition(
					pNode,
					animFrame * perFrame,
					NULL,
					&pParentGlobalPosition
				);
			globalT = global.GetT();

			convBuf = (float)globalT[0];
			outFile.write((char*)&convBuf, sizeof(float));
			convBuf = (float)globalT[1];
			outFile.write((char*)&convBuf, sizeof(float));
			convBuf = (float)globalT[2];
			outFile.write((char*)&convBuf, sizeof(float));
		}
	}
}

// Deform the vertex array with the shapes contained in the mesh.
void ComputeShapeDeformation(
	FbxMesh* pMesh,
	FbxTime& pTime,
	FbxAnimLayer* pAnimLayer,
	FbxVector4* pVertexArray
)
{
	int lVertexCount = pMesh->GetControlPointsCount();

	FbxVector4* lSrcVertexArray = pVertexArray;
	FbxVector4* lDstVertexArray = new FbxVector4[lVertexCount];
	memcpy(lDstVertexArray, pVertexArray, lVertexCount * sizeof(FbxVector4));

	int lBlendShapeDeformerCount = pMesh->GetDeformerCount(FbxDeformer::eBlendShape);
	for (int lBlendShapeIndex = 0; lBlendShapeIndex < lBlendShapeDeformerCount; ++lBlendShapeIndex)
	{
		FbxBlendShape* lBlendShape = (FbxBlendShape*)pMesh->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

		int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
		for (int lChannelIndex = 0; lChannelIndex < lBlendShapeChannelCount; ++lChannelIndex)
		{
			FbxBlendShapeChannel* lChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);
			if (lChannel)
			{
				// Get the percentage of influence on this channel.
				FbxAnimCurve* lFCurve = pMesh->GetShapeChannel(lBlendShapeIndex, lChannelIndex, pAnimLayer);
				if (!lFCurve) continue;
				double lWeight = lFCurve->Evaluate(pTime);

				lFCurve->Destroy();

				/*
				If there is only one targetShape on this channel, the influence is easy to calculate:
				influence = (targetShape - baseGeometry) * weight * 0.01
				dstGeometry = baseGeometry + influence

				But if there are more than one targetShapes on this channel, this is an in-between
				blendshape, also called progressive morph. The calculation of influence is different.

				For example, given two in-between targets, the full weight percentage of first target
				is 50, and the full weight percentage of the second target is 100.
				When the weight percentage reach 50, the base geometry is already be fully morphed
				to the first target shape. When the weight go over 50, it begin to morph from the
				first target shape to the second target shape.

				To calculate influence when the weight percentage is 25:
				1. 25 falls in the scope of 0 and 50, the morphing is from base geometry to the first target.
				2. And since 25 is already half way between 0 and 50, so the real weight percentage change to
				the first target is 50.
				influence = (firstTargetShape - baseGeometry) * (25-0)/(50-0) * 100
				dstGeometry = baseGeometry + influence

				To calculate influence when the weight percentage is 75:
				1. 75 falls in the scope of 50 and 100, the morphing is from the first target to the second.
				2. And since 75 is already half way between 50 and 100, so the real weight percentage change
				to the second target is 50.
				influence = (secondTargetShape - firstTargetShape) * (75-50)/(100-50) * 100
				dstGeometry = firstTargetShape + influence
				*/

				// Find the two shape indices for influence calculation according to the weight.
				// Consider index of base geometry as -1.

				int lShapeCount = lChannel->GetTargetShapeCount();
				double* lFullWeights = lChannel->GetTargetShapeFullWeights();

				// Find out which scope the lWeight falls in.
				int lStartIndex = -1;
				int lEndIndex = -1;
				for (int lShapeIndex = 0; lShapeIndex < lShapeCount; ++lShapeIndex)
				{
					if (lWeight > 0 && lWeight <= lFullWeights[0])
					{
						lEndIndex = 0;
						break;
					}
					if (lWeight > lFullWeights[lShapeIndex] && lWeight < lFullWeights[lShapeIndex + 1])
					{
						lStartIndex = lShapeIndex;
						lEndIndex = lShapeIndex + 1;
						break;
					}
				}

				FbxShape* lStartShape = NULL;
				FbxShape* lEndShape = NULL;
				if (lStartIndex > -1)
				{
					lStartShape = lChannel->GetTargetShape(lStartIndex);
				}
				if (lEndIndex > -1)
				{
					lEndShape = lChannel->GetTargetShape(lEndIndex);
				}

				//The weight percentage falls between base geometry and the first target shape.
				if (lStartIndex == -1 && lEndShape)
				{
					double lEndWeight = lFullWeights[0];
					// Calculate the real weight.
					lWeight = (lWeight / lEndWeight) * 100;
					// Initialize the lDstVertexArray with vertex of base geometry.
					memcpy(lDstVertexArray, lSrcVertexArray, lVertexCount * sizeof(FbxVector4));
					for (int j = 0; j < lVertexCount; j++)
					{
						// Add the influence of the shape vertex to the mesh vertex.
						FbxVector4 lInfluence = (lEndShape->GetControlPoints()[j] - lSrcVertexArray[j]) * lWeight * 0.01;
						lDstVertexArray[j] += lInfluence;
					}
				}
				//The weight percentage falls between two target shapes.
				else if (lStartShape && lEndShape)
				{
					double lStartWeight = lFullWeights[lStartIndex];
					double lEndWeight = lFullWeights[lEndIndex];
					// Calculate the real weight.
					lWeight = ((lWeight - lStartWeight) / (lEndWeight - lStartWeight)) * 100;
					// Initialize the lDstVertexArray with vertex of the previous target shape geometry.
					memcpy(lDstVertexArray, lStartShape->GetControlPoints(), lVertexCount * sizeof(FbxVector4));
					for (int j = 0; j < lVertexCount; j++)
					{
						// Add the influence of the shape vertex to the previous shape vertex.
						FbxVector4 lInfluence = (lEndShape->GetControlPoints()[j] - lStartShape->GetControlPoints()[j]) * lWeight * 0.01;
						lDstVertexArray[j] += lInfluence;
					}
				}
			}//If lChannel is valid
		}//For each blend shape channel
	}//For each blend shape deformer

	memcpy(pVertexArray, lDstVertexArray, lVertexCount * sizeof(FbxVector4));

	delete[] lDstVertexArray;
}

// Get the geometry offset to a node. It is never inherited by the children.
FbxAMatrix GetGeometry(FbxNode* pNode)
{
	const FbxVector4 lT = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 lR = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 lS = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

	return FbxAMatrix(lT, lR, lS);
}

//Compute the transform matrix that the cluster will transform the vertex.
void ComputeClusterDeformation(
	FbxAMatrix& pGlobalPosition,
	FbxMesh* pMesh,
	FbxCluster* pCluster,
	FbxAMatrix& pVertexTransformMatrix,
	FbxTime pTime)
{
	FbxCluster::ELinkMode lClusterMode = pCluster->GetLinkMode();

	FbxAMatrix lReferenceGlobalInitPosition;
	FbxAMatrix lReferenceGlobalCurrentPosition;
	FbxAMatrix lAssociateGlobalInitPosition;
	FbxAMatrix lAssociateGlobalCurrentPosition;
	FbxAMatrix lClusterGlobalInitPosition;
	FbxAMatrix lClusterGlobalCurrentPosition;

	FbxAMatrix lReferenceGeometry;
	FbxAMatrix lAssociateGeometry;
	FbxAMatrix lClusterGeometry;

	FbxAMatrix lClusterRelativeInitPosition;
	FbxAMatrix lClusterRelativeCurrentPositionInverse;

	// 현재 뼈대의 변환 행렬을 lReferenceGlobalInitPosition에 복제
	pCluster->GetTransformMatrix(lReferenceGlobalInitPosition);
	lReferenceGlobalCurrentPosition = pGlobalPosition;
	// Multiply lReferenceGlobalInitPosition by Geometric Transformation

	FbxNode* pNode = pMesh->GetNode();

	lReferenceGeometry = FbxAMatrix(
		pNode->GetGeometricTranslation(FbxNode::eSourcePivot),
		pNode->GetGeometricRotation(FbxNode::eSourcePivot),
		pNode->GetGeometricScaling(FbxNode::eSourcePivot)
	);

	lReferenceGlobalInitPosition *= lReferenceGeometry;

	// Get the link initial global position and the link current global position.
	// 초기 T 포즈에서의 Position과 현재 포즈에서의 Position을 이어주는 Link Matrix를 제공 
	pCluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);

	// lClusterGlobalCurrentPosition = GetGlobalPosition(pCluster->GetLink(), pTime, NULL);
	lClusterGlobalCurrentPosition = pCluster->GetLink()->EvaluateGlobalTransform(pTime);


	// Compute the initial position of the link relative to the reference.
	// T 포즈인 모든 뼈대를 원점으로 만들어주는 변환 행렬
	lClusterRelativeInitPosition =
		lClusterGlobalInitPosition.Inverse() *
		lReferenceGlobalInitPosition;

	// Compute the current position of the link relative to the reference.
	// 원점 위치의 뼈대를 현재 포즈로 만들어주는 변환 행렬
	lClusterRelativeCurrentPositionInverse =
		lReferenceGlobalCurrentPosition.Inverse() *
		lClusterGlobalCurrentPosition;

	// Compute the shift of the link relative to the reference.
	// 다음 포즈를 만들어주는 변환 행렬을 만들어냅니다.
	pVertexTransformMatrix = lClusterRelativeCurrentPositionInverse * lClusterRelativeInitPosition;
}

void MatrixScale(FbxAMatrix& pMatrix, double pValue)
{
	int i, j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pMatrix[i][j] *= pValue;
		}
	}
}

void MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue)
{
	pMatrix[0][0] += pValue;
	pMatrix[1][1] += pValue;
	pMatrix[2][2] += pValue;
	pMatrix[3][3] += pValue;
}

void MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix)
{
	int i, j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pDstMatrix[i][j] += pSrcMatrix[i][j];
		}
	}
}

// Deform the vertex array in classic linear way.

void ComputeLinearDeformation(
	FbxAMatrix& pGlobalPosition,
	FbxMesh* pMesh,
	FbxTime& pTime,
	FbxVector4* pVertexArray)
{
	// All the links must have the same link mode.
	FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

	int lVertexCount = pMesh->GetControlPointsCount();
	FbxAMatrix* lClusterDeformation = new FbxAMatrix[lVertexCount];
	memset(lClusterDeformation, 0, lVertexCount * sizeof(FbxAMatrix));

	double* lClusterWeight = new double[lVertexCount];
	memset(lClusterWeight, 0, lVertexCount * sizeof(double));

	if (lClusterMode == FbxCluster::eAdditive)
	{
		for (int i = 0; i < lVertexCount; ++i)
		{
			lClusterDeformation[i].SetIdentity();
		}
	}

	// For all skins and all clusters, accumulate their deformation and weight
	// on each vertices and store them in lClusterDeformation and lClusterWeight.
	int lSkinCount = pMesh->GetDeformerCount(FbxDeformer::eSkin);
	int lSkinIndex = 0;
	FbxSkin* lSkinDeformer = nullptr;
	int lClusterCount = 0;
	int lClusterIndex = 0;
	FbxCluster* lCluster = nullptr;
	// 각 프레임에 따른 뼈대 변형 행렬
	FbxAMatrix lVertexTransformMatrix;
	int lVertexIndexCount;
	int i = 0;
	int k = 0;
	int lIndex = 0;
	double lWeight = 0.0;
	FbxAMatrix lInfluence;

	for (lSkinIndex = 0; lSkinIndex < lSkinCount; ++lSkinIndex)
	{
		// Skin 로드
		lSkinDeformer = (FbxSkin*)pMesh->GetDeformer(lSkinIndex, FbxDeformer::eSkin);

		// 한 Skin에 클러스터(뼈대) 개수 로드 
		lClusterCount = lSkinDeformer->GetClusterCount();
		for (lClusterIndex = 0; lClusterIndex < lClusterCount; ++lClusterIndex)
		{
			// 뼈대 로드 
			lCluster = lSkinDeformer->GetCluster(lClusterIndex);
			// 정보가 없다면 패스
			if (!lCluster->GetLink())
				continue;

			// pTime 프레임 에서의 포즈 변환 행렬을 얻어옴.
			ComputeClusterDeformation(pGlobalPosition, pMesh, lCluster, lVertexTransformMatrix, pTime);
			// 정점의 개수 로드
			lVertexIndexCount = lCluster->GetControlPointIndicesCount();

			for (k = 0; k < lVertexIndexCount; ++k)
			{
				// 버텍스가 존재하는 인덱스 위치를 얻어옴
				lIndex = lCluster->GetControlPointIndices()[k];

				// Sometimes, the mesh can have less points than at the time of the skinning
				// because a smooth operator was active when skinning but has been deactivated during export.
				if (lIndex >= lVertexCount)
					continue;

				// Get Bone Weight
				lWeight = lCluster->GetControlPointWeights()[k];
				// Weight가 0.0일 때 Cloth Weight를 업데이트 하지 않음
				if (lWeight == 0.0)
					continue;

				// 현 버텍스가 받는 영향력을 계산 합니다.
				lInfluence = lVertexTransformMatrix;
				MatrixScale(lInfluence, lWeight);

				// 하나의 Vertex가 2개 이상의 Bone에 대해 영향을 받을 때
				if (lClusterMode == FbxCluster::eAdditive)
				{
					// Multiply with the product of the deformations on the vertex.
					MatrixAddToDiagonal(lInfluence, 1.0 - lWeight);
					lClusterDeformation[lIndex] = lInfluence * lClusterDeformation[lIndex];

					// Set the link to 1.0 just to know this vertex is influenced by a link.
					lClusterWeight[lIndex] = 1.0;
				}
				else // lLinkMode == FbxCluster::eNormalize || lLinkMode == FbxCluster::eTotalOne
				{
					// Add to the sum of the deformations on the vertex.
					MatrixAdd(lClusterDeformation[lIndex], lInfluence);

					// Add to the sum of weights to either normalize or complete the vertex.
					lClusterWeight[lIndex] += lWeight;
				}
			}//For each vertex		
		}//lClusterCount
	}

	//Actually deform each vertices here by information stored in lClusterDeformation and lClusterWeight
	FbxVector4 lSrcVertex;
	lWeight = 0;
	//double lWeight = 0;

	for (i = 0; i < lVertexCount; i++)
	{
		lSrcVertex = pVertexArray[i];
		FbxVector4& lDstVertex = pVertexArray[i];

		// count of weight == controlPointVertexCount
		lWeight = lClusterWeight[i];

		// Deform the vertex if there was at least a link with an influence on the vertex,
		if (lWeight != 0.0)
		{
			// 기본 IDLE Vertex Position에서 Pose 변형 행렬을 곱하여 결과 Vertex Position을 구한다
			lDstVertex = lClusterDeformation[i].MultT(lSrcVertex);

			if (lClusterMode == FbxCluster::eNormalize)
			{
				// In the normalized link mode, a vertex is always totally influenced by the links. 
				lDstVertex /= lWeight;
			}
			else if (lClusterMode == FbxCluster::eTotalOne)
			{
				// In the total 1 link mode, a vertex can be partially influenced by the links. 
				lSrcVertex *= (1.0 - lWeight);
				lDstVertex += lSrcVertex;
			}
		}
	}

	delete[] lClusterDeformation;
	delete[] lClusterWeight;
}

int DrawMesh(
	FbxNode* pNode,
	GeometryGenerator::MeshData& meshData,
	bool uvMode
)
{
	FbxMesh* lMesh = pNode->GetMesh();

	// FbxMesh
	FbxMesh* mesh = nullptr;
	// Material단위의 Surface
	FbxSurfaceMaterial* smat = nullptr;
	// Mesh 속성
	FbxProperty prop;
	// Material당 Texture  
	FbxLayeredTexture* layered_texture = nullptr;
	FbxFileTexture* fTexture = nullptr;

	// Get a FbxSurfaceMaterials Count
	int mcount = -1;
	// Get a Layer Count
	int lcount = -1;
	const char* file_texture_name = nullptr;

	const int vertexCount = lMesh->GetControlPointsCount();

	if (vertexCount == 0)
	{
		return 1;
	}

	//const bool hasShape = lMesh->GetShapeCount() > 0;
	//const bool hasSkin = lMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
	//const bool hasDeformation = hasShape || hasSkin;

	//if (!hasDeformation)
	//{
	//	return -1;
	//}

	// Find Texture Path
	mcount = pNode->GetSrcObjectCount<FbxSurfaceMaterial>();

	for (int i = 0; i < mcount; i++)
	{
		smat = (FbxSurfaceMaterial*)pNode->GetSrcObject<FbxSurfaceMaterial>(i);

		if (smat) {
			prop = smat->FindProperty(FbxSurfaceMaterial::sDiffuse);

			int layered_texture_count = prop.GetSrcObjectCount<FbxLayeredTexture>();
			if (layered_texture_count > 0)
			{
				for (int j = 0; j < layered_texture_count; j++)
				{
					layered_texture = FbxCast<FbxLayeredTexture>(prop.GetSrcObject<FbxLayeredTexture>(j));
					lcount = layered_texture->GetSrcObjectCount<FbxFileTexture>();
					for (int k = 0; k < lcount; k++)
					{
						fTexture = FbxCast<FbxFileTexture>(layered_texture->GetSrcObject<FbxFileTexture>(k));
						file_texture_name = fTexture->GetFileName();
					}
				}
			}
			else
			{
				int texture_count = prop.GetSrcObjectCount<FbxFileTexture>();
				for (int j = 0; j < texture_count; j++)
				{
					fTexture = FbxCast<FbxFileTexture>(prop.GetSrcObject<FbxFileTexture>(j));
					file_texture_name = fTexture->GetFileName();
				}
			}
		}
	}

	// Get a Textures Path Name
	if (file_texture_name) {
		meshData.texPath = std::string(file_texture_name);
	}
	else {
		meshData.texPath = "";
	}

	// GetControlPointVertex
	//if (hasDeformation)
	{
		meshData.Vertices.resize(vertexCount);

		for (int i = 0; i < vertexCount; i++) {
			meshData.Vertices[i].Position =
				DirectX::XMFLOAT3(
					static_cast<float>(lMesh->GetControlPointAt(i)[0]),
					static_cast<float>(lMesh->GetControlPointAt(i)[1]),
					static_cast<float>(lMesh->GetControlPointAt(i)[2])
				);

			meshData.Vertices[i].Normal =
				DirectX::XMFLOAT3(
					static_cast<float>(lMesh->GetElementNormal()->GetDirectArray().GetAt(i)[0]),
					static_cast<float>(lMesh->GetElementNormal()->GetDirectArray().GetAt(i)[1]),
					static_cast<float>(lMesh->GetElementNormal()->GetDirectArray().GetAt(i)[2])
				);
		}

		if (lMesh->GetElementTangentCount()) {
			for (int i = 0; i < vertexCount; i++) {
				meshData.Vertices[i].TangentU =
					DirectX::XMFLOAT3(
						static_cast<float>(lMesh->GetElementTangent()->GetDirectArray().GetAt(i)[0]),
						static_cast<float>(lMesh->GetElementTangent()->GetDirectArray().GetAt(i)[1]),
						static_cast<float>(lMesh->GetElementTangent()->GetDirectArray().GetAt(i)[2])
					);
			}
		}
		else {
			for (int i = 0; i < vertexCount; i++) {
				meshData.Vertices[i].TangentU =
					DirectX::XMFLOAT3(0, 0, 0);
			}
		}
	}

	// GetControlPointVertexIndices
	//if (hasDeformation)
	{
		unsigned int triCount = lMesh->GetPolygonCount();
		unsigned int vertexIndex = 0;

		std::vector<int> pre;

		unsigned i, j;
		uint32_t numIndices = 1;
		uint32_t polygonVertexSize = 0;

		for (i = 0; i < triCount; ++i)
		{
			polygonVertexSize = lMesh->GetPolygonSize(i) - 2;
			numIndices += polygonVertexSize * 3;
;
		}

		// Index Count
		uint32_t numIndex = -1;
		meshData.Indices32.resize(numIndices);

		for (i = 0; i < triCount; ++i)
		{
			polygonVertexSize = lMesh->GetPolygonSize(i);

			pre.resize(polygonVertexSize);
			for (j = 0; j < polygonVertexSize; ++j)
				pre[j] = lMesh->GetPolygonVertex(i, j);

			for (UINT indicesIDX = 0; indicesIDX < polygonVertexSize - 2; indicesIDX++) {
				meshData.Indices32[++numIndex] = pre[0];
				meshData.Indices32[++numIndex] = pre[indicesIDX + 1];
				meshData.Indices32[++numIndex] = pre[indicesIDX + 2];
			}
		}

		pre.clear();

		std::vector<fbxsdk::FbxVector2> uvs;
		{
			//get all UV set names
			FbxStringList lUVSetNameList;
			lMesh->GetUVSetNames(lUVSetNameList);

			//iterating over all uv sets
			for (int lUVSetIndex = 0; lUVSetIndex < lUVSetNameList.GetCount(); lUVSetIndex++)
			{
				//get lUVSetIndex-th uv set
				const char* lUVSetName = lUVSetNameList.GetStringAt(lUVSetIndex);
				const FbxGeometryElementUV* lUVElement = lMesh->GetElementUV(lUVSetName);

				if (!lUVElement)
					continue;

				// only support mapping mode eByPolygonVertex and eByControlPoint
				if (lUVElement->GetMappingMode() != FbxGeometryElement::eByPolygonVertex &&
					lUVElement->GetMappingMode() != FbxGeometryElement::eByControlPoint)
					printf("");

				//index array, where holds the index referenced to the uv data
				const bool lUseIndex = lUVElement->GetReferenceMode() != FbxGeometryElement::eDirect;
				const int lIndexCount = (lUseIndex) ? lUVElement->GetIndexArray().GetCount() : 0;

				//iterating through the data by polygon
				const int lPolyCount = lMesh->GetPolygonCount();

				if (lUVElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
				{
					for (int lPolyIndex = 0; lPolyIndex < lPolyCount; ++lPolyIndex)
					{
						// build the max index array that we need to pass into MakePoly
						const int lPolySize = lMesh->GetPolygonSize(lPolyIndex);
						for (int lVertIndex = 0; lVertIndex < lPolySize; ++lVertIndex)
						{
							FbxVector2 lUVValue;
							//get the index of the current vertex in control points array
							int lPolyVertIndex = lMesh->GetPolygonVertex(lPolyIndex, lVertIndex);

							//the UV index depends on the reference mode
							int lUVIndex = lUseIndex ? lUVElement->GetIndexArray().GetAt(lPolyVertIndex) : lPolyVertIndex;

							lUVValue = lUVElement->GetDirectArray().GetAt(lUVIndex);

							uvs.push_back(lUVValue);
						}
					}
				}
				else if (lUVElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
				{
					int vertexCounter = 0;
					int lPolyIndexCounter = 0;

					for (int lPolyIndex = 0; lPolyIndex < lPolyCount; ++lPolyIndex)
					{
						// build the max index array that we need to pass into MakePoly
						const int lPolySize = lMesh->GetPolygonSize(lPolyIndex);
						for (int lVertIndex = 0; lVertIndex < lPolySize; ++lVertIndex)
						{
							if (lPolyIndexCounter < lIndexCount)
							{
								FbxVector2 lUVValue;

								//if (uvMode) 
								//{
								//	int mTextureUVIndex = lMesh->GetTextureUVIndex(lPolyIndex, lVertIndex);

								//	lUVValue = lUVElement->GetDirectArray().GetAt(mTextureUVIndex);

								//	// Convert to floats
								//	meshData.Vertices[meshData.Indices32[vertexCounter]].TexC.x = static_cast<float>(lUVValue[0]);
								//	meshData.Vertices[meshData.Indices32[vertexCounter]].TexC.y = static_cast<float>(lUVValue[1]);

								//	vertexCounter++;
								//}
								//else
								{
									//the UV index depends on the reference mode
									int lUVIndex =
										lUseIndex ?
										lUVElement->GetIndexArray().GetAt(lPolyIndexCounter) :
										lPolyIndexCounter;

									lUVValue = lUVElement->GetDirectArray().GetAt(lUVIndex);

									uvs.push_back(lUVValue);

									lPolyIndexCounter++;
								}
							} // if (lPolyIndexCounter < lIndexCount)
						} // lVertIndex
					} // lPolyIndex
				}
			}
		}

		if (uvs.size() > 0)
		{
			int vertexCounter = 0;

			for (int i = 0; i != lMesh->GetPolygonCount(); ++i) {
				int polygonSize = lMesh->GetPolygonSize(i);

				for (int j = 0; j != polygonSize; ++j) {
					int vertexIndex = lMesh->GetPolygonVertex(i, j);
					meshData.Vertices[vertexIndex].TexC.x = (float)uvs[vertexCounter][0];
					meshData.Vertices[vertexIndex].TexC.y = 1.0f - (float)uvs[vertexCounter][1];

					++vertexCounter;
				}
			}
		}
	}

	lMesh->Destroy();

	return 0;
}

const UINT kMaxThreadCount = 8;

// Animation Thread Resource
DWORD hLoadAnimThreadID[kMaxThreadCount];
HANDLE hLoadAnimThread[kMaxThreadCount];

HANDLE hLoadAnimReadEvent[kMaxThreadCount];
HANDLE hLoadAnimWriteEvent[kMaxThreadCount];

static float mFrame;

FbxMesh* mStaticMesh;

float** mStaticVertexArrays[kMaxThreadCount];

UINT mStaticVertexCount;

UINT ClipStartTime[kMaxThreadCount];
UINT ClipEndTime[kMaxThreadCount];

// animation * cluster<Matrix>
std::vector<fbxsdk::FbxAMatrix> lVertexTransformMatrixs[kMaxThreadCount];

static bool mStopLoadAnimationThread = false;

DWORD WINAPI LoadAnimationThread(LPVOID prc)
{
	UINT mThreadIDX = (UINT)prc;

	UINT i, j, k;
	float mTime = 0.0f;

	UINT lClusterCount;
	UINT lClusterIndex = 0;
	FbxCluster* lCluster = nullptr;

	UINT animFrame = 0;

	fbxsdk::FbxAMatrix* lClusterDeformation;
	lClusterDeformation = new FbxAMatrix[mStaticVertexCount];

	UINT lVertexIndexCount;
	int lIndex = 0;
	double lWeight = 0.0;

	FbxSkin* lSkinDeformer = (FbxSkin*)mStaticMesh->GetDeformer(0, FbxDeformer::eSkin);

	// 클러스터 (deform) 개수를 얻어옴
	lClusterCount = lSkinDeformer->GetClusterCount();

	fbxsdk::FbxAMatrix lVertexTransformMatrix;

	fbxsdk::FbxVector4 x;
	float*	mDatas;

	int*	mIndices;
	double* mWeights;

	fbxsdk::FbxVector4* iter = mStaticMesh->GetControlPoints();

	UINT mAnimationIndex = 0;
	UINT mClusterIndexByAnimation = 0;

	while (true)
	{
		SetEvent(hLoadAnimReadEvent[mThreadIDX]);
		WaitForSingleObject(hLoadAnimWriteEvent[mThreadIDX], INFINITE);

		if (mStopLoadAnimationThread)
			break;

		mAnimationIndex = 0;
		mClusterIndexByAnimation = 0;

		for (animFrame = ClipStartTime[mThreadIDX]; animFrame < ClipEndTime[mThreadIDX]; animFrame++)
		{
			memset(lClusterDeformation, 0, mStaticVertexCount * sizeof(fbxsdk::FbxAMatrix));

			for (lClusterIndex = 0; lClusterIndex < lClusterCount; ++lClusterIndex)
			{
				lCluster = lSkinDeformer->GetCluster(lClusterIndex);

				lVertexTransformMatrix = lVertexTransformMatrixs[mThreadIDX][mClusterIndexByAnimation++];

				lVertexIndexCount = lCluster->GetControlPointIndicesCount();

				mIndices = lCluster->GetControlPointIndices();
				mWeights = lCluster->GetControlPointWeights();

				for (k = 0; k < lVertexIndexCount; ++k)
				{
					lIndex = mIndices[k];
					lWeight = mWeights[k];

					if (lWeight == 1.0)
					{
						lClusterDeformation[lIndex] = lVertexTransformMatrix;
					}
					else
					{
						for (i = 0; i < 4; i++)
							for (j = 0; j < 4; j++)
								lClusterDeformation[lIndex][i][j] += lVertexTransformMatrix[i][j] * lWeight;
					}
				}//For each vertex
			}//lClusterCount

			mDatas = mStaticVertexArrays[mThreadIDX][mAnimationIndex];

			for (i = 0; i < mStaticVertexCount; i++)
			{
				// 현재 버텍스에 animation frame Matrix를 곱하여 다음 애니메이션 포즈에 대응하는 위치로
				// 버텍스가 변형됨.
				x = (lClusterDeformation[i].MultT(iter[i]));

				mDatas[i * 4]		= (float)x.Buffer()[0];
				mDatas[i * 4 + 1]	= (float)x.Buffer()[1];
				mDatas[i * 4 + 2]	= (float)x.Buffer()[2];
				mDatas[i * 4 + 3]	= (float)x.Buffer()[3];
			}

			mAnimationIndex++;
		}

		ResetEvent(hLoadAnimWriteEvent[mThreadIDX]);
	}

	delete[](lClusterDeformation);

	ResetEvent(hLoadAnimWriteEvent[mThreadIDX]);
	SetEvent(hLoadAnimReadEvent[mThreadIDX]);
	return (DWORD)(0);
}

void DrawMesh(
	FbxNode* pNode,
	GeometryGenerator::MeshData& meshData,
	std::vector<FbxTime> mStarts,
	std::vector<FbxTime> mStops,
	fbxsdk::FbxAMatrix& pGlobalPosition,
	std::vector<std::vector<float*>>& lVertexArrays,
	std::vector<FbxUInt>& mAnimVertexSizes,
	AnimationClip& mAnimClips
)
{
	FbxMesh* lMesh = pNode->GetMesh();

	// Surface에서 Material 추출. (GetSrcObject<FbxSurfaceMaterial>)
	FbxSurfaceMaterial* smat = nullptr;
	// Mesh 속성 
	FbxProperty prop; 
	FbxLayeredTexture* layered_texture = nullptr;
	FbxFileTexture* fTexture = nullptr;

	// Get a FbxSurfaceMaterials Count
	int mcount = -1;
	// Get a Layer Count
	int lcount = -1;
	const char* file_texture_name = nullptr;

	const int vertexCount = lMesh->GetControlPointsCount();

	if (!vertexCount)
		return;

	// Create Mesh
	FbxVector4* lVertexArray = NULL;

	/*long long halfFrame = mStops[0].GetFrameCount() / 2;*/
	long long halfFrame = mStops[0].GetFrameCount();

	const bool hasShape = lMesh->GetShapeCount() > 0;
	const bool hasSkin = lMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
	const bool hasDeformation = hasShape || hasSkin;

	if (!hasDeformation)
		return;

	// Find Texture Path
	mcount = pNode->GetSrcObjectCount<FbxSurfaceMaterial>();

	int i = 0;
	int j = 0;
	int k = 0;
	int layered_texture_count;
	int texture_count;

	for (i = 0; i < mcount; i++)
	{
		smat = (FbxSurfaceMaterial*)pNode->GetSrcObject<FbxSurfaceMaterial>(i);

		if (smat) {
			prop = smat->FindProperty(FbxSurfaceMaterial::sDiffuse);

			layered_texture_count = prop.GetSrcObjectCount<FbxLayeredTexture>();
			if (layered_texture_count > 0)
			{
				for (j = 0; j < layered_texture_count; j++)
				{
					layered_texture = FbxCast<FbxLayeredTexture>(prop.GetSrcObject<FbxLayeredTexture>(j));
					lcount = layered_texture->GetSrcObjectCount<FbxFileTexture>();
					for (k = 0; k < lcount; k++)
					{
						fTexture = FbxCast<FbxFileTexture>(layered_texture->GetSrcObject<FbxFileTexture>(k));
						file_texture_name = fTexture->GetFileName();
					}
				}
			}
			else
			{
				texture_count = prop.GetSrcObjectCount<FbxFileTexture>();
				for (j = 0; j < texture_count; j++)
				{
					fTexture = FbxCast<FbxFileTexture>(prop.GetSrcObject<FbxFileTexture>(j));
					file_texture_name = fTexture->GetFileName();
				}
			}
		}
	}

	// Get a Textures Path Name
	if (file_texture_name) {
		meshData.texPath = std::string(file_texture_name);
	}
	else {
		meshData.texPath = "";
	}

	// GetControlPointVertex
	meshData.Vertices.resize(vertexCount);

	for (i = 0; i < vertexCount; i++) {
		meshData.Vertices[i].Position =
		{
			static_cast<float>(lMesh->GetControlPoints()[i][0]),
			static_cast<float>(lMesh->GetControlPoints()[i][1]),
			static_cast<float>(lMesh->GetControlPoints()[i][2])
		};
		meshData.Vertices[i].Normal =
		{
			static_cast<float>(lMesh->GetElementNormal()->GetDirectArray().GetAt(i)[0]),
			static_cast<float>(lMesh->GetElementNormal()->GetDirectArray().GetAt(i)[1]),
			static_cast<float>(lMesh->GetElementNormal()->GetDirectArray().GetAt(i)[2])
		};
	}

	if (lMesh->GetElementTangentCount()) {
		for (int i = 0; i < vertexCount; i++) {
			meshData.Vertices[i].TangentU =
			{
				static_cast<float>(lMesh->GetElementTangent()->GetDirectArray().GetAt(i)[0]),
				static_cast<float>(lMesh->GetElementTangent()->GetDirectArray().GetAt(i)[1]),
				static_cast<float>(lMesh->GetElementTangent()->GetDirectArray().GetAt(i)[2])
			};
		}
	}
	else {
		for (i = 0; i < vertexCount; i++) {
			meshData.Vertices[i].TangentU = { 0, 0, 0 };
		}
	}

	// GetControlPointVertexIndices
	unsigned int triCount = lMesh->GetPolygonCount();
	unsigned int vertexIndex = 0;

	std::vector<int> pre;

	uint32_t numIndices = 1;
	uint32_t polygonVertexSize = 0;

	for (UINT i = 0; i < triCount; ++i)
	{
		polygonVertexSize = lMesh->GetPolygonSize(i) - 2;
		numIndices += polygonVertexSize * 3;
		;
	}

	// Index Count
	uint32_t numIndex = -1;
	meshData.Indices32.resize(numIndices);

	for (UINT i = 0; i < triCount; ++i)
	{
		polygonVertexSize = lMesh->GetPolygonSize(i);

		pre.resize(polygonVertexSize);
		for (UINT j = 0; j < polygonVertexSize; ++j)
			pre[j] = lMesh->GetPolygonVertex(i, j);

		for (UINT indicesIDX = 0; indicesIDX < polygonVertexSize - 2; indicesIDX++) {
			meshData.Indices32[++numIndex] = pre[0];
			meshData.Indices32[++numIndex] = pre[indicesIDX + 1];
			meshData.Indices32[++numIndex] = pre[indicesIDX + 2];
		}
	}

	pre.clear();

	std::vector<fbxsdk::FbxVector2> uvs;
	{
		//get all UV set names
		FbxStringList lUVSetNameList;
		lMesh->GetUVSetNames(lUVSetNameList);

		//iterating over all uv sets
		for (int lUVSetIndex = 0; lUVSetIndex < lUVSetNameList.GetCount(); lUVSetIndex++)
		{
			//get lUVSetIndex-th uv set
			const char* lUVSetName = lUVSetNameList.GetStringAt(lUVSetIndex);
			const FbxGeometryElementUV* lUVElement = lMesh->GetElementUV(lUVSetName);

			if (!lUVElement)
				continue;

			//index array, where holds the index referenced to the uv data
			const bool lUseIndex = lUVElement->GetReferenceMode() != FbxGeometryElement::eDirect;
			const int lIndexCount = (lUseIndex) ? lUVElement->GetIndexArray().GetCount() : 0;

			//iterating through the data by polygon
			const int lPolyCount = lMesh->GetPolygonCount();

			if (lUVElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
			{
				for (int lPolyIndex = 0; lPolyIndex < lPolyCount; ++lPolyIndex)
				{
					// build the max index array that we need to pass into MakePoly
					const int lPolySize = lMesh->GetPolygonSize(lPolyIndex);
					for (int lVertIndex = 0; lVertIndex < lPolySize; ++lVertIndex)
					{
						FbxVector2 lUVValue;
						//get the index of the current vertex in control points array
						int lPolyVertIndex = lMesh->GetPolygonVertex(lPolyIndex, lVertIndex);

						//the UV index depends on the reference mode
						int lUVIndex = lUseIndex ? lUVElement->GetIndexArray().GetAt(lPolyVertIndex) : lPolyVertIndex;

						lUVValue = lUVElement->GetDirectArray().GetAt(lUVIndex);

						uvs.push_back(lUVValue);
					}
				}
			}
			else if (lUVElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
			{
				int vertexCounter = 0;
				int lPolyIndexCounter = 0;

				int lUVIndex;
				FbxVector2 lUVValue;

				for (int lPolyIndex = 0; lPolyIndex < lPolyCount; ++lPolyIndex)
				{
					// build the max index array that we need to pass into MakePoly
					const int lPolySize = lMesh->GetPolygonSize(lPolyIndex);
					for (int lVertIndex = 0; lVertIndex < lPolySize; ++lVertIndex)
					{
						if (lPolyIndexCounter < lIndexCount)
						{
							//the UV index depends on the reference mode
							lUVIndex =
								lUseIndex ?
								lUVElement->GetIndexArray().GetAt(lPolyIndexCounter) :
								lPolyIndexCounter;

							lUVValue = lUVElement->GetDirectArray().GetAt(lUVIndex);

							uvs.push_back(lUVValue);

							lPolyIndexCounter++;

						} // if (lPolyIndexCounter < lIndexCount)
					} // lVertIndex
				} // lPolyIndex
			}
		}
	}

	if (uvs.size() > 0)
	{
		int vertexCounter = 0;
		int polygonSize = 0;
		int vertexIndex = 0;


		for (i = 0; i != lMesh->GetPolygonCount(); ++i) {
			polygonSize = lMesh->GetPolygonSize(i);

			for (j = 0; j != polygonSize; ++j) {
				vertexIndex = lMesh->GetPolygonVertex(i, j);
				meshData.Vertices[vertexIndex].TexC.x = (float)uvs[vertexCounter][0];
				meshData.Vertices[vertexIndex].TexC.y = 1.0f - (float)uvs[vertexCounter][1];

				++vertexCounter;
			}
		}
	}

	lVertexArray = new FbxVector4[vertexCount];
	FbxTime mTime = 0;

	//we need to get the number of clusters
	const int lSkinCount = lMesh->GetDeformerCount(FbxDeformer::eSkin);
	int lClusterCount = 0;
	int lSkinIndex = 0;

	for (lSkinIndex = 0; lSkinIndex < lSkinCount; ++lSkinIndex)
	{
		lClusterCount +=
			((FbxSkin*)(lMesh->GetDeformer(lSkinIndex, FbxDeformer::eSkin)))->GetClusterCount();
	}

	FbxSkin* lSkinDeformer = (FbxSkin*)lMesh->GetDeformer(0, FbxDeformer::eSkin);
	FbxSkin::EType lSkinningType = lSkinDeformer->GetSkinningType();

	// Cluster(뼈대) 개수
	lClusterCount = lSkinDeformer->GetClusterCount();

	// For all skins and all clusters, accumulate their deformation and weight
	// on each vertices and store them in lClusterDeformation and lClusterWeight.
	int lClusterIndex = 0;
	FbxCluster* lCluster = nullptr;

	fbxsdk::FbxAMatrix lVertexTransformMatrix;
	fbxsdk::FbxAMatrix lClusterRelativeInitPosition;

	int lIndex = 0;
	double lWeight = 0.0;
	fbxsdk::FbxAMatrix lInfluence;

	fbxsdk::FbxAMatrix lReferenceGlobalInitPosition;
	fbxsdk::FbxAMatrix lReferenceGlobalCurrentPosition = pGlobalPosition.Inverse();
	fbxsdk::FbxAMatrix lAssociateGlobalInitPosition;
	fbxsdk::FbxAMatrix lAssociateGlobalCurrentPosition;
	fbxsdk::FbxAMatrix lClusterGlobalInitPosition;
	fbxsdk::FbxAMatrix lClusterGlobalCurrentPosition;

	fbxsdk::FbxAMatrix lReferenceGeometry;
	fbxsdk::FbxAMatrix lAssociateGeometry;
	fbxsdk::FbxAMatrix lClusterGeometry;

	fbxsdk::FbxAMatrix lClusterRelativeCurrentPositionInverse;

	// All the links must have the same link mode.
	FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)lMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

	int lVertexCount = lMesh->GetControlPointsCount();

	fbxsdk::FbxAMatrix* lClusterDeformation = new FbxAMatrix[lVertexCount];
	//double* lClusterWeight			= new double[lVertexCount];

	bool passCondition =
		lClusterCount &&
		lSkinDeformer &&
		(lSkinningType == FbxSkin::eLinear ||
			lSkinningType == FbxSkin::eRigid);

	mStaticMesh = lMesh;
	mStaticVertexCount = lVertexCount;
	pNode = lMesh->GetNode();

	lReferenceGeometry = fbxsdk::FbxAMatrix(
		pNode->GetGeometricTranslation(FbxNode::eSourcePivot),
		pNode->GetGeometricRotation(FbxNode::eSourcePivot),
		pNode->GetGeometricScaling(FbxNode::eSourcePivot)
	);

	int* mIndexPtr = nullptr;
	double* mWeightPtr = nullptr;

	for (int threadIDX = 0; threadIDX < kMaxThreadCount; threadIDX++)
	{
		hLoadAnimReadEvent[threadIDX] = CreateEvent(nullptr, false, false, nullptr);
		hLoadAnimWriteEvent[threadIDX] = CreateEvent(nullptr, false, false, nullptr);

		hLoadAnimThread[threadIDX] = CreateThread(
			NULL,
			0,
			LoadAnimationThread,
			(LPVOID*)threadIDX,
			0,
			&hLoadAnimThreadID[threadIDX]
		);
	}

	mFrame = ((mStops[0].Get() - mStarts[0].Get()) / (float)halfFrame);

	std::string mStreamName = "Animation\\" + mStaticName + mStaticNumber + ".anim";

	std::ifstream isExistClusterFile(mStreamName, std::ifstream::binary);
	if (!isExistClusterFile.good())
	{
		std::ofstream mCreateClusterFile(mStreamName, std::ios::binary);

		std::vector<fbxsdk::FbxAMatrix> lClusterRelativeInitPositions(lClusterCount);
		fbxsdk::FbxAMatrix res;

		for (lClusterIndex = 0; lClusterIndex < lClusterCount; ++lClusterIndex) {
			// 클러스터 로드
			lCluster = lSkinDeformer->GetCluster(lClusterIndex);
			// 현 클러스터에서 글로벌 변형 매트릭스를 로드
			lCluster->GetTransformMatrix(lReferenceGlobalInitPosition);

			lReferenceGlobalInitPosition *= lReferenceGeometry;

			// Get the link initial global position and the link current global position.
			// 초기 T 포즈에서의 Position과 현재 포즈에서의 Position을 이어주는 Link Matrix를 제공 
			lCluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);

			// Compute the initial position of the link relative to the reference.
			// 초기 포즈에서 모든 뼈대를 원점으로 만들어주는 변형 행렬을 만듦
			lClusterRelativeInitPositions[lClusterIndex] =
				lClusterGlobalInitPosition.Inverse() *
				lReferenceGlobalInitPosition;
		}

		if (mAnimClips.mClips.size() == 0)
		{
			for (int animFrame = 0; animFrame < (int)halfFrame; animFrame++) {
				mTime = mFrame * animFrame;

				for (lClusterIndex = 0; lClusterIndex < lClusterCount; ++lClusterIndex) {
					// 클러스터 로드
					lCluster = lSkinDeformer->GetCluster(lClusterIndex);

					lClusterGlobalCurrentPosition = lCluster->GetLink()->EvaluateGlobalTransform(mTime);

					// Compute the current position of the link relative to the reference.
					// mTime 프레임 에서의 Pose를 만드는 Matrix
					lClusterRelativeCurrentPositionInverse =
						lReferenceGlobalCurrentPosition *
						lClusterGlobalCurrentPosition;

					// Compute the shift of the link relative to the reference.
					// 결과적으로 T포즈에서 mTime 프레임 에서의 Pose로 한번에 변형되는 Matrix를 계산
					res =
						pGlobalPosition *
						lClusterRelativeCurrentPositionInverse *
						lClusterRelativeInitPositions[lClusterIndex];

					mCreateClusterFile.write((char*)&res, sizeof(double) * 16);
				}
			}
		}
		else
		{
			for (int clipIDX = 0; clipIDX < mAnimClips.mClips.size(); clipIDX++)
			{
				for (int animFrame = mAnimClips.mClips[clipIDX].mStartTime;
					animFrame < mAnimClips.mClips[clipIDX].mEndTime;
					animFrame++
					) {
					mTime = mFrame * animFrame;

					for (lClusterIndex = 0; lClusterIndex < lClusterCount; ++lClusterIndex) {
						// 클러스터 로드
						lCluster = lSkinDeformer->GetCluster(lClusterIndex);
						lClusterGlobalCurrentPosition = lCluster->GetLink()->EvaluateGlobalTransform(mTime);

						// Compute the current position of the link relative to the reference.
						// mTime 프레임 에서의 Pose를 만드는 Matrix
						lClusterRelativeCurrentPositionInverse =
							lReferenceGlobalCurrentPosition *
							lClusterGlobalCurrentPosition;

						// Compute the shift of the link relative to the reference.
						// 결과적으로 T포즈에서 mTime 프레임 에서의 Pose로 한번에 변형되는 Matrix를 계산
						res =
							pGlobalPosition *
							lClusterRelativeCurrentPositionInverse *
							lClusterRelativeInitPositions[lClusterIndex];

						mCreateClusterFile.write((char*)&res, sizeof(double) * 16);
					}
				}
			}
		}

		lClusterRelativeInitPositions.clear();
		mCreateClusterFile.close();
	}
	isExistClusterFile.close();

	std::ifstream mClusterFile(mStreamName, std::ifstream::binary);
	std::vector<float*> mVertexArray;

	// 현 Node에 Cluster(뼈대)가 존재하는지, Deform이 존재하는지의 여부가 true일 때
	if (passCondition)
	{
		mAnimVertexSizes.push_back(vertexCount);

		if (mAnimClips.mClips.size() == 0)
		{
			mVertexArray.resize(halfFrame);

			for (int animFrame = 0; animFrame < halfFrame; animFrame++)
				mVertexArray[animFrame] = new float[vertexCount * 4];

			UINT mTaskCountByThread = (halfFrame / kMaxThreadCount);
			for (int mThreadIDX = 0; mThreadIDX < kMaxThreadCount; mThreadIDX++)
			{
				ClipStartTime[mThreadIDX] = (int)mTaskCountByThread * (mThreadIDX);
				ClipEndTime[mThreadIDX] = ((mThreadIDX + 1) < kMaxThreadCount) ? 
					(int)mTaskCountByThread * (mThreadIDX + 1) : (halfFrame);

				mStaticVertexArrays[mThreadIDX] = &mVertexArray[ClipStartTime[mThreadIDX]];

				UINT mClipFrame = ClipEndTime[mThreadIDX] - ClipStartTime[mThreadIDX];
				lVertexTransformMatrixs[mThreadIDX].resize(mClipFrame * lClusterCount);

				mClusterFile.read((char*)lVertexTransformMatrixs[mThreadIDX].data(), sizeof(double) * 16 * mClipFrame * lClusterCount);

				SetEvent(hLoadAnimWriteEvent[mThreadIDX]);
				ResetEvent(hLoadAnimReadEvent[mThreadIDX]);
			}

			WaitForMultipleObjects(kMaxThreadCount, hLoadAnimReadEvent, true, INFINITE);
		}
		else
		{
			UINT mTotalFrameSize = 0;
			for (int clipIDX = 0; clipIDX < mAnimClips.mClips.size(); clipIDX++)
			{
				mTotalFrameSize +=
					(UINT)(mAnimClips.mClips[clipIDX].mEndTime -
						mAnimClips.mClips[clipIDX].mStartTime);
			}

			mVertexArray.resize(mTotalFrameSize);
			for (UINT animFrame = 0; animFrame < mTotalFrameSize; animFrame++)
				mVertexArray[animFrame] = new float[vertexCount * 4];

			UINT mThreadCount = 0;
			UINT mClipSize = 0;
			UINT mFrameOffset = 0;
			for (int clipIDX = 0; clipIDX < mAnimClips.mClips.size(); clipIDX += kMaxThreadCount)
			{
				mThreadCount = ((int)mAnimClips.mClips.size() - clipIDX > kMaxThreadCount) ?
					kMaxThreadCount : (mAnimClips.mClips.size() - clipIDX);

				for (UINT mThreadIDX = 0; mThreadIDX < mThreadCount; mThreadIDX++)
				{
					if ((UINT)mAnimClips.mClips.size() <= (clipIDX + mThreadIDX)) 
						break;

					ClipStartTime[mThreadIDX] = (UINT)mAnimClips.mClips[clipIDX + mThreadIDX].mStartTime;
					ClipEndTime[mThreadIDX] = (UINT)mAnimClips.mClips[clipIDX + mThreadIDX].mEndTime;

					mStaticVertexArrays[mThreadIDX] = &(mVertexArray[mFrameOffset]);
					mClipSize = (UINT)(ClipEndTime[mThreadIDX] - ClipStartTime[mThreadIDX]);
					mFrameOffset += mClipSize;

					lVertexTransformMatrixs[mThreadIDX].resize(mClipSize * lClusterCount);

					mClusterFile.read((char*)lVertexTransformMatrixs[mThreadIDX].data(), sizeof(double) * 16 * mClipSize * lClusterCount);

					SetEvent(hLoadAnimWriteEvent[mThreadIDX]);
					ResetEvent(hLoadAnimReadEvent[mThreadIDX]);
				}

				WaitForMultipleObjects(mThreadCount, hLoadAnimReadEvent, true, INFINITE);
			}
		}

		lVertexArrays.push_back(mVertexArray);

		//mStopLoadAnimationThread = true;
		//for (int mThreadIDX = 0; mThreadIDX < 4; mThreadIDX++)
		//{
		//	SetEvent(hLoadAnimWriteEvent[mThreadIDX]);
		//	ResetEvent(hLoadAnimReadEvent[mThreadIDX]);

		//	WaitForMultipleObjects(4, hLoadAnimReadEvent, true, INFINITE);
		//}

		delete[] lClusterDeformation;
		delete[] lVertexArray;
	}

	mClusterFile.close();
}

// Draw an oriented camera box where the node is located.
void DrawCamera(FbxNode* pNode,
	FbxTime& pTime,
	FbxAnimLayer* pAnimLayer,
	FbxAMatrix& pGlobalPosition)
{
	FbxAMatrix lCameraGlobalPosition;
	FbxVector4 lCameraPosition, lCameraDefaultDirection, lCameraInterestPosition;

	lCameraPosition = pGlobalPosition.GetT();

	// By default, FBX cameras point towards the X positive axis.
	FbxVector4 lXPositiveAxis(1.0, 0.0, 0.0);
	lCameraDefaultDirection = lCameraPosition + lXPositiveAxis;

	lCameraGlobalPosition = pGlobalPosition;

	// If the camera is linked to an interest, get the interest position.
	if (pNode->GetTarget())
	{
		lCameraInterestPosition = GetGlobalPosition(pNode->GetTarget(), pTime).GetT();

		// Compute the required rotation to make the camera point to it's interest.
		FbxVector4 lCameraDirection;
		FbxVector4::AxisAlignmentInEulerAngle(lCameraPosition,
			lCameraDefaultDirection,
			lCameraInterestPosition,
			lCameraDirection);

		// Must override the camera rotation 
		// to make it point to it's interest.
		lCameraGlobalPosition.SetR(lCameraDirection);
	}

	// Get the camera roll.
	FbxCamera* cam = pNode->GetCamera();
	double lRoll = 0;

	if (cam)
	{
		lRoll = cam->Roll.Get();
		FbxAnimCurve* fc = cam->Roll.GetCurve(pAnimLayer);
		if (fc) fc->Evaluate(pTime);
	}
	// GlDrawCamera(lCameraGlobalPosition, lRoll);
}

void DrawNode(
	FbxNode* pNode,
	std::vector<GeometryGenerator::MeshData>& meshDatas,
	bool uvMode
) {
	FbxNodeAttribute* attr = pNode->GetNodeAttribute();

	if (attr)
	{
		if (attr->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			GeometryGenerator::MeshData meshData;

			int res = DrawMesh(
				pNode,
				meshData,
				uvMode
			);

			if (!res)
			{
				meshData.mName = std::string(pNode->GetName());
				meshDatas.push_back(meshData);
			}
		}
	}
}

void DrawNode(
	FbxNode* pNode,
	std::vector<FbxTime> mStarts,
	std::vector<FbxTime> mStops,
	FbxAMatrix& pGlobalPosition,
	std::vector<GeometryGenerator::MeshData>& meshDatas,
	std::vector<std::vector<float*>>& animVertexArrays,
	std::vector<FbxUInt>& mAnimVertexSizes,
	AnimationClip& mAnimClips
)
{
	FbxNodeAttribute* attr = pNode->GetNodeAttribute();

	if (attr)
	{
		if (attr->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			GeometryGenerator::MeshData meshData;

			std::vector<FbxAMatrix> lClusterDeformation;
			std::unordered_map<FbxUInt, std::vector<FbxUInt>> lClusterIndice;
			std::vector<double> lClusterWeight;

			mStaticNumber = std::to_string(meshDatas.size());

			DrawMesh(
				pNode,
				meshData,
				mStarts,
				mStops,
				pGlobalPosition,
				animVertexArrays,
				mAnimVertexSizes,
				mAnimClips
			);

			if (meshData.Vertices.size() > 0)
				meshDatas.push_back(meshData);
		}
	}
}

void DrawBone(
	FbxNode* pNode,
	std::vector<FbxTime> mStops,
	FbxAMatrix& pOriginGlobalPosition,
	FbxAMatrix& pParentGlobalPosition,
	long long perFrame,
	std::ofstream& outFile
)
{
	std::string boneName(pNode->GetName());

	if (boneName == "Shoulder.L" || boneName == "Shoulder.R")
	{
		printf("");
	}

	try {
		int transSkeletonPairSize = transSkeletonPairs.at(boneName).size();
		if (transSkeletonPairSize == 0)
			return;
	}
	catch (std::exception&) {
		return;
	}

	FbxAMatrix global, local, finalM;
	// Default Position
	FbxVector4 localT, localS, deltaT;
	FbxVector4 localR;
	FbxQuaternion localQ;

	global = pOriginGlobalPosition;

	// 현재 뼈대 이름을 key로 갖는 values의 개수를 로드
	int transSkeletonPairsSize = transSkeletonPairs.at(boneName).size();

	outFile.write((char*)&transSkeletonPairsSize, sizeof(int));

	// 현재 뼈대 이름을 key로 갖는 value들의 이름들을 리스트에 적는다.
	for (int i = 0; i < transSkeletonPairsSize; i++)
	{
		outFile.write((const char*)transSkeletonPairs.at(boneName)[i].c_str(), transSkeletonPairs.at(boneName)[i].size());
		outFile.write("\n", 1);
	}

	int frameCount = mStops[0].GetFrameCount(FbxTime::eFrames30);

	FbxVector4 reS;
	reS.mData[0] = 0.1f;
	reS.mData[1] = 0.1f;
	reS.mData[2] = 0.1f;
	reS.mData[3] = 1.0f;

	float convBuf;
	for (int animFrame = 0; animFrame < frameCount; animFrame++)
	{
		// FRAME COUNT 만큼 반복
		local =
			GetGlobalPosition(
				pNode,
				animFrame * perFrame,
				NULL,
				&global
			);

		//////////////////////////
		finalM = local;

		// 로테이션이 초기화 됨.
		// 로테이션이 잘못된 것 같음 고쳐볼 것
		//finalM.SetR(localR);
		finalM.SetS(reS);

		localT = finalM.GetT();
		localQ = finalM.GetQ();
		localS = finalM.GetS();

		localT = localS * localT;

		//////////////////////////

		if (boneName == "Shoulder.L" || boneName == "Shoulder.R")
		{
			localT.mData[0] += 3000000.0f;
		}
		convBuf = (float)localT.mData[0];
		outFile.write((char*)&convBuf, sizeof(float));
		convBuf = (float)localT.mData[1];
		outFile.write((char*)&convBuf, sizeof(float));
		convBuf = (float)localT.mData[2];
		outFile.write((char*)&convBuf, sizeof(float));
		convBuf = 1.0f;
		outFile.write((char*)&convBuf, sizeof(float));

		convBuf = (float)localQ.mData[0];
		outFile.write((char*)&convBuf, sizeof(float));
		convBuf = (float)localQ.mData[1];
		outFile.write((char*)&convBuf, sizeof(float));
		convBuf = (float)localQ.mData[2];
		outFile.write((char*)&convBuf, sizeof(float));
		convBuf = (float)localQ.mData[3];
		outFile.write((char*)&convBuf, sizeof(float));
	}
}