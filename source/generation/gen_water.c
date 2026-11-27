/* Rivers, lakes and waterfalls follow the topology: a priority flood from the
 * sea gives every cell a downhill route and the level any hollow fills to. */
#include "gen.h"
#include <string.h>

#define CELLS (WORLD_BN * WORLD_BN)
#define RIVER_THRESHOLD 1100.0f  /* upstream cells (256 m2 each) */
#define LAKE_DEPTH 1.4f
#define LAKE_MIN_CELLS 28

typedef struct { float key; int32_t cell; } Item;
typedef struct { Item *items; int count; } Heap;

static void heap_push(Heap *h, float key, int32_t cell)
{
	int i = h->count++;
	while (i > 0) {
		const int p = (i - 1) / 2;
		if (h->items[p].key <= key)
			break;
		h->items[i] = h->items[p];
		i = p;
	}
	h->items[i].key = key;
	h->items[i].cell = cell;
}

static Item heap_pop(Heap *h)
{
	const Item top = h->items[0], last = h->items[--h->count];
	int i = 0;
	for (;;) {
		int c = i * 2 + 1;
		if (c >= h->count)
			break;
		if (c + 1 < h->count && h->items[c + 1].key < h->items[c].key)
			++c;
		if (h->items[c].key >= last.key)
			break;
		h->items[i] = h->items[c];
		i = c;
	}
	if (h->count)
		h->items[i] = last;
	return top;
}

static float bheight(const F3DSGen *g, int cell)
{
	return g->h[(cell / WORLD_BN) * 2 * WORLD_N + (cell % WORLD_BN) * 2];
}

/* Lower the ground along a stretch of river and mark its water. */
static void carve_river(F3DSGen *g, float ax, float az, float al, float bx, float bz, float bl, float half)
{
	const float reach = half + 22.0f;
	const int i0 = gen_clampi((int)((fminf(ax, bx) - reach + WORLD_HALF) / WORLD_CELL), 0, WORLD_N - 1);
	const int i1 = gen_clampi((int)((fmaxf(ax, bx) + reach + WORLD_HALF) / WORLD_CELL) + 1, 0, WORLD_N - 1);
	const int j0 = gen_clampi((int)((fminf(az, bz) - reach + WORLD_HALF) / WORLD_CELL), 0, WORLD_N - 1);
	const int j1 = gen_clampi((int)((fmaxf(az, bz) + reach + WORLD_HALF) / WORLD_CELL) + 1, 0, WORLD_N - 1);
	const float dx = bx - ax, dz = bz - az;
	const float len2 = dx * dx + dz * dz;
	for (int j = j0; j <= j1; ++j)
		for (int i = i0; i <= i1; ++i) {
			const float x = gen_x(i), z = gen_x(j);
			const float u = len2 > 0.0f ?
				clampf(((x - ax) * dx + (z - az) * dz) / len2, 0.0f, 1.0f) : 0.0f;
			const float ex = x - ax - dx * u, ez = z - az - dz * u;
			const float d = sqrtf(ex * ex + ez * ez);
			if (d > reach)
				continue;
			const float level = lerpf(al, bl, u);
			float target;
			if (d < half)
				target = level - 1.7f * (1.0f - 0.5f * (d / half) * (d / half));
			else
				target = level - 0.4f + (d - half) * 0.42f;
			float *h = &g->h[j * WORLD_N + i];
			if (target < *h)
				*h = fmaxf(target, -28.0f);
			if (d < half + 11.0f && (i & 1) == 0 && (j & 1) == 0) {
				const int c = (j / 2) * WORLD_BN + i / 2;
				if (!(g->wet[c] & GEN_WET_LAKE) && level > 0.3f &&
				    (g->wlevel[c] == WORLD_DRY || level < g->wlevel[c])) {
					g->wlevel[c] = level;
					g->wet[c] |= GEN_WET_RIVER;
				}
			}
		}
}

