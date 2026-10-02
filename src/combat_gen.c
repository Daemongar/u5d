#include "common/common.h"
#include "vars.h"
#include "macros.h"
#include "tiles.h"
#include "3000.h"

#include "combat_gen.h"

#if defined(ENABLE_VARIED_COMBAT)

// ---------------------------------------------------------------------------
// Terrain profiles
// ---------------------------------------------------------------------------

// Number of feature tile types per profile entry
#define PROFILE_MAX_FEATURES 4

typedef struct TerrainProfile
{
    u8 baseTile;
    u8 featureTiles[PROFILE_MAX_FEATURES];
    u8 featureDensity;  // number of clusters to scatter
    u8 clusterSize;     // max cells per cluster
} TerrainProfile;

// One entry per CBT map index (0..15).
// Indices map to the same terrain types as ULTIMA_6150_Combat:
//   0  = camping / generic open
//   1  = swamp / poison
//   2  = grass / open field
//   3  = shrubland
//   4  = forest
//   5  = mountains
//   6  = hills / mixed terrain
//   7  = beach / ruins
//   8  = dungeon interior / town
//   9  = cave
//  10  = shadowlord encounter
//  11  = sea + shore (frigate vs land)
//  12-15 = open sea (pirate / water combat)
static const TerrainProfile PROFILES[16] =
{
    /* 0: camping / open grass — flat, no obstacles */
    {
        TILE_MAP_GRASS,
        { 0xff, 0xff, 0xff, 0xff },
        0, 0
    },
    /* 1: swamp — mostly poison with small clear islands */
    {
        TILE_MAP_POISON,
        { TILE_MAP_GRASS, TILE_MAP_GRASS, 0xff, 0xff },
        4, 3
    },
    /* 2: grass / open field — light tree and shrub clusters */
    {
        TILE_MAP_GRASS,
        { TILE_MAP_7, TILE_MAP_TREE, TILE_MAP_STUMP, 0xff },
        5, 3
    },
    /* 3: shrubland — mixed brush and low hills */
    {
        TILE_MAP_8,
        { TILE_MAP_6, TILE_MAP_7, TILE_MAP_A, 0xff },
        5, 4
    },
    /* 4: forest — dense tree cover with occasional stumps */
    {
        TILE_MAP_7,
        { TILE_MAP_TREE, TILE_MAP_B, TILE_MAP_STUMP, TILE_MAP_6 },
        6, 5
    },
    /* 5: mountains — rocky terrain with crevasses */
    {
        TILE_MAP_9,
        { TILE_MAP_A, TILE_MAP_B, TILE_MAP_C, TILE_MAP_D },
        5, 4
    },
    /* 6: hills — rolling mixed terrain */
    {
        TILE_MAP_A,
        { TILE_MAP_9, TILE_MAP_B, TILE_MAP_8, 0xff },
        4, 4
    },
    /* 7: beach / ruins — sandy floor with rubble and ruins tiles */
    {
        TILE_MAP_GRASS,
        { TILE_MAP_1D, TILE_MAP_49, TILE_MAP_48, 0xff },
        4, 3
    },
    /* 8: dungeon interior / town indoors — stone floor with walls and lava pits */
    {
        TILE_MAP_87,
        { TILE_MAP_WALL, TILE_MAP_LAVA, TILE_MAP_84, TILE_MAP_85 },
        4, 3
    },
    /* 9: cave — rough floor with walls and rubble */
    {
        TILE_MAP_86,
        { TILE_MAP_WALL, TILE_MAP_85, TILE_MAP_84, 0xff },
        4, 3
    },
    /* 10: shadowlord — oppressive stone with lava pools */
    {
        TILE_MAP_87,
        { TILE_MAP_LAVA, TILE_MAP_WALL, TILE_MAP_LAVA, TILE_MAP_84 },
        5, 3
    },
    /* 11: sea + shore — open water with small land patches */
    {
        TILE_MAP_WATER_1,
        { TILE_MAP_GRASS, TILE_MAP_GRASS, 0xff, 0xff },
        3, 3
    },
    /* 12-15: open sea — no obstacles */
    {
        TILE_MAP_WATER_2,
        { 0xff, 0xff, 0xff, 0xff },
        0, 0
    },
    {
        TILE_MAP_WATER_2,
        { 0xff, 0xff, 0xff, 0xff },
        0, 0
    },
    {
        TILE_MAP_WATER_3,
        { 0xff, 0xff, 0xff, 0xff },
        0, 0
    },
    {
        TILE_MAP_WATER_3,
        { 0xff, 0xff, 0xff, 0xff },
        0, 0
    },
};

// ---------------------------------------------------------------------------
// Spawn-point protection
// ---------------------------------------------------------------------------

