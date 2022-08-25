#pragma once

#include "BoxApp.h"

std::vector<int> mapIndex;

class MapObject {
private:
	BoxApp* app = nullptr;

	RenderItem* mGround = nullptr;
	RenderItem* mYard = nullptr;

	RenderItem* mTree = nullptr;
	RenderItem* mBushs = nullptr;
	RenderItem* mBigStone = nullptr;
	RenderItem* mGrass = nullptr;
	RenderItem* mBranchs = nullptr;
	RenderItem* mLogs = nullptr;

public:
	MapObject(BoxApp* mApp) :app(mApp) {}

	MapObject() = delete;
	MapObject(const MapObject& rhs) = delete;
	MapObject& operator=(const MapObject& rhs) = delete;
	~MapObject() = default;

	void _Awake()
	{
		// Create Init Objects
		mGround = app->CreateGameObject("MapGroundGeo", 0);
		app->CreateFBXObjectSplitSubmeshs(
			std::string("MapGroundGeo"),
			std::string("D:\\ClientMap\\ClientMap\\Assets\\HYPEPOLY\\Models"),
			std::string("untitled.fbx"),
			mGround,
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f },
			ObjectData::RenderType::_MAP_BASE_RENDER_TYPE,
			72,
			false
		);

		mYard = app->CreateGameObject("MapYardGeo", 0);
		app->CreateFBXObjectSplitSubmeshs(
			std::string("MapYardGeo"),
			std::string("D:\\ClientMap\\ClientMap\\Assets\\HYPEPOLY\\Models"),
			std::string("untitled.fbx"),
			mYard,
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f },
			ObjectData::RenderType::_MAP_BASE_RENDER_TYPE,
			71,
			false
		);

		mTree = app->CreateGameObject("MapTreeGeo", 0);
		app->CreateFBXObjectSplitSubmeshs(
			std::string("MapTreeGeo"),
			std::string("D:\\ClientMap\\ClientMap\\Assets\\HYPEPOLY\\Models"),
			std::string("untitled.fbx"),
			mTree,
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f },
			ObjectData::RenderType::_MAP_BASE_RENDER_TYPE,
			60,
			false
		);

		mBushs = app->CreateGameObject("MapBushGeo", 0);
		app->CreateFBXObjectSplitSubmeshs(
			std::string("MapBushGeo"),
			std::string("D:\\ClientMap\\ClientMap\\Assets\\HYPEPOLY\\Models"),
			std::string("untitled.fbx"),
			mBushs,
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f },
			ObjectData::RenderType::_MAP_BASE_RENDER_TYPE,
			67,
			false
		);

		mBigStone = app->CreateGameObject("MapBigStoneGeo", 0);
		app->CreateFBXObjectSplitSubmeshs(
			std::string("MapBigStoneGeo"),
			std::string("D:\\ClientMap\\ClientMap\\Assets\\HYPEPOLY\\Models"),
			std::string("untitled.fbx"),
			mBigStone,
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f },
			ObjectData::RenderType::_MAP_BASE_RENDER_TYPE,
			99,
			false
		);

		mGrass = app->CreateGameObject("MapGrassGeo", 0);
		app->CreateFBXObjectSplitSubmeshs(
			std::string("MapGrassGeo"),
			std::string("D:\\ClientMap\\ClientMap\\Assets\\HYPEPOLY\\Models"),
			std::string("untitled.fbx"),
			mGrass,
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f },
			ObjectData::RenderType::_MAP_BASE_RENDER_TYPE,
			18,
			false
		);

		mBranchs = app->CreateGameObject("MapBranchGeo", 0);
		app->CreateFBXObjectSplitSubmeshs(
			std::string("MapBranchGeo"),
			std::string("D:\\ClientMap\\ClientMap\\Assets\\HYPEPOLY\\Models"),
			std::string("untitled.fbx"),
			mBranchs,
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f },
			ObjectData::RenderType::_MAP_BASE_RENDER_TYPE,
			157,
			false
		);

		mLogs = app->CreateGameObject("MapLogsGeo", 0);
		app->CreateFBXObjectSplitSubmeshs(
			std::string("MapLogsGeo"),
			std::string("D:\\ClientMap\\ClientMap\\Assets\\HYPEPOLY\\Models"),
			std::string("untitled.fbx"),
			mLogs,
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f },
			ObjectData::RenderType::_MAP_BASE_RENDER_TYPE,
			63,
			false
		);

		mGround->setIsDrawShadow(true);
		mYard->setIsDrawShadow(true);

		mTree->setIsDrawShadow(true);
		mBushs->setIsDrawShadow(true);
		mBigStone->setIsDrawShadow(true);
		mGrass->setIsDrawShadow(true);
		mBranchs->setIsDrawShadow(true);
		mLogs->setIsDrawShadow(true);

		mGround->setIsBaked(true);
		mYard->setIsBaked(true);

		mTree->setIsBaked(true);
		mBushs->setIsBaked(true);
		mBigStone->setIsBaked(true);
		mGrass->setIsBaked(true);
		mBranchs->setIsBaked(true);
		mLogs->setIsBaked(true);
	}

	ObjectData* mGroundData = nullptr;

	void _Start() 
	{
		std::vector<std::vector<int>> mapHeight = app->mMapGene.loadHeightMap();
		std::vector<std::vector<bool>> mapHole = app->mMapGene.loadHolesMap();
		std::vector<std::vector<bool>> mapLadder = app->mMapGene.loadLaddersMap();
		std::vector<std::vector<bool>> mapRoads = app->mMapGene.loadRoadsMap();

		float mSize = 10.0f;

		float floor = 0.0f;
		bool isBaseFloor = false;
		bool isBarrierGround = false;

		UINT index = 0;
		for (int i = 0; i < mapHeight.size(); i++)
		{
			for (int j = 0; j < mapHeight[0].size(); j++)
			{
				floor = (float)mapHeight[i][j];

				isBaseFloor = (floor == 0);
				isBarrierGround = ((0 == i || i == mapHeight.size() - 1) || (0 == j || j == mapHeight[0].size() - 1));

				if (!mapHole[i][j])
				{
					mapIndex.push_back(index++);

					if ((isBaseFloor && isBarrierGround) || floor > 0)
					{
						mGround->Instantiate(
							{ (float)i * mSize * 2.0f, (floor - 1) * (mSize * 0.5f), (float)j * mSize * 2.0f },
							{ 0.0f, 0.0f, 0.0f },
							{ mSize, floor * (mSize * 0.5f) + mSize, mSize }
						);
					}

					mYard->Instantiate(
						{ (float)i * mSize * 2.0f, (floor - 1) * (mSize - 1), (float)j * mSize * 2.0f },
						{ 0.0f, 0.0f, 0.0f },
						{ mSize, mSize, mSize },
						{ mSize*2, 3.0f, mSize*2 }
					);
				}
				else
				{
					mapIndex.push_back(0);
				}
			}
		}

		mGroundData = app->GetData("MapYardGeo");

		app->mMapGene.AdditionalFilling(
			GeneratorParametr::Random,
			30,
			mGroundData->Bounds,
			mTree,
			mBushs,
			mBigStone,
			mGrass,
			mBranchs,
			mLogs
		);

		printf("");
	}

	void _Update(const GameTimer& gt) {}

	void _End() {}
};