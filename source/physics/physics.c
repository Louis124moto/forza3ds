/* Forza3DS - vehicle physics.
 * Planar model with longitudinal forces, inertia-limited steering, lateral
 * grip with slide/oversteer, gearbox, plus vertical dynamics over terrain
 * ramps (gravity, airborne state) and per-surface grip factors.
 */
#include "physics.h"
#include <string.h>

#define F3DS_GRAVITY 24.0f /* slightly arcade-tuned gravity, m/s^2 */

static FVec3 forward_of(float yaw)
{
	return fvec3(sinf(yaw), 0.0f, cosf(yaw));
}

static float engine_torque(const F3DSVehicleDef *def, float rpm)
{
	const float t = clampf((rpm - def->torqueFallRpm) / (def->maxRpm - def->torqueFallRpm), 0.0f, 1.0f);
	return def->torqueBase * (1.0f - 0.4f * t);
}

void physics_vehicle_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *inRaw, const F3DSTerrain *terrain, float dt)
{
	const FVec3 fwd = forward_of(car->yaw);

	/* While airborne there is no drive, brake or grip authority. */
	F3DSInputState inZeroed;
	if (car->airborne) {
		inZeroed = *inRaw;
		inZeroed.throttle = 0.0f;
		inZeroed.brake = 0.0f;
		inZeroed.handbrake = 0;
		inRaw = &inZeroed;
	}

	/* Surface grip (gravel zones etc). */
	const float gripFactor = car->airborne ? 0.0f : terrain_grip_factor(terrain, car->pos.x, car->pos.z);

	float fwdVel = car->vx * fwd.x + car->vz * fwd.z;
	FVec3 latWorld = fvec3_sub(fvec3(car->vx, 0.0f, car->vz), fvec3_scale(fwd, fwdVel));

	/* --- Steering: progressive with inertia, reduced authority at speed. */
	const float speedFactor = 1.0f - clampf(fabsf(fwdVel) * def->steerSpeedDrop, 0.0f, 0.6f);
	const float steerTarget = inRaw->steer * def->maxSteerAngle * speedFactor;
	const float steerRate = 4.0f * dt;
	car->steerAngle = approachf(car->steerAngle, steerTarget, steerRate);

	/* --- Powertrain: automatic gearbox + explicit reverse gear. */
	if (car->shiftTimer > 0.0f)
		car->shiftTimer -= dt;

	if (inRaw->brake > 0.0f && fwdVel < 0.5f && car->gear != 0) {
		car->gear = 0;
		car->shiftTimer = def->shiftTime;
	} else if (inRaw->throttle > 0.0f && fwdVel > -0.5f && car->gear == 0) {
		car->gear = 1;
		car->shiftTimer = def->shiftTime;
	}

	float ratio = 0.0f;
	if (car->gear != 0) {
		if (car->shiftTimer <= 0.0f) {
			if (car->rpm > def->shiftUpRpm && car->gear < F3DS_GEARS) {
				car->gear++;
				car->shiftTimer = def->shiftTime;
			} else if (car->rpm < def->shiftDownRpm && car->gear > 1) {
				car->gear--;
				car->shiftTimer = def->shiftTime;
			}
		}
		ratio = def->gearRatios[car->gear - 1] * def->finalDrive;
	} else if (fwdVel < -0.2f) {
		const float reverseRatio = def->gearRatioR * def->finalDrive;
		car->rpm = clampf(-fwdVel / def->wheelRadius * reverseRatio * 30.0f / F3DS_PI, def->idleRpm, def->maxRpm);
	}

	if (car->gear != 0) {
		const float wheelRpm = fabsf(fwdVel) / def->wheelRadius * 60.0f / (2.0f * F3DS_PI);
		float target = wheelRpm * ratio;
		if (inRaw->throttle > 0.0f && fwdVel < 0.5f)
			target = def->maxRpm * 0.25f;
		target = clampf(target, def->idleRpm, def->maxRpm);
		car->rpm = approachf(car->rpm, target, 12000.0f * dt);
	} else if (fwdVel >= -0.2f) {
		const float target = clampf(def->idleRpm + inRaw->throttle * def->maxRpm * 0.25f, def->idleRpm, def->maxRpm);
		car->rpm = approachf(car->rpm, target, 8000.0f * dt);
	}

	/* Longitudinal forces. */
	float flong = 0.0f;
	if (car->shiftTimer <= 0.0f) {
		if (car->gear != 0 && inRaw->throttle > 0.0f) {
			flong += engine_torque(def, car->rpm) * ratio * def->drivetrainEff / def->wheelRadius * inRaw->throttle;
		} else if (car->gear == 0 && fwdVel > -25.0f) {
			flong -= def->handbrakeForce * inRaw->brake;
		}
	}
	if (car->gear != 0)
		flong -= def->brakeForce * inRaw->brake * (fwdVel > 0.0f ? 1.0f : 0.0f);
	if (inRaw->handbrake && fabsf(fwdVel) > 0.1f)
		flong -= def->handbrakeForce * (fwdVel > 0.0f ? 1.0f : -1.0f);
	flong -= def->drag * fwdVel * fabsf(fwdVel) + def->rollResist * (car->airborne ? 0.0f : 1.0f) * fwdVel;

	fwdVel += (flong / def->mass) * dt;
	if (fabsf(fwdVel) < 0.05f && inRaw->throttle < 0.01f)
		fwdVel = 0.0f;

	/* --- Yaw dynamics. */
	float yawTarget = 0.0f;
	if (fabsf(fwdVel) > 0.1f)
		yawTarget = fwdVel / def->wheelBase * tanf(car->steerAngle) * def->yawAuthority * (0.35f + 0.65f * gripFactor);
	if (inRaw->handbrake && fabsf(fwdVel) > 2.0f)
		yawTarget *= 1.6f;

	const float yawBlend = (inRaw->handbrake ? 4.0f : 8.0f) * dt;
	car->yawRate = approachf(car->yawRate, yawTarget, yawBlend * fabsf(yawTarget) + yawBlend * 0.3f);
	car->yaw += car->yawRate * dt;

	/* --- Lateral grip with surface factor. */
	const float gripK = (inRaw->handbrake ? def->gripLatHandbrake : def->gripLat) * (0.3f + 0.7f * gripFactor);
	const float latLen = fvec3_len(latWorld);
	if (latLen > 1e-4f && gripK > 0.01f) {
		const float decay = expf(-gripK * dt);
		latWorld = fvec3_scale(latWorld, decay);
		const float maxLat = fabsf(car->yawRate * fwdVel) * 0.8f + 0.5f;
		const float newLen = fvec3_len(latWorld);
		if (newLen > maxLat)
			latWorld = fvec3_scale(latWorld, maxLat / newLen);
	} else if (gripK <= 0.01f) {
		/* airborne / ice: lateral velocity persists (free slide). */
	}

	const FVec3 nfwd = forward_of(car->yaw);
	car->vx = nfwd.x * fwdVel + latWorld.x;
	car->vz = nfwd.z * fwdVel + latWorld.z;
	car->speed = fwdVel;

	car->pos.x += car->vx * dt;
	car->pos.z += car->vz * dt;
}

void physics_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in,
		  const F3DSTrack *track, const F3DSTerrain *terrain, float dt)
{
	physics_vehicle_step(def, car, in, terrain, dt);
	track_resolve_collision(track, &car->pos, &car->vx, &car->vz);

	/* --- Vertical dynamics over terrain ramps. */
	const float ground = terrain_height(terrain, car->pos.x, car->pos.z);
	if (car->airborne) {
		car->vy -= F3DS_GRAVITY * dt;
		car->pos.y += car->vy * dt;
		if (car->pos.y <= ground) {
			car->pos.y = ground;
			/* Landing scrub proportional to impact speed. */
			const float impact = -car->vy;
			car->vy = 0.0f;
			car->airborne = false;
			const float scrub = clampf(1.0f - impact * 0.03f, 0.6f, 1.0f);
			car->vx *= scrub;
			car->vz *= scrub;
		}
	} else {
		const float g = ground;
		const float dy = g - car->pos.y;
		if (dy < -0.4f) {
			/* Ground vanished below us (drove off the ramp edge):
			 * keep the climb velocity and go airborne. */
			car->airborne = true;
			car->pos.y += car->vy * dt;
		} else {
			car->vy = dy / dt;
			car->pos.y = g;
		}
	}
}
