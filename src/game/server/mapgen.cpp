#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include "mapgen.h"
#include <game/server/gamecontext.h>
#include <game/layers.h>

CMapGen::CMapGen()
{
	m_pLayers = 0x0;
	m_pCollision = 0x0;
	m_pNoise = 0x0;
}
CMapGen::~CMapGen()
{
	if (m_pNoise)
		delete m_pNoise;
}

void CMapGen::Init(CLayers *pLayers, CCollision *pCollision)
{
	m_pLayers = pLayers;
	m_pCollision = pCollision;

	if (!m_pNoise)
		m_pNoise = new CPerlinOctave(3); // TODO: Add a seed
}

void CMapGen::GenerateMap()
{
	if (!m_pLayers->MineTeeLayer())
		return;

	int MineTeeLayerSize = m_pLayers->MineTeeLayer()->m_Width*m_pLayers->MineTeeLayer()->m_Height;

	// clear map, but keep background, envelopes etc
	for(int i = 0; i < MineTeeLayerSize; i++)
	{
		int x = i%m_pLayers->MineTeeLayer()->m_Width;
		ivec2 TilePos(x, (i-x)/m_pLayers->MineTeeLayer()->m_Width);
		
		// clear the different layers
		m_pCollision->ModifTile(TilePos, m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), 0, 0);
		m_pCollision->ModifTile(TilePos, m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeFGLayerIndex(), 0, 0);
		m_pCollision->ModifTile(TilePos, m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeBGLayerIndex(), 0, 0);
	}

	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	/* ~~~ Generate the world ~~~ */
	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	// terrain
	GenerateBasicTerrain();
	// ores
	GenerateOre(CBlockManager::COAL_ORE, 200.0f, COAL_LEVEL, 50, 4);
	GenerateOre(CBlockManager::IRON_ORE, 320.0f, IRON_LEVEL, 30, 2);
	GenerateOre(CBlockManager::GOLD_ORE, 350.0f, GOLD_LEVEL, 15, 2);
	GenerateOre(CBlockManager::DIAMOND_ORE, 680.0f, DIAMOND_LEVEL, 15, 1);
	// /ores1
	GenerateCaves();
	GenerateWater();

	// vegetation
	GenerateFlowers();
	GenerateMushrooms();
	GenerateTrees();

	// misc
	GenerateBorder(); // as long as there are no infinite (chunked) maps

	// Performance
	GenerateSkip();
}

void CMapGen::GenerateBasicTerrain()
{
	// generate the surface, 1d noise
	int TilePosY = DIRT_LEVEL;
	int TilePosX = 0;
	for(int x = 0; x < m_pLayers->MineTeeLayer()->m_Width; x++)
	{
		TilePosX = x;
		
		float frequency = 25.0f / (float)m_pLayers->MineTeeLayer()->m_Width;
		TilePosY = m_pNoise->Noise((float) TilePosX * frequency) * (m_pLayers->MineTeeLayer()->m_Height/6) + DIRT_LEVEL;

		m_pCollision->ModifTile(ivec2(TilePosX, TilePosY), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::GRASS, 0);
		
		// fill the tiles under the defined tile
		int TempTileY = TilePosY+1;
		while(TempTileY < m_pLayers->MineTeeLayer()->m_Height)
		{
			m_pCollision->ModifTile(ivec2(TilePosX, TempTileY), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::DIRT, 0);
			TempTileY++;
		}
	}

	// fill the underground with stones
	for(int x = 0; x < m_pLayers->MineTeeLayer()->m_Width; x++)
	{
		TilePosX = x;
		TilePosY -= (rand()%3)-1;
		
		if(TilePosY - STONE_LEVEL < LEVEL_TOLERANCE*-1)
			TilePosY++;
		else if(TilePosY - STONE_LEVEL > LEVEL_TOLERANCE)
			TilePosY--;

		m_pCollision->ModifTile(ivec2(TilePosX, TilePosY), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::STONE, 0);
		
		// fill the tiles under the random tile
		int TempTileY = TilePosY+1;
		while(TempTileY < m_pLayers->MineTeeLayer()->m_Height)
		{
			m_pCollision->ModifTile(ivec2(TilePosX, TempTileY), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::STONE, 0);
			TempTileY++;
		}
	}
}

