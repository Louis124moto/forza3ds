#include "test.h"
#include <math.h>
#include <string.h>
#include "catalog.h"
#include "physics.h"

#define DT (1.0f / 60.0f)

typedef struct {
	F3DSVehicleDef def;
	F3DSVehicleState car;
	F3DSInputState in;
} Rig;

static Rig rig(int id)
{
	Rig r;
	memset(&r, 0, sizeof(r));
	catalog_def(id, &r.def);
	vehicle_state_reset(&r.car, &r.def, fvec3(0, 0, 0), 0.0f);
	r.in.absEnabled = r.in.tcsEnabled = 1;
	return r;
}

/* Flat asphalt, no walls. */
static void run(Rig *r, float seconds)
{
	for (int i = 0; i < (int)(seconds / DT + 0.5f); ++i) {
		physics_vehicle_step(&r->def, &r->car, &r->in, NULL, NULL, DT);
		r->in.gearUp = r->in.gearDown = 0;
	}
}

/* Straight line at a given forward speed, in a suitable gear. */
static void launch(Rig *r, float speed, int gear)
{
	r->car.vx = 0.0f;
	r->car.vz = speed;
	r->car.speed = speed;
	r->car.gear = gear;
}

static bool finite_state(const F3DSVehicleState *s)
{
	const float v[] = { s->pos.x, s->pos.z, s->vx, s->vz, s->yaw, s->yawRate, s->rpm, s->boost,
		s->pitch, s->roll, s->accelLong, s->accelLat, s->speed, s->lateralSpeed,
		s->wheelLoad[0], s->wheelLoad[1], s->wheelLoad[2], s->wheelLoad[3] };
	for (unsigned i = 0; i < sizeof(v) / sizeof(v[0]); ++i)
		if (!isfinite(v[i])) return false;
	return true;
}

static void definitions(void)
{
	for (int i = 0; i < F3DS_CAR_COUNT; ++i) {
		F3DSVehicleDef d;
		catalog_def(i, &d);
		CHECKF(vehicle_def_valid(&d), "car %d", i);
		CHECK(vehicle_peak_power_kw(&d) > 40.0f && vehicle_peak_power_kw(&d) < 500.0f);
	}
}

static void rest(void)
{
	for (int i = 0; i < F3DS_CAR_COUNT; ++i) {
		Rig r = rig(i);
		run(&r, 5.0f);
		CHECK(fabsf(r.car.pos.x) < 0.01f && fabsf(r.car.pos.z) < 0.01f);
		CHECK(fabsf(r.car.rpm - r.def.idleRpm) < 1.0f);
		const float total = r.car.wheelLoad[0] + r.car.wheelLoad[1] + r.car.wheelLoad[2] + r.car.wheelLoad[3];
		CHECK(fabsf(total - r.def.mass * F3DS_GRAVITY) < 1.0f);
	}
}

static float time_to_100(int id)
{
	Rig r = rig(id);
	r.in.throttle = 1.0f;
	for (int i = 0; i < 60 * 40; ++i) {
		run(&r, DT);
		if (r.car.speed >= 100.0f / 3.6f)
			return i * DT;
	}
	return 99.0f;
}

static void acceleration(void)
{
	float t[F3DS_CAR_COUNT];
	for (int i = 0; i < F3DS_CAR_COUNT; ++i) {
		t[i] = time_to_100(i);
		printf("  car %d (%s): 0-100 km/h %.2f s\n", i, catalog_info(i)->name, t[i]);
		CHECKF(t[i] > 2.5f && t[i] < 18.0f, "car %d: %f", i, t[i]);
	}
	for (int i = 0; i < F3DS_CAR_COUNT; ++i)
		for (int j = 0; j < i; ++j)
			CHECKF(fabsf(t[i] - t[j]) > 0.15f, "cars %d and %d feel identical", i, j);

	/* Longitudinal load transfer: weight moves rearwards, the nose lifts. */
	Rig r = rig(0);
	const float staticRear = r.car.wheelLoad[2] + r.car.wheelLoad[3];
	r.in.throttle = 1.0f;
	run(&r, 1.5f);
	CHECK(r.car.wheelLoad[2] + r.car.wheelLoad[3] > staticRear * 1.05f);
	CHECK(r.car.pitch > 0.003f);
}

