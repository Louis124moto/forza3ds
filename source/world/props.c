/* Per-sector scenery. Only the details seed varies here: the relief, roads and
 * buildings are never affected. */
#include <string.h>
#include "catalog.h"
#include "props.h"
#include "rng.h"

#define TREE_GRID 18                      /* candidates per axis and sector */
#define TREE_STEP (WORLD_SECTOR / TREE_GRID)
#define SALT_PROPS 0x5CE7E417u

static bool add(F3DSSector *s, int kind, float x, float y, float z, float yaw, float scale, int variant)
{
	if (s->propCount >= PROPS_MAX)
		return false;
	F3DSProp *p = &s->props[s->propCount++];
	p->kind = (uint8_t)kind;
	p->variant = (uint8_t)variant;
	p->reserved = 0;
	p->x = x;
	p->y = y;
	p->z = z;
	p->yaw = yaw;
	p->scale = scale;
	return true;
}

/* Distance from (x, z) to the closest asphalt or dirt edge; large when far. */
static float road_clearance(const F3DSWorld *w, float x, float z)
{
	F3DSRoadHit hit;
	return world_road_nearest(w, x, z, 26.0f, &hit) ? hit.distance - hit.half : 100.0f;
}

/* Whether a disc overlaps a building of the sector or of its neighbours. */
static bool near_building(const F3DSWorld *w, float x, float z, float radius, const F3DSBuilding *ignore)
{
	const int cx = (int)((x + WORLD_HALF) / WORLD_SECTOR), cz = (int)((z + WORLD_HALF) / WORLD_SECTOR);
	for (int j = cz - 1; j <= cz + 1; ++j)
		for (int i = cx - 1; i <= cx + 1; ++i) {
			if (i < 0 || j < 0 || i >= WORLD_SECTORS || j >= WORLD_SECTORS)
				continue;
			const int c = j * WORLD_SECTORS + i;
			for (int k = w->sectorBuildings[c]; k < w->sectorBuildings[c + 1]; ++k) {
				const F3DSBuilding *b = &w->buildings[k];
				if (b == ignore || b->kind == F3DS_BUILD_TORII)
					continue;
				const float dx = x - b->x, dz = z - b->z;
				const float co = cosf(b->yaw), si = sinf(b->yaw);
				const float u = fmaxf(fabsf(dx * co - dz * si) - b->hx, 0.0f);
				const float v = fmaxf(fabsf(dx * si + dz * co) - b->hz, 0.0f);
				if (u * u + v * v < radius * radius)
					return true;
			}
		}
	return false;
}

typedef struct { uint8_t kind, share; } Mix;
typedef struct { float density; Mix mix[4]; } Flora;

static const Flora flora[F3DS_BIOME_COUNT] = {
	[F3DS_BIOME_BEACH] = { 0.07f, { { F3DS_PROP_PALM, 80 }, { F3DS_PROP_ROCK, 20 } } },
	[F3DS_BIOME_COAST] = { 0.17f, { { F3DS_PROP_PINE, 55 }, { F3DS_PROP_ROCK, 25 }, { F3DS_PROP_BUSH, 20 } } },
	[F3DS_BIOME_PLAIN] = { 0.09f, { { F3DS_PROP_SAKURA, 20 }, { F3DS_PROP_PINE, 25 }, { F3DS_PROP_BUSH, 45 }, { F3DS_PROP_ROCK, 10 } } },
	[F3DS_BIOME_FARM] = { 0.08f, { { F3DS_PROP_HAY, 60 }, { F3DS_PROP_BUSH, 30 }, { F3DS_PROP_SAKURA, 10 } } },
	[F3DS_BIOME_FOREST] = { 0.78f, { { F3DS_PROP_CEDAR, 45 }, { F3DS_PROP_PINE, 33 }, { F3DS_PROP_MAPLE, 14 }, { F3DS_PROP_BUSH, 8 } } },
	[F3DS_BIOME_BAMBOO] = { 0.86f, { { F3DS_PROP_BAMBOO, 92 }, { F3DS_PROP_BUSH, 8 } } },
	[F3DS_BIOME_HIGHLAND] = { 0.24f, { { F3DS_PROP_PINE, 50 }, { F3DS_PROP_ROCK, 30 }, { F3DS_PROP_BUSH, 20 } } },
	[F3DS_BIOME_ROCK] = { 0.20f, { { F3DS_PROP_ROCK, 85 }, { F3DS_PROP_PINE, 15 } } },
	[F3DS_BIOME_SNOW] = { 0.12f, { { F3DS_PROP_ROCK, 60 }, { F3DS_PROP_PINE, 40 } } },
	[F3DS_BIOME_LAKE] = { 0.08f, { { F3DS_PROP_BUSH, 60 }, { F3DS_PROP_ROCK, 40 } } },
	[F3DS_BIOME_RIVER] = { 0.10f, { { F3DS_PROP_BUSH, 55 }, { F3DS_PROP_ROCK, 45 } } },
	[F3DS_BIOME_VILLAGE] = { 0.12f, { { F3DS_PROP_SAKURA, 55 }, { F3DS_PROP_BUSH, 45 } } },
	[F3DS_BIOME_CITY] = { 0.04f, { { F3DS_PROP_SAKURA, 50 }, { F3DS_PROP_BUSH, 50 } } },
	[F3DS_BIOME_INDUSTRY] = { 0.03f, { { F3DS_PROP_BUSH, 60 }, { F3DS_PROP_ROCK, 40 } } },
};

