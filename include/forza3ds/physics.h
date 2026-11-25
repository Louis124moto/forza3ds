#ifndef F3DS_PHYSICS_H
#define F3DS_PHYSICS_H

#include "input.h"
#include "track.h"
#include "vehicle.h"

void physics_vehicle_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in, float dt);
void physics_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in, const F3DSTrack *track, float dt);

#endif
