#include "MapGenerator.h"

MapGenerator::MapGenerator()
{
	tiles.resize(99);
	for (int i = 0; i < tiles.size(); i++)
		tiles[i].name = std::string("tiles") + std::to_string(i);
	treesPrefabs.resize(18);
	for (int i = 0; i < 6; i++)
	{
		treesPrefabs[i + 6 * 0].name = 
			std::string("TreeV") + std::to_string(1) + std::string(".00") + std::to_string(i);
		treesPrefabs[i + 6 * 1].name = 
			std::string("TreeV") + std::to_string(2) + std::string(".00") + std::to_string(i);
		treesPrefabs[i + 6 * 2].name = 
			std::string("TreeV") + std::to_string(3) + std::string(".00") + std::to_string(i);
	}
	littleStonesPrefabs.resize(30);
	for (int i = 0; i < littleStonesPrefabs.size(); i++)
	{
		if (i < 10)
			littleStonesPrefabs[i].name = std::string("LittleStone.00") + std::to_string(i);
		else
			littleStonesPrefabs[i].name = std::string("LittleStone.0") + std::to_string(i);
	}
	bigStonesPrefabs.resize(30);
	for (int i = 0; i < bigStonesPrefabs.size(); i++)
	{
		if (i < 10)
			bigStonesPrefabs[i].name = std::string("BigStone.00") + std::to_string(i);
		else
			bigStonesPrefabs[i].name = std::string("BigStone.0") + std::to_string(i);
	}
	bushsPrefabs.resize(9);
	for (int i = 0; i < bushsPrefabs.size(); i++)
		bushsPrefabs[i].name = std::string("Bush.00") + std::to_string(i);
	grassPrefabs.resize(9);
	{
		grassPrefabs[0].name = std::string("Grass_TypeB_Big.001");
		grassPrefabs[1].name = std::string("Grass_TypeB_Big.002");
		grassPrefabs[2].name = std::string("Grass_TypeB_Big");
		grassPrefabs[3].name = std::string("Grass_TypeB_Medium.001");
		grassPrefabs[4].name = std::string("Grass_TypeB_Medium.002");
		grassPrefabs[5].name = std::string("Grass_TypeB_Medium");
		grassPrefabs[6].name = std::string("Grass_TypeB_Small.001");
		grassPrefabs[7].name = std::string("Grass_TypeB_Small.002");
		grassPrefabs[8].name = std::string("Grass_TypeB_Small");
	}
	branchsPrefabs.resize(12);
	for (int i = 0; i < branchsPrefabs.size(); i++)
	{
		if (i < 10)
			branchsPrefabs[i].name = std::string("Branch.00") + std::to_string(i);
		else
			branchsPrefabs[i].name = std::string("Branch.0") + std::to_string(i);
	}
	logsPrefabs.resize(3);
	for (int i = 0; i < logsPrefabs.size(); i++)
		logsPrefabs[i].name = std::string("Log.00") + std::to_string(i);
	laddersTiles.resize(3);
	{
		laddersTiles[0].name = std::string("Ladder 2");
		laddersTiles[1].name = std::string("Ladder 1");
		laddersTiles[2].name = std::string("Ladder");
	}
	roadBridges.resize(2);
	{
		roadBridges[0].name = std::string("Bridge 1");
		roadBridges[1].name = std::string("Bridge 0");
	}
	interestPointTiles.resize(7);
	{
		interestPointTiles[0].name = std::string("Tile POI");
		interestPointTiles[1].name = std::string("Tile Chest");
		interestPointTiles[2].name = std::string("Tile Portal");
		interestPointTiles[3].name = std::string("Tile Dungeon");
		interestPointTiles[4].name = std::string("Tile Bonfire");
		interestPointTiles[5].name = std::string("Gem");
		interestPointTiles[6].name = std::string("Coin");
	}
}

