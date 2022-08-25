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
		for (UINT i = 0; i < xSize; i++)
		{
			delete(lastRoadsMap[i]);
		}
		delete[](lastRoadsMap);

		lastRoadsMap = nullptr;
	}
	if (lastHeightMap)
	{
		for (UINT i = 0; i < xSize; i++)
		{
			delete(lastHeightMap[i]);
		}
		delete[](lastHeightMap);

		lastHeightMap = nullptr;
	}
	if (lastHolesMap)
	{
		for (UINT i = 0; i < xSize; i++)
		{
			delete(lastHolesMap[i]);
		}
		delete[](lastHolesMap);

		lastHolesMap = nullptr;
	}
	if (lastLaddersMap)
	{
		for (UINT i = 0; i < xSize; i++)
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
bool MapGenerator::IsPosAvailableByDistance(DirectX::XMFLOAT3 posToCheck, std::vector<DirectX::XMFLOAT3> otherPoses, float minDistance)
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
// Ÿ�� ����
void MapGenerator::SpawnTile(int _i, int _a, float _x, float _z, DirectX::XMFLOAT3 _mapPos, Transform _parent, bool _isRoad, int _height)
{
	// �������� Ÿ���� ����
	Object* newPart = Instantiate(tiles[Random::RangeInt(0, tiles.size())]);
	// ���� �� ���� �����Ǵ� Ÿ���� ��Ÿ
	float posY = _mapPos.y;
	// ���� ����
	int heightForThisTile = _height;

	// Ÿ���� ���� ���
	if (heightForThisTile > 0)
	{
		// ���� ���� + ���� �� �� �����Ǵ� Ÿ���� ��Ÿ = 
		// ���� Ÿ���� ����
		posY += (heightForThisTile * 2.0f);
	}
	// ���������� Ÿ����ġ ����
	newPart->transform.position = { _mapPos.x + _x, posY, _mapPos.z + _z };

	//// Ÿ���� �� ��° ���ϵ� �������� ��ġ�� ����
	//if (heightForThisTile > 0)
	//{
	//	newPart->transform.GetChild(1).localScale = new Vector3(1f, 1f + (heightForThisTile * 1.2f), 1f);
	//	newPart->transform.GetChild(1).localPosition = new Vector3(0f, -0.7f * heightForThisTile, 0f);
	//}

	// �������� Ÿ���� ȸ����Ų��.
	//int rotRandom = Random::RangeInt(0, 4);
	//if (rotRandom == 0)
	//{
	//	newPart->transform.rotation = { 0.0f, 90.0f, 0.0f };
	//}
	//else if (rotRandom == 1)
	//{
	//	newPart->transform.rotation = { 0.0f, 180.0f, 0.0f };
	//}
	//else if (rotRandom == 2)
	//{
	//	newPart->transform.rotation = { 0.0f, 270.0f, 0.0f };
	//}
	//else
	//{
	//	newPart->transform.rotation = { 0.0f, 0.0f, 0.0f };
	//}
	newPart->transform.setParent(_parent);
}

// ���� ��ġ�� �����¿��� ����� ���� ���� ����Ͽ� ���� ū ���� ���̸� ���ϰ�
// ���� �� ���� ���̰� 1 �ʰ���� ���� ��ġ�� ��� ���̸� 1 �÷��� ��Ų��.
int** MapGenerator::SmoothHeights(int** curentMap, int _x, int _z)
{
	// ���� ��ġ�� ���� ��
	int myHeight = curentMap[_x][_z];
	int heightDifference = 0;

	// �����¿��� ������ ���� ����Ͽ� ���� ���̰� ���� ���� ����� ���ϰ�
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

	// �� ���� 1���� ũ�ٸ� ���� ����� ���̸� 1���� ��Ų��.
	if (heightDifference > 1)
		curentMap[_x][_z] = myHeight + 1;

	return curentMap;
}
// �����¿츦 ��ȸ�ϸ�
// ���� ����Ʈ�� ������� 0 ~ 2��ŭ ���̸� ��½�Ų��.
int** MapGenerator::SmoothHeightDown(int** curentMap, int _x, int _z)
{
	// ���� ����Ʈ�� ���̰�
	int myHeight = curentMap[_x][_z];

	// �����¿츦 ��ȸ�ϸ�
	// ���� ����Ʈ�� ������� 0 ~ 2��ŭ ���̸� ��½�Ų��.
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

//  RaiseHeight(heightMap, hX, hZ, maxHeightSize, maxHeightInTiles, holesMap, 0, _heightSmoothing);
int** MapGenerator::RaiseHeight(int** curentMap, int _x, int _z, int maxSize, int maxHeight, bool** holesMap, int iteration, EnableDisable hs)
{
	// heightsSmooth�� true�鼭 ��ó�� ������ ���ų�, heightsSmooth�� false�̸�
	if ((hs == EnableDisable::Enabled &&
		!IsNeighboringHole(_x, _z, holesMap)) ||
		hs == EnableDisable::Disabled)
	{
		// �ִ� ���̰� �ƴ� �̻� ���̸� 1�� �ø���
		if (curentMap[_x][_z] < maxHeight)
			curentMap[_x][_z] += 1;

		// ��ó�� �߰������� ���̰� �ö� ����� ���� Ȯ���� ���Ѵ�.
		float chanceForAdditionalHeight = 100.0f;

		// iteration�� ũ��� changeforAdditionalHeight�� ����Ѵ�.
		int anywayHeights = (int)(floor(((float)maxSize / 1.35f)));
		if (iteration > anywayHeights)
		{
			chanceForAdditionalHeight = 100.0f - ((100.0f / (float)(maxSize - anywayHeights)) * ((float)(iteration - anywayHeights)));
		}

		iteration++;

		// x, z�� heightMap�� ���ֿ� ����� �ʰ� �ϸ鼭, chanceForAdditionHeight Ȯ���� ������ �Ͽ��ٸ�
		// �ٹ濡 �߰� ���̸� �����Ѵ�.
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
// CreateHoles(holesMap, hX, hZ, maxHoleSize, 0);
// _x, _z : ���� ������ ��ġ
// maxSize: ������ �ִ� ũ��
// iteration: recursion iterator
bool** MapGenerator::CreateHoles(bool** curentMap, int _x, int _z, int maxSize, int iteration)
{
	// Hole Map�� x, z��ġ�� false �� ���
	// �� �ű� Hole�� ���
	if (!curentMap[_x][_z])
	{
		// Hole�� ����
		curentMap[_x][_z] = true;
		// �ٹ濡 �߰����� ������ ������� Ȯ���� ����
		float chanceForAdditionalHole = 100.0f;
		// �ٸ� ����� Ȧ�� ���� ����
		int anywayHoles = (int)(floor(((float)maxSize / 1.25f)));

		// �����¿� ����� ������ ���� Ȯ���� ����. (�����)
		if (iteration > anywayHoles)
		{
			// iteration�� factor�� chanceForAdditionalHole�� �������� �ȴ�.
			chanceForAdditionalHole = 100.0f - ((100.0f / (float)(maxSize - anywayHoles)) * ((float)(iteration - anywayHoles)));
		}

		iteration++;


		if (iteration < maxSize)
		{
			// ���� ���ָ� ����� �ʰ�, �߰������� ������ �ո� Ȯ���� �����Ѵٸ�
			if (_z - 1 > 0 &&
				Random::RangeInt(0, 100) < (chanceForAdditionalHole * Random::Rangefloat(0.75f, 1.0f))
				)
			{
				// �Ʒ��� ����� ������ �߰� ����
				curentMap = CreateHoles(curentMap, _x, _z - 1, maxSize, iteration);
			}
			if (_z + 1 < (sizeof(curentMap[0]) / sizeof(bool)) &&
				Random::RangeInt(0, 100) < (chanceForAdditionalHole * Random::Rangefloat(0.75f, 1.0f))
				)
			{
				// ���� ����� ������ �߰� ����
				curentMap = CreateHoles(curentMap, _x, _z + 1, maxSize, iteration);
			}
			if (_x - 1 > 0 &&
				Random::RangeInt(0, 100) < (chanceForAdditionalHole * Random::Rangefloat(0.75f, 1.0f))
				)
			{
				// ���� ����� ������ �߰� ����
				curentMap = CreateHoles(curentMap, _x - 1, _z, maxSize, iteration);
			}
			if (_x + 1 < (sizeof(curentMap) / sizeof(curentMap[0])) &&
				Random::RangeInt(0, 100) < (chanceForAdditionalHole * Random::Rangefloat(0.75f, 1.0f))
				)
			{
				// ������ ����� ������ �߰� ����
				curentMap = CreateHoles(curentMap, _x + 1, _z, maxSize, iteration);
			}
		}
	}

	return curentMap;
}
bool MapGenerator::IsNeighboringHigher(int** curentMap, int _x, int _z)
{
	bool isHigher = false;
	int myHeight = curentMap[_x][_z];
	if (_z - 1 > 0)
	{
		if (curentMap[_x][_z - 1] > myHeight) isHigher = true;
	}
	if (_z + 1 < (sizeof(curentMap[0]) / sizeof(int)))
	{
		if (curentMap[_x][_z + 1] > myHeight) isHigher = true;
	}
	if (_x - 1 > 0)
	{
		if (curentMap[_x - 1][_z] > myHeight) isHigher = true;
	}
	if (_x + 1 < (sizeof(curentMap) / sizeof(curentMap[0])))
	{
		if (curentMap[_x + 1][_z] > myHeight) isHigher = true;
	}
	return isHigher;
}
bool MapGenerator::IsNeighboringHole(int _x, int _z, bool** curentHoles)
{
	bool isHole = false;

	if (_z - 1 > 0)
	{
		if (curentHoles[_x][_z - 1]) isHole = true;
	}
	if (_z + 1 < (sizeof(curentHoles[0]) / sizeof(bool)))
	{
		if (curentHoles[_x][_z + 1]) isHole = true;
	}
	if (_x - 1 > 0)
	{
		if (curentHoles[_x - 1][_z]) isHole = true;
	}
	if (_x + 1 < (sizeof(curentHoles) / sizeof(curentHoles[0])))
	{
		if (curentHoles[_x + 1][_z]) isHole = true;
	}

	return isHole;
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
