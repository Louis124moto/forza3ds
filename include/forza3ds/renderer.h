/* Forza3DS - Citro3D renderer */
#ifndef F3DS_RENDERER_H
#define F3DS_RENDERER_H

#include <citro3d.h>
#include "camera.h"
#include "terrain.h"
#include "track.h"
#include "vehicle.h"

void renderer_init(void);
void renderer_exit(void);
void renderer_set_track(const F3DSTrack *track, const F3DSTerrain *terrain);
void renderer_set_garage(bool enabled);
void renderer_render(const F3DSCamera *cam, const F3DSVehicleState *car);

#endif