static void steering(void)
{
	/* Positive input steers to the driver's right: heading +Z, right is -X. */
	const float speeds[] = { 3.0f, 12.0f, 40.0f };
	for (int s = 0; s < 3; ++s) {
		for (int side = -1; side <= 1; side += 2) {
			Rig r = rig(0);
			launch(&r, speeds[s], 3);
			r.in.steer = (float)side;
			r.in.throttle = 0.3f;
			run(&r, 0.8f);
			CHECKF(r.car.yaw * side < -0.03f, "v=%f side=%d yaw=%f", speeds[s], side, r.car.yaw);
			CHECKF(r.car.pos.x * side < -0.03f, "v=%f side=%d x=%f", speeds[s], side, r.car.pos.x);
		}
	}
	/* Progressive: the wheels need time to reach lock. */
	Rig r = rig(0);
	r.in.steer = 1.0f;
	run(&r, DT);
	CHECK(fabsf(r.car.steerAngle) < r.def.maxSteerAngle * 0.2f);
	run(&r, 1.0f);
	CHECK(fabsf(r.car.steerAngle) > r.def.maxSteerAngle * 0.95f);
	/* Cornering is limited by grip, and the body rolls outwards. */
	for (int id = 0; id < F3DS_CAR_COUNT; ++id) {
		Rig c = rig(id);
		launch(&c, 28.0f, 3);
		c.in.steer = -1.0f; /* left */
		c.in.throttle = 0.4f;
		float peak = 0.0f;
		for (int i = 0; i < 180; ++i) {
			run(&c, DT);
			const float ay = fabsf(c.car.speed * c.car.yawRate);
			if (i > 20) peak = fmaxf(peak, ay);
		}
		const float limit = tire_surface_grip(c.def.tireType, F3DS_SURFACE_ASPHALT) * F3DS_GRAVITY;
		CHECKF(peak > 0.55f * limit && peak < 1.35f * limit, "car %d: %f vs %f", id, peak, limit);
		CHECK(finite_state(&c.car));
	}
	Rig c = rig(0);
	launch(&c, 20.0f, 3);
	c.in.steer = -0.5f;
	run(&c, 0.8f);
	CHECK(c.car.roll > 0.005f); /* left turn: leans right */
	CHECK(c.car.wheelLoad[1] > c.car.wheelLoad[0]);
}

static float braking_distance(Rig *r, float speed)
{
	launch(r, speed, 6);
	r->in.throttle = 0.0f;
	r->in.brake = 1.0f;
	r->def.manualTransmission = 1;
	for (int i = 0; i < 60 * 30; ++i) {
		run(r, DT);
		if (r->car.speed < 0.2f)
			break;
	}
	CHECKF(r->car.speed < 0.2f && r->car.speed > -0.2f, "speed %f", r->car.speed);
	run(r, 1.0f);
	CHECK(fabsf(r->car.speed) < 0.05f); /* never pushed backwards */
	return r->car.pos.z;
}

static void braking_and_surfaces(void)
{
	for (int id = 0; id < F3DS_CAR_COUNT; ++id) {
		Rig r = rig(id);
		const float d = braking_distance(&r, 30.0f);
		const float decel = 30.0f * 30.0f / (2.0f * d);
		printf("  car %d: 108-0 km/h in %.1f m (%.2f g)\n", id, d, decel / F3DS_GRAVITY);
		CHECKF(decel > 6.0f && decel < 14.0f, "decel %f", decel);
	}
	Rig abs = rig(0), locked = rig(0);
	locked.in.absEnabled = 0;
	locked.def.brakeForce = abs.def.brakeForce = 30000.0f;
	const float a = braking_distance(&abs, 30.0f);
	launch(&locked, 30.0f, 3);
	locked.in.brake = 1.0f;
	run(&locked, 0.3f);
	CHECK(locked.car.wheelsLocked);
	locked = rig(0);
	locked.in.absEnabled = 0;
	locked.def.brakeForce = 30000.0f;
	const float l = braking_distance(&locked, 30.0f);
	CHECKF(l > a * 1.08f, "abs %f locked %f", a, l);

	/* Tire types against surfaces (grip table drives the simulation). */
	CHECK(tire_surface_grip(F3DS_TIRE_SLICK, F3DS_SURFACE_ASPHALT) >
		tire_surface_grip(F3DS_TIRE_ROAD, F3DS_SURFACE_ASPHALT));
	CHECK(tire_surface_grip(F3DS_TIRE_RALLY, F3DS_SURFACE_GRAVEL) >
		tire_surface_grip(F3DS_TIRE_SLICK, F3DS_SURFACE_GRAVEL) * 1.5f);
	float dist[F3DS_TIRE_COUNT];
	for (int t = 0; t < F3DS_TIRE_COUNT; ++t) {
		Rig r = rig(0);
		r.def.tireType = (F3DSTireType)t;
		r.def.brakeForce = 30000.0f;
		dist[t] = braking_distance(&r, 30.0f);
	}
	CHECK(dist[F3DS_TIRE_SLICK] < dist[F3DS_TIRE_SEMISLICK]);
	CHECK(dist[F3DS_TIRE_SEMISLICK] < dist[F3DS_TIRE_SPORT]);
	CHECK(dist[F3DS_TIRE_SPORT] < dist[F3DS_TIRE_ROAD]);
	CHECK(dist[F3DS_TIRE_ROAD] < dist[F3DS_TIRE_OFFROAD]);

	/* Grip also depends on speed. */
	Rig slow = rig(0), fast = rig(0);
	slow.def.brakeForce = fast.def.brakeForce = 30000.0f;
	slow.def.drag = fast.def.drag = 0.0f; /* tires only */
	const float s20 = braking_distance(&slow, 20.0f), s50 = braking_distance(&fast, 50.0f);
	CHECK(2500.0f / (2.0f * s50) < 400.0f / (2.0f * s20) * 1.02f);
}

