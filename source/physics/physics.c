#include "physics.h"

static FVec3 forward_of(float yaw)
{
	return fvec3(sinf(yaw), 0.0f, cosf(yaw));
}

/* Torque curve: flat plateau, linear falloff to 60 percent at max rpm. */
static float engine_torque(const F3DSVehicleDef *def, float rpm)
{
	const float t = clampf((rpm - def->torqueFallRpm) / (def->maxRpm - def->torqueFallRpm), 0.0f, 1.0f);
	return def->torqueBase * (1.0f - 0.4f * t);
}

void physics_vehicle_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in, float dt)
{
	const FVec3 fwd = forward_of(car->yaw);

	/* Decompose world velocity in the car frame. */
	float fwdVel = car->vx * fwd.x + car->vz * fwd.z;
	/* Lateral velocity kept in world space. */
	FVec3 latWorld = fvec3_sub(fvec3(car->vx, 0.0f, car->vz), fvec3_scale(fwd, fwdVel));

	/* --- Steering: progressive with inertia, reduced authority at speed. */
	const float speedFactor = 1.0f - clampf(fabsf(fwdVel) * def->steerSpeedDrop, 0.0f, 0.6f);
	const float steerTarget = in->steer * def->maxSteerAngle * speedFactor;
	const float steerRate = 4.0f * dt;
	car->steerAngle = approachf(car->steerAngle, steerTarget, steerRate);

	/* --- Powertrain: automatic gearbox + explicit reverse gear. */
	if (car->shiftTimer > 0.0f)
		car->shiftTimer -= dt;

	if (in->brake > 0.0f && fwdVel < 0.5f && car->gear != 0) {
		car->gear = 0;
		car->shiftTimer = def->shiftTime;
	} else if (in->throttle > 0.0f && fwdVel > -0.5f && car->gear == 0) {
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
		/* Reverse: rpm tracked from wheel speed, no drive torque. */
		const float reverseRatio = def->gearRatioR * def->finalDrive;
		car->rpm = clampf(-fwdVel / def->wheelRadius * reverseRatio * 30.0f / F3DS_PI, def->idleRpm, def->maxRpm);
	}

	/* RPM in forward gears: derived from wheel speed, clamped above idle. */
	if (car->gear != 0) {
		const float wheelRpm = fabsf(fwdVel) / def->wheelRadius * 60.0f / (2.0f * F3DS_PI);
		float target = wheelRpm * ratio;
		if (in->throttle > 0.0f && fwdVel < 0.5f)
			target = def->maxRpm * 0.25f; /* revving from standstill */
		target = clampf(target, def->idleRpm, def->maxRpm);
		car->rpm = approachf(car->rpm, target, 12000.0f * dt);
	} else if (fwdVel >= -0.2f) {
		const float target = clampf(def->idleRpm + in->throttle * def->maxRpm * 0.25f, def->idleRpm, def->maxRpm);
		car->rpm = approachf(car->rpm, target, 8000.0f * dt);
	}

	/* Longitudinal forces along the current heading. */
	float flong = 0.0f;
	if (car->shiftTimer <= 0.0f) {
		if (car->gear != 0 && in->throttle > 0.0f) {
			flong += engine_torque(def, car->rpm) * ratio * def->drivetrainEff / def->wheelRadius * in->throttle;
		} else if (car->gear == 0 && fwdVel > -25.0f) {
			/* In reverse gear the brake pedal propels backwards. */
			flong -= def->handbrakeForce * in->brake;
		}
	}

	/* Brakes always slow the car down (never reverse it). */
	if (car->gear != 0)
		flong -= def->brakeForce * in->brake * (fwdVel > 0.0f ? 1.0f : 0.0f);

	/* Handbrake: direct longitudinal force; low rear grip handled below. */
	if (in->handbrake && fabsf(fwdVel) > 0.1f)
		flong -= def->handbrakeForce * (fwdVel > 0.0f ? 1.0f : -1.0f);

	/* Aero drag and rolling resistance oppose motion. */
	flong -= def->drag * fwdVel * fabsf(fwdVel) + def->rollResist * fwdVel;

	fwdVel += (flong / def->mass) * dt;
	if (fabsf(fwdVel) < 0.05f && in->throttle < 0.01f)
		fwdVel = 0.0f;

	/* --- Yaw dynamics: kinematic target rate, inertia-limited blend.
	 * Handbrake cuts rear grip so the requested yaw rate exceeds grip,
	 * producing oversteer/drift instead of turning. */
	float yawTarget = 0.0f;
	if (fabsf(fwdVel) > 0.1f)
		yawTarget = fwdVel / def->wheelBase * tanf(car->steerAngle) * def->yawAuthority;
	if (in->handbrake && fabsf(fwdVel) > 2.0f)
		yawTarget *= 1.6f;

	const float yawBlend = (in->handbrake ? 4.0f : 8.0f) * dt;
	car->yawRate = approachf(car->yawRate, yawTarget, yawBlend * fabsf(yawTarget) + yawBlend * 0.3f);
	car->yaw += car->yawRate * dt;

	/* --- Lateral grip: world-space lateral velocity decays toward zero;
	 * lower rate with handbrake => sustained slides. */
	const float gripK = in->handbrake ? def->gripLatHandbrake : def->gripLat;
	const float latLen = fvec3_len(latWorld);
	if (latLen > 1e-4f) {
		const float decay = expf(-gripK * dt);
		latWorld = fvec3_scale(latWorld, decay);
		/* Cap the lateral slip implied by the yaw rate (physical limit). */
		const float maxLat = fabsf(car->yawRate * fwdVel) * 0.8f + 0.5f;
		const float newLen = fvec3_len(latWorld);
		if (newLen > maxLat)
			latWorld = fvec3_scale(latWorld, maxLat / newLen);
	} else {
		latWorld = fvec3(0.0f, 0.0f, 0.0f);
	}

	/* Rebuild world velocity from the NEW heading and updated components. */
	const FVec3 nfwd = forward_of(car->yaw);
	car->vx = nfwd.x * fwdVel + latWorld.x;
	car->vz = nfwd.z * fwdVel + latWorld.z;
	car->speed = fwdVel;

	/* Integrate position. */
	car->pos.x += car->vx * dt;
	car->pos.z += car->vz * dt;
}

void physics_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in, const F3DSTrack *track, float dt)
{
	physics_vehicle_step(def, car, in, dt);
	track_resolve_collision(track, &car->pos, &car->vx, &car->vz);
}
