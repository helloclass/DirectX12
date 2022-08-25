#pragma once
#include "../../Common/d3dUtil.h"

typedef struct Transform
{
	Transform()
	{
		this->parent = nullptr;
	}
	~Transform()
	{
		try
		{
			if (!this->parent)
				delete(this->parent);
		}
		catch (std::exception& e)
		{
			throw std::runtime_error("�Ҹ� ����");
		}
	}

	Transform& operator=(Transform& rhs)
	{
		this->parent = rhs.parent;

		this->position = rhs.position;
		this->rotation = rhs.rotation;
		this->scale = rhs.scale;

		return *this;
	};

	void setParent(Transform& p)
	{
		if (parent == nullptr)
		{
			parent = new Transform;
		}
		*this->parent = p;
	}

	struct Transform* parent = nullptr;

	DirectX::XMVECTOR position;
	DirectX::XMVECTOR rotation;
	DirectX::XMVECTOR scale;
}Transform;

typedef struct Object
{
	std::string name;
	struct Transform transform;

	Object& operator=(Object& rhs)
	{
		this->name = rhs.name;
		this->transform = rhs.transform;

		return *this;
	}
}Object;

typedef enum GeneratorParameter
{
	None,
	VeryLow,
	Low,
	Medium,
	High,
	VeryHigh,
	Random
}GeneratorParametr;
typedef enum EnableDisable
{
	Enabled,
	Disabled
}EnableDisable;

static Object* Instantiate(Object& obj)
{
	Object* newObj = new Object;
	*newObj = obj;

	return newObj;
}

static Object* Instantiate(Object& obj, DirectX::XMFLOAT3 position, Transform& parent)
{
	Object* newObj = new Object;
	*newObj = obj;

	newObj->transform.position = DirectX::XMLoadFloat3(&position);
	newObj->transform.setParent(parent);

	return newObj;
}

class MapGenerator
{
public:
	MapGenerator();
	~MapGenerator();

public:
	// Tiles Prefabs
	std::vector<Object> tiles;

	// Map Parameters
	std::string mapName;
	DirectX::XMFLOAT3 mapPosition;
	int mapSize;
	GeneratorParametr holesCount;
	GeneratorParametr holesSizes;
	GeneratorParametr heightsCount;
	GeneratorParametr heightsSizes;
	GeneratorParametr maxHeight;
	EnableDisable heightSmoothing;

	// Map Filling
	GeneratorParametr additionalFilling;
	std::vector<Object> treesPrefabs;
	std::vector<Object> littleStonesPrefabs;
	std::vector<Object> bigStonesPrefabs;
	std::vector<Object> bushsPrefabs;
	std::vector<Object> grassPrefabs;
	std::vector<Object> branchsPrefabs;
	std::vector<Object> logsPrefabs;

	// [Map points of interest (POI)]
	EnableDisable contentOnMap;
	int poiCount;
	Object startTile;
	Object endTile;
	std::vector<Object> interestPointTiles;

	// [Map roads between POI's]
	EnableDisable roads;
	int roadsBetweenPOI;
	int roadsFilling;
	int roadsFenceChance;
	Object roadStraight;
	Object roadRotate;
	Object roadCrossroad;
	Object roadTriple;
	Object roadEnd;
	std::vector<Object> roadBridges;

	// [Ladders on map]
	EnableDisable ladders;

	int laddersChance;
	std::vector<Object> laddersTiles;

	// Tap this checkbox to generate in play mode
	bool TapToGenerate = false;

private:
	Object lastMap;
	bool** lastRoadsMap = nullptr;
	int**  lastHeightMap = nullptr;
	bool** lastHolesMap = nullptr;
	bool** lastLaddersMap = nullptr;

	UINT xSize, zSize;

public:
	bool isPosNotInPOI(DirectX::XMFLOAT3 posToCheck, bool** roadsMap, bool** laddersMap);
	bool IsPosAvailableByDistance(DirectX::XMFLOAT3 posToCheck, std::vector<DirectX::XMFLOAT3> otherPoses, float minDistance);
	bool isPosInRangeOf(DirectX::XMFLOAT3 posToCheck, std::vector<DirectX::XMFLOAT3> otherPoses, float needDistance);
	bool RoadIsNotPOI(int x, int z, std::vector<std::array<int, 2>> poi);
	// Ÿ�� ����
	void SpawnTile(int _i, int _a, float _x, float _z, DirectX::XMFLOAT3 _mapPos, Transform _parent, bool _isRoad, int _height);
	// ���� ��ġ�� �����¿��� ����� ���� ���� ����Ͽ� ���� ū ���� ���̸� ���ϰ�
	// ���� �� ���� ���̰� 1 �ʰ���� ���� ��ġ�� ��� ���̸� 1 �÷��� ��Ų��.
	int** SmoothHeights(int** curentMap, int _x, int _z);
	// �����¿츦 ��ȸ�ϸ�
	// ���� ����Ʈ�� ������� 0 ~ 2��ŭ ���̸� ��½�Ų��.
	int** SmoothHeightDown(int** curentMap, int _x, int _z);
	//  RaiseHeight(heightMap, hX, hZ, maxHeightSize, maxHeightInTiles, holesMap, 0, _heightSmoothing);
	int** RaiseHeight(int** curentMap, int _x, int _z, int maxSize, int maxHeight, bool** holesMap, int iteration, EnableDisable hs);
	bool** CreateHoles(bool** curentMap, int _x, int _z, int maxSize, int iteration);
	bool IsNeighboringHigher(int** curentMap, int _x, int _z);
	bool IsNeighboringHole(int _x, int _z, bool** curentHoles);
	bool IsHole(int _x, int _z, bool** _holes);

public:
	std::vector<std::vector<bool>> loadRoadsMap()
	{
		std::vector<std::vector<bool>> testRoadMap;
		testRoadMap.resize(xSize);
		for (UINT i = 0; i < xSize; i++)
		{
			testRoadMap[i].resize(zSize);
			for (UINT j = 0; j < zSize; j++)
			{
				testRoadMap[i][j] = lastRoadsMap[i][j];
			}
		}

		return testRoadMap;
	}
	std::vector<std::vector<int>>loadHeightMap()
	{
		std::vector<std::vector<int>> testHeightMap;
		testHeightMap.resize(xSize);
		for (UINT i = 0; i < xSize; i++)
		{
			testHeightMap[i].resize(zSize);
			for (UINT j = 0; j < zSize; j++)
			{
				testHeightMap[i][j] = lastHeightMap[i][j];
			}
		}

		return testHeightMap;
	}
	std::vector<std::vector<bool>> loadHolesMap()
	{
		std::vector<std::vector<bool>> testHolesMap;
		testHolesMap.resize(xSize);
		for (UINT i = 0; i < xSize; i++)
		{
			testHolesMap[i].resize(zSize);
			for (UINT j = 0; j < zSize; j++)
			{
				testHolesMap[i][j] = lastHolesMap[i][j];
			}
		}

		return testHolesMap;
	}
	std::vector<std::vector<bool>> loadLaddersMap()
	{
		std::vector<std::vector<bool>> testLaddersMap;
		testLaddersMap.resize(xSize);
		for (UINT i = 0; i < xSize; i++)
		{
			testLaddersMap[i].resize(zSize);
			for (UINT j = 0; j < zSize; j++)
			{
				testLaddersMap[i][j] = lastLaddersMap[i][j];
			}
		}

		return testLaddersMap;
	}

public:
	// StartGenerator Coroutine
	// NewMap