static void vegetation(const F3DSWorld *w, F3DSSector *s, F3DSRng *r, float x0, float z0)
{
	for (int j = 0; j < TREE_GRID; ++j)
		for (int i = 0; i < TREE_GRID; ++i) {
			/* Draw every value so that a rejected spot does not shift the
			 * ones after it. */
			const float x = x0 + (i + rng_range(r, 0.1f, 0.9f)) * TREE_STEP;
			const float z = z0 + (j + rng_range(r, 0.1f, 0.9f)) * TREE_STEP;
			const float chance = rng_float(r), pick = rng_float(r) * 100.0f;
			const float scale = rng_range(r, 0.75f, 1.35f), yaw = rng_range(r, 0.0f, 2.0f * F3DS_PI);
			const F3DSBiome biome = world_biome(w, x, z);
			const Flora *f = &flora[biome];
			if (chance >= f->density)
				continue;
			int kind = -1;
			float total = 0.0f;
			for (int k = 0; k < 4 && kind < 0; ++k) {
				total += f->mix[k].share;
				if (pick < total)
					kind = f->mix[k].kind;
			}
			if (kind < 0)
				continue;
			const float y = world_terrain_height(w, x, z);
			const float water = world_water_level(w, x, z);
			if (y < 0.5f || (water != WORLD_DRY && water > y - 0.1f))
				continue;
			const float slope = fabsf(world_terrain_height(w, x + 3.0f, z) - y) +
				fabsf(world_terrain_height(w, x, z + 3.0f) - y);
			if (slope > 4.2f && kind != F3DS_PROP_ROCK)
				continue;
			const bool small = kind == F3DS_PROP_BUSH;
			if (road_clearance(w, x, z) < (small ? 2.5f : 4.5f) || near_building(w, x, z, 2.5f, NULL))
				continue;
			add(s, kind, x, y, z, yaw, scale, biome == F3DS_BIOME_SNOW ? 1 : (int)(pick) & 1);
		}
}

static bool settled(F3DSBiome b)
{
	return b == F3DS_BIOME_CITY || b == F3DS_BIOME_VILLAGE || b == F3DS_BIOME_INDUSTRY;
}

