/* Forza3DS - island demo surface features: ramps and low-grip surface zones.
 * Used to exercise the physics (jumps, airborne state, surface grip) while
 * the procedural world (stage 7) is not implemented yet. */
#ifndef F3DS_TERRAIN_H
#define F3DS_TERRAIN_H

#include "common.h"
#include "track.h"

#define TERRAIN_RAMP_MAX 6
#define TERRAIN_SLICK_MAX 8

typedef struct {
	FVec3 origin; /* start of the ramp, on the ground */
	FVec3 fwd;    /* rise direction (unit) */
	FVec3 right;  /* across direction (unit) */
	float length;
	float width;
	float height;
} F3DSTerrainRamp;

typedef struct {
	float x, z;
	float radius;
} F3DSTerrainSlick;

typedef struct {
	F3DSTerrainRamp ramps[TERRAIN_RAMP_MAX];
	int rampCount;
	F3DSTerrainSlick slicks[TERRAIN_SLICK_MAX];
	int slickCount;
} F3DSTerrain;

/* Build the fixed demo-island surface features. */
void terrain_init(F3DSTerrain *t, const F3DSTrack *track);

/* Ground height at a world position (0 outside features). */
float terrain_height(const F3DSTerrain *t, float x, float z);

/* Lateral grip multiplier at a world position (1.0 = full asphalt grip,
 * lower on gravel/slick zones). */
float terrain_grip_factor(const F3DSTerrain *t, float x, float z);

#endif