	// GenerateMap
	void GenerateMap(
		std::string _mapName,
		DirectX::XMFLOAT3 _mapPos,
		int _mapSize,
		GeneratorParametr _holesCount,
		GeneratorParametr _holesSizes,
		GeneratorParametr _heightsCount,
		GeneratorParametr _heightsSizes,
		GeneratorParametr _maxHeight,
		EnableDisable _heightSmoothing,
		EnableDisable _POIs,
		int _POIsCount,
		EnableDisable _roads,
		int _roadsFilling,
		int _roadsFenceChance,
		int _roadsBetweenPOI,
		EnableDisable _ladders,
		int _laddersChance)
	{
		Object* map = new Object();
		map->name = _mapName;
		map->transform.position = DirectX::XMLoadFloat3(&_mapPos);

		xSize = _mapSize;
		zSize = _mapSize;

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


		bool** roadsMap = new bool*[xSize];
		int** heightMap = new int*[xSize];
		bool** holesMap = new bool*[xSize];
		bool** laddersMap = new bool*[xSize];

		for (UINT i = 0; i < xSize; i++)
		{
			roadsMap[i]		= new bool[zSize];
			heightMap[i]	= new int[zSize];
			holesMap[i]		= new bool[zSize];
			laddersMap[i]	= new bool[zSize];
		}

		for (UINT i = 0; i < xSize; i++)
		{
			for (UINT j = 0; j < zSize; j++)
			{
				roadsMap[i][j]		= false;
				heightMap[i][j]		= 0;
				holesMap[i][j]		= false;
				laddersMap[i][j]	= false;
			}
		}

		std::vector<std::array<int, 2>> pointsOfInterest;

		DirectX::XMVECTOR leftDownCorner = { 0, 0 };
		DirectX::XMVECTOR rightTopCorner = { xSize, zSize };
		float maxMapDistance = sqrt(pow(xSize, 2) + pow(zSize, 2));

		if (_POIs == EnableDisable::Enabled)
		{
			for (int i = 0; i < _POIsCount; i++)
			{
				int trys = 0;
				while (true)
				{
					trys++;

					std::array<int, 2> newPoi = { 
						Random::Rangefloat(0.0f, xSize), 
						Random::Rangefloat(0.0f, zSize) 
					};

					// Skip - newPoi�� pointsOfInterest�� �������� ���� ��
					float minDistance = -1.0f;
					for (int a = 0; a < pointsOfInterest.size(); a++)
					{
						DirectX::XMVECTOR firstPoint = { pointsOfInterest[a][0], pointsOfInterest[a][1] };
						DirectX::XMVECTOR secondPoint = { newPoi[0], newPoi[1] };
						float distance = sqrt(
							pow(newPoi[0] - pointsOfInterest[a][0], 2) +
							pow(newPoi[1] - pointsOfInterest[a][1], 2)
						);

						if (distance < minDistance || minDistance < 0.0f) 
							minDistance = distance;
					}

					if (minDistance > (maxMapDistance / 4.0f) ||
						minDistance < 0.0f ||
						trys > xSize * zSize)
					{
						pointsOfInterest.push_back(newPoi);
						roadsMap[newPoi[0]][newPoi[1]] = true;
						trys = 0;
						break;
					}
				}
			}
		}

		if (_roads == EnableDisable::Enabled)
		{
			// ������ ������ POI�� ��������
			for (int i = 0; i < pointsOfInterest.size(); i++)
			{
				for (int a = i; a < pointsOfInterest.size(); a++)
				{
					if (i != a)
					{
						bool createThisConnection = true;

						if (i == 0 && a == 1) 
						{

						}
						else
						{
							createThisConnection = Random::RangeInt(0, 100) < _roadsBetweenPOI;
						}

						if (createThisConnection)
						{
							int minX = pointsOfInterest[i][0] < pointsOfInterest[a][0] ? pointsOfInterest[i][0] : pointsOfInterest[a][0];
							int maxX = pointsOfInterest[i][0] < pointsOfInterest[a][0] ? pointsOfInterest[a][0] : pointsOfInterest[i][0];
							int minZ = pointsOfInterest[i][1] < pointsOfInterest[a][1] ? pointsOfInterest[i][1] : pointsOfInterest[a][1];
							int maxZ = pointsOfInterest[i][1] < pointsOfInterest[a][1] ? pointsOfInterest[a][1] : pointsOfInterest[i][1];

							bool down = Random::RangeInt(0, 100) < 50;

							bool left = true;
							if (pointsOfInterest[i][1] > pointsOfInterest[a][1])
							{
								if (pointsOfInterest[i][0] < pointsOfInterest[a][0])
								{
									if (down)
										left = true;
									else
										left = false;
								}
								else
								{
									if (down)
										left = false;
									else
										left = true;
								}
							}
							else
							{
								if (pointsOfInterest[a][0] < pointsOfInterest[i][0])
								{
									if (down)
										left = true;
									else
										left = false;
								}
								else
								{
									if (down)
										left = false;
									else
										left = true;
								}
							}

							for (int p = minX; p <= maxX; p++)
							{
								if (down)
									roadsMap[p][minZ] = true;
								else
									roadsMap[p][maxZ] = true;
							}
							for (int p = minZ; p < maxZ; p++)
							{
								if (left)
									roadsMap[minX][p] = true;
								else
									roadsMap[maxX][p] = true;
							}
						}
					}
				}
			}
		}

		// HOLES CREATING
		if (_holesCount != GeneratorParameter::None)
		{
			// ������ ������ �ɼǿ� ���� �����Ѵ�.
			float holesMultiplier = 0.0f;

			// �ɼǿ� ���� ��Ƽ�ö��� ���� ���´�.
			if (_holesCount == GeneratorParametr::Random) _holesCount = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_holesCount == GeneratorParametr::VeryLow) holesMultiplier = 0.1f;
			else if (_holesCount == GeneratorParametr::Low) holesMultiplier = 0.2f;
			else if (_holesCount == GeneratorParametr::Medium) holesMultiplier = 0.3f;
			else if (_holesCount == GeneratorParametr::High) holesMultiplier = 0.4f;
			else if (_holesCount == GeneratorParametr::VeryHigh) holesMultiplier = 0.5f;

			// ������ ũ�⸦ �ɼǿ� ���� �����Ѵ�
			float holesSizesMultiplier = 0.0f;

			// �ɼǿ� ���� ��Ƽ�ö��� ���� ���´�.
			if (_holesSizes == GeneratorParametr::Random || _holesSizes == GeneratorParametr::None) _holesSizes = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_holesSizes == GeneratorParametr::VeryLow) holesSizesMultiplier = 0.1f;
			else if (_holesSizes == GeneratorParametr::Low) holesSizesMultiplier = 0.25f;
			else if (_holesSizes == GeneratorParametr::Medium) holesSizesMultiplier = 0.5f;
			else if (_holesSizes == GeneratorParametr::High) holesSizesMultiplier = 0.85f;
			else if (_holesSizes == GeneratorParametr::VeryHigh) holesSizesMultiplier = 1.0f;