/* Lamps, poles and guard rails along the road segments that start here. */
static void roadside(const F3DSWorld *w, F3DSSector *s)
{
	const int perSector = (int)(WORLD_SECTOR / WORLD_ROAD_CELL);
	for (int cj = s->sz * perSector; cj < (s->sz + 1) * perSector; ++cj)
		for (int ci = s->sx * perSector; ci < (s->sx + 1) * perSector; ++ci) {
			const int c = cj * WORLD_ROAD_CELLS + ci;
			for (uint32_t k = w->cellStart[c]; k < w->cellStart[c + 1]; ++k) {
				const int n = w->cellItems[k];
				const F3DSRoadSample *a = &w->samples[n], *b = a + 1;
				/* A segment is listed in several cells: use its start. */
				if ((int)((a->x + WORLD_HALF) / WORLD_ROAD_CELL) != ci ||
				    (int)((a->z + WORLD_HALF) / WORLD_ROAD_CELL) != cj)
					continue;
				const float dx = b->x - a->x, dz = b->z - a->z;
				const float len = hypotf(dx, dz);
				if (len < 0.5f)
					continue;
				const float nx = -dz / len, nz = dx / len;
				const float half = road_half_width(a->type);
				const bool bridge = a->flags & b->flags & F3DS_SAMPLE_BRIDGE;
				const bool paved = a->type <= F3DS_ROAD_STREET;
				const bool town = settled(world_biome(w, a->x, a->z));

				for (int side = -1; side <= 1; side += 2) {
					const float off = (half + 0.55f) * side;
					const float mx = (a->x + b->x) * 0.5f + nx * off, mz = (a->z + b->z) * 0.5f + nz * off;
					const float drop = (a->y + b->y) * 0.5f - world_terrain_height(w,
						mx + nx * side * 17.0f, mz + nz * side * 17.0f);
					if ((bridge || (paved && !town && drop > 3.2f)) &&
					    road_clearance(w, mx, mz) > 0.35f && s->railCount < PROPS_RAIL_MAX) {
						F3DSRail *rail = &s->rails[s->railCount++];
						rail->ax = a->x + nx * off;
						rail->az = a->z + nz * off;
						rail->ay = a->y;
						rail->bx = b->x + nx * off;
						rail->bz = b->z + nz * off;
						rail->by = b->y;
					}
				}
				if (bridge || !paved)
					continue;
				const bool lamp = town && n % 4 == 0;
				const bool pole = !town && a->type == F3DS_ROAD_MAIN && n % 6 == 0 &&
					world_biome(w, a->x, a->z) != F3DS_BIOME_ROCK;
				if (!lamp && !pole)
					continue;
				const float side = lamp && (n / 4) % 2 ? -1.0f : 1.0f;
				const float off = (half + (lamp ? 1.1f : 3.2f)) * side;
				const float x = a->x + nx * off, z = a->z + nz * off;
				if (road_clearance(w, x, z) < 0.8f || near_building(w, x, z, 0.4f, NULL))
					continue;
				const float y = world_terrain_height(w, x, z);
				const float water = world_water_level(w, x, z);
				if (y < 0.5f || (water != WORLD_DRY && water > y))
					continue;
				add(s, lamp ? F3DS_PROP_LAMP : F3DS_PROP_POLE, x, y, z,
					lamp ? atan2f(-nx * side, -nz * side) : atan2f(dx, dz), 1.0f, 0);
			}
		}
}

/* Parked cars beside buildings and boats by the piers. */
static void around_buildings(const F3DSWorld *w, F3DSSector *s, F3DSRng *r)
{
	const int c = s->sz * WORLD_SECTORS + s->sx;
	for (int k = w->sectorBuildings[c]; k < w->sectorBuildings[c + 1]; ++k) {
		const F3DSBuilding *b = &w->buildings[k];
		const float chance = rng_float(r);
		const float side = rng_float(r) < 0.5f ? -1.0f : 1.0f;
		const int model = rng_int(r, F3DS_CAR_COUNT), paint = rng_int(r, F3DS_PAINT_COUNT);
		const float co = cosf(b->yaw), si = sinf(b->yaw);
		if (b->kind == F3DS_BUILD_PIER) {
			for (int n = 0; n < 2; ++n) {
				const float u = (b->hx + 2.2f) * (n ? -1.0f : 1.0f), v = rng_range(r, -0.2f, 0.7f) * b->hz;
				const float x = b->x + u * co + v * si, z = b->z - u * si + v * co;
				if (world_terrain_height(w, x, z) < -0.6f)
					add(s, F3DS_PROP_BOAT, x, 0.0f, z, b->yaw, rng_range(r, 0.8f, 1.3f), paint);
			}
			continue;
		}
		if (chance > 0.34f || (b->kind != F3DS_BUILD_HOUSE && b->kind != F3DS_BUILD_SHOP &&
		    b->kind != F3DS_BUILD_BLOCK && b->kind != F3DS_BUILD_WAREHOUSE))
			continue;
		const float u = (b->hx + 1.7f) * side;
		const float x = b->x + u * co, z = b->z - u * si;
		const float y = world_terrain_height(w, x, z);
		/* Room for the whole car: off the road and away from walls. */
		bool free = y > 1.0f && fabsf(y - b->y) < 1.5f;
		for (int e = -1; e <= 1 && free; ++e) {
			const float px = x + e * 1.6f * si, pz = z + e * 1.6f * co;
			free = road_clearance(w, px, pz) > 1.4f && !near_building(w, px, pz, 1.1f, b) &&
				world_water_level(w, px, pz) == WORLD_DRY;
		}
		if (free)
			add(s, F3DS_PROP_CAR, x, y, z, b->yaw, 1.0f, model);
	}
}

