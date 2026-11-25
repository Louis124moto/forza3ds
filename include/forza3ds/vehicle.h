#ifndef F3DS_VEHICLE_H
#define F3DS_VEHICLE_H

#include "common.h"

#define F3DS_GEARS 6
#define F3DS_GEARS_ALL 7

typedef struct {
	float mass;
	float wheelRadius;
	float wheelBase;
	float cgHeight;
	float maxSteerAngle;
	float steerSpeedDrop;
	float drag;
	float rollResist;
	float brakeForce;
	float handbrakeForce;
	float gripLat;
	float gripLatHandbrake;
	float yawAuthority;

	float idleRpm;
	float maxRpm;
	float torqueBase;
	float torqueFallRpm;
	float shiftUpRpm;
	float shiftDownRpm;
	float shiftTime;
	float gearRatioR;
	float finalDrive;
	float drivetrainEff;
	float gearRatios[F3DS_GEARS];
} F3DSVehicleDef;

typedef struct {
	int visualModelId;

	FVec3 pos;
	float yaw;
	float vx, vz;
	float yawRate;
	float steerAngle;
	int gear;
	float rpm;
	float shiftTimer;
	float speed;
} F3DSVehicleState;

void vehicle_def_default(F3DSVehicleDef *d);

#endif
