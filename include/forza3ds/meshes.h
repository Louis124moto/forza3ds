/* Forza3DS - procedurally generated prototype meshes (builtin fallback assets) */
#ifndef F3DS_MESHES_H
#define F3DS_MESHES_H

#include <3ds.h>
#include <stdint.h>
#include "terrain.h"
#include "track.h"

typedef struct {
	float x, y, z;
	float r, g, b, a;
	float u, v;
} F3DSVertex;

typedef struct {
	F3DSVertex *data;
	uint32_t total;
	uint32_t oceanStart, oceanCount;
	uint32_t islandStart, islandCount;
	uint32_t beachStart, beachCount;
	uint32_t forestStart, forestCount;
	uint32_t roadStart, roadCount;
	uint32_t roadLinesStart, roadLinesCount;
	uint32_t obstacleStart, obstacleCount;
	uint32_t decorStart, decorCount;
	uint32_t terrainStart, terrainCount;
	uint32_t gravelStart, gravelCount;
	uint32_t carStart, carCount;
	uint32_t cockpitStart, cockpitCount;
	uint32_t garageStart, garageCount;
} F3DSMeshes;

void meshes_build(F3DSMeshes *m, const F3DSTrack *track, const F3DSTerrain *terrain);
void meshes_free(F3DSMeshes *m);

#endif
