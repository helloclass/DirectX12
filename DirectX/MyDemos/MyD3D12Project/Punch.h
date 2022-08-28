#pragma once

#include "BoxApp.h"

class PunchSkillEffect {
private:
	BoxApp* app = nullptr;

	RenderItem* Punch = nullptr;

public:
	PunchSkillEffect(BoxApp* mApp):app(mApp){}

	PunchSkillEffect() = delete;
	PunchSkillEffect(const PunchSkillEffect& rhs) = delete;
	PunchSkillEffect& operator=(const PunchSkillEffect& rhs) = delete;
	~PunchSkillEffect() = default;

public:
	DirectX::XMVECTOR mPosition;
	DirectX::XMVECTOR mRotation;
	bool isUpdate;

	void _Awake()
	{
		std::vector<std::string> MainPunchTexturePaths;

		isUpdate = false;

		// Create Init Objects
		Punch = app->CreateGameObject("PunchGeo", 1);

		app->GetData(Punch->mName)->mTranslate[0].position =
			{ 0.0f, -20.0f, 0.0f, 1.0f };

		{
			{
				RenderItem* Fire = new RenderItem("Fire", 1);

				app->CreateBillBoardObject(
					"FireGeo",
					"",
					"SKILL",
					Fire,
					40,
					{ 0.0f, 0.0f, 0.0f },
					{ 80, 80 },
					ObjectData::RenderType::_SKILL_TONADO_SPLASH_3_TYPE,
					false
				);

				Fire->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(-6.0f, -6.0f, -6.0f),
					DirectX::XMFLOAT3(6.0f, 6.0f, 6.0f),
					true
				);

				Fire->ParticleGene();

				Fire->setTextureSheetAnimationXY(4, 4);
				Fire->setTextureSheetAnimationFrame(1.0f / 16.0f);

				Fire->appendScaleAnimation(0.0f, { 80.0f, 80.0f, 80.0f });
				Fire->appendScaleAnimation(0.5f, { 0.0f, 0.0f, 0.0f });
				Fire->appendScaleAnimation(3.0f, { 0.0f, 0.0f, 0.0f });

				Fire->appendDiffuseAnimation(0.0f, { 0.0f, 0.0f, 0.0f, 0.0f });
				Fire->appendDiffuseAnimation(0.25f, { 0.8f, 0.8f, 0.8f, 0.8f });
				Fire->appendDiffuseAnimation(0.70f, { 1.0f, 1.0f, 1.0f, 1.0f });
				Fire->appendDiffuseAnimation(0.90f, { 0.5019608f, 0.1733903f, 0.0f, 0.8f });
				Fire->appendDiffuseAnimation(1.0f, { 0.0f, 0.0f, 0.0f, 0.0f });
				Fire->appendDiffuseAnimation(3.0f, { 0.0f, 0.0f, 0.0f, 0.0f });

				Punch->appendChildRenderItem(Fire);

				Texture Fire_Diffuse_Tex;
				Fire_Diffuse_Tex.Name = "Fire_Diffuse_Tex";
				Fire_Diffuse_Tex.Filename = L"../../Textures/Skill/EpicToon/SS1.dds";

				app->uploadTexture(Fire_Diffuse_Tex, false);

				app->uploadMaterial(
					"SKILL_PUNCH_FIRE_MATERIAL",
					"Fire_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Punch->getChildRenderItem("Fire"),
					"SKILL_PUNCH_FIRE_MATERIAL"
				);
			}

			{
				RenderItem* DarkSmoke = new RenderItem("DarkSmoke", 1);

				app->CreateBillBoardObject(
					"DarkSmokeGeo",
					"",
					"SKILL",
					DarkSmoke,
					20,
					{ 0.0f, 0.0f, 0.0f },
					{ 80, 80 },
					ObjectData::RenderType::_SKILL_TONADO_SPLASH_3_TYPE,
					false
				);

				DarkSmoke->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 30.0f, 25.0f),
					DirectX::XMFLOAT3(0.0f, 50.0f, 150.0f),
					DirectX::XMFLOAT3(-5.0f, -3.0f, -140.0f),
					DirectX::XMFLOAT3(5.0f, 5.0f, -100.0f),
					false
				);