/* A basin dug into gentle high ground when the relief offers no lake. */
static void dig_lake(F3DSGen *g, uint32_t seed)
{
	F3DSRng r = rng_make(seed);
	float bestX = 0.0f, bestZ = 0.0f, bestScore = 1e9f, bestLevel = 0.0f;
	const float radius = 150.0f;
	for (int t = 0; t < 600; ++t) {
		const float x = rng_range(&r, -2400.0f, 2400.0f), z = rng_range(&r, -2400.0f, 2400.0f);
		const float h = gen_height_at(g, x, z);
		if (h < 18.0f || h > 150.0f)
			continue;
		float lo = h, hi = h;
		for (int k = 0; k < 16; ++k) {
			const float a = k * F3DS_PI / 8.0f;
			const float v = gen_height_at(g, x + cosf(a) * radius, z + sinf(a) * radius);
			lo = fminf(lo, v);
			hi = fmaxf(hi, v);
		}
		if (lo > 6.0f && hi - lo < bestScore) {
			bestScore = hi - lo;
			bestX = x;
			bestZ = z;
			bestLevel = lo;
		}
	}
	if (bestScore >= 1e9f)
		return;
	const float level = bestLevel - 0.5f;
	for (int j = 0; j < WORLD_N; ++j)
		for (int i = 0; i < WORLD_N; ++i) {
			const float dx = gen_x(i) - bestX, dz = gen_x(j) - bestZ;
			const float d = sqrtf(dx * dx + dz * dz) / radius;
			if (d >= 1.0f)
				continue;
			float *h = &g->h[j * WORLD_N + i];
			*h = fminf(*h, level + 1.5f - 6.5f * (1.0f - d * d));
			if ((i & 1) == 0 && (j & 1) == 0 && *h < level) {
				const int c = (j / 2) * WORLD_BN + i / 2;
				g->wlevel[c] = level;
				g->wet[c] = GEN_WET_LAKE;
			}
		}
	g->w->lakeCount = 1;
}

bool gen_water(F3DSGen *g)
{
	float *filled = malloc(sizeof(float) * CELLS);
	int32_t *parent = malloc(sizeof(int32_t) * CELLS);
	int32_t *order = malloc(sizeof(int32_t) * CELLS);
	float *flow = malloc(sizeof(float) * CELLS);
	Heap heap = { malloc(sizeof(Item) * CELLS), 0 };
	bool ok = filled && parent && order && flow && heap.items;
	if (!ok)
		goto done;

	for (int c = 0; c < CELLS; ++c) {
		g->wlevel[c] = WORLD_DRY;
		g->wet[c] = 0;
		parent[c] = -2; /* unvisited */
	}
	for (int c = 0; c < CELLS; ++c) {
		const int x = c % WORLD_BN, y = c / WORLD_BN;
		if (bheight(g, c) <= 0.0f || x == 0 || y == 0 || x == WORLD_BN - 1 || y == WORLD_BN - 1) {
			parent[c] = -1;
			filled[c] = fmaxf(bheight(g, c), 0.0f);
			heap_push(&heap, filled[c], c);
		}
	}
	static const int dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	static const int dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
	int visited = 0;
	while (heap.count) {
		const int c = heap_pop(&heap).cell;
		order[visited++] = c;
		const int x = c % WORLD_BN, y = c / WORLD_BN;
		/* Visit order of the neighbours varies so that drainage does not
		 * prefer one compass direction. */
		const int turn = (int)(hash_u32((uint32_t)c) & 7u);
		for (int k = 0; k < 8; ++k) {
			const int d = (k + turn) & 7;
			const int nx = x + dx[d], ny = y + dy[d];
			if (nx < 0 || ny < 0 || nx >= WORLD_BN || ny >= WORLD_BN)
				continue;
			const int n = ny * WORLD_BN + nx;
			if (parent[n] != -2)
				continue;
			parent[n] = c;
			filled[n] = fmaxf(bheight(g, n), filled[c] + 0.004f);
			heap_push(&heap, filled[n], n);
		}
	}
	gen_report(g, 43, "Rivières et lacs");

	for (int c = 0; c < CELLS; ++c)
		flow[c] = 1.0f;
	for (int k = visited - 1; k >= 0; --k) {
		const int c = order[k];
		if (parent[c] >= 0)
			flow[parent[c]] += flow[c];
	}

	/* Lakes: connected hollows deep and wide enough. "order" is reused as
	 * the flood stack and member list. */
	g->w->lakeCount = 0;
	for (int c = 0; c < CELLS; ++c)
		g->wet[c] = 0;
	for (int c = 0; c < CELLS; ++c) {
		if (g->wet[c] || parent[c] < 0 || filled[c] - bheight(g, c) < LAKE_DEPTH)
			continue;
		int n = 0, head = 0;
		order[n++] = c;
		g->wet[c] = 0x80;
		float level = filled[c];
		while (head < n) {
			const int q = order[head++];
			level = fminf(level, filled[q]);
			const int x = q % WORLD_BN, y = q / WORLD_BN;
			for (int d = 0; d < 4; ++d) {
				const int nx = x + dx[d], ny = y + dy[d];
				if (nx < 1 || ny < 1 || nx >= WORLD_BN - 1 || ny >= WORLD_BN - 1)
					continue;
				const int m = ny * WORLD_BN + nx;
				if (!g->wet[m] && parent[m] >= 0 && filled[m] - bheight(g, m) >= LAKE_DEPTH) {
					g->wet[m] = 0x80;
					order[n++] = m;
				}
			}
		}
		const bool lake = n >= LAKE_MIN_CELLS && level > 1.0f;
		if (lake)
			g->w->lakeCount++;
		for (int k = 0; k < n; ++k) {
			const int q = order[k];
			if (lake && bheight(g, q) < level) {
				g->wet[q] = 0x80 | GEN_WET_LAKE;
				g->wlevel[q] = level;
			}
		}
	}
	for (int c = 0; c < CELLS; ++c)
		g->wet[c] &= GEN_WET_LAKE;

	/* Rivers: carve every channel cell towards its downstream neighbour. */
	g->w->riverCells = 0;
	g->w->fallCount = 0;
	for (int c = 0; c < CELLS; ++c) {
		const int p = parent[c];
		if (p < 0 || flow[c] < RIVER_THRESHOLD || bheight(g, c) <= 0.0f)
			continue;
		if ((g->wet[c] & GEN_WET_LAKE) && (g->wet[p] & GEN_WET_LAKE))
			continue;
		g->w->riverCells++;
		const float half = clampf(7.0f + (flow[c] - RIVER_THRESHOLD) * 0.0016f, 7.0f, 15.0f);
		const float al = filled[c] - 0.7f;
		const float bl = parent[p] < 0 ? 0.0f : filled[p] - 0.7f;
		const float ax = gen_bx(c % WORLD_BN), az = gen_bx(c / WORLD_BN);
		const float bx = gen_bx(p % WORLD_BN), bz = gen_bx(p / WORLD_BN);
		carve_river(g, ax, az, al, bx, bz, bl, half);
		if (al - bl > 4.5f && g->w->fallCount < WORLD_FALL_MAX) {
			F3DSWaterfall *f = &g->w->falls[g->w->fallCount];
			bool close = false;
			for (int k = 0; k < g->w->fallCount && !close; ++k) {
				const float ex = g->w->falls[k].x - ax, ez = g->w->falls[k].z - az;
				close = ex * ex + ez * ez < 80.0f * 80.0f;
			}
			if (!close) {
				f->x = (ax + bx) * 0.5f;
				f->z = (az + bz) * 0.5f;
				f->top = al;
				f->bottom = bl;
				f->width = half * 1.4f;
				f->yaw = atan2f(bx - ax, bz - az);
				g->w->fallCount++;
			}
		}
	}
	if (g->w->lakeCount == 0)
		dig_lake(g, hash_seed(g->relief, 77));

	/* Water standing above the carved ground only. */
	for (int c = 0; c < CELLS; ++c)
		if (g->wlevel[c] != WORLD_DRY && bheight(g, c) > g->wlevel[c] + 1.5f) {
			g->wlevel[c] = WORLD_DRY;
			g->wet[c] = 0;
		}
done:
	free(filled);
	free(parent);
	free(order);
	free(flow);
	free(heap.items);
	return ok;
}