MapGenerator::~MapGenerator()
{
	if (lastRoadsMap)
	{
		for (int i = 0; i < xSize; i++)
		{
			delete(lastRoadsMap[i]);
		}
		delete[](lastRoadsMap);

		lastRoadsMap = nullptr;
	}
	if (lastHeightMap)
	{
		for (int i = 0; i < xSize; i++)
		{
			delete(lastHeightMap[i]);
		}
		delete[](lastHeightMap);

		lastHeightMap = nullptr;
	}
	if (lastHolesMap)
	{
		for (int i = 0; i < xSize; i++)
		{
			delete(lastHolesMap[i]);
		}
		delete[](lastHolesMap);

		lastHolesMap = nullptr;
	}
	if (lastLaddersMap)
	{
		for (int i = 0; i < xSize; i++)
		{
			delete(lastLaddersMap[i]);
		}
		delete[](lastLaddersMap);

		lastLaddersMap = nullptr;
	}

	tiles.clear();
	treesPrefabs.clear();
	littleStonesPrefabs.clear();
	bigStonesPrefabs.clear();
	bushsPrefabs.clear();
	grassPrefabs.clear();
	branchsPrefabs.clear();
	logsPrefabs.clear();
	laddersTiles.clear();
	roadBridges.clear();
	interestPointTiles.clear();
}

bool MapGenerator::isPosNotInPOI(DirectX::XMFLOAT3 posToCheck, bool** roadsMap, bool** laddersMap)
{
	int x = (int)(ceil(posToCheck.x / 2.0f));
	int z = (int)(ceil(posToCheck.z / 2.0f));

	if (x < (sizeof(roadsMap) / sizeof(roadsMap[0])) && z < (sizeof(roadsMap[0]) / sizeof(bool)))
	{
		if (!roadsMap[z][x] && !laddersMap[x][z])
		{
			return true;
		}
		else
			return false;
	}
	else
		return false;
}
bool MapGenerator::IsPosAvailableByDistance(
	DirectX::XMFLOAT3 posToCheck, 
	std::vector<DirectX::XMFLOAT3> otherPoses, 
	float minDistance
)
{
	float minDistanceWeHas = -1.0f;

	for (int i = 0; i < otherPoses.size(); i++)
	{
		float distance = sqrt(
			pow(posToCheck.x - otherPoses[i].x, 2) +
			pow(posToCheck.y - otherPoses[i].y, 2) +
			pow(posToCheck.z - otherPoses[i].z, 2)
		);
		if (distance < minDistanceWeHas || minDistanceWeHas < 0.0f) minDistanceWeHas = distance;
	}

	if (minDistanceWeHas > minDistance || minDistanceWeHas < 0.0f)
		return true;
	else
		return false;
}
bool MapGenerator::isPosInRangeOf(DirectX::XMFLOAT3 posToCheck, std::vector<DirectX::XMFLOAT3> otherPoses, float needDistance)
{
	float minDistanceWeHas = -1.0f;

	for (int i = 0; i < otherPoses.size(); i++)
	{
		float distance = sqrt(
			pow(posToCheck.x - otherPoses[i].x, 2) +
			pow(posToCheck.y - otherPoses[i].y, 2) +
			pow(posToCheck.z - otherPoses[i].z, 2)
		);
		if (distance < minDistanceWeHas || minDistanceWeHas < 0.0f) minDistanceWeHas = distance;
	}

	if (minDistanceWeHas < needDistance)
		return true;
	else
		return false;
}
bool MapGenerator::RoadIsNotPOI(int x, int z, std::vector<std::array<int, 2>> poi)
{
	bool roadIsNotPoi = true;

	for (int i = 0; i < poi.size(); i++)
	{
		if (poi[i][0] == x && poi[i][1] == z)
		{
			roadIsNotPoi = false;
			break;
		}
	}

	return roadIsNotPoi;
}
// 생성된 베이스 지형에 타일 생성
void MapGenerator::SpawnTile(float _x, float _z, DirectX::XMFLOAT3 _mapPos, Transform _parent, int _height)
{
	// 랜덤 타입의 타일을 하나 생성 후
	Object* newPart = Instantiate(tiles[Random::RangeInt(0, (int)tiles.size())]);
	// 현재 지형의 높이 값을 가지고 옴
	float posY = _mapPos.y;
	// 그리고 해당 타일이 지형으로 부터 얼마만큼 떨어질 것인지 오프셋도 저장
	int heightForThisTile = _height;

	// 만일 타일이 지형과의 오프셋 거리를 가지고 있다면
	if (heightForThisTile > 0)
	{
		posY += (heightForThisTile * 2.0f);
	}
	// 새로운 타일의 위치를 최신화
	newPart->transform.position = { _mapPos.x + _x, posY, _mapPos.z + _z };
	newPart->transform.setParent(_parent);
}

