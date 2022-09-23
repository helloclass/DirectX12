#pragma once

#include "BoxApp.h"

class LaserSkillEffect {
private:
	BoxApp* app = nullptr;

	RenderItem* Laser = nullptr;

public:
	LaserSkillEffect(BoxApp* mApp):app(mApp){}

	LaserSkillEffect() = delete;
	LaserSkillEffect(const LaserSkillEffect& rhs) = delete;
	LaserSkillEffect& operator=(const LaserSkillEffect& rhs) = delete;
	~LaserSkillEffect() = default;

public:
	DirectX::XMVECTOR mPosition;
	DirectX::XMVECTOR mRotation;
	bool isUpdate;

	void _Awake()
	{
		std::vector<std::string> MainLaserTexturePaths;

		isUpdate = false;

		// Create Init Objects
		Laser = app->CreateGameObject("LaserGeo", 1);
			
		app->GetData(Laser->mName)->mTranslate[0].position =
			{ 0.0f, -20.0f, 0.0f, 1.0f };

		{
			//{
			//	RenderItem* Sparks = new RenderItem("Sparks", 1);

			//	app->CreateBillBoardObject(
			//		"Sparks",
			//		"",
			//		"SKILL",
			//		Sparks,
			//		50,
			//		{ 0.0f, 0.0f, 0.0f },
			//		{ 2, 2 },
			//		ObjectData::RenderType::_SKILL_PUNCH_SPARKS_TYPE,
			//		false
			//	);

			//	Sparks->InitParticleSystem(
			//		1.0f,
			//		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			//		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			//		DirectX::XMFLOAT3(-35.0f, -35.0f, -35.0f),
			//		DirectX::XMFLOAT3(35.0f, 35.0f, 35.0f),
			//		true
			//	);
			//	Sparks->setIsFilled(false);

			//	Sparks->ParticleGene();

			//	Sparks->appendScaleAnimation(0.0f, { 8.0f, 0.5f, 0.5f });
			//	Sparks->appendScaleAnimation(3.0f, { 8.0f, 0.5f, 0.5f });

			//	Sparks->appendDiffuseAnimation(0.0f, { 0.8349056f, 0.9328136f, 1.0f, 0.5f });
			//	Sparks->appendDiffuseAnimation(1.0f, { 0.8349056f , 0.9328136f, 1.0f, 0.5f });

			//	Laser->appendChildRenderItem(Sparks);

			//	Texture Laser_Diffuse_Tex;
			//	Laser_Diffuse_Tex.Name = "Laser_Diffuse_Tex";
			//	Laser_Diffuse_Tex.Filename = L"../../Textures/Skill/ToonProject2/Point11.dds";

			//	app->uploadTexture(Laser_Diffuse_Tex, false);

			//	app->uploadMaterial(
			//		"SKILL_LASER_SPARKS_LONG_MATERIAL",
			//		"Laser_Diffuse_Tex",
			//		false
			//	);

			//	app->BindMaterial(
			//		Laser->getChildRenderItem("Sparks"),
			//		"SKILL_LASER_SPARKS_LONG_MATERIAL"
			//	);
			//}

			{
				RenderItem* Core = new RenderItem("Core", 1);

				app->CreateBillBoardObject(
					"Core",
					"",
					"SKILL",
					Core,
					3,
					{ 0.0f, 0.0f, 0.0f },
					{ 150, 150 },
					ObjectData::RenderType::_SKILL_PUNCH_SPARKS_TYPE,
					false
				);

				Core->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(-0.001f, -0.001f, -0.001f),
					DirectX::XMFLOAT3(0.001f, 0.001f, 0.001f),
					true
				);

				Core->ParticleGene();

				Core->appendScaleAnimation(0.0f, { 0.0f, 0.0f, 0.0f });
				Core->appendScaleAnimation(0.25f, { 70.0f, 70.0f, 70.0f });
				Core->appendScaleAnimation(1.5f, { 83.0f, 83.0f, 83.0f });
				Core->appendScaleAnimation(2.5f, { 0.0f, 0.0f, 0.0f });

				Core->appendDiffuseAnimation(0.0f, { 1.3915094f, 1.5277054f, 2.0f, 0.8f });
				Core->appendDiffuseAnimation(2.0f, { 1.3915094f, 1.5277054f, 2.0f, 0.6f });
				Core->appendDiffuseAnimation(3.0f, { 1.3915094f, 1.5277054f, 2.0f, 0.3f });

				Laser->appendChildRenderItem(Core);

				Texture Laser_Diffuse_Tex;
				Laser_Diffuse_Tex.Name = "Core_Diffuse_Tex";
				Laser_Diffuse_Tex.Filename = L"../../Textures/Skill/EpicToon/Point5.dds";

				app->uploadTexture(Laser_Diffuse_Tex, false);

				app->uploadMaterial(
					"SKILL_LASER_CORE_MATERIAL",
					"Core_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Laser->getChildRenderItem("Core"),
					"SKILL_LASER_CORE_MATERIAL"
				);
			}

			{
				RenderItem* Circles = new RenderItem("Circles", 1);

				app->CreateBillBoardObject(
					"Circles",
					"",
					"SKILL",
					Circles,
					3,
					{ 0.0f, 0.0f, 0.0f },
					{ 150, 150 },
					ObjectData::RenderType::_SKILL_PUNCH_SPARKS_TYPE,
					false
				);

				Circles->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(-0.001f, -0.001f, -0.001f),
					DirectX::XMFLOAT3(0.001f, 0.001f, 0.001f),
					true
				);

				Circles->ParticleGene();

				Circles->appendScaleAnimation(0.0f, { 0.0f, 0.0f, 0.0f });
				Circles->appendScaleAnimation(0.2f, { 82.0f, 82.0f, 82.0f });
				Circles->appendScaleAnimation(0.8f, { 100.0f, 100.0f, 100.0f });

				Circles->appendDiffuseAnimation(0.0f, { 0.3915094f, 0.5712811f, 1.0f, 0.5f });
				Circles->appendDiffuseAnimation(1.0f, { 0.8349056f, 0.9328136f, 1.0f, 0.0f });

				Laser->appendChildRenderItem(Circles);

				Texture Laser_Diffuse_Tex;
				Laser_Diffuse_Tex.Name = "Circles_Diffuse_Tex";
				Laser_Diffuse_Tex.Filename = L"../../Textures/Skill/EpicToon/Circle17.dds";

				app->uploadTexture(Laser_Diffuse_Tex, false);

				app->uploadMaterial(
					"SKILL_LASER_CIRCLES_MATERIAL",
					"Circles_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Laser->getChildRenderItem("Circles"),
					"SKILL_LASER_CIRCLES_MATERIAL"
				);
			}

