#pragma once

#include <cstdint>
#include <DirectXMath.h>
#include <set>
#include <unordered_map>
#include <array>
#include <string>
#include <filesystem>

#include "d3dUtil.h"
#include "../MyDemos/MyD3D12Project/Animation.h"

#pragma comment(lib, "libfbxsdk.lib")

#define KFBX_DLLINFO

static std::unordered_map<std::string, std::vector<std::string>> transSkeletonPairs;

class GeometryGenerator
{
public:
	GeometryGenerator() {}
	~GeometryGenerator() {
	}

    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;

	struct Vertex
	{
		Vertex(){}
        Vertex(
            const DirectX::XMFLOAT3& p, 
            const DirectX::XMFLOAT3& n, 
            const DirectX::XMFLOAT3& t, 
            const DirectX::XMFLOAT2& uv) :
            Position(p), 
            Normal(n), 
            TangentU(t), 
            TexC(uv){}
		Vertex(
			float px, float py, float pz, 
			float nx, float ny, float nz,
			float tx, float ty, float tz,
			float u, float v) : 
            Position(px,py,pz), 
            Normal(nx,ny,nz),
			TangentU(tx, ty, tz), 
            TexC(u,v){}

        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT3 TangentU;
        DirectX::XMFLOAT2 TexC;
	};

	struct MeshData
	{
		std::string mName;
		std::string texPath;

		std::vector<Vertex> Vertices;
        std::vector<uint32> Indices32;

        std::vector<uint16>& GetIndices16()
        {
			if(mIndices16.empty())
			{
				mIndices16.resize(Indices32.size());
				for(size_t i = 0; i < Indices32.size(); ++i)
					mIndices16[i] = static_cast<uint16>(Indices32[i]);
			}

			return mIndices16;
        }

	private:
		std::vector<uint16> mIndices16;
	};


    MeshData CreateBox(
		_In_ float width, 
		_In_ float height,
		_In_ float depth,
		_In_ uint32 numSubdivisions
	);
    MeshData CreateSphere(
		_In_ float radius,
		_In_ uint32 sliceCount,
		_In_ uint32 stackCount
	);
    MeshData CreateGeosphere(
		_In_ float radius,
		_In_ uint32 numSubdivisions
	);
    MeshData CreateCylinder(
		_In_ float bottomRadius,
		_In_ float topRadius,
		_In_ float height,
		_In_ uint32 sliceCount,
		_In_ uint32 stackCount
	);
    MeshData CreateGrid(
		_In_ float width,
		_In_ float depth,
		_In_ uint32 m,
		_In_ uint32 n
	);
    MeshData CreateQuad(
		_In_ float x,
		_In_ float y,
		_In_ float w,
		_In_ float h,
		_In_ float depth
	);

public:
	// 해당 아이템의 애니메이션 이름 리스트
	FbxArray<FbxString*> animNameLists;
	// 뼈대 루트 RTS 매트릭스
	FbxAMatrix pRootGlobalPosition;
	// 뼈대 중심축의 RTS 매트릭스 (전체 뼈대 메쉬의 Transform) 
	std::vector<FbxAMatrix> pParentGlobalPosition;
	FbxAnimLayer* currAnimLayer;
	// 각 애니메이션의 시작, 끝 시간
	std::vector<FbxTime> mStart, mStop;
	// 현재 애니메이션 타임
	FbxTime mCurrentTime;

public:
	int CreateFBXModel(
		std::vector<GeometryGenerator::MeshData>& meshData, 
		_In_ std::string Path,
		_In_ bool uvMode
	);
	int CreateFBXSkinnedModel(
		std::vector<GeometryGenerator::MeshData>& meshData,
		_In_ std::string mName,
		_In_ std::string Path,
		_Out_ FbxArray<FbxString*>& animNameLists,
		_Out_ std::vector<FbxTime>& mStarts,
		_Out_ std::vector<FbxTime>& mStops,
		_Out_ std::vector<long long>& countOfFrame,
		_Out_ std::vector<std::vector<float*>>& animVertexArrays,
		_Out_ std::vector<FbxUInt>& mAnimVertexSizes,
		_In_ AnimationClip& mAnimClips
	);

