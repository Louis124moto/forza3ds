/* Forza3DS - the island: a compact logical description generated from a seed.
 *
 * 8192 x 8192 m, centred on the origin. Heights are kept on an 8 m grid,
 * biomes and inland water on a 16 m grid, roads as polylines and buildings as
 * a list. Everything small (trees, lamps, rails...) is derived on demand, per
 * 256 m sector, from the details seed (see props.h). Hardware independent. */
#ifndef F3DS_WORLD_H
#define F3DS_WORLD_H

#include "common.h"
#include "vehicle.h"

/* Bump when the same seed no longer produces the same island. */
#define WORLD_GEN_VERSION 1u

#define WORLD_SIZE 8192.0f
#define WORLD_HALF 4096.0f
#define WORLD_CELL 8.0f
#define WORLD_N 1025                    /* height vertices per axis */
#define WORLD_BIOME_CELL 16.0f
#define WORLD_BN 513                    /* biome / water vertices per axis */
#define WORLD_SECTOR 256.0f
#define WORLD_SECTORS 32                /* per axis */
#define WORLD_HEIGHT_UNIT (1.0f / 32.0f)
#define WORLD_NO_WATER INT16_MIN
#define WORLD_DRY (-1000.0f)

#define WORLD_ROAD_SAMPLE_MAX 26000
#define WORLD_ROAD_MAX 320
#define WORLD_ROAD_SPACING 8.0f
#define WORLD_ROAD_CELL 64.0f
#define WORLD_ROAD_CELLS 128            /* per axis */
#define WORLD_SITE_MAX 24
#define WORLD_BUILDING_MAX 1400
#define WORLD_FALL_MAX 48

typedef enum {
	F3DS_BIOME_OCEAN = 0,
	F3DS_BIOME_BEACH,
	F3DS_BIOME_COAST,
	F3DS_BIOME_PLAIN,
	F3DS_BIOME_FARM,
	F3DS_BIOME_FOREST,
	F3DS_BIOME_BAMBOO,
	F3DS_BIOME_HIGHLAND,
	F3DS_BIOME_ROCK,
	F3DS_BIOME_SNOW,
	F3DS_BIOME_LAKE,
	F3DS_BIOME_RIVER,
	F3DS_BIOME_VILLAGE,
	F3DS_BIOME_CITY,
	F3DS_BIOME_INDUSTRY,
	F3DS_BIOME_COUNT
} F3DSBiome;

typedef enum {
	F3DS_ROAD_MAIN = 0,
	F3DS_ROAD_SECONDARY,
	F3DS_ROAD_STREET,
	F3DS_ROAD_DIRT,
	F3DS_ROAD_HIDDEN,
	F3DS_ROAD_TYPE_COUNT
} F3DSRoadType;

/* What a stretch of road is good for. */
typedef enum {
	F3DS_USE_ROUTE = 0,
	F3DS_USE_DRIFT,
	F3DS_USE_MOUNTAIN,
	F3DS_USE_RALLY,
	F3DS_USE_COUNT
} F3DSRoadUse;

#define F3DS_SAMPLE_LAST   1u   /* no segment starts here */
#define F3DS_SAMPLE_BRIDGE 2u

typedef struct {
	float x, z, y;
	uint8_t type;   /* F3DSRoadType */
	uint8_t use;    /* F3DSRoadUse */
	uint8_t flags;
	uint8_t reserved;
} F3DSRoadSample;

typedef struct {
	uint16_t first, count;
	uint8_t type;
	uint8_t reserved[3];
} F3DSRoad;

typedef enum {
	F3DS_SITE_CITY = 0,
	F3DS_SITE_INDUSTRY,
	F3DS_SITE_VILLAGE,
	F3DS_SITE_HARBOUR,
	F3DS_SITE_VIEWPOINT,
	F3DS_SITE_LIGHTHOUSE,
	F3DS_SITE_LAKE,
	F3DS_SITE_BEACH,
	F3DS_SITE_TRAIL,
	F3DS_SITE_KIND_COUNT
} F3DSSiteKind;

typedef struct {
	float x, z, radius, height;
	int kind;
	char name[28];
	/* Where a car is dropped when travelling there: on a road. */
	float roadX, roadZ, roadYaw;
} F3DSSite;

