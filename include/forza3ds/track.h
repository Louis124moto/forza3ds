#ifndef F3DS_TRACK_H
#define F3DS_TRACK_H

#include "common.h"

#define TRACK_POINTS 96
#define TRACK_HALF_WIDTH 6.0f
#define TRACK_OBSTACLE_MAX 24
#define TRACK_WALL_MAX 64
#define TRACK_TREE_MAX 220
#define TRACK_TRUNK_RADIUS 0.35f
#define TRACK_WALL_HEIGHT 1.0f
#define TRACK_WALL_HALF_THICKNESS 0.3f

typedef struct {
	FVec3 center;
	float hx, hz;
	float height; /* total height above the ground */
	int kind;     /* F3DSObstacleKind */
} F3DSObstacle;

typedef enum {
	F3DS_OBSTACLE_POST = 0,
	F3DS_OBSTACLE_CRATE,
	F3DS_OBSTACLE_TORII
} F3DSObstacleKind;

typedef struct {
	float x, z;
	float height, crown;
	int blossom; /* cherry tree instead of cedar */
} F3DSTree;

/* Barrier: a thick segment between a and b (ground level). */
typedef struct {
	FVec3 a, b;
} F3DSWall;

typedef struct {
	FVec3 pts[TRACK_POINTS + 1];
	FVec3 fwd[TRACK_POINTS];
	FVec3 right[TRACK_POINTS];
	FVec3 jointRight[TRACK_POINTS + 1]; /* mitred side vector at each point */
	float segLen[TRACK_POINTS];
	F3DSObstacle obstacles[TRACK_OBSTACLE_MAX];
	int obstacleCount;
	F3DSWall walls[TRACK_WALL_MAX];
	int wallCount;
	F3DSTree trees[TRACK_TREE_MAX];
	int treeCount;
} F3DSTrack;

void track_init(F3DSTrack *t);
/* Distance from p to the road centre line. */
float track_nearest(const F3DSTrack *t, FVec3 p, int *seg, float *s);
/* Radius of the drivable island in the direction of (x, z). */
float track_coast_radius(float x, float z);

#endif
