#include "Link.h"
#include "Player.h"

Player player;

void BoxApp::_Awake(BoxApp* app) {
	// Bind Default Texture, Materials
	{
		Texture skyTexture;
		skyTexture.Name = "snowcube1024";
		skyTexture.Filename = L"../../Textures/snowcube1024.dds";

		app->uploadTexture(skyTexture, true);
		app->uploadMaterial("SkyMat", "snowcube1024", true);

		Texture BottomTex;
		BottomTex.Name = "bottomTex";
		BottomTex.Filename = L"../../Textures/ice.dds";

		app->uploadTexture(BottomTex, false);
		app->uploadMaterial("BottomMat", "bottomTex", false);

		Texture texList[6];

		// mTexture에다가 싸그리 집어넣고 BoxApp 초기화 첫 단계에서 LoadTexture 넣어주세요.
		texList[0].Name = "bricksTex";
		texList[0].Filename = L"../../Textures/bricks.dds";

		texList[1].Name = "stoneTex";
		texList[1].Filename = L"../../Textures/stone.dds";

		texList[2].Name = "tileTex";
		texList[2].Filename = L"../../Textures/tile.dds";

		texList[3].Name = "grassTex";
		texList[3].Filename = L"../../Textures/grass.dds";

		texList[4].Name = "waterTex";
		texList[4].Filename = L"../../Textures/water1.dds";

		texList[5].Name = "fenceTex";
		texList[5].Filename = L"../../Textures/WireFence.dds";

		for (int i = 0; i < 6; i++) {
			app->uploadTexture(texList[i]);
		}

		app->uploadMaterial("Default");
	}

	player._Awake(app);
}

void BoxApp::_Start() {
	player._Start();
}

void BoxApp::_Update(const GameTimer& gt) {
	player._Update(gt);
}

void BoxApp::_Exit() {
	player._End();
}