/* Two-axle vehicle model with four wheel loads.
 *
 * Longitudinal and lateral load transfer come from sprung-mass pitch and roll
 * (spring and damper per axle). Each axle has a tire capacity that depends on
 * tire type, surface, speed and wheel loads; drive, brake and cornering forces
 * share that capacity (friction ellipse). Differential locking decides how much
 * of an axle's capacity the drive can use. Below walking pace the model blends
 * into a kinematic bicycle so it stays stable at rest. */
#include "physics.h"

#define SUBSTEP_MAX (1.0f / 240.0f)
#define REFERENCE_RIDE 0.14f

typedef struct {
	float capacity;   /* N, whole axle */
	float weakWheel;  /* N, capacity of the less loaded wheel */
	float peakSlip;   /* rad */
} Axle;

/* Largest suspension compression caused by body roll and pitch, metres. */
static float spring_travel(const F3DSVehicleDef *d, const F3DSVehicleState *s)
{
	return fabsf(s->roll) * d->trackWidth * 0.5f + fabsf(s->pitch) * d->wheelBase * 0.5f;
}

/* Unconditionally stable implicit update of I a'' = M - K a - C a'. */
static void sprung_axis(float *angle, float *rate, float moment, float stiffness,
	float damping, float inertia, float dt)
{
	*rate = (*rate + dt * (moment - stiffness * *angle) / inertia) /
		(1.0f + dt * damping / inertia + dt * dt * stiffness / inertia);
	*angle += dt * *rate;
}

static void update_loads(const F3DSVehicleDef *d, F3DSVehicleState *s, float dt)
{
	const float L = d->wheelBase, W = d->trackWidth;
	const float h = d->cgHeight +
		0.5f * (d->rideHeight[0] + d->rideHeight[1]) - REFERENCE_RIDE;

	/* Pitch: positive = nose up (load moves to the rear). */
	const float kf = 2.0f * d->springRate[F3DS_FRONT], kr = 2.0f * d->springRate[F3DS_REAR];
	const float kPitch = (kf + kr) * L * L * 0.25f;
	float cPitch = 0.0f;
	for (int a = 0; a < 2; ++a) {
		const bool compressing = a == F3DS_FRONT ? s->pitchRate < 0.0f : s->pitchRate > 0.0f;
		const float c = compressing ? d->damperCompression[a] : d->damperRebound[a];
		cPitch += 2.0f * c * L * L * 0.25f;
	}
	sprung_axis(&s->pitch, &s->pitchRate, d->mass * s->accelLong * h, kPitch, cPitch,
		d->mass * L * L / 6.0f, dt);

	/* Roll: positive = leaning right (left turn, positive lateral accel). */
	float kRoll[2], cRoll[2];
	for (int a = 0; a < 2; ++a) {
		kRoll[a] = d->springRate[a] * W * W * 0.5f;
		/* The right side compresses when roll rate is positive. */
		cRoll[a] = 0.5f * (d->damperCompression[a] + d->damperRebound[a]) * W * W * 0.5f;
	}
	sprung_axis(&s->roll, &s->rollRate, d->mass * s->accelLat * h, kRoll[0] + kRoll[1],
		cRoll[0] + cRoll[1], d->mass * W * W / 5.0f, dt);

	const float weight = d->mass * F3DS_GRAVITY;
	/* An axle can at most carry the whole car. */
	const float longShift = clampf((kPitch * s->pitch + cPitch * s->pitchRate) / L, /* + = rearwards */
		-weight * (1.0f - d->weightFront), weight * d->weightFront);
	float axleLoad[2] = {
		weight * d->weightFront - longShift,
		weight * (1.0f - d->weightFront) + longShift,
	};
	for (int a = 0; a < 2; ++a) {
		axleLoad[a] = fmaxf(axleLoad[a], 0.0f);
		float latShift = (kRoll[a] * s->roll + cRoll[a] * s->rollRate) / W; /* + = to the right */
		latShift = clampf(latShift, -axleLoad[a] * 0.5f, axleLoad[a] * 0.5f);
		s->wheelLoad[a * 2 + 0] = axleLoad[a] * 0.5f - latShift; /* left */
		s->wheelLoad[a * 2 + 1] = axleLoad[a] * 0.5f + latShift; /* right */
	}
}

