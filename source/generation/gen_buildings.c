/* Buildings along the streets of the city, villages and industrial zone, plus
 * the landmarks: shrine, torii gates, lighthouse and harbour piers. */
#include "gen.h"
#include <string.h>

#define ROAD_GAP 1.5f     /* clear space between a wall and the asphalt edge */
#define NEIGHBOUR_GAP 1.0f

typedef struct { float x, z, ax, az, bx, bz, hx, hz; } Box;

static Box make_box(float x, float z, float hx, float hz, float yaw)
{
	/* a: local +X (across), b: local +Z (towards the front). */
	const Box b = { x, z, cosf(yaw), -sinf(yaw), sinf(yaw), cosf(yaw), hx, hz };
	return b;
}

static float box_point_distance(const Box *b, float x, float z)
{
	const float dx = x - b->x, dz = z - b->z;
	const float u = fmaxf(fabsf(dx * b->ax + dz * b->az) - b->hx, 0.0f);
	const float v = fmaxf(fabsf(dx * b->bx + dz * b->bz) - b->hz, 0.0f);
	return sqrtf(u * u + v * v);
}

/* Separating axis test between two rotated rectangles. */
static bool boxes_touch(const Box *p, const Box *q, float gap)
{
	const float axes[4][2] = {
		{ p->ax, p->az }, { p->bx, p->bz }, { q->ax, q->az }, { q->bx, q->bz }
	};
	for (int i = 0; i < 4; ++i) {
		const float x = axes[i][0], z = axes[i][1];
		const float rp = p->hx * fabsf(p->ax * x + p->az * z) + p->hz * fabsf(p->bx * x + p->bz * z);
		const float rq = q->hx * fabsf(q->ax * x + q->az * z) + q->hz * fabsf(q->bx * x + q->bz * z);
		if (fabsf((q->x - p->x) * x + (q->z - p->z) * z) > rp + rq + gap)
			return false;
	}
	return true;
}

