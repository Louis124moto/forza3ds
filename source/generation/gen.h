/* Forza3DS - shared state of the island generator (private to generation/). */
#ifndef F3DS_GEN_H
#define F3DS_GEN_H

#include <stdlib.h>
#include "noise.h"
#include "rng.h"
#include "world.h"

#define GEN_WET_RIVER 1u
#define GEN_WET_LAKE  2u

/* Purposes of the independent sub-seeds. */
enum {
	GEN_SEED_RELIEF = 1, GEN_SEED_BIOME, GEN_SEED_ROADS, GEN_SEED_CITIES,
	GEN_SEED_BUILDINGS, GEN_SEED_DETAILS
};

typedef struct {
	F3DSWorld *w;
	float *h;        /* WORLD_N^2 terrain heights, metres */
	float *coast;    /* WORLD_BN^2 distance to the sea, metres */
	float *wlevel;   /* WORLD_BN^2 inland water level, WORLD_DRY if dry */
	uint8_t *wet;    /* WORLD_BN^2 GEN_WET_* */
	uint32_t relief, biome, roads, cities, buildings;
	float volcanoX, volcanoZ, volcanoRadius;
	float lighthouseDirX, lighthouseDirZ;
	F3DSProgress progress;
	void *user;
} F3DSGen;

static inline void gen_report(F3DSGen *g, int percent, const char *step)
{
	if (g->progress)
		g->progress(g->user, percent, step);
}

static inline float gen_x(int i) { return (float)i * WORLD_CELL - WORLD_HALF; }
static inline float gen_bx(int i) { return (float)i * WORLD_BIOME_CELL - WORLD_HALF; }

static inline int gen_clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* Bilinear height from the generation grid. */
float gen_height_at(const F3DSGen *g, float x, float z);
/* Nearest biome-grid index. */
int gen_bindex(float x, float z);

bool gen_relief(F3DSGen *g);
bool gen_water(F3DSGen *g);
bool gen_coast(F3DSGen *g);
bool gen_sites(F3DSGen *g);
bool gen_roads(F3DSGen *g);
bool gen_road_index(F3DSGen *g);
bool gen_carve_roads(F3DSGen *g);
bool gen_buildings(F3DSGen *g);
bool gen_biomes(F3DSGen *g);

#endif
