#ifndef F3DS_RENDERER_H
#define F3DS_RENDERER_H

#include <citro3d.h>
#include "camera.h"
#include "track.h"
#include "vehicle.h"

void renderer_init(void);
void renderer_exit(void);
void renderer_set_track(const F3DSTrack *track);
void renderer_render(const F3DSCamera *cam, const F3DSVehicleState *car, const F3DSTrack *track);

#endif
