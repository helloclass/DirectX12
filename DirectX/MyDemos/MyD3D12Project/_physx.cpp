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

	//if (!PxInitExtensions(*gPhysics, gPvd))
	//	throw std::runtime_error("PxInitExtensions Failed!!");

	//PxCudaContextManagerDesc cudaContextManagerDesc;

	//PxCudaContextManager* gCudaContextManager = PxCreateCudaContextManager(*gFoundation, cudaContextManagerDesc);

	PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -55.0f, 0.0f);
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

	//PxRigidStatic* groundPlane = PxCreatePlane(*gPhysics, PxPlane(0, 1, 0, 0), *gMaterial);
	//gScene->addActor(*groundPlane);

	CreateKinematic(
		PxTransform(physx::PxTransform({ 0, 0, 0 })), 
		CreateBox(5, 5, 5),
		1
	);

	CreateKinematic(
		PxTransform(physx::PxTransform({ 5, 0, 5 })),
		CreateBox(10, 7, 10),
		1
	);
}

void Physics::CleanUp() {
	try {
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

		//for (int bindObjCnt = 0; bindObjCnt < bindObj.size(); bindObjCnt++)
		//	delete(bindObj[bindObjCnt]);

		//for (int colliderCnt = 0; colliderCnt < StaticColliders.size(); colliderCnt++)
		//	StaticColliders[colliderCnt]->release();

		//for (int colliderCnt = 0; colliderCnt < DynamicColliders.size(); colliderCnt++)
		//	DynamicColliders[colliderCnt]->release();
	}
	catch (std::exception&)
	{
		throw std::runtime_error("");
	}
}

void Physics::BindObjColliber(PxRigidStatic* obj, struct PhysResource* res) {
	size_t colliderSize = (
		StaticColliders.size() + 
		DynamicColliders.size()
	);

	if (bindObj.size() != colliderSize)
		throw std::runtime_error("바인드 디스크립터와 실제 콜라이더의 싱크가 올바르지 않습니다.");

	// Bind Physx Resource
	bindObj.push_back(res);
	StaticColliders.push_back(obj);
}

