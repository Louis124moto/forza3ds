/* Forza3DS - vehicle mechanical definition and runtime state.
 * Visual data lives separately in catalog.h. */
#ifndef F3DS_VEHICLE_H
#define F3DS_VEHICLE_H

#include "common.h"

#define F3DS_MAX_GEARS 10

enum { F3DS_FRONT = 0, F3DS_REAR = 1 };

typedef enum {
	F3DS_TIRE_ROAD = 0,
	F3DS_TIRE_SPORT,
	F3DS_TIRE_SEMISLICK,
	F3DS_TIRE_SLICK,
	F3DS_TIRE_RALLY,
	F3DS_TIRE_OFFROAD,
	F3DS_TIRE_COUNT
} F3DSTireType;

typedef enum {
	F3DS_SURFACE_ASPHALT = 0,
	F3DS_SURFACE_GRAVEL,
	F3DS_SURFACE_GRASS,
	F3DS_SURFACE_COUNT
} F3DSSurface;

/* Physical definition (SI units). Everything below "tunable" can be changed in
 * the workshop and is read by the simulation on the very next step. */
typedef struct {
	/* Chassis. */
	float mass, weightFront, wheelRadius, wheelBase, trackWidth, cgHeight;
	float maxSteerAngle, steerRate;
	float drag, rollResist;

	/* Brakes (tunable). */
	float brakeForce, brakeBiasFront, handbrakeForce;

	/* Suspension per axle (tunable). */
	float rideHeight[2], springRate[2], damperCompression[2], damperRebound[2];

	/* Tires (tunable). */
	F3DSTireType tireType;
	float tireGrip;

	/* Drivetrain (tunable). */
	float frontTorqueShare, diffFrontLock, diffCenterLock, diffRearLock;
	float finalDrive, gearRatioR, drivetrainEff, shiftTime;
	int gearCount;
	int manualTransmission;
	float gearRatios[F3DS_MAX_GEARS];

	/* Engine and replaceable parts (tunable). */
	float idleRpm, maxRpm, torqueBase, torquePeakRpm;
	float turboBoostBar;
	int turboInstalled;
	int intakeLevel, exhaustLevel, camshaftLevel, injectorLevel, pistonLevel;
	int cylinders;
	float displacementLitres;
} F3DSVehicleDef;

/* Runtime state. */
typedef struct {
	int visualModelId;
	FVec3 pos;
	float yaw;
	float vx, vz, vy;
	bool airborne;
	float yawRate;
	float steerAngle;          /* road wheel angle, rad, positive = left */
	int gear;                  /* 0 = reverse, 1..N forward */
	float rpm, engineLoad, throttle;
	float shiftTimer;
	float speed, lateralSpeed; /* body frame, m/s (lateral positive = left) */
	float boost;               /* current turbo pressure, bar */
	float pitch, pitchRate;    /* sprung mass, rad; positive pitch = nose up */
	float roll, rollRate;      /* rad; positive roll = leaning to the right */
	float accelLong, accelLat; /* m/s^2, body frame */
	float wheelLoad[4];        /* N: FL, FR, RL, RR */
	float slip[2];             /* 0..1 sliding amount per axle */
	float wheelSpin;           /* 0..1 driven wheels spinning */
	float wheelAngle;          /* visual wheel rotation, rad */
	float impact;              /* strength of the last collision, m/s */
	bool absActive, tcsActive, wheelsLocked, revLimited;
	F3DSSurface surface;
} F3DSVehicleState;

/* Reset the runtime state at a pose, engine idling in first gear. */
void vehicle_state_reset(F3DSVehicleState *s, const F3DSVehicleDef *d, FVec3 pos, float yaw);

/* Full-load crankshaft torque (Nm) at a given rpm with the given boost. */
float vehicle_torque_boosted(const F3DSVehicleDef *d, float rpm, float boostBar);
/* Steady-state full-load torque (turbo at its configured pressure). */
float vehicle_engine_torque(const F3DSVehicleDef *d, float rpm);
/* Peak power in kW over the rev range (steady state). */
float vehicle_peak_power_kw(const F3DSVehicleDef *d);

float tire_surface_grip(F3DSTireType tire, F3DSSurface surface);
float tire_peak_slip(F3DSTireType tire);
const char *tire_name(F3DSTireType tire);

/* True when every field is finite and physically usable. */
bool vehicle_def_valid(const F3DSVehicleDef *d);

#endif