static void gearbox(void)
{
	/* Rev limiter: first gear has a top speed. */
	Rig r = rig(0);
	r.def.manualTransmission = 1;
	r.in.throttle = 1.0f;
	run(&r, 20.0f);
	const float ratio = r.def.gearRatios[0] * r.def.finalDrive;
	const float vmax = r.def.maxRpm / 60.0f * 2.0f * F3DS_PI * r.def.wheelRadius / ratio;
	CHECKF(r.car.speed < vmax * 1.03f && r.car.speed > vmax * 0.9f, "%f vs %f", r.car.speed, vmax);
	CHECK(r.car.revLimited && r.car.rpm <= r.def.maxRpm);
	r.in.gearUp = 1;
	run(&r, 3.0f);
	CHECK(r.car.gear == 2 && r.car.speed > vmax * 1.05f);

	/* Automatic: up through the box, then down again. */
	Rig a = rig(0);
	a.in.throttle = 1.0f;
	int highest = 1;
	for (int i = 0; i < 60 * 30; ++i) {
		run(&a, DT);
		if (a.car.gear > highest) highest = a.car.gear;
	}
	CHECKF(highest >= 4, "highest %d", highest);
	a.in.throttle = 0.0f;
	a.in.brake = 0.4f;
	for (int i = 0; i < 60 * 20 && a.car.speed > 2.0f; ++i) run(&a, DT);
	CHECKF(a.car.gear == 1, "gear %d at %f", a.car.gear, a.car.speed);

	/* Automatic reverse: brake pedal at rest, accelerator brakes. */
	Rig b = rig(0);
	b.in.brake = 1.0f;
	run(&b, 3.0f);
	CHECK(b.car.gear == 0 && b.car.speed < -2.0f && b.car.pos.z < -2.0f);
	b.in.brake = 0.0f;
	b.in.throttle = 1.0f;
	run(&b, 4.0f);
	CHECK(b.car.gear == 1 && b.car.speed > 2.0f);

	/* Steering in reverse turns the other way, like a real car. */
	Rig rv = rig(0);
	rv.in.brake = 0.6f;
	rv.in.steer = 1.0f;
	run(&rv, 2.0f);
	const float rearX = rv.car.pos.x - sinf(rv.car.yaw) * rv.def.wheelBase * rv.def.weightFront;
	CHECKF(rv.car.yaw > 0.05f && rearX < -0.05f, "yaw %f rear x %f", rv.car.yaw, rearX);

	/* Manual reverse: shift down from first at rest. */
	Rig m = rig(0);
	m.def.manualTransmission = 1;
	m.in.gearDown = 1;
	run(&m, 0.5f);
	CHECK(m.car.gear == 0);
	m.in.throttle = 0.6f;
	run(&m, 2.0f);
	CHECK(m.car.speed < -1.0f);
	m.in.throttle = 0.0f;
	m.in.brake = 1.0f;
	run(&m, 3.0f);
	m.in.gearUp = 1;
	run(&m, 0.5f);
	CHECK(m.car.gear == 1);

	/* Ratios and gear count change performance. */
	Rig shortf = rig(0), longf = rig(0);
	shortf.def.finalDrive = 4.6f;
	longf.def.finalDrive = 2.4f;
	shortf.in.throttle = longf.in.throttle = 1.0f;
	run(&shortf, 3.0f);
	run(&longf, 3.0f);
	CHECK(shortf.car.speed > longf.car.speed * 1.1f);
	Rig one = rig(0);
	one.def.gearCount = 1;
	one.in.throttle = 1.0f;
	run(&one, 25.0f);
	CHECK(one.car.gear == 1 && one.car.speed < vmax * 1.03f);
	Rig ten = rig(0);
	ten.def.gearCount = F3DS_MAX_GEARS;
	ten.def.torqueBase = 900.0f;
	ten.in.throttle = 1.0f;
	run(&ten, 40.0f);
	CHECK(ten.car.gear > 6 && finite_state(&ten.car));
}

