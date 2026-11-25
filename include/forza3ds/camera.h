#ifndef F3DS_CAMERA_H
#define F3DS_CAMERA_H

#include "input.h"
#include "vehicle.h"

typedef enum {
	F3DS_CAM_CHASE = 0,
	F3DS_CAM_COCKPIT = 1
} F3DSCameraMode;

typedef struct {
	F3DSCameraMode mode;
	float chaseDistance;
	float chaseHeight;
	float orbitYaw;
	float orbitPitch;
	float recenterSpeed;
	FVec3 eye;
	FVec3 target;
} F3DSCamera;

void camera_init(F3DSCamera *cam);
void camera_update(F3DSCamera *cam, const F3DSVehicleState *car, const F3DSInputState *in, float dt);

#endif
