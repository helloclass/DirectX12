#include "_physx.h"

void Physics::Init() {
	gFoundation = PxCreateFoundation(PX_FOUNDATION_VERSION, gAllocator, gErrorCallback);

	if (!gFoundation)
		throw std::runtime_error("Foundation is NULL!!");

	gPvd = PxCreatePvd(*gFoundation);
	PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
	gPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

	if (!gPvd)
		throw std::runtime_error("Pvd is NULL!!");

	gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true, gPvd);
	gCooking = PxCreateCooking(PX_PHYSICS_VERSION, *gFoundation, PxCookingParams(PxTolerancesScale()));

	if (!gPhysics)
		throw std::runtime_error("Physics is NULL!!");
	if (!gCooking)
		throw std::runtime_error("Cooking is NULL!!");

	if (!PxInitExtensions(*gPhysics, gPvd))
		throw std::runtime_error("PxInitExtensions Failed!!");

	//PxCudaContextManagerDesc cudaContextManagerDesc;

	//PxCudaContextManager* gCudaContextManager = PxCreateCudaContextManager(*gFoundation, cudaContextManagerDesc);

	PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	gDispatcher = PxDefaultCpuDispatcherCreate(4);
	sceneDesc.cpuDispatcher = gDispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;

	//sceneDesc.gpuDispatcher = gCudaContextManager->getGpuDispatcher();
	//sceneDesc.flags |= PxSceneFlag::eENABLE_GPU_DYNAMICS;
	//sceneDesc.broadPhaseType = PxBroadPhaseType::eGPU;

	gScene = gPhysics->createScene(sceneDesc);

	PxPvdSceneClient* pvdClient = gScene->getScenePvdClient();
	if (pvdClient)
	{
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}

	gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.5f);

	PxRigidStatic* groundPlane = PxCreatePlane(*gPhysics, PxPlane(0, 1, 0, 0), *gMaterial);
	gScene->addActor(*groundPlane);
}

void Physics::CleanUp() {
	{
		gScene->release();
		gFoundation->release();
		gMaterial->release();
		gDispatcher->release();
		gPhysics->release();
		gCooking->release();

		if (gPvd)
		{
			PxPvdTransport* transport = gPvd->getTransport();
			gPvd->release();
			transport->release();
		}

		for (int bindObjCnt = 0; bindObjCnt < bindObj.size(); bindObjCnt++)
			delete(bindObj[bindObjCnt]);

		for (int colliderCnt = 0; colliderCnt < Colliders.size(); colliderCnt++)
			Colliders[colliderCnt]->release();
	}
}

int Physics::BindObjColliber(PxRigidDynamic* obj, struct PhyxResource* res) {
	if (bindObj.size() != Colliders.size())
		throw std::runtime_error("사이즈가 다릅니다");

	// Bind Physx Resource
	bindObj.push_back(res);
	Colliders.push_back(obj);

	return (int)bindObj.size() - 1;
}

PhyxResource* Physics::getTranspose(int idx) {
	PxTransform tf = Colliders[idx]->getGlobalPose();

	bindObj[idx]->Position[0] = tf.p.x;
	bindObj[idx]->Position[1] = tf.p.y;
	bindObj[idx]->Position[2] = tf.p.z;

	bindObj[idx]->Quaternion[0] = tf.q.x;
	bindObj[idx]->Quaternion[1] = tf.q.y;
	bindObj[idx]->Quaternion[2] = tf.q.z;
	bindObj[idx]->Quaternion[3] = tf.q.w;

	return bindObj[idx];
}

void Physics::Update() {
	gScene->simulate(1.0f / 30.0f);
	gScene->fetchResults(true);
}

PxShape* Physics::CreateBox(float x, float y, float z) {
	PxShape* shape = gPhysics->createShape(PxBoxGeometry(x, y, z), *gMaterial);
	return shape;
}

PxShape* Physics::CreateSphere(float r) {
	PxShape* shape = gPhysics->createShape(PxSphereGeometry(r), *gMaterial);
	return shape;
}

PxConvexMesh* Physics::CreateConvexMesh(const PxConvexMeshDesc& convexDesc) {
	PxDefaultMemoryOutputStream buf;
	PxConvexMeshCookingResult::Enum result;
	if (!gCooking->cookConvexMesh(convexDesc, buf, &result))
		return NULL;
	PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
	PxConvexMesh* convexMesh = gPhysics->createConvexMesh(input);

	return convexMesh;
}

