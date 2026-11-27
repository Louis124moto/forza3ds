/* The road network: an A* search on a 32 m grid follows the natural relief,
 * the path is rounded and resampled every 8 m, its profile is limited in
 * grade, and the ground is then cut and filled to meet it. A route that cannot
 * respect its limits is searched again with another sub-seed. */
#include "gen.h"
#include <string.h>

#define RN 257
#define RCELL 32.0f
#define NODES (RN * RN)
#define PIECE_MAX 1600
#define ATTEMPTS 4

typedef struct { float key; int32_t node; } Item;

typedef struct {
	F3DSGen *g;
	float *height;     /* RN^2 */
	uint8_t *blocked;  /* sea or lake */
	uint8_t *river;
	int32_t *road;     /* nearest road sample of a cell, -1 */
	float *cost;
	int32_t *parent;
	uint8_t *closed;
	Item *heap;
	int heapCount, heapCapacity;
	int32_t *path;
	float *px, *pz, *py, *sx, *sz;
} Router;

static const float grade_limit[F3DS_ROAD_TYPE_COUNT] = { 0.105f, 0.135f, 0.08f, 0.19f, 0.23f };
static const float radius_limit[F3DS_ROAD_TYPE_COUNT] = { 30.0f, 20.0f, 14.0f, 14.0f, 12.0f };

float road_half_width(int type)
{
	static const float half[F3DS_ROAD_TYPE_COUNT] = { 5.0f, 4.2f, 4.2f, 3.0f, 2.4f };
	return half[(unsigned)type < F3DS_ROAD_TYPE_COUNT ? type : 0];
}

static float node_x(int n) { return (float)(n % RN) * RCELL - WORLD_HALF; }
static float node_z(int n) { return (float)(n / RN) * RCELL - WORLD_HALF; }

static int node_at(float x, float z)
{
	const int i = gen_clampi((int)lroundf((x + WORLD_HALF) / RCELL), 0, RN - 1);
	const int j = gen_clampi((int)lroundf((z + WORLD_HALF) / RCELL), 0, RN - 1);
	return j * RN + i;
}

static bool heap_push(Router *r, float key, int32_t node)
{
	if (r->heapCount >= r->heapCapacity)
		return false;
	int i = r->heapCount++;
	while (i > 0) {
		const int p = (i - 1) / 2;
		if (r->heap[p].key <= key)
			break;
		r->heap[i] = r->heap[p];
		i = p;
	}
	r->heap[i].key = key;
	r->heap[i].node = node;
	return true;
}

static int32_t heap_pop(Router *r)
{
	const int32_t top = r->heap[0].node;
	const Item last = r->heap[--r->heapCount];
	int i = 0;
	for (;;) {
		int c = i * 2 + 1;
		if (c >= r->heapCount)
			break;
		if (c + 1 < r->heapCount && r->heap[c + 1].key < r->heap[c].key)
			++c;
		if (r->heap[c].key >= last.key)
			break;
		r->heap[i] = r->heap[c];
		i = c;
	}
	if (r->heapCount)
		r->heap[i] = last;
	return top;
}

/* Cheapest path between two nodes; returns its length in nodes, or 0. */
static int search(Router *r, int start, int goal, int type, uint32_t seed, int attempt)
{
	static const int step[16][2] = {
		{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 },
		{ 2, 1 }, { 1, 2 }, { -1, 2 }, { -2, 1 }, { -2, -1 }, { -1, -2 }, { 1, -2 }, { 2, -1 }
	};
	const float limit = grade_limit[type];
	const float gx = node_x(goal), gz = node_z(goal);
	for (int n = 0; n < NODES; ++n) {
		r->cost[n] = 1e30f;
		r->closed[n] = 0;
	}
	r->heapCount = 0;
	r->cost[start] = 0.0f;
	r->parent[start] = -1;
	heap_push(r, 0.0f, start);
	while (r->heapCount) {
		const int n = heap_pop(r);
		if (r->closed[n])
			continue;
		r->closed[n] = 1;
		if (n == goal)
			break;
		const int x = n % RN, z = n / RN;
		for (int k = 0; k < 16; ++k) {
			const int nx = x + step[k][0], nz = z + step[k][1];
			if (nx < 1 || nz < 1 || nx >= RN - 1 || nz >= RN - 1)
				continue;
			const int m = nz * RN + nx;
			if (r->closed[m] || (r->blocked[m] && m != goal))
				continue;
			if (k >= 8) {
				/* Long moves must not jump over blocked ground. */
				const int a = (z + step[k][1] / 2) * RN + x + step[k][0] / 2;
				const int b = (z + (step[k][1] - step[k][1] / 2)) * RN + x + (step[k][0] - step[k][0] / 2);
				if (r->blocked[a] || r->blocked[b])
					continue;
			}
			const float len = RCELL * (k < 4 ? 1.0f : k < 8 ? 1.41421356f : 2.23606798f);
			const float grade = fabsf(r->height[m] - r->height[n]) / len;
			const float q = grade / limit;
			float f = 1.0f + 0.45f * (noise2(seed, node_x(m) / 230.0f, node_z(m) / 230.0f) + 0.7f);
			f += (q * q * 2.2f + (q > 1.0f ? (q - 1.0f) * (22.0f + 20.0f * attempt) : 0.0f));
			if (r->river[m])
				f += 5.0f;
			if (r->height[m] < 2.2f)
				f += 0.8f;
			if (r->road[m] >= 0)
				f *= 0.34f;
			const float cost = r->cost[n] + len * f;
			if (cost < r->cost[m]) {
				r->cost[m] = cost;
				r->parent[m] = n;
				const float h = 0.34f * hypotf(node_x(m) - gx, node_z(m) - gz);
				if (!heap_push(r, cost + h, m))
					return 0;
			}
		}
	}
	if (!r->closed[goal])
		return 0;
	int count = 0;
	for (int n = goal; n >= 0 && count < PIECE_MAX; n = r->parent[n])
		r->path[count++] = n;
	/* Start first. */
	for (int i = 0; i < count / 2; ++i) {
		const int32_t t = r->path[i];
		r->path[i] = r->path[count - 1 - i];
		r->path[count - 1 - i] = t;
	}
	return count;
}

