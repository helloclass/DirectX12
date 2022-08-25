#pragma once

#include <sstream>
#include "BoxApp.h"
#include "Tonado.h"
#include "Punch.h"
#include "Laser.h"
#include "Map.h"

class Player {
private:
	BoxApp* app = nullptr;

	MapObject* mMap = nullptr;
	TonadoSkillEffect* mTonado = nullptr;
	PunchSkillEffect* mPunch = nullptr;
	LaserSkillEffect* mLaser = nullptr;

private:
	RenderItem* SkyBox = nullptr;

	RenderItem* Charactor = nullptr;
	std::vector<std::string> CharactorTextures;
	std::vector<std::string> CharactorNormalTextures;
	std::vector<std::string> CharactorAlphaTextures;

	RenderItem* Skeleton = nullptr;
	RenderItem* Pinix = nullptr;
	RenderItem* TestBox = nullptr;

public:
	Player() {}
	Player(const Player& rhs) = delete;
	Player& operator=(const Player& rhs) = delete;
	~Player() = default;

	void _Awake(BoxApp* app)
	{
		this->app = app;
		std::vector<std::string> texturePaths;
		std::vector<std::string> PinixTexturePaths;
		std::vector<std::string> MainTonadoTexturePaths;

		// Create Init Objects
		{
			SkyBox		= app->CreateGameObject("SkyBoxGeo", 1);

			Charactor	= app->CreateDynamicGameObject("CharactorGeo", 1);
			Skeleton	= app->CreateDynamicGameObject("SkeletonGeo", 1);
			Pinix		= app->CreateDynamicGameObject("PinixGeo", 1);
			TestBox		= app->CreateDynamicGameObject("TestGeo", 1);

			//app->ExtractAnimBones (
			//	std::string("D:\\Animation\\ImportAnimation\\Assets\\m\\Wisard"),
			//	std::string("test.fbx"),
			//	std::string("D:\\Modeling\\9c801715_Ganyu_HiPolySet_1.01\\Ganyu_HiPolySet_1.01"),
			//	std::string("test.pmx"),
			//	Charactor
			//);

			ObjectData::RenderType renderType = ObjectData::RenderType::_SKY_FORMAT_RENDER_TYPE;

			app->BindMaterial(SkyBox, "SkyMat", true);
			app->CreateSphereObject(
				"SkyBox",
				"",
				"",
				SkyBox,
				5000.0f,
				30,
				20,
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(1, 1, 1),
				renderType,
				true
			);

			renderType = ObjectData::RenderType::_OPAQUE_RENDER_TYPE;

			app->CreatePMXObject
			(
				"CharactorGeo",
				std::string("D:\\Modeling\\9c801715_Ganyu_HiPolySet_1.01\\Ganyu_HiPolySet_1.01"),
				std::string("test1.pmx"),
				texturePaths,
				Charactor,
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(1, 1, 1),
				true
			);
			//app->CreateDebugBoxObject(Charactor);
			Charactor->setIsDrawShadow(true);
			Charactor->setAnimClip("RUN", false);

			renderType = ObjectData::RenderType::_DRAW_MAP_RENDER_TYPE;

			app->BindMaterial(TestBox, "SketchMat");
			app->CreateGridObject(
				"TestGeo",
				"",
				"",
				TestBox,
				30.0f,
				30.0f,
				2,
				2,
				{ 0.0f, 15.0f, 0 },
				{ 90.0f, 0.0f, 0.0f },
				{ 1, 1, 1 },
				renderType, 
				false,
				true
			);
			//app->CreateDebugBoxObject(TestBox);

			AnimationClip mSkeletonAnimation;

			mSkeletonAnimation.appendClip("IDLE",			0.0f,		100.0f);
			mSkeletonAnimation.appendClip("HIT",			330.0f,		365.0f);
			mSkeletonAnimation.appendClip("WALKING",		2186.0f,	2217.0f);
			mSkeletonAnimation.appendClip("WALKING BACK",	1332.0f,	1365.0f);
			mSkeletonAnimation.appendClip("RUN",			1633.0f,	1655.0f);
			mSkeletonAnimation.appendClip("KNIFE DOWN",		2041.0f,	2076.0f);
			mSkeletonAnimation.appendClip("KNIFE LEFT",		2077.0f,	2112.0f);
			mSkeletonAnimation.appendClip("KNIFE RIGHT",	2113.0f,	2148.0f);
			mSkeletonAnimation.appendClip("KNIFE THRUST",	2149.0f,	2184.0f);
			mSkeletonAnimation.appendClip("BOW CHARGING",	2348.0f,	2383.0f);
			mSkeletonAnimation.appendClip("BOW AIM",		2384.0f,	2484.0f);
			mSkeletonAnimation.appendClip("BOW SHOOT",		2485.0f,	2520.0f);

			renderType = ObjectData::RenderType::_OPAQUE_RENDER_TYPE;

			app->CreateFBXSkinnedObject
			(
				"SkeletonGeo",
				std::string("D:\\Portfolio\\source\\DX12\\DirectX\\Models\\Mob\\ARMY_of__SKELETONS_pack"),
				std::string("Skeleton_archer.FBX"),
				PinixTexturePaths,
				Skeleton,
				XMFLOAT3(21.0f, 100.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.11f, 0.11f, 0.11f),
				mSkeletonAnimation,
				true
			);
			Skeleton->setBoundaryScale({ 10.0f, 10.5f, 10.0f });
			Skeleton->setIsDrawShadow(true);
			Skeleton->setAnimClip("IDLE");

			AnimationClip mPinixAnimation;

			app->CreateFBXSkinnedObject
			(
				"PinixGeo",
				std::string("D:\\Modeling\\AnimationTEST\\phoenix-bird\\source"),
				std::string("fly.fbx"),
				PinixTexturePaths,
				Pinix,
				XMFLOAT3(5.0f, 30.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.1f, 0.1f, 0.1f),
				mPinixAnimation,
				true,
				true
			);
			Pinix->setIsDrawShadow(true);
		}

		mTonado = new TonadoSkillEffect(app);
		mTonado->_Awake();
		mPunch = new PunchSkillEffect(app);
		mPunch->_Awake();
		mLaser = new LaserSkillEffect(app);
		mLaser->_Awake();

		mMap = new MapObject(app);
		mMap->_Awake();

		// Light
		{
			Light lightData;
			lightData.mLightType = LightType::DIR_LIGHTS;
			lightData.mPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
			lightData.mDirection = { 0.57735f, -0.57735f, 0.57735f, 0.0f };
			lightData.mStrength = { 0.8f, 0.8f, 0.8f, 0.0f };
			lightData.mFalloffStart = 0.0f;
			lightData.mFalloffEnd = 100.0f;
			app->uploadLight(lightData);

			lightData.mLightType = LightType::POINT_LIGHT;
			lightData.mPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
			lightData.mDirection = { -0.57735f, -0.57735f, 0.57735f, 0.0f };
			lightData.mStrength = { 0.4f, 0.4f, 0.4f, 0.0f };
			lightData.mFalloffStart = 0.0f;
			lightData.mFalloffEnd = 100.0f;
			app->uploadLight(lightData);

			lightData.mLightType = LightType::SPOT_LIGHT;
			lightData.mPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
			lightData.mDirection = { 0.0f, -0.707f, -0.707f , 0.0f };
			lightData.mStrength = { 0.2f, 0.2f, 0.2f, 0.0f };
			lightData.mFalloffStart = 0.0f;
			lightData.mFalloffEnd = 100.0f;
			app->uploadLight(lightData);
		}
	}

