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
			SkyBox		= app->CreateGameObject("SkyBoxGeo", 1);
			Bottom		= app->CreateGameObject("BottomGeo", 1);

			Charactor	= app->CreateDynamicGameObject("CharactorGeo", 1);
			//Pinix		= app->CreateDynamicGameObject("PinixGeo", 1);
			TestBox		= app->CreateDynamicGameObject("TestGeo", 1);

			Tonado		= app->CreateGameObject("TonadoGeo", 1);

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

			app->CreateGridObject(
				"Bottom",
				"",
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
			app->BindMaterial(Bottom, "BottomMat", false);

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
			{
				RenderItem* StartSplash = new RenderItem("StartSplash", 1);

				app->CreateBillBoardObject(
					"StartSplashGeo",
					"",
					"SKILL",
					StartSplash,
					50,
					{ 0.0f, 1.0f, 0.0f },
					{ 10, 10 },
					ObjectData::RenderType::_SKILL_TONADO_SPLASH_TYPE,
					false
				);

				StartSplash->InitParticleSystem(
					10.0f,
					DirectX::XMFLOAT3(0.0f, -20.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, -20.0f, 0.0f),
					DirectX::XMFLOAT3(-7.5f, 25.0f, -7.5f),
					DirectX::XMFLOAT3(7.5f, 40.0f, 7.5f),
					true
				);

				StartSplash->ParticleGene();

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
					"SKILL_TONADO_SPLASH_MATERIAL",
					"StartSplash_Diffuse_Tex",
					"StartSplash_Mask_Tex",
					"StartSplash_Noise_Tex",
					false
				);

				app->BindMaterial(
					Tonado->getChildRenderItem("StartSplash"),
					"SKILL_TONADO_SPLASH_MATERIAL"
				);
			}
		}

		{
			RenderItem* StartSplash2 = new RenderItem("StartSplash2", 1);

			app->CreateBillBoardObject(
				"StartSplash2Geo",
				"",
				"SKILL",
				StartSplash2,
				30,
				{ 0.0f, 1.0f, 0.0f },
				{ 10, 10 },
				ObjectData::RenderType::_SKILL_TONADO_SPLASH_TYPE,
				false
			);

			StartSplash2->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, -20.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, -20.0f, 0.0f),
				DirectX::XMFLOAT3(-20.0f, 15.0f, -20.0f),
				DirectX::XMFLOAT3(20.0f, 20.0f, 20.0f),
				true
			);

			StartSplash2->ParticleGene();

			Tonado->appendChildRenderItem(StartSplash2);

			app->BindMaterial(
				Tonado->getChildRenderItem("StartSplash2"),
				"SKILL_TONADO_SPLASH_MATERIAL"
			);
		}

		{
			RenderItem* StartSplash3 = new RenderItem("StartSplash3", 1);

			app->CreateBillBoardObject(
				"StartSplash3Geo",
				"",
				"SKILL",
				StartSplash3,
				16,
				{ 0.0f, 1.0f, 0.0f },
				{ 10, 10 },
				ObjectData::RenderType::_SKILL_TONADO_SPLASH_TYPE,
				false
			);

			StartSplash3->setIsFilled(false);
			StartSplash3->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, -10.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, -10.0f, 0.0f),
				DirectX::XMFLOAT3(-15.0f, 30.0f, -15.0f),
				DirectX::XMFLOAT3(15.0f, 30.0f, 15.0f),
				true
			);

			StartSplash3->ParticleGene();

			Tonado->appendChildRenderItem(StartSplash3);

			Texture StartSplash_Diffuse_Tex;
			StartSplash_Diffuse_Tex.Name = "StartSplash3_Diffuse_Tex";
			StartSplash_Diffuse_Tex.Filename = L"../../Textures/Skill/ToonProject2/HPwater2.dds";

			app->uploadTexture(StartSplash_Diffuse_Tex, false);

			app->uploadMaterial(
				"SKILL_TONADO_SPLASH_3_MATERIAL",
				"StartSplash3_Diffuse_Tex",
				false
			);

			app->BindMaterial(
				Tonado->getChildRenderItem("StartSplash3"),
				"SKILL_TONADO_SPLASH_3_MATERIAL"
			);
		}
	}

	void _Start()
	{
		
	}

	float testestest = 0.0f;

	void _Update(const GameTimer& gt)
	{
		RenderItem* particle = Tonado->getChildRenderItem("StartSplash");
		RenderItem* particle2 = Tonado->getChildRenderItem("StartSplash2");
		RenderItem* particle3 = Tonado->getChildRenderItem("StartSplash3");

		float endTime = particle->getAnimEndIndex();
		if (testestest > 3.0f)
		{
			testestest = 0.0f;
			particle->ParticleReset();
			particle2->ParticleReset();
			particle3->ParticleReset();
		}

		particle->ParticleUpdate(gt.DeltaTime());
		particle2->ParticleUpdate(gt.DeltaTime());
		particle3->ParticleUpdate(gt.DeltaTime());

		testestest += gt.DeltaTime();
	}

	void _End()
	{

	}
};