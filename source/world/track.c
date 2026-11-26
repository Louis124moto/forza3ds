#include "track.h"
#include <string.h>

float track_coast_radius(float x, float z)
{
	const float r = sqrtf(x * x + z * z);
	if (r < 0.001f) return 250.0f;
	/* Evaluated on the nominal shoreline in the same direction, so the limit
	 * depends only on the heading from the island centre. */
	const float cx = x / r * 250.0f, cz = z / r * 250.0f;
	return 250.0f + 13.0f * sinf(cx * 0.018f) + 10.0f * cosf(cz * 0.021f) - 6.0f;
}

/* An obstacle is dropped when it would intrude on another stretch of road:
 * "clearance" is the least distance allowed from any centre line. */
static void push_obstacle(F3DSTrack *t, FVec3 base, float hx, float hz, float height,
	F3DSObstacleKind kind, float clearance)
{
	if (t->obstacleCount >= TRACK_OBSTACLE_MAX ||
	    track_nearest(t, base, NULL, NULL) < clearance)
		return;
	F3DSObstacle *o = &t->obstacles[t->obstacleCount++];
	o->center = fvec3(base.x, 0.0f, base.z);
	o->hx = hx;
	o->hz = hz;
	o->height = height;
	o->kind = kind;
}

static float random01(uint32_t *state)
{
	*state = *state * 1664525u + 1013904223u;
	return (float)(*state >> 16) / 65535.0f;
}

/* Deterministic woodland on the grass, kept clear of the road and barriers. */
static void plant_trees(F3DSTrack *t)
{
	uint32_t seed = 0xF03A3D5u;
	for (int tries = 0; tries < 2000 && t->treeCount < TRACK_TREE_MAX; ++tries) {
		const float a = random01(&seed) * 2.0f * F3DS_PI;
		const float r = sqrtf(random01(&seed)) * 255.0f;
		const float x = cosf(a) * r, z = sinf(a) * r;
		const float height = 3.0f + 2.6f * random01(&seed);
		const float crown = 1.3f + 1.0f * random01(&seed);
		const int blossom = random01(&seed) < 0.18f;
		if (r > track_coast_radius(x, z) - 8.0f)
			continue;
		if (track_nearest(t, fvec3(x, 0.0f, z), NULL, NULL) < TRACK_HALF_WIDTH + 6.0f)
			continue;
		bool crowded = false;
		for (int i = 0; i < t->treeCount && !crowded; ++i) {
			const float dx = x - t->trees[i].x, dz = z - t->trees[i].z;
			crowded = dx * dx + dz * dz < 36.0f;
		}
		if (crowded)
			continue;
		F3DSTree *tree = &t->trees[t->treeCount++];
		tree->x = x;
		tree->z = z;
		tree->height = height;
		tree->crown = crown;
		tree->blossom = blossom;
	}
}

static FVec3 side_point(const F3DSTrack *t, int i, float offset)
{
	return fvec3_add(t->pts[i], fvec3_scale(t->jointRight[i], offset));
}

/* A barrier is only kept when none of it intrudes on any part of the road. */
static bool wall_clear(const F3DSTrack *t, FVec3 a, FVec3 b)
{
	for (int k = 0; k <= 16; ++k) {
		const FVec3 p = fvec3_add(a, fvec3_scale(fvec3_sub(b, a), (float)k / 16.0f));
		if (track_nearest(t, p, NULL, NULL) < TRACK_HALF_WIDTH + TRACK_WALL_HALF_THICKNESS + 0.4f)
			return false;
	}
	return true;
}

/* Deterministic smooth closed circuit: radius modulated with sine terms. */
static FVec3 circuit(int k, int samples)
{
	const float a = (float)k / (float)samples * 2.0f * F3DS_PI;
	const float r = 140.0f + 30.0f * sinf(3.0f * a + 0.7f) + 12.0f * sinf(5.0f * a + 2.1f)
		+ 45.0f * sinf(a + 0.3f);
	return fvec3(r * cosf(a), 0.0f, r * sinf(a));
}

/* Points evenly spaced along the curve, so every segment has the same length
 * and no bend is squeezed into a few metres. */
static void build_centre_line(F3DSTrack *t)
{
	const int samples = 1536;
	float total = 0.0f;
	for (int k = 0; k < samples; k++)
		total += fvec3_len(fvec3_sub(circuit(k + 1, samples), circuit(k, samples)));
	const float spacing = total / TRACK_POINTS;
	float walked = 0.0f;
	int next = 0;
	for (int k = 0; k < samples && next < TRACK_POINTS; k++) {
		const FVec3 a = circuit(k, samples), d = fvec3_sub(circuit(k + 1, samples), a);
		const float len = fvec3_len(d);
		while (next < TRACK_POINTS && spacing * next <= walked + len) {
			const float u = len > 0.0f ? (spacing * next - walked) / len : 0.0f;
			t->pts[next++] = fvec3_add(a, fvec3_scale(d, u));
		}
		walked += len;
	}
	t->pts[TRACK_POINTS] = t->pts[0];
}