static void slides(void)
{
	/* Handbrake: the rear steps out; releasing it recovers. */
	Rig plain = rig(0), hand = rig(0);
	launch(&plain, 18.0f, 3);
	launch(&hand, 18.0f, 3);
	plain.in.steer = hand.in.steer = -0.7f;
	hand.in.handbrake = 1;
	float maxPlain = 0.0f, maxHand = 0.0f, slip = 0.0f;
	for (int i = 0; i < 60; ++i) {
		run(&plain, DT);
		run(&hand, DT);
		maxPlain = fmaxf(maxPlain, fabsf(plain.car.lateralSpeed));
		maxHand = fmaxf(maxHand, fabsf(hand.car.lateralSpeed));
		slip = fmaxf(slip, hand.car.slip[1]);
	}
	CHECKF(maxHand > maxPlain * 1.5f && slip > 0.4f, "%f %f %f", maxHand, maxPlain, slip);
	CHECK(fabsf(hand.car.yaw) > fabsf(plain.car.yaw));
	hand.in.handbrake = 0;
	hand.in.steer = 0.0f;
	run(&hand, 5.0f);
	CHECKF(fabsf(hand.car.yawRate) < 0.05f && fabsf(hand.car.lateralSpeed) < 0.3f,
		"%f %f", hand.car.yawRate, hand.car.lateralSpeed);

	/* Power oversteer without traction control on a powerful rear drive car. */
	Rig aided = rig(3), raw = rig(3);
	raw.in.tcsEnabled = 0;
	launch(&aided, 12.0f, 2);
	launch(&raw, 12.0f, 2);
	aided.in.steer = raw.in.steer = -0.8f;
	aided.in.throttle = raw.in.throttle = 1.0f;
	float spin = 0.0f;
	float drift = 0.0f, grip = 0.0f;
	for (int i = 0; i < 90; ++i) {
		run(&aided, DT);
		run(&raw, DT);
		spin = fmaxf(spin, raw.car.wheelSpin);
		drift = fmaxf(drift, fabsf(raw.car.lateralSpeed));
		grip = fmaxf(grip, fabsf(aided.car.lateralSpeed));
	}
	CHECKF(spin > 0.2f && drift > grip * 1.15f, "spin %f drift %f grip %f", spin, drift, grip);
	CHECK(aided.car.wheelSpin < 0.01f);
	/* A rear-heavy brake bias and front-limited setups change the balance. */
	Rig front = rig(2); /* front drive: power understeer */
	front.in.tcsEnabled = 0;
	launch(&front, 14.0f, 2);
	front.in.steer = -1.0f;
	Rig coast = front;
	front.in.throttle = 1.0f;
	run(&front, 1.5f);
	run(&coast, 1.5f);
	CHECKF(fabsf(front.car.yaw) < fabsf(coast.car.yaw) * 1.05f, "%f %f", front.car.yaw, coast.car.yaw);
}