			{
				RenderItem* Rays = new RenderItem("Ray", 1);

				app->CreateBillBoardObject(
					"RaysGeo",
					"",
					"SKILL",
					Rays,
					50,
					{ 0.0f, 0.0f, 0.0f },
					{ 20, 20 },
					ObjectData::RenderType::_SKILL_PUNCH_SPARKS_TYPE,
					false
				);

				Rays->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(-0.001f, -0.001f, -0.001f),
					DirectX::XMFLOAT3(0.001f, 0.001f, 0.001f),
					true
				);

				Rays->ParticleGene();

				Rays->appendScaleAnimation(0.0f, { 0.0f, 0.0f, 0.0f });
				Rays->appendScaleAnimation(0.5f, { 3.0f, 70.0f, 3.0f });
				Rays->appendScaleAnimation(3.0f, { 3.0f, 80.0f, 3.0f });

				Rays->appendDiffuseAnimation(0.0f, { 0.8783019f, 0.9830627f, 1.0f, 1.0f });
				Rays->appendDiffuseAnimation(0.5f, { 0.8783019f, 0.9830627f, 1.0f, 0.8f });
				Rays->appendDiffuseAnimation(1.0f, { 0.8783019f, 0.9830627f, 1.0f, 0.0f });

				Laser->appendChildRenderItem(Rays);

				Texture Laser_Diffuse_Tex;
				Laser_Diffuse_Tex.Name = "Rays_Diffuse_Tex";
				Laser_Diffuse_Tex.Filename = L"../../Textures/Skill/ToonProject2/Trail54.dds";