/* Corner cutting (Chaikin), keeping both ends. */
static int round_off(float *x, float *z, float *tx, float *tz, int n)
{
	for (int pass = 0; pass < 2 && n >= 3 && n * 2 < PIECE_MAX; ++pass) {
		int m = 0;
		tx[m] = x[0];
		tz[m++] = z[0];
		for (int i = 0; i + 1 < n; ++i) {
			if (i > 0) {
				tx[m] = lerpf(x[i], x[i + 1], 0.25f);
				tz[m++] = lerpf(z[i], z[i + 1], 0.25f);
			}
			if (i + 2 < n) {
				tx[m] = lerpf(x[i], x[i + 1], 0.75f);
				tz[m++] = lerpf(z[i], z[i + 1], 0.75f);
			}
		}
		tx[m] = x[n - 1];
		tz[m++] = z[n - 1];
		memcpy(x, tx, sizeof(float) * (size_t)m);
		memcpy(z, tz, sizeof(float) * (size_t)m);
		n = m;
	}
	return n;
}

/* Points every WORLD_ROAD_SPACING metres (last gap between 0.5 and 1.5). */
static int resample(const float *x, const float *z, int n, float *ox, float *oz)
{
	float total = 0.0f;
	for (int i = 0; i + 1 < n; ++i)
		total += hypotf(x[i + 1] - x[i], z[i + 1] - z[i]);
	int segments = (int)lroundf(total / WORLD_ROAD_SPACING);
	if (segments < 1)
		segments = 1;
	if (segments >= PIECE_MAX)
		segments = PIECE_MAX - 1;
	const float spacing = total / segments;
	float walked = 0.0f;
	int out = 0;
	for (int i = 0; i + 1 < n; ++i) {
		const float len = hypotf(x[i + 1] - x[i], z[i + 1] - z[i]);
		while (out < segments && spacing * out <= walked + len) {
			const float u = len > 0.0f ? clampf((spacing * out - walked) / len, 0.0f, 1.0f) : 0.0f;
			ox[out] = lerpf(x[i], x[i + 1], u);
			oz[out++] = lerpf(z[i], z[i + 1], u);
		}
		walked += len;
	}
	ox[out] = x[n - 1];
	oz[out++] = z[n - 1];
	return out;
}

static float turn_radius(const float *x, const float *z, int i)
{
	const float ax = x[i] - x[i - 1], az = z[i] - z[i - 1];
	const float bx = x[i + 1] - x[i], bz = z[i + 1] - z[i];
	const float cx = x[i + 1] - x[i - 1], cz = z[i + 1] - z[i - 1];
	const float cross = fabsf(ax * bz - az * bx);
	if (cross < 1e-4f)
		return ax * bx + az * bz >= 0.0f ? 1e6f : 0.0f;
	return hypotf(ax, az) * hypotf(bx, bz) * hypotf(cx, cz) / (2.0f * cross);
}

/* Relax only the corners tighter than the limit. */
static int smooth_bends(Router *r, int n, float minRadius)
{
	for (int round = 0; round < 2; ++round) {
		for (int pass = 0; pass < 220; ++pass) {
			bool tight = false;
			for (int i = 1; i + 1 < n; ++i)
				if (turn_radius(r->px, r->pz, i) < minRadius) {
					for (int k = i - 1; k <= i + 1; ++k) {
						if (k <= 0 || k >= n - 1)
							continue;
						r->px[k] = 0.5f * r->px[k] + 0.25f * (r->px[k - 1] + r->px[k + 1]);
						r->pz[k] = 0.5f * r->pz[k] + 0.25f * (r->pz[k - 1] + r->pz[k + 1]);
					}
					tight = true;
				}
			if (!tight)
				break;
		}
		n = resample(r->px, r->pz, n, r->sx, r->sz);
		memcpy(r->px, r->sx, sizeof(float) * (size_t)n);
		memcpy(r->pz, r->sz, sizeof(float) * (size_t)n);
	}
	return n;
}

/* Highest water level on the corners of the surrounding biome cell, the same
 * rule as world_water_level(). */
static float water_near(const F3DSGen *g, float x, float z)
{
	const int i = gen_clampi((int)((x + WORLD_HALF) / WORLD_BIOME_CELL), 0, WORLD_BN - 2);
	const int j = gen_clampi((int)((z + WORLD_HALF) / WORLD_BIOME_CELL), 0, WORLD_BN - 2);
	const float *p = &g->wlevel[j * WORLD_BN + i];
	return fmaxf(fmaxf(p[0], p[1]), fmaxf(p[WORLD_BN], p[WORLD_BN + 1]));
}

/* Height of a road point before levelling: the ground, or over the water. */
static float natural_height(const F3DSGen *g, float x, float z, float *minimum)
{
	const float h = gen_height_at(g, x, z);
	const float water = water_near(g, x, z);
	*minimum = water != WORLD_DRY && h < water + 0.6f ? water + 2.0f : 1.1f;
	return fmaxf(h, *minimum);
}

/* Fit a drivable profile. hasA/hasB pin the ends to existing heights.
 * Returns the cut-and-fill badness, or a negative value if impossible. */