static Axle axle_state(const F3DSVehicleDef *d, const F3DSVehicleState *s, int a,
	F3DSSurface surface, float speed)
{
	const float share = a == F3DS_FRONT ? d->weightFront : 1.0f - d->weightFront;
	const float nominal = d->mass * F3DS_GRAVITY * share * 0.5f;
	float mu = tire_surface_grip(d->tireType, surface) * fmaxf(d->tireGrip, 0.05f);
	/* Slightly wider rear tires: the balance at the limit is mild understeer. */
	if (a == F3DS_REAR)
		mu *= 1.08f;
	/* Grip falls gently with speed. */
	mu *= 1.0f - clampf((fabsf(speed) - 15.0f) * 0.0022f, 0.0f, 0.16f);
	/* Bottoming out on the bump stops costs grip. */
	const float over = spring_travel(d, s) - d->rideHeight[a] * 0.75f;
	if (over > 0.0f)
		mu *= clampf(1.0f - over * 4.0f, 0.65f, 1.0f);

	float wheel[2];
	for (int w = 0; w < 2; ++w) {
		const float load = s->wheelLoad[a * 2 + w];
		/* Load sensitivity: a heavily loaded tire is less efficient. */
		const float eff = clampf(1.0f - 0.12f * (load / nominal - 1.0f), 0.6f, 1.15f);
		wheel[w] = mu * eff * load;
	}
	Axle out = { wheel[0] + wheel[1], fminf(wheel[0], wheel[1]), tire_peak_slip(d->tireType) };
	return out;
}

/* Normalised lateral force: 1 at the peak slip angle, ~0.85 when sliding. */
static float lateral_curve(float slip, float peak)
{
	return sinf(1.35f * atanf(2.33f * slip / peak));
}

static float gear_ratio(const F3DSVehicleDef *d, int gear)
{
	if (gear <= 0)
		return fabsf(d->gearRatioR) * d->finalDrive;
	return d->gearRatios[gear - 1] * d->finalDrive;
}

static float wheel_rpm(const F3DSVehicleDef *d, float speed, int gear)
{
	return fabsf(speed) / d->wheelRadius * 60.0f / (2.0f * F3DS_PI) * gear_ratio(d, gear);
}

static void engage(const F3DSVehicleDef *d, F3DSVehicleState *s, int gear)
{
	s->gear = gear;
	s->shiftTimer = d->shiftTime;
}

/* Gear selection. Returns the accelerator and brake as seen by the wheels. */
static void transmission(const F3DSVehicleDef *d, F3DSVehicleState *s, const F3DSInputState *in,
	float u, float *drivePedal, float *brakePedal)
{
	if (s->gear > d->gearCount) s->gear = d->gearCount;
	if (s->gear < 0) s->gear = 0;

	if (d->manualTransmission) {
		if (s->shiftTimer <= 0.0f) {
			if (in->gearUp && s->gear < d->gearCount) {
				if (s->gear > 0 || u > -1.0f) engage(d, s, s->gear + 1);
			} else if (in->gearDown) {
				if (s->gear > 1) engage(d, s, s->gear - 1);
				else if (s->gear == 1 && u < 1.0f) engage(d, s, 0);
			}
		}
		*drivePedal = in->throttle;
		*brakePedal = in->brake;
		return;
	}

	/* Automatic: holding the brake at rest selects reverse; in reverse the
	 * brake pedal drives backwards and the accelerator brakes. */
	if (s->gear > 0 && in->brake > 0.0f && in->throttle <= 0.0f && u < 0.5f)
		engage(d, s, 0);
	else if (s->gear == 0 && in->throttle > 0.0f && in->brake <= 0.0f && u > -0.5f)
		engage(d, s, 1);
	if (s->gear == 0) {
		*drivePedal = in->brake;
		*brakePedal = in->throttle;
		return;
	}
	*drivePedal = in->throttle;
	*brakePedal = in->brake;

	if (s->shiftTimer > 0.0f || s->airborne)
		return;
	const float rpm = wheel_rpm(d, u, s->gear);
	const float upAt = d->idleRpm + (d->maxRpm - d->idleRpm) * (0.55f + 0.38f * in->throttle);
	const float downAt = d->idleRpm + (d->maxRpm - d->idleRpm) * (0.14f + 0.22f * in->throttle);
	if (s->gear < d->gearCount && rpm >= upAt && s->wheelSpin < 0.1f) {
		engage(d, s, s->gear + 1);
	} else if (s->gear > 1 && rpm < downAt &&
		   wheel_rpm(d, u, s->gear - 1) < upAt * 0.92f) {
		engage(d, s, s->gear - 1);
	}
}