PxTriangleMesh* Physics::CreateTriangleMesh(const PxTriangleMeshDesc& meshDesc) {
	PxDefaultMemoryOutputStream writeBuffer;
	PxTriangleMeshCookingResult::Enum result;
	bool status = gCooking->cookTriangleMesh(meshDesc, writeBuffer, &result);

	PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());

	PxTriangleMesh* mesh = gPhysics->createTriangleMesh(readBuffer);
	return mesh;

	physx::PxMeshScale scale(physx::PxVec3(1.0f, 1.0f, -1.0f), physx::PxQuat(physx::PxIdentity));
	gPhysics->createShape(physx::PxTriangleMeshGeometry(mesh, scale), *gMaterial, true);
}

PxShape* Physics::CreateCapsule(float r) {
	PxRigidDynamic* aCapsuleActor = gPhysics->createRigidDynamic(PxTransform(0.0f, 0.0f, 0.0f));
	PxTransform relativePose(PxQuat(PxHalfPi, PxVec3(0, 0, 1)));
	PxShape* shape = PxRigidActorExt::createExclusiveShape(
		*aCapsuleActor,
		PxCapsuleGeometry(r, r),
		*gMaterial);
	return shape;
}

void Physics::CreateStack(PxShape* shape, const PxTransform& t, PxU32 size, PxReal halfExtent)
{
	for (PxU32 i = 0; i < size; i++)
	{
		for (PxU32 j = 0; j < size - i; j++)
		{
			PxTransform localTm(PxVec3(PxReal(j * 2) - PxReal(size - i), PxReal(i * 2 + 1), 0) * halfExtent);
			PxRigidDynamic* body = gPhysics->createRigidDynamic(t.transform(localTm));
			body->attachShape(*shape);
			PxRigidBodyExt::updateMassAndInertia(*body, 10.0f);
			gScene->addActor(*body);
		}
	}
	shape->release();
}

PxShape* Physics::CreateConvexMeshToShape(const PxConvexMeshDesc& convexDesc) {
	PxDefaultMemoryOutputStream buf;
	PxConvexMeshCookingResult::Enum result;
	if (!gCooking->cookConvexMesh(convexDesc, buf, &result))
		return NULL;
	PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
	PxConvexMesh* mesh = gPhysics->createConvexMesh(input);

	physx::PxMeshScale scale(physx::PxVec3(1.0f, 1.0f, -1.0f), physx::PxQuat(physx::PxIdentity));

	return gPhysics->createShape(physx::PxConvexMeshGeometry(mesh, scale), *gMaterial);
}

PxShape* Physics::CreateTriangleMeshToShape(const PxTriangleMeshDesc& meshDesc) {
	PxDefaultMemoryOutputStream writeBuffer;
	PxTriangleMeshCookingResult::Enum result;
	bool status = gCooking->cookTriangleMesh(meshDesc, writeBuffer, &result);

	PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());

	PxTriangleMesh* mesh = gPhysics->createTriangleMesh(readBuffer);

	physx::PxMeshScale scale(physx::PxVec3(1.0f, 1.0f, -1.0f), physx::PxQuat(physx::PxIdentity));
	physx::PxTriangleMeshGeometry meshGeom(mesh, scale);

	return gPhysics->createShape(meshGeom, *gMaterial);
}

PxRigidStatic* Physics::CreateStatic(const PxTransform& t, PxShape* shape)
{
	PxRigidStatic* staticMesh = PxCreateStatic(*gPhysics, t, *shape);

	gScene->addActor(*staticMesh);

	return staticMesh;
}

PxRigidDynamic* Physics::CreateKinematic(const PxTransform& t, PxShape* shape, float density, const PxVec3& velocity)
{
	PxRigidDynamic* kinematic = PxCreateKinematic(*gPhysics, t, *shape, density);
	if (!kinematic)
		throw std::runtime_error("Create Kinematic Failed..");

	kinematic->setLinearVelocity(velocity);
	gScene->addActor(*kinematic);

	return kinematic;
}

PxRigidDynamic* Physics::CreateDynamic(const PxTransform& t, const PxGeometry& geometry, const PxVec3& velocity)
{
	PxRigidDynamic* dynamic = PxCreateDynamic(*gPhysics, t, geometry, *gMaterial, 10.0f);

	dynamic->setLinearVelocity(velocity);
	gScene->addActor(*dynamic);

	return dynamic;
}

void Physics::freeGeometry(PxShape* shape) {
	shape->release();
}

