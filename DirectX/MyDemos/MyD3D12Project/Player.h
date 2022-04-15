#pragma once

#include <sstream>
#include "BoxApp.h"

class Player {
private:
	BoxApp* app = nullptr;
	RenderItem* p = nullptr;

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

		// Create Init Objects
		{
			Charactor = app->CreateDynamicGameObject("CharactorGeo", 1);
			Pinix = app->CreateDynamicGameObject("PinixGeo", 2);

			//app->ExtractAnimBones (
			//	std::string("D:\\Animation\\ImportAnimation\\Assets\\m\\Wisard"),
			//	std::string("test.fbx"),
			//	std::string("D:\\Modeling\\9c801715_Ganyu_HiPolySet_1.01\\Ganyu_HiPolySet_1.01"),
			//	std::string("test.pmx"),
			//	Charactor
			//);

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