/* Engine speed, turbo and the longitudinal force requested from the tires,
 * positive towards the front of the car. */
static float powertrain(const F3DSVehicleDef *d, F3DSVehicleState *s, float pedal, float u, float dt)
{
	const float dir = s->gear == 0 ? -1.0f : 1.0f;
	const float connected = wheel_rpm(d, u, s->gear);
	const bool shifting = s->shiftTimer > 0.0f;
	/* Clutch slip lets the engine rev above wheel speed when pulling away. */
	const float launch = d->idleRpm + pedal * (d->maxRpm - d->idleRpm) * 0.38f;
	float target = s->airborne ? d->idleRpm + pedal * (d->maxRpm - d->idleRpm)
		: fmaxf(connected, launch);
	if (shifting)
		target = fmaxf(connected, d->idleRpm);
	const float response = (target > s->rpm ? 9500.0f : 7500.0f) * (1.0f + d->pistonLevel * 0.08f);
	s->rpm = approachf(s->rpm, clampf(target, d->idleRpm, d->maxRpm), response * dt);
	s->revLimited = connected >= d->maxRpm || (s->airborne && s->rpm >= d->maxRpm - 1.0f);

	/* Turbo spool: pressure follows load and revs with a lag. */
	float boostTarget = 0.0f;
	if (d->turboInstalled) {
		const float spool = smoothstepf((s->rpm - d->idleRpm) / fmaxf((d->maxRpm - d->idleRpm) * 0.35f, 1.0f));
		boostTarget = d->turboBoostBar * spool * pedal;
	}
	const float lag = 0.22f + 0.12f * fmaxf(d->turboBoostBar, 0.0f) / (1.0f + d->intakeLevel * 0.25f);
	const float k = boostTarget > s->boost ? dt / (lag + dt) : dt / (0.08f + dt);
	s->boost += (boostTarget - s->boost) * clampf(k, 0.0f, 1.0f);

	s->throttle = pedal;
	s->engineLoad = s->revLimited || shifting ? pedal * 0.2f : pedal;
	if (shifting)
		return 0.0f;

	const float perNm = gear_ratio(d, s->gear) / d->wheelRadius;
	float force = 0.0f;
	if (pedal > 0.0f && !s->revLimited)
		force = dir * vehicle_torque_boosted(d, s->rpm, s->boost) * perNm * d->drivetrainEff * pedal;
	/* Engine braking; strong when a downshift pushes past the rev limit. */
	if (fabsf(u) > 0.5f) {
		float drag = (1.0f - pedal) * 0.09f * d->torqueBase *
			clampf((connected - d->idleRpm) / fmaxf(d->maxRpm - d->idleRpm, 1.0f), 0.0f, 1.0f);
		if (connected > d->maxRpm * 1.02f)
			drag += d->torqueBase * 0.45f;
		force -= (u > 0.0f ? 1.0f : -1.0f) * drag * perNm;
	}
	return force;
}

