#pragma once

#include <sstream>
#include "BoxApp.h"

class Player {
private:
	BoxApp* app = nullptr;

	RenderItem* p = nullptr;
	RenderItem* SkyBox = nullptr;
	RenderItem* Bottom = nullptr;
	RenderItem* Charactor = nullptr;
	std::vector<std::string> CharactorTextures;
	std::vector<std::string> CharactorNormalTextures;
	std::vector<std::string> CharactorAlphaTextures;

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

		// Create Init Objects
		{
			SkyBox		= app->CreateStaticGameObject("SkyBoxGeo", 1);
			Bottom		= app->CreateStaticGameObject("BottomGeo", 1);
			Charactor	= app->CreateDynamicGameObject("CharactorGeo", 1);
			//Pinix		= app->CreateDynamicGameObject("PinixGeo", 1);
			TestBox		= app->CreateDynamicGameObject("TestGeo", 1);

			//app->ExtractAnimBones (
			//	std::string("D:\\Animation\\ImportAnimation\\Assets\\m\\Wisard"),
			//	std::string("test.fbx"),
			//	std::string("D:\\Modeling\\9c801715_Ganyu_HiPolySet_1.01\\Ganyu_HiPolySet_1.01"),
			//	std::string("test.pmx"),
			//	Charactor
			//);

			RenderItem::RenderType renderType = RenderItem::RenderType::_SKY_FORMAT_RENDER_TYPE;

			app->BindMaterial(SkyBox, "SkyMat", true);

			app->CreateSphereObject(
				"SkyBox",
				"",
				SkyBox,
				1.0f,
				30,
				20,
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(1, 1, 1),
				renderType,
				false
			);

			renderType = RenderItem::RenderType::_OPAQUE_RENDER_TYPE;

			app->BindMaterial(Bottom, "BottomMat", false);
			app->CreateGridObject(
				"Bottom",
				"",
				Bottom,
				500.0f, 
				500.0f, 
				120, 
				80,
				{ 0, -3.0f, 0 },
				{ 0.0f, 0.0f, 0.0f },
				{ 1, 1, 1 },
				renderType,
				false
			);

			app->CreatePMXObject
			(
				"Charactor",
				std::string("D:\\Modeling\\9c801715_Ganyu_HiPolySet_1.01\\Ganyu_HiPolySet_1.01"),
				std::string("test1.pmx"),
				texturePaths,
				Charactor,
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(1, 1, 1)
			);

			app->BindMaterial(TestBox, "BottomMat", false);
			app->CreateSphereObject(
				"TestGeo",
				"",
				TestBox,
				10.0f,
				8,
				8,
				{ -20.0f, 0.0f, -30.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 1, 1, 1 },
				renderType
			);

			//app->CreateFBXSkinnedObject
			//(
			//	"Pet",
			//	std::string("D:\\Modeling\\AnimationTEST\\phoenix-bird\\source"),
			//	std::string("fly.fbx"),
			//	PinixTexturePaths,
			//	Pinix,
			//	XMFLOAT3(8.0f, 20.0f, 0.0f),
			//	XMFLOAT3(0.0f, 0.0f, 0.0f),
			//	XMFLOAT3(0.01f, 0.01f, 0.01f)
			//);
		}

		// Light
		{
			Light lightData;
			lightData.mLightType = LightType::DIR_LIGHTS;
			lightData.mPosition = { 0.0f, 0.0f, 0.0f };
			lightData.mDirection = { 0.57735f, -0.57735f, 0.57735f };
			lightData.mStrength = { 0.8f, 0.8f, 0.8f };
			lightData.mFalloffStart = 0.0f;
			lightData.mFalloffEnd = 100.0f;
			app->uploadLight(lightData);

			lightData.mLightType = LightType::POINT_LIGHT;
			lightData.mPosition = { 0.0f, 0.0f, 0.0f };
			lightData.mDirection = { -0.57735f, -0.57735f, 0.57735f };
			lightData.mStrength = { 0.4f, 0.4f, 0.4f };
			lightData.mFalloffStart = 0.0f;
			lightData.mFalloffEnd = 100.0f;
			app->uploadLight(lightData);

			lightData.mLightType = LightType::SPOT_LIGHT;
			lightData.mPosition = { 0.0f, 0.0f, 0.0f };
			lightData.mDirection = { 0.0f, -0.707f, -0.707f };
			lightData.mStrength = { 0.2f, 0.2f, 0.2f };
			lightData.mFalloffStart = 0.0f;
			lightData.mFalloffEnd = 100.0f;
			app->uploadLight(lightData);

		}
	}

	void _Start()
	{
		
	}

	void _Update(const GameTimer& gt)
	{

	}

	void _End()
	{

	}
};