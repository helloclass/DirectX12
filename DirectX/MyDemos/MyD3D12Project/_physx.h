#pragma once

//#define PX_PHYSX_STATIC_LIB
//#define PX_FOUNDATION_DLL 0
//#include "PxPhysics.h"
//#include "PxPhysicsAPI.h"
//#include "foundation/PxFoundation.h"

#ifndef PHYSX_SAMPLE_H
#define PHYSX_SAMPLE_H

#include <iostream>
#include <PxPhysicsAPI.h>
#include <extensions/PxExtensionsAPI.h>
#include <extensions/PxDefaultErrorCallback.h>
//#include <extensions/PxDefaultAllocator.h>
//#include <extensions/PxDefaultSimulationFilterShader.h>
//#include <extensions/PxDefaultCpuDispatcher.h>
//#include <extensions/PxShapeExt.h>
//#include <extensions/PxSimpleFactory.h>

//#include <PxSimulationEventCallback.h>

//#include <foundation/PxFoundation.h>

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

struct PhyxResource {
	float Position[3];
	float Rotation[3];
	float Quaternion[4];
	float Scale[3];
};

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
	std::vector<struct PhyxResource*> bindObj;
	std::vector<PxRigidDynamic*> Colliders;

public:
	void Init();
	void CleanUp();

	void Update();

	int BindObjColliber(PxRigidDynamic*, struct PhyxResource*);
	PhyxResource* getTranspose(int idx);

	PxShape* CreateBox(float x, float y, float z);
	PxShape* CreateSphere(float r);
	PxShape* CreateCapsule(float r);
	PxShape* CreateConvexMeshToShape(const PxConvexMeshDesc& convexDesc);
	PxShape* CreateTriangleMeshToShape(const PxTriangleMeshDesc& meshDesc);
	PxConvexMesh* CreateConvexMesh(const PxConvexMeshDesc& convexDesc);
	PxTriangleMesh* CreateTriangleMesh(const PxTriangleMeshDesc& meshDesc);

	void CreateStack(PxShape* shape, const PxTransform&, PxU32, PxReal);

	// 고정되어 있는 물체 (건축 양식)
	PxRigidStatic* CreateStatic(const PxTransform& t, PxShape* shape);
	// 같은 Kinenmatic과 Static을 제외한 Dynamic과만 충돌이 가능, 움직임, 관성등은 프로그래머가 집적 조작 해야 함.
	PxRigidDynamic* CreateKinematic(const PxTransform& t, PxShape* shape, float density, const PxVec3& velocity = PxVec3(0));
	// 모든 오브젝트와 충돌 가능, 움직임 가능
	PxRigidDynamic* CreateDynamic(const PxTransform&, const PxGeometry&, const PxVec3& = PxVec3(0));

	void freeGeometry(PxShape* shape);

public:
	void setPosition(int idx, float x, float y, float z);
	void setRotation(int idx, float x, float y, float z);

	void setVelocity(int idx, float x, float y, float z);
	void setTorque(int idx, float x, float y, float z);

	PxCloth* LoadCloth(PxClothParticle* vertices, PxClothMeshDesc& meshDesc);
	bool RemoveCloth(PxCloth* cloth);
public:
	Physics() { this->Init(); }
	~Physics() { 
		this->CleanUp();
		// clothes 할당 해제
	}
};