	ObjectData* mCharactorData = nullptr;
	ObjectData* mSkeletonData = nullptr;
	ObjectData* mGroundData = nullptr;

	std::list<InstanceData>::iterator mCharactorInstanceIterator;
	std::list<DirectX::BoundingBox>::iterator mCharactorBoundsIterator;
	std::list<DirectX::BoundingBox>::iterator mSkeletonBoundsIterator;

	std::list<DirectX::BoundingBox>::iterator mGroundBoundsIterator;
	std::list<DirectX::BoundingBox>::iterator mForwardGroundBoundsIterator;

	float hpGuage = 0.5f;
	float mpGuage = 0.8f;

	void _Start()
	{
		mMap->_Start();
		mTonado->_Start();
		mPunch->_Start();
		mLaser->_Start();

		mCharactorData = app->GetData("CharactorGeo");
		mSkeletonData = app->GetData("SkeletonGeo");
		mGroundData = app->GetData("MapYardGeo");

		mCharactorInstanceIterator = mCharactorData->mInstances.begin();

		app->mInputVector.insert(std::make_pair(VK_SPACE, 0));
		app->mInputVector.insert(std::make_pair(VK_LSHIFT, 0));
		app->mInputVector.insert(std::make_pair('W', 0));
		app->mInputVector.insert(std::make_pair('A', 0));
		app->mInputVector.insert(std::make_pair('S', 0));
		app->mInputVector.insert(std::make_pair('D', 0));

		app->mInputVector.insert(std::make_pair('1', 0));
		app->mInputVector.insert(std::make_pair('2', 0));
		app->mInputVector.insert(std::make_pair('3', 0));

		// Update Player HP, MP State
		ImGuiWindowFlags window_flags = 0;
		window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
		window_flags |= ImGuiWindowFlags_NoBackground;
		window_flags |= ImGuiWindowFlags_NoTitleBar;

		std::shared_ptr<ImGuiFrameComponent> clothParamComp = pushFrame("PlayerStateGUI");
		clothParamComp->setWindowFlags(window_flags);

		clothParamComp->pushImageComponent("HP", app->mGUIResources["HP"], 150.0f, 30.0f, true, &hpGuage);
		clothParamComp->pushImageComponent("MP", app->mGUIResources["MP"], 150.0f, 30.0f, true, &mpGuage);
		clothParamComp->pushImageComponent("Storm", app->mGUIResources["STORM"], 30.0f, 30.0f, false);
		clothParamComp->pushImageComponent("Thunder", app->mGUIResources["Thunder"], 30.0f, 30.0f, false);
		clothParamComp->pushImageComponent("Earthquake", app->mGUIResources["Earthquake"], 30.0f, 30.0f, true);
	}

