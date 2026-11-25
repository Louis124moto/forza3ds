#include "camera.h"

void camera_init(F3DSCamera *cam)
{
	cam->mode = F3DS_CAM_CHASE;
	cam->chaseDistance = 7.5f;
	cam->chaseHeight = 3.2f;
	cam->orbitYaw = 0.0f;
	cam->orbitPitch = 0.35f;
	cam->recenterSpeed = 2.5f;
	cam->eye = fvec3(0.0f, 5.0f, -10.0f);
	cam->target = fvec3(0.0f, 0.0f, 0.0f);
}

void camera_update(F3DSCamera *cam, const F3DSVehicleState *car, const F3DSInputState *in, float dt)
{
	/* One-shot actions */
	if (in->toggleView)
		cam->mode = (cam->mode == F3DS_CAM_CHASE) ? F3DS_CAM_COCKPIT : F3DS_CAM_CHASE;

	/* Adjustable distance (D-pad up/down) and height (D-pad left/right). */
	if (in->camDistanceDown)
		cam->chaseDistance = clampf(cam->chaseDistance + 10.0f * dt, 3.0f, 20.0f);
	if (in->camDistanceUp)
		cam->chaseDistance = clampf(cam->chaseDistance - 10.0f * dt, 3.0f, 20.0f);
	if (in->camHeightUp)
		cam->chaseHeight = clampf(cam->chaseHeight + 6.0f * dt, 1.0f, 12.0f);
	if (in->camHeightDown)
		cam->chaseHeight = clampf(cam->chaseHeight - 6.0f * dt, 1.0f, 12.0f);

	/* C-stick orbit. */
	cam->orbitYaw -= in->orbitYaw * 12.0f * dt;
	cam->orbitPitch = clampf(cam->orbitPitch + in->orbitPitch * 8.0f * dt, -0.15f, 1.1f);

	/* Automatic recentering after C-stick release; no free vertical movement. */
	if (fabsf(in->orbitYaw) <= 0.02f) {
		const float speed = clampf(cam->recenterSpeed * dt, 0.0f, 1.0f);
		cam->orbitYaw -= cam->orbitYaw * speed * 1.5f;
		cam->orbitPitch = approachf(cam->orbitPitch, 0.35f, cam->recenterSpeed * dt * 0.25f);
	}

	const FVec3 fwd = fvec3(sinf(car->yaw), 0.0f, cosf(car->yaw));

	if (cam->mode == F3DS_CAM_CHASE) {
		const float a = car->yaw + F3DS_PI + cam->orbitYaw;
		const float horiz = cosf(cam->orbitPitch) * cam->chaseDistance;
		const float vert = cam->chaseHeight + sinf(cam->orbitPitch) * cam->chaseDistance * 0.6f;
		cam->eye = fvec3(car->pos.x + sinf(a) * horiz, car->pos.y + vert, car->pos.z + cosf(a) * horiz);
		cam->target = fvec3(car->pos.x + fwd.x * 3.0f, car->pos.y + 1.0f, car->pos.z + fwd.z * 3.0f);
	} else {
		/* Cockpit: fixed standard position; dashboard drawn in cockpit view. */
		const FVec3 seat = fvec3(car->pos.x + fwd.x * 0.15f, car->pos.y + 1.05f, car->pos.z + fwd.z * 0.15f);
		cam->eye = seat;
		cam->target = fvec3(seat.x + fwd.x * 10.0f, seat.y - 0.3f, seat.z + fwd.z * 10.0f);
	}
}
