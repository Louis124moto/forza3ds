/* Forza3DS - small scenery of the island, derived on demand for one 256 m
 * sector from the details seed: trees, rocks, lamps, rails, parked cars...
 * and the obstacles a car can hit. Hardware independent. */
#ifndef F3DS_PROPS_H
#define F3DS_PROPS_H

#include "world.h"

#define PROPS_MAX 640
#define PROPS_RAIL_MAX 320
#define SCENERY_CACHE 6

typedef enum {
	F3DS_PROP_PINE = 0,
	F3DS_PROP_CEDAR,
	F3DS_PROP_SAKURA,
	F3DS_PROP_MAPLE,
	F3DS_PROP_BAMBOO,
	F3DS_PROP_PALM,
	F3DS_PROP_BUSH,
	F3DS_PROP_ROCK,
	F3DS_PROP_HAY,
	F3DS_PROP_LAMP,       /* yaw points the arm over the road */
	F3DS_PROP_POLE,
	F3DS_PROP_BOAT,
	F3DS_PROP_CAR,        /* variant: catalogue car */
	F3DS_PROP_COUNT
} F3DSPropKind;

typedef struct {
	float x, y, z, yaw, scale;
	uint8_t kind, variant;
	uint16_t reserved;
} F3DSProp;

typedef struct { float ax, ay, az, bx, by, bz; } F3DSRail;

typedef struct {
	int sx, sz;
	int propCount, railCount;
	F3DSProp props[PROPS_MAX];
	F3DSRail rails[PROPS_RAIL_MAX];
} F3DSSector;

typedef enum { F3DS_COLLIDER_CIRCLE = 0, F3DS_COLLIDER_BOX } F3DSColliderShape;

/* An upright obstacle between heights y0 and y1. A circle uses hx as radius;
 * a box has half extents hx (across) and hz (along its yaw). */
typedef struct {
	float x, z, hx, hz, yaw, y0, y1;
	uint8_t shape;
} F3DSCollider;

void props_build(const F3DSWorld *w, int sx, int sz, F3DSSector *out);
/* False for scenery a car drives through. */
bool props_collider(const F3DSProp *p, F3DSCollider *out);

/* A few recently used sectors, for the queries around the car. */
typedef struct {
	const F3DSWorld *world;
	F3DSSector sector[SCENERY_CACHE];
	uint32_t used[SCENERY_CACHE];
	uint32_t clock, builds;
} F3DSScenery;

void scenery_reset(F3DSScenery *s, const F3DSWorld *w);
const F3DSSector *scenery_sector(F3DSScenery *s, int sx, int sz);
/* Obstacles whose footprint may come within "radius" of (x, z). */
int scenery_colliders(F3DSScenery *s, float x, float z, float radius, F3DSCollider *out, int max);

#endif