static bool clear_of_roads(const F3DSWorld *w, const Box *b, float gap)
{
	const int i = gen_clampi((int)((b->x + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
	const int j = gen_clampi((int)((b->z + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
	const int c = j * WORLD_ROAD_CELLS + i;
	for (uint32_t k = w->cellStart[c]; k < w->cellStart[c + 1]; ++k) {
		const F3DSRoadSample *s = &w->samples[w->cellItems[k]], *t = s + 1;
		const float need = road_half_width(s->type) + gap;
		for (int q = 0; q <= 4; ++q) {
			const float u = q * 0.25f;
			if (box_point_distance(b, lerpf(s->x, t->x, u), lerpf(s->z, t->z, u)) < need)
				return false;
		}
	}
	return true;
}

static bool clear_of_buildings(const F3DSWorld *w, const Box *b)
{
	for (int i = 0; i < w->buildingCount; ++i) {
		const F3DSBuilding *o = &w->buildings[i];
		const float reach = b->hx + b->hz + o->hx + o->hz + NEIGHBOUR_GAP;
		if (fabsf(o->x - b->x) > reach || fabsf(o->z - b->z) > reach || o->kind == F3DS_BUILD_TORII)
			continue;
		const Box other = make_box(o->x, o->z, o->hx, o->hz, o->yaw);
		if (boxes_touch(b, &other, NEIGHBOUR_GAP))
			return false;
	}
	return true;
}

/* Dry, reasonably level ground under a footprint; returns its lowest point. */
static bool footing(const F3DSGen *g, const Box *b, float *base)
{
	float lo = 1e9f, hi = -1e9f;
	for (int j = -1; j <= 1; ++j)
		for (int i = -1; i <= 1; ++i) {
			const float x = b->x + i * b->hx * b->ax + j * b->hz * b->bx;
			const float z = b->z + i * b->hx * b->az + j * b->hz * b->bz;
			const float h = gen_height_at(g, x, z);
			if (h < 0.9f || g->wlevel[gen_bindex(x, z)] != WORLD_DRY)
				return false;
			lo = fminf(lo, h);
			hi = fmaxf(hi, h);
		}
	*base = lo;
	return hi - lo <= 3.0f;
}

static void push(F3DSWorld *w, int kind, float x, float z, float y, float hx, float hz, float height,
	float yaw, int variant)
{
	F3DSBuilding *o = &w->buildings[w->buildingCount++];
	memset(o, 0, sizeof(*o));
	o->kind = (uint8_t)kind;
	o->x = x;
	o->z = z;
	o->y = y;
	o->hx = hx;
	o->hz = hz;
	o->height = height;
	o->yaw = yaw;
	o->variant = (uint8_t)variant;
}

static bool try_place(F3DSGen *g, int kind, float x, float z, float hx, float hz, float height,
	float yaw, int variant)
{
	F3DSWorld *w = g->w;
	if (w->buildingCount >= WORLD_BUILDING_MAX)
		return false;
	const Box b = make_box(x, z, hx, hz, yaw);
	float base;
	if (!footing(g, &b, &base) || !clear_of_roads(w, &b, ROAD_GAP) || !clear_of_buildings(w, &b))
		return false;
	push(w, kind, x, z, base, hx, hz, height, yaw, variant);
	return true;
}

/* The nearest free spot around a point, facing it. */
static bool place_near(F3DSGen *g, int kind, float x, float z, float hx, float hz, float height,
	float preferX, float preferZ)
{
	const float first = atan2f(preferX, preferZ);
	for (float radius = 0.0f; radius <= 90.0f; radius += 5.0f)
		for (int k = 0; k < 16; ++k) {
			/* Fan out on both sides of the preferred direction. */
			const float a = first + (float)((k + 1) / 2) * (k & 1 ? 1.0f : -1.0f) * F3DS_PI / 8.0f;
			const float px = x + sinf(a) * radius, pz = z + cosf(a) * radius;
			if (try_place(g, kind, px, pz, hx, hz, height, wrap_angle(a + F3DS_PI), 0))
				return true;
		}
	return false;
}

/* A gate spanning the road closest to (x, z). */
static void torii(F3DSGen *g, float x, float z)
{
	F3DSWorld *w = g->w;
	int best = -1;
	float bestD = 90.0f * 90.0f;
	for (int s = 1; s + 1 < w->sampleCount; ++s) {
		const F3DSRoadSample *p = &w->samples[s];
		if ((p->flags & (F3DS_SAMPLE_LAST | F3DS_SAMPLE_BRIDGE)) ||
		    (p[-1].flags & F3DS_SAMPLE_LAST))
			continue;
		const float d = (p->x - x) * (p->x - x) + (p->z - z) * (p->z - z);
		/* Not right on top of the destination. */
		if (d < bestD && d > 22.0f * 22.0f) {
			bestD = d;
			best = s;
		}
	}
	if (best < 0 || w->buildingCount >= WORLD_BUILDING_MAX)
		return;
	const F3DSRoadSample *p = &w->samples[best];
	const float yaw = atan2f(p[1].x - p[0].x, p[1].z - p[0].z);
	const float hx = road_half_width(p->type) + 1.6f;
	/* Both pillars must stand clear of every road and building. */
	for (int side = -1; side <= 1; side += 2) {
		const Box pillar = make_box(p->x + side * hx * cosf(yaw), p->z - side * hx * sinf(yaw),
			0.4f, 0.4f, yaw);
		if (!clear_of_roads(w, &pillar, 0.4f) || !clear_of_buildings(w, &pillar))
			return;
	}
	push(w, F3DS_BUILD_TORII, p->x, p->z, p->y - 0.2f, hx, 0.4f, 6.2f, yaw, 0);
}

static void piers(F3DSGen *g, const F3DSSite *s)
{
	/* The shortest way from the harbour to open water. */
	float bestDist = 1e9f, dx = 0.0f, dz = 0.0f;
	for (int k = 0; k < 24; ++k) {
		const float a = k * F3DS_PI / 12.0f;
		for (float d = 6.0f; d < 220.0f && d < bestDist; d += 4.0f)
			if (gen_height_at(g, s->x + sinf(a) * d, s->z + cosf(a) * d) < -0.2f) {
				bestDist = d;
				dx = sinf(a);
				dz = cosf(a);
				break;
			}
	}
	if (bestDist >= 1e9f)
		return;
	const float yaw = atan2f(dx, dz);
	for (int k = -1; k <= 1; ++k) {
		const float ox = s->x + dz * k * 26.0f, oz = s->z - dx * k * 26.0f;
		/* Walk to this pier's own shoreline. */
		float d = 0.0f;
		while (d < 260.0f && gen_height_at(g, ox + dx * d, oz + dz * d) > 0.3f)
			d += 2.0f;
		if (d >= 260.0f || g->w->buildingCount >= WORLD_BUILDING_MAX)
			continue;
		const float half = 20.0f - 4.0f * (k != 0);
		const float x = ox + dx * (d + half - 3.0f), z = oz + dz * (d + half - 3.0f);
		if (gen_height_at(g, x + dx * half, z + dz * half) > -0.2f)
			continue;
		const Box b = make_box(x, z, 2.6f, half, yaw);
		if (clear_of_roads(g->w, &b, 1.0f) && clear_of_buildings(g->w, &b))
			push(g->w, F3DS_BUILD_PIER, x, z, 0.0f, 2.6f, half, 1.1f, yaw, k + 1);
	}
}

/* A random building suited to a place; d is the distance from its centre as a
 * share of its radius. Returns false to leave the plot empty. */
static bool along_street(F3DSGen *g, F3DSRng *r, const F3DSSite *s, const F3DSRoadSample *p,
	float nx, float nz, float d, int *shrines)
{
	int kind;
	float hx, hz, height;
	const float roll = rng_float(r);
	if (s->kind == F3DS_SITE_CITY) {
		if (rng_float(r) > 0.97f - 0.35f * d)
			return false;
		const float tower = d < 0.36f ? 0.46f : 0.0f;
		const float block = tower + (d < 0.66f ? 0.42f : 0.13f);
		const float shop = block + (d < 0.66f ? 0.30f : 0.17f);
		kind = roll < tower ? F3DS_BUILD_TOWER : roll < block ? F3DS_BUILD_BLOCK :
			roll < shop ? F3DS_BUILD_SHOP : F3DS_BUILD_HOUSE;
	} else if (s->kind == F3DS_SITE_VILLAGE) {
		if (rng_float(r) > 0.88f - 0.3f * d)
			return false;
		kind = roll < 0.75f ? F3DS_BUILD_HOUSE : roll < 0.93f ? F3DS_BUILD_SHOP :
			*shrines ? F3DS_BUILD_HOUSE : F3DS_BUILD_SHRINE;
	} else {
		if (rng_float(r) > 0.9f)
			return false;
		kind = roll < 0.45f ? F3DS_BUILD_WAREHOUSE : roll < 0.70f ? F3DS_BUILD_FACTORY :
			roll < 0.90f ? F3DS_BUILD_TANK : F3DS_BUILD_CHIMNEY;
	}
	switch (kind) {
	case F3DS_BUILD_TOWER:
		hx = rng_range(r, 6.5f, 8.5f);
		hz = rng_range(r, 6.5f, 8.5f);
		height = rng_range(r, 28.0f, 62.0f);
		break;
	case F3DS_BUILD_BLOCK:
		hx = rng_range(r, 6.0f, 8.5f);
		hz = rng_range(r, 5.0f, 7.0f);
		height = rng_range(r, 11.0f, 22.0f);
		break;
	case F3DS_BUILD_SHOP:
		hx = rng_range(r, 4.0f, 6.5f);
		hz = rng_range(r, 3.5f, 5.0f);
		height = rng_range(r, 4.0f, 7.5f);
		break;
	case F3DS_BUILD_WAREHOUSE:
		hx = rng_range(r, 9.0f, 13.0f);
		hz = rng_range(r, 7.0f, 10.0f);
		height = rng_range(r, 7.0f, 10.0f);
		break;
	case F3DS_BUILD_FACTORY:
		hx = rng_range(r, 10.0f, 13.0f);
		hz = rng_range(r, 8.0f, 11.0f);
		height = rng_range(r, 9.0f, 15.0f);
		break;
	case F3DS_BUILD_TANK:
		hx = hz = rng_range(r, 4.0f, 7.0f);
		height = rng_range(r, 7.0f, 13.0f);
		break;
	case F3DS_BUILD_CHIMNEY:
		hx = hz = 1.8f;
		height = rng_range(r, 26.0f, 40.0f);
		break;
	case F3DS_BUILD_SHRINE:
		hx = 5.0f;
		hz = 4.0f;
		height = 6.0f;
		break;
	default:
		hx = rng_range(r, 3.5f, 5.5f);
		hz = rng_range(r, 3.0f, 4.5f);
		height = rng_range(r, 4.2f, 6.8f);
		break;
	}
	const float off = road_half_width(p->type) + ROAD_GAP + 1.2f + hz + rng_range(r, 0.0f, 2.0f);
	const int variant = rng_int(r, 4);
	if (!try_place(g, kind, p->x + nx * off, p->z + nz * off, hx, hz, height, atan2f(-nx, -nz), variant))
		return false;
	if (kind == F3DS_BUILD_SHRINE) {
		(*shrines)++;
		torii(g, p->x, p->z);
	}
	return true;
}

static void settlement(F3DSGen *g, const F3DSSite *s, uint32_t seed)
{
	F3DSWorld *w = g->w;
	F3DSRng r = rng_make(seed);
	const float reach = s->radius * 0.93f;
	int shrines = 0;
	for (int k = 0; k < w->roadCount; ++k) {
		const F3DSRoad *road = &w->roads[k];
		if (road->type != F3DS_ROAD_MAIN && road->type != F3DS_ROAD_SECONDARY &&
		    road->type != F3DS_ROAD_STREET)
			continue;
		const int step = s->kind == F3DS_SITE_INDUSTRY ? 3 : 2;
		for (int i = 1; i + 1 < road->count; i += step) {
			const F3DSRoadSample *p = &w->samples[road->first + i];
			const float d = hypotf(p->x - s->x, p->z - s->z) / reach;
			if (d >= 1.0f || (p->flags & F3DS_SAMPLE_BRIDGE))
				continue;
			const float tx = p[1].x - p[-1].x, tz = p[1].z - p[-1].z;
			const float len = hypotf(tx, tz);
			if (len < 0.1f)
				continue;
			for (int side = -1; side <= 1; side += 2)
				along_street(g, &r, s, p, -tz / len * side, tx / len * side, d, &shrines);
		}
	}
}

/* Group the list by sector (stable), so that a sector is a contiguous run. */
static bool sort_by_sector(F3DSWorld *w)
{
	const int sectors = WORLD_SECTORS * WORLD_SECTORS;
	F3DSBuilding *sorted = malloc(sizeof(F3DSBuilding) * WORLD_BUILDING_MAX);
	if (!sorted)
		return false;
	memset(w->sectorBuildings, 0, sizeof(w->sectorBuildings));
	for (int pass = 0; pass < 2; ++pass) {
		for (int i = pass ? w->buildingCount - 1 : 0; i >= 0 && i < w->buildingCount; i += pass ? -1 : 1) {
			const F3DSBuilding *o = &w->buildings[i];
			const int x = gen_clampi((int)((o->x + WORLD_HALF) / WORLD_SECTOR), 0, WORLD_SECTORS - 1);
			const int z = gen_clampi((int)((o->z + WORLD_HALF) / WORLD_SECTOR), 0, WORLD_SECTORS - 1);
			uint16_t *slot = &w->sectorBuildings[z * WORLD_SECTORS + x];
			if (pass)
				sorted[--(*slot)] = *o;
			else
				(*slot)++;
		}
		if (!pass) {
			int total = 0;
			for (int c = 0; c < sectors; ++c) {
				total += w->sectorBuildings[c];
				w->sectorBuildings[c] = (uint16_t)total;
			}
			w->sectorBuildings[sectors] = (uint16_t)total;
		}
	}
	free(w->buildings);
	w->buildings = sorted;
	return true;
}

bool gen_buildings(F3DSGen *g)
{
	F3DSWorld *w = g->w;
	w->buildingCount = 0;
	/* Landmarks first: they must not lose their spot to an ordinary house. */
	for (int i = 0; i < w->siteCount; ++i) {
		const F3DSSite *s = &w->sites[i];
		if (s->kind == F3DS_SITE_LIGHTHOUSE)
			place_near(g, F3DS_BUILD_LIGHTHOUSE, s->x, s->z, 3.6f, 3.6f, 27.0f,
				g->lighthouseDirX, g->lighthouseDirZ);
		else if (s->kind == F3DS_SITE_VIEWPOINT) {
			place_near(g, F3DS_BUILD_SHRINE, s->x, s->z, 5.0f, 4.0f, 6.0f, 0.0f, 1.0f);
			torii(g, s->x, s->z);
		} else if (s->kind == F3DS_SITE_HARBOUR)
			piers(g, s);
	}
	/* The city is built last so that it cannot use up the whole budget. */
	for (int pass = 0; pass < 2; ++pass)
		for (int i = 0; i < w->siteCount; ++i) {
			const F3DSSite *s = &w->sites[i];
			const bool city = s->kind == F3DS_SITE_CITY;
			if (city != (pass == 1) || (!city && s->kind != F3DS_SITE_VILLAGE &&
			    s->kind != F3DS_SITE_INDUSTRY))
				continue;
			settlement(g, s, hash_seed(g->buildings, (uint32_t)i));
		}
	return sort_by_sector(w);
}