static float profile(Router *r, int n, float limit, bool hasA, float ya, bool hasB, float yb)
{
	static float natural[PIECE_MAX], minimum[PIECE_MAX], s[PIECE_MAX];
	s[0] = 0.0f;
	for (int i = 0; i < n; ++i) {
		if (i)
			s[i] = s[i - 1] + hypotf(r->px[i] - r->px[i - 1], r->pz[i] - r->pz[i - 1]);
		natural[i] = natural_height(r->g, r->px[i], r->pz[i], &minimum[i]);
		r->py[i] = natural[i];
	}
	const float length = s[n - 1];
	if (hasA) r->py[0] = ya;
	if (hasB) r->py[n - 1] = yb;
	if (hasA && hasB && fabsf(yb - ya) > limit * length * 0.995f)
		return -1.0f;
	for (int pass = 0; pass < 40; ++pass)
		for (int i = 1; i + 1 < n; ++i)
			r->py[i] = fmaxf(0.5f * r->py[i] + 0.25f * (r->py[i - 1] + r->py[i + 1]), minimum[i]);
	/* Keep every point inside the cone reachable from the pinned ends, then
	 * limit the grade from one end to the other. */
	for (int i = 0; i < n; ++i) {
		float lo = -1e9f, hi = 1e9f;
		if (hasA) { lo = fmaxf(lo, ya - limit * s[i]); hi = fminf(hi, ya + limit * s[i]); }
		if (hasB) {
			lo = fmaxf(lo, yb - limit * (length - s[i]));
			hi = fminf(hi, yb + limit * (length - s[i]));
		}
		if (lo <= hi)
			r->py[i] = clampf(r->py[i], lo, hi);
	}
	if (hasB && !hasA)
		for (int i = n - 2; i >= 0; --i) {
			const float d = limit * (s[i + 1] - s[i]);
			r->py[i] = clampf(r->py[i], r->py[i + 1] - d, r->py[i + 1] + d);
		}
	else
		for (int i = 1; i < n; ++i) {
			const float d = limit * (s[i] - s[i - 1]);
			r->py[i] = clampf(r->py[i], r->py[i - 1] - d, r->py[i - 1] + d);
		}
	if (hasA) r->py[0] = ya;
	if (hasB) r->py[n - 1] = yb;
	float worst = 0.0f;
	for (int i = 0; i < n; ++i) {
		if (i && fabsf(r->py[i] - r->py[i - 1]) > limit * (s[i] - s[i - 1]) * 1.02f + 0.02f)
			return -1.0f;
		worst = fmaxf(worst, fabsf(r->py[i] - natural[i]));
	}
	return worst;
}

/* Remember which routing cells a stretch of road serves. */
static void mark(Router *r, int sample)
{
	const F3DSRoadSample *s = &r->g->w->samples[sample];
	const int n = node_at(s->x, s->z);
	if (r->road[n] < 0) {
		r->road[n] = sample;
		return;
	}
	const F3DSRoadSample *old = &r->g->w->samples[r->road[n]];
	if (hypotf(s->x - node_x(n), s->z - node_z(n)) < hypotf(old->x - node_x(n), old->z - node_z(n)))
		r->road[n] = sample;
}

static bool append_road(Router *r, const float *x, const float *z, const float *y, int n, int type)
{
	F3DSWorld *w = r->g->w;
	if (n < 2 || w->roadCount >= WORLD_ROAD_MAX || w->sampleCount + n > WORLD_ROAD_SAMPLE_MAX)
		return false;
	F3DSRoad *road = &w->roads[w->roadCount++];
	road->first = (uint16_t)w->sampleCount;
	road->count = (uint16_t)n;
	road->type = (uint8_t)type;
	for (int i = 0; i < n; ++i) {
		F3DSRoadSample *s = &w->samples[w->sampleCount];
		memset(s, 0, sizeof(*s));
		s->x = x[i];
		s->z = z[i];
		s->y = y[i];
		s->type = (uint8_t)type;
		float minimum;
		natural_height(r->g, x[i], z[i], &minimum);
		if (minimum > 1.2f)
			s->flags |= F3DS_SAMPLE_BRIDGE;
		if (i == n - 1)
			s->flags |= F3DS_SAMPLE_LAST;
		mark(r, w->sampleCount);
		w->sampleCount++;
		if (i)
			w->roadLength += hypotf(x[i] - x[i - 1], z[i] - z[i - 1]);
	}
	return true;
}

/* The rounded, evenly spaced line along path nodes first..last. */
static int geometry(Router *r, int first, int last, int ja, int jb)
{
	const F3DSWorld *w = r->g->w;
	int n = 0;
	for (int i = first; i <= last; ++i, ++n) {
		r->px[n] = node_x(r->path[i]);
		r->pz[n] = node_z(r->path[i]);
	}
	if (ja >= 0) { r->px[0] = w->samples[ja].x; r->pz[0] = w->samples[ja].z; }
	if (jb >= 0) { r->px[n - 1] = w->samples[jb].x; r->pz[n - 1] = w->samples[jb].z; }
	n = round_off(r->px, r->pz, r->sx, r->sz, n);
	n = resample(r->px, r->pz, n, r->sx, r->sz);
	memcpy(r->px, r->sx, sizeof(float) * (size_t)n);
	memcpy(r->pz, r->sz, sizeof(float) * (size_t)n);
	return n;
}

/* Build the part of a found path that is not already a road. "first" and
 * "last" index the path; ends that touch a road are joined to its samples. */
static bool build_piece(Router *r, int first, int last, int type, bool final)
{
	F3DSWorld *w = r->g->w;
	const int ja = r->road[r->path[first]], jb = r->road[r->path[last]];
	if (last <= first)
		return true;
	int n;
	n = geometry(r, first, last, ja, jb);
	n = smooth_bends(r, n, radius_limit[type]);

	const float ya = ja >= 0 ? w->samples[ja].y : 0.0f, yb = jb >= 0 ? w->samples[jb].y : 0.0f;
	float limit = grade_limit[type];
	float badness = profile(r, n, limit, ja >= 0, ya, jb >= 0, yb);
	if (!final && (badness < 0.0f || badness > 26.0f))
		return false;
	if (badness < 0.0f) {
		/* Last resort: keep the bends of the path, which make it longer,
		 * and allow the gentlest grade that joins both ends. */
		n = geometry(r, first, last, ja, jb);
		float length = 0.0f;
		for (int i = 1; i < n; ++i)
			length += hypotf(r->px[i] - r->px[i - 1], r->pz[i] - r->pz[i - 1]);
		limit = fmaxf(limit, fabsf(yb - ya) / fmaxf(length, 1.0f) * 1.1f);
		if (limit > 0.28f || profile(r, n, limit, ja >= 0, ya, jb >= 0, yb) < 0.0f) {
			w->routeFailures++;
			return true;
		}
	}
	append_road(r, r->px, r->pz, r->py, n, type);
	return true;
}

