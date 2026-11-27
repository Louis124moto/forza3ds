/* Queries on a generated island: ground, roads, surfaces, water, biomes. */
#include "world.h"

#define SHOULDER 2.5f

float world_terrain_height(const F3DSWorld *w, float x, float z)
{
	const float fx = clampf((x + WORLD_HALF) / WORLD_CELL, 0.0f, WORLD_N - 1.001f);
	const float fz = clampf((z + WORLD_HALF) / WORLD_CELL, 0.0f, WORLD_N - 1.001f);
	const int i = (int)fx, j = (int)fz;
	const float u = fx - i, v = fz - j;
	const int16_t *p = &w->height[j * WORLD_N + i];
	const float h00 = p[0], h11 = p[WORLD_N + 1];
	/* Each cell is two triangles split along the (0,0)-(1,1) diagonal. */
	const float h = u > v ? h00 + (p[1] - h00) * u + (h11 - p[1]) * v
		: h00 + (h11 - p[WORLD_N]) * u + (p[WORLD_N] - h00) * v;
	return h * WORLD_HEIGHT_UNIT;
}

static int road_cell(float v)
{
	const int i = (int)((v + WORLD_HALF) / WORLD_ROAD_CELL);
	return i < 0 ? 0 : i >= WORLD_ROAD_CELLS ? WORLD_ROAD_CELLS - 1 : i;
}

static void project(const F3DSWorld *w, int s, float x, float z, F3DSRoadHit *hit)
{
	const F3DSRoadSample *a = &w->samples[s], *b = a + 1;
	const float dx = b->x - a->x, dz = b->z - a->z;
	const float len2 = dx * dx + dz * dz;
	const float u = len2 > 0.0f ?
		clampf(((x - a->x) * dx + (z - a->z) * dz) / len2, 0.0f, 1.0f) : 0.0f;
	hit->sample = s;
	hit->t = u;
	hit->x = a->x + dx * u;
	hit->z = a->z + dz * u;
	hit->y = lerpf(a->y, b->y, u);
	hit->half = road_half_width(a->type);
	hit->distance = sqrtf((x - hit->x) * (x - hit->x) + (z - hit->z) * (z - hit->z));
}

bool world_road_nearest(const F3DSWorld *w, float x, float z, float reach, F3DSRoadHit *hit)
{
	const int c = road_cell(z) * WORLD_ROAD_CELLS + road_cell(x);
	float best = reach;
	bool found = false;
	for (uint32_t k = w->cellStart[c]; k < w->cellStart[c + 1]; ++k) {
		F3DSRoadHit h;
		project(w, w->cellItems[k], x, z, &h);
		if (h.distance <= best) {
			best = h.distance;
			*hit = h;
			found = true;
		}
	}
	return found;
}

bool world_road_search(const F3DSWorld *w, float x, float z, F3DSRoadHit *hit)
{
	float best = 1e30f;
	bool found = false;
	for (int s = 0; s + 1 < w->sampleCount; ++s) {
		if (w->samples[s].flags & F3DS_SAMPLE_LAST)
			continue;
		F3DSRoadHit h;
		project(w, s, x, z, &h);
		if (h.distance < best) {
			best = h.distance;
			*hit = h;
			found = true;
		}
	}
	return found;
}

float world_height(const F3DSWorld *w, float x, float z)
{
	const float ground = world_terrain_height(w, x, z);
	const int c = road_cell(z) * WORLD_ROAD_CELLS + road_cell(x);
	float sum = 0.0f, weight = 0.0f, cover = 0.0f;
	/* One candidate per stretch of road (segments of a cell are listed in
	 * order), blended so that junctions have no step. */
	F3DSRoadHit best;
	best.sample = -1;
	int previous = -2;
	for (uint32_t k = w->cellStart[c]; k <= w->cellStart[c + 1]; ++k) {
		const bool end = k == w->cellStart[c + 1];
		const int s = end ? -10 : w->cellItems[k];
		if (end || previous - s != 1) {
			if (best.sample >= 0) {
				const bool bridge = w->samples[best.sample].flags &
					w->samples[best.sample + 1].flags & F3DS_SAMPLE_BRIDGE;
				/* A deck high above the ground has no shoulder to slide down. */
				const float high = bridge ? clampf((best.y - ground - 0.5f) / 1.5f, 0.0f, 1.0f) : 0.0f;
				const float edge = lerpf(SHOULDER, 0.3f, high);
				const float reach = best.half + edge - best.distance;
				if (reach > 0.0f) {
					const float q = reach * reach;
					sum += best.y * q;
					weight += q;
					cover = fmaxf(cover, clampf(reach / edge, 0.0f, 1.0f));
				}
				best.sample = -1;
			}
		}
		if (end)
			break;
		previous = s;
		F3DSRoadHit h;
		project(w, s, x, z, &h);
		if (h.distance < h.half + SHOULDER && (best.sample < 0 || h.distance < best.distance))
			best = h;
	}
	return weight > 0.0f ? lerpf(ground, sum / weight, cover) : ground;
}

