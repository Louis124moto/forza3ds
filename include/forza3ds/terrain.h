/* Forza3DS - demo island surface features: ramps and gravel zones. */
#ifndef F3DS_TERRAIN_H
#define F3DS_TERRAIN_H

#include "track.h"
#include "vehicle.h"

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

void terrain_init(F3DSTerrain *t, const F3DSTrack *track);
float terrain_height(const F3DSTerrain *t, float x, float z);
F3DSSurface terrain_surface(const F3DSTerrain *t, const F3DSTrack *track, float x, float z);

#endif