static void substep(const F3DSVehicleDef *d, F3DSVehicleState *s, const F3DSInputState *in,
	const F3DSSurface surface[2], float dt)
{
	const FVec3 f = yaw_forward(s->yaw), l = yaw_left(s->yaw);
	float u = s->vx * f.x + s->vz * f.z;
	float w = s->vx * l.x + s->vz * l.z;
	float r = s->yawRate;

	/* Progressive steering. Full stick asks for slightly more than the angle
	 * the front tires can use at this speed; extra lock is available towards
	 * the direction of travel so that slides can be counter-steered. Input is
	 * positive to the right; the road wheel angle is positive to the left. */
	const float grip = tire_surface_grip(d->tireType, surface[0]) * fmaxf(d->tireGrip, 0.05f);
	const float usable = atanf(d->wheelBase * grip * F3DS_GRAVITY / fmaxf(u * u, 1.0f)) +
		1.15f * tire_peak_slip(d->tireType);
	const float drift = fabsf(u) > 3.0f ? atan2f(w, fabsf(u)) : 0.0f;
	const float command = -clampf(in->steer, -1.0f, 1.0f);
	const float counter = fmaxf(command >= 0.0f ? drift : -drift, 0.0f);
	const float lock = fminf(d->maxSteerAngle, usable + counter);
	s->steerAngle = approachf(s->steerAngle, command * lock, d->steerRate * dt);

	if (s->shiftTimer > 0.0f)
		s->shiftTimer = fmaxf(0.0f, s->shiftTimer - dt);
	float drivePedal, brakePedal;
	transmission(d, s, in, u, &drivePedal, &brakePedal);
	const float drive = powertrain(d, s, drivePedal, u, dt);
	s->wheelAngle = fmodf(s->wheelAngle + u / d->wheelRadius * dt, 2.0f * F3DS_PI);

	s->absActive = s->tcsActive = s->wheelsLocked = false;
	s->wheelSpin = 0.0f;
	s->slip[0] = s->slip[1] = 0.0f;

	if (s->airborne) {
		const float v = sqrtf(s->vx * s->vx + s->vz * s->vz);
		const float k = 1.0f / (1.0f + d->drag * v * dt / d->mass);
		s->vx *= k;
		s->vz *= k;
		s->yawRate *= 1.0f - clampf(0.2f * dt, 0.0f, 1.0f);
		s->yaw = wrap_angle(s->yaw + s->yawRate * dt);
		s->accelLong = s->accelLat = 0.0f;
		update_loads(d, s, dt);
		s->speed = u;
		s->lateralSpeed = w;
		return;
	}

	update_loads(d, s, dt);
	const float speed = sqrtf(u * u + w * w);
	const float dir = u >= 0.0f ? 1.0f : -1.0f;
	/* Blend to the kinematic bicycle model at low speed. */
	float blend = clampf((speed - 1.0f) / 3.0f, 0.0f, 1.0f);
	if (fabsf(w) > 2.0f) blend = 1.0f;
	const float lf = d->wheelBase * (1.0f - d->weightFront);
	const float lr = d->wheelBase * d->weightFront;

	Axle axle[2];
	for (int a = 0; a < 2; ++a)
		axle[a] = axle_state(d, s, a, surface[a], u);

	/* Torque split; locking the centre differential moves torque to the axle
	 * with more grip. */
	float split = clampf(d->frontTorqueShare, 0.0f, 1.0f);
	if (split > 0.001f && split < 0.999f) {
		const float total = axle[0].capacity + axle[1].capacity;
		if (total > 1.0f)
			split = lerpf(split, axle[0].capacity / total, d->diffCenterLock);
	}

	float fx[2], fyScale[2];
	for (int a = 0; a < 2; ++a) {
		const float share = a == F3DS_FRONT ? split : 1.0f - split;
		const float lockRatio = a == F3DS_FRONT ? d->diffFrontLock : d->diffRearLock;
		const float cap = fmaxf(axle[a].capacity, 1.0f);
		/* An open differential is limited by its weaker wheel. */
		const float driveCap = fminf(cap, lerpf(2.0f * axle[a].weakWheel, cap, lockRatio));
		float fd = drive * share;
		float spin = 0.0f;
		const bool powered = fd * dir > 0.0f || speed < 0.5f;
		if (powered && fabsf(fd) > driveCap) {
			if (in->tcsEnabled) {
				s->tcsActive = true;
				fd = copysignf(driveCap * 0.94f, fd);
			} else {
				spin = clampf(fabsf(fd) / fmaxf(driveCap, 1.0f) - 1.0f, 0.15f, 1.0f);
				fd = copysignf(driveCap * 0.86f, fd);
			}
		}

		const float bias = a == F3DS_FRONT ? d->brakeBiasFront : 1.0f - d->brakeBiasFront;
		float fb = d->brakeForce * bias * brakePedal;
		bool locked = false;
		if (fb > cap * 0.98f) {
			if (in->absEnabled) {
				s->absActive = true;
				fb = cap * 0.96f;
			} else {
				locked = true;
				fb = cap * 0.80f;
			}
		}
		const bool hand = a == F3DS_REAR && in->handbrake;
		if (hand)
			fb = fmaxf(fb, fminf(d->handbrakeForce, cap * 0.80f));

		/* Brakes oppose motion and never push the car the other way. */
		const float stopForce = fabsf(u) / dt * d->mass * 0.5f;
		fx[a] = fd - dir * fminf(fb, stopForce);

		const float used = clampf(fabsf(fx[a]) / cap, 0.0f, 1.0f);
		fyScale[a] = sqrtf(fmaxf(1.0f - used * used * 0.82f, 0.0f));
		if (spin > 0.0f) {
			fyScale[a] *= 1.0f - 0.55f * spin;
			s->wheelSpin = fmaxf(s->wheelSpin, spin);
		}
		if (locked && speed > 1.0f) {
			fyScale[a] *= 0.35f;
			s->wheelsLocked = true;
		}
		if (hand && speed > 2.0f) {
			fyScale[a] = fminf(fyScale[a], 0.38f);
			s->wheelsLocked = true;
		}
	}

	/* Slip angles and lateral forces. */
	const float den = fmaxf(fabsf(u), 2.5f);
	const float slipF = atan2f(w + lf * r, den) - s->steerAngle * (u >= -0.5f ? 1.0f : -1.0f);
	const float slipR = atan2f(w - lr * r, den);
	const float inertia = d->mass * (d->wheelBase * d->wheelBase + d->trackWidth * d->trackWidth) / 6.0f;
	const float cosS = cosf(s->steerAngle), sinS = sinf(s->steerAngle);
	/* A tire can at most cancel the sideways velocity of its axle within one
	 * substep; this keeps extreme grip settings numerically stable. */
	const float sideF = (w + lf * r) * cosS - u * sinS, sideR = w - lr * r;
	const float limitF = 0.5f * fabsf(sideF) / ((1.0f / d->mass + lf * lf / inertia) * dt);
	const float limitR = 0.5f * fabsf(sideR) / ((1.0f / d->mass + lr * lr / inertia) * dt);
	float fyF = -axle[0].capacity * fyScale[0] * lateral_curve(slipF, axle[0].peakSlip);
	float fyR = -axle[1].capacity * fyScale[1] * lateral_curve(slipR, axle[1].peakSlip);
	fyF = clampf(fyF, -limitF, limitF);
	fyR = clampf(fyR, -limitR, limitR);

	const float bodyFx = fx[1] + fx[0] * cosS - fyF * sinS;
	const float bodyFy = fyR + fyF * cosS + fx[0] * sinS;
	float resist = d->drag * speed * u + d->rollResist * u *
		surface_rolling(surface[1]);

	/* Locked differentials resist yaw: stability off throttle. */
	const float lockDamp = (d->diffRearLock * (1.0f - 0.75f * drivePedal) + 0.5f * d->diffFrontLock) *
		0.11f * (axle[0].capacity + axle[1].capacity) * d->trackWidth * d->trackWidth /
		fmaxf(fabsf(u), 6.0f);
	float moment = lf * (fyF * cosS + fx[0] * sinS) - lr * fyR;
	/* A locked rear axle under power helps rotate the car into the turn. */
	moment += d->diffRearLock * fminf(fabsf(fx[1]), axle[1].capacity) * drivePedal *
		s->steerAngle * 0.15f * dir * d->trackWidth;

	/* Tire aligning effect: yaw much faster than the grip can turn the path is
	 * damped, except while the handbrake is used to break the rear loose. */
	if (!in->handbrake && speed > 5.0f) {
		const float limit = 1.08f * grip * F3DS_GRAVITY / speed;
		const float excess = fabsf(r) - limit;
		if (excess > 0.0f)
			moment -= copysignf(excess, r) * inertia * (in->tcsEnabled ? 14.0f : 3.0f);
	}

	const float ax = (bodyFx - resist) / d->mass;
	const float ay = bodyFy / d->mass;
	const float uDyn = u + (ax + w * r) * dt;
	const float wDyn = w + (ay - u * r) * dt;
	const float rDyn = (r + moment / inertia * dt) / (1.0f + lockDamp / inertia * dt);

	const float rKin = uDyn * tanf(s->steerAngle) / d->wheelBase;
	const float wKin = lr * rKin;
	const float follow = clampf(20.0f * dt, 0.0f, 1.0f);
	const float rLow = r + (rKin - r) * follow;
	const float wLow = w + (wKin - w) * follow;
	float nu = uDyn;
	float nw = lerpf(wLow, wDyn, blend);
	float nr = lerpf(rLow, rDyn, blend);

	/* Rest: hold the car instead of creeping around zero. */
	if (fabsf(drive) < 1.0f && speed < 0.35f && fabsf(nu) < 0.35f) {
		nu = approachf(nu, 0.0f, 2.0f * dt);
		nw = approachf(nw, 0.0f, 2.0f * dt);
		if (nu == 0.0f) nr = 0.0f;
	}
	if (brakePedal > 0.0f && u != 0.0f && nu * u < 0.0f && fabsf(drive) < 1.0f)
		nu = 0.0f;

	s->accelLong = (bodyFx - resist) / d->mass;
	s->accelLat = bodyFy / d->mass * blend + nu * nr * (1.0f - blend);

	s->slip[0] = clampf((fabsf(slipF) / axle[0].peakSlip - 1.0f) * blend, 0.0f, 1.0f);
	s->slip[1] = clampf((fabsf(slipR) / axle[1].peakSlip - 1.0f) * blend, 0.0f, 1.0f);
	if (s->wheelsLocked) s->slip[1] = fmaxf(s->slip[1], 0.6f * blend);

	s->yawRate = nr;
	s->yaw = wrap_angle(s->yaw + 0.5f * (r + nr) * dt);
	/* Velocity is kept in world space; (nu, nw) were integrated in the frame at
	 * the start of the substep including the rotating-frame terms. */
	const FVec3 nf = yaw_forward(s->yaw), nl = yaw_left(s->yaw);
	s->vx = nf.x * nu + nl.x * nw;
	s->vz = nf.z * nu + nl.z * nw;
	s->speed = nu;
	s->lateralSpeed = nw;
}