typedef enum {
	F3DS_BUILD_HOUSE = 0,   /* traditional house with a tiled roof */
	F3DS_BUILD_SHOP,
	F3DS_BUILD_BLOCK,       /* flat-roofed apartment block */
	F3DS_BUILD_TOWER,
	F3DS_BUILD_WAREHOUSE,
	F3DS_BUILD_FACTORY,
	F3DS_BUILD_TANK,
	F3DS_BUILD_CHIMNEY,
	F3DS_BUILD_SHRINE,
	F3DS_BUILD_LIGHTHOUSE,
	F3DS_BUILD_PIER,
	F3DS_BUILD_TORII,       /* gate over a road: only the pillars collide */
	F3DS_BUILD_KIND_COUNT
} F3DSBuildingKind;

/* Footprint half extents hx (across) and hz (depth), rotated by yaw. */
typedef struct {
	float x, z, y;
	float hx, hz, height, yaw;
	uint8_t kind, variant;
	uint16_t reserved;
} F3DSBuilding;

typedef struct {
	float x, z, top, bottom, width, yaw;
} F3DSWaterfall;

typedef struct {
	uint32_t seed, detail, version;

	int16_t *height;      /* WORLD_N * WORLD_N, rows along +Z */
	uint8_t *biome;       /* WORLD_BN * WORLD_BN */
	int16_t *water;       /* inland water level, WORLD_NO_WATER elsewhere */

	F3DSRoadSample *samples;
	int sampleCount;
	F3DSRoad roads[WORLD_ROAD_MAX];
	int roadCount;
	uint32_t *cellStart;  /* WORLD_ROAD_CELLS^2 + 1 */
	uint16_t *cellItems;  /* segment = index of its first sample */
	uint32_t itemCount;

	F3DSSite sites[WORLD_SITE_MAX];
	int siteCount;
	F3DSBuilding *buildings; /* sorted by sector */
	int buildingCount;
	uint16_t sectorBuildings[WORLD_SECTORS * WORLD_SECTORS + 1];

	F3DSWaterfall falls[WORLD_FALL_MAX];
	int fallCount;

	float spawnX, spawnZ, spawnYaw;

	/* Generation report. */
	float roadLength;     /* m */
	float peakHeight;
	int routeRetries, routeFailures, siteRetries, lakeCount, riverCells;
} F3DSWorld;

typedef void (*F3DSProgress)(void *user, int percent, const char *step);

/* Same seed and version: same island. "detail" only changes small scenery.
 * Returns false when memory runs out; the world is then empty. */
bool world_generate(F3DSWorld *w, uint32_t seed, uint32_t detail, F3DSProgress progress, void *user);
void world_free(F3DSWorld *w);
/* Bytes held by a generated world. */
size_t world_memory(const F3DSWorld *w);
/* Checksum of the structure (relief, water, roads, buildings). */
uint32_t world_checksum(const F3DSWorld *w);

/* Ground mesh height: exactly the triangles that are drawn. */
float world_terrain_height(const F3DSWorld *w, float x, float z);
/* Height the car drives on: road deck where there is one. */
float world_height(const F3DSWorld *w, float x, float z);
F3DSSurface world_surface(const F3DSWorld *w, float x, float z);
F3DSBiome world_biome(const F3DSWorld *w, float x, float z);
/* Water surface height at (x, z), or WORLD_DRY. */
float world_water_level(const F3DSWorld *w, float x, float z);

typedef struct {
	int sample;       /* first sample of the segment */
	float t;          /* 0..1 along it */
	float distance;   /* horizontal distance to the centre line */
	float y;          /* road height at that point */
	float half;       /* half width */
	float x, z;       /* closest point */
} F3DSRoadHit;

/* Closest road segment within "reach" metres (at most one road cell). */
bool world_road_nearest(const F3DSWorld *w, float x, float z, float reach, F3DSRoadHit *hit);
/* Closest road anywhere (slow, for placing the car). */
bool world_road_search(const F3DSWorld *w, float x, float z, F3DSRoadHit *hit);

float road_half_width(int type);
const char *world_biome_name(F3DSBiome b);

#endif
