/* The island pipeline: relief, water, coast, sites, roads, earthworks,
 * buildings, biomes. Each stage draws from its own sub-seed. */
#include "gen.h"
#include <string.h>

float gen_height_at(const F3DSGen *g, float x, float z)
{
	const float fx = clampf((x + WORLD_HALF) / WORLD_CELL, 0.0f, WORLD_N - 1.001f);
	const float fz = clampf((z + WORLD_HALF) / WORLD_CELL, 0.0f, WORLD_N - 1.001f);
	const int i = (int)fx, j = (int)fz;
	const float u = fx - i, v = fz - j;
	const float *h = &g->h[j * WORLD_N + i];
	return lerpf(lerpf(h[0], h[1], u), lerpf(h[WORLD_N], h[WORLD_N + 1], u), v);
}

int gen_bindex(float x, float z)
{
	const int i = gen_clampi((int)((x + WORLD_HALF) / WORLD_BIOME_CELL + 0.5f), 0, WORLD_BN - 1);
	const int j = gen_clampi((int)((z + WORLD_HALF) / WORLD_BIOME_CELL + 0.5f), 0, WORLD_BN - 1);
	return j * WORLD_BN + i;
}

void world_free(F3DSWorld *w)
{
	free(w->height);
	free(w->biome);
	free(w->water);
	free(w->samples);
	free(w->cellStart);
	free(w->cellItems);
	free(w->buildings);
	memset(w, 0, sizeof(*w));
}

size_t world_memory(const F3DSWorld *w)
{
	if (!w->height)
		return 0;
	return sizeof(int16_t) * (size_t)WORLD_N * WORLD_N +
		(sizeof(uint8_t) + sizeof(int16_t)) * (size_t)WORLD_BN * WORLD_BN +
		sizeof(F3DSRoadSample) * (size_t)w->sampleCount +
		sizeof(uint32_t) * ((size_t)WORLD_ROAD_CELLS * WORLD_ROAD_CELLS + 1) +
		sizeof(uint16_t) * (size_t)w->itemCount +
		sizeof(F3DSBuilding) * (size_t)WORLD_BUILDING_MAX;
}

/* FNV-1a. */
static uint32_t mix(uint32_t h, const void *data, size_t n)
{
	const uint8_t *p = data;
	for (size_t i = 0; i < n; ++i)
		h = (h ^ p[i]) * 16777619u;
	return h;
}

uint32_t world_checksum(const F3DSWorld *w)
{
	if (!w->height)
		return 0;
	uint32_t h = 2166136261u;
	h = mix(h, w->height, sizeof(int16_t) * (size_t)WORLD_N * WORLD_N);
	h = mix(h, w->water, sizeof(int16_t) * (size_t)WORLD_BN * WORLD_BN);
	h = mix(h, w->biome, (size_t)WORLD_BN * WORLD_BN);
	h = mix(h, w->samples, sizeof(F3DSRoadSample) * (size_t)w->sampleCount);
	h = mix(h, w->buildings, sizeof(F3DSBuilding) * (size_t)w->buildingCount);
	for (int i = 0; i < w->siteCount; ++i) {
		h = mix(h, &w->sites[i].x, sizeof(float) * 4);
		h = mix(h, w->sites[i].name, sizeof(w->sites[i].name));
	}
	return h;
}

/* Where the car is dropped when travelling to each site. */
static void arrival_points(F3DSWorld *w)
{
	for (int i = 0; i < w->siteCount; ++i) {
		F3DSSite *s = &w->sites[i];
		float best = 1e30f;
		int sample = 0;
		for (int k = 1; k + 2 < w->sampleCount; ++k) {
			const F3DSRoadSample *p = &w->samples[k];
			if ((p->flags & (F3DS_SAMPLE_LAST | F3DS_SAMPLE_BRIDGE)) ||
			    (p[1].flags & F3DS_SAMPLE_LAST))
				continue;
			const float dx = p->x - s->x, dz = p->z - s->z;
			float d = dx * dx + dz * dz;
			/* Prefer a wide road when it is about as close. */
			if (p->type == F3DS_ROAD_HIDDEN || p->type == F3DS_ROAD_DIRT)
				d = d * 3.0f + 900.0f;
			if (d < best) {
				best = d;
				sample = k;
			}
		}
		const F3DSRoadSample *p = &w->samples[sample];
		s->roadX = (p[0].x + p[1].x) * 0.5f;
		s->roadZ = (p[0].z + p[1].z) * 0.5f;
		s->roadYaw = atan2f(p[1].x - p[0].x, p[1].z - p[0].z);
		/* Face the destination. */
		if (sinf(s->roadYaw) * (s->x - s->roadX) + cosf(s->roadYaw) * (s->z - s->roadZ) < 0.0f)
			s->roadYaw = wrap_angle(s->roadYaw + F3DS_PI);
	}
}