/* Whether the roads serving two neighbouring cells already meet: the same
 * road further along, or two roads touching at the same height. */
static bool joined(const Router *r, int a, int b)
{
	const F3DSWorld *w = r->g->w;
	const int lo = a < b ? a : b, hi = a < b ? b : a;
	if (hi - lo <= 16) {
		int s = lo;
		while (s < hi && !(w->samples[s].flags & F3DS_SAMPLE_LAST))
			++s;
		if (s == hi)
			return true;
	}
	const F3DSRoadSample *p = &w->samples[a], *q = &w->samples[b];
	return hypotf(p->x - q->x, p->z - q->z) < 20.0f && fabsf(p->y - q->y) < 1.2f;
}

/* Whether a new stretch of road between two path nodes can reach from one
 * pinned height to the other. */
static bool reachable(const Router *r, int i, int j, int type)
{
	const int a = r->road[r->path[i]], b = r->road[r->path[j]];
	if (a < 0 || b < 0)
		return true;
	float length = 0.0f;
	for (int k = i; k < j; ++k)
		length += hypotf(node_x(r->path[k + 1]) - node_x(r->path[k]),
			node_z(r->path[k + 1]) - node_z(r->path[k]));
	const float dy = r->g->w->samples[b].y - r->g->w->samples[a].y;
	return fabsf(dy) <= grade_limit[type] * 0.9f * length;
}

static bool lay_route(Router *r, int from, int to, int type, uint32_t index)
{
	F3DSWorld *w = r->g->w;
	for (int attempt = 0; attempt < ATTEMPTS; ++attempt) {
		const int count = search(r, from, to, type,
			hash_seed(r->g->roads, index * 8u + (uint32_t)attempt), attempt);
		if (count < 2) {
			w->routeFailures++;
			return false;
		}
		const int roadsBefore = w->roadCount, samplesBefore = w->sampleCount;
		const float lengthBefore = w->roadLength;
		bool ok = true;
		int i = 0;
		while (ok && i + 1 < count) {
			/* Skip stretches already served by a road. */
			const int a = r->road[r->path[i]], b = r->road[r->path[i + 1]];
			if (a >= 0 && b >= 0 && joined(r, a, b)) {
				++i;
				continue;
			}
			/* Branch off, and come back at the first road that is within
			 * reach of a drivable slope. */
			int j = i + 1;
			while (j + 1 < count && (r->road[r->path[j]] < 0 || !reachable(r, i, j, type)))
				++j;
			/* The far end is a road too steeply above or below: start the
			 * climb earlier along the path. */
			int from = i;
			while (from > 0 && !reachable(r, from, j, type))
				--from;
			ok = build_piece(r, from, j, type, attempt == ATTEMPTS - 1);
			i = j;
		}
		if (ok)
			return true;
		/* Local retry: remove only what this route added. */
		w->routeRetries++;
		w->roadCount = roadsBefore;
		w->sampleCount = samplesBefore;
		w->roadLength = lengthBefore;
		for (int n = 0; n < NODES; ++n)
			if (r->road[n] >= samplesBefore)
				r->road[n] = -1;
	}
	return false;
}

/* A rectangular grid of level streets inside a settlement. */
static void streets(Router *r, const F3DSSite *s, float spacing, int lines, float angle)
{
	const float c = cosf(angle), si = sinf(angle);
	const float reach = s->radius * 0.80f;
	for (int axis = 0; axis < 2; ++axis)
		for (int k = -lines; k <= lines; ++k) {
			const float v = (float)k * spacing + (axis ? 0.0f : spacing * 0.18f);
			if (fabsf(v) >= reach - 20.0f)
				continue;
			const float half = sqrtf(reach * reach - v * v);
			const int segments = (int)(2.0f * half / WORLD_ROAD_SPACING);
			if (segments < 4)
				continue;
			int n = 0;
			for (int i = 0; i <= segments && n < PIECE_MAX; ++i) {
				const float u = -half + 2.0f * half * i / segments;
				const float lx = axis ? v : u, lz = axis ? u : v;
				const float x = s->x + lx * c - lz * si, z = s->z + lx * si + lz * c;
				/* Streets stop at water and at the shore. */
				if (gen_height_at(r->g, x, z) < 1.2f || r->g->wlevel[gen_bindex(x, z)] != WORLD_DRY) {
					if (n >= 5)
						append_road(r, r->px, r->pz, r->py, n, F3DS_ROAD_STREET);
					n = 0;
					continue;
				}
				r->px[n] = x;
				r->pz[n] = z;
				r->py[n] = s->height;
				++n;
			}
			if (n >= 5)
				append_road(r, r->px, r->pz, r->py, n, F3DS_ROAD_STREET);
		}
}

/* Make sure a settlement's streets meet the road passing through it. */
static void link_streets(Router *r, const F3DSSite *s)
{
	F3DSWorld *w = r->g->w;
	int from = -1, to = -1;
	float best = 90.0f * 90.0f;
	for (int a = 0; a < w->sampleCount; ++a) {
		const F3DSRoadSample *p = &w->samples[a];
		if (p->type != F3DS_ROAD_STREET || hypotf(p->x - s->x, p->z - s->z) > s->radius)
			continue;
		for (int b = 0; b < w->sampleCount; ++b) {
			const F3DSRoadSample *q = &w->samples[b];
			if (q->type == F3DS_ROAD_STREET)
				continue;
			const float d = (p->x - q->x) * (p->x - q->x) + (p->z - q->z) * (p->z - q->z);
			if (d < best) {
				best = d;
				from = a;
				to = b;
			}
		}
	}
	if (from < 0 || best < 5.0f * 5.0f)
		return;
	const F3DSRoadSample a = w->samples[from], b = w->samples[to];
	const int segments = (int)(sqrtf(best) / WORLD_ROAD_SPACING) + 1;
	for (int i = 0; i <= segments; ++i) {
		const float u = (float)i / segments;
		r->px[i] = lerpf(a.x, b.x, u);
		r->pz[i] = lerpf(a.z, b.z, u);
		r->py[i] = lerpf(a.y, b.y, u);
	}
	append_road(r, r->px, r->pz, r->py, segments + 1, F3DS_ROAD_STREET);
}