// 지형의 높낮이를 완화
int** MapGenerator::SmoothHeights(int** curentMap, int _x, int _z)
{
	// 현재 지형의 높이를 얻어서
	int myHeight = curentMap[_x][_z];
	int heightDifference = 0;

	// 상하좌우의 지형의 높이와의 격차를 구하여 최대 격차를 구한다.
	if (_z - 1 > 0)
	{
		int difference = curentMap[_x][_z - 1] - myHeight;
		if (difference > heightDifference) heightDifference = difference;
	}
	if (_z + 1 < (sizeof(curentMap[0]) / sizeof(int)))
	{
		int difference = curentMap[_x][_z + 1] - myHeight;
		if (difference > heightDifference) heightDifference = difference;
	}
	if (_x - 1 > 0)
	{
		int difference = curentMap[_x - 1][_z] - myHeight;
		if (difference > heightDifference) heightDifference = difference;
	}
	if (_x + 1 < (sizeof(curentMap) / sizeof(curentMap[0])))
	{
		int difference = curentMap[_x + 1][_z] - myHeight;
		if (difference > heightDifference) heightDifference = difference;
	}

	// 추후 최대 격차가 1을 초과하였다면, 현재 지형의 높이를 1추가한다.
	if (heightDifference > 1)
		curentMap[_x][_z] = myHeight + 1;

	return curentMap;
}

// 지형의 높낮이를 완화(현재 지형을 낮추는 방향으로)
int** MapGenerator::SmoothHeightDown(int** curentMap, int _x, int _z)
{
	// 현재 지형의 높이를 구하여
	int myHeight = curentMap[_x][_z];

	// 상하좌우 지형의 높이보다 낮은지 체크
	// 만일 상하좌우 중 지형이 높은 곳이 있다면, (현재 지형 높이 ~ +2)까지 범위에서
	// 랜덤된 값을 해당 지형의 높이로 변경하고
	// 이와 같은 작업을 재귀한다.
	if (_z - 1 > 0)
	{
		if (curentMap[_x][_z - 1] > myHeight)
		{
			curentMap[_x][_z - 1] = Random::RangeInt(myHeight, myHeight + 2);
			curentMap = SmoothHeightDown(curentMap, _x, _z - 1);
		}
	}
	if (_z + 1 < (sizeof(curentMap[0]) / sizeof(int)))
	{
		if (curentMap[_x][_z + 1] > myHeight)
		{
			curentMap[_x][_z + 1] = Random::RangeInt(myHeight, myHeight + 2);
			curentMap = SmoothHeightDown(curentMap, _x, _z + 1);
		}
	}
	if (_x - 1 > 0)
	{
		if (curentMap[_x - 1][_z] > myHeight)
		{
			curentMap[_x - 1][_z] = Random::RangeInt(myHeight, myHeight + 2);
			curentMap = SmoothHeightDown(curentMap, _x - 1, _z);
		}
	}
	if (_x + 1 < (sizeof(curentMap) / sizeof(curentMap[0])))
	{
		if (curentMap[_x + 1][_z] > myHeight)
		{
			curentMap[_x + 1][_z] = Random::RangeInt(myHeight, myHeight + 2);
			curentMap = SmoothHeightDown(curentMap, _x + 1, _z);
		}
	}

	return curentMap;
}