// Returns 1 if (x, y) is a party or monster spawn coordinate loaded from
// the current CBT, 0 otherwise. Spawn cells must never be blocked.
static int IsSpawnPoint(int x, int y)
{
    int i;

    // Party spawn positions (up to D_585b members)
    for (i = 0; i < (int)D_585b; i++)
    {
        if ((int)D_1724[i] == x && (int)D_172c[i] == y)
        {
            return 1;
        }
    }

    // Monster spawn positions (up to 16 slots)
    for (i = 0; i < 0x10; i++)
    {
        if ((int)D_1704[i] == x && (int)D_1714[i] == y)
        {
            return 1;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Cluster growth
// ---------------------------------------------------------------------------

// Direction offsets for the four cardinal directions.
static const int DIR_DX[4] = {  0, 1, 0, -1 };
static const int DIR_DY[4] = { -1, 0, 1,  0 };

// Grow a cluster of `tile` starting from (seedX, seedY).
// Each step randomly tries one of the four cardinal neighbours; if the
// neighbour is inside the 11x11 grid and not a spawn point it is painted.
// The seed cell itself is painted first (if it is not a spawn point).
// Growth stops after maxSize cells have been painted.
static void GrowCluster(int seedX, int seedY, u8 tile, int maxSize)
{
    int cx = seedX;
    int cy = seedY;
    int painted = 0;
    int attempts;
    int dir;
    int nx, ny;

    // Paint the seed cell if eligible
    if (!IsSpawnPoint(cx, cy))
    {
        GetCombatMap(cx, cy) = tile;
        painted++;
    }

    // Random walk: try to grow into adjacent cells
    for (attempts = 0; attempts < maxSize * 8 && painted < maxSize; attempts++)
    {
        dir = ULTIMA_3aae_Random(3);        // 0..3
        nx = cx + DIR_DX[dir];
        ny = cy + DIR_DY[dir];

        if (nx >= 0 && nx < COMBAT_MAP_TILE_WIDTH &&
            ny >= 0 && ny < COMBAT_MAP_TILE_WIDTH &&
            !IsSpawnPoint(nx, ny))
        {
            GetCombatMap(nx, ny) = tile;
            painted++;
            // Advance the walk head so clusters are contiguous
            cx = nx;
            cy = ny;
        }
    }
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

void COMBAT_GEN_GenerateArena(int mapIndex)
{
    const TerrainProfile* prof;
    int x, y;
    int cluster;
    int seedX, seedY;
    u8 featureTile;
    int featureCount;
    int fi;

    if (mapIndex < 0 || mapIndex >= 16)
    {
        return;
    }

    prof = &PROFILES[mapIndex];

    // Count usable feature tile slots in this profile
    featureCount = 0;
    for (fi = 0; fi < PROFILE_MAX_FEATURES; fi++)
    {
        if (prof->featureTiles[fi] != 0xff)
        {
            featureCount++;
        }
    }

    // Pass 1: flood-fill the 11x11 terrain grid with the base tile
    for (y = 0; y < COMBAT_MAP_TILE_WIDTH; y++)
    {
        for (x = 0; x < COMBAT_MAP_TILE_WIDTH; x++)
        {
            GetCombatMap(x, y) = prof->baseTile;
        }
    }

    // Pass 2: scatter feature clusters (skip if no features defined)
    if (featureCount > 0 && prof->featureDensity > 0)
    {
        for (cluster = 0; cluster < (int)prof->featureDensity; cluster++)
        {
            // Random seed position within the 11x11 grid
            seedX = ULTIMA_3aae_Random(COMBAT_MAP_TILE_WIDTH - 1);
            seedY = ULTIMA_3aae_Random(COMBAT_MAP_TILE_WIDTH - 1);

            // Pick a random feature tile from the profile's list
            fi = ULTIMA_3aae_Random(featureCount - 1);
            featureTile = prof->featureTiles[fi];

            GrowCluster(seedX, seedY, featureTile, (int)prof->clusterSize);
        }
    }

    // Pass 3: enforce spawn-point safety — write base tile back to every
    // party and monster spawn coordinate so they are always walkable.
    for (y = 0; y < (int)D_585b; y++)
    {
        x = (int)D_1724[y];
        fi = (int)D_172c[y];
        if (x < COMBAT_MAP_TILE_WIDTH && fi < COMBAT_MAP_TILE_WIDTH)
        {
            GetCombatMap(x, fi) = prof->baseTile;
        }
    }

    for (cluster = 0; cluster < 0x10; cluster++)
    {
        x = (int)D_1704[cluster];
        y = (int)D_1714[cluster];
        if (x < COMBAT_MAP_TILE_WIDTH && y < COMBAT_MAP_TILE_WIDTH)
        {
            GetCombatMap(x, y) = prof->baseTile;
        }
    }
}

#else // !ENABLE_VARIED_COMBAT

void COMBAT_GEN_GenerateArena(int mapIndex)
{
    (void)mapIndex;
}

#endif // ENABLE_VARIED_COMBAT
