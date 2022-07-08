#pragma once

#include <sstream>
#include "BoxApp.h"

class Player {
private:
	BoxApp* app = nullptr;

	RenderItem* SkyBox = nullptr;
	RenderItem* Bottom = nullptr;

	RenderItem* Charactor = nullptr;
	std::vector<std::string> CharactorTextures;
	std::vector<std::string> CharactorNormalTextures;
	std::vector<std::string> CharactorAlphaTextures;

	RenderItem* Pinix = nullptr;
	RenderItem* TestBox = nullptr;

	RenderItem* Tonado = nullptr;

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

			Tonado		= app->CreateDynamicGameObject("TonadoGeo", 1);

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
				SkyBox,
				500.0f,
				30,
				20,
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(1, 1, 1),
				renderType,
				false
			);

			renderType = ObjectData::RenderType::_OPAQUE_RENDER_TYPE;

			app->BindMaterial(Bottom, "BottomMat", false);
			app->CreateGridObject(
				"Bottom",
				"",
				Bottom,
				500.0f, 
				500.0f, 
				120, 
				80,
				{ 0, -1.0f, 0 },
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
			app->CreateDebugBoxObject(Charactor);
			Charactor->setAnimClip("RUN");

			renderType = ObjectData::RenderType::_DRAW_MAP_RENDER_TYPE;

			app->BindMaterial(TestBox, "SketchMat");
			app->CreateGridObject(
				"TestGeo",
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
			app->CreateDebugBoxObject(TestBox);

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

		// 스킬 이펙트
		{
			RenderItem* StartSplash = new RenderItem("StartSplash", 50);

			app->CreateGridObject(
				"StartSplashGeo",
				"",
				StartSplash,
				10.0f,
				10.0f,
				2,
				2,
				{ 0.0f, 1.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 1, 1, 1 },
				ObjectData::RenderType::_OPAQUE_RENDER_TYPE,
				false,
				true
			);

			app->BindMaterial(StartSplash, "BottomMat");

			Tonado->appendChildRenderItem(StartSplash);

			Texture StartSplash_Diffuse_Tex;
			StartSplash_Diffuse_Tex.Name = "StartSplash_Diffuse_Tex";
			StartSplash_Diffuse_Tex.Filename = L"../../Textures/White.dds";

			Texture StartSplash_Mask_Tex;
			StartSplash_Mask_Tex.Name = "StartSplash_Mask_Tex";
			StartSplash_Mask_Tex.Filename = L"../../Textures/Skill/ToonProject2/MaskForShader.dds";

			Texture StartSplash_Noise_Tex;
			StartSplash_Noise_Tex.Name = "StartSplash_Noise_Tex";
			StartSplash_Noise_Tex.Filename = L"../../Textures/Skill/ToonProject2/Noise34.dds";

			app->uploadTexture(StartSplash_Diffuse_Tex, false);
			app->uploadTexture(StartSplash_Mask_Tex, false);
			app->uploadTexture(StartSplash_Noise_Tex, false);
			
			app->uploadMaterial(
				"SketchMat", 
				"StartSplash_Diffuse_Tex",
				"StartSplash_Mask_Tex", 
				"StartSplash_Noise_Tex",
				false
			);

			app->BindMaterial(
				Tonado->getChildRenderItem("StartSplash"),
				"SketchMat"
			);

			// 여기에 Velocity, 사라지는 시간 설정 필요.
			Particle mStartSplashParticles(StartSplash->InstanceCount);

			mStartSplashParticles.setDurationTime(3.0f);

			mStartSplashParticles.setMinVelo(DirectX::XMFLOAT3(-3.0f, 3.0f, -3.0f));
			mStartSplashParticles.setMaxVelo(DirectX::XMFLOAT3(3.0f, 10.0f, 3.0f));

			mStartSplashParticles.setOnPlayAwake(true);

			mStartSplashParticles.Generator();
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