static void tuning(void)
{
	/* Every engine part adds torque at high revs, with no ceiling. */
	F3DSVehicleDef base;
	catalog_def(0, &base);
	const float rpm = 6200.0f, stock = vehicle_engine_torque(&base, rpm);
	for (int part = 0; part < 5; ++part) {
		F3DSVehicleDef d = base;
		int *level[5] = { &d.intakeLevel, &d.exhaustLevel, &d.camshaftLevel, &d.injectorLevel, &d.pistonLevel };
		*level[part] = 2;
		const float two = vehicle_engine_torque(&d, rpm);
		*level[part] = 40;
		/* A camshaft moves the power band up: compare the best torque. */
		float best = 0.0f;
		for (float n = 1000.0f; n < 40000.0f; n += 250.0f)
			best = fmaxf(best, vehicle_engine_torque(&d, n));
		CHECKF(two > stock && best > two, "part %d", part);
	}
	F3DSVehicleDef turbo = base;
	turbo.turboInstalled = 1;
	turbo.turboBoostBar = 1.0f;
	CHECK(vehicle_engine_torque(&turbo, rpm) > stock * 1.4f);
	turbo.turboBoostBar = 30.0f;
	CHECK(vehicle_engine_torque(&turbo, rpm) > stock * 10.0f);

	/* The turbo spools: response lags behind the pedal. */
	Rig t = rig(1);
	launch(&t, 15.0f, 2);
	t.in.throttle = 1.0f;
	run(&t, DT * 3);
	const float early = t.car.boost;
	run(&t, 2.0f);
	CHECK(early < t.def.turboBoostBar * 0.5f && t.car.boost > t.def.turboBoostBar * 0.8f);

	/* A change made in the workshop acts on the very next step. */
	Rig a = rig(0), b = rig(0);
	a.in.throttle = b.in.throttle = 1.0f;
	run(&a, 2.0f);
	run(&b, 2.0f);
	CHECK(memcmp(&a.car, &b.car, sizeof(a.car)) == 0); /* deterministic */
	launch(&a, 30.0f, 4);
	launch(&b, 30.0f, 4);
	b.def.torqueBase *= 1.5f;
	run(&a, DT);
	run(&b, DT);
	CHECK(b.car.vz > a.car.vz);

	/* Suspension: stiffer springs roll less, damping calms the body. */
	Rig soft = rig(0), stiff = rig(0);
	for (int i = 0; i < 2; ++i) {
		soft.def.springRate[i] = 16000.0f;
		stiff.def.springRate[i] = 90000.0f;
	}
	launch(&soft, 18.0f, 3);
	launch(&stiff, 18.0f, 3);
	soft.in.steer = stiff.in.steer = -0.5f;
	run(&soft, 2.0f);
	run(&stiff, 2.0f);
	CHECKF(soft.car.roll > stiff.car.roll * 2.0f, "%f %f", soft.car.roll, stiff.car.roll);
	Rig loose = rig(0), damped = rig(0);
	for (int i = 0; i < 2; ++i) {
		loose.def.damperCompression[i] = loose.def.damperRebound[i] = 200.0f;
		damped.def.damperCompression[i] = damped.def.damperRebound[i] = 9000.0f;
	}
	loose.car.roll = damped.car.roll = 0.05f;
	float swingLoose = 0.0f, swingDamped = 0.0f;
	for (int i = 0; i < 120; ++i) {
		run(&loose, DT);
		run(&damped, DT);
		if (i > 30) {
			swingLoose = fmaxf(swingLoose, fabsf(loose.car.roll));
			swingDamped = fmaxf(swingDamped, fabsf(damped.car.roll));
		}
	}
	CHECKF(swingLoose > swingDamped * 3.0f, "%f %f", swingLoose, swingDamped);
	/* Compression and rebound act separately in pitch. */
	Rig comp = rig(0), reb = rig(0);
	comp.def.damperCompression[1] = 9000.0f;
	reb.def.damperRebound[1] = 9000.0f;
	comp.in.throttle = reb.in.throttle = 1.0f;
	run(&comp, 0.25f);
	run(&reb, 0.25f);
	CHECK(fabsf(comp.car.pitch - reb.car.pitch) > 1e-5f);
	Rig tall = rig(0), low = rig(0);
	tall.def.rideHeight[0] = tall.def.rideHeight[1] = 0.35f;
	low.def.rideHeight[0] = low.def.rideHeight[1] = 0.08f;
	launch(&tall, 18.0f, 3);
	launch(&low, 18.0f, 3);
	tall.in.steer = low.in.steer = -0.4f;
	run(&tall, 1.5f);
	run(&low, 1.5f);
	CHECK(tall.car.roll > low.car.roll * 1.15f);

	/* Differential lock helps traction out of a corner. */
	Rig open = rig(3), locked = rig(3);
	open.def.diffRearLock = 0.0f;
	locked.def.diffRearLock = 1.0f;
	open.in.tcsEnabled = locked.in.tcsEnabled = 1;
	launch(&open, 15.0f, 2);
	launch(&locked, 15.0f, 2);
	open.in.steer = locked.in.steer = -0.8f;
	open.in.throttle = locked.in.throttle = 1.0f;
	run(&open, 1.2f);
	run(&locked, 1.2f);
	const float vo = hypotf(open.car.vx, open.car.vz), vl = hypotf(locked.car.vx, locked.car.vz);
	CHECKF(vl > vo + 0.1f, "open %f locked %f", vo, vl);
	/* Torque split changes the drive behaviour. */
	Rig rwd = rig(1), fwd = rig(1);
	rwd.def.frontTorqueShare = 0.0f;
	fwd.def.frontTorqueShare = 1.0f;
	launch(&rwd, 10.0f, 2);
	launch(&fwd, 10.0f, 2);
	rwd.in.tcsEnabled = fwd.in.tcsEnabled = 0;
	rwd.in.throttle = fwd.in.throttle = 1.0f;
	rwd.in.steer = fwd.in.steer = -0.7f;
	run(&rwd, 1.5f);
	run(&fwd, 1.5f);
	CHECK(fabsf(rwd.car.yaw - fwd.car.yaw) > 0.05f);
	Rig c0 = rig(1), c1 = rig(1);
	c0.def.diffCenterLock = 0.0f;
	c1.def.diffCenterLock = 1.0f;
	c0.in.throttle = c1.in.throttle = 1.0f;
	run(&c0, 1.5f);
	run(&c1, 1.5f);
	CHECK(fabsf(c0.car.speed - c1.car.speed) > 0.01f);
}

