#include "vehicle.h"

void vehicle_def_default(F3DSVehicleDef *d)
{
	/* Prototype sports car. All values in SI units. */
	d->mass = 1200.0f;
	d->wheelRadius = 0.31f;
	d->wheelBase = 2.6f;
	d->cgHeight = 0.5f;
	d->maxSteerAngle = 0.6f;
	d->steerSpeedDrop = 0.02f;
	d->drag = 0.42f;
	d->rollResist = 12.0f;
	d->brakeForce = 10000.0f;
	d->handbrakeForce = 3500.0f;
	d->gripLat = 6.0f;
	d->gripLatHandbrake = 1.8f;
	d->yawAuthority = 0.85f;

	d->idleRpm = 900.0f;
	d->maxRpm = 7200.0f;
	d->torqueBase = 220.0f;
	d->torqueFallRpm = 5200.0f;
	d->shiftUpRpm = 6200.0f;
	d->shiftDownRpm = 1800.0f;
	d->shiftTime = 0.2f;
	d->gearRatioR = -3.2f;
	d->finalDrive = 3.6f;
	d->drivetrainEff = 0.85f;
	d->gearRatios[0] = 3.4f;
	d->gearRatios[1] = 2.2f;
	d->gearRatios[2] = 1.5f;
	d->gearRatios[3] = 1.1f;
	d->gearRatios[4] = 0.85f;
	d->gearRatios[5] = 0.7f;
}
