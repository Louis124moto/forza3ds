/* Where the city, villages, industry and points of interest go, the ground
 * levelling under them, and the final biome map. */
#include "gen.h"
#include <stdio.h>
#include <string.h>

static float coast_at(const F3DSGen *g, float x, float z) { return g->coast[gen_bindex(x, z)]; }

/* Height range inside a disc, or a huge value when it touches water. */
static float roughness(const F3DSGen *g, float x, float z, float radius)
{
	float lo = 1e9f, hi = -1e9f;
	for (int j = -3; j <= 3; ++j)
		for (int i = -3; i <= 3; ++i) {
			if (i * i + j * j > 10)
				continue;
			const float px = x + i * radius / 3.0f, pz = z + j * radius / 3.0f;
			const float h = gen_height_at(g, px, pz);
			if (h < 0.8f || g->wlevel[gen_bindex(px, pz)] != WORLD_DRY)
				return 1e9f;
			lo = fminf(lo, h);
			hi = fmaxf(hi, h);
		}
	return hi - lo;
}

static bool spaced(const F3DSWorld *w, float x, float z, float gap)
{
	for (int i = 0; i < w->siteCount; ++i) {
		const float dx = w->sites[i].x - x, dz = w->sites[i].z - z;
		const float need = gap + w->sites[i].radius;
		if (dx * dx + dz * dz < need * need)
			return false;
	}
	return true;
}

static F3DSSite *add_site(F3DSWorld *w, int kind, const char *name, float x, float z, float radius,
	float height)
{
	if (w->siteCount >= WORLD_SITE_MAX)
		return NULL;
	F3DSSite *s = &w->sites[w->siteCount++];
	memset(s, 0, sizeof(*s));
	s->kind = kind;
	s->x = x;
	s->z = z;
	s->radius = radius;
	s->height = height;
	snprintf(s->name, sizeof(s->name), "%s", name);
	return s;
}

/* Average height of a disc, used as the level of a settlement. */
static float disc_level(const F3DSGen *g, float x, float z, float radius)
{
	float sum = 0.0f;
	int n = 0;
	for (int j = -4; j <= 4; ++j)
		for (int i = -4; i <= 4; ++i) {
			const float h = gen_height_at(g, x + i * radius * 0.15f, z + j * radius * 0.15f);
			if (h > 0.0f) {
				sum += h;
				++n;
			}
		}
	return n ? fmaxf(sum / n, 2.8f) : 2.8f;
}

/* Level the ground around a site, blending into the surroundings. */
static void flatten(F3DSGen *g, float cx, float cz, float radius, float level)
{
	const float reach = radius * 1.55f;
	const int i0 = gen_clampi((int)((cx - reach + WORLD_HALF) / WORLD_CELL), 0, WORLD_N - 1);
	const int i1 = gen_clampi((int)((cx + reach + WORLD_HALF) / WORLD_CELL) + 1, 0, WORLD_N - 1);
	const int j0 = gen_clampi((int)((cz - reach + WORLD_HALF) / WORLD_CELL), 0, WORLD_N - 1);
	const int j1 = gen_clampi((int)((cz + reach + WORLD_HALF) / WORLD_CELL) + 1, 0, WORLD_N - 1);
	for (int j = j0; j <= j1; ++j)
		for (int i = i0; i <= i1; ++i) {
			const float dx = gen_x(i) - cx, dz = gen_x(j) - cz;
			const float d = sqrtf(dx * dx + dz * dz);
			float *h = &g->h[j * WORLD_N + i];
			/* The coastline and rivers keep their shape. */
			if (d >= reach || *h <= 0.3f)
				continue;
			const int c = gen_bindex(gen_x(i), gen_x(j));
			if (g->wlevel[c] != WORLD_DRY)
				continue;
			const float t = 1.0f - smoothstepf((d - radius) / (reach - radius));
			*h = lerpf(*h, level, t);
		}
}

typedef float (*Score)(const F3DSGen *g, float x, float z, float range, int pass);

/* Best of many random candidates; thresholds relax on each new attempt, each
 * with its own sub-seed, without touching sites already placed. */