	int CreatePMXModel(
		_Out_ std::vector<GeometryGenerator::MeshData>& meshData,
		_In_ std::string Path,
		_Out_ std::vector<std::wstring>& texturePaths
	);

	int ExtractedAnimationBone(
		_In_ std::string Path,
		_In_ std::string targetPath,
		_Out_ FbxArray<FbxString*>& animNameLists,
		_Out_ std::vector<FbxTime>& mStarts,
		_Out_ std::vector<FbxTime>& mStops,
		_Out_ std::vector<long long>& countOfFrame
	);

private:
	void Subdivide(
		_Out_ MeshData& meshData
	);
    Vertex MidPoint(
		_In_ const Vertex& v0, 
		_In_ const Vertex& v1
	);
    void BuildCylinderTopCap(
		_In_ float bottomRadius,
		_In_ float topRadius,
		_In_ float height,
		_In_ uint32 sliceCount,
		_In_ uint32 stackCount,
		_Out_ MeshData& meshData
	);
    void BuildCylinderBottomCap(
		_In_ float bottomRadius,
		_In_ float topRadius,
		_In_ float height,
		_In_ uint32 sliceCount,
		_In_ uint32 stackCount,
		_Out_ MeshData& meshData
	);
};

FbxAMatrix GetPoseMatrix(FbxPose* pPose, int pNodeIndex);

FbxAMatrix GetGlobalPosition(
	FbxNode* pNode,
	const FbxTime& pTime,
	FbxPose* pPose = 0,
	FbxAMatrix* pParentGlobalPosition = 0
);

void DrawNodeRecursive(
	FbxNode* pNode,
	std::vector<GeometryGenerator::MeshData>& meshData,
	bool uvMode
);

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
);

void DrawBoneRecursive(
	FbxNode* pNode,
	std::vector<FbxTime> mStops,
	FbxTime& pTime,
	FbxAMatrix& pGlobalRootPositions,
	std::vector<FbxAMatrix>& pParentGlobalPositions,
	FbxPose* pPose,
	long long perFrame,
	std::ofstream& outFile
);

void DrawNode(
	FbxNode* pNode,
	std::vector<GeometryGenerator::MeshData>& meshDatas,
	bool uvMode
);

void DrawNode(
	FbxNode* pNode,
	std::vector<FbxTime> mStarts,
	std::vector<FbxTime> mStops,
	FbxAMatrix& pGlobalPosition,
	std::vector<GeometryGenerator::MeshData>& meshDatas,
	std::vector<std::vector<float*>>& animVertexArrays,
	std::vector<FbxUInt>& mAnimVertexSizes,
	AnimationClip& mAnimClips
);

void DrawBone(
	FbxNode* pNode,
	std::vector<FbxTime> mStops,
	FbxAMatrix& pOriginGlobalPosition,
	FbxAMatrix& pParentGlobalPosition,
	long long perFrame,
	std::ofstream& outFile
);

void ComputeShapeDeformation(
	FbxMesh* pMesh,
	FbxTime& pTime,
	FbxAnimLayer* pAnimLayer,
	FbxVector4* pVertexArray);
void ComputeClusterDeformation(
	FbxAMatrix& pGlobalPosition,
	FbxMesh* pMesh,
	FbxCluster* pCluster,
	FbxAMatrix& pVertexTransformMatrix,
	FbxTime pTime);
void ComputeLinearDeformation(
	FbxAMatrix& pGlobalPosition,
	FbxMesh* pMesh,
	FbxTime& pTime,
	FbxVector4* pVertexArray);

void DrawCamera(
	FbxNode* pNode, 
	FbxTime& pTime, 
	FbxAnimLayer* pAnimLayer, 
	FbxAMatrix& pGlobalPosition
);