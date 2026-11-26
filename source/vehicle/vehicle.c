#include "vehicle.h"
#include <string.h>

void vehicle_state_reset(F3DSVehicleState *s, const F3DSVehicleDef *d, FVec3 pos, float yaw)
{
	const int model = s->visualModelId;
	memset(s, 0, sizeof(*s));
	s->visualModelId = model;
	s->pos = pos;
	s->yaw = yaw;
	s->gear = 1;
	s->rpm = d->idleRpm;
	const float front = d->mass * F3DS_GRAVITY * d->weightFront * 0.5f;
	const float rear = d->mass * F3DS_GRAVITY * (1.0f - d->weightFront) * 0.5f;
	s->wheelLoad[0] = s->wheelLoad[1] = front;
	s->wheelLoad[2] = s->wheelLoad[3] = rear;
}

/* Naturally aspirated curve: builds up to the torque peak, then falls away
 * smoothly. The camshaft moves the peak up; intake and exhaust help most at
 * high revs. No ceiling is applied to any part level or to the rev limit. */
float vehicle_torque_boosted(const F3DSVehicleDef *d, float rpm, float boostBar)
{
	const float peak = fmaxf(d->torquePeakRpm + d->camshaftLevel * 350.0f, d->idleRpm + 100.0f);
	float curve;
	if (rpm <= peak) {
		const float t = (rpm - d->idleRpm) / (peak - d->idleRpm);
		const float low = fmaxf(0.35f, 0.72f - d->camshaftLevel * 0.025f);
		curve = low + (1.0f - low) * smoothstepf(t);
	} else {
		const float span = peak * (0.75f + 0.04f * (d->intakeLevel + d->exhaustLevel));
		const float over = (rpm - peak) / fmaxf(span, 100.0f);
		curve = 1.0f / (1.0f + 0.55f * over * over);
	}
	const float highRev = smoothstepf((rpm - d->idleRpm) / fmaxf(peak - d->idleRpm, 100.0f));
	const float parts = 1.0f
		+ d->intakeLevel * (0.025f + 0.030f * highRev)
		+ d->exhaustLevel * (0.030f + 0.035f * highRev)
		+ d->camshaftLevel * (0.060f * highRev)
		+ d->injectorLevel * 0.040f
		+ d->pistonLevel * 0.055f;
	const float charge = 1.0f + (d->turboInstalled ? fmaxf(boostBar, 0.0f) * 0.55f : 0.0f);
	return d->torqueBase * curve * parts * charge;
}

float vehicle_engine_torque(const F3DSVehicleDef *d, float rpm)
{
	return vehicle_torque_boosted(d, rpm, d->turboBoostBar);
}

float vehicle_peak_power_kw(const F3DSVehicleDef *d)
{
	float best = 0.0f;
	const float step = fmaxf((d->maxRpm - d->idleRpm) / 64.0f, 1.0f);
	for (float rpm = d->idleRpm; rpm <= d->maxRpm; rpm += step) {
		const float kw = vehicle_engine_torque(d, rpm) * rpm * (2.0f * F3DS_PI / 60.0f) / 1000.0f;
		best = fmaxf(best, kw);
	}
	return best;
}

float tire_surface_grip(F3DSTireType tire, F3DSSurface surface)
{
	static const float grip[F3DS_TIRE_COUNT][F3DS_SURFACE_COUNT] = {
		/* asphalt, gravel, grass */
		{ 1.00f, 0.55f, 0.45f }, /* road */
		{ 1.12f, 0.50f, 0.40f }, /* sport */
		{ 1.24f, 0.44f, 0.34f }, /* semi-slick */
		{ 1.36f, 0.36f, 0.28f }, /* slick */
		{ 0.94f, 0.80f, 0.62f }, /* rally */
		{ 0.84f, 0.76f, 0.76f }, /* off-road */
	};
	if ((unsigned)tire >= F3DS_TIRE_COUNT) tire = F3DS_TIRE_ROAD;
	if ((unsigned)surface >= F3DS_SURFACE_COUNT) surface = F3DS_SURFACE_ASPHALT;
	return grip[tire][surface];
}

/* Slip angle (rad) at which each tire reaches peak lateral force. */
float tire_peak_slip(F3DSTireType tire)
{
	static const float peak[F3DS_TIRE_COUNT] = { 0.125f, 0.112f, 0.102f, 0.092f, 0.160f, 0.190f };
	if ((unsigned)tire >= F3DS_TIRE_COUNT) tire = F3DS_TIRE_ROAD;
	return peak[tire];
}

const char *tire_name(F3DSTireType tire)
{
	static const char *names[F3DS_TIRE_COUNT] = {
		"Route", "Sport", "Semi-slick", "Slick", "Rallye", "Tout-terrain"
	};
	return (unsigned)tire < F3DS_TIRE_COUNT ? names[tire] : "?";
}

static bool positive(float v) { return isfinite(v) && v > 0.0f; }
static bool non_negative(float v) { return isfinite(v) && v >= 0.0f; }
static bool unit(float v) { return isfinite(v) && v >= 0.0f && v <= 1.0f; }

bool vehicle_def_valid(const F3DSVehicleDef *d)
{
	if (!positive(d->mass) || !positive(d->wheelRadius) || !positive(d->wheelBase) ||
	    !positive(d->trackWidth) || !positive(d->cgHeight) || !positive(d->maxSteerAngle) ||
	    !positive(d->steerRate) ||
	    !non_negative(d->drag) || !non_negative(d->rollResist) ||
	    !(d->weightFront > 0.1f && d->weightFront < 0.9f))
		return false;
	if (!non_negative(d->brakeForce) || !unit(d->brakeBiasFront) || !non_negative(d->handbrakeForce))
		return false;
	for (int a = 0; a < 2; ++a)
		if (!positive(d->rideHeight[a]) || !positive(d->springRate[a]) ||
		    !non_negative(d->damperCompression[a]) || !non_negative(d->damperRebound[a]))
			return false;
	if ((unsigned)d->tireType >= F3DS_TIRE_COUNT || !positive(d->tireGrip))
		return false;
	if (!unit(d->frontTorqueShare) || !unit(d->diffFrontLock) || !unit(d->diffCenterLock) ||
	    !unit(d->diffRearLock) || !positive(d->finalDrive) || !positive(-d->gearRatioR) ||
	    !positive(d->drivetrainEff) || !non_negative(d->shiftTime) ||
	    d->gearCount < 1 || d->gearCount > F3DS_MAX_GEARS ||
	    (d->manualTransmission != 0 && d->manualTransmission != 1))
		return false;
	for (int i = 0; i < F3DS_MAX_GEARS; ++i)
		if (!positive(d->gearRatios[i])) return false;
	return positive(d->idleRpm) && isfinite(d->maxRpm) && d->maxRpm > d->idleRpm &&
		positive(d->torqueBase) && positive(d->torquePeakRpm) &&
		non_negative(d->turboBoostBar) &&
		(d->turboInstalled == 0 || d->turboInstalled == 1) &&
		d->intakeLevel >= 0 && d->exhaustLevel >= 0 && d->camshaftLevel >= 0 &&
		d->injectorLevel >= 0 && d->pistonLevel >= 0 &&
		d->cylinders > 0 && positive(d->displacementLitres);
}
