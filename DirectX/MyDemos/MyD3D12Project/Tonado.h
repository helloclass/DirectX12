#pragma once

#include "BoxApp.h"

class TonadoSkillEffect {
private:
	BoxApp* app = nullptr;

	RenderItem* Tonado = nullptr;

public:
	TonadoSkillEffect(BoxApp* mApp):app(mApp){}

	TonadoSkillEffect() = delete;
	TonadoSkillEffect(const TonadoSkillEffect& rhs) = delete;
	TonadoSkillEffect& operator=(const TonadoSkillEffect& rhs) = delete;
	~TonadoSkillEffect() = default;

public:
	DirectX::XMVECTOR mPosition;
	DirectX::XMVECTOR mRotation;
	bool isUpdate;

	void _Awake()
	{
		std::vector<std::string> MainTonadoTexturePaths;

		isUpdate = false;

		// Create Init Objects
		Tonado = app->CreateGameObject("TonadoGeo", 1);
		Tonado->setIsBaked(true);

		app->GetData(Tonado->mName)->mTranslate[0].position = 
			{ 0.0f, -20.0f, 0.0f, 1.0f };

		// ½ºÅ³ ÀÌÆåÆ®
		{
			{
				RenderItem* StartSplash = new RenderItem("StartSplash", 1);

				app->CreateBillBoardObject(
					"StartSplash",
					"",
					"SKILL",
					StartSplash,
					50,
					{ 0.0f, 0.0f, 0.0f },
					{ 10, 10 },
					ObjectData::RenderType::_SKILL_TONADO_SPLASH_TYPE,
					false
				);

				//StartSplash->setIsBaked(true);
				StartSplash->setIsDrawShadow(false);

				StartSplash->InitParticleSystem(
					10.0f,
					DirectX::XMFLOAT3(0.0f, -20.0f, 0.0f),
					DirectX::XMFLOAT3(0.0f, -20.0f, 0.0f),
					DirectX::XMFLOAT3(-12.5f, 10.0f, -12.5f),
					DirectX::XMFLOAT3(12.5f, 60.0f, 12.5f),
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
				"StartSplash2",
				"",
				"SKILL",
				StartSplash2,
				30,
				{ 0.0f, 0.0f, 0.0f },
				{ 10, 10 },
				ObjectData::RenderType::_SKILL_TONADO_SPLASH_TYPE,
				false
			);

			//StartSplash2->setIsBaked(true);
			StartSplash2->setIsDrawShadow(false);

			StartSplash2->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, -40.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, -40.0f, 0.0f),
				DirectX::XMFLOAT3(-30.0f, 15.0f, -30.0f),
				DirectX::XMFLOAT3(30.0f, 20.0f, 30.0f),
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
				"StartSplash3",
				"",
				"SKILL",
				StartSplash3,
				16,
				{ 0.0f, 0.0f, 0.0f },
				{ 20, 5 },
				ObjectData::RenderType::_SKILL_TONADO_SPLASH_3_TYPE,
				false
			);

			//StartSplash3->setIsBaked(true);
			StartSplash3->setIsDrawShadow(false);

			StartSplash3->setIsFilled(false);
			StartSplash3->setTextureSheetAnimationXY(7, 2);
			StartSplash3->setTextureSheetAnimationFrame(1.0f / 14.0f);

			StartSplash3->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, -10.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, -5.0f, 0.0f),
				DirectX::XMFLOAT3(-20.0f, 30.0f, -20.0f),
				DirectX::XMFLOAT3(20.0f, 30.0f, 20.0f),
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

		{
			RenderItem* StartSplash4 = new RenderItem("StartSplash4", 1);

			app->CreateFBXObject(
				"StartSplash4",
				"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
				"CylindricalCone.fbx",
				MainTonadoTexturePaths,
				StartSplash4,
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 20.0f, 100.0f, 20.0f },
				ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE,
				false
			);

			//StartSplash4->setIsBaked(true);
			StartSplash4->setIsDrawShadow(false);

