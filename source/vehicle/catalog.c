#include "catalog.h"
#include <string.h>

static const F3DSCarInfo s_info[F3DS_CAR_COUNT] = {
	{ "Kitsune S",   "Coupé sport propulsion, équilibré" },
	{ "Tanuki 4x4",  "Rallye turbo 4 roues motrices" },
	{ "Suzume",      "Citadine légère, traction avant" },
	{ "Raijin V8",   "Grosse propulsion, beaucoup de couple" },
};

static const F3DSCarVisual s_visual[F3DS_CAR_COUNT] = {
	{ .halfWidth = 0.90f, .halfLength = 2.10f, .floorY = 0.30f, .beltY = 0.86f, .roofY = 1.38f,
	  .cabinFront = 0.62f, .cabinRear = -1.35f, .roofFront = 0.05f, .roofRear = -0.85f,
	  .wheelHalfWidth = 0.13f, .seatX = -0.38f, .eyeY = 1.17f, .eyeZ = -0.30f,
	  .spoiler = 1, .defaultPaint = 0 },
	{ .halfWidth = 0.92f, .halfLength = 2.00f, .floorY = 0.38f, .beltY = 1.00f, .roofY = 1.62f,
	  .cabinFront = 0.60f, .cabinRear = -1.75f, .roofFront = 0.10f, .roofRear = -1.55f,
	  .wheelHalfWidth = 0.14f, .seatX = -0.40f, .eyeY = 1.35f, .eyeZ = -0.28f,
	  .spoiler = 1, .defaultPaint = 1 },
	{ .halfWidth = 0.80f, .halfLength = 1.75f, .floorY = 0.28f, .beltY = 0.90f, .roofY = 1.50f,
	  .cabinFront = 0.55f, .cabinRear = -1.60f, .roofFront = 0.05f, .roofRear = -1.40f,
	  .wheelHalfWidth = 0.10f, .seatX = -0.34f, .eyeY = 1.25f, .eyeZ = -0.25f,
	  .spoiler = 0, .defaultPaint = 3 },
	{ .halfWidth = 0.98f, .halfLength = 2.40f, .floorY = 0.32f, .beltY = 0.90f, .roofY = 1.40f,
	  .cabinFront = 0.35f, .cabinRear = -1.55f, .roofFront = -0.25f, .roofRear = -1.05f,
	  .wheelHalfWidth = 0.16f, .seatX = -0.42f, .eyeY = 1.19f, .eyeZ = -0.55f,
	  .spoiler = 0, .defaultPaint = 5 },
};

const F3DSCarInfo *catalog_info(int car)
{
	return &s_info[(unsigned)car < F3DS_CAR_COUNT ? car : 0];
}

const F3DSCarVisual *catalog_visual(int car)
{
	return &s_visual[(unsigned)car < F3DS_CAR_COUNT ? car : 0];
}

static void base_def(F3DSVehicleDef *d)
{
	memset(d, 0, sizeof(*d));
	d->mass = 1280.0f; d->weightFront = 0.52f; d->wheelRadius = 0.315f;
	d->wheelBase = 2.62f; d->trackWidth = 1.55f; d->cgHeight = 0.50f;
	d->maxSteerAngle = 0.58f; d->steerRate = 3.2f;
	d->drag = 0.42f; d->rollResist = 13.0f;
	d->brakeForce = 14500.0f; d->brakeBiasFront = 0.64f; d->handbrakeForce = 6500.0f;
	for (int a = 0; a < 2; ++a) {
		d->rideHeight[a] = 0.14f;
		d->springRate[a] = 32000.0f;
		d->damperCompression[a] = 2800.0f;
		d->damperRebound[a] = 3900.0f;
	}
	d->tireType = F3DS_TIRE_ROAD; d->tireGrip = 1.0f;
	d->frontTorqueShare = 0.0f; d->diffFrontLock = 0.0f;
	d->diffCenterLock = 0.0f; d->diffRearLock = 0.28f;
	d->finalDrive = 3.73f; d->gearRatioR = -3.25f; d->drivetrainEff = 0.88f;
	d->shiftTime = 0.18f; d->gearCount = 6;
	static const float ratios[F3DS_MAX_GEARS] = {
		3.55f, 2.18f, 1.52f, 1.18f, 0.94f, 0.78f, 0.66f, 0.58f, 0.52f, 0.47f
	};
	memcpy(d->gearRatios, ratios, sizeof(ratios));
	d->idleRpm = 850.0f; d->maxRpm = 7200.0f;
	d->torqueBase = 245.0f; d->torquePeakRpm = 4600.0f;
	d->turboBoostBar = 0.0f; d->turboInstalled = 0;
	d->cylinders = 4; d->displacementLitres = 2.0f;
}

