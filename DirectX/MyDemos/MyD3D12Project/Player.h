#pragma once

#include <sstream>
#include "BoxApp.h"
#include "Skeleton.h"
#include "Tonado.h"
#include "Punch.h"
#include "Laser.h"

class Player {
private:
	BoxApp* app = nullptr;

	MapObject*			mMap		= nullptr;
	Skeleton*			mSkeleton	= nullptr;
	TonadoSkillEffect*	mTonado		= nullptr;
	PunchSkillEffect*	mPunch		= nullptr;
	LaserSkillEffect*	mLaser		= nullptr;

private:
	RenderItem* SkyBox = nullptr;

	RenderItem* Charactor = nullptr;
	std::vector<std::string> CharactorTextures;
	std::vector<std::string> CharactorNormalTextures;
	std::vector<std::string> CharactorAlphaTextures;

	RenderItem* Pinix = nullptr;
	RenderItem* TestBox = nullptr;

	float mControlY = 0.0f;

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

		app->mMouseWheel = 50.0f;

		// Create Init Objects
		{
			SkyBox		= app->CreateGameObject("SkyBoxGeo", 1);

			Charactor	= app->CreateDynamicGameObject("CharactorGeo", 1);
			Pinix		= app->CreateDynamicGameObject("PinixGeo", 1);
			TestBox		= app->CreateDynamicGameObject("TestGeo", 1);

			Charactor->InstanceCount = 1;

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
				std::string("test2.pmx"),
				texturePaths,
				Charactor,
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(1, 1, 1),
				true
			);

			//app->CreateDebugBoxObject(Charactor);
			Charactor->setIsDrawShadow(true);
			Charactor->setAnimClip("RUN", 0, true);

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

			AnimationClip mPinixAnimation(1);

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



		mSkeleton = new Skeleton();
		mSkeleton->_Awake(app);

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
			lightData.mStrength = { 0.75f, 0.75f, 0.8f, 0.0f };
			lightData.mFalloffStart = 0.0f;
			lightData.mFalloffEnd = 100.0f;
			app->uploadLight(lightData);

			lightData.mLightType = LightType::DIR_LIGHTS;
			lightData.mPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
			lightData.mDirection = { -0.57735f, -0.57735f, 0.57735f, 0.0f };
			lightData.mStrength = { 0.5f, 0.4f, 0.4f, 0.0f };
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
	ObjectData* mGroundData = nullptr;

	std::list<InstanceData>::iterator mCharactorInstanceIterator;
	std::list<DirectX::BoundingBox>::iterator mCharactorBoundsIterator;

	std::list<DirectX::BoundingBox>::iterator mGroundBoundsIterator;
	std::list<DirectX::BoundingBox>::iterator mForwardGroundBoundsIterator;

	float hpGuage = 0.5f;
	float mpGuage = 0.8f;

	void _Start()
	{
		mSkeleton->_Start();
		mMap->_Start();
		mTonado->_Start();
		mPunch->_Start();
		mLaser->_Start();

		mCharactorData = app->GetData("CharactorGeo");
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

		app->mInputVector.insert(std::make_pair(VK_UP, 0));
		app->mInputVector.insert(std::make_pair(VK_DOWN, 0));

		// Update Player HP, MP State
		ImGuiWindowFlags window_flags = 0;
		window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
		window_flags |= ImGuiWindowFlags_NoBackground;
		window_flags |= ImGuiWindowFlags_NoTitleBar;

		std::shared_ptr<ImGuiFrameComponent> clothParamComp = pushFrame("PlayerStateGUI");
		clothParamComp->setWindowFlags(window_flags);

		clothParamComp->pushImageComponent("HP", app->mGUIResources["HP"], 150, 30, true, &hpGuage);
		clothParamComp->pushImageComponent("MP", app->mGUIResources["MP"], 150, 30, true, &mpGuage);
		clothParamComp->pushImageComponent("Storm", app->mGUIResources["STORM"], 30, 30, false);
		clothParamComp->pushImageComponent("Thunder", app->mGUIResources["Thunder"], 30, 30, false);
		clothParamComp->pushImageComponent("Earthquake", app->mGUIResources["Earthquake"], 30, 30, true);
	}

	bool isGround = true;
	
	const float mPlayerSpeed = 30.0f;

	int pre = 0;
	float gravity = -90.0f;
	std::string charactorName = "IDLE1";