void CMapGen::GenerateOre(int Type, float F, int Level, int Radius, int ClusterSize)
{
	for(int x = 0; x < m_pLayers->MineTeeLayer()->m_Width; x++)
	{
		for(int y = Level; y < Level + Radius; y++)
		{
			float frequency = F / (float)m_pLayers->MineTeeLayer()->m_Width;
			float noise = m_pNoise->Noise((float)x * frequency, (float)y * frequency);

			if(noise > 0.85f)
			{
				// place the "entry point"-tile
				m_pCollision->ModifTile(ivec2(x, y), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), Type, 0);

				// generate a cluster
				int CS = ClusterSize + rand()%4;

				for(int cx = x-CS/2; cx < x+CS/2; cx++)
				{
					for(int cy = y-CS/2; cy < y+CS/2; cy++)
					{
						if(!(rand()%3))
							m_pCollision->ModifTile(ivec2(cx, cy), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), Type, 0);
					}
				}
			}
		}
	}
}

void CMapGen::GenerateCaves()
{
	// cut in the caves with a 2d perlin noise
	for(int x = 0; x < m_pLayers->MineTeeLayer()->m_Width; x++)
	{
		for(int y = STONE_LEVEL; y < m_pLayers->MineTeeLayer()->m_Height; y++)
		{
			float frequency = 32.0f / (float)m_pLayers->MineTeeLayer()->m_Width;
			float noise = m_pNoise->Noise((float)x * frequency, (float)y * frequency);
	
			if(noise > 0.4f)
				m_pCollision->ModifTile(ivec2(x, y), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::AIR, 0);
		}
	}
}

void CMapGen::GenerateWater()
{
	for(int x = 0; x < m_pLayers->MineTeeLayer()->m_Width; x++)
	{
		for(int y = WATER_LEVEL_MAX; y <= WATER_LEVEL_MIN; y++)
		{
			if(m_pCollision->GetMineTeeTileAt(vec2(x*32, y*32)) == CBlockManager::AIR)
			{
				m_pCollision->ModifTile(ivec2(x, y), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::WATER_A, 0);
			}
		}
	}
}

void CMapGen::GenerateFlowers()
{
	for(int x = 1; x < m_pLayers->MineTeeLayer()->m_Width-1; x++)
	{
		if(rand()%32 == 0)
		{
			int FieldSize = 2+(rand()%3);
			for(int f = x; f - x <= FieldSize; f++)
			{
				int y = 0;
				while(m_pCollision->GetMineTeeTileAt(vec2(f*32, (y+1)*32)) != CBlockManager::GRASS && y < m_pLayers->MineTeeLayer()->m_Height)
				{
					y++;
				}

				if(y >= m_pLayers->MineTeeLayer()->m_Height)
					return;

				m_pCollision->ModifTile(ivec2(f, y), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::ROSE + rand()%2, 0);
			}

			x += FieldSize;
		}
	}
}

void CMapGen::GenerateMushrooms()
{
	for(int x = 1; x < m_pLayers->MineTeeLayer()->m_Width-1; x++)
	{
		if(rand()%256 == 0)
		{
			int FieldSize = 5+(rand()%8);
			for(int f = x; f - x <= FieldSize; f++)
			{
				int y = 0;
				while(m_pCollision->GetMineTeeTileAt(vec2(f*32, (y+1)*32)) != CBlockManager::GRASS && y < m_pLayers->MineTeeLayer()->m_Height)
				{
					y++;
				}

				if(y >= m_pLayers->MineTeeLayer()->m_Height)
					return;

				m_pCollision->ModifTile(ivec2(f, y), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::MUSHROOM_RED + rand()%2, 0);
			}

			x += FieldSize;
		}
	}
}