// 특정 지점을 기준으로 주위 지형과 함께 상승시킴
int** MapGenerator::RaiseHeight(int** curentMap, int _x, int _z, int maxSize, int maxHeight, bool** holesMap, int iteration, EnableDisable hs)
{
	// 만일 heightSmooth 기능이 ON 되어있고, 주위 지형에 구멍이 없거나 또는
	// heightSmooth 기능이 OFF 되어있는 경우 
	if (hs == EnableDisable::Enabled)
	{
		// 해당 지형의 높이가 최대 높이 보다 작다면 높이를 1 상승
		if (curentMap[_x][_z] < maxHeight)
			curentMap[_x][_z] += 1;

		// 확률을 생성하여 주위의 지형 또한 상승 시킬 것인지를 랜덤으로 판별
		float chanceForAdditionalHeight = 100.0f;

		int anywayHeights = (int)(floor(((float)maxSize / 1.35f)));
		if (iteration > anywayHeights)
		{
			chanceForAdditionalHeight = 100.0f - ((100.0f / (float)(maxSize - anywayHeights)) * ((float)(iteration - anywayHeights)));
		}

		iteration++;

		// 상하좌우를 돌며 만일 랜덤 값에 성사 된다면 재귀적으로 해당 지형도 높이를 상승 시킴
		if (_z - 1 > 0 && Random::RangeInt(0, 100) < (chanceForAdditionalHeight * Random::Rangefloat(0.75f, 1.0f)))
		{
			curentMap = RaiseHeight(curentMap, _x, _z - 1, maxSize, maxHeight, holesMap, iteration, hs);
		}
		if (_z + 1 < (sizeof(curentMap[0]) / sizeof(int)) && Random::RangeInt(0, 100) < (chanceForAdditionalHeight * Random::Rangefloat(0.75f, 1.0f)))
		{
			curentMap = RaiseHeight(curentMap, _x, _z + 1, maxSize, maxHeight, holesMap, iteration, hs);
		}
		if (_x - 1 > 0 && Random::RangeInt(0, 100) < (chanceForAdditionalHeight * Random::Rangefloat(0.75f, 1.0f)))
		{
			curentMap = RaiseHeight(curentMap, _x - 1, _z, maxSize, maxHeight, holesMap, iteration, hs);
		}
		if (_x + 1 < (sizeof(curentMap) / sizeof(curentMap[0])) && Random::RangeInt(0, 100) < (chanceForAdditionalHeight * Random::Rangefloat(0.75f, 1.0f)))
		{
			curentMap = RaiseHeight(curentMap, _x + 1, _z, maxSize, maxHeight, holesMap, iteration, hs);
		}
	}
	return curentMap;
}

// 지형에 구멍 생성
bool** MapGenerator::CreateHoles(bool** curentMap, int _x, int _z, int maxSize, int iteration)
{
	// 만일 구멍 여부가 false라면
	if (!curentMap[_x][_z])
	{
		// 현재 지형에 구멍을 생성
		curentMap[_x][_z] = true;

		// 확률을 지정하여 주변의 땅에도 랜덤으로 구멍을 생성
		float chanceForAdditionalHole = 100.0f;
		int anywayHoles = (int)(floor(((float)maxSize / 1.25f)));

		// 구멍 생성이 거듭 될 수록 확률이 줄어들어야 함으로 iteration에 따른 확률을 재생성
		if (iteration > anywayHoles)
		{
			chanceForAdditionalHole = 
				100.0f - 
				(	
					(100.0f / (float)(maxSize - anywayHoles)) * 
					((float)(iteration - anywayHoles))
				);
		}

		// 구멍 생성 반복++
		iteration++;


		if (iteration < maxSize)
		{
			// 상하좌우를 돌며, 만일 해당 확률에 성립되었다면 해당 위치를 재귀적으로 구멍생성
			if (_z - 1 > 0 &&
				Random::RangeInt(0, 100) < (chanceForAdditionalHole * Random::Rangefloat(0.75f, 1.0f))
				)
			{
				curentMap = CreateHoles(curentMap, _x, _z - 1, maxSize, iteration);
			}
			if (_z + 1 < (sizeof(curentMap[0]) / sizeof(bool)) &&
				Random::RangeInt(0, 100) < (chanceForAdditionalHole * Random::Rangefloat(0.75f, 1.0f))
				)
			{
				curentMap = CreateHoles(curentMap, _x, _z + 1, maxSize, iteration);
			}
			if (_x - 1 > 0 &&
				Random::RangeInt(0, 100) < (chanceForAdditionalHole * Random::Rangefloat(0.75f, 1.0f))
				)
			{
				curentMap = CreateHoles(curentMap, _x - 1, _z, maxSize, iteration);
			}
			if (_x + 1 < (sizeof(curentMap) / sizeof(curentMap[0])) &&
				Random::RangeInt(0, 100) < (chanceForAdditionalHole * Random::Rangefloat(0.75f, 1.0f))
				)
			{
				curentMap = CreateHoles(curentMap, _x + 1, _z, maxSize, iteration);
			}
		}
	}

	return curentMap;
}

bool MapGenerator::IsHole(int _x, int _z, bool** _holes)
{
	if (_z >= 0 &&
		_z < (sizeof(_holes) / sizeof(_holes[0])) &&
		_x >= 0 &&
		_x < (sizeof(_holes[0]) / sizeof(bool))
		)
	{
		return _holes[_z][_x];
	}
	return false;
}
