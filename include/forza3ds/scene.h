/* Forza3DS - turns the application view into an ordered draw list. Uses the
 * citro3d maths library only, so it also runs in host tests. */
#ifndef F3DS_SCENE_H
#define F3DS_SCENE_H

#include <c3d/maths.h>
#include "app.h"
#include "meshes.h"

#define F3DS_SCENE_MAX_DRAWS 128
#define F3DS_NEAR_PLANE 0.20f
#define F3DS_FAR_PLANE 900.0f
#define F3DS_FOV_Y (60.0f * F3DS_PI / 180.0f)

typedef struct {
	F3DSRange range;
	C3D_Mtx mvp;      /* projection * view * model */
	float tint[3];
} F3DSDraw;

typedef struct {
	F3DSDraw draws[F3DS_SCENE_MAX_DRAWS];
	int count;
	uint32_t vertices;
	uint32_t clearColor; /* RGBA8 */
} F3DSScene;

void scene_build(F3DSScene *scene, const F3DSMeshes *meshes, const F3DSView *view);

#endif