	void _Update(const GameTimer& gt)
	{
		mSkeleton->_Update(gt);
		mMap->_Update(gt);
		mTonado->_Update(gt);
		mPunch->_Update(gt);
		mLaser->_Update(gt);

		mCharactorBoundsIterator = mCharactorData->Bounds.begin();
		DirectX::XMVECTOR playerPos = mCharactorData->mTranslate[0].position;
		playerPos.m128_f32[0] += 5.0f;
		playerPos.m128_f32[2] += 10.0f;

		DirectX::XMVECTOR closeUpPos = mCharactorData->mTranslate[0].position;
		closeUpPos.m128_f32[1] += mControlY;

		//////
		// 이후 각 라이트마다의 경계구로 변형 요망
		//////
		app->mSceneBounds.Radius = 600.0f;

		// Find under the floor of main charactor
		int mForwardBottomIndex = 0;
		int mBottomIndex = ((int)playerPos.m128_f32[0] / 20) * 30 + (int)playerPos.m128_f32[2] / 20;
		mBottomIndex = mBottomIndex > -1 ? mBottomIndex : 0;
		mGroundBoundsIterator = std::next(mGroundData->Bounds.begin(), mapIndex[mBottomIndex]);

		float mOffsetScale = app->mMouseWheel;

		DirectX::XMVECTOR offset = {
			-2.0f * mOffsetScale * sinf(app->mMainCameraDeltaRotationY),
			mOffsetScale,
			-2.0f * mOffsetScale * cosf(app->mMainCameraDeltaRotationY),
			1.0f
		};

		app->mCamera.SetPosition(
			closeUpPos +
			offset
		);

		app->mCamera.LookAt(
			app->mCamera.GetPosition(),
			closeUpPos +
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

			if (app->mInputVector[VK_UP])
			{
				app->mInputVector[VK_UP] = 0;

				mControlY += gt.DeltaTime();
			}
			else if (app->mInputVector[VK_DOWN])
			{
				app->mInputVector[VK_DOWN] = 0;
				
				mControlY -= gt.DeltaTime();
			}

			if(app->mInputVector['W'])
			{
				isNotPut = true;
				app->mInputVector['W'] = 0;

				mCharactorData->mTranslate[0].rotation = {
					0.0f,
					app->mMainCameraDeltaRotationY,
					0.0f,
					1.0f
				};

				if (!isPutShift)
				{
					stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
					mCharactorData->mTranslate[0].setVelocity({ 0.0f, 0.0f, mPlayerSpeed, 1.0f });
					mCharactorData->mTranslate[0].addVelocityY(stockY);

					if (charactorName != "RUN")
					{
						charactorName = "RUN";
						Charactor->setAnimClip(charactorName, 0, true);
					}
				}
				else
				{
					stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
					mCharactorData->mTranslate[0].setVelocity({ 0.0f, 0.0f, mPlayerSpeed, 1.0f });
					mCharactorData->mTranslate[0].addVelocityY(stockY);

					if (charactorName != "RUN")
					{
						charactorName = "RUN";
						Charactor->setAnimClip(charactorName, 0, true);
					}
				}
			}
			else if (app->mInputVector['S'])
			{
				isNotPut = true;
				app->mInputVector['S'] = 0;
				
				mCharactorData->mTranslate[0].rotation = {
					0.0f,
					app->mMainCameraDeltaRotationY,
					0.0f,
					1.0f
				};

				stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
				mCharactorData->mTranslate[0].setVelocity({ 0.0f, 0.0f, -mPlayerSpeed, 1.0f });
				mCharactorData->mTranslate[0].addVelocityY(stockY);

				if (charactorName != "RUN")
				{
					charactorName = "RUN";
					Charactor->setAnimClip(charactorName, 0, true);
				}
			}
			if (app->mInputVector['A'])
			{
				isNotPut = true;
				app->mInputVector['A'] = 0;

				stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
				mCharactorData->mTranslate[0].setVelocity({ -mPlayerSpeed, 0.0f, 0.0f, 1.0f });
				mCharactorData->mTranslate[0].addVelocityY(stockY);

				if (charactorName != "RUN")
				{
					charactorName = "RUN";
					Charactor->setAnimClip(charactorName, 0, true);
				}
			}
			else if (app->mInputVector['D'])
			{
				isNotPut = true;
				app->mInputVector['D'] = 0;

				stockY = mCharactorData->mTranslate[0].velocity.m128_f32[1];
				mCharactorData->mTranslate[0].setVelocity({ mPlayerSpeed, 0.0f, 0.0f, 1.0f });
				mCharactorData->mTranslate[0].addVelocityY(stockY);

				if (charactorName != "RUN")
				{
					charactorName = "RUN";
					Charactor->setAnimClip(charactorName, 0, true);
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
					Charactor->setAnimClip(charactorName, 0, true);
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
				Charactor->setAnimClip(charactorName, 0, false);
			}

			// 리스폰
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

			if (charactorName != "EARTHQUAKE_SPELL")
			{
				charactorName = "EARTHQUAKE_SPELL";
				Charactor->setAnimClip(charactorName, 0, false);
			}

			mTonado->mPosition = mCharactorData->mTranslate[0].position;
			mTonado->mRotation = mCharactorData->mTranslate[0].rotation;

			mTonado->mPosition.m128_f32[0] *= 0.1f;
			mTonado->mPosition.m128_f32[1] *= 0.1f;
			mTonado->mPosition.m128_f32[2] *= 0.1f;

			mTonado->isUpdate = true;
		}
		else if (app->mInputVector['2'])
		{
			app->mInputVector['2'] = 0;

			if (charactorName != "EARTHQUAKE_SPELL")
			{
				charactorName = "EARTHQUAKE_SPELL";
				Charactor->setAnimClip(charactorName, 0, false);
			}

			mPunch->mPosition = mCharactorData->mTranslate[0].position;
			mPunch->mRotation = mCharactorData->mTranslate[0].rotation;

			mPunch->mPosition.m128_f32[0] *= 0.1f;
			mPunch->mPosition.m128_f32[1] *= 0.1f;
			mPunch->mPosition.m128_f32[2] *= 0.1f;

			mPunch->mPosition.m128_f32[1] += 1.0f;

			mPunch->isUpdate = true;
		}
		else if (app->mInputVector['3'])
		{
			app->mInputVector['3'] = 0;

			if (charactorName != "EARTHQUAKE_SPELL")
			{
				charactorName = "EARTHQUAKE_SPELL";
				Charactor->setAnimClip(charactorName, 0, false);
			}

			mLaser->mPosition = mCharactorData->mTranslate[0].position;
			mLaser->mRotation = mCharactorData->mTranslate[0].rotation;

			mLaser->mPosition.m128_f32[0] *= 0.1f;
			mLaser->mPosition.m128_f32[1] *= 0.1f;
			mLaser->mPosition.m128_f32[2] *= 0.1f;

			mLaser->mPosition.m128_f32[1] += 1.0f;

			mLaser->isUpdate = true;
		}
	}

	void _End()
	{
		mMap->_End();
	}
};