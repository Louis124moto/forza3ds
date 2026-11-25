#ifndef F3DS_TRACK_H
#define F3DS_TRACK_H

#include "common.h"

#define TRACK_POINTS 96
#define TRACK_HALF_WIDTH 6.0f
#define TRACK_OBSTACLE_MAX 24

typedef struct {
	FVec3 pts[TRACK_POINTS + 1];
	FVec3 fwd[TRACK_POINTS];
	FVec3 right[TRACK_POINTS];
	float segLen[TRACK_POINTS];

	struct {
		FVec3 center;
		float hx, hz;
		float height;
	} obstacles[TRACK_OBSTACLE_MAX];
	int obstacleCount;
} F3DSTrack;

void track_init(F3DSTrack *t);
float track_nearest(const F3DSTrack *t, FVec3 p, int *seg, float *s);
int track_resolve_collision(const F3DSTrack *t, FVec3 *pos, float *vx, float *vz);

#endif