void track_init(F3DSTrack *t)
{
	memset(t, 0, sizeof(*t));
	build_centre_line(t);

	for (int i = 0; i < TRACK_POINTS; i++) {
		const FVec3 d = fvec3_sub(t->pts[i + 1], t->pts[i]);
		const float len = fvec3_len(d);
		t->segLen[i] = len;
		t->fwd[i] = len > 0.0001f ? fvec3_scale(d, 1.0f / len) : fvec3(0.0f, 0.0f, 1.0f);
		t->right[i] = fvec3(-t->fwd[i].z, 0.0f, t->fwd[i].x);
	}
	for (int i = 0; i <= TRACK_POINTS; i++) {
		const int cur = i % TRACK_POINTS;
		const int prev = (i + TRACK_POINTS - 1) % TRACK_POINTS;
		const FVec3 sum = fvec3_add(t->right[cur], t->right[prev]);
		const float len2 = fvec3_dot(sum, sum);
		/* Mitre so both road edges stay TRACK_HALF_WIDTH from both segments,
		 * limited on very sharp joints. */
		float scale = len2 > 0.01f ? 2.0f / len2 : 0.5f;
		if (scale * sqrtf(len2) > 1.5f) scale = 1.5f / sqrtf(len2);
		t->jointRight[i] = fvec3_scale(sum, scale);
	}

	/* Static obstacles: posts along both sides of the track. */
	for (int i = 0; i < TRACK_POINTS; i += 12) {
		push_obstacle(t, fvec3_add(t->pts[i], fvec3_scale(t->right[i], TRACK_HALF_WIDTH + 2.8f)),
			0.5f, 0.5f, 2.6f, F3DS_OBSTACLE_POST,
			TRACK_HALF_WIDTH + 1.2f);
		const int j = (i + 48) % TRACK_POINTS;
		push_obstacle(t, fvec3_add(t->pts[j], fvec3_scale(t->right[j], -(TRACK_HALF_WIDTH + 2.8f))),
			0.5f, 0.5f, 2.6f, F3DS_OBSTACLE_POST,
			TRACK_HALF_WIDTH + 1.2f);
	}
	/* A few crates on the road itself, to exercise collisions. */
	for (int i = 6; i < TRACK_POINTS; i += 24)
		push_obstacle(t, fvec3_add(t->pts[i], fvec3_scale(t->right[i], 3.2f)), 0.8f, 0.8f, 1.2f, F3DS_OBSTACLE_CRATE, 2.9f);

	/* Torii gate over the road just after the start. */
	for (int side = -1; side <= 1; side += 2)
		push_obstacle(t, fvec3_add(t->pts[2], fvec3_scale(t->right[2],
			(float)side * (TRACK_HALF_WIDTH + 1.2f))), 0.35f, 0.35f, 6.0f, F3DS_OBSTACLE_TORII,
			TRACK_HALF_WIDTH + 0.6f);

	/* Barriers on the outside of the sharper bends. */
	for (int i = 0; i < TRACK_POINTS && t->wallCount < TRACK_WALL_MAX; i++) {
		const int next = (i + 1) % TRACK_POINTS;
		const int prev = (i + TRACK_POINTS - 1) % TRACK_POINTS;
		const float inTurn = fvec3_dot(t->fwd[i], t->right[prev]);
		const float outTurn = fvec3_dot(t->fwd[next], t->right[i]);
		const float turn = fabsf(inTurn) > fabsf(outTurn) ? inTurn : outTurn;
		if (fabsf(turn) < 0.24f)
			continue;
		/* Turning left (negative) puts the outside on the right. */
		const float side = turn < 0.0f ? 1.0f : -1.0f;
		const float off = side * (TRACK_HALF_WIDTH + 1.5f);
		const FVec3 a = side_point(t, i, off);
		const FVec3 b = side_point(t, i + 1, off);
		if (!wall_clear(t, a, b))
			continue;
		t->walls[t->wallCount].a = a;
		t->walls[t->wallCount].b = b;
		t->wallCount++;
	}

	plant_trees(t);
}

float track_nearest(const F3DSTrack *t, FVec3 p, int *seg, float *s)
{
	float bestDist2 = 1e30f;
	float bestS = 0.0f;
	int bestSeg = 0;
	for (int i = 0; i < TRACK_POINTS; i++) {
		const FVec3 a = t->pts[i];
		const FVec3 d = t->fwd[i];
		const float along = clampf((p.x - a.x) * d.x + (p.z - a.z) * d.z, 0.0f, t->segLen[i]);
		const float cx = a.x + d.x * along - p.x;
		const float cz = a.z + d.z * along - p.z;
		const float d2 = cx * cx + cz * cz;
		if (d2 < bestDist2) {
			bestDist2 = d2;
			bestSeg = i;
			bestS = along;
		}
	}
	if (seg) *seg = bestSeg;
	if (s) *s = bestS;
	return sqrtf(bestDist2);
}