static int biome_index(float v)
{
	const int i = (int)((v + WORLD_HALF) / WORLD_BIOME_CELL + 0.5f);
	return i < 0 ? 0 : i >= WORLD_BN ? WORLD_BN - 1 : i;
}

F3DSBiome world_biome(const F3DSWorld *w, float x, float z)
{
	if (fabsf(x) > WORLD_HALF || fabsf(z) > WORLD_HALF)
		return F3DS_BIOME_OCEAN;
	return (F3DSBiome)w->biome[biome_index(z) * WORLD_BN + biome_index(x)];
}

float world_water_level(const F3DSWorld *w, float x, float z)
{
	if (fabsf(x) > WORLD_HALF || fabsf(z) > WORLD_HALF)
		return 0.0f;
	const float ground = world_terrain_height(w, x, z);
	const int i = (int)clampf((x + WORLD_HALF) / WORLD_BIOME_CELL, 0.0f, WORLD_BN - 2.0f);
	const int j = (int)clampf((z + WORLD_HALF) / WORLD_BIOME_CELL, 0.0f, WORLD_BN - 2.0f);
	int level = WORLD_NO_WATER;
	for (int dj = 0; dj <= 1; ++dj)
		for (int di = 0; di <= 1; ++di) {
			const int v = w->water[(j + dj) * WORLD_BN + i + di];
			if (v > level)
				level = v;
		}
	if (level != WORLD_NO_WATER && level * WORLD_HEIGHT_UNIT > ground)
		return level * WORLD_HEIGHT_UNIT;
	return ground < 0.0f ? 0.0f : WORLD_DRY;
}

F3DSSurface world_surface(const F3DSWorld *w, float x, float z)
{
	F3DSRoadHit hit;
	if (world_road_nearest(w, x, z, 5.5f, &hit) && hit.distance <= hit.half) {
		const int type = w->samples[hit.sample].type;
		return type == F3DS_ROAD_DIRT ? F3DS_SURFACE_DIRT :
			type == F3DS_ROAD_HIDDEN ? F3DS_SURFACE_GRAVEL : F3DS_SURFACE_ASPHALT;
	}
	const float water = world_water_level(w, x, z);
	if (water != WORLD_DRY && water - world_terrain_height(w, x, z) > 0.12f)
		return F3DS_SURFACE_WATER;
	switch (world_biome(w, x, z)) {
	case F3DS_BIOME_BEACH:
	case F3DS_BIOME_OCEAN:
		return F3DS_SURFACE_SAND;
	case F3DS_BIOME_ROCK:
	case F3DS_BIOME_SNOW:
	case F3DS_BIOME_LAKE:
	case F3DS_BIOME_RIVER:
		return F3DS_SURFACE_GRAVEL;
	case F3DS_BIOME_FARM:
		return F3DS_SURFACE_DIRT;
	case F3DS_BIOME_CITY:
	case F3DS_BIOME_INDUSTRY:
		return F3DS_SURFACE_ASPHALT;
	default:
		return F3DS_SURFACE_GRASS;
	}
}

const char *world_biome_name(F3DSBiome b)
{
	static const char *names[F3DS_BIOME_COUNT] = {
		"Océan", "Plage", "Littoral", "Plaine", "Campagne", "Forêt", "Bambouseraie",
		"Hauts plateaux", "Rochers", "Neiges", "Lac", "Rivière", "Village", "Ville",
		"Zone industrielle"
	};
	return names[(unsigned)b < F3DS_BIOME_COUNT ? b : 0];
}
