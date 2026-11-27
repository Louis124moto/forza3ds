/* The island generator: determinism, road network, sites and buildings. */
#include <stdlib.h>
#include <string.h>
#include "test.h"
#include "props.h"

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

/* Decor: reproducible, bounded, off the road, and driven by the details seed. */
static void check_props(F3DSWorld *w)
{
	static F3DSSector a, b;
	static F3DSScenery scenery;
	int total = 0, most = 0, rails = 0, mostRails = 0, kinds[F3DS_PROP_COUNT] = { 0 }, blocked = 0;
	for (int sz = 0; sz < WORLD_SECTORS; ++sz)
		for (int sx = 0; sx < WORLD_SECTORS; ++sx) {
			props_build(w, sx, sz, &a);
			total += a.propCount;
			rails += a.railCount;
			if (a.propCount > most) most = a.propCount;
			if (a.railCount > mostRails) mostRails = a.railCount;
			for (int i = 0; i < a.propCount; ++i) {
				const F3DSProp *p = &a.props[i];
				kinds[p->kind]++;
				F3DSCollider c;
				F3DSRoadHit hit;
				if (props_collider(p, &c) && world_road_nearest(w, p->x, p->z, 12.0f, &hit))
					blocked += hit.distance < hit.half + (c.shape == F3DS_COLLIDER_BOX ? c.hx : c.hx) * 0.8f;
			}
		}
	printf("  decor: %d props (max %d per sector), %d rails (max %d)\n", total, most, rails, mostRails);
	CHECK(total > 20000);
	CHECKF(most < PROPS_MAX, "%d", most);
	CHECKF(mostRails < PROPS_RAIL_MAX, "%d", mostRails);
	CHECK(rails > 200);
	for (int k = 0; k < F3DS_PROP_COUNT; ++k)
		CHECKF(kinds[k] > 0, "prop kind %d", k);
	CHECKF(blocked == 0, "%d obstacles on a road", blocked);

	/* A town sector: same seeds, same decor; other details, other decor and
	 * the same structure. */
	const int sx = (int)((w->sites[0].x + WORLD_HALF) / WORLD_SECTOR);
	const int sz = (int)((w->sites[0].z + WORLD_HALF) / WORLD_SECTOR);
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	props_build(w, sx, sz, &a);
	props_build(w, sx, sz, &b);
	CHECK(a.propCount > 0 && memcmp(&a, &b, sizeof(a)) == 0);
	const uint32_t sum = world_checksum(w);
	w->detail++;
	props_build(w, sx, sz, &b);
	CHECK(memcmp(&a, &b, sizeof(a)) != 0);
	CHECK(world_checksum(w) == sum);
	w->detail--;

	/* The cache serves the same sector without rebuilding it. */
	scenery_reset(&scenery, w);
	F3DSCollider out[64];
	const int n = scenery_colliders(&scenery, w->sites[0].x, w->sites[0].z, 40.0f, out, 64);
	CHECK(n > 0 && n <= 64);
	const uint32_t builds = scenery.builds;
	CHECK(scenery_colliders(&scenery, w->sites[0].x, w->sites[0].z, 40.0f, out, 64) == n);
	CHECK(scenery.builds == builds);
	for (int i = 0; i < 20; ++i)
		scenery_sector(&scenery, i, 16);
	CHECK(scenery.builds >= builds + 20);
	CHECK(scenery_colliders(&scenery, 1e6f, 1e6f, 10.0f, out, 64) == 0);
	/* The start is clear. */
	CHECK(scenery_colliders(&scenery, w->spawnX, w->spawnZ, 2.5f, out, 64) >= 0);
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
		if (i < 2)
			check_props(&w);
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
