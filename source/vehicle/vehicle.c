#include "vehicle.h"
#include <math.h>

void vehicle_def_default(F3DSVehicleDef *d)
{
	/* A balanced rear-drive prototype; all dimensions use SI units. */
	d->mass = 1280.0f;
	d->wheelRadius = 0.315f;
	d->wheelBase = 2.62f;
	d->cgHeight = 0.52f;
	d->maxSteerAngle = 0.58f;
	d->steerSpeedDrop = 0.018f;
	d->drag = 0.42f;
	d->rollResist = 13.0f;
	d->brakeForce = 11800.0f;
	d->handbrakeForce = 3900.0f;
	d->gripLat = 7.0f;
	d->gripLatHandbrake = 1.65f;
	d->yawAuthority = 0.82f;
	d->springRate = 28000.0f;
	d->damperCompression = 3200.0f;
	d->damperRebound = 4100.0f;
	d->rideHeight = 0.16f;
	d->tireGrip = 1.0f;
	d->tireType = F3DS_TIRE_ROAD;
	d->diffFrontLock = 0.0f;
	d->diffCenterLock = 0.0f;
	d->diffRearLock = 0.28f;
	d->frontTorqueShare = 0.0f;

	d->idleRpm = 850.0f;
	d->maxRpm = 7200.0f;
	d->torqueBase = 245.0f;
	d->torqueFallRpm = 5400.0f;
	d->turboBoostBar = 0.0f;
	d->turboInstalled = 0;
	d->intakeLevel = 0;
	d->exhaustLevel = 0;
	d->camshaftLevel = 0;
	d->injectorLevel = 0;
	d->cylinders = 4;
	d->displacementLitres = 2.0f;
	d->shiftUpRpm = 6500.0f;
	d->shiftDownRpm = 1900.0f;
	d->shiftTime = 0.18f;
	d->gearRatioR = -3.25f;
	d->finalDrive = 3.73f;
	d->drivetrainEff = 0.88f;
	d->gearCount = 6;
	d->gearRatios[0] = 3.55f;
	d->gearRatios[1] = 2.18f;
	d->gearRatios[2] = 1.52f;
	d->gearRatios[3] = 1.18f;
	d->gearRatios[4] = 0.92f;
	d->gearRatios[5] = 0.74f;
}

float vehicle_engine_torque(const F3DSVehicleDef *d, float rpm)
{
	const float span = fmaxf(d->maxRpm - d->torqueFallRpm, 1.0f);
	const float fall = clampf((rpm - d->torqueFallRpm) / span, 0.0f, 1.0f);
	const float turbo = d->turboInstalled ? 1.0f + fmaxf(d->turboBoostBar, 0.0f) * 0.115f : 1.0f;
	const float breathing = 1.0f + d->intakeLevel * 0.035f + d->exhaustLevel * 0.045f
		+ d->camshaftLevel * 0.065f + d->injectorLevel * 0.025f;
	const float curve = 1.0f - 0.38f * fall;
	return d->torqueBase * turbo * breathing * curve;
}

float vehicle_sound_pitch(const F3DSVehicleDef *d, float rpm)
{
	const float normalized = clampf(rpm / fmaxf(d->maxRpm, 1.0f), 0.0f, 1.5f);
	const float cylinderFactor = (float)d->cylinders / 4.0f;
	return 0.65f + normalized * (1.7f + cylinderFactor * 0.22f);
}

float vehicle_sound_timbre(const F3DSVehicleDef *d)
{
	return clampf(0.18f + d->exhaustLevel * 0.12f + d->intakeLevel * 0.07f
		+ (d->turboInstalled ? 0.16f : 0.0f), 0.05f, 1.0f);
}
