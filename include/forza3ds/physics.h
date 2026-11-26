/* Forza3DS - vehicle dynamics and collisions. Deterministic, hardware-free. */
#ifndef F3DS_PHYSICS_H
#define F3DS_PHYSICS_H

#include "input.h"
#include "terrain.h"
#include "track.h"
#include "vehicle.h"

/* Tire/powertrain/suspension integration only (no walls, no height). The
 * surface under each axle is sampled from track and terrain; passing NULL for
 * track and terrain selects flat asphalt everywhere. */
void physics_vehicle_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in,
	const F3DSTrack *track, const F3DSTerrain *terrain, float dt);
/* Full step: dynamics, ramps/jumps and collisions. */
void physics_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in,
	const F3DSTrack *track, const F3DSTerrain *terrain, float dt);

/* Push the car out of walls, obstacles and the coast. Returns true on contact. */
bool collision_resolve(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSTrack *track);

#endif
