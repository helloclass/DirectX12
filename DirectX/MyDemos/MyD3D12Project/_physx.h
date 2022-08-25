#pragma once

#ifndef PHYSX_SAMPLE_H
#define PHYSX_SAMPLE_H

#include <iostream>
#include <PxPhysicsAPI.h>
#include <extensions/PxExtensionsAPI.h>
#include <extensions/PxDefaultErrorCallback.h>

#include <cloth/PxCloth.h>
#include <cloth/PxClothCollisionData.h>
#include <cloth/PxClothFabric.h>
#include <cloth/PxClothParticleData.h>
#include <cloth/PxClothTypes.h>

#include <assert.h>
#include <vector>

using namespace physx;

#endif // PHYSX_SAMPLE_H

#if PX_SUPPORT_GPU_PHYSX
static PxClothFlag::Enum gGpuFlag = PxClothFlag::eCUDA;
#elif PX_XBOXONE
static PxClothFlag::Enum gGpuFlag = PxClothFlag::eDEFAULT;
#endif

//struct PhysResource {
//public:
//	float Position[3]	= {0.0f, 0.0f, 0.0f};
//	float Rotation[3]	= { 0.0f, 0.0f, 0.0f };
//	float Quaternion[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
//	float Scale[3]		= { 1.0f, 1.0f, 1.0f };
//};

class Physics {
private:
	PxDefaultAllocator		gAllocator;
	PxDefaultErrorCallback	gErrorCallback;
	PxDefaultCpuDispatcher* gDispatcher = NULL;
	PxTolerancesScale		gToleranceScale;
	PxCooking*				gCooking = NULL;
	PxFoundation*			gFoundation = NULL;
	PxPhysics*				gPhysics = NULL;
	PxScene*				gScene;
	PxMaterial*				gMaterial = NULL;
	PxPvd*					gPvd = NULL;

public:
	std::vector<struct PhysResource*> bindObj;
	std::vector<PxRigidStatic*>  StaticColliders;
	std::vector<PxRigidDynamic*> DynamicColliders;

public:
	void Init();
	void CleanUp();

	void Update();

	void BindObjColliber(_In_ PxRigidStatic*, _In_  struct PhysResource*);
	void BindObjColliber(_In_ PxRigidDynamic*, _In_  struct PhysResource*);

	PxShape* CreateBox(_In_ float x, _In_  float y, _In_  float z);
	PxShape* CreateSphere(_In_ float r);
	PxShape* CreateCapsule(_In_ float r);
	PxShape* CreateConvexMeshToShape(_In_ const PxConvexMeshDesc& convexDesc);
	PxShape* CreateTriangleMeshToShape(_In_ const PxTriangleMeshDesc& meshDesc);
	PxConvexMesh* CreateConvexMesh(_In_ const PxConvexMeshDesc& convexDesc);
	PxTriangleMesh* CreateTriangleMesh(_In_ const PxTriangleMeshDesc& meshDesc);

	void CreateStack(PxShape* shape, const PxTransform&, PxU32, PxReal);

	// 고정되어 있는 물체 (건축 양식)
	PxRigidStatic* CreateStatic(const PxTransform& t, PxShape* shape);
	// 같은 Kinenmatic과 Static을 제외한 Dynamic과만 충돌이 가능, 움직임, 관성등은 프로그래머가 집적 조작 해야 함.
	PxRigidDynamic* CreateKinematic(const PxTransform& t, PxShape* shape, float density, const PxVec3& velocity = PxVec3(0));
	// 모든 오브젝트와 충돌 가능, 움직임 가능
	PxRigidDynamic* CreateDynamic(const PxTransform&, const PxGeometry&, const PxVec3& = PxVec3(0));

	void freeGeometry(PxShape* shape);

public:
	void setPosition(PxRigidDynamic* collider, float x, float y, float z);
	void setRotation(PxRigidDynamic* collider, float x, float y, float z);

	void setVelocity(PxRigidDynamic* collider, float x, float y, float z);
	void setTorque(PxRigidDynamic* collider, float x, float y, float z);

	PxCloth* LoadCloth(PxClothParticle* vertices, PxClothMeshDesc& meshDesc);
	bool RemoveCloth(PxCloth* cloth);
public:
	Physics() { 
		this->Init();
	}
	~Physics() { 
		this->CleanUp();
		// clothes 할당 해제
	}
};