static void make_gen(F3DSGen *g, F3DSWorld *w, uint32_t seed, F3DSProgress progress, void *user)
{
	memset(g, 0, sizeof(*g));
	g->w = w;
	g->relief = hash_seed(seed, GEN_SEED_RELIEF);
	g->biome = hash_seed(seed, GEN_SEED_BIOME);
	g->roads = hash_seed(seed, GEN_SEED_ROADS);
	g->cities = hash_seed(seed, GEN_SEED_CITIES);
	g->buildings = hash_seed(seed, GEN_SEED_BUILDINGS);
	g->progress = progress;
	g->user = user;
}

bool world_generate(F3DSWorld *w, uint32_t seed, uint32_t detail, F3DSProgress progress, void *user)
{
	memset(w, 0, sizeof(*w));
	w->seed = seed;
	w->detail = detail;
	w->version = WORLD_GEN_VERSION;

	F3DSGen g;
	make_gen(&g, w, seed, progress, user);
	const size_t nh = (size_t)WORLD_N * WORLD_N, nb = (size_t)WORLD_BN * WORLD_BN;

	/* Long-lived buffers first, so that the scratch memory freed at the end
	 * stays in one piece. */
	w->height = malloc(sizeof(int16_t) * nh);
	w->biome = malloc(nb);
	w->water = malloc(sizeof(int16_t) * nb);
	w->samples = malloc(sizeof(F3DSRoadSample) * WORLD_ROAD_SAMPLE_MAX);
	w->buildings = malloc(sizeof(F3DSBuilding) * WORLD_BUILDING_MAX);
	g.h = malloc(sizeof(float) * nh);
	g.coast = malloc(sizeof(float) * nb);
	g.wlevel = malloc(sizeof(float) * nb);
	g.wet = malloc(nb);
	bool ok = w->height && w->biome && w->water && w->samples && w->buildings &&
		g.h && g.coast && g.wlevel && g.wet;

	gen_report(&g, 0, "Contour de l'île");
	ok = ok && gen_relief(&g);
	if (ok) gen_report(&g, 40, "Rivières et lacs");
	ok = ok && gen_water(&g) && gen_coast(&g);
	if (ok) gen_report(&g, 46, "Villes et villages");
	ok = ok && gen_sites(&g);
	if (ok) gen_report(&g, 50, "Route principale");
	ok = ok && gen_roads(&g) && w->sampleCount > 2;
	if (ok) gen_report(&g, 76, "Terrassement des routes");
	ok = ok && gen_road_index(&g) && gen_carve_roads(&g);
	if (ok) gen_report(&g, 84, "Bâtiments");
	ok = ok && gen_buildings(&g);
	if (ok) gen_report(&g, 92, "Forêts et paysages");
	ok = ok && gen_biomes(&g);
	if (ok) {
		for (size_t i = 0; i < nh; ++i)
			w->height[i] = (int16_t)lroundf(clampf(g.h[i], -1000.0f, 1000.0f) / WORLD_HEIGHT_UNIT);
		arrival_points(w);
		const F3DSSite *start = &w->sites[0];
		w->spawnX = start->roadX;
		w->spawnZ = start->roadZ;
		w->spawnYaw = start->roadYaw;
	}
	free(g.h);
	free(g.coast);
	free(g.wlevel);
	free(g.wet);
	if (!ok) {
		world_free(w);
		return false;
	}
	gen_report(&g, 100, "Prêt");
	return true;
}