void CMapGen::GenerateTrees()
{
	int LastTreeX = 0;

	for(int x = 10; x < m_pLayers->MineTeeLayer()->m_Width-10; x++)
	{
		// trees like to spawn in groups
		if((rand()%64 == 0 && abs(LastTreeX - x) >= 8) || (abs(LastTreeX - x) <= 8 && abs(LastTreeX - x) >= 3 && rand()%8 == 0))
		{
			int TempTileY = 0;
			while(m_pCollision->GetMineTeeTileAt(vec2(x*32, (TempTileY+1)*32)) != CBlockManager::GRASS && TempTileY < m_pLayers->MineTeeLayer()->m_Height)
			{
				TempTileY++;
			}

			if(TempTileY >= m_pLayers->MineTeeLayer()->m_Height)
				return;

			LastTreeX = x;

			// plant the tree \o/
			int TreeHeight = 4 + (rand()%5);
			for(int h = 0; h <= TreeHeight; h++)
				m_pCollision->ModifTile(ivec2(x, TempTileY-h), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::WOOD_BROWN, 0);
		
			// add the leafs
			for(int l = 0; l <= TreeHeight/2.5f; l++)
				m_pCollision->ModifTile(ivec2(x, TempTileY - TreeHeight - l), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::LEAFS, 0);
		
			int TreeWidth = TreeHeight/1.2f;
			if(!(TreeWidth%2)) // odd numbers please
				TreeWidth++;
			for(int h = 0; h <= TreeHeight/2; h++)
			{
				for(int w = 0; w < TreeWidth; w++)
				{
					if(m_pCollision->GetMineTeeTileAt(vec2((x-(w-(TreeWidth/2)))*32, (TempTileY-(TreeHeight-(TreeHeight/2.5f)+h))*32)) != CBlockManager::AIR)
						continue;

					m_pCollision->ModifTile(ivec2(x-(w-(TreeWidth/2)), TempTileY-(TreeHeight-(TreeHeight/2.5f)+h)), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::LEAFS, 0);
				}
			}

		}
	}
}

void CMapGen::GenerateBorder()
{
	// draw a boarder all around the map
	for(int i = 0; i < m_pLayers->MineTeeLayer()->m_Width; i++)
	{
		// bottom border
		m_pCollision->ModifTile(ivec2(i, m_pLayers->MineTeeLayer()->m_Height-1), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::BEDROCK, 0);
	}

	for(int i = 0; i < m_pLayers->MineTeeLayer()->m_Height; i++)
	{
		// left border
		m_pCollision->ModifTile(ivec2(0, i), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::BEDROCK, 0);
		
		// right border
		m_pCollision->ModifTile(ivec2(m_pLayers->MineTeeLayer()->m_Width-1, i), m_pLayers->GetMineTeeGroupIndex(), m_pLayers->GetMineTeeLayerIndex(), CBlockManager::BEDROCK, 0);
	}
}

void CMapGen::GenerateSkip()
{

	for(int g = 0; g < m_pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(g);

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer+l);

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTmap = (CMapItemLayerTilemap *)pLayer;
				CTile *pTiles = (CTile *)m_pLayers->Map()->GetData(pTmap->m_Data);
				for(int y = 0; y < pTmap->m_Height; y++)
				{
					for(int x = 1; x < pTmap->m_Width;)
					{
						int sx;
						for(sx = 1; x+sx < pTmap->m_Width && sx < 255; sx++)
						{
							if(pTiles[y*pTmap->m_Width+x+sx].m_Index)
								break;
						}

						pTiles[y*pTmap->m_Width+x].m_Skip = sx-1;
						x += sx;
					}
				}
			}
		}
	}
}