static bool place(F3DSGen *g, uint32_t seed, float radius, float gap, float maxRange, Score score,
	float *outX, float *outZ)
{
	for (int pass = 0; pass < 6; ++pass) {
		F3DSRng r = rng_make(hash_seed(seed, (uint32_t)pass));
		float best = -1e30f;
		for (int t = 0; t < 2600; ++t) {
			const float x = rng_range(&r, -3600.0f, 3600.0f), z = rng_range(&r, -3600.0f, 3600.0f);
			if (gen_height_at(g, x, z) < 1.0f || !spaced(g->w, x, z, gap))
				continue;
			const float range = roughness(g, x, z, radius);
			if (range > maxRange * (1.0f + 0.8f * pass))
				continue;
			const float s = score(g, x, z, range, pass) + rng_float(&r) * 6.0f;
			if (s > best) {
				best = s;
				*outX = x;
				*outZ = z;
			}
		}
		if (best > -1e29f) {
			g->w->siteRetries += pass;
			return true;
		}
	}
	return false;
}

static float score_city(const F3DSGen *g, float x, float z, float range, int pass)
{
	const float c = coast_at(g, x, z);
	if (pass < 4 && (c < 260.0f || c > 1100.0f))
		return -1e30f;
	return -range - 0.03f * fabsf(c - 480.0f);
}

static float score_industry(const F3DSGen *g, float x, float z, float range, int pass)
{
	const F3DSSite *city = &g->w->sites[0];
	const float d = hypotf(x - city->x, z - city->z);
	if (pass < 3 && (d > 2300.0f || coast_at(g, x, z) > 700.0f))
		return -1e30f;
	return -range - 0.012f * fabsf(d - 1200.0f) - 0.02f * coast_at(g, x, z);
}

static float score_village(const F3DSGen *g, float x, float z, float range, int pass)
{
	(void)pass;
	(void)g;
	/* Spread over the whole island rather than only on the flattest ground. */
	return -0.6f * range + 0.004f * hypotf(x, z);
}

static float score_viewpoint(const F3DSGen *g, float x, float z, float range, int pass)
{
	const float h = gen_height_at(g, x, z);
	if (pass < 4 && (h < 130.0f || h > 330.0f))
		return -1e30f;
	return h * 0.4f - range;
}

static float score_lighthouse(const F3DSGen *g, float x, float z, float range, int pass)
{
	const float c = coast_at(g, x, z);
	if (c > 60.0f + 40.0f * pass)
		return -1e30f;
	return (x * g->lighthouseDirX + z * g->lighthouseDirZ) * 0.1f - 0.2f * range;
}

static float score_harbour(const F3DSGen *g, float x, float z, float range, int pass)
{
	const F3DSSite *city = &g->w->sites[0];
	if (coast_at(g, x, z) > 50.0f + 30.0f * pass || gen_height_at(g, x, z) > 14.0f)
		return -1e30f;
	return -0.1f * hypotf(x - city->x, z - city->z) - 0.2f * range;
}

static float score_beach(const F3DSGen *g, float x, float z, float range, int pass)
{
	if (coast_at(g, x, z) > 40.0f + 30.0f * pass || gen_height_at(g, x, z) > 5.0f)
		return -1e30f;
	return -range * 4.0f;
}