void Physics::BindObjColliber(PxRigidDynamic* obj, struct PhysResource* res) {
	size_t colliderSize = (
		StaticColliders.size() + 
		DynamicColliders.size()
	);

	if (bindObj.size() != colliderSize)
		throw std::runtime_error("바인드 디스크립터와 실제 콜라이더의 싱크가 올바르지 않습니다.");

	// Bind Physx Resource
	bindObj.push_back(res);
	DynamicColliders.push_back(obj);
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

void Physics::setPosition(PxRigidDynamic* collider, float x, float y, float z) {
	PxTransform pos = collider->getGlobalPose();
	pos.p.x = x;
	pos.p.y = y;
	pos.p.z = z;

	collider->setGlobalPose(pos);
}
void Physics::setRotation(PxRigidDynamic* collider, float x, float y, float z) {
	PxTransform pos = collider->getGlobalPose();

	float angle[3] = { x, y, z };
	PxQuat XQ(x, PxVec3(1, 0, 0));
	PxQuat YQ(y, PxVec3(0, 1, 0));
	PxQuat ZQ(z, PxVec3(0, 0, 1));

	PxQuat RQ = XQ * YQ * ZQ;

	pos.q.x = RQ.x;
	pos.q.y = RQ.y;
	pos.q.z = RQ.z;
	pos.q.w = RQ.w;

	collider->setGlobalPose(pos);
}
void Physics::setVelocity(PxRigidDynamic* collider, float x, float y, float z) 
{
	collider->setLinearVelocity(PxVec3(x, y, z));
}
void Physics::setTorque(PxRigidDynamic* collider, float x, float y, float z) 
{
	collider->addTorque(PxVec3(x, y, z));
}

// Cloth 실험
PxCloth* Physics::LoadCloth(PxClothParticle* vertices, PxClothMeshDesc& meshDesc) {
	PxClothFabric* fabric = PxClothFabricCreate(*gPhysics, meshDesc, PxVec3(0, 0, 0));
	PX_ASSERT(fabric);

	PxTransform pose = PxTransform(PxIdentity);
	PxCloth* cloth = gPhysics->createCloth(pose, *fabric, vertices, PxClothFlags());
	//PX_ASSERT(cloth);

	fabric->release();

	// Cloth Update FPS
	cloth->setSolverFrequency(45.0f);

	gScene->addActor(*cloth);

	////////////////////////////////////////////////////////////////////////////////////
	// 각 버텍스에 속도, 가속도를 부여하여 모델이 깨져보일 수 있다.
	////////////////////////////////////////////////////////////////////////////////////

	// damp global particle velocity to 90% every 0.1 seconds
	//cloth->setDampingCoefficient(PxVec3(0.2f)); // damp local particle velocity
	//cloth->setLinearDragCoefficient(PxVec3(0.2f)); // transfer frame velocity
	//cloth->setAngularDragCoefficient(PxVec3(0.2f)); // transfer frame rotation

	// reduce impact of frame acceleration
	// x, z: cloth swings out less when walking in a circle
	// y: cloth responds less to jump acceleration
	//cloth->setLinearInertiaScale(PxVec3(0.8f, 0.6f, 0.8f));

	////////////////////////////////////////////////////////////////////////////////////

	//// leave impact of frame torque at default
	//cloth->setAngularInertiaScale(PxVec3(1.0f));

	//// reduce centrifugal force of rotating frame
	//cloth->setCentrifugalInertiaScale(PxVec3(0.3f));

	//cloth->setInertiaScale(0.5f);

	// 쿨롱 마찰 활성화
	cloth->setFrictionCoefficient(0.5f);
	// 충돌 입자의 질량을 임의로 늘린다.
	cloth->setCollisionMassScale(1.0f);

	//// 자체 충돌 동작 활성화
	//cloth->setSelfCollisionDistance(0.2f);
	//cloth->setSelfCollisionStiffness(1.0f);

	const bool useSweptContact = true;
	const bool useCustomConfig = true;

	// ccd
	cloth->setClothFlag(PxClothFlag::eSWEPT_CONTACT, useSweptContact);

	// use GPU or not
//#if PX_SUPPORT_GPU_PHYSX || PX_XBOXONE
//	cloth->setClothFlag(gGpuFlag, true);
//#endif

	// custom fiber configuration
	if (useCustomConfig)
	{
		// Limit of Stretch each Vertices
		physx::PxClothStretchConfig mStretchConf;
		mStretchConf.stiffness = 1.0f;
		mStretchConf.stiffnessMultiplier = 1.0f;
		mStretchConf.compressionLimit = 0.9f;
		mStretchConf.stretchLimit = 1.0f;

		cloth->setStretchConfig(PxClothFabricPhaseType::eVERTICAL, mStretchConf);
		cloth->setStretchConfig(PxClothFabricPhaseType::eHORIZONTAL, mStretchConf);
		cloth->setStretchConfig(PxClothFabricPhaseType::eSHEARING, mStretchConf);
		cloth->setStretchConfig(PxClothFabricPhaseType::eBENDING, mStretchConf);

		//// Limit of Stretch each Vertices
		//cloth->setStretchConfig(PxClothFabricPhaseType::eVERTICAL, 1.0f);
		//cloth->setStretchConfig(PxClothFabricPhaseType::eHORIZONTAL, 1.0f);
		//cloth->setStretchConfig(PxClothFabricPhaseType::eSHEARING, 1.0f);
		//cloth->setStretchConfig(PxClothFabricPhaseType::eBENDING, 1.0f);

		//physx::PxClothMotionConstraintConfig
		//physx::PxClothStretchConfig

		PxClothTetherConfig mTetherConf;
		mTetherConf.stiffness = 1.0f;
		mTetherConf.stretchLimit = 1.0f;

		cloth->setTetherConfig(mTetherConf);
	}

	return cloth;
}

bool Physics::RemoveCloth(PxCloth* cloth) {
	gScene->removeActor(*cloth);

	return true;
}