/* Nearest dry, open node: route ends must not fall in water. */
static int open_node(const Router *r, float x, float z)
{
	const int n = node_at(x, z);
	if (!r->blocked[n] && !r->river[n])
		return n;
	for (int radius = 1; radius < 8; ++radius)
		for (int dz = -radius; dz <= radius; ++dz)
			for (int dx = -radius; dx <= radius; ++dx) {
				const int i = gen_clampi(n % RN + dx, 1, RN - 2);
				const int j = gen_clampi(n / RN + dz, 1, RN - 2);
				const int m = j * RN + i;
				if (!r->blocked[m] && !r->river[m])
					return m;
			}
	return n;
}

/* A random spot of wild country for a trail to lead to. */
static int wild_node(Router *r, F3DSRng *rng, float fromX, float fromZ, float minH, float maxH)
{
	for (int t = 0; t < 400; ++t) {
		const float a = rng_range(rng, 0.0f, 2.0f * F3DS_PI), d = rng_range(rng, 450.0f, 1300.0f);
		const float x = fromX + cosf(a) * d, z = fromZ + sinf(a) * d;
		if (fabsf(x) > 3700.0f || fabsf(z) > 3700.0f)
			continue;
		const int n = node_at(x, z);
		const float h = r->height[n];
		if (!r->blocked[n] && !r->river[n] && r->road[n] < 0 && h > minH && h < maxH)
			return n;
	}
	return -1;
}

/* Where two roads overlap (crossings, forks), bring them to one height so
 * that no step is left on the asphalt. */
