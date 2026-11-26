#include "camera.h"
#include <string.h>

#define ORBIT_RATE 2.4f      /* rad/s at full C-stick deflection */
#define STOPPED_SPEED 0.6f

void camera_init(F3DSCamera *cam)
{
	memset(cam, 0, sizeof(*cam));
	cam->mode = F3DS_CAM_CHASE;
	cam->chaseDistance = 7.5f;
	cam->chaseHeight = 2.8f;
	cam->recenterSpeed = 2.5f;
	cam->wasStopped = true;
	cam->eye = fvec3(0.0f, 5.0f, -10.0f);
}

void camera_update(F3DSCamera *cam, const F3DSCarVisual *car, FVec3 pos, float yaw, float speed,
	const F3DSInputState *in, float dt)
{
	if (in->toggleView)
		cam->mode = cam->mode == F3DS_CAM_CHASE ? F3DS_CAM_COCKPIT : F3DS_CAM_CHASE;
	cam->chaseDistance = clampf(cam->chaseDistance + in->camDistance * 5.0f * dt, 3.0f, 20.0f);
	cam->chaseHeight = clampf(cam->chaseHeight + in->camHeight * 2.5f * dt, 1.0f, 12.0f);

	/* C-stick orbit. Recentring starts when the stick is released while
	 * driving, or when the car sets off again after a stop; a stationary car
	 * keeps the chosen angle for inspection. */
	const bool stopped = fabsf(speed) < STOPPED_SPEED;
	const bool restarting = cam->wasStopped && !stopped;
	cam->wasStopped = stopped;
	if (fabsf(in->orbit) > 0.02f) {
		cam->orbitYaw = wrap_angle(cam->orbitYaw - in->orbit * ORBIT_RATE * dt);
		cam->recentering = !stopped;
	} else {
		if (!stopped || restarting)
			cam->recentering = true;
		if (cam->recentering) {
			cam->orbitYaw -= cam->orbitYaw * clampf(cam->recenterSpeed * dt, 0.0f, 1.0f);
			if (fabsf(cam->orbitYaw) < 0.002f) {
				cam->orbitYaw = 0.0f;
				cam->recentering = false;
			}
		}
	}

	const FVec3 f = yaw_forward(yaw), l = yaw_left(yaw);
	if (cam->mode == F3DS_CAM_CHASE) {
		const float a = yaw + F3DS_PI + cam->orbitYaw;
		cam->eye = fvec3(pos.x + sinf(a) * cam->chaseDistance, pos.y + cam->chaseHeight,
			pos.z + cosf(a) * cam->chaseDistance);
		const float ahead = 2.5f * cosf(cam->orbitYaw);
		cam->target = fvec3(pos.x + f.x * ahead, pos.y + 0.9f, pos.z + f.z * ahead);
	} else {
		/* Cockpit: fixed standard position at the driver's eyes. */
		cam->eye = fvec3(pos.x + f.x * car->eyeZ + l.x * car->seatX, pos.y + car->eyeY,
			pos.z + f.z * car->eyeZ + l.z * car->seatX);
		cam->target = fvec3(cam->eye.x + f.x * 10.0f, cam->eye.y - 1.1f, cam->eye.z + f.z * 10.0f);
	}
}