/* Distance from every cell to the sea (two-pass chamfer transform). */
bool gen_coast(F3DSGen *g)
{
	float *d = g->coast;
	for (int c = 0; c < CELLS; ++c)
		d[c] = bheight(g, c) <= 0.0f ? 0.0f : 1e9f;
	const float a = WORLD_BIOME_CELL, b = WORLD_BIOME_CELL * 1.41421356f;
	for (int pass = 0; pass < 2; ++pass) {
		const int step = pass ? -1 : 1;
		for (int y = pass ? WORLD_BN - 1 : 0; y >= 0 && y < WORLD_BN; y += step)
			for (int x = pass ? WORLD_BN - 1 : 0; x >= 0 && x < WORLD_BN; x += step) {
				float *v = &d[y * WORLD_BN + x];
				const int px = x - step, py = y - step, qx = x + step;
				if (px >= 0 && px < WORLD_BN)
					*v = fminf(*v, d[y * WORLD_BN + px] + a);
				if (py >= 0 && py < WORLD_BN) {
					*v = fminf(*v, d[py * WORLD_BN + x] + a);
					if (px >= 0 && px < WORLD_BN)
						*v = fminf(*v, d[py * WORLD_BN + px] + b);
					if (qx >= 0 && qx < WORLD_BN)
						*v = fminf(*v, d[py * WORLD_BN + qx] + b);
				}
			}
	}
	return true;
}