static void dynamics(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in,
	const F3DSTrack *track, const F3DSTerrain *terrain, float dt)
{
	F3DSSurface surface[2];
	const FVec3 f = yaw_forward(car->yaw);
	const float lf = def->wheelBase * (1.0f - def->weightFront);
	const float lr = def->wheelBase * def->weightFront;
	surface[0] = terrain_surface(terrain, track, car->pos.x + f.x * lf, car->pos.z + f.z * lf);
	surface[1] = terrain_surface(terrain, track, car->pos.x - f.x * lr, car->pos.z - f.z * lr);
	car->surface = surface[1];

	int n = (int)ceilf(dt / SUBSTEP_MAX - 0.001f);
	if (n < 1) n = 1;
	const float h = dt / (float)n;
	F3DSInputState sub = *in;
	for (int i = 0; i < n; ++i) {
		substep(def, car, &sub, surface, h);
		sub.gearUp = sub.gearDown = 0; /* one shift request per step */
		car->pos.x += car->vx * h;
		car->pos.z += car->vz * h;
	}
}

void physics_vehicle_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in,
	const F3DSTrack *track, const F3DSTerrain *terrain, float dt)
{
	if (!isfinite(dt) || dt <= 0.0f)
		return;
	dynamics(def, car, in, track, terrain, dt);
}