				DarkSmoke->ParticleGene();

				DarkSmoke->setTextureSheetAnimationXY(4, 4);
				DarkSmoke->setTextureSheetAnimationFrame(1.0f / 16.0f);

				DarkSmoke->appendScaleAnimation(0.0f, { 0.0f, 0.0f, 0.0f });
				DarkSmoke->appendScaleAnimation(1.0f, { 80.0f, 80.0f, 80.0f });

				DarkSmoke->appendDiffuseAnimation(0.0f, { 0.6830189f, 0.36783684f, 0.0f, 1.0f });
				DarkSmoke->appendDiffuseAnimation(0.5f, { 0.2830189f, 0.06783684f, 0.0f, 1.0f });
				DarkSmoke->appendDiffuseAnimation(3.0f, { 0.0f, 0.0f, 0.0f, 0.0f });

				Punch->appendChildRenderItem(DarkSmoke);

				app->uploadMaterial(
					"SKILL_PUNCH_DARK_SMOKE_MATERIAL",
					"Fire_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Punch->getChildRenderItem("DarkSmoke"),
					"SKILL_PUNCH_DARK_SMOKE_MATERIAL"
				);
			}

			{
				RenderItem* Sparks = new RenderItem("Sparks", 1);

				app->CreateBillBoardObject(
					"SparksGeo",
					"",
					"SKILL",
					Sparks,
					150,
					{ 0.0f, 0.0f, 0.0f },
					{ 2, 2 },
					ObjectData::RenderType::_SKILL_PUNCH_SPARKS_TYPE,
					false
				);

				Sparks->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(-40.0f, -40.0f, -40.0f),
					DirectX::XMFLOAT3(40.0f, 40.0f, 40.0f),
					true
				);

				Sparks->ParticleGene();

				Sparks->appendDiffuseAnimation(0.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
				Sparks->appendDiffuseAnimation(1.0f, { 1.0f, 1.0f, 1.0f, 0.0f });
				Sparks->appendDiffuseAnimation(3.0f, { 1.0f, 1.0f, 1.0f, 0.0f });

				Punch->appendChildRenderItem(Sparks);

				Texture Sparks_Diffuse_Tex;
				Sparks_Diffuse_Tex.Name = "Sparks_Diffuse_Tex";
				Sparks_Diffuse_Tex.Filename = L"../../Textures/Skill/ToonProject2/Point12.dds";

				app->uploadTexture(Sparks_Diffuse_Tex, false);

				app->uploadMaterial(
					"SKILL_PUNCH_SPARKS_MATERIAL",
					"Sparks_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Punch->getChildRenderItem("Sparks"),
					"SKILL_PUNCH_SPARKS_MATERIAL"
				);
			}

			{
				RenderItem* SparksLong = new RenderItem("SparksLong", 1);

				app->CreateBillBoardObject(
					"SparksLongGeo",
					"",
					"SKILL",
					SparksLong,
					50,
					{ 0.0f, 0.0f, 0.0f },
					{ 2, 2 },
					ObjectData::RenderType::_SKILL_PUNCH_SPARKS_TYPE,
					false
				);

				SparksLong->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(-15.0f, -15.0f, 40.0f),
					DirectX::XMFLOAT3(15.0f, 15.0f, 50.0f),
					true
				);

				SparksLong->ParticleGene();

				SparksLong->appendScaleAnimation(0.0f, {1.0f, 1.0f, 1.0f});
				SparksLong->appendScaleAnimation(0.3f, { 1.0f, 1.0f, 40.0f });
				SparksLong->appendScaleAnimation(1.0f, { 1.0f, 1.0f, 1.0f });

				SparksLong->appendDiffuseAnimation(0.0f, { 0.854902f, 0.9241238f, 1.0f, 0.5f });
				SparksLong->appendDiffuseAnimation(1.0f, { 0.854902f, 0.9241238f, 1.0f, 0.0f });

				Punch->appendChildRenderItem(SparksLong);

				app->uploadMaterial(
					"SKILL_PUNCH_SPARKS_LONG_MATERIAL",
					"Sparks_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Punch->getChildRenderItem("SparksLong"),
					"SKILL_PUNCH_SPARKS_LONG_MATERIAL"
				);
			}

			{
				RenderItem* Glow = new RenderItem("Glow", 1);

				app->CreateBillBoardObject(
					"GlowGeo",
					"",
					"SKILL",
					Glow,
					3,
					{ 0.0f, 0.0f, 0.0f },
					{ 100, 100 },
					ObjectData::RenderType::_SKILL_PUNCH_SPARKS_TYPE,
					false
				);

				Glow->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.01f, 0.0f),
					true
				);

				Glow->ParticleGene();

				Glow->appendScaleAnimation(0.0f, { 0.0f, 0.0f, 0.0f });
				Glow->appendScaleAnimation(0.2f, { 300.0f, 300.0f, 300.0f });
				Glow->appendScaleAnimation(0.6f, { 0.0f, 0.0f, 0.0f });
				Glow->appendScaleAnimation(3.0f, { 0.0f, 0.0f, 0.0f });

				Glow->appendDiffuseAnimation(0.0f, { 1.0f, 0.5917661f, 1.0f, 0.0f });
				Glow->appendDiffuseAnimation(0.2f, { 1.0f, 0.5917661f, 1.0f, 0.6f });
				Glow->appendDiffuseAnimation(1.0f, { 1.0f, 0.5917661f, 0.0f, 0.4f });
				Glow->appendDiffuseAnimation(3.0f, { 1.0f, 0.5917661f, 0.0f, 0.0f });

				Punch->appendChildRenderItem(Glow);

				app->uploadMaterial(
					"SKILL_PUNCH_GLOW_MATERIAL",
					"Sparks_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Punch->getChildRenderItem("Glow"),
					"SKILL_PUNCH_GLOW_MATERIAL"
				);
			}

			{
				RenderItem* EndlDesbis = new RenderItem("EndlDesbis", 1);

				app->CreateBillBoardObject(
					"EndlDesbisGeo",
					"",
					"SKILL",
					EndlDesbis,
					50,
					{ 0.0f, 0.0f, 0.0f },
					{ 0.5f, 0.5f },
					ObjectData::RenderType::_SKILL_PUNCH_ENDL_DESBIS_TYPE,
					false
				);

				EndlDesbis->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, -40.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, -40.0f, 0.0f),
					DirectX::XMFLOAT3(-20.0f, 10.0f, -20.0f),
					DirectX::XMFLOAT3(20.0f, 20.0f, 20.0f),
					true
				);

				EndlDesbis->setTextureSheetAnimationXY(2, 2);
				EndlDesbis->setTextureSheetAnimationFrame(1.0f / 16.0f);

				EndlDesbis->ParticleGene();

				EndlDesbis->appendDiffuseAnimation(0.0f, { 0.0f, 0.0f, 0.0f, 1.0f });
				EndlDesbis->appendDiffuseAnimation(1.0f, { 0.0f, 0.0f, 0.0f, 1.0f });

				Punch->appendChildRenderItem(EndlDesbis);

				Texture EndlDesbis_Diffuse_Tex;
				EndlDesbis_Diffuse_Tex.Name = "EndlDesbis_Diffuse_Tex";
				EndlDesbis_Diffuse_Tex.Filename = L"../../Textures/Skill/ToonProject2/Snow4.dds";

				app->uploadTexture(EndlDesbis_Diffuse_Tex, false);

				app->uploadMaterial(
					"SKILL_PUNCH_EndlDesbis_MATERIAL",
					"EndlDesbis_Diffuse_Tex",
					false
				);

				app->BindMaterial(
					Punch->getChildRenderItem("EndlDesbis"),
					"SKILL_PUNCH_EndlDesbis_MATERIAL"
				);
			}

			{
				RenderItem* ShockWave = new RenderItem("ShockWave", 1);

				app->CreateFBXObject(
					"ShockWaveGeo",
					"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
					"CylinderFromCenter2.fbx",
					MainPunchTexturePaths,
					ShockWave,
					{ 0.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 0.0f },
					{ 8.0f, 4.0f, 8.0f },
					ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE
				);

				ShockWave->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.01f, 0.0f),
					true
				);

				ShockWave->appendScaleAnimation(0.0f, DirectX::XMFLOAT3(0.0f, 3.0f, 0.0f));
				ShockWave->appendScaleAnimation(1.0f, DirectX::XMFLOAT3(50.0f, 3.0f, 50.0f));

				ShockWave->appendDiffuseAnimation(0.0f, { 0.8066038f, 0.8884997f, 1.0f, 1.0f });

				Punch->appendChildRenderItem(ShockWave);

				Texture ShockWave_Diffuse_Tex;
				ShockWave_Diffuse_Tex.Name = "ShockWave_5_Diffuse_Tex";
				ShockWave_Diffuse_Tex.Filename = L"../../Textures/White.dds";

				Texture ShockWave_Mask_Tex;
				ShockWave_Mask_Tex.Name = "ShockWave_5_Mask_Tex";
				ShockWave_Mask_Tex.Filename = L"../../Textures/Skill/ToonProject2/Gradient11.dds";

				Texture ShockWave_Noise_Tex;
				ShockWave_Noise_Tex.Name = "ShockWave_5_Noise_Tex";
				ShockWave_Noise_Tex.Filename = L"../../Textures/Skill/EpicToon/Noise51.dds";

				app->uploadTexture(ShockWave_Diffuse_Tex, false);
				app->uploadTexture(ShockWave_Mask_Tex, false);
				app->uploadTexture(ShockWave_Noise_Tex, false);

				app->uploadMaterial(
					"SKILL_PUNCH_SHOCK_WAVE_MATERIAL",
					"ShockWave_5_Diffuse_Tex",
					"ShockWave_5_Mask_Tex",
					"ShockWave_5_Noise_Tex",
					false,
					{ 1.0f, 1.0f },
					{ 1.0f, 1.0f, 1.0f, 1.0f }
				);

				app->BindMaterial(
					Punch->getChildRenderItem("ShockWave"),
					"SKILL_PUNCH_SHOCK_WAVE_MATERIAL",
					false
				);
			}

			{
				RenderItem* ShockWave2 = new RenderItem("ShockWave2", 1);

				app->CreateFBXObject(
					"ShockWave2Geo",
					"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
					"CylinderFromCenter2.fbx",
					MainPunchTexturePaths,
					ShockWave2,
					{ -10.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 90.0f },
					{ 8.0f, 4.0f, 8.0f },
					ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE
				);

				ShockWave2->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.01f, 0.0f),
					true
				);

				ShockWave2->appendScaleAnimation(0.0f, DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f));
				ShockWave2->appendScaleAnimation(0.4f, DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f));
				ShockWave2->appendScaleAnimation(0.6f, DirectX::XMFLOAT3(10.0f, 2.0f, 10.0f));
				ShockWave2->appendScaleAnimation(1.0f, DirectX::XMFLOAT3(12.0f, 2.0f, 12.0f));

				ShockWave2->appendDiffuseAnimation(0.0f, { 0.9f, 0.8f, 0.0f, 1.0f });
				ShockWave2->appendDiffuseAnimation(1.0f, { 0.9f, 0.8f, 0.0f, 1.0f });

				Punch->appendChildRenderItem(ShockWave2);

				app->uploadMaterial(
					"SKILL_PUNCH_SHOCK_WAVE_2_MATERIAL",
					"ShockWave_5_Diffuse_Tex",
					"ShockWave_5_Mask_Tex",
					"ShockWave_5_Noise_Tex",
					false,
					{ 1.0f, 1.0f },
					{ 1.0f, 1.0f, 1.0f, 1.0f }
				);

				app->BindMaterial(
					Punch->getChildRenderItem("ShockWave2"),
					"SKILL_PUNCH_SHOCK_WAVE_2_MATERIAL",
					false
				);
			}

			{
				RenderItem* ShockWave3 = new RenderItem("ShockWave3", 1);

				app->CreateFBXObject(
					"ShockWave3Geo",
					"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
					"CylinderFromCenter2.fbx",
					MainPunchTexturePaths,
					ShockWave3,
					{ -13.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 90.0f },
					{ 1.0f, 2.0f, 1.0f },
					ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE
				);

				ShockWave3->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.01f, 0.0f),
					true
				);

				ShockWave3->appendScaleAnimation(0.0f, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
				ShockWave3->appendScaleAnimation(0.4f, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
				ShockWave3->appendScaleAnimation(0.6f, DirectX::XMFLOAT3(8.0f, 2.0f, 8.0f));
				ShockWave3->appendScaleAnimation(1.0f, DirectX::XMFLOAT3(10.0f, 2.0f, 10.0f));

				ShockWave3->appendDiffuseAnimation(0.0f, { 1.0f, 0.8f, 0.0f, 1.0f });
				ShockWave3->appendDiffuseAnimation(1.0f, { 1.0f, 0.8f, 0.0f, 1.0f });

				Punch->appendChildRenderItem(ShockWave3);

				app->uploadMaterial(
					"SKILL_PUNCH_SHOCK_WAVE_3_MATERIAL",
					"ShockWave_5_Diffuse_Tex",
					"ShockWave_5_Mask_Tex",
					"ShockWave_5_Noise_Tex",
					false,
					{ 1.0f, 1.0f },
					{ 1.0f, 1.0f, 1.0f, 1.0f }
				);

				app->BindMaterial(
					Punch->getChildRenderItem("ShockWave3"),
					"SKILL_PUNCH_SHOCK_WAVE_3_MATERIAL",
					false
				);
			}

			{
				RenderItem* RayWaves = new RenderItem("RayWave", 5);

				app->CreateFBXObject(
					"RayWave",
					"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
					"CylinderFromCenter2.fbx",
					MainPunchTexturePaths,
					RayWaves,
					{ 0.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 0.0f },
					{ 20.0f, 20.0f, 20.0f },
					ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE
				);

				RayWaves->InitParticleSystem(
					1.0f,
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 30.0f),
					DirectX::XMFLOAT3(0.0f, 0.0f, 200.0f),
					true
				);

				RayWaves->appendScaleAnimation(0.0f, { 0.0f, 0.0f, 0.0f });
				RayWaves->appendScaleAnimation(0.8f, { 150.0f, 30.0f, 150.0f });
				RayWaves->appendScaleAnimation(3.0f, { 200.0f, 50.0f, 200.0f });

				RayWaves->appendDiffuseAnimation(0.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
				RayWaves->appendDiffuseAnimation(1.5f, { 1.0f, 1.0f, 1.0f, 1.0f });
				RayWaves->appendDiffuseAnimation(2.5f, { 1.0f, 1.0f, 1.0f, 0.0f });

				Punch->appendChildRenderItem(RayWaves);

				Texture Laser_Diffuse_Tex;
				Laser_Diffuse_Tex.Name = "Rays_Diffuse_Tex";
				Laser_Diffuse_Tex.Filename = L"../../Textures/Skill/ToonProject2/Trail54.dds";

				app->uploadTexture(Laser_Diffuse_Tex, false);

				Texture Laser_Mask_Tex;
				Laser_Mask_Tex.Name = "Ray_Waves_Mask_Tex";
				Laser_Mask_Tex.Filename = L"../../Textures/Skill/ToonProject2/Gradient11.dds";

				app->uploadTexture(Laser_Mask_Tex, false);

				Texture Laser_Noise_Tex;
				Laser_Noise_Tex.Name = "Ray_Waves_Noise_Tex";
				Laser_Noise_Tex.Filename = L"../../Textures/Skill/EpicToon/Noise51.dds";

				app->uploadTexture(Laser_Noise_Tex, false);

				app->uploadMaterial(
					"SKILL_LASER_RAY_WAVES_LONG_MATERIAL",
					"Rays_Diffuse_Tex",
					"Ray_Waves_Mask_Tex",
					"Ray_Waves_Noise_Tex",
					false
				);

				app->BindMaterial(
					Punch->getChildRenderItem("RayWave"),
					"SKILL_LASER_RAY_WAVES_LONG_MATERIAL"
				);
			}
		}
	}

	float testestest = 0.0f;

	RenderItem* particle  = nullptr;
	RenderItem* particle1 = nullptr;
	RenderItem* particle2 = nullptr;
	RenderItem* particle3 = nullptr;
	RenderItem* particle4 = nullptr;
	RenderItem* particle5 = nullptr;
	RenderItem* particle6 = nullptr;
	RenderItem* particle7 = nullptr;
	RenderItem* particle8 = nullptr;
	RenderItem* particle9 = nullptr;
	RenderItem* particle10 = nullptr;

	void _Start()
	{
		particle = Punch->getChildRenderItem("Fire");
		particle1 = Punch->getChildRenderItem("DarkSmoke");
		particle2 = Punch->getChildRenderItem("Sparks");
		particle3 = Punch->getChildRenderItem("SparksLong");
		particle4 = Punch->getChildRenderItem("Glow");
		particle5 = Punch->getChildRenderItem("EndlDesbis");
		particle6 = Punch->getChildRenderItem("ShockWave");
		particle7 = Punch->getChildRenderItem("ShockWave2");
		particle8 = Punch->getChildRenderItem("ShockWave3");
		particle9 = Punch->getChildRenderItem("RayWave");
	}

	void _Update(const GameTimer& gt)
	{
		if (isUpdate)
		{
			if (testestest == 0.0f)
			{
				app->GetData(Punch->mName)->mTranslate[0].position = mPosition;
				app->GetData(Punch->mName)->mTranslate[0].rotation = mRotation;

				particle->ParticleReset();
				particle2->ParticleReset();
				particle3->ParticleReset();
				particle4->ParticleReset();
				particle5->ParticleReset();
				particle6->ParticleReset();
				particle7->ParticleReset();
				particle9->ParticleReset();
			}
			else if (testestest > 1.0f)
			{
				isUpdate = false;
				testestest = 0.0f;

				app->GetData(Punch->mName)->mTranslate[0].position =
					{ 0.0f, -20.0f, 0.0f, 1.0f };

				particle->ParticleReset();
				particle2->ParticleReset();
				particle3->ParticleReset();
				particle4->ParticleReset();
				particle5->ParticleReset();
				particle6->ParticleReset();
				particle7->ParticleReset();
				particle9->ParticleReset();
			}

			particle->ParticleUpdate(gt.DeltaTime(), testestest);
			particle1->ParticleUpdate(gt.DeltaTime(), testestest);
			particle2->ParticleUpdate(gt.DeltaTime(), testestest);
			particle3->ParticleUpdate(gt.DeltaTime(), testestest);
			particle4->ParticleUpdate(gt.DeltaTime(), testestest);
			particle5->ParticleUpdate(gt.DeltaTime(), testestest);
			particle6->ParticleUpdate(gt.DeltaTime(), testestest);
			particle7->ParticleUpdate(gt.DeltaTime(), testestest);
			particle8->ParticleUpdate(gt.DeltaTime(), testestest);
			particle9->ParticleUpdate(gt.DeltaTime(), testestest);

			testestest += gt.DeltaTime();
		}
	}

	void _End()
	{

	}
};