bool gen_sites(F3DSGen *g)
{
	F3DSWorld *w = g->w;
	w->siteCount = 0;
	w->siteRetries = 0;
	float x, z;
	const uint32_t seed = g->cities;
#define PLACE(salt, radius, gap, range, score) \
	place(g, hash_seed(seed, salt), radius, gap, range, score, &x, &z)

	/* The city must exist: fall back to the best land spot if need be. */
	if (!PLACE(1, 380.0f, 0.0f, 26.0f, score_city)) {
		x = 0.0f;
		z = 0.0f;
	}
	add_site(w, F3DS_SITE_CITY, "Minato", x, z, 430.0f, disc_level(g, x, z, 300.0f));
	flatten(g, x, z, 430.0f, w->sites[0].height);

	if (PLACE(2, 230.0f, 420.0f, 24.0f, score_industry)) {
		add_site(w, F3DS_SITE_INDUSTRY, "Zone Kojo", x, z, 260.0f, disc_level(g, x, z, 180.0f));
		flatten(g, x, z, 260.0f, w->sites[w->siteCount - 1].height);
	}
	static const char *villages[6] = {
		"Sakura-mura", "Takedani", "Hoshi-mura", "Kawabe", "Yamanoue", "Shiozaki"
	};
	for (int i = 0; i < 6; ++i)
		if (PLACE(10 + (uint32_t)i, 130.0f, 720.0f, 22.0f, score_village)) {
			add_site(w, F3DS_SITE_VILLAGE, villages[i], x, z, 165.0f, disc_level(g, x, z, 110.0f));
			flatten(g, x, z, 165.0f, w->sites[w->siteCount - 1].height);
		}
	if (PLACE(30, 40.0f, 150.0f, 28.0f, score_viewpoint)) {
		add_site(w, F3DS_SITE_VIEWPOINT, "Belvédère Tengu", x, z, 55.0f, disc_level(g, x, z, 40.0f));
		flatten(g, x, z, 55.0f, w->sites[w->siteCount - 1].height);
	}
	if (PLACE(31, 24.0f, 60.0f, 20.0f, score_lighthouse)) {
		add_site(w, F3DS_SITE_LIGHTHOUSE, "Phare Misaki", x, z, 30.0f, disc_level(g, x, z, 20.0f));
		flatten(g, x, z, 30.0f, w->sites[w->siteCount - 1].height);
	}
	/* The harbour sits on the shore, possibly just outside the city. */
	{
		const F3DSSite city = w->sites[0];
		w->sites[0].radius = 0.0f;
		const bool found = PLACE(32, 30.0f, 0.0f, 16.0f, score_harbour);
		w->sites[0].radius = city.radius;
		if (found)
			add_site(w, F3DS_SITE_HARBOUR, "Port de Minato", x, z, 45.0f, gen_height_at(g, x, z));
	}
	if (PLACE(33, 30.0f, 400.0f, 8.0f, score_beach))
		add_site(w, F3DS_SITE_BEACH, "Plage Nagisa", x, z, 60.0f, gen_height_at(g, x, z));
#undef PLACE
	/* The largest lake: centre of mass of its first connected patch. */
	int bestCell = -1, bestCount = 0;
	for (int c = 0; c < WORLD_BN * WORLD_BN; c += 3) {
		if (!(g->wet[c] & GEN_WET_LAKE))
			continue;
		int n = 0;
		for (int dy = -8; dy <= 8; dy += 2)
			for (int dx = -8; dx <= 8; dx += 2) {
				const int q = c + dy * WORLD_BN + dx;
				if (q >= 0 && q < WORLD_BN * WORLD_BN && (g->wet[q] & GEN_WET_LAKE))
					++n;
			}
		if (n > bestCount) {
			bestCount = n;
			bestCell = c;
		}
	}
	if (bestCell >= 0)
		add_site(w, F3DS_SITE_LAKE, "Lac Kagami", gen_bx(bestCell % WORLD_BN),
			gen_bx(bestCell / WORLD_BN), 40.0f, g->wlevel[bestCell]);
	return true;
}