	bool isGround = true;
	
	int pre = 0;
	float gravity = -90.0f;
	std::string charactorName = "IDLE1";

	// Skeleton
	bool isSkeletonGround = true;
	std::string skeletonName = "IDLE";

	void _Update(const GameTimer& gt)
	{
		mMap->_Update(gt);
		mTonado->_Update(gt);
		mPunch->_Update(gt);
		mLaser->_Update(gt);

		mCharactorBoundsIterator = mCharactorData->Bounds.begin();
		mSkeletonBoundsIterator = mSkeletonData->Bounds.begin();
		DirectX::XMVECTOR playerPos = mCharactorData->mTranslate[0].position;
		playerPos.m128_f32[0] += 5.0f;
		playerPos.m128_f32[2] += 10.0f;

		//////
		// 이후 각 라이트마다의 경계구로 변형 요망
		//////
		app->mSceneBounds.Radius = 600.0f;

		// Find under the floor of main charactor
		int mForwardBottomIndex = 0;
		int mBottomIndex = ((int)playerPos.m128_f32[0] / 20) * 30 + (int)playerPos.m128_f32[2] / 20;
		mGroundBoundsIterator = std::next(mGroundData->Bounds.begin(), mapIndex[mBottomIndex]);

		DirectX::XMVECTOR offset = {
			-100.0f * sinf(app->mMainCameraDeltaRotationY),
			40.0f,
			-100.0f * cosf(app->mMainCameraDeltaRotationY),
			1.0f
		};

		mCharactorData->mTranslate[0].rotation = {
			0.0f,
			app->mMainCameraDeltaRotationY,
			0.0f,
			1.0f 
		};

		app->mCamera.SetPosition(
			playerPos +
			offset
		);

		app->mCamera.LookAt(
			app->mCamera.GetPosition(),
			playerPos +
			DirectX::XMVECTOR({ 0.0f, 7.0f, 0, 1.0f }),
			{ 0.0f, 1.0f, 0.0f }
		);

		// Ground
		float stockY = 0.0f;
		bool isNotPut = false;
		bool isPutShift = false;
		if ((*mCharactorBoundsIterator).Contains(*(mGroundBoundsIterator)))
		{
			if (app->mInputVector[VK_SPACE])
			{
				isNotPut = true;
				app->mInputVector[VK_SPACE] = 0;
	
				mCharactorData->mTranslate[0].velocity.m128_f32[1] = 100.0f;
			}

			if (app->mInputVector[VK_LSHIFT])
			{
				app->mInputVector[VK_LSHIFT] = 0;
				isPutShift = true;
			}

			if(app->mInputVector['W'])
			{
				isNotPut = true;
				app->mInputVector['W'] = 0;

				if (!isPutShift)
				{
					stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
					mCharactorData->mTranslate[0].setVelocity({ 0.0f, 0.0f, 20.0f, 1.0f });
					mCharactorData->mTranslate[0].addVelocityY(stockY);

					if (charactorName != "RUN")
					{
						charactorName = "RUN";
						Charactor->setAnimClip(charactorName, false);
					}
				}
				else
				{
					stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
					mCharactorData->mTranslate[0].setVelocity({ 0.0f, 0.0f, 60.0f, 1.0f });
					mCharactorData->mTranslate[0].addVelocityY(stockY);

					if (charactorName != "RUN")
					{
						charactorName = "RUN";
						Charactor->setAnimClip(charactorName, false);
					}
				}
			}
			else if (app->mInputVector['S'])
			{
				isNotPut = true;
				app->mInputVector['S'] = 0;

				stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
				mCharactorData->mTranslate[0].setVelocity({ 0.0f, 0.0f, -20.0f, 1.0f });
				mCharactorData->mTranslate[0].addVelocityY(stockY);

				if (charactorName != "RUN")
				{
					charactorName = "RUN";
					Charactor->setAnimClip(charactorName, false);
				}
			}
			if (app->mInputVector['A'])
			{
				isNotPut = true;
				app->mInputVector['A'] = 0;

				stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
				mCharactorData->mTranslate[0].setVelocity({ -20.0f, 0.0f, 0.0f, 1.0f });
				mCharactorData->mTranslate[0].addVelocityY(stockY);

				if (charactorName != "RUN")
				{
					charactorName = "RUN";
					Charactor->setAnimClip(charactorName, false);
				}
			}
			else if (app->mInputVector['D'])
			{
				isNotPut = true;
				app->mInputVector['D'] = 0;

				stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
				mCharactorData->mTranslate[0].setVelocity({ 20.0f, 0.0f, 0.0f, 1.0f });
				mCharactorData->mTranslate[0].addVelocityY(stockY);

				if (charactorName != "RUN")
				{
					charactorName = "RUN";
					Charactor->setAnimClip(charactorName, false);
				}
			}

			if (!isNotPut || !isGround)
			{
				isGround = true;

				mCharactorData->mTranslate[0].velocity.m128_f32[0] = 0.0f;
				mCharactorData->mTranslate[0].velocity.m128_f32[1] = 0.0f;
				mCharactorData->mTranslate[0].velocity.m128_f32[2] = 0.0f;
				mCharactorData->mTranslate[0].accelerate = { 0.0f, 0.0f, 0.0f, 1.0f };

				if (charactorName != "IDLE1")
				{
					charactorName = "IDLE1";
					Charactor->setAnimClip(charactorName, false);
				}
			}
		}
		// Non-Ground
		else
		{
			isGround = false;
			mCharactorData->mTranslate[0].accelerate = { 0.0f, gravity, 0.0f, 1.0f };

			if (charactorName != "JUMP_BACK")
			{
				charactorName = "JUMP_BACK";
				Charactor->setAnimClip(charactorName, false);
			}

			if (mCharactorData->mTranslate[0].position.m128_f32[1] < -30.0f)
			{
				mCharactorData->mTranslate[0].velocity.m128_f32[0] = 0.0f;
				mCharactorData->mTranslate[0].velocity.m128_f32[1] = 0.0f;
				mCharactorData->mTranslate[0].velocity.m128_f32[2] = 0.0f;

				mCharactorData->mTranslate[0].position.m128_f32[0] = 0.0f;
				mCharactorData->mTranslate[0].position.m128_f32[1] = 50.0f;
				mCharactorData->mTranslate[0].position.m128_f32[2] = 0.0f;
			}
		}

		if (app->mInputVector['1'])
		{
			app->mInputVector['1'] = 0;

			mTonado->mPosition = mCharactorData->mTranslate[0].position;

			mTonado->mPosition.m128_f32[0] *= 0.1f;
			mTonado->mPosition.m128_f32[1] *= 0.1f;
			mTonado->mPosition.m128_f32[2] *= 0.1f;

			mTonado->isUpdate = true;
		}
		else if (app->mInputVector['2'])
		{
			app->mInputVector['2'] = 0;

			mPunch->mPosition = mCharactorData->mTranslate[0].position;

			mPunch->mPosition.m128_f32[0] *= 0.1f;
			mPunch->mPosition.m128_f32[1] *= 0.1f;
			mPunch->mPosition.m128_f32[2] *= 0.1f;

			mPunch->mPosition.m128_f32[1] += 1.0f;

			mPunch->isUpdate = true;
		}
		else if (app->mInputVector['3'])
		{
			app->mInputVector['3'] = 0;

			mLaser->mPosition = mCharactorData->mTranslate[0].position;

			mLaser->mPosition.m128_f32[0] *= 0.1f;
			mLaser->mPosition.m128_f32[1] *= 0.1f;
			mLaser->mPosition.m128_f32[2] *= 0.1f;

			mLaser->mPosition.m128_f32[1] += 1.0f;

			mLaser->isUpdate = true;
		}


		DirectX::XMVECTOR skeletonPos;

		// Calc Skeleton to Player
		DirectX::XMVECTOR Skeleton2Player;

		for (int i = 0; i < mSkeletonData->mTranslate.size(); i++)
		{
			skeletonPos = mSkeletonData->mTranslate[i].position;
			skeletonPos.m128_f32[0] += 10.0f;
			skeletonPos.m128_f32[2] += 10.0f;

			///////////////
				Skeleton2Player = playerPos - skeletonPos;
				Skeleton2Player = DirectX::XMVector4Normalize(Skeleton2Player);

				// 길이 20 벡터의 3차원 노말 34.6410f
				Skeleton2Player.m128_f32[0] *= 34.6410f;
				Skeleton2Player.m128_f32[2] *= 34.6410f;

			mForwardBottomIndex = 
				(int)(skeletonPos.m128_f32[0] + Skeleton2Player.m128_f32[0]) / 20 * 30 +
				(int)(skeletonPos.m128_f32[2] + Skeleton2Player.m128_f32[2]) / 20;
			mForwardBottomIndex =
				mForwardBottomIndex >= 0 ? mForwardBottomIndex : 0;

			///////////////

			mBottomIndex = ((int)skeletonPos.m128_f32[0] / 20) * 30 + (int)skeletonPos.m128_f32[2] / 20;
			mBottomIndex = mBottomIndex >= 0 ? mBottomIndex : 0;

			mGroundBoundsIterator = std::next(mGroundData->Bounds.begin(), mapIndex[mBottomIndex]);
			mForwardGroundBoundsIterator = std::next(mGroundData->Bounds.begin(), mapIndex[mForwardBottomIndex]);

			isNotPut = false;
			if ((*mSkeletonBoundsIterator).Contains(*(mGroundBoundsIterator)))
			{
				if (skeletonName != "RUN")
				{
					skeletonName = "RUN";
					Skeleton->setAnimClip(skeletonName);
				}

				mSkeletonData->mTranslate[i].velocity.m128_f32[0] = Skeleton2Player.m128_f32[0];
				mSkeletonData->mTranslate[i].velocity.m128_f32[2] = Skeleton2Player.m128_f32[2];

				mSkeletonData->mTranslate[i].rotation = {
					0.0f,
					sinf(Skeleton2Player.m128_f32[1]) + cosf(Skeleton2Player.m128_f32[1]),
					0.0f,
					1.0f
				};

				if ((*mGroundBoundsIterator).Center.y < (*mForwardGroundBoundsIterator).Center.y ||
					mapIndex[mForwardBottomIndex] == 0)
				{
					isNotPut = true;

					mSkeletonData->mTranslate[i].velocity.m128_f32[1] = 100.0f;
				}
				if (!isNotPut || !isGround)
				{
					isSkeletonGround = true;

					//mSkeletonData->mTranslate[i].velocity.m128_f32[0] = 0.0f;
					mSkeletonData->mTranslate[i].velocity.m128_f32[1] = 0.0f;
					//mSkeletonData->mTranslate[i].velocity.m128_f32[2] = 0.0f;
					mSkeletonData->mTranslate[i].accelerate = { 0.0f, 0.0f, 0.0f, 1.0f };
				}
			}
			// Non-Ground
			else
			{
				isSkeletonGround = false;

				mSkeletonData->mTranslate[i].accelerate = { 0.0f, gravity, 0.0f, 1.0f };
				
				if (mSkeletonData->mTranslate[i].position.m128_f32[1] < -30.0f)
				{
					if (skeletonName != "IDLE")
					{
						skeletonName = "IDLE";
						Skeleton->setAnimClip(skeletonName);
					}

					mSkeletonData->mTranslate[i].velocity.m128_f32[0] = 0.0f;
					mSkeletonData->mTranslate[i].velocity.m128_f32[1] = 0.0f;
					mSkeletonData->mTranslate[i].velocity.m128_f32[2] = 0.0f;

					mSkeletonData->mTranslate[i].position.m128_f32[0] = 21.0f;
					mSkeletonData->mTranslate[i].position.m128_f32[1] = 50.0f;
					mSkeletonData->mTranslate[i].position.m128_f32[2] = 0.0f;
				}
			}
		}
	}

	void _End()
	{
		mMap->_End();
	}
};