#ifndef F3DS_HUD_H
#define F3DS_HUD_H

#include "camera.h"
#include "core.h"
#include "vehicle.h"

void hud_render(const F3DSVehicleState *car, const F3DSCamera *cam, const F3DSLoopState *loop);

#endif
