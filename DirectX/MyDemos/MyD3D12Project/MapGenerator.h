#pragma once
#include "../../Common/d3dUtil.h"

// 청사진 속의 가상의 오브젝트의 '위치' 정보
typedef struct Transform
{
	Transform()
	{
		this->parent = nullptr;
	}
	~Transform()
	{
		if (!this->parent)
			delete(this->parent);
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

// 청사진 속의 가상의 오브젝트의 정보
// Transform을 이용하여 청사진을 그리고, 이후 실제 맵에서 랜더링 할 것
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

// 특정 오브젝트의 생성 개수를 결정하는 파라미터
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

	int xSize, zSize;

public:
	bool isPosNotInPOI(DirectX::XMFLOAT3 posToCheck, bool** roadsMap, bool** laddersMap);
	bool IsPosAvailableByDistance(DirectX::XMFLOAT3 posToCheck, std::vector<DirectX::XMFLOAT3> otherPoses, float minDistance);
	bool isPosInRangeOf(DirectX::XMFLOAT3 posToCheck, std::vector<DirectX::XMFLOAT3> otherPoses, float needDistance);
	bool RoadIsNotPOI(int x, int z, std::vector<std::array<int, 2>> poi);
	// 타일 생성
	void SpawnTile(float _x, float _z, DirectX::XMFLOAT3 _mapPos, Transform _parent, int _height);
	// 주변 높이에 따라, 해당 지형의 높낮이를 재지정하여 높낮이 완화
	int** SmoothHeights(int** curentMap, int _x, int _z);
	// 주변 높낮이를 낮추어, 지형의 높낮이를 완화
	int** SmoothHeightDown(int** curentMap, int _x, int _z);
	// 지형의 높이 지정
	int** RaiseHeight(int** curentMap, int _x, int _z, int maxSize, int maxHeight, bool** holesMap, int iteration, EnableDisable hs);
	// 구멍 생성
	bool** CreateHoles(bool** curentMap, int _x, int _z, int maxSize, int iteration);
	bool IsHole(int _x, int _z, bool** _holes);

public:
	std::vector<std::vector<bool>> loadRoadsMap()
	{
		std::vector<std::vector<bool>> testRoadMap;
		testRoadMap.resize(xSize);
		for (int i = 0; i < xSize; i++)
		{
			testRoadMap[i].resize(zSize);
			for (int j = 0; j < zSize; j++)
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
		for (int i = 0; i < xSize; i++)
		{
			testHeightMap[i].resize(zSize);
			for (int j = 0; j < zSize; j++)
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
		for (int i = 0; i < xSize; i++)
		{
			testHolesMap[i].resize(zSize);
			for (int j = 0; j < zSize; j++)
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
		for (int i = 0; i < xSize; i++)
		{
			testLaddersMap[i].resize(zSize);
			for (int j = 0; j < zSize; j++)
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


		bool** roadsMap = new bool*[xSize];
		int** heightMap = new int*[xSize];
		bool** holesMap = new bool*[xSize];
		bool** laddersMap = new bool*[xSize];

		for (int i = 0; i < xSize; i++)
		{
			roadsMap[i]		= new bool[zSize];
			heightMap[i]	= new int[zSize];
			holesMap[i]		= new bool[zSize];
			laddersMap[i]	= new bool[zSize];
		}

		for (int i = 0; i < xSize; i++)
		{
			for (int j = 0; j < zSize; j++)
			{
				roadsMap[i][j]		= false;
				heightMap[i][j]		= 0;
				holesMap[i][j]		= false;
				laddersMap[i][j]	= false;
			}
		}

		std::vector<std::array<int, 2>> pointsOfInterest;

		DirectX::XMVECTOR leftDownCorner = { 0, 0 };
		DirectX::XMVECTOR rightTopCorner = { (float)xSize, (float)zSize };
		float maxMapDistance = (float)sqrt(pow(xSize, 2) + pow(zSize, 2));

		if (_POIs == EnableDisable::Enabled)
		{
			for (int i = 0; i < _POIsCount; i++)
			{
				int trys = 0;
				while (true)
				{
					trys++;

					std::array<int, 2> newPoi = { 
						Random::Rangefloat(0.0f, (float)(xSize)), 
						Random::Rangefloat(0.0f, (float)(zSize)) 
					};

					// 새로운 POI를 생성하는데, 새로운 POI의 위치가 현재 POI의 위치에 가장 근접해 있는 POI를 기준으로 하여 minDistance를 업데이트
					float minDistance = -1.0f;
					for (int a = 0; a < pointsOfInterest.size(); a++)
					{
						DirectX::XMVECTOR firstPoint = { 
							(float)pointsOfInterest[a][0], 
							(float)pointsOfInterest[a][1]
						};
						DirectX::XMVECTOR secondPoint = { 
							(float)newPoi[0],
							(float)newPoi[1]
						};
						float distance = (float)sqrt(
							pow(newPoi[0] - pointsOfInterest[a][0], 2) +
							pow(newPoi[1] - pointsOfInterest[a][1], 2)
						);

						if (distance < minDistance || minDistance < 0.0f) 
							minDistance = distance;
					}

					// 새로운 POI가 현재 POI보다 특정 기준 떨어져있다면
					// 새로운 POI를 생성.
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
			// 두 POIs 내 위치에 road 생성여부를 결정한다.
			for (int i = 0; i < pointsOfInterest.size(); i++)
			{
				for (int a = i; a < pointsOfInterest.size(); a++)
				{
					// 두 POI가 다를 때 두 POI 내의 영역에 오브젝트 생성 여부를 결정한다.
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
			// 구멍 개수의 배수
			float holesMultiplier = 0.0f;

			if (_holesCount == GeneratorParametr::Random) _holesCount = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_holesCount == GeneratorParametr::VeryLow) holesMultiplier = 0.1f;
			else if (_holesCount == GeneratorParametr::Low) holesMultiplier = 0.2f;
			else if (_holesCount == GeneratorParametr::Medium) holesMultiplier = 0.3f;
			else if (_holesCount == GeneratorParametr::High) holesMultiplier = 0.4f;
			else if (_holesCount == GeneratorParametr::VeryHigh) holesMultiplier = 0.5f;

			// 구멍 사이즈 배수
			float holesSizesMultiplier = 0.0f;

			if (_holesSizes == GeneratorParametr::Random || _holesSizes == GeneratorParametr::None) _holesSizes = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_holesSizes == GeneratorParametr::VeryLow) holesSizesMultiplier = 0.1f;
			else if (_holesSizes == GeneratorParametr::Low) holesSizesMultiplier = 0.25f;
			else if (_holesSizes == GeneratorParametr::Medium) holesSizesMultiplier = 0.5f;
			else if (_holesSizes == GeneratorParametr::High) holesSizesMultiplier = 0.85f;
			else if (_holesSizes == GeneratorParametr::VeryHigh) holesSizesMultiplier = 1.0f;

			// 전체 면적 width, height에서 작은 길이를 얻어온다
			int minSide = zSize < xSize ? zSize : xSize;

			// 구멍 개수 결정
			int holesCountToCreate = (int)((float)minSide * holesMultiplier);
			// 최대 구멍 사이즈 결정
			int maxHoleSize = (int)(((float)minSide * 0.3f) * holesSizesMultiplier);

			for (int i = 0; i < holesCountToCreate; i++)
			{
				// 구멍 위치를 랜덤으로 결정
				int hX = Random::RangeInt(0, xSize);
				int hZ = Random::RangeInt(0, zSize);

				// 구멍 생성
				holesMap = CreateHoles(holesMap, hX, hZ, maxHoleSize, 0);
			}
		}

		//-------------------------------------------------------------

		//HEIGHTS CREATING --------------------------------------------
		if (_heightsCount != GeneratorParametr::None)
		{
			// 지형 높이 배수
			float heightsMultiplier = 0.0f;

			if (_heightsCount == GeneratorParametr::Random) _heightsCount = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_heightsCount == GeneratorParametr::VeryLow) heightsMultiplier = 0.1f;
			else if (_heightsCount == GeneratorParametr::Low) heightsMultiplier = 0.2f;
			else if (_heightsCount == GeneratorParametr::Medium) heightsMultiplier = 0.3f;
			else if (_heightsCount == GeneratorParametr::High) heightsMultiplier = 0.4f;
			else if (_heightsCount == GeneratorParametr::VeryHigh) heightsMultiplier = 0.5f;

			// 지형 최대 높이 배수
			float heightsSizesMultiplier = 0.0f;

			if (_heightsSizes == GeneratorParametr::Random || _heightsSizes == GeneratorParametr::None) _heightsSizes = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_heightsSizes == GeneratorParametr::VeryLow) heightsSizesMultiplier = 0.1f;
			else if (_heightsSizes == GeneratorParametr::Low) heightsSizesMultiplier = 0.25f;
			else if (_heightsSizes == GeneratorParametr::Medium) heightsSizesMultiplier = 0.5f;
			else if (_heightsSizes == GeneratorParametr::High) heightsSizesMultiplier = 0.85f;
			else if (_heightsSizes == GeneratorParametr::VeryHigh) heightsSizesMultiplier = 1.0f;

			// 전체 면적 width, height에서 작은 길이를 얻어온다
			int minSide = zSize < xSize ? zSize : xSize;
			// 높이 개수 결정
			int heightsCountToCreate = (int)((float)minSide * heightsMultiplier);
			// 최대 높이 결정
			int maxHeightSize = (int)(((float)minSide * 0.4f) * heightsSizesMultiplier);

			// 타일의 최대 높이
			int maxHeightInTiles = 0;

			if (_maxHeight == GeneratorParametr::Random || _maxHeight == GeneratorParametr::None) _maxHeight = (GeneratorParametr)Random::RangeInt(1, 6);

			if (_maxHeight == GeneratorParametr::VeryLow) maxHeightInTiles = 1;
			else if (_maxHeight == GeneratorParametr::Low) maxHeightInTiles = 2;
			else if (_maxHeight == GeneratorParametr::Medium) maxHeightInTiles = 3;
			else if (_maxHeight == GeneratorParametr::High) maxHeightInTiles = 4;
			else if (_maxHeight == GeneratorParametr::VeryHigh) maxHeightInTiles = 5;

			// 지형의 높이 업데이트
			for (int i = 0; i < heightsCountToCreate; i++)
			{
				// 랜덤으로 위치를 하나 잡고
				int hX = Random::RangeInt(0, xSize);
				int hZ = Random::RangeInt(0, zSize);
				// 높이 업데이트
				heightMap = RaiseHeight(heightMap, hX, hZ, maxHeightSize, maxHeightInTiles, holesMap, 0, _heightSmoothing);
			}
		}

		//-------------------------------------------------------------

		//HEIGHT SMOOTING----------------------------------------------
		if (_heightSmoothing == EnableDisable::Enabled)
		{
			for (int i = 0; i < xSize; i++)
			{
				for (int a = 0; a < zSize; a++)
				{
					SmoothHeights(heightMap, i, a);
				}
			}
		}
		//-------------------------------------------------------------

		//ROADS-------------------------------------------------------- 
		float roadsSumHeights = 0.0f;
		// 모든 지형의 높이 합을 구함
		for (int i = 0; i < xSize; i++)
		{
			for (int a = 0; a < zSize; a++)
			{
				roadsSumHeights += heightMap[i][a];
			}
		}

		// 모든 지형의 높이의 평균
		int roadsHeight = (int)(ceil(roadsSumHeights / (xSize * zSize)));

		// 지형이 POI이면서 roads 생성이 허용된 경우
		if (_POIs == EnableDisable::Enabled && _roads == EnableDisable::Enabled)
		{
			for (int i = 0; i < pointsOfInterest.size(); i++)
			{
				int xPos = pointsOfInterest[i][1];
				int zPos = pointsOfInterest[i][0];

				// road 생성을 위하여, POI위치의 구멍을 매꾼다
				holesMap[xPos][zPos] = false;

				// poi를 기준으로 상하좌우 평탄화 시킴.
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

			for (int i = 0; i < xSize; i++)
			{
				for (int a = 0; a < zSize; a++)
				{
					// 만일 해당 위치에 로드 생성이 허용된 경우
					if (roadsMap[i][a])
					{
						// 높이를 평탄화시킴.
						heightMap[a][i] = roadsHeight;
						// 주변 경사 또한 스무스 시킴
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

						// 상하좌우의 지형 중 현재 지형보다 높이가 1 높은 지형을 찾아, 해당 지형에 사다리를 둘 것을 명시.
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

						// 사다리가 상,하,좌,우로 생성 되면서 로테이션 되어야 하는 경우가 있기 때문에 y축 회전 저장을 위한 y
						float y = 0;
						// 사다리가 생성 되어야 하는지의 여부
						bool needSpawn = false;

						// 사다리가 상하좌우 중 사다리가 생성되는 방향을 고려하고,
						// 만일 앞에 사다리가 생성되어야 하는 경우, 좌우에 구멍이 뚫려있는지 여부를 검사한 후 해당 검사에 통과하였다면 앞에 사다리를 생성한다.
						// 다른 경우도 다음과 같다.
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

						// 사다리 생성 여부가 확정된경우
						if (needSpawn)
						{
							if (Random::RangeInt(0, 100) < _laddersChance)
							{
								laddersMap[i][a] = true;
								Object* ladder = Instantiate(laddersTiles[Random::RangeInt(0, (int)laddersTiles.size())]);
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
				// 구멍이 없는 지형에 타일을 생성
				if (!holesMap[i][a])
				{
					SpawnTile(
						x,
						z,
						mapPosition,
						map->transform,
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
			for (int i = 0; i < pointsOfInterest.size(); i++)
			{
				int xPos = pointsOfInterest[i][1];
				int zPos = pointsOfInterest[i][0];
				Object* poiObj = nullptr;

				// POI start, end
				if (i == 0) poiObj = Instantiate(startTile);
				else if (i == 1) poiObj = Instantiate(endTile);
				// POI 
				else poiObj = Instantiate(interestPointTiles[Random::RangeInt(0, (int)interestPointTiles.size())]);

				// 새로운 POI의 위치를 지정
				poiObj->transform.position = { 
					zPos * 2.0f, 
					_mapPos.y + (heightMap[xPos][zPos] * 2.0f), 
					xPos * 2.0f 
				};
				poiObj->transform.setParent(map->transform);

				xPos = pointsOfInterest[i][0];
				zPos = pointsOfInterest[i][1];

				// 상하좌우 중 로드맵이 지정되어 있는 영역이 있는지 검사를 하며, 그 위치에 따라 도로가 이어질 수 있도록 현재 도로를 회전을 시킨다.
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
			if ((int)roadBridges.size() > 0) bridgeNumber = 
				Random::RangeInt(0, (int)roadBridges.size());
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
								SpawnTile(
									(i * 2.0f), 
									(a * 2.0f), 
									mapPosition, 
									map->transform, 
									heightMap[a][i]
								);
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

	// 지형 외의 오브젝트를 생성
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
		float _customAdditionalFilling = 0.5f;

		if (_additionalFilling == GeneratorParametr::Random) 
			_additionalFilling = (GeneratorParametr)Random::RangeInt(1, 6);
		if (_additionalFilling != GeneratorParametr::None)
		{
			// 오브젝트가 생성될 범위(원)의 반지름을 결정한다.
			// _additionalFilling 가중치를 통하여 오브젝트 그룹 범위가 줄어든다.
			int countsCycle = (int)(((float)sizeOfMap / 5.0f)) * (int)_customAdditionalFilling;

			// 오브젝트 군집이 생성되는 원들의 범위를 결정 (빽빽하게 또는 듬성듬성)
			float circlesRange = ((float)sizeOfMap / 6.0f) + (((float)sizeOfMap / 30.0f) * _customAdditionalFilling);
			circlesRange *= 20.0f;

			// 한 원 당 생성되는 오브젝트 개수
			float objectsCounts = sizeOfMap / 2.5f + ((sizeOfMap / 6) * _customAdditionalFilling);

			DirectX::XMVECTOR rayOrigin;
			DirectX::XMVECTOR rayDown = { 0.0f, -1.0f, 0.0f, 0.0f };
			float tmin = 0.0f;

			// 
			std::vector<DirectX::XMFLOAT3> treesPoints;
			std::vector<DirectX::XMFLOAT3> bushsPoints;
			std::vector<DirectX::XMFLOAT3> bigStonesPoints;
			std::vector<DirectX::XMFLOAT3> grassPoint;
			std::vector<DirectX::XMFLOAT3> branchsPoints;
			std::vector<DirectX::XMFLOAT3> logsPoints;

			// 원의 개수 만큼 반복
			for (int a = 0; a < countsCycle; a++)
			{
				DirectX::XMFLOAT3 circleTreesPos = {
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f),
					30.0f,
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f)
				};

				tmin = 0.0f;
				for (int i = 0; i < objectsCounts; i++) //Trees
				{
					DirectX::XMFLOAT3 hitPoint;

					// 원 내에 랜덤으로 점 하나를 찍고
					DirectX::XMFLOAT3 rayPos = circleTreesPos;

					rayPos.x += Random::Rangefloat(-circlesRange, circlesRange);
					rayPos.z += Random::Rangefloat(-circlesRange, circlesRange);

					// 해당 점을 광선의 시작 포인트로 잡는다
					rayOrigin = DirectX::XMLoadFloat3(&rayPos);

					// 광선이 만일 지형에 닿았을 경우
					if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
					{
						// 나무가 생성 될 위치가 hit 범위의 1.5f 반경 내에 있는지 여부 
						bool isInnerPoint = IsPosAvailableByDistance(hitPoint, treesPoints, 1.5f);
						//bool isInnerPOI = isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap);

						if (isInnerPoint/* && isInnerPOI*/)
						{
							// 나무 오브젝트 생성, 추가
							Object* tree = Instantiate(
								treesPrefabs[Random::RangeInt(0, (int)treesPrefabs.size())],
								hitPoint,
								lastMap.transform
							);
							tree->transform.rotation = {
								Random::Rangefloat(-7.5f, 7.5f),
								Random::Rangefloat(0.0f, 360.0f),
								Random::Rangefloat(-7.5f, 7.5f)
							};
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
						//if (IsPosAvailableByDistance(hitPoint, bushsPoints, 2.0f) &&
						//	isPosInRangeOf(hitPoint, treesPoints, 4.0f) &&
						//	isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap)
						//	)
						{
							Object* tree = Instantiate(
								bushsPrefabs[Random::RangeInt(0, (int)bushsPrefabs.size())],
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
						//if (IsPosAvailableByDistance(hitPoint, grassPoint, 0.25f) &&
						//	isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap)
						//	)
						{
							Object* tree = Instantiate(
								grassPrefabs[Random::RangeInt(0, (int)grassPrefabs.size())],
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
						//if (isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap))
						{
							Object* tree = Instantiate(
								littleStonesPrefabs[Random::RangeInt(0, (int)littleStonesPrefabs.size())],
								hitPoint, 
								lastMap.transform
							);
							tree->transform.rotation = { Random::Rangefloat(0.0f, 360.0f), Random::Rangefloat(0.0f, 360.0f), Random::Rangefloat(0.0f, 360.0f) };
						}
					}
				}
			}

			tmin = 0.0f;
			for (int i = 0; i < ((objectsCounts * countsCycle)*_customAdditionalFilling); i++) //Grass
			{
				DirectX::XMFLOAT3 hitPoint;

				DirectX::XMFLOAT3  rayPos = { 
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f), 
					30.0f, 
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f) 
				};

				rayOrigin = DirectX::XMLoadFloat3(&rayPos);

				if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
				{
					//if (IsPosAvailableByDistance(hitPoint, grassPoint, 0.25f) &&
					//	isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap)
					//	)
					{
						Object* tree = Instantiate(
							grassPrefabs[Random::RangeInt(0, (int)grassPrefabs.size())],
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
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f), 
					30.0f, 
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f) 
				};

				rayOrigin = DirectX::XMLoadFloat3(&rayPos);

				if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
				{
					//if (IsPosAvailableByDistance(hitPoint, bigStonesPoints, 10.0f) &&
					//	isPosInRangeOf(hitPoint, treesPoints, 8.0f) &&
					//	isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap))
					{
						Object* tree = Instantiate(
							bigStonesPrefabs[Random::RangeInt(0, (int)bigStonesPrefabs.size())],
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
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f), 
					30.0f, 
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f) 
				};

				rayOrigin = DirectX::XMLoadFloat3(&rayPos);

				if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
				{
					//if (IsPosAvailableByDistance(hitPoint, bigStonesPoints, 10.0f) &&
					//	isPosInRangeOf(hitPoint, treesPoints, 8.0f) &&
					//	isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap)
					//	)
					{
						Object* tree = Instantiate(
							branchsPrefabs[Random::RangeInt(0, (int)branchsPrefabs.size())],
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
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f), 
					30.0f,
					Random::Rangefloat(0.0f, sizeOfMap * 20.0f) 
				};

				rayOrigin = DirectX::XMLoadFloat3(&rayPos);

				if (isTargetHitRay(rayOrigin, rayDown, hitPoint, boundBoxs))
				{
					//if (IsPosAvailableByDistance(hitPoint, bigStonesPoints, 10.0f) &&
					//	isPosInRangeOf(hitPoint, treesPoints, 8.0f) &&
					//	isPosNotInPOI(hitPoint, lastRoadsMap, lastLaddersMap))
					{
						Object* tree = Instantiate(
							logsPrefabs[Random::RangeInt(0, (int)logsPrefabs.size())],
							hitPoint, 
							lastMap.transform
						);
						tree->transform.rotation = { 0.0f, Random::Rangefloat(0.0f, 360.0f), 0.0f };
						logsPoints.push_back(hitPoint);
					}
				}
			}

			int iter = 0;
			float mOffsetY = 10.0f;
			DirectX::XMFLOAT3 mScale = { 10.0f, 10.0f, 10.0f };

			for (iter = 0; iter < bushsPoints.size(); iter++)
			{
				bushsPoints[iter].y += mOffsetY;

				treeObject->Instantiate(
					bushsPoints[iter],
					{ 0.0f, 0.0f, 0.0 },
					mScale
				);
			}
			for (iter = 0; iter < treesPoints.size(); iter++)
			{
				treesPoints[iter].y += mOffsetY;

				bushsObject->Instantiate(
					treesPoints[iter],
					{ 0.0f, 0.0f, 0.0 },
					mScale
				);
			}
			for (iter = 0; iter < bigStonesPoints.size(); iter++)
			{
				bigStonesPoints[iter].y += mOffsetY;

				bigStonesObject->Instantiate(
					bigStonesPoints[iter],
					{ 0.0f, 0.0f, 0.0 },
					mScale
				);
			}
			for (iter = 0; iter < grassPoint.size(); iter++)
			{
				grassPoint[iter].y += mOffsetY;

				grassObject->Instantiate(
					grassPoint[iter],
					{ 0.0f, 0.0f, 0.0 },
					mScale
				);
			}
			for (iter = 0; iter < branchsPoints.size(); iter++)
			{
				branchsPoints[iter].y += mOffsetY;

				branchsObject->Instantiate(
					branchsPoints[iter],
					{ 0.0f, 0.0f, 0.0 },
					mScale
				);
			}
			for (iter = 0; iter < logsPoints.size(); iter++)
			{
				logsPoints[iter].y += mOffsetY;

				logsObject->Instantiate(
					logsPoints[iter],
					{ 0.0f, 0.0f, 0.0 },
					mScale
				);
			}
		}
	}
};