			// ���� ûũ�� ���簢���̸�, x�� z �� � ���� ���̰� ������
			int minSide = zSize < xSize ? zSize : xSize;

			// ������ ������ ���Ѵ�.
			int holesCountToCreate = (int)((float)minSide * holesMultiplier);
			// ������ ����� ���Ѵ�.
			int maxHoleSize = (int)(((float)minSide * 0.3f) * holesSizesMultiplier);

			// ������ ������ŭ Ȧ�� �����Ѵ�.
			for (int i = 0; i < holesCountToCreate; i++)
			{
				// �������� ������ ��ġ�� ���ϰ�
				int hX = Random::RangeInt(0, xSize);
				int hZ = Random::RangeInt(0, zSize);

				// ������ ����
				holesMap = CreateHoles(holesMap, hX, hZ, maxHoleSize, 0);
			}
		}

		//-------------------------------------------------------------

		//HEIGHTS CREATING --------------------------------------------
		// ���� �Ķ���Ͱ� None�� �ƴϸ�
		if (_heightsCount != GeneratorParametr::None)
		{
			// ���� ���
			float heightsMultiplier = 0.0f;

			// �����̸� ���� ����� (1~6)�� ��������
			// ������ �ƴϸ� ������ �Ķ���ͷ� heightsMultiplier�� ����
			if (_heightsCount == GeneratorParametr::Random) _heightsCount = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_heightsCount == GeneratorParametr::VeryLow) heightsMultiplier = 0.1f;
			else if (_heightsCount == GeneratorParametr::Low) heightsMultiplier = 0.2f;
			else if (_heightsCount == GeneratorParametr::Medium) heightsMultiplier = 0.3f;
			else if (_heightsCount == GeneratorParametr::High) heightsMultiplier = 0.4f;
			else if (_heightsCount == GeneratorParametr::VeryHigh) heightsMultiplier = 0.5f;

			// ���� ������ ��� 
			float heightsSizesMultiplier = 0.0f;

			if (_heightsSizes == GeneratorParametr::Random || _heightsSizes == GeneratorParametr::None) _heightsSizes = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_heightsSizes == GeneratorParametr::VeryLow) heightsSizesMultiplier = 0.1f;
			else if (_heightsSizes == GeneratorParametr::Low) heightsSizesMultiplier = 0.25f;
			else if (_heightsSizes == GeneratorParametr::Medium) heightsSizesMultiplier = 0.5f;
			else if (_heightsSizes == GeneratorParametr::High) heightsSizesMultiplier = 0.85f;
			else if (_heightsSizes == GeneratorParametr::VeryHigh) heightsSizesMultiplier = 1.0f;

			// ���� ûũ�� ���簢���� ��� ���ο� ������ �� ª�� ���� ����
			int minSide = zSize < xSize ? zSize : xSize;
			// ª�� ���� ���̿� ���߾� �ִ� ���� ����
			int heightsCountToCreate = (int)((float)minSide * heightsMultiplier);
			// 
			int maxHeightSize = (int)(((float)minSide * 0.4f) * heightsSizesMultiplier);

			// Ÿ�� �� �ִ� ����
			int maxHeightInTiles = 0;

			if (_maxHeight == GeneratorParametr::Random || _maxHeight == GeneratorParametr::None) _maxHeight = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_maxHeight == GeneratorParametr::VeryLow) maxHeightInTiles = 1;
			else if (_maxHeight == GeneratorParametr::Low) maxHeightInTiles = 2;
			else if (_maxHeight == GeneratorParametr::Medium) maxHeightInTiles = 3;
			else if (_maxHeight == GeneratorParametr::High) maxHeightInTiles = 4;
			else if (_maxHeight == GeneratorParametr::VeryHigh) maxHeightInTiles = 5;

			// 
			for (int i = 0; i < heightsCountToCreate; i++)
			{
				// �������� ���� ���� �� ��ġ �����ϰ�
				int hX = Random::RangeInt(0, xSize);
				int hZ = Random::RangeInt(0, zSize);
				// ���� ����
				heightMap = RaiseHeight(heightMap, hX, hZ, maxHeightSize, maxHeightInTiles, holesMap, 0, _heightSmoothing);
			}
		}

		//-------------------------------------------------------------

		//HEIGHT SMOOTING----------------------------------------------
		if (_heightSmoothing == EnableDisable::Enabled)
		{
			// ��ü ���� ��ȸ�ϸ鼭
			for (UINT i = 0; i < xSize; i++)
			{
				for (UINT a = 0; a < zSize; a++)
				{
					// ���̰��� �ϸ��ϰ� �����.
					SmoothHeights(heightMap, i, a);
				}
			}
		}
		//-------------------------------------------------------------

		//ROADS-------------------------------------------------------- 
		float roadsSumHeights = 0.0f;
		// ûũ�� ��� ����� ��ȸ�ϸ�
		for (UINT i = 0; i < xSize; i++)
		{
			for (UINT a = 0; a < zSize; a++)
			{
				// ��� ����� ������ ���� ���Ѵ�.
				roadsSumHeights += heightMap[i][a];
			}
		}

		// �׸��� ������ ����� ���Ѵ�.
		int roadsHeight = (int)(ceil(roadsSumHeights / (xSize * zSize)));

		// ���� POI�� isRoad�� Enabled �Ǿ��ٸ�
		if (_POIs == EnableDisable::Enabled && _roads == EnableDisable::Enabled)
		{
			// POI ����Ʈ�� ��ȸ�ϸ�
			for (int i = 0; i < pointsOfInterest.size(); i++)
			{
				// POI ��ġ�� ���
				int xPos = pointsOfInterest[i][1];
				int zPos = pointsOfInterest[i][0];
				// ������ �Ųٰ�
				holesMap[xPos][zPos] = false;
				// �����¿� ��ȸ�� �ϸ�
				// ���� POI�� �����¿� ����� ��� ������ ��źȭ �Ѵ�.
				if (xPos + 1 < xSize)
				{
					heightMap[xPos + 1][zPos] = roadsHeight;
					if (zPos + 1 < zSize)
					{
						heightMap[xPos + 1][zPos + 1] = roadsHeight;
					}
					if (zPos - 1 > 0)
					{
						heightMap[xPos + 1][zPos - 1] = roadsHeight;
					}
				}
				if (zPos + 1 < zSize)
				{
					heightMap[xPos][zPos + 1] = roadsHeight;
				}
				if (xPos - 1 > 0)
				{
					heightMap[xPos - 1][zPos] = roadsHeight;
					if (zPos + 1 < zSize)
					{
						heightMap[xPos - 1][zPos + 1] = roadsHeight;
					}
					if (zPos - 1 > 0)
					{
						heightMap[xPos - 1][zPos - 1] = roadsHeight;
					}
				}
				if (zPos - 1 > 0)
				{
					heightMap[xPos][zPos - 1] = roadsHeight;
				}
			}

			// �׸��� ��ü ûũ�� ��ȸ�ϸ�
			for (UINT i = 0; i < xSize; i++)
			{
				for (UINT a = 0; a < zSize; a++)
				{
					// ���� �ε���� �ڸ����
					if (roadsMap[i][a])
					{
						// ���̸� ������� ��źȭ ��Ű��
						heightMap[a][i] = roadsHeight;
						// �����¿츦 ��ȸ�ϸ�
						// ���� ����Ʈ�� ��Ϻ��� ���̸� 0 ~ 2��ŭ ��� ��Ų��.
						SmoothHeightDown(heightMap, a, i);
					}
				}
			}
		}
		//-------------------------------------------------------------

		//LADDERS------------------------------------------------------
		if (_ladders == EnableDisable::Enabled)
		{
			// Generate Ladders
			for (int i = 0; i < xSize; i++)
			{
				for (int a = 0; a < zSize; a++)
				{
					// if the position is not hole
					if (!holesMap[i][a])
					{
						// got a height of Bottom.
						int myHeight = heightMap[a][i];

						bool right = false;
						bool left = false;
						bool up = false;
						bool down = false;

						// �����¿츦 ��ȸ�ϸ�
						// ���� ���� ����� ���̺��� 1��ŭ ��ٸ� �ش� ������ Boolean���� true�� �����.
						if (i + 1 < xSize)
						{
							if (!holesMap[i + 1][a] && !laddersMap[i + 1][a])
							{
								if (heightMap[a][i + 1] == (myHeight + 1)) right = true;
							}
						}
						if (i - 1 >= 0)
						{
							if (!holesMap[i - 1][a] && !laddersMap[i - 1][a])
							{
								if (heightMap[a][i - 1] == (myHeight + 1)) left = true;
							}
						}
						if (a + 1 < zSize)
						{
							if (!holesMap[i][a + 1] && !laddersMap[i][a + 1])
							{
								if (heightMap[a + 1][i] == (myHeight + 1)) up = true;
							}
						}
						if (a - 1 >= 0)
						{
							if (!holesMap[i][a - 1] && !laddersMap[i][a - 1])
							{
								if (heightMap[a - 1][i] == (myHeight + 1)) down = true;
							}
						}

						// ���� �����¿� �� ���� ����� ���̺��� 1��ŭ ū ����� "�� �ϳ���" ���� �Ѵٸ�
						// ���� ���� ���̸� ħ���ϴ��� ���θ� �˻��ϰ�
						// 
						// ��, ���� ���, �� �Ǵ� ��, ���� ���, �쿡 ������ ���� ��
						// y = 0 �׸��� needSpawn true
						float y = 0;
						bool needSpawn = false;
						if (right && !left && !down && !up) //Ladder to right
						{
							if (i - 1 >= 0)
							{
								if (heightMap[a][i - 1] == myHeight)
								{
									if (!IsHole(i - 1, a, holesMap) && !IsHole(i + 1, a, holesMap) && !IsHole(i, a, holesMap))
									{
										y = 0;
										needSpawn = true;
									}
								}
							}
						}
						if (!right && left && !down && !up) //Ladder to left
						{
							if (i + 1 < xSize)
							{
								if (heightMap[a][i + 1] == myHeight)
								{
									if (!IsHole(i - 1, a, holesMap) && !IsHole(i + 1, a, holesMap) && !IsHole(i, a, holesMap))
									{
										y = 180.0f;
										needSpawn = true;
									}
								}
							}
						}
						if (!right && !left && down && !up) //Ladder to down
						{
							if (a + 1 < zSize)
							{
								if (heightMap[a + 1][i] == myHeight)
								{
									if (!IsHole(i, a - 1, holesMap) && !IsHole(i, a + 1, holesMap) && !IsHole(i, a, holesMap))
									{
										y = 90.0f;
										needSpawn = true;
									}
								}
							}
						}
						if (!right && !left && !down && up) //Ladder to up
						{
							if (a - 1 >= 0)
							{
								if (heightMap[a][a - 1] == myHeight)
								{
									if (!IsHole(i, a - 1, holesMap) && !IsHole(i, a + 1, holesMap) && !IsHole(i, a, holesMap))
									{
										y = -90.0f;
										needSpawn = true;
									}
								}
							}
						}

						if (needSpawn)
						{
							if (Random::RangeInt(0, 100) < _laddersChance)
							{
								laddersMap[i][a] = true;
								Object* ladder = Instantiate(laddersTiles[Random::RangeInt(0, laddersTiles.size())]);
								ladder->transform.position = { 
									i * 2.0f, 
									_mapPos.y + (heightMap[a][i] * 2.0f), 
									a * 2.0f 
								};

								ladder->transform.setParent(map->transform);
								ladder->transform.rotation = { 0.0f, y, 0.0f };
							}
						}
					}
				}
			}
		}
		//-------------------------------------------------------------

		//SPAWNING MAP-------------------------------------------------
		float x = 0.0f;
		float z = 0.0f;

		for (int i = 0; i < xSize; i++)
		{
			for (int a = 0; a < zSize; a++)
			{
				// ������ ���� ���
				if (!holesMap[i][a])
				{
					// 
					SpawnTile(
						i,
						a,
						x,
						z,
						mapPosition,
						map->transform,
						roadsMap[a][i],
						heightMap[i][a]
					);
				}

				x += 2.0f;
			}
			z += 2.0f;
			x = 0.0f;
		}
		//-------------------------------------------------------------

		//POIS SPAWNING------------------------------------------------
		if (_POIs == EnableDisable::Enabled)
		{
			// ��� POI�� ��ȸ�Ѵ�.
			for (int i = 0; i < pointsOfInterest.size(); i++)
			{
				// POI ��ġ�� ���
				int xPos = pointsOfInterest[i][1];
				int zPos = pointsOfInterest[i][0];
				Object* poiObj = nullptr;

				// POI start, end
				if (i == 0) poiObj = Instantiate(startTile);
				else if (i == 1) poiObj = Instantiate(endTile);
				// POI �� �������� �ϳ� ��� ������
				else poiObj = Instantiate(interestPointTiles[Random::RangeInt(0, interestPointTiles.size())]);

				// ���� ���̸� ����Ͽ� �������� ����
				poiObj->transform.position = { 
					zPos * 2.0f, 
					_mapPos.y + (heightMap[xPos][zPos] * 2.0f), 
					xPos * 2.0f 
				};
				poiObj->transform.setParent(map->transform);

				xPos = pointsOfInterest[i][0];
				zPos = pointsOfInterest[i][1];

				// �����¿츦 ��ȸ�ϸ�
				// �ش� ��ġ�� ���ΰ� �����Ѵٸ� �ش� ��ġ�� ���ΰ� �̾��� �� �ֵ��� ȸ��
				if (xPos + 1 < xSize)
				{
					if (roadsMap[xPos + 1][zPos])
					{
						poiObj->transform.rotation = { 0.0f, -90.0f, 0.0f };
					}
				}
				if (zPos + 1 < zSize)
				{
					if (roadsMap[xPos][zPos + 1])
					{
						poiObj->transform.rotation = { 0.0f, -180.0f, 0.0f };
					}
				}
				if (xPos - 1 > 0)
				{
					if (roadsMap[xPos - 1][zPos])
					{
						poiObj->transform.rotation = { 0.0f, -270.0f, 0.0f };
					}
				}
				if (zPos - 1 > 0)
				{
					if (roadsMap[xPos][zPos - 1])
					{
						poiObj->transform.rotation = { 0.0f, 0.0f, 0.0f };
					}
				}
			}
		}
		//-------------------------------------------------------------

		//ROADS SPAWNING---------------------------------------------
		if (_POIs == EnableDisable::Enabled && _roads == EnableDisable::Enabled)
		{
			int bridgeNumber = -1;
			if (roadBridges.size() > 0) bridgeNumber = Random::RangeInt(0, roadBridges.size());
			for (int i = 0; i < xSize; i++)
			{
				for (int a = 0; a < zSize; a++)
				{
					if (roadsMap[i][a])
					{
						int xPos = i;
						int zPos = a;

						bool right = false;
						bool left = false;
						bool up = false;
						bool down = false;

						if (xPos + 1 < xSize) //RightTile checking
						{
							if (roadsMap[xPos + 1][zPos] && !holesMap[a][i + 1]) //RightTile road +
							{
								right = true;
							}
						}
						if (xPos - 1 >= 0)
						{
							if (roadsMap[xPos - 1][zPos] && !holesMap[a][i - 1])
							{
								left = true;
							}
						}
						if (zPos + 1 < zSize)
						{
							if (roadsMap[xPos][zPos + 1] && !holesMap[a + 1][i])
							{
								up = true;
							}
						}
						if (zPos - 1 >= 0)
						{
							if (roadsMap[xPos][zPos - 1] && !holesMap[a - 1][i])
							{
								down = true;
							}
						}

						if (up && down && !IsHole(i, a, holesMap))
						{
							if (Random::RangeInt(0, 100) < (100 - _roadsFilling))
								roadsMap[i][a] = false;
						}
						else if (left && right && !IsHole(i, a, holesMap))
						{
							if (Random::RangeInt(0, 100) < (100 - _roadsFilling))
								roadsMap[i][a] = false;
						}
					}
				}
			}

			for (int i = 0; i < xSize; i++)
			{
				for (int a = 0; a < zSize; a++)
				{
					if (roadsMap[i][a] && RoadIsNotPOI(i, a, pointsOfInterest))
					{
						int xPos = i;
						int zPos = a;
						float yEulers = 0.0f;
						bool spawned = true;

						Object* road = nullptr;

						bool right = false;
						bool left = false;
						bool up = false;
						bool down = false;

						if (xPos + 1 < xSize) //RightTile checking
						{
							if (roadsMap[xPos + 1][zPos]) //RightTile road +
							{
								right = true;
							}
						}
						if (xPos - 1 >= 0)
						{
							if (roadsMap[xPos - 1][zPos])
							{
								left = true;
							}
						}
						if (zPos + 1 < zSize)
						{
							if (roadsMap[xPos][zPos + 1])
							{
								up = true;
							}
						}
						if (zPos - 1 >= 0)
						{
							if (roadsMap[xPos][zPos - 1])
							{
								down = true;
							}
						}

						if (up && down && right && left)
						{
							road = Instantiate(roadCrossroad);
							yEulers = 90.0f * Random::RangeInt(0, 4);
						}
						else if (up && down && right)
						{
							road = Instantiate(roadTriple);
							yEulers = 0.0f;
						}
						else if (up && left && right)
						{
							road = Instantiate(roadTriple);
							yEulers = -90.0f;
						}
						else if (up && left && down)
						{
							road = Instantiate(roadTriple);
							yEulers = -180.0f;
						}
						else if (right && left && down)
						{
							road = Instantiate(roadTriple);
							yEulers = -270.0f;
						}
						else if (right && left)
						{
							if (holesMap[a][i])
							{
								road = Instantiate(roadBridges[bridgeNumber]);
								yEulers = 90.0f;
							}
							else
							{
								if (RoadIsNotPOI(i + 1, a, pointsOfInterest) && RoadIsNotPOI(i - 1, a, pointsOfInterest))
								{
									road = Instantiate(roadStraight);
									//if (Random::RangeInt(0, 100) < _roadsFenceChance)
									//	road.transform.GetChild(1).gameObject.SetActive(true);
									//if (Random::RangeInt(0, 100) < _roadsFenceChance)
									//	road.transform.GetChild(2).gameObject.SetActive(true);

									yEulers = 90.0f;
								}
								else
								{
									if (holesMap[a][i])
										road = Instantiate(roadBridges[bridgeNumber]);
									else
										road = Instantiate(roadEnd);

									if (RoadIsNotPOI(i + 1, a, pointsOfInterest)) yEulers = -90.0f;
									else yEulers = 90.0f;
								}
							}
						}
						else if (up && down)
						{
							if (holesMap[a][i])
							{
								road = Instantiate(roadBridges[bridgeNumber]);
								yEulers = 0.0f;
							}
							else
							{
								if (RoadIsNotPOI(i + 1, a, pointsOfInterest) && RoadIsNotPOI(i - 1, a, pointsOfInterest))
								{
									road = Instantiate(roadStraight);
									//if (Random::RangeInt(0, 100) < _roadsFenceChance)
									//	road.transform.GetChild(1).gameObject.SetActive(true);
									//if (Random::RangeInt(0, 100) < _roadsFenceChance)
									//	road.transform.GetChild(2).gameObject.SetActive(true);

									yEulers = 0.0f;
								}
								else
								{
									if (holesMap[a][i])
										road = Instantiate(roadBridges[bridgeNumber]);
									else
										road = Instantiate(roadEnd);

									if (RoadIsNotPOI(i + 1, a, pointsOfInterest)) yEulers = -90.0f;
									else yEulers = 90.0f;
								}
							}
						}
						else if (right && down)
						{
							road = Instantiate(roadRotate);
							yEulers = 0.0f;
						}
						else if (right && up)
						{
							road = Instantiate(roadRotate);
							yEulers = -90.0f;
						}
						else if (left && up)
						{
							road = Instantiate(roadRotate);
							yEulers = 180.0f;
						}
						else if (left && down)
						{
							road = Instantiate(roadRotate);
							yEulers = 90.0f;
						}
						else if (up)
						{
							if (holesMap[a][i])
								road = Instantiate(roadBridges[bridgeNumber]);
							else
								road = Instantiate(roadEnd);
							yEulers = 180.0f;
						}
						else if (down)
						{
							if (holesMap[a][i])
								road = Instantiate(roadBridges[bridgeNumber]);
							else
								road = Instantiate(roadEnd);
							yEulers = 0.0f;
						}
						else if (right)
						{
							if (holesMap[a][i])
								road = Instantiate(roadBridges[bridgeNumber]);
							else
								road = Instantiate(roadEnd);
							yEulers = -90.0f;
						}
						else if (left)
						{
							if (holesMap[a][i])
								road = Instantiate(roadBridges[bridgeNumber]);
							else
								road = Instantiate(roadEnd);
							yEulers = 90.0f;
						}
						else
						{
							spawned = false;
						}

						//Additional Tiles
						if ((left && down) || (right && down) || (right && up) || (up && left) || (up && down && right && left) ||
							(up && down && right) || (up && left && right) || (up && left && down) || (right && left && down))
						{
							if (holesMap[a][i])
							{
								SpawnTile(i, a, (i * 2), (a * 2), mapPosition, map->transform, true, heightMap[a][i]);
							}
						}

						if (spawned)
						{
							road->transform.position = { 
								i * 2.0f, 
								_mapPos.y + (heightMap[a][i] * 2.0f), 
								a * 2.0f 
							};
							road->transform.setParent(map->transform);
							road->transform.rotation = { 0.0f, yEulers, 0.0f };
						}
					}
				}
			}
		}
		//-------------------------------------------------------------

		lastRoadsMap = roadsMap;
		lastHeightMap = heightMap;
		lastHolesMap = holesMap;
		lastLaddersMap = laddersMap;
		lastMap = *map;
	}
	// ��Ÿ �ڿ��� �������� ��ġ 
	void AdditionalFilling(
		GeneratorParametr _additionalFilling, 
		int sizeOfMap, 
		std::list<DirectX::BoundingBox>& boundBoxs,
		RenderItem* treeObject,
		RenderItem* bushsObject,
		RenderItem* bigStonesObject,
		RenderItem* grassObject,
		RenderItem* branchsObject,
		RenderItem* logsObject
	)
	{
		if (_additionalFilling == GeneratorParametr::Random) 
			_additionalFilling = (GeneratorParametr)Random::RangeInt(1, 6);
		if (_additionalFilling != GeneratorParametr::None)
		{
			// �ϳ��� ûũ�� 1/25ũ��� ������ _additionalFilling ��ŭ ������Ʈ�� �����Ѵ�.
			// _additionalFilling�� Ŭ ���� �ϳ��� ������ ���� ������ �ڿ��� ��ġ�� Ȯ���� ũ��.
			int countsCycle = (int)(((float)sizeOfMap / 5.0f)) * (int)_additionalFilling;

			// ���� ũ��
			// _additionalFilling�� Ŭ ���� ���� ���� �κп� ������Ʈ�� �����Ѵ�.
			float circlesRange = ((float)sizeOfMap / 6.0f) + (((float)sizeOfMap / 30.0f) * (int)_additionalFilling);

			// �� �ϳ� �� ������Ʈ ����
			float objectsCounts = sizeOfMap / 2.5f + ((sizeOfMap / 6) * (int)_additionalFilling);

			DirectX::XMVECTOR rayOrigin;
			DirectX::XMVECTOR rayDown = { 0.0f, -1.0f, 0.0f, 0.0f };
			float tmin = 0.0f;

			// ��� ���繰�� transform�� ��� ����Ʈ
			std::vector<DirectX::XMFLOAT3> treesPoints;
			std::vector<DirectX::XMFLOAT3> bushsPoints;
			std::vector<DirectX::XMFLOAT3> bigStonesPoints;
			std::vector<DirectX::XMFLOAT3> grassPoint;
			std::vector<DirectX::XMFLOAT3> branchsPoints;
			std::vector<DirectX::XMFLOAT3> logsPoints;

			// ����Ŭ ��ŭ �ݺ��ϸ鼭 ������Ʈ�� ����
			for (int a = 0; a < countsCycle; a++)
			{
				// �� ������ �������� ������Ʈ�� ���� ��ġ�� ����.
				// y�� 15�� ������ ���߿��� ���̸� ��, �ٴڿ� ���� ��� ������Ʈ�� ���� ��Ű�� ����
				DirectX::XMFLOAT3 circleTreesPos = {
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f),
					15.0f,
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f)
				};

				tmin = 0.0f;
				// �� �ϳ��� ������Ʈ ���� ��ŭ ����
				for (int i = 0; i < objectsCounts; i++) //Trees
				{
					DirectX::XMFLOAT3 hitPoint;

					// ���߿��� ���� x, z�� �������� �ϴ� ���� ���� ������ circlesRange ���� �������� �� �ϳ��� ����
					DirectX::XMFLOAT3 rayPos = circleTreesPos;
					rayPos.x += Random::Rangefloat(-circlesRange, circlesRange);
					rayPos.z += Random::Rangefloat(-circlesRange, circlesRange);

					rayOrigin = DirectX::XMLoadFloat3(&rayPos);

					// �� ������ ���� ���� �Ʒ��������� ���̸� ������ ���� �� �κп� ���� ������Ʈ�� ����
					if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
					{
						// �������� �����Ǿ� ������ ����� �����ֱ� ����.
						bool isInnerPoint = IsPosAvailableByDistance(hitPoint, treesPoints, 1.5f);
						bool isInnerPOI = isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap);

						if (isInnerPoint && isInnerPOI)
						{
							// �������� ���� �ϳ��� ��� ��Ʈ ����Ʈ�� �ɴ´�
							Object* tree = Instantiate(
								treesPrefabs[Random::RangeInt(0, treesPrefabs.size())],
								hitPoint,
								lastMap.transform
							);
							tree->transform.rotation = {
								Random::Rangefloat(-7.5f, 7.5f),
								Random::Rangefloat(0.0f, 360.0f),
								Random::Rangefloat(-7.5f, 7.5f)
							};
							// ��� ���� ��ġ ����
							treesPoints.push_back(hitPoint);
						}
					}
				}

				tmin = 0.0f;
				for (int i = 0; i < objectsCounts / 3; i++) //Bushs
				{
					DirectX::XMFLOAT3 hitPoint;

					DirectX::XMFLOAT3 rayPos = circleTreesPos;
					rayPos.x += Random::Rangefloat(-circlesRange, circlesRange);
					rayPos.z += Random::Rangefloat(-circlesRange, circlesRange);

					rayOrigin = DirectX::XMLoadFloat3(&rayPos);

					if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
					{
						if (IsPosAvailableByDistance(hitPoint, bushsPoints, 2.0f) &&
							isPosInRangeOf(hitPoint, treesPoints, 4.0f) &&
							isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap)
							)
						{
							Object* tree = Instantiate(
								bushsPrefabs[Random::RangeInt(0, bushsPrefabs.size())], 
								hitPoint, 
								lastMap.transform
							);
							tree->transform.rotation = { 0.0f, Random::Rangefloat(0.0f, 360.0f), 0.0f };
							bushsPoints.push_back(hitPoint);
						}
					}
				}

				tmin = 0.0f;
				for (int i = 0; i < objectsCounts * 4; i++) //Grass
				{
					DirectX::XMFLOAT3 hitPoint;

					DirectX::XMFLOAT3 rayPos = circleTreesPos;
					rayPos.x += Random::Rangefloat(-circlesRange, circlesRange);
					rayPos.z += Random::Rangefloat(-circlesRange, circlesRange);

					rayOrigin = DirectX::XMLoadFloat3(&rayPos);

					if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
					{
						if (IsPosAvailableByDistance(hitPoint, grassPoint, 0.25f) &&
							isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap)
							)
						{
							Object* tree = Instantiate(
								grassPrefabs[Random::RangeInt(0, grassPrefabs.size())], 
								hitPoint, 
								lastMap.transform
							);
							tree->transform.rotation = { 0.0f, Random::Rangefloat(0.0f, 360.0f), 0.0f };
							grassPoint.push_back(hitPoint);
						}
					}
				}

				tmin = 0.0f;
				for (int i = 0; i < objectsCounts; i++) //Little stones
				{
					DirectX::XMFLOAT3 hitPoint;

					DirectX::XMFLOAT3 rayPos = circleTreesPos;
					rayPos.x += Random::Rangefloat(-circlesRange, circlesRange);
					rayPos.z += Random::Rangefloat(-circlesRange, circlesRange);

					rayOrigin = DirectX::XMLoadFloat3(&rayPos);

					if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
					{
						if (isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap))
						{
							Object* tree = Instantiate(
								littleStonesPrefabs[Random::RangeInt(0, littleStonesPrefabs.size())],
								hitPoint, 
								lastMap.transform
							);
							tree->transform.rotation = { Random::Rangefloat(0.0f, 360.0f), Random::Rangefloat(0.0f, 360.0f), Random::Rangefloat(0.0f, 360.0f) };
						}
						}
					}
				}

			tmin = 0.0f;
			for (int i = 0; i < ((objectsCounts * countsCycle)*(int)_additionalFilling); i++) //Grass
			{
				DirectX::XMFLOAT3 hitPoint;

				DirectX::XMFLOAT3  rayPos = { 
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f), 
					15.0f, 
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f) 
				};

				rayOrigin = DirectX::XMLoadFloat3(&rayPos);

				if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
				{
					if (IsPosAvailableByDistance(hitPoint, grassPoint, 0.25f) &&
						isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap)
						)
					{
						Object* tree = Instantiate(
							grassPrefabs[Random::RangeInt(0, grassPrefabs.size())],
							hitPoint, 
							lastMap.transform
						);
						tree->transform.rotation = { 0.0f, Random::Rangefloat(0.0f, 360.0f), 0.0f };
						grassPoint.push_back(hitPoint);
					}
				}
			}

			tmin = 0.0f;
			for (int i = 0; i < objectsCounts / 2; i++) //big stones
			{
				DirectX::XMFLOAT3 hitPoint;

				DirectX::XMFLOAT3  rayPos = { 
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f), 
					15.0f, 
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f) 
				};

				rayOrigin = DirectX::XMLoadFloat3(&rayPos);

				if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
				{
					if (IsPosAvailableByDistance(hitPoint, bigStonesPoints, 10.0f) &&
						isPosInRangeOf(hitPoint, treesPoints, 8.0f) &&
						isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap))
					{
						Object* tree = Instantiate(
							bigStonesPrefabs[Random::RangeInt(0, bigStonesPrefabs.size())],
							hitPoint, 
							lastMap.transform
						);
						tree->transform.rotation = { Random::Rangefloat(0.0f, 360.0f), Random::Rangefloat(0.0f, 360.0f), Random::Rangefloat(0.0f, 360.0f) };
						bigStonesPoints.push_back(hitPoint);
					}
				}
			}

			tmin = 0.0f;
			for (int i = 0; i < objectsCounts / 2; i++) //branchs
			{
				DirectX::XMFLOAT3 hitPoint;

				DirectX::XMFLOAT3 rayPos = { 
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f), 
					15.0f, 
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f) 
				};

				rayOrigin = DirectX::XMLoadFloat3(&rayPos);

				if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
				{
					if (IsPosAvailableByDistance(hitPoint, bigStonesPoints, 10.0f) &&
						isPosInRangeOf(hitPoint, treesPoints, 8.0f) &&
						isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap)
						)
					{
						Object* tree = Instantiate(
							branchsPrefabs[Random::RangeInt(0, branchsPrefabs.size())],
							hitPoint, 
							lastMap.transform
						);
						tree->transform.rotation = { 0.0f, Random::Rangefloat(0.0f, 360.0f), 0.0f };
						branchsPoints.push_back(hitPoint);
					}
				}
			}

			tmin = 0.0f;
			for (int i = 0; i < objectsCounts / 2; i++) //logs
			{
				DirectX::XMFLOAT3 hitPoint;

				DirectX::XMFLOAT3 rayPos = { 
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f), 
					15.0f,
					Random::Rangefloat(0.0f, sizeOfMap * 2.0f) 
				};

				rayOrigin = DirectX::XMLoadFloat3(&rayPos);

				if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
				{
					if (IsPosAvailableByDistance(hitPoint, bigStonesPoints, 10.0f) &&
						isPosInRangeOf(hitPoint, treesPoints, 8.0f) &&
						isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap))
					{
						Object* tree = Instantiate(
							logsPrefabs[Random::RangeInt(0, logsPrefabs.size())],
							hitPoint, 
							lastMap.transform
						);
						tree->transform.rotation = { 0.0f, Random::Rangefloat(0.0f, 360.0f), 0.0f };
						logsPoints.push_back(hitPoint);
					}
				}
			}

			int iter = 0;

			for (iter = 0; iter < bushsPoints.size(); iter++)
				treeObject->Instantiate(
					bushsPoints[iter]
				);
			for (iter = 0; iter < treesPoints.size(); iter++)
				bushsObject->Instantiate(
					treesPoints[iter]
				);
			for (iter = 0; iter < bigStonesPoints.size(); iter++)
				bigStonesObject->Instantiate(
					bigStonesPoints[iter]
				);
			for (iter = 0; iter < grassPoint.size(); iter++)
				grassObject->Instantiate(
					grassPoint[iter]
				);
			for (iter = 0; iter < branchsPoints.size(); iter++)
				branchsObject->Instantiate(
					branchsPoints[iter]
				);
			for (iter = 0; iter < logsPoints.size(); iter++)
				logsObject->Instantiate(
					logsPoints[iter]
				);
		}
	}
};