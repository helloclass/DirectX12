#pragma once

#include <sstream>
#include "BoxApp.h"

class Player {
private:
	BoxApp* app = nullptr;
	RenderItem* p = nullptr;

	RenderItem* SkyBox = nullptr;

	// 이렇게 하나의 번들임.
	RenderItem* Charactor = nullptr;
	std::vector<std::string> CharactorTextures;
	std::vector<std::string> CharactorNormalTextures;
	std::vector<std::string> CharactorAlphaTextures;

	RenderItem* Pinix = nullptr;

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

		// Upload SkyBox Texture
		{
			Texture SkyboxTex;
			SkyboxTex.Name = "skyTex";
			SkyboxTex.isCube = true;
			SkyboxTex.Filename = L"../../Textures/snowcube1024.dds";

			app->uploadTexture(SkyboxTex, true);
		}

		// Create Init Objects
		{
			SkyBox		= app->CreateStaticGameObject("SkyBoxGeo", 1);
			Charactor	= app->CreateDynamicGameObject("CharactorGeo", 1);
			Pinix		= app->CreateDynamicGameObject("PinixGeo", 1);

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
				renderType
			);

			app->CreatePMXObject
			(
				"Charactor",
				std::string("D:\\Modeling\\9c801715_Ganyu_HiPolySet_1.01\\Ganyu_HiPolySet_1.01"),
				std::string("test.pmx"),
				texturePaths,
				Charactor,
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(1, 1, 1)
			);

			app->CreateFBXSkinnedObject
			(
				"Pet",
				std::string("D:\\Modeling\\AnimationTEST\\phoenix-bird\\source"),
				std::string("fly.fbx"),
				PinixTexturePaths,
				Pinix,
				XMFLOAT3(8.0f, 20.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.01f, 0.01f, 0.01f)
			);
		}

		// Light
		{
			// AmbientLight = { 1.00f, 0.85f, 0.85f, 1.0f };
			// Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
			// Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
			// Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
			// Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
			// Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
			// Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

			LightData lightData;
			lightData.Direction = { 0.57735f, -0.57735f, 0.57735f };
			lightData.Strength = { 0.8f, 0.8f, 0.8f };

			app->uploadLight(lightData);

			lightData.Direction = { -0.57735f, -0.57735f, 0.57735f };
			lightData.Strength = { 0.4f, 0.4f, 0.4f };

			app->uploadLight(lightData);

			lightData.Direction = { 0.0f, -0.707f, -0.707f };
			lightData.Strength = { 0.2f, 0.2f, 0.2f };

			app->uploadLight(lightData);

		}
	}

	void _Start()
	{
		{
			
		}

	}

	void _Update(const GameTimer& gt)
	{

	}

	void _End()
	{

	}
};