/* The island generator: determinism, road network, sites and buildings. */
#include <stdlib.h>
#include <string.h>
#include "test.h"
#include "world.h"

static int root(int *p, int i)
{
	while (p[i] != i) {
		p[i] = p[p[i]];
		i = p[i];
	}
	return i;
}

/* Every stretch of road is joined to every other one. */
static void connectivity(const F3DSWorld *w)
{
	static int p[WORLD_ROAD_SAMPLE_MAX];
	for (int i = 0; i < w->sampleCount; ++i)
		p[i] = i;
	for (int i = 0; i + 1 < w->sampleCount; ++i)
		if (!(w->samples[i].flags & F3DS_SAMPLE_LAST))
			p[root(p, i)] = root(p, i + 1);
	for (int i = 0; i < w->sampleCount; ++i)
		for (int j = i + 1; j < w->sampleCount; ++j) {
			const float dx = w->samples[i].x - w->samples[j].x;
			const float dz = w->samples[i].z - w->samples[j].z;
			if (dx * dx + dz * dz < 49.0f && fabsf(w->samples[i].y - w->samples[j].y) < 1.0f)
				p[root(p, i)] = root(p, j);
		}
	int joined = 0;
	for (int i = 0; i < w->sampleCount; ++i)
		joined += root(p, i) == root(p, 0);
	CHECKF(joined == w->sampleCount, "seed %u: %d of %d", w->seed, joined, w->sampleCount);
}

