/* Forza3DS - built-in low-poly geometry. Hardware independent: the caller
 * provides the vertex buffer (linear memory on the console). */
#ifndef F3DS_MESHES_H
#define F3DS_MESHES_H

#include "catalog.h"
#include "terrain.h"
#include "track.h"

#define F3DS_MESH_CAPACITY 90000u
#define F3DS_CHUNK_AXIS 8
#define F3DS_CHUNK_COUNT (F3DS_CHUNK_AXIS * F3DS_CHUNK_AXIS)

typedef struct {
	float x, y, z;
	uint8_t r, g, b, a;
} F3DSVertex;

typedef struct {
	uint32_t start, count;
} F3DSRange;

/* A square of the world, culled as a whole. */
typedef struct {
	F3DSRange range;
	float cx, cz, radius;
} F3DSChunk;

/* Car local frame: +Z forward, +X left, origin on the ground under the middle
 * of the wheelbase. "paint" ranges are tinted with the body colour. */
typedef struct {
	F3DSRange paint, trim;
	F3DSRange cockpitPaint, cockpitTrim;
	F3DSRange steering;    /* wheel centred on the origin, facing -Z */
} F3DSCarMesh;

typedef struct {
	F3DSVertex *data;
	uint32_t count, capacity;
	bool overflow;
	F3DSRange sea, horizon, garage;
	F3DSChunk chunks[F3DS_CHUNK_COUNT];
	F3DSCarMesh cars[F3DS_CAR_COUNT];
	F3DSRange tyre, rim;   /* unit radius and half width, axle along X */
	F3DSRange needle;      /* unit length along +Y, facing -Z */
	F3DSRange shadow;      /* unit half extents in X and Z */
} F3DSMeshes;

/* Cockpit instrument placement relative to the driver's eye. */
#define F3DS_DIAL_OFFSET_X 0.075f
#define F3DS_DIAL_Y (-0.295f)
#define F3DS_DIAL_Z 0.505f
#define F3DS_DIAL_RADIUS 0.062f
#define F3DS_STEERING_Y (-0.34f)
#define F3DS_STEERING_Z 0.45f
#define F3DS_STEERING_TILT 0.30f

/* Returns false when the buffer is too small. */
bool meshes_build(F3DSMeshes *m, F3DSVertex *buffer, uint32_t capacity,
	const F3DSTrack *track, const F3DSTerrain *terrain);

#endif