			StartSplash4->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.01f, 0.0f),
				true
			);

			StartSplash4->appendScaleAnimation(0.0f, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
			StartSplash4->appendScaleAnimation(0.15f, DirectX::XMFLOAT3(0.7f * 4.0f, 0.7f * 100.0f, 0.7f * 4.0f));
			StartSplash4->appendScaleAnimation(0.3f , DirectX::XMFLOAT3(1.5f * 5.0f, 1.5f * 100.0f, 1.5f * 5.0f));
			StartSplash4->appendScaleAnimation(0.5f , DirectX::XMFLOAT3(1.2f * 5.0f, 1.2f * 100.0f, 1.2f * 5.0f));

			StartSplash4->appendDiffuseAnimation(0.0f, { 0.0f, 0.0f, 1.0f, 1.0f });
			StartSplash4->appendDiffuseAnimation(0.3f, { 0.7019608f, 0.905909f * 1.6f, 2.0f, 1.0f});

			Tonado->appendChildRenderItem(StartSplash4);

			Texture StartSplash_Diffuse_Tex;
			StartSplash_Diffuse_Tex.Name = "StartSplash_4_Diffuse_Tex";
			StartSplash_Diffuse_Tex.Filename = L"../../Textures/Skill/EpicToon/WaterF2.dds";

			Texture StartSplash_Mask_Tex;
			StartSplash_Mask_Tex.Name = "StartSplash_4_Mask_Tex";
			StartSplash_Mask_Tex.Filename = L"../../Textures/Skill/ToonProject2/Gradient4.3.dds";

			Texture StartSplash_Noise_Tex;
			StartSplash_Noise_Tex.Name = "StartSplash_4_Noise_Tex";
			StartSplash_Noise_Tex.Filename = L"../../Textures/Skill/EpicToon/Noise43.dds";

			app->uploadTexture(StartSplash_Diffuse_Tex, false);
			app->uploadTexture(StartSplash_Mask_Tex, false);
			app->uploadTexture(StartSplash_Noise_Tex, false);

			app->uploadMaterial(
				"SKILL_TONADO_SPLASH_4_MATERIAL",
				"StartSplash_4_Diffuse_Tex",
				"StartSplash_4_Mask_Tex",
				"StartSplash_4_Noise_Tex",
				false,
				{ 1.0f, 6.0f },
				{ 0.7019608f, 0.905909f * 1.6f, 1.5f, 1.0f }
			);

			app->BindMaterial(
				Tonado->getChildRenderItem("StartSplash4"),
				"SKILL_TONADO_SPLASH_4_MATERIAL",
				false
			);
		}

		{
			RenderItem* StartSplash5 = new RenderItem("StartSplash5", 1);

			app->CreateFBXObject(
				"StartSplash5",
				"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
				"CylindricalCone.fbx",
				MainTonadoTexturePaths,
				StartSplash5,
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 4.5f, 60.0f, 4.5f },
				ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE,
				false
			);

			//StartSplash5->setIsBaked(true);
			StartSplash5->setIsDrawShadow(false);

			StartSplash5->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.01f, 0.0f),
				true
			);

			StartSplash5->appendScaleAnimation(0.0f, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
			StartSplash5->appendScaleAnimation(0.15f, DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f));
			StartSplash5->appendScaleAnimation(0.3f, DirectX::XMFLOAT3(1.2f, 1.2f, 1.2f));
			StartSplash5->appendScaleAnimation(0.5f, DirectX::XMFLOAT3(0.8f, 0.8f, 0.8f));

			StartSplash5->appendDiffuseAnimation(0.0f, { 0.0f, 0.0f, 1.0f, 1.0f });
			StartSplash5->appendDiffuseAnimation(0.3f, { 0.7019608f, 0.905909f * 1.6f, 2.0f, 1.0f });

			Tonado->appendChildRenderItem(StartSplash5);

			Texture StartSplash_Mask_Tex;
			StartSplash_Mask_Tex.Name = "StartSplash_5_Mask_Tex";
			StartSplash_Mask_Tex.Filename = L"../../Textures/Skill/ToonProject2/Gradient12.dds";

			Texture StartSplash_Noise_Tex;
			StartSplash_Noise_Tex.Name = "StartSplash_5_Noise_Tex";
			StartSplash_Noise_Tex.Filename = L"../../Textures/Skill/EpicToon/Noise53.dds";

			app->uploadTexture(StartSplash_Mask_Tex, false);
			app->uploadTexture(StartSplash_Noise_Tex, false);

			app->uploadMaterial(
				"SKILL_TONADO_SPLASH_5_MATERIAL",
				"StartSplash_4_Diffuse_Tex",
				"StartSplash_5_Mask_Tex",
				"StartSplash_5_Noise_Tex",
				false,
				{ 1.0f, 6.0f },
				{ 0.7019608f, 0.905909f * 1.6f, 1.5f, 1.0f }
			);

			app->BindMaterial(
				Tonado->getChildRenderItem("StartSplash5"),
				"SKILL_TONADO_SPLASH_5_MATERIAL",
				false
			);
		}

		{
			RenderItem* StartSplash6 = new RenderItem("StartSplash6", 1);

			app->CreateBillBoardObject(
				"StartSplash6",
				"",
				"SKILL",
				StartSplash6,
				80,
				{ 0.0f, 0.0f, 0.0f },
				{ 1, 1 },
				ObjectData::RenderType::_SKILL_TONADO_SPLASH_3_TYPE,
				false
			);

			//StartSplash6->setIsBaked(true);
			StartSplash6->setIsDrawShadow(false);

			StartSplash6->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, -50.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, -50.0f, 0.0f),
				DirectX::XMFLOAT3(-30.0f, 20.0f, -30.0f),
				DirectX::XMFLOAT3(30.0f, 50.0f, 30.0f),
				true
			);

			StartSplash6->ParticleGene();

			Tonado->appendChildRenderItem(StartSplash6);

			Texture StartSplash_Diffuse_Tex;
			StartSplash_Diffuse_Tex.Name = "StartSplash_6_Diffuse_Tex";
			StartSplash_Diffuse_Tex.Filename = L"../../Textures/Skill/EpicToon/Mask9.dds";

			app->uploadTexture(StartSplash_Diffuse_Tex, false);

			app->uploadMaterial(
				"SKILL_TONADO_SPLASH_6_MATERIAL",
				"StartSplash_6_Diffuse_Tex",
				false
			);

			app->BindMaterial(
				Tonado->getChildRenderItem("StartSplash6"),
				"SKILL_TONADO_SPLASH_6_MATERIAL"
			);
		}

		{
			// Diffuse ¹Ù²ã¾ß ÇÔ.

			RenderItem* StartSplash7 = new RenderItem("StartSplash7", 1);

			app->CreateBillBoardObject(
				"StartSplash7",
				"",
				"SKILL",
				StartSplash7,
				40,
				{ 0.0f, 0.0f, 0.0f },
				{ 10, 10 },
				ObjectData::RenderType::_SKILL_TONADO_SPLASH_TYPE,
				false
			);

			//StartSplash7->setIsBaked(true);
			StartSplash7->setIsDrawShadow(false);

			StartSplash7->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, -50.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, -50.0f, 0.0f),
				DirectX::XMFLOAT3(-30.0f, 20.0f, -30.0f),
				DirectX::XMFLOAT3(30.0f, 50.0f, 30.0f),
				true
			);

			StartSplash7->ParticleGene();

			Tonado->appendChildRenderItem(StartSplash7);

			app->BindMaterial(
				Tonado->getChildRenderItem("StartSplash7"),
				"SKILL_TONADO_SPLASH_MATERIAL"
			);
		}

		{
			RenderItem* StartSplash8 = new RenderItem("StartSplash8", 1);

			app->CreateFBXObject(
				"StartSplash8",
				"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
				"CylindricalCone.fbx",
				MainTonadoTexturePaths,
				StartSplash8,
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 11.0f, 5.0f, 11.0f },
				ObjectData::RenderType::_SKILL_TONADO_MAIN_TONADO_TYPE,
				false
			);

			//StartSplash8->setIsBaked(true);
			StartSplash8->setIsDrawShadow(false);

			StartSplash8->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.01f, 0.0f),
				true
			);

			//StartSplash8->appendScaleAnimation(0.0f, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
			//StartSplash8->appendScaleAnimation(0.3f, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));

			Tonado->appendChildRenderItem(StartSplash8);

			Texture StartSplash_Noise_Tex;
			StartSplash_Noise_Tex.Name = "StartSplash_8_Noise_Tex";
			StartSplash_Noise_Tex.Filename = L"../../Textures/Skill/ToonProject2/Noise50.dds";

			app->uploadTexture(StartSplash_Noise_Tex, false);

			app->uploadMaterial(
				"SKILL_TONADO_SPLASH_8_MATERIAL",
				"StartSplash_4_Diffuse_Tex",
				"StartSplash_4_Mask_Tex",
				"StartSplash_8_Noise_Tex",
				false,
				{ 1.0f, 1.0f },
				{ 0.7019608f, 0.905909f * 1.6f, 1.5f, 1.0f }
			);

			app->BindMaterial(
				Tonado->getChildRenderItem("StartSplash8"),
				"SKILL_TONADO_SPLASH_8_MATERIAL",
				false
			);
		}

		{
			RenderItem* StartSplash9 = new RenderItem("StartSplash9", 1);

			app->CreateFBXObject(
				"StartSplash9",
				"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
				"HalfTorus1.fbx",
				MainTonadoTexturePaths,
				StartSplash9,
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 15.0f, 10.0f, 15.0f },
				ObjectData::RenderType::_SKILL_TONADO_WATER_TORUS_TYPE,
				false
			);

			//StartSplash9->setIsBaked(true);
			StartSplash9->setIsDrawShadow(false);

			StartSplash9->InitParticleSystem(
				10.0f,
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(0.0f, 0.01f, 0.0f),
				true
			);

			StartSplash9->setTextureSheetAnimationXY(6, 1);
			StartSplash9->setTextureSheetAnimationFrame(1.0f / 12.0f);

			StartSplash9->appendScaleAnimation(0.0f, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
			StartSplash9->appendScaleAnimation(0.3f, DirectX::XMFLOAT3(13.0f, 15.0f, 13.0f));

			Tonado->appendChildRenderItem(StartSplash9);

			Texture StartSplash_Diffuse_Tex;
			StartSplash_Diffuse_Tex.Name = "StartSplash_9_Diffuse_Tex";
			StartSplash_Diffuse_Tex.Filename = L"../../Textures/Skill/EpicToon/HPWater1.dds";

			Texture StartSplash_Mask_Tex;
			StartSplash_Mask_Tex.Name = "StartSplash_9_Mask_Tex";
			StartSplash_Mask_Tex.Filename = L"../../Textures/Skill/EpicToon/HPWater1b.dds";

			Texture StartSplash_Noise_Tex;
			StartSplash_Noise_Tex.Name = "StartSplash_9_Noise_Tex";
			StartSplash_Noise_Tex.Filename = L"../../Textures/Skill/ToonProject2/Noise34.dds";

			app->uploadTexture(StartSplash_Diffuse_Tex, false);
			app->uploadTexture(StartSplash_Mask_Tex, false);
			app->uploadTexture(StartSplash_Noise_Tex, false);

			app->uploadMaterial(
				"SKILL_TONADO_SPLASH_9_MATERIAL",
				"StartSplash_9_Diffuse_Tex",
				"StartSplash_9_Mask_Tex",
				"StartSplash_9_Noise_Tex",
				false,
				{ 1.0f, 3.0f },
				{ 1.0f, 1.0f, 1.0f, 1.0f }
			);

			app->BindMaterial(
				Tonado->getChildRenderItem("StartSplash9"),
				"SKILL_TONADO_SPLASH_9_MATERIAL",
				false
			);
		}

		{
			RenderItem* RayWaves = new RenderItem("StartSplash10", 5);

			app->CreateFBXObject(
				"StartSplash10",
				"D:\\Portfolio\\source\\DX12\\DirectX\\Models",
				"CylinderFromCenter2.fbx",
				MainTonadoTexturePaths,
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

			Tonado->appendChildRenderItem(RayWaves);

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
				Tonado->getChildRenderItem("StartSplash10"),
				"SKILL_LASER_RAY_WAVES_LONG_MATERIAL"
			);
		}
	}

	float testestest = -1.0f;

	RenderItem* particle  = nullptr;
	RenderItem* particle2 = nullptr;
	RenderItem* particle3 = nullptr;
	RenderItem* particle4 = nullptr;
	RenderItem* particle5 = nullptr;
	RenderItem* particle6 = nullptr;
	RenderItem* particle7 = nullptr;
	RenderItem* particle9 = nullptr;
	RenderItem* particle10 = nullptr;

	void _Start()
	{
		particle  = Tonado->getChildRenderItem("StartSplash");
		particle2 = Tonado->getChildRenderItem("StartSplash2");
		particle3 = Tonado->getChildRenderItem("StartSplash3");
		particle4 = Tonado->getChildRenderItem("StartSplash4");
		particle5 = Tonado->getChildRenderItem("StartSplash5");
		particle6 = Tonado->getChildRenderItem("StartSplash6");
		particle7 = Tonado->getChildRenderItem("StartSplash7");
		particle9 = Tonado->getChildRenderItem("StartSplash9");
		particle10 = Tonado->getChildRenderItem("StartSplash10");

		app->mTonadoBox.Center = { 
			0.0f, 
			-1000.0f, 
			0.0f 
		};
		app->mTonadoBox.Extents = {
			40.0f, 
			40.0f, 
			40.0f 
		};
	}

	void _Update(const GameTimer& gt)
	{
		if (isUpdate)
		{
			if (testestest < 0.0f)
			{
				testestest = 0.0f;

				app->GetData(Tonado->mName)->mTranslate[0].position = mPosition;
				app->GetData(Tonado->mName)->mTranslate[0].rotation = mRotation;

				app->mTonadoBox.Center = {
					mPosition.m128_f32[0] * 10.0f,
					mPosition.m128_f32[1] * 10.0f,
					mPosition.m128_f32[2] * 10.0f
				};

				particle->ParticleReset();
				particle2->ParticleReset();
				particle3->ParticleReset();
				particle4->ParticleReset();
				particle5->ParticleReset();
				particle6->ParticleReset();
				particle7->ParticleReset();
				particle9->ParticleReset();
				particle10->ParticleReset();
			}
			else if (testestest > 3.0f)
			{
				isUpdate = false;
				testestest = -1.0f;

				app->GetData(Tonado->mName)->mTranslate[0].position = 
					{ 0.0f, -20.0f, 0.0f, 1.0f };

				app->mTonadoBox.Center = {
					0.0f,
					-1000.0f,
					0.0f
				};

				particle->ParticleReset();
				particle2->ParticleReset();
				particle3->ParticleReset();
				particle4->ParticleReset();
				particle5->ParticleReset();
				particle6->ParticleReset();
				particle7->ParticleReset();
				particle9->ParticleReset();
				particle10->ParticleReset();
			}

			particle->ParticleUpdate(gt.DeltaTime(), testestest);
			particle2->ParticleUpdate(gt.DeltaTime(), testestest);
			particle3->ParticleUpdate(gt.DeltaTime(), testestest);
			particle4->ParticleUpdate(gt.DeltaTime(), testestest);
			particle5->ParticleUpdate(gt.DeltaTime(), testestest);
			particle6->ParticleUpdate(gt.DeltaTime(), testestest);
			particle7->ParticleUpdate(gt.DeltaTime(), testestest);
			particle9->ParticleUpdate(gt.DeltaTime(), testestest);
			particle10->ParticleUpdate(gt.DeltaTime(), testestest);

			testestest += gt.DeltaTime();
		}
	}

	void _End()
	{

	}
};