void Physics::setPosition(int idx, float x, float y, float z) {
	PxTransform pos = Colliders[idx]->getGlobalPose();
	pos.p.x = x;
	pos.p.y = y;
	pos.p.z = z;

	Colliders[idx]->setGlobalPose(pos);
}
void Physics::setRotation(int idx, float x, float y, float z) {
	PxTransform pos = Colliders[idx]->getGlobalPose();

	float angle[3] = { x, y, z };
	PxQuat XQ(x, PxVec3(1, 0, 0));
	PxQuat YQ(y, PxVec3(0, 1, 0));
	PxQuat ZQ(z, PxVec3(0, 0, 1));

	PxQuat RQ = XQ * YQ * ZQ;

	pos.q.x = RQ.x;
	pos.q.y = RQ.y;
	pos.q.z = RQ.z;
	pos.q.w = RQ.w;

	Colliders[idx]->setGlobalPose(pos);
}
void Physics::setVelocity(int idx, float x, float y, float z) {
	Colliders[idx]->setLinearVelocity(PxVec3(x, y, z));
}
void Physics::setTorque(int idx, float x, float y, float z) {
	Colliders[idx]->addTorque(PxVec3(x, y, z));
}

// Cloth 실험
PxCloth* Physics::LoadCloth(PxClothParticle* vertices, PxClothMeshDesc& meshDesc) {
	PxClothFabric* fabric = PxClothFabricCreate(*gPhysics, meshDesc, PxVec3(0, -1, 0));
	//PX_ASSERT(fabric);

	PxTransform pose = PxTransform(PxIdentity);
	PxCloth* cloth = gPhysics->createCloth(pose, *fabric, vertices, PxClothFlags());
	//PX_ASSERT(cloth);

	fabric->release();

	// Cloth Update FPS
	cloth->setSolverFrequency(60.0f);

	gScene->addActor(*cloth);

	cloth->setStiffnessFrequency(10.0f);

	// damp global particle velocity to 90% every 0.1 seconds
	cloth->setDampingCoefficient(PxVec3(0.2f)); // damp local particle velocity
	cloth->setLinearDragCoefficient(PxVec3(0.2f)); // transfer frame velocity
	cloth->setAngularDragCoefficient(PxVec3(0.2f)); // transfer frame rotation

	// reduce impact of frame acceleration
	// x, z: cloth swings out less when walking in a circle
	// y: cloth responds less to jump acceleration
	cloth->setLinearInertiaScale(PxVec3(0.8f, 0.6f, 0.8f));

	// leave impact of frame torque at default
	cloth->setAngularInertiaScale(PxVec3(1.0f));

	// reduce centrifugal force of rotating frame
	cloth->setCentrifugalInertiaScale(PxVec3(0.3f));

	cloth->setInertiaScale(0.5f);

	// Continuouse Detecting Collision
	cloth->setClothFlag(PxClothFlag::eSWEPT_CONTACT, true);

	// 쿨롱 마찰 활성화
	cloth->setFrictionCoefficient(0.5f);
	// 충돌 입자의 질량을 임의로 늘린다.
	cloth->setCollisionMassScale(1.0f);

	// 자체 충돌 동작 활성화
	cloth->setSelfCollisionDistance(0.1f);
	cloth->setSelfCollisionStiffness(1.0f);

	const bool useSweptContact = true;
	const bool useCustomConfig = true;

	// ccd
	// cloth->setClothFlag(PxClothFlag::eSCENE_COLLISION, true);
	cloth->setClothFlag(PxClothFlag::eSWEPT_CONTACT, useSweptContact);

	// use GPU or not
//#if PX_SUPPORT_GPU_PHYSX || PX_XBOXONE
//	cloth->setClothFlag(gGpuFlag, true);
//#endif

	// custom fiber configuration
	if (useCustomConfig)
	{
		PxClothStretchConfig stretchConfig;

		stretchConfig.stiffness = 1.0f;
		stretchConfig.stiffnessMultiplier = 1.0f;
		stretchConfig.compressionLimit = 1.0f;
		stretchConfig.stretchLimit = 1.0f;

		//cloth->setStretchConfig(PxClothFabricPhaseType::eVERTICAL, stretchConfig);
		//cloth->setStretchConfig(PxClothFabricPhaseType::eHORIZONTAL, stretchConfig);
		//cloth->setStretchConfig(PxClothFabricPhaseType::eSHEARING, stretchConfig);
		//cloth->setStretchConfig(PxClothFabricPhaseType::eBENDING, stretchConfig);

		cloth->setStretchConfig(PxClothFabricPhaseType::eVERTICAL, 1.0f);
		cloth->setStretchConfig(PxClothFabricPhaseType::eHORIZONTAL, 1.0f);
		cloth->setStretchConfig(PxClothFabricPhaseType::eSHEARING, 0.75f);
		cloth->setStretchConfig(PxClothFabricPhaseType::eBENDING, 0.5f);

		cloth->setTetherConfig(PxClothTetherConfig(1.0f));
	}

	return cloth;
}

bool Physics::RemoveCloth(PxCloth* cloth) {
	gScene->removeActor(*cloth);

	return true;
}