/* Biome from altitude, slope, distance to the sea and two climate fields. */
bool gen_biomes(F3DSGen *g)
{
	F3DSWorld *w = g->w;
	const uint32_t seed = g->biome;
	for (int j = 0; j < WORLD_BN; ++j)
		for (int i = 0; i < WORLD_BN; ++i) {
			const int c = j * WORLD_BN + i;
			const float x = gen_bx(i), z = gen_bx(j);
			const float h = gen_height_at(g, x, z);
			const float sx = gen_height_at(g, x + 8.0f, z) - gen_height_at(g, x - 8.0f, z);
			const float sz = gen_height_at(g, x, z + 8.0f) - gen_height_at(g, x, z - 8.0f);
			const float slope = sqrtf(sx * sx + sz * sz) / 16.0f;
			const float coast = g->coast[c];
			const float moist = noise_fbm(seed + 1u, x / 760.0f, z / 760.0f, 3);
			const float bamboo = noise_fbm(seed + 2u, x / 430.0f, z / 430.0f, 3);
			const float jitter = noise2(seed + 3u, x / 90.0f, z / 90.0f);
			int b;
			if (h <= 0.0f)
				b = F3DS_BIOME_OCEAN;
			else if (h < 3.6f + jitter && coast < 80.0f && slope < 0.22f)
				b = F3DS_BIOME_BEACH;
			else if (h > 390.0f + 30.0f * jitter)
				b = F3DS_BIOME_SNOW;
			else if (slope > 0.74f + 0.1f * jitter || h > 330.0f)
				b = F3DS_BIOME_ROCK;
			else if (coast < 110.0f + 40.0f * jitter)
				b = F3DS_BIOME_COAST;
			else if (h > 190.0f + 30.0f * jitter)
				b = F3DS_BIOME_HIGHLAND;
			else if (bamboo > 0.20f && h > 12.0f && h < 170.0f)
				b = F3DS_BIOME_BAMBOO;
			else if (moist + h * 0.0011f + 0.03f * jitter > 0.08f)
				b = F3DS_BIOME_FOREST;
			else if (slope < 0.11f && h < 90.0f && moist < -0.06f)
				b = F3DS_BIOME_FARM;
			else
				b = F3DS_BIOME_PLAIN;
			if (g->wlevel[c] != WORLD_DRY && h < g->wlevel[c] + 0.2f)
				b = (g->wet[c] & GEN_WET_LAKE) ? F3DS_BIOME_LAKE : F3DS_BIOME_RIVER;
			w->biome[c] = (uint8_t)b;
		}
	for (int s = 0; s < w->siteCount; ++s) {
		const F3DSSite *site = &w->sites[s];
		int b;
		float radius = site->radius;
		switch (site->kind) {
		case F3DS_SITE_CITY: b = F3DS_BIOME_CITY; radius *= 0.93f; break;
		case F3DS_SITE_INDUSTRY: b = F3DS_BIOME_INDUSTRY; break;
		case F3DS_SITE_VILLAGE: b = F3DS_BIOME_VILLAGE; radius *= 0.95f; break;
		default: continue;
		}
		const int i0 = gen_clampi((int)((site->x - radius + WORLD_HALF) / WORLD_BIOME_CELL), 0, WORLD_BN - 1);
		const int i1 = gen_clampi((int)((site->x + radius + WORLD_HALF) / WORLD_BIOME_CELL) + 1, 0, WORLD_BN - 1);
		const int j0 = gen_clampi((int)((site->z - radius + WORLD_HALF) / WORLD_BIOME_CELL), 0, WORLD_BN - 1);
		const int j1 = gen_clampi((int)((site->z + radius + WORLD_HALF) / WORLD_BIOME_CELL) + 1, 0, WORLD_BN - 1);
		for (int j = j0; j <= j1; ++j)
			for (int i = i0; i <= i1; ++i) {
				const int c = j * WORLD_BN + i;
				const float dx = gen_bx(i) - site->x, dz = gen_bx(j) - site->z;
				const float edge = radius * (1.0f + 0.10f * noise2(seed + 4u,
					gen_bx(i) / 70.0f, gen_bx(j) / 70.0f));
				const int old = w->biome[c];
				if (dx * dx + dz * dz < edge * edge && old != F3DS_BIOME_OCEAN &&
				    old != F3DS_BIOME_LAKE && old != F3DS_BIOME_RIVER)
					w->biome[c] = (uint8_t)b;
			}
	}
	for (int c = 0; c < WORLD_BN * WORLD_BN; ++c) {
		const float level = g->wlevel[c];
		w->water[c] = level == WORLD_DRY ? WORLD_NO_WATER
			: (int16_t)lroundf(clampf(level, -900.0f, 900.0f) / WORLD_HEIGHT_UNIT);
	}
	return true;
}