static void abuse(void)
{
	/* Absurd but valid setups and random inputs stay finite. */
	unsigned seed = 12345u;
	for (int trial = 0; trial < 40; ++trial) {
		Rig r = rig(trial % F3DS_CAR_COUNT);
		if (trial & 1) {
			r.def.torqueBase = 20000.0f;
			r.def.turboInstalled = 1;
			r.def.turboBoostBar = 50.0f;
			r.def.maxRpm = 60000.0f;
		}
		if (trial & 2) {
			r.def.springRate[0] = r.def.springRate[1] = 1e7f;
			r.def.damperRebound[0] = 1e6f;
			r.def.brakeForce = 1e6f;
		}
		if (trial & 4) {
			r.def.finalDrive = 40.0f;
			r.def.tireGrip = 20.0f;
			r.def.rideHeight[1] = 5.0f;
		}
		if (trial & 8) {
			r.def.finalDrive = 0.5f;
			r.def.springRate[0] = 6000.0f;
			r.def.tireGrip = 0.2f;
		}
		CHECK(vehicle_def_valid(&r.def));
		bool ok = true;
		for (int i = 0; i < 1200 && ok; ++i) {
			if (i % 40 == 0) {
				seed = seed * 1664525u + 1013904223u;
				r.in.throttle = (seed >> 8) & 1;
				r.in.brake = (seed >> 9) & 1;
				r.in.handbrake = (seed >> 10) & 1;
				r.in.steer = (float)((int)((seed >> 11) % 21) - 10) / 10.0f;
				r.in.gearUp = (seed >> 16) & 1;
				r.in.gearDown = (seed >> 17) & 1;
				r.in.absEnabled = (seed >> 18) & 1;
				r.in.tcsEnabled = (seed >> 19) & 1;
				r.def.manualTransmission = (seed >> 20) & 1;
			}
			run(&r, DT);
			ok = finite_state(&r.car) && fabsf(r.car.yawRate) < 60.0f && fabsf(r.car.speed) < 2000.0f;
		}
		CHECKF(ok, "trial %d yawRate %f speed %f", trial, r.car.yawRate, r.car.speed);
	}
	/* Bad time steps are ignored. */
	Rig r = rig(0);
	const F3DSVehicleState before = r.car;
	physics_vehicle_step(&r.def, &r.car, &r.in, NULL, NULL, NAN);
	physics_vehicle_step(&r.def, &r.car, &r.in, NULL, NULL, -1.0f);
	CHECK(memcmp(&before, &r.car, sizeof(before)) == 0);
}

void test_physics(void)
{
	definitions();
	rest();
	acceleration();
	steering();
	braking_and_surfaces();
	gearbox();
	slides();
	tuning();
	abuse();
}