void catalog_def(int car, F3DSVehicleDef *d)
{
	base_def(d);
	switch (car) {
	case 1: /* Tanuki 4x4 */
		d->mass = 1390.0f; d->weightFront = 0.57f; d->wheelRadius = 0.325f;
		d->wheelBase = 2.54f; d->cgHeight = 0.55f;
		for (int a = 0; a < 2; ++a) {
			d->rideHeight[a] = 0.19f; d->springRate[a] = 29000.0f;
			d->damperCompression[a] = 3200.0f; d->damperRebound[a] = 4300.0f;
		}
		d->tireType = F3DS_TIRE_RALLY;
		d->frontTorqueShare = 0.45f; d->diffFrontLock = 0.20f;
		d->diffCenterLock = 0.45f; d->diffRearLock = 0.35f;
		d->finalDrive = 4.10f;
		d->torqueBase = 215.0f; d->torquePeakRpm = 3800.0f; d->maxRpm = 7000.0f;
		d->turboInstalled = 1; d->turboBoostBar = 0.7f;
		break;
	case 2: /* Suzume */
		d->mass = 920.0f; d->weightFront = 0.62f; d->wheelRadius = 0.290f;
		d->wheelBase = 2.38f; d->trackWidth = 1.42f; d->cgHeight = 0.52f;
		d->brakeForce = 10200.0f; d->handbrakeForce = 4800.0f;
		for (int a = 0; a < 2; ++a) {
			d->springRate[a] = 23000.0f;
			d->damperCompression[a] = 2000.0f; d->damperRebound[a] = 2800.0f;
		}
		d->frontTorqueShare = 1.0f; d->diffRearLock = 0.0f; d->diffFrontLock = 0.10f;
		d->finalDrive = 4.06f; d->gearCount = 5;
		d->torqueBase = 128.0f; d->torquePeakRpm = 4400.0f; d->maxRpm = 6800.0f;
		d->displacementLitres = 1.3f; d->idleRpm = 900.0f;
		break;
	case 3: /* Raijin V8 */
		d->mass = 1640.0f; d->weightFront = 0.54f; d->wheelRadius = 0.340f;
		d->wheelBase = 2.80f; d->trackWidth = 1.64f; d->cgHeight = 0.50f;
		d->maxSteerAngle = 0.55f; d->drag = 0.46f;
		d->brakeForce = 18500.0f; d->handbrakeForce = 8000.0f;
		for (int a = 0; a < 2; ++a) {
			d->springRate[a] = 40000.0f;
			d->damperCompression[a] = 3800.0f; d->damperRebound[a] = 5200.0f;
		}
		d->tireType = F3DS_TIRE_SPORT; d->diffRearLock = 0.35f;
		d->finalDrive = 3.15f; d->shiftTime = 0.22f;
		d->torqueBase = 475.0f; d->torquePeakRpm = 3900.0f; d->maxRpm = 6500.0f;
		d->cylinders = 8; d->displacementLitres = 5.0f; d->idleRpm = 700.0f;
		break;
	default:
		break;
	}
}

static const char *s_paintNames[F3DS_PAINT_COUNT] = {
	"Rouge", "Bleu", "Blanc", "Jaune", "Vert", "Noir", "Orange", "Violet"
};
static const float s_paints[F3DS_PAINT_COUNT][3] = {
	{ 0.85f, 0.12f, 0.10f }, { 0.10f, 0.32f, 0.85f }, { 0.90f, 0.91f, 0.93f }, { 0.95f, 0.78f, 0.08f },
	{ 0.08f, 0.55f, 0.28f }, { 0.12f, 0.12f, 0.14f }, { 0.95f, 0.46f, 0.05f }, { 0.48f, 0.18f, 0.70f },
};
static const char *s_rimNames[F3DS_RIM_COUNT] = { "Argent", "Noir", "Or", "Blanc" };
static const float s_rims[F3DS_RIM_COUNT][3] = {
	{ 0.66f, 0.68f, 0.72f }, { 0.20f, 0.20f, 0.22f }, { 0.82f, 0.64f, 0.18f }, { 0.94f, 0.94f, 0.94f },
};

const char *catalog_paint_name(int p) { return s_paintNames[(unsigned)p < F3DS_PAINT_COUNT ? p : 0]; }
const char *catalog_rim_name(int r) { return s_rimNames[(unsigned)r < F3DS_RIM_COUNT ? r : 0]; }
void catalog_paint_color(int p, float rgb[3])
{
	memcpy(rgb, s_paints[(unsigned)p < F3DS_PAINT_COUNT ? p : 0], sizeof(float) * 3);
}
void catalog_rim_color(int r, float rgb[3])
{
	memcpy(rgb, s_rims[(unsigned)r < F3DS_RIM_COUNT ? r : 0], sizeof(float) * 3);
}