void props_build(const F3DSWorld *w, int sx, int sz, F3DSSector *out)
{
	out->sx = sx;
	out->sz = sz;
	out->propCount = 0;
	out->railCount = 0;
	if (sx < 0 || sz < 0 || sx >= WORLD_SECTORS || sz >= WORLD_SECTORS || !w->height)
		return;
	const uint32_t seed = hash_seed(hash_seed(w->seed, SALT_PROPS), w->detail);
	F3DSRng r = rng_make(hash_2d(seed, sx, sz));
	const float x0 = sx * WORLD_SECTOR - WORLD_HALF, z0 = sz * WORLD_SECTOR - WORLD_HALF;
	/* Solid things first, so that vegetation never crowds them out. */
	roadside(w, out);
	around_buildings(w, out, &r);
	F3DSRng plants = rng_make(hash_2d(seed + 1u, sx, sz));
	vegetation(w, out, &plants, x0, z0);
}

bool props_collider(const F3DSProp *p, F3DSCollider *out)
{
	float radius = 0.0f, height = 4.0f;
	switch (p->kind) {
	case F3DS_PROP_PINE:
	case F3DS_PROP_SAKURA:
	case F3DS_PROP_MAPLE: radius = 0.32f * p->scale; break;
	case F3DS_PROP_CEDAR: radius = 0.45f * p->scale; break;
	case F3DS_PROP_PALM: radius = 0.28f * p->scale; break;
	case F3DS_PROP_BAMBOO: radius = 0.45f * p->scale; break;
	case F3DS_PROP_ROCK:
		/* Pebbles are driven over. */
		if (p->scale < 0.9f)
			return false;
		radius = 0.85f * p->scale;
		height = 1.2f * p->scale;
		break;
	case F3DS_PROP_HAY: radius = 0.75f * p->scale; height = 1.3f; break;
	case F3DS_PROP_LAMP: radius = 0.18f; height = 7.0f; break;
	case F3DS_PROP_POLE: radius = 0.20f; height = 8.0f; break;
	case F3DS_PROP_CAR: break;
	default: return false;
	}
	out->shape = F3DS_COLLIDER_CIRCLE;
	out->x = p->x;
	out->z = p->z;
	out->hx = radius;
	out->hz = radius;
	out->yaw = 0.0f;
	out->y0 = p->y - 1.0f;
	out->y1 = p->y + height;
	if (p->kind == F3DS_PROP_CAR) {
		const F3DSCarVisual *v = catalog_visual(p->variant);
		out->shape = F3DS_COLLIDER_BOX;
		out->hx = v->halfWidth;
		out->hz = v->halfLength;
		out->yaw = p->yaw;
		out->y1 = p->y + v->roofY;
	}
	return true;
}

void scenery_reset(F3DSScenery *s, const F3DSWorld *w)
{
	s->world = w;
	s->clock = 0;
	s->builds = 0;
	for (int i = 0; i < SCENERY_CACHE; ++i) {
		s->used[i] = 0;
		s->sector[i].sx = s->sector[i].sz = -1;
	}
}

const F3DSSector *scenery_sector(F3DSScenery *s, int sx, int sz)
{
	int oldest = 0;
	for (int i = 0; i < SCENERY_CACHE; ++i) {
		if (s->used[i] && s->sector[i].sx == sx && s->sector[i].sz == sz) {
			s->used[i] = ++s->clock;
			return &s->sector[i];
		}
		if (s->used[i] < s->used[oldest])
			oldest = i;
	}
	props_build(s->world, sx, sz, &s->sector[oldest]);
	s->used[oldest] = ++s->clock;
	s->builds++;
	return &s->sector[oldest];
}