void physics_step(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSInputState *in,
	const F3DSTrack *track, const F3DSTerrain *terrain, float dt)
{
	if (!isfinite(dt) || dt <= 0.0f)
		return;
	const FVec3 before = car->pos;
	car->impact = 0.0f;
	dynamics(def, car, in, track, terrain, dt);
	collision_resolve(def, car, track);

	float ground = terrain_height(terrain, car->pos.x, car->pos.z);
	/* A tall step (side or back of a ramp) is a wall, not a climb. */
	if (ground - car->pos.y > 0.45f) {
		car->impact = fmaxf(car->impact, sqrtf(car->vx * car->vx + car->vz * car->vz));
		car->pos.x = before.x;
		car->pos.z = before.z;
		car->vx *= -0.2f;
		car->vz *= -0.2f;
		car->yawRate *= 0.5f;
		ground = terrain_height(terrain, car->pos.x, car->pos.z);
	}

	if (car->airborne) {
		car->vy -= F3DS_GRAVITY * dt;
		car->pos.y += car->vy * dt;
		if (car->pos.y <= ground) {
			car->pos.y = ground;
			const float hit = fmaxf(-car->vy, 0.0f);
			car->vy = 0.0f;
			car->airborne = false;
			const float scrub = clampf(1.0f - hit * 0.02f, 0.6f, 1.0f);
			car->vx *= scrub;
			car->vz *= scrub;
			car->pitchRate -= hit * 0.08f;
			car->impact = fmaxf(car->impact, hit);
		}
	} else {
		const float drop = ground - car->pos.y;
		const float fall = car->vy * dt - 0.5f * F3DS_GRAVITY * dt * dt;
		/* Leave the ground when it drops away faster than gravity can follow. */
		if (drop < fall - 0.06f) {
			car->airborne = true;
			car->pos.y += fall;
			car->vy -= F3DS_GRAVITY * dt;
		} else {
			car->vy = drop / dt;
			car->pos.y = ground;
		}
	}
}
