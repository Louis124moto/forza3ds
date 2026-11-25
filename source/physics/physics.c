/* Vehicle dynamics: longitudinal powertrain, tire-limited forces, lateral slip,
 * differential influence and a compact sprung-mass approximation. */
#include "physics.h"
#include <string.h>

#define F3DS_GRAVITY 9.81f

static FVec3 forward_of(float yaw)
{
	return fvec3(sinf(yaw), 0.0f, cosf(yaw));
}

void physics_vehicle_step(const F3DSVehicleDef *def, F3DSVehicleState *car,
	const F3DSInputState *inRaw, const F3DSTerrain *terrain, float dt)
{
	const FVec3 fwd = forward_of(car->yaw);
	F3DSInputState inZeroed;
	if (car->airborne) {
		inZeroed = *inRaw;
		inZeroed.throttle = 0.0f;
		inZeroed.brake = 0.0f;
		inZeroed.handbrake = 0;
		inRaw = &inZeroed;
	}

	const float surfaceGrip = car->airborne ? 0.0f :
		terrain_grip_factor(terrain, car->pos.x, car->pos.z);
	const float tireGrip = fmaxf(def->tireGrip, 0.05f);
	float fwdVel = car->vx * fwd.x + car->vz * fwd.z;
	FVec3 latWorld = fvec3_sub(fvec3(car->vx, 0.0f, car->vz), fvec3_scale(fwd, fwdVel));

	/* Steering angle has finite response and loses authority as speed rises. */
	const float speedFactor = 1.0f - clampf(fabsf(fwdVel) * def->steerSpeedDrop, 0.0f, 0.62f);
	const float steerTarget = inRaw->steer * def->maxSteerAngle * speedFactor;
	car->steerAngle = approachf(car->steerAngle, steerTarget, 3.8f * dt);

	if (car->shiftTimer > 0.0f)
		car->shiftTimer = fmaxf(0.0f, car->shiftTimer - dt);

	/* Automatic transmission; zero is reverse. Gear count is tunable in the workshop. */
	if (inRaw->brake > 0.0f && fwdVel < 0.5f && car->gear != 0) {
		car->gear = 0;
		car->shiftTimer = def->shiftTime;
	} else if (inRaw->throttle > 0.0f && fwdVel > -0.5f && car->gear == 0) {
		car->gear = 1;
		car->shiftTimer = def->shiftTime;
	}

	int gearCount = def->gearCount;
	if (gearCount < 1) gearCount = 1;
	if (gearCount > F3DS_GEARS) gearCount = F3DS_GEARS;
	float ratio = 0.0f;
	if (car->gear > 0) {
		if (car->gear > gearCount) car->gear = gearCount;
		if (car->shiftTimer <= 0.0f) {
			if (car->rpm > def->shiftUpRpm && car->gear < gearCount) {
				car->gear++;
				car->shiftTimer = def->shiftTime;
			} else if (car->rpm < def->shiftDownRpm && car->gear > 1) {
				car->gear--;
				car->shiftTimer = def->shiftTime;
			}
		}
		ratio = def->gearRatios[car->gear - 1] * def->finalDrive;
	}

	if (car->gear > 0) {
		const float wheelRpm = fabsf(fwdVel) / fmaxf(def->wheelRadius, 0.05f) * 60.0f / (2.0f * F3DS_PI);
		float targetRpm = wheelRpm * ratio;
		if (inRaw->throttle > 0.0f && fwdVel < 0.5f)
			targetRpm = def->idleRpm + inRaw->throttle * (def->maxRpm - def->idleRpm) * 0.28f;
		targetRpm = clampf(targetRpm, def->idleRpm, fmaxf(def->maxRpm, def->idleRpm + 100.0f));
		car->rpm = approachf(car->rpm, targetRpm, 11000.0f * dt);
	} else if (fwdVel >= -0.2f) {
		const float targetRpm = def->idleRpm + inRaw->throttle *
			(fmaxf(def->maxRpm, def->idleRpm + 100.0f) - def->idleRpm) * 0.28f;
		car->rpm = approachf(car->rpm, targetRpm, 7600.0f * dt);
	}
	car->rpm = clampf(car->rpm, def->idleRpm, fmaxf(def->maxRpm, def->idleRpm + 100.0f));
	car->throttle = inRaw->throttle;
	car->engineLoad = clampf(inRaw->throttle + fabsf(fwdVel) * def->drag * 0.0007f, 0.0f, 1.0f);

	/* Engine parts, boost and gear ratios feed directly into wheel force. */
	float longitudinalForce = 0.0f;
	if (car->shiftTimer <= 0.0f && car->gear > 0 && inRaw->throttle > 0.0f) {
		const float wheelForce = vehicle_engine_torque(def, car->rpm) * ratio *
			def->drivetrainEff / fmaxf(def->wheelRadius, 0.05f) * inRaw->throttle;
		const float split = clampf(def->frontTorqueShare, 0.0f, 1.0f);
		const float driveShare = 1.0f - fabsf(split - 0.5f) * 0.16f;
		const float diffEffect = 1.0f + 0.12f * def->diffRearLock +
			0.08f * def->diffCenterLock + 0.04f * def->diffFrontLock;
		const float tractionLimit = def->mass * F3DS_GRAVITY * tireGrip *
			(0.72f + 0.18f * surfaceGrip) * diffEffect * driveShare;
		longitudinalForce += fminf(wheelForce, tractionLimit);
	}
	if (car->gear > 0)
		longitudinalForce -= def->brakeForce * inRaw->brake * (fwdVel > 0.0f ? 1.0f : 0.0f);
	else if (fwdVel > -25.0f)
		longitudinalForce -= def->handbrakeForce * inRaw->brake;
	if (inRaw->handbrake && fabsf(fwdVel) > 0.1f)
		longitudinalForce -= def->handbrakeForce * (fwdVel > 0.0f ? 1.0f : -1.0f);
	longitudinalForce -= def->drag * fwdVel * fabsf(fwdVel) + def->rollResist *
		(car->airborne ? 0.0f : 1.0f) * fwdVel;
	fwdVel += (longitudinalForce / fmaxf(def->mass, 1.0f)) * dt;
	if (fabsf(fwdVel) < 0.04f && inRaw->throttle < 0.01f)
		fwdVel = 0.0f;

	/* Sprung-mass roll approximation: spring rate and damping affect lateral grip. */
	const float lateralAcceleration = car->yawRate * fwdVel;
	const float spring = fmaxf(def->springRate, 1000.0f);
	const float cgLever = fmaxf(def->cgHeight + def->rideHeight, 0.1f);
	const float targetTravel = clampf(def->mass * lateralAcceleration / spring * cgLever * 0.34f, -0.16f, 0.16f);
	const float damping = targetTravel > car->suspensionTravel ?
		fmaxf(def->damperCompression, 0.0f) : fmaxf(def->damperRebound, 0.0f);
	const float suspensionRate = spring / fmaxf(def->mass, 1.0f) +
		damping / fmaxf(def->mass, 1.0f);
	car->suspensionTravel = approachf(car->suspensionTravel, targetTravel,
		fmaxf(0.02f, suspensionRate * dt * 0.12f));
	const float suspensionGrip = clampf(1.0f - fabsf(car->suspensionTravel) * 1.8f, 0.68f, 1.0f);

	/* Yaw response depends on wheelbase, tire grip and differential setup. */
	const float diffYaw = 1.0f + 0.18f * def->diffRearLock - 0.10f * def->diffFrontLock;
	float yawTarget = 0.0f;
	if (fabsf(fwdVel) > 0.1f)
		yawTarget = fwdVel / fmaxf(def->wheelBase, 0.5f) * tanf(car->steerAngle) *
			def->yawAuthority * diffYaw * (0.25f + 0.75f * surfaceGrip);
	if (inRaw->handbrake && fabsf(fwdVel) > 2.0f)
		yawTarget *= 1.65f;
	const float yawBlend = (inRaw->handbrake ? 4.0f : 7.5f) * dt;
	car->yawRate = approachf(car->yawRate, yawTarget,
		yawBlend * fabsf(yawTarget) + yawBlend * 0.25f);
	car->yaw += car->yawRate * dt;

	const float centerDiffGrip = 1.0f - 0.12f * def->diffCenterLock;
	const float gripK = (inRaw->handbrake ? def->gripLatHandbrake : def->gripLat) *
		tireGrip * suspensionGrip * centerDiffGrip * (0.25f + 0.75f * surfaceGrip);
	const float latLen = fvec3_len(latWorld);
	if (latLen > 1e-4f && gripK > 0.01f) {
		latWorld = fvec3_scale(latWorld, expf(-gripK * dt));
		const float maxLat = fabsf(car->yawRate * fwdVel) * 0.85f + 0.35f;
		const float newLen = fvec3_len(latWorld);
		if (newLen > maxLat)
			latWorld = fvec3_scale(latWorld, maxLat / newLen);
	}

	const FVec3 newForward = forward_of(car->yaw);
	car->vx = newForward.x * fwdVel + latWorld.x;
	car->vz = newForward.z * fwdVel + latWorld.z;
	car->speed = fwdVel;
	car->pos.x += car->vx * dt;
	car->pos.z += car->vz * dt;
}

void physics_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in,
	const F3DSTrack *track, const F3DSTerrain *terrain, float dt)
{
	physics_vehicle_step(def, car, in, terrain, dt);
	track_resolve_collision(track, &car->pos, &car->vx, &car->vz);

	const float ground = terrain_height(terrain, car->pos.x, car->pos.z);
	if (car->airborne) {
		car->vy -= F3DS_GRAVITY * dt;
		car->pos.y += car->vy * dt;
		if (car->pos.y <= ground) {
			car->pos.y = ground;
			const float impact = -car->vy;
			car->vy = 0.0f;
			car->airborne = false;
			const float scrub = clampf(1.0f - impact * 0.025f, 0.55f, 1.0f);
			car->vx *= scrub;
			car->vz *= scrub;
		}
	} else {
		const float dy = ground - car->pos.y;
		if (dy < -0.4f) {
			car->airborne = true;
			car->pos.y += car->vy * dt;
		} else {
			car->vy = dy / dt;
			car->pos.y = ground;
		}
	}
}