/* Whether an obstacle can reach the disc (x, z, radius); stores it if so. */
static int keep(const F3DSCollider *c, float x, float z, float radius, F3DSCollider *out, int n, int max)
{
	const float reach = radius + (c->shape == F3DS_COLLIDER_BOX ? c->hx + c->hz : c->hx);
	const float dx = c->x - x, dz = c->z - z;
	if (n < max && dx * dx + dz * dz < reach * reach)
		out[n++] = *c;
	return n;
}

int scenery_colliders(F3DSScenery *s, float x, float z, float radius, F3DSCollider *out, int max)
{
	const F3DSWorld *w = s->world;
	if (!w || !w->height)
		return 0;
	int n = 0;
	const int cx = (int)floorf((x + WORLD_HALF) / WORLD_SECTOR);
	const int cz = (int)floorf((z + WORLD_HALF) / WORLD_SECTOR);
	/* Buildings are few: look at every neighbouring sector. */
	for (int j = cz - 1; j <= cz + 1; ++j)
		for (int i = cx - 1; i <= cx + 1; ++i) {
			if (i < 0 || j < 0 || i >= WORLD_SECTORS || j >= WORLD_SECTORS)
				continue;
			const int c = j * WORLD_SECTORS + i;
			for (int k = w->sectorBuildings[c]; k < w->sectorBuildings[c + 1]; ++k) {
				const F3DSBuilding *b = &w->buildings[k];
				F3DSCollider o = { b->x, b->z, b->hx, b->hz, b->yaw, b->y - 2.0f,
					b->y + b->height + 3.0f, F3DS_COLLIDER_BOX };
				if (b->kind == F3DS_BUILD_TANK || b->kind == F3DS_BUILD_CHIMNEY ||
				    b->kind == F3DS_BUILD_LIGHTHOUSE)
					o.shape = F3DS_COLLIDER_CIRCLE;
				if (b->kind != F3DS_BUILD_TORII) {
					n = keep(&o, x, z, radius, out, n, max);
					continue;
				}
				for (int side = -1; side <= 1; side += 2) {
					F3DSCollider pillar = o;
					pillar.shape = F3DS_COLLIDER_CIRCLE;
					pillar.x = b->x + side * b->hx * cosf(b->yaw);
					pillar.z = b->z - side * b->hx * sinf(b->yaw);
					pillar.hx = pillar.hz = 0.35f;
					n = keep(&pillar, x, z, radius, out, n, max);
				}
			}
		}
	/* Props and rails belong to the sector their position starts in; rails and
	 * parked cars reach a few metres beyond it. */
	const float edge = radius + 9.0f;
	const int i0 = (int)floorf((x - edge + WORLD_HALF) / WORLD_SECTOR);
	const int i1 = (int)floorf((x + edge + WORLD_HALF) / WORLD_SECTOR);
	const int j0 = (int)floorf((z - edge + WORLD_HALF) / WORLD_SECTOR);
	const int j1 = (int)floorf((z + edge + WORLD_HALF) / WORLD_SECTOR);
	for (int j = j0; j <= j1; ++j)
		for (int i = i0; i <= i1; ++i) {
			if (i < 0 || j < 0 || i >= WORLD_SECTORS || j >= WORLD_SECTORS)
				continue;
			const F3DSSector *sector = scenery_sector(s, i, j);
			for (int k = 0; k < sector->propCount; ++k) {
				const F3DSProp *p = &sector->props[k];
				F3DSCollider o;
				if (fabsf(p->x - x) < radius + 4.0f && fabsf(p->z - z) < radius + 4.0f &&
				    props_collider(p, &o))
					n = keep(&o, x, z, radius, out, n, max);
			}
			for (int k = 0; k < sector->railCount; ++k) {
				const F3DSRail *q = &sector->rails[k];
				const float dx = q->bx - q->ax, dz = q->bz - q->az;
				const F3DSCollider o = { (q->ax + q->bx) * 0.5f, (q->az + q->bz) * 0.5f, 0.15f,
					hypotf(dx, dz) * 0.5f + 0.1f, atan2f(dx, dz), fminf(q->ay, q->by) - 1.0f,
					fmaxf(q->ay, q->by) + 1.1f, F3DS_COLLIDER_BOX };
				n = keep(&o, x, z, radius, out, n, max);
			}
		}
	return n;
}
