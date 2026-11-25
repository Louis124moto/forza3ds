#ifndef F3DS_MESHES_H
#define F3DS_MESHES_H

#include <3ds.h>
#include <stdint.h>
#include "track.h"

typedef struct {
	float x, y, z;
	float r, g, b, a;
} F3DSVertex;

typedef struct {
	F3DSVertex *data;
	uint32_t total;
	uint32_t groundStart, groundCount;
	uint32_t roadStart, roadCount;
	uint32_t obstacleStart, obstacleCount;
	uint32_t decorStart, decorCount;
	uint32_t carStart, carCount;
	uint32_t cockpitStart, cockpitCount;
} F3DSMeshes;

void meshes_build(F3DSMeshes *m, const F3DSTrack *track);
void meshes_free(F3DSMeshes *m);

#endif