static void check_island(const F3DSWorld *w)
{
	CHECKF(w->roadLength > 30000.0f, "%f", w->roadLength);
	CHECKF(w->peakHeight > 300.0f && w->peakHeight < 1000.0f, "%f", w->peakHeight);
	CHECK(w->routeFailures == 0);
	CHECK(w->lakeCount > 0 && w->riverCells > 100);
	CHECK(w->buildingCount > 500 && w->buildingCount <= WORLD_BUILDING_MAX);
	CHECKF(world_memory(w) < 4u * 1024u * 1024u, "%zu", world_memory(w));

	int kinds[F3DS_SITE_KIND_COUNT] = { 0 };
	for (int i = 0; i < w->siteCount; ++i) {
		const F3DSSite *s = &w->sites[i];
		kinds[s->kind]++;
		F3DSRoadHit hit;
		CHECKF(world_road_nearest(w, s->roadX, s->roadZ, 6.0f, &hit) && hit.distance < 0.5f, "%s", s->name);
		CHECKF(hypotf(s->roadX - s->x, s->roadZ - s->z) < (s->kind == F3DS_SITE_LAKE ? 260.0f : 130.0f), "%s %f", s->name,
			hypotf(s->roadX - s->x, s->roadZ - s->z));
		CHECK(s->name[0] != '\0');
	}
	CHECK(kinds[F3DS_SITE_CITY] == 1 && kinds[F3DS_SITE_INDUSTRY] == 1);
	CHECK(kinds[F3DS_SITE_VILLAGE] >= 4 && kinds[F3DS_SITE_VIEWPOINT] == 1);
	CHECK(kinds[F3DS_SITE_LIGHTHOUSE] == 1 && kinds[F3DS_SITE_HARBOUR] == 1);

	/* Many different environments. */
	int biomes[F3DS_BIOME_COUNT] = { 0 };
	for (int c = 0; c < WORLD_BN * WORLD_BN; ++c)
		biomes[w->biome[c]]++;
	for (int b = 0; b < F3DS_BIOME_COUNT; ++b)
		CHECKF(biomes[b] > 10, "seed %u biome %s: %d", w->seed, world_biome_name((F3DSBiome)b), biomes[b]);
	/* The sea all around. */
	for (float t = -WORLD_HALF; t <= WORLD_HALF; t += 64.0f) {
		CHECK(world_terrain_height(w, t, -WORLD_HALF) < 0.0f);
		CHECK(world_terrain_height(w, WORLD_HALF, t) < 0.0f);
	}

	/* Roads: bounded grade, no step, ground below, never under water. */
	int sunk = 0, wet = 0, segments = 0;
	float worstStep = 0.0f, worstGrade = 0.0f;
	for (int s = 0; s + 1 < w->sampleCount; ++s) {
		const F3DSRoadSample *a = &w->samples[s], *b = a + 1;
		if (a->flags & F3DS_SAMPLE_LAST)
			continue;
		++segments;
		const float len = hypotf(b->x - a->x, b->z - a->z);
		if (len > 1.0f)
			worstGrade = fmaxf(worstGrade, fabsf(b->y - a->y) / len);
		float previous = world_height(w, a->x, a->z);
		const int n = (int)(len * 2.0f) + 1;
		for (int k = 1; k <= n; ++k) {
			const float u = (float)k / n;
			const float y = world_height(w, lerpf(a->x, b->x, u), lerpf(a->z, b->z, u));
			worstStep = fmaxf(worstStep, fabsf(y - previous));
			previous = y;
		}
		if (!(a->flags & F3DS_SAMPLE_BRIDGE)) {
			sunk += world_terrain_height(w, a->x, a->z) > a->y + 0.02f;
			const float level = world_water_level(w, a->x, a->z);
			wet += level != WORLD_DRY && level > a->y + 0.05f;
		}
	}
	CHECKF(worstGrade < 0.36f, "seed %u grade %f", w->seed, worstGrade);
	CHECKF(worstStep < 0.5f, "seed %u step %f", w->seed, worstStep);
	CHECKF(sunk * 200 < segments, "seed %u sunk %d/%d", w->seed, sunk, segments);
	CHECKF(wet == 0, "seed %u wet %d", w->seed, wet);

	/* Buildings: sorted by sector, on land, off the road. */
	CHECK(w->sectorBuildings[WORLD_SECTORS * WORLD_SECTORS] == w->buildingCount);
	for (int c = 0; c < WORLD_SECTORS * WORLD_SECTORS; ++c)
		for (int i = w->sectorBuildings[c]; i < w->sectorBuildings[c + 1]; ++i) {
			const F3DSBuilding *o = &w->buildings[i];
			CHECK((int)((o->x + WORLD_HALF) / WORLD_SECTOR) == c % WORLD_SECTORS);
			CHECK((int)((o->z + WORLD_HALF) / WORLD_SECTOR) == c / WORLD_SECTORS);
			if (o->kind == F3DS_BUILD_TORII || o->kind == F3DS_BUILD_PIER)
				continue;
			CHECK(o->y > 0.0f);
			F3DSRoadHit hit;
			if (world_road_nearest(w, o->x, o->z, 25.0f, &hit))
				CHECKF(hit.distance > hit.half + fminf(o->hx, o->hz), "kind %d at %f %f: %f",
					o->kind, o->x, o->z, hit.distance);
		}

	/* The start is on a road, dry and on asphalt. */
	F3DSRoadHit hit;
	CHECK(world_road_nearest(w, w->spawnX, w->spawnZ, 2.0f, &hit));
	CHECK(world_surface(w, w->spawnX, w->spawnZ) == F3DS_SURFACE_ASPHALT);
	CHECK(world_surface(w, WORLD_HALF - 10.0f, 0.0f) == F3DS_SURFACE_WATER);
	CHECK(world_water_level(w, WORLD_HALF - 10.0f, 0.0f) == 0.0f);
}

void test_island(void)
{
	static F3DSWorld w, other;
	static const uint32_t seeds[] = { 1u, 8u, 13u, 2026u };
	uint32_t previous = 0;
	for (int i = 0; i < 4; ++i) {
		CHECK(world_generate(&w, seeds[i], 1u, NULL, NULL));
		if (!w.height)
			return;
		check_island(&w);
		connectivity(&w);
		const uint32_t sum = world_checksum(&w);
		CHECK(sum != previous);
		previous = sum;
		if (i == 0) {
			/* Same seed, same island; details do not touch the structure. */
			CHECK(world_generate(&other, seeds[i], 99u, NULL, NULL));
			CHECK(world_checksum(&other) == sum);
			CHECK(other.detail == 99u && other.version == WORLD_GEN_VERSION);
			world_free(&other);
			CHECK(other.height == NULL && world_memory(&other) == 0);
		}
		world_free(&w);
	}
}