static bool level_junctions(F3DSWorld *w)
{
	const int cells = WORLD_ROAD_CELLS * WORLD_ROAD_CELLS, count = w->sampleCount;
	int32_t *head = malloc(sizeof(int32_t) * (size_t)cells);
	int32_t *next = malloc(sizeof(int32_t) * (size_t)(count ? count : 1));
	uint16_t *piece = malloc(sizeof(uint16_t) * (size_t)(count ? count : 1));
	float *y = malloc(sizeof(float) * (size_t)(count ? count : 1));
	bool ok = head && next && piece && y;
	if (!ok)
		goto done;
	for (int c = 0; c < cells; ++c)
		head[c] = -1;
	for (int k = 0; k < w->roadCount; ++k)
		for (int i = 0; i < w->roads[k].count; ++i)
			piece[w->roads[k].first + i] = (uint16_t)k;
	for (int s = 0; s < count; ++s) {
		const int i = gen_clampi((int)((w->samples[s].x + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
		const int j = gen_clampi((int)((w->samples[s].z + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
		next[s] = head[j * WORLD_ROAD_CELLS + i];
		head[j * WORLD_ROAD_CELLS + i] = s;
	}
	for (int round = 0; round < 40; ++round) {
		for (int s = 0; s < count; ++s) {
			const F3DSRoadSample *a = &w->samples[s];
			const int ci = gen_clampi((int)((a->x + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
			const int cj = gen_clampi((int)((a->z + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
			float sum = 0.0f, weight = 0.0f;
			for (int j = cj > 0 ? cj - 1 : 0; j <= cj + 1 && j < WORLD_ROAD_CELLS; ++j)
				for (int i = ci > 0 ? ci - 1 : 0; i <= ci + 1 && i < WORLD_ROAD_CELLS; ++i)
					for (int t = head[j * WORLD_ROAD_CELLS + i]; t >= 0; t = next[t]) {
						if (piece[t] == piece[s])
							continue;
						const F3DSRoadSample *b = &w->samples[t];
						const float reach = road_half_width(a->type) + road_half_width(b->type) + 4.0f;
						const float dx = a->x - b->x, dz = a->z - b->z;
						const float d2 = dx * dx + dz * dz;
						if (d2 >= reach * reach || fabsf(a->y - b->y) > 4.0f)
							continue;
						const float q = 1.0f - sqrtf(d2) / reach;
						sum += (b->y - a->y) * q * q;
						weight += q * q;
					}
			y[s] = a->y + (weight > 0.0f ? 0.5f * sum / weight : 0.0f);
		}
		for (int s = 0; s < count; ++s)
			w->samples[s].y = y[s];
		/* Then ease each road's profile around the corrections. */
		for (int s = 1; s + 1 < count; ++s) {
			if ((w->samples[s - 1].flags | w->samples[s].flags) & F3DS_SAMPLE_LAST)
				continue;
			w->samples[s].y += 0.3f * (0.5f * (w->samples[s - 1].y + w->samples[s + 1].y) -
				w->samples[s].y);
		}
		for (int s = 0; s + 1 < count; ++s) {
			F3DSRoadSample *a = &w->samples[s], *b = a + 1;
			if (a->flags & F3DS_SAMPLE_LAST)
				continue;
			const float most = 0.24f * hypotf(b->x - a->x, b->z - a->z);
			const float excess = fabsf(b->y - a->y) - most;
			if (excess > 0.0f) {
				const float move = copysignf(0.5f * excess, b->y - a->y);
				a->y += move;
				b->y -= move;
			}
		}
	}
done:
	free(head);
	free(next);
	free(piece);
	free(y);
	return ok;
}

static void classify(F3DSWorld *w)
{
	for (int k = 0; k < w->roadCount; ++k) {
		const F3DSRoad *road = &w->roads[k];
		static float x[PIECE_MAX], z[PIECE_MAX];
		for (int i = 0; i < road->count; ++i) {
			x[i] = w->samples[road->first + i].x;
			z[i] = w->samples[road->first + i].z;
		}
		for (int i = 0; i < road->count; ++i) {
			F3DSRoadSample *s = &w->samples[road->first + i];
			const int a = i > 2 ? i - 2 : 0, b = i + 2 < road->count ? i + 2 : road->count - 1;
			const float run = hypotf(x[b] - x[a], z[b] - z[a]);
			const float grade = run > 1.0f ?
				fabsf(w->samples[road->first + b].y - w->samples[road->first + a].y) / run : 0.0f;
			int bends = 0;
			for (int j = i - 6; j <= i + 6; ++j)
				if (j > 0 && j + 1 < road->count && turn_radius(x, z, j) < 55.0f)
					++bends;
			if (road->type == F3DS_ROAD_DIRT || road->type == F3DS_ROAD_HIDDEN)
				s->use = F3DS_USE_RALLY;
			else if (road->type != F3DS_ROAD_STREET && bends >= 5)
				s->use = F3DS_USE_DRIFT;
			else if (road->type != F3DS_ROAD_STREET && (grade > 0.065f || s->y > 120.0f))
				s->use = F3DS_USE_MOUNTAIN;
			else
				s->use = F3DS_USE_ROUTE;
		}
	}
}

bool gen_roads(F3DSGen *g)
{
	F3DSWorld *w = g->w;
	Router r;
	memset(&r, 0, sizeof(r));
	r.g = g;
	r.heapCapacity = NODES * 6;
	r.height = malloc(sizeof(float) * NODES);
	r.blocked = malloc(NODES);
	r.river = malloc(NODES);
	r.road = malloc(sizeof(int32_t) * NODES);
	r.cost = malloc(sizeof(float) * NODES);
	r.parent = malloc(sizeof(int32_t) * NODES);
	r.closed = malloc(NODES);
	r.heap = malloc(sizeof(Item) * (size_t)r.heapCapacity);
	r.path = malloc(sizeof(int32_t) * PIECE_MAX);
	r.px = malloc(sizeof(float) * PIECE_MAX * 5);
	bool ok = r.height && r.blocked && r.river && r.road && r.cost && r.parent && r.closed &&
		r.heap && r.path && r.px;
	if (!ok)
		goto done;
	r.pz = r.px + PIECE_MAX;
	r.py = r.pz + PIECE_MAX;
	r.sx = r.py + PIECE_MAX;
	r.sz = r.sx + PIECE_MAX;

	for (int n = 0; n < NODES; ++n) {
		const float x = node_x(n), z = node_z(n);
		const int c = gen_bindex(x, z);
		r.height[n] = gen_height_at(g, x, z);
		r.blocked[n] = r.height[n] < 0.9f || (g->wet[c] & GEN_WET_LAKE) != 0;
		r.river[n] = (g->wet[c] & GEN_WET_RIVER) != 0;
		r.road[n] = -1;
	}
	w->roadCount = w->sampleCount = 0;
	w->roadLength = 0.0f;
	w->routeRetries = w->routeFailures = 0;

	/* 1. A main ring through the city, industry and villages, ordered by
	 * their bearing from the middle of the island. */
	int ring[WORLD_SITE_MAX], ringCount = 0;
	for (int i = 0; i < w->siteCount; ++i) {
		const int kind = w->sites[i].kind;
		if (kind != F3DS_SITE_CITY && kind != F3DS_SITE_INDUSTRY && kind != F3DS_SITE_VILLAGE)
			continue;
		const float a = atan2f(w->sites[i].z, w->sites[i].x);
		int k = ringCount++;
		while (k > 0 && atan2f(w->sites[ring[k - 1]].z, w->sites[ring[k - 1]].x) > a) {
			ring[k] = ring[k - 1];
			--k;
		}
		ring[k] = i;
	}
	uint32_t route = 0;
	for (int i = 0; i < ringCount && ringCount > 1; ++i) {
		const F3DSSite *a = &w->sites[ring[i]], *b = &w->sites[ring[(i + 1) % ringCount]];
		lay_route(&r, open_node(&r, a->x, a->z), open_node(&r, b->x, b->z), F3DS_ROAD_MAIN, route++);
		gen_report(g, 50 + i * 14 / ringCount, "Route principale");
	}

	/* 2. Town and village streets. */
	F3DSRng rng = rng_make(hash_seed(g->roads, 5000));
	for (int i = 0; i < w->siteCount; ++i) {
		const F3DSSite *s = &w->sites[i];
		const float angle = rng_range(&rng, 0.0f, F3DS_PI * 0.5f);
		if (s->kind == F3DS_SITE_CITY)
			streets(&r, s, 78.0f, 4, angle);
		else if (s->kind == F3DS_SITE_INDUSTRY)
			streets(&r, s, 110.0f, 1, angle);
		else if (s->kind == F3DS_SITE_VILLAGE)
			streets(&r, s, 70.0f, 0, angle);
	}

	/* 3. Secondary roads: points of interest and a mountain crossing. */
	const F3DSSite *view = NULL;
	for (int i = 0; i < w->siteCount; ++i)
		if (w->sites[i].kind == F3DS_SITE_VIEWPOINT)
			view = &w->sites[i];
	for (int i = 0; i < w->siteCount; ++i) {
		const F3DSSite *s = &w->sites[i];
		if (s->kind != F3DS_SITE_VIEWPOINT && s->kind != F3DS_SITE_LIGHTHOUSE &&
		    s->kind != F3DS_SITE_HARBOUR && s->kind != F3DS_SITE_BEACH &&
		    s->kind != F3DS_SITE_LAKE)
			continue;
		/* Join from the closest settlement. */
		float best = 1e30f, tx = s->x, tz = s->z;
		for (int k = 0; k < ringCount; ++k) {
			const F3DSSite *o = &w->sites[ring[k]];
			const float d = hypotf(o->x - s->x, o->z - s->z);
			if (d < best) {
				best = d;
				tx = o->x;
				tz = o->z;
			}
		}
		/* A lake is reached on its shore, not in the middle. */
		lay_route(&r, open_node(&r, tx, tz), open_node(&r, s->x, s->z), F3DS_ROAD_SECONDARY, route++);
		gen_report(g, 66, "Routes secondaires");
	}
	if (view && ringCount > 2) {
		/* From the viewpoint down to a settlement on the far side. */
		float best = -1.0f;
		int far = 0;
		const F3DSSite *city = &w->sites[0];
		for (int k = 0; k < ringCount; ++k) {
			const F3DSSite *o = &w->sites[ring[k]];
			const float d = hypotf(o->x - city->x, o->z - city->z);
			const float dv = hypotf(o->x - view->x, o->z - view->z);
			if (d - dv * 0.5f > best) {
				best = d - dv * 0.5f;
				far = ring[k];
			}
		}
		lay_route(&r, open_node(&r, view->x, view->z),
			open_node(&r, w->sites[far].x, w->sites[far].z), F3DS_ROAD_SECONDARY, route++);
	}
	gen_report(g, 70, "Pistes de rallye");

	/* 4. Dirt trails into the wild, looping back where possible. */
	int trails = 0;
	for (int k = 0; k < ringCount && trails < 7; ++k) {
		const F3DSSite *s = &w->sites[ring[k]];
		if (s->kind != F3DS_SITE_VILLAGE)
			continue;
		const int target = wild_node(&r, &rng, s->x, s->z, 14.0f, 260.0f);
		if (target < 0)
			continue;
		if (lay_route(&r, open_node(&r, s->x, s->z), target, F3DS_ROAD_DIRT, route++)) {
			++trails;
			const F3DSSite *next = &w->sites[ring[(k + 1) % ringCount]];
			lay_route(&r, target, open_node(&r, next->x, next->z), F3DS_ROAD_DIRT, route++);
			if (trails == 1)
				if (w->siteCount < WORLD_SITE_MAX) {
					F3DSSite *t = &w->sites[w->siteCount++];
					memset(t, 0, sizeof(*t));
					t->kind = F3DS_SITE_TRAIL;
					t->x = node_x(target);
					t->z = node_z(target);
					t->radius = 20.0f;
					t->height = r.height[target];
					strcpy(t->name, "Piste Inoshishi");
				}
		}
	}
	/* 5. Hidden narrow passages: shortcuts through wild country between two
	 * existing roads. */
	for (int i = 0, made = 0; i < 40 && made < 3 && w->sampleCount > 2; ++i) {
		const F3DSRoadSample from = w->samples[rng_int(&rng, w->sampleCount)];
		const F3DSRoadSample to = w->samples[rng_int(&rng, w->sampleCount)];
		const float d = hypotf(to.x - from.x, to.z - from.z);
		if (d < 500.0f || d > 1400.0f || from.type == F3DS_ROAD_STREET ||
		    to.type == F3DS_ROAD_STREET || ((from.flags | to.flags) & F3DS_SAMPLE_BRIDGE))
			continue;
		const int start = node_at(from.x, from.z), end = node_at(to.x, to.z);
		if (r.road[start] < 0 || r.road[end] < 0)
			continue;
		const int before = w->roadCount;
		lay_route(&r, start, end, F3DS_ROAD_HIDDEN, route++);
		made += w->roadCount > before;
	}
	for (int i = 0; i < w->siteCount; ++i) {
		const int kind = w->sites[i].kind;
		if (kind == F3DS_SITE_CITY || kind == F3DS_SITE_INDUSTRY || kind == F3DS_SITE_VILLAGE)
			link_streets(&r, &w->sites[i]);
	}
	ok = level_junctions(w);
	if (ok)
		classify(w);
done:
	free(r.height);
	free(r.blocked);
	free(r.river);
	free(r.road);
	free(r.cost);
	free(r.parent);
	free(r.closed);
	free(r.heap);
	free(r.path);
	free(r.px);
	return ok;
}

/* Cells covered by a segment, with the margin used by every query. */
static void segment_cells(const F3DSRoadSample *a, const F3DSRoadSample *b, int *i0, int *i1,
	int *j0, int *j1)
{
	const float m = road_half_width(a->type) + 30.0f;
	*i0 = gen_clampi((int)((fminf(a->x, b->x) - m + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
	*i1 = gen_clampi((int)((fmaxf(a->x, b->x) + m + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
	*j0 = gen_clampi((int)((fminf(a->z, b->z) - m + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
	*j1 = gen_clampi((int)((fmaxf(a->z, b->z) + m + WORLD_HALF) / WORLD_ROAD_CELL), 0, WORLD_ROAD_CELLS - 1);
}

bool gen_road_index(F3DSGen *g)
{
	F3DSWorld *w = g->w;
	const int cells = WORLD_ROAD_CELLS * WORLD_ROAD_CELLS;
	w->cellStart = calloc((size_t)cells + 1, sizeof(uint32_t));
	if (!w->cellStart)
		return false;
	for (int pass = 0; pass < 2; ++pass) {
		for (int s = 0; s + 1 < w->sampleCount; ++s) {
			if (w->samples[s].flags & F3DS_SAMPLE_LAST)
				continue;
			int i0, i1, j0, j1;
			segment_cells(&w->samples[s], &w->samples[s + 1], &i0, &i1, &j0, &j1);
			for (int j = j0; j <= j1; ++j)
				for (int i = i0; i <= i1; ++i) {
					uint32_t *slot = &w->cellStart[j * WORLD_ROAD_CELLS + i];
					if (pass == 0)
						(*slot)++;
					else
						w->cellItems[--(*slot)] = (uint16_t)s;
				}
		}
		if (pass == 0) {
			/* Running ends; filling backwards turns them into starts. */
			uint32_t total = 0;
			for (int c = 0; c < cells; ++c) {
				total += w->cellStart[c];
				w->cellStart[c] = total;
			}
			w->cellStart[cells] = total;
			w->itemCount = total;
			w->cellItems = malloc(sizeof(uint16_t) * (total ? total : 1));
			if (!w->cellItems)
				return false;
		}
	}
	return true;
}

/* Cut and fill the ground so that it meets every road from below. Scratch
 * grids are kept in fixed point: the console has little memory to spare. */
#define NO_CORE 255u

bool gen_carve_roads(F3DSGen *g)
{
	F3DSWorld *w = g->w;
	const size_t n = (size_t)WORLD_N * WORLD_N;
	/* Under and beside a road, the ground follows the closest point of it:
	 * "core" is that distance in eighths of a metre. */
	uint8_t *core = malloc(n);
	int16_t *level = malloc(sizeof(int16_t) * n);
	uint8_t *weight = calloc(n, 1);
	int16_t *blend = malloc(sizeof(int16_t) * n);
	if (!core || !level || !weight || !blend) {
		free(level);
		free(core);
		free(weight);
		free(blend);
		return false;
	}
	memset(core, NO_CORE, n);
	for (int s = 0; s + 1 < w->sampleCount; ++s) {
		const F3DSRoadSample *a = &w->samples[s], *b = a + 1;
		if ((a->flags & F3DS_SAMPLE_LAST) || ((a->flags & b->flags) & F3DS_SAMPLE_BRIDGE))
			continue;
		const float half = road_half_width(a->type);
		const float inner = half + 11.4f, outer = half + 30.0f;
		const int i0 = gen_clampi((int)((fminf(a->x, b->x) - outer + WORLD_HALF) / WORLD_CELL), 0, WORLD_N - 1);
		const int i1 = gen_clampi((int)((fmaxf(a->x, b->x) + outer + WORLD_HALF) / WORLD_CELL) + 1, 0, WORLD_N - 1);
		const int j0 = gen_clampi((int)((fminf(a->z, b->z) - outer + WORLD_HALF) / WORLD_CELL), 0, WORLD_N - 1);
		const int j1 = gen_clampi((int)((fmaxf(a->z, b->z) + outer + WORLD_HALF) / WORLD_CELL) + 1, 0, WORLD_N - 1);
		const float dx = b->x - a->x, dz = b->z - a->z;
		const float len2 = dx * dx + dz * dz;
		for (int j = j0; j <= j1; ++j)
			for (int i = i0; i <= i1; ++i) {
				const float x = gen_x(i), z = gen_x(j);
				const float u = len2 > 0.0f ?
					clampf(((x - a->x) * dx + (z - a->z) * dz) / len2, 0.0f, 1.0f) : 0.0f;
				const float d = hypotf(x - a->x - dx * u, z - a->z - dz * u);
				if (d >= outer)
					continue;
				const int16_t y = (int16_t)floorf((lerpf(a->y, b->y, u) - 0.20f) / WORLD_HEIGHT_UNIT);
				const size_t k = (size_t)j * WORLD_N + i;
				if (d <= inner) {
					const uint8_t q = (uint8_t)(d * 8.0f);
					if (q < core[k] || (q == core[k] && y < level[k])) {
						core[k] = q;
						level[k] = y;
					}
				} else {
					const int t = (int)(255.0f * (1.0f - smoothstepf((d - inner) / (outer - inner))));
					if (t > weight[k]) {
						weight[k] = (uint8_t)t;
						blend[k] = y;
					}
				}
			}
	}
	for (int j = 0; j < WORLD_N; ++j)
		for (int i = 0; i < WORLD_N; ++i) {
			const size_t k = (size_t)j * WORLD_N + i;
			const bool inCore = core[k] != NO_CORE;
			float target;
			if (inCore)
				target = level[k] * WORLD_HEIGHT_UNIT;
			else if (weight[k] > 0 && g->h[k] > 0.0f)
				target = lerpf(g->h[k], blend[k] * WORLD_HEIGHT_UNIT, weight[k] / 255.0f);
			else
				continue;
			/* Never fill in a river or lake bed. */
			const float water = g->wlevel[gen_bindex(gen_x(i), gen_x(j))];
			if (water != WORLD_DRY && g->h[k] < water)
				target = fminf(target, g->h[k]);
			g->h[k] = target;
		}
	free(core);
	free(level);
	free(weight);
	free(blend);
	/* Where two roads run side by side at different heights, the ground
	 * must not poke through the lower one. */
	for (int s = 0; s + 1 < w->sampleCount; ++s) {
		const F3DSRoadSample *a = &w->samples[s], *b = a + 1;
		if ((a->flags & F3DS_SAMPLE_LAST) || ((a->flags & b->flags) & F3DS_SAMPLE_BRIDGE))
			continue;
		const float half = road_half_width(a->type);
		const float reach = half + 4.5f;
		const int i0 = gen_clampi((int)((fminf(a->x, b->x) - reach + WORLD_HALF) / WORLD_CELL), 0, WORLD_N - 1);
		const int i1 = gen_clampi((int)((fmaxf(a->x, b->x) + reach + WORLD_HALF) / WORLD_CELL) + 1, 0, WORLD_N - 1);
		const int j0 = gen_clampi((int)((fminf(a->z, b->z) - reach + WORLD_HALF) / WORLD_CELL), 0, WORLD_N - 1);
		const int j1 = gen_clampi((int)((fmaxf(a->z, b->z) + reach + WORLD_HALF) / WORLD_CELL) + 1, 0, WORLD_N - 1);
		const float dx = b->x - a->x, dz = b->z - a->z;
		const float len2 = dx * dx + dz * dz;
		for (int j = j0; j <= j1; ++j)
			for (int i = i0; i <= i1; ++i) {
				const float x = gen_x(i), z = gen_x(j);
				const float u = len2 > 0.0f ?
					clampf(((x - a->x) * dx + (z - a->z) * dz) / len2, 0.0f, 1.0f) : 0.0f;
				const float d = hypotf(x - a->x - dx * u, z - a->z - dz * u);
				if (d >= reach)
					continue;
				const float top = lerpf(a->y, b->y, u) - 0.12f + 0.25f * fmaxf(d - half, 0.0f);
				float *h = &g->h[j * WORLD_N + i];
				*h = fminf(*h, top);
			}
	}
	return true;
}