				app->uploadTexture(Laser_Diffuse_Tex, false);

				app->uploadMaterial(
					"SKILL_LASER_RAYS_LONG_MATERIAL",
					"Rays_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Laser->getChildRenderItem("Ray"),
					"SKILL_LASER_RAYS_LONG_MATERIAL"
				);
			}

			//{
			//	RenderItem* RayWaves = new RenderItem("RayWave", 5);

			//	app->CreateFBXObject(
			//		"RayWave",
			//		"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
			//		"CylinderFromCenter2.fbx",
			//		MainLaserTexturePaths,
			//		RayWaves,
			//		{ 0.0f, 0.0f, 0.0f },
			//		{ 90.0f, 0.0f, 0.0f },
			//		{ 20.0f, 20.0f, 20.0f },
			//		ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE
			//	);

			//	RayWaves->InitParticleSystem(
			//		1.0f,
			//		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			//		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			//		DirectX::XMFLOAT3(0.0f, 0.0f, 30.0f),
			//		DirectX::XMFLOAT3(0.0f, 0.0f, 200.0f),
			//		true
			//	);

			//	RayWaves->appendScaleAnimation(0.0f, { 0.0f, 0.0f, 0.0f });
			//	RayWaves->appendScaleAnimation(0.8f, { 50.0f, 200.0f, 50.0f });
			//	RayWaves->appendScaleAnimation(3.0f, { 50.0f, 200.0f, 50.0f });

			//	RayWaves->setIsUsedErrorScale({ 30.0f, 0.0f, 30.0f });

			//	RayWaves->appendDiffuseAnimation(0.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
			//	RayWaves->appendDiffuseAnimation(1.5f, { 1.0f, 1.0f, 1.0f, 1.0f });
			//	RayWaves->appendDiffuseAnimation(2.5f, { 1.0f, 1.0f, 1.0f, 0.0f });

			//	Laser->appendChildRenderItem(RayWaves);

				Texture Laser_Diffuse_Tex;
				Laser_Diffuse_Tex.Name = "Ray_Waves_Diffuse_Tex";
				Laser_Diffuse_Tex.Filename = L"../../Textures/White.dds";

				app->uploadTexture(Laser_Diffuse_Tex, false);

				Texture Laser_Mask_Tex;
				Laser_Mask_Tex.Name = "Ray_Waves_Mask_Tex";
				Laser_Mask_Tex.Filename = L"../../Textures/Skill/ToonProject2/Gradient11.dds";

				app->uploadTexture(Laser_Mask_Tex, false);

				Texture Laser_Noise_Tex;
				Laser_Noise_Tex.Name = "Ray_Waves_Noise_Tex";
				Laser_Noise_Tex.Filename = L"../../Textures/Skill/EpicToon/Noise51.dds";

				app->uploadTexture(Laser_Noise_Tex, false);

			//	app->uploadMaterial(
			//		"SKILL_LASER_RAY_WAVES_LONG_MATERIAL",
			//		"Rays_Diffuse_Tex",
			//		"Ray_Waves_Mask_Tex",
			//		"Ray_Waves_Noise_Tex",
			//		false
			//	);

			//	app->BindMaterial(
			//		Laser->getChildRenderItem("RayWave"),
			//		"SKILL_LASER_RAY_WAVES_LONG_MATERIAL"
			//	);
			//}

			{
				RenderItem* RayWaves2 = new RenderItem("RayWave2", 5);

				app->CreateFBXObject(
					"RayWave2",
					"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
					"CylinderFromCenter2.fbx",
					MainLaserTexturePaths,
					RayWaves2,
					{ 0.0f, 0.0f, 0.0f },
					{ 90.0f, 0.0f, 0.0f },
					/*{ 10.0f, 20.0f, 10.0f },*/
					{ 1.0f, 1.0f, 1.0f },
					ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE
				);

				RayWaves2->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 30.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 150.0f),
					true
				);

				RayWaves2->appendScaleAnimation(0.0f, { 0.0f, 20.0f, 0.0f });
				RayWaves2->appendScaleAnimation(3.0f, { 40.0f, 20.0f, 40.0f });

				RayWaves2->appendDiffuseAnimation(0.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
				RayWaves2->appendDiffuseAnimation(1.5f, { 1.0f, 1.0f, 1.0f, 1.0f });
				RayWaves2->appendDiffuseAnimation(2.5f, { 1.0f, 1.0f, 1.0f, 0.0f });

				Laser->appendChildRenderItem(RayWaves2);

				app->uploadMaterial(
					"SKILL_LASER_RAY_WAVES_2_LONG_MATERIAL",
					"Rays_Diffuse_Tex",
					"Ray_Waves_Mask_Tex",
					"Ray_Waves_Noise_Tex",
					false
				);

				app->BindMaterial(
					Laser->getChildRenderItem("RayWave2"),
					"SKILL_LASER_RAY_WAVES_2_LONG_MATERIAL"
				);
			}

			{
				RenderItem* LaserCore = new RenderItem("LaserCore", 1);

				app->CreateFBXObject(
					"LaserCore",
					"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
					"LaserCone1.fbx",
					MainLaserTexturePaths,
					LaserCore,
					{ 0.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 0.0f },
					{ 5.0f, 5.0f, 5.0f },
					ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE
				);

				LaserCore->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.1f, 0.0f),
					true
				);

				LaserCore->appendScaleAnimation(0.0f, { 0.0f, 0.0f, 0.0f });
				LaserCore->appendScaleAnimation(0.2f, { 20.0f, 20.0f, 100.0f });
				LaserCore->appendScaleAnimation(2.0f, { 20.0f, 20.0f, 100.0f });
				LaserCore->appendScaleAnimation(2.5f, { 0.0f, 0.0f, 0.0f });

				LaserCore->appendDiffuseAnimation(0.0f, { 1.7009434f, 1.952063f, 2.0f, 1.0f });
				LaserCore->appendDiffuseAnimation(3.0f, { 1.7009434f, 1.952063f, 2.0f, 1.0f });

				Laser->appendChildRenderItem(LaserCore);

				Texture Laser_Diffuse_Tex;
				Laser_Diffuse_Tex.Name = "LaserCore_Waves_Diffuse_Tex";
				Laser_Diffuse_Tex.Filename = L"../../Textures/Skill/EpicToon/Noise43.dds";

				app->uploadTexture(Laser_Diffuse_Tex, false);

				Texture Laser_Mask_Tex;
				Laser_Mask_Tex.Name = "LaserCore_Waves_Mask_Tex";
				Laser_Mask_Tex.Filename = L"../../Textures/Skill/ToonProject2/Gradient12.dds";

				app->uploadTexture(Laser_Mask_Tex, false);

				Texture Laser_Noise_Tex;
				Laser_Noise_Tex.Name = "LaserCore_Waves_Noise_Tex";
				Laser_Noise_Tex.Filename = L"../../Textures/Skill/EpicToon/Noise41.dds";

				app->uploadTexture(Laser_Noise_Tex, false);

				app->uploadMaterial(
					"SKILL_LASER_CORE_WAVES_LONG_MATERIAL",
					"LaserCore_Waves_Diffuse_Tex",
					"LaserCore_Waves_Mask_Tex",
					"LaserCore_Waves_Noise_Tex",
					false
				);

				app->BindMaterial(
					Laser->getChildRenderItem("LaserCore"),
					"SKILL_LASER_CORE_WAVES_LONG_MATERIAL"
				);
			}

			{
				RenderItem* LaserTrails = new RenderItem("LaserTrails", 1);

				app->CreateFBXObject(
					"LaserTrails",
					"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
					"KameCone.fbx",
					MainLaserTexturePaths,
					LaserTrails,
					{ 0.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 0.0f },
					{ 10.0f, 10.0f, 10.0f },
					ObjectData::RenderType::_SKILL_LASER_TRAILS_TYPE
				);

				LaserTrails->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.1f, 0.0f),
					true
				);

				LaserTrails->appendScaleAnimation(0.0f, { 13.0f, 13.0f, 0.0f });
				LaserTrails->appendScaleAnimation(0.5f, { 13.0f, 13.0f, 20.0f });

				LaserTrails->appendDiffuseAnimation(0.0f, { 0.259434f, 0.2996547f, 1.0f, 1.0f });
				LaserTrails->appendDiffuseAnimation(3.0f, { 0.259434f, 0.2996547f, 1.0f, 0.0f });

				Laser->appendChildRenderItem(LaserTrails);

				Texture Laser_Diffuse_Tex;
				Laser_Diffuse_Tex.Name = "LaserTrails_Waves_Diffuse_Tex";
				Laser_Diffuse_Tex.Filename = L"../../Textures/Skill/EpicToon/TrailPart9.dds";

				app->uploadTexture(Laser_Diffuse_Tex, false);

				app->uploadMaterial(
					"SKILL_LASER_TRAILS_WAVES_LONG_MATERIAL",
					"LaserTrails_Waves_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Laser->getChildRenderItem("LaserTrails"),
					"SKILL_LASER_TRAILS_WAVES_LONG_MATERIAL"
				);
			}
		}
	}

	float testestest = -1.0f;

	RenderItem* particle;
	RenderItem* particle1;
	RenderItem* particle2;
	RenderItem* particle3;
	RenderItem* particle4;
	RenderItem* particle5;
	RenderItem* particle6;
	RenderItem* particle7;

	void _Start()
	{
		//particle = Laser->getChildRenderItem("Sparks");
		particle1 = Laser->getChildRenderItem("Core");
		particle2 = Laser->getChildRenderItem("Circles");
		particle3 = Laser->getChildRenderItem("Ray");
		//particle4 = Laser->getChildRenderItem("RayWave");
		particle5 = Laser->getChildRenderItem("RayWave2");
		particle6 = Laser->getChildRenderItem("LaserCore");
		particle7 = Laser->getChildRenderItem("LaserTrails");

		app->mLaserBox.Center = { 
			0.0f, 
			-1000.0f, 
			0.0f 
		};
		app->mLaserBox.Extents = {
			10.0f, 
			10.0f, 
			10.0f 
		};
	}

	void _Update(const GameTimer& gt)
	{
		if (isUpdate)
		{
			if (testestest < 0.0f)
			{
				testestest = 0.0f;

				app->GetData(Laser->mName)->mTranslate[0].position = mPosition;
				app->GetData(Laser->mName)->mTranslate[0].rotation = mRotation;

				app->mLaserBox.Center = {
					mPosition.m128_f32[0] * 10.0f,
					mPosition.m128_f32[1] * 10.0f,
					mPosition.m128_f32[2] * 10.0f
				};

				//particle->ParticleReset();
				particle2->ParticleReset();
				particle3->ParticleReset();
				//particle4->ParticleReset();
				particle5->ParticleReset();
				particle6->ParticleReset();
				particle7->ParticleReset();
			}
			else if (testestest > 3.0f)
			{
				isUpdate = false;
				testestest = -1.0f;

				app->GetData(Laser->mName)->mTranslate[0].position =
				{ 0.0f, -20.0f, 0.0f, 1.0f };

				app->mLaserBox.Center = {
					0.0f,
					-1000.0f,
					0.0f
				};

				//particle->ParticleReset();
				particle2->ParticleReset();
				particle3->ParticleReset();
				//particle4->ParticleReset();
				particle5->ParticleReset();
				particle6->ParticleReset();
				particle7->ParticleReset();
			}

			//particle->ParticleUpdate(gt.DeltaTime(), testestest);
			particle1->ParticleUpdate(gt.DeltaTime(), testestest);
			particle2->ParticleUpdate(gt.DeltaTime(), testestest);
			particle3->ParticleUpdate(gt.DeltaTime(), testestest);
			//particle4->ParticleUpdate(gt.DeltaTime(), testestest);
			particle5->ParticleUpdate(gt.DeltaTime(), testestest);
			particle6->ParticleUpdate(gt.DeltaTime(), testestest);
			particle7->ParticleUpdate(gt.DeltaTime(), testestest);

			testestest += gt.DeltaTime();
		}
	}

	void _End()
	{

	}
};