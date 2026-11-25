#include "track.h"

static void track_push_box(F3DSTrack *t, FVec3 c, float hx, float hz, float h)
{
	if (t->obstacleCount >= TRACK_OBSTACLE_MAX)
		return;
	t->obstacles[t->obstacleCount].center = c;
	t->obstacles[t->obstacleCount].hx = hx;
	t->obstacles[t->obstacleCount].hz = hz;
	t->obstacles[t->obstacleCount].height = h;
	t->obstacleCount++;
}

void track_init(F3DSTrack *t)
{
	/* Deterministic smooth closed circuit: radius modulated with sine terms. */
	const float n = (float)TRACK_POINTS;
	for (int i = 0; i < TRACK_POINTS; i++) {
		const float a = (float)i / n * 2.0f * F3DS_PI;
		const float r = 130.0f + 38.0f * sinf(3.0f * a + 0.7f) + 22.0f * sinf(5.0f * a + 2.1f) + 55.0f * sinf(a + 0.3f);
		t->pts[i] = fvec3(r * cosf(a), 0.0f, r * sinf(a));
	}
	t->pts[TRACK_POINTS] = t->pts[0];

	for (int i = 0; i < TRACK_POINTS; i++) {
		const FVec3 d = fvec3_sub(t->pts[i + 1], t->pts[i]);
		const float len = fvec3_len(d);
		t->segLen[i] = len;
		if (len > 0.0001f) {
			t->fwd[i] = fvec3_scale(d, 1.0f / len);
			t->right[i] = fvec3(-t->fwd[i].z, 0.0f, t->fwd[i].x);
		} else {
			t->fwd[i] = fvec3(0.0f, 0.0f, 1.0f);
			t->right[i] = fvec3(1.0f, 0.0f, 0.0f);
		}
	}

	/* Static obstacles: posts along both sides of the track. */
	t->obstacleCount = 0;
	for (int i = 0; i < TRACK_POINTS; i += 12) {
		const FVec3 base = fvec3_add(t->pts[i], fvec3_scale(t->right[i], TRACK_HALF_WIDTH + 2.5f));
		track_push_box(t, fvec3(base.x, 1.0f, base.z), 0.6f, 0.6f, 2.0f);
		const int j = (i + 48) % TRACK_POINTS;
		const FVec3 other = fvec3_add(t->pts[j], fvec3_scale(t->right[j], -(TRACK_HALF_WIDTH + 2.5f)));
		track_push_box(t, fvec3(other.x, 1.0f, other.z), 0.6f, 0.6f, 2.0f);
	}
	/* A few crates on the track itself for the collision demo. */
	for (int i = 6; i < TRACK_POINTS; i += 24) {
		const FVec3 base = fvec3_add(t->pts[i], fvec3_scale(t->right[i], 2.0f));
		track_push_box(t, fvec3(base.x, 0.6f, base.z), 1.0f, 1.0f, 1.2f);
	}
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
	return bestDist2 > 0.0f ? sqrtf(bestDist2) : 0.0f;
}

int track_resolve_collision(const F3DSTrack *t, FVec3 *pos, float *vx, float *vz)
{
	int collided = 0;

	/* Track corridor: distance from the centerline. */
	int seg = 0;
	float s = 0.0f;
	const float lat = track_nearest(t, *pos, &seg, &s);
	(void)s;
	if (lat > TRACK_HALF_WIDTH) {
		const FVec3 r = t->right[seg];
		const float side = ((pos->x - t->pts[seg].x) * r.x + (pos->z - t->pts[seg].z) * r.z) >= 0.0f ? 1.0f : -1.0f;
		const FVec3 inward = side > 0.0f ? fvec3(-r.x, 0.0f, -r.z) : fvec3(r.x, 0.0f, r.z);
		const float depth = lat - TRACK_HALF_WIDTH;
		pos->x += inward.x * depth;
		pos->z += inward.z * depth;
		/* Scrub speed against the invisible corridor wall. */
		*vx *= 0.7f;
		*vz *= 0.7f;
		collided = 1;
	}

	/* Obstacle boxes, pushout along the axis of least penetration. */
	const float carRadius = 0.9f;
	for (int i = 0; i < t->obstacleCount; i++) {
		const FVec3 c = t->obstacles[i].center;
		const float dx = pos->x - c.x;
		const float dz = pos->z - c.z;
		const float ex = t->obstacles[i].hx + carRadius;
		const float ez = t->obstacles[i].hz + carRadius;
		if (fabsf(dx) < ex && fabsf(dz) < ez) {
			const float penX = ex - fabsf(dx);
			const float penZ = ez - fabsf(dz);
			if (penX < penZ) {
				pos->x += (dx >= 0.0f) ? penX : -penX;
				*vx = -0.2f * *vx;
			} else {
				pos->z += (dz >= 0.0f) ? penZ : -penZ;
				*vz = -0.2f * *vz;
			}
			*vx *= 0.6f;
			*vz *= 0.6f;
			collided = 1;
		}
	}
	return collided;
}
