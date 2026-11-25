#include "terrain.h"

void terrain_init(F3DSTerrain *t, const F3DSTrack *track)
{
	t->rampCount = 0;
	t->slickCount = 0;

	/* Two launch ramps aligned with the track direction, long approach
	 * straight before each. */
	const int rampIdx[2] = { 24, 64 };
	for (int i = 0; i < 2; i++) {
		const int idx = rampIdx[i];
		if (t->rampCount >= TERRAIN_RAMP_MAX)
			break;
		F3DSTerrainRamp *r = &t->ramps[t->rampCount++];
		r->origin = track->pts[idx];
		r->fwd = track->fwd[idx];
		r->right = track->right[idx];
		r->length = 14.0f;
		r->width = TRACK_HALF_WIDTH * 1.4f;
		r->height = i == 0 ? 3.2f : 2.2f;
	}

	/* Gravel/slick patches for grip testing. */
	const int slickIdx[3] = { 12, 44, 84 };
	for (int i = 0; i < 3; i++) {
		const int idx = slickIdx[i];
		if (t->slickCount >= TERRAIN_SLICK_MAX)
			break;
		F3DSTerrainSlick *s = &t->slicks[t->slickCount++];
		s->x = track->pts[idx].x;
		s->z = track->pts[idx].z;
		s->radius = 9.0f + 3.0f * (float)i;
	}
}

float terrain_height(const F3DSTerrain *t, float x, float z)
{
	for (int i = 0; i < t->rampCount; i++) {
		const F3DSTerrainRamp *r = &t->ramps[i];
		const float dx = x - r->origin.x;
		const float dz = z - r->origin.z;
		const float u = dx * r->fwd.x + dz * r->fwd.z;   /* along rise  */
		const float v = dx * r->right.x + dz * r->right.z; /* across     */
		if (u >= 0.0f && u <= r->length && fabsf(v) <= r->width * 0.5f)
			return r->height * (u / r->length);
	}
	return 0.0f;
}

float terrain_grip_factor(const F3DSTerrain *t, float x, float z)
{
	for (int i = 0; i < t->slickCount; i++) {
		const F3DSTerrainSlick *s = &t->slicks[i];
		const float dx = x - s->x;
		const float dz = z - s->z;
		if (dx * dx + dz * dz <= s->radius * s->radius)
			return 0.25f; /* gravel: quarter lateral grip */
	}
	return 1.0f;
}
