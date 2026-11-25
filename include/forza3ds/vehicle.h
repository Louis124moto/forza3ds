/* Forza3DS - vehicle mechanical specification and runtime state. */
#ifndef F3DS_VEHICLE_H
#define F3DS_VEHICLE_H

#include "common.h"

#define F3DS_GEARS 6
#define F3DS_GEARS_ALL 7

typedef enum {
	F3DS_TIRE_ROAD = 0,
	F3DS_TIRE_SPORT,
	F3DS_TIRE_SEMISLICK,
	F3DS_TIRE_SLICK,
	F3DS_TIRE_RALLY,
	F3DS_TIRE_OFFROAD,
	F3DS_TIRE_COUNT
} F3DSTireType;

typedef struct {
	/* Chassis and running gear: SI units. */
	float mass, wheelRadius, wheelBase, cgHeight;
	float maxSteerAngle, steerSpeedDrop, drag, rollResist;
	float brakeForce, handbrakeForce, gripLat, gripLatHandbrake, yawAuthority;
	float springRate, damperCompression, damperRebound, rideHeight;
	float tireGrip;
	F3DSTireType tireType;
	float diffFrontLock, diffCenterLock, diffRearLock, frontTorqueShare;

	/* Engine and replaceable parts. */
	float idleRpm, maxRpm, torqueBase, torqueFallRpm;
	float turboBoostBar;
	int turboInstalled;
	int intakeLevel, exhaustLevel, camshaftLevel, injectorLevel;
	int cylinders;
	float displacementLitres;
	float shiftUpRpm, shiftDownRpm, shiftTime;
	float gearRatioR, finalDrive, drivetrainEff;
	int gearCount;
	float gearRatios[F3DS_GEARS];
} F3DSVehicleDef;

typedef struct {
	int visualModelId;
	FVec3 pos;
	float yaw;
	float vx, vz, vy;
	bool airborne;
	float yawRate, steerAngle;
	int gear;
	float rpm, engineLoad, throttle;
	float shiftTimer, speed;
	float suspensionTravel;
} F3DSVehicleState;

void vehicle_def_default(F3DSVehicleDef *d);
float vehicle_engine_torque(const F3DSVehicleDef *d, float rpm);
float vehicle_sound_pitch(const F3DSVehicleDef *d, float rpm);
float vehicle_sound_timbre(const F3DSVehicleDef *d);

#endif
