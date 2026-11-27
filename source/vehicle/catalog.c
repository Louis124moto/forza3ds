#include "catalog.h"
#include <string.h>

static const F3DSCarInfo s_info[F3DS_CAR_COUNT] = {
	{ "Kitsune S",     "Coupé sport propulsion, équilibré" },
	{ "Tanuki 4x4",    "Rallye turbo 4 roues motrices" },
	{ "Suzume",        "Citadine légère, traction avant" },
	{ "Raijin V8",     "Grosse propulsion, très coupleuse" },
	{ "Mame K",        "Kei car turbo, minuscule et vive" },
	{ "Hayabusa GT",   "Supercar V10 à moteur central" },
	{ "Shika Roadster", "Cabriolet léger et joueur" },
	{ "Kuma 4x4",      "Gros tout-terrain, passe partout" },
	{ "Yagi Pick-up",  "Pick-up diesel, benne et couple" },
	{ "Tsubame R",     "Berline turbo 4 roues motrices" },
	{ "Shiba Break",   "Break familial turbo et stable" },
	{ "Ryu V12",       "Grand tourisme V12, longues courbes" },
	{ "Inago Buggy",   "Buggy léger, longs débattements" },
	{ "Koi Van",       "Fourgon haut, lourd et pataud" },
	{ "Kame 1200",     "Ancienne propulsion, freins modestes" },
	{ "Ookami Drift",  "Coupé turbo préparé pour la glisse" },
};

#define V(hw, hl, fy, by, ry, cf, cr, rf, rr, whw, sx, ey, ez, nd, td, st, tr, paint) \
	{ .halfWidth = hw, .halfLength = hl, .floorY = fy, .beltY = by, .roofY = ry, \
	  .cabinFront = cf, .cabinRear = cr, .roofFront = rf, .roofRear = rr, .wheelHalfWidth = whw, \
	  .seatX = sx, .eyeY = ey, .eyeZ = ez, .noseDrop = nd, .tailDrop = td, .style = st, \
	  .trim = tr, .defaultPaint = paint }

static const F3DSCarVisual s_visual[F3DS_CAR_COUNT] = {
	/*  hw     hl    floor belt  roof   cabF   cabR   roofF  roofR  tyre   seatX  eyeY  eyeZ */
	V(0.90f, 2.10f, 0.30f, 0.86f, 1.38f, 0.62f, -1.35f, 0.05f, -0.85f, 0.13f, -0.38f, 1.17f, -0.30f,
	  0.12f, 0.02f, F3DS_BODY_COUPE, F3DS_TRIM_LIP, 0),
	V(0.92f, 2.00f, 0.38f, 1.00f, 1.62f, 0.60f, -1.75f, 0.10f, -1.55f, 0.14f, -0.40f, 1.35f, -0.28f,
	  0.10f, 0.00f, F3DS_BODY_HATCH,
	  F3DS_TRIM_WING | F3DS_TRIM_FLARES | F3DS_TRIM_LIGHT_POD | F3DS_TRIM_SCOOP, 1),
	V(0.80f, 1.75f, 0.28f, 0.90f, 1.50f, 0.55f, -1.60f, 0.05f, -1.40f, 0.10f, -0.34f, 1.25f, -0.25f,
	  0.14f, 0.00f, F3DS_BODY_HATCH, F3DS_TRIM_ROUND_LIGHTS, 3),
	V(0.98f, 2.40f, 0.32f, 0.90f, 1.40f, 0.35f, -1.55f, -0.25f, -1.05f, 0.16f, -0.42f, 1.19f, -0.55f,
	  0.06f, 0.04f, F3DS_BODY_MUSCLE, F3DS_TRIM_STRIPES | F3DS_TRIM_SCOOP | F3DS_TRIM_LIP, 5),
	V(0.72f, 1.58f, 0.30f, 0.95f, 1.74f, 0.80f, -1.48f, 0.48f, -1.42f, 0.09f, -0.30f, 1.44f, -0.05f,
	  0.16f, 0.00f, F3DS_BODY_KEI, F3DS_TRIM_TWO_TONE | F3DS_TRIM_ROUND_LIGHTS, 10),
	V(1.00f, 2.22f, 0.18f, 0.70f, 1.14f, 0.95f, -0.80f, 0.15f, -0.40f, 0.17f, -0.40f, 0.97f, 0.00f,
	  0.22f, 0.00f, F3DS_BODY_SUPERCAR, F3DS_TRIM_WING | F3DS_TRIM_FLARES, 6),
	V(0.84f, 1.95f, 0.27f, 0.80f, 1.22f, 0.45f, -1.05f, 0.02f, -0.30f, 0.11f, -0.34f, 1.12f, -0.40f,
	  0.14f, 0.06f, F3DS_BODY_ROADSTER, F3DS_TRIM_ROLL_BAR | F3DS_TRIM_ROUND_LIGHTS, 4),
	V(1.02f, 2.35f, 0.52f, 1.22f, 1.98f, 0.75f, -2.20f, 0.42f, -2.14f, 0.17f, -0.44f, 1.68f, -0.10f,
	  0.06f, 0.00f, F3DS_BODY_SUV,
	  F3DS_TRIM_ROOF_RACK | F3DS_TRIM_SPARE | F3DS_TRIM_FLARES | F3DS_TRIM_TWO_TONE, 11),
	V(0.98f, 2.62f, 0.50f, 1.16f, 1.86f, 0.95f, -0.62f, 0.52f, -0.56f, 0.15f, -0.44f, 1.58f, 0.10f,
	  0.07f, 0.00f, F3DS_BODY_PICKUP, F3DS_TRIM_ROLL_BAR | F3DS_TRIM_TWO_TONE | F3DS_TRIM_LIGHT_POD, 2),
	V(0.91f, 2.30f, 0.30f, 0.90f, 1.46f, 0.70f, -1.40f, 0.14f, -0.92f, 0.13f, -0.39f, 1.24f, -0.22f,
	  0.10f, 0.02f, F3DS_BODY_SEDAN, F3DS_TRIM_WING | F3DS_TRIM_SCOOP, 8),
	V(0.90f, 2.35f, 0.31f, 0.92f, 1.52f, 0.72f, -2.20f, 0.15f, -2.08f, 0.12f, -0.38f, 1.28f, -0.20f,
	  0.10f, 0.00f, F3DS_BODY_WAGON, F3DS_TRIM_ROOF_RACK, 7),
	V(0.99f, 2.45f, 0.24f, 0.80f, 1.30f, 0.30f, -1.35f, -0.28f, -0.90f, 0.16f, -0.42f, 1.10f, -0.62f,
	  0.14f, 0.05f, F3DS_BODY_GT, F3DS_TRIM_LIP | F3DS_TRIM_STRIPES, 1),
	V(0.86f, 1.70f, 0.48f, 0.82f, 1.66f, 0.55f, -0.85f, 0.18f, -0.62f, 0.18f, -0.32f, 1.34f, -0.30f,
	  0.12f, 0.00f, F3DS_BODY_BUGGY,
	  F3DS_TRIM_ROLL_BAR | F3DS_TRIM_LIGHT_POD | F3DS_TRIM_ROUND_LIGHTS, 6),
	V(0.95f, 2.45f, 0.40f, 1.18f, 2.24f, 1.75f, -2.36f, 1.20f, -2.34f, 0.12f, -0.44f, 1.76f, 0.80f,
	  0.22f, 0.00f, F3DS_BODY_VAN, F3DS_TRIM_ROOF_RACK, 2),
	V(0.78f, 1.82f, 0.33f, 0.88f, 1.44f, 0.42f, -1.10f, 0.08f, -0.84f, 0.09f, -0.32f, 1.21f, -0.34f,
	  0.05f, 0.04f, F3DS_BODY_CLASSIC, F3DS_TRIM_ROUND_LIGHTS | F3DS_TRIM_TWO_TONE, 9),
	V(0.92f, 2.18f, 0.24f, 0.82f, 1.34f, 0.58f, -1.32f, 0.02f, -0.82f, 0.14f, -0.38f, 1.14f, -0.34f,
	  0.13f, 0.02f, F3DS_BODY_COUPE,
	  F3DS_TRIM_WING | F3DS_TRIM_FLARES | F3DS_TRIM_STRIPES | F3DS_TRIM_SCOOP, 5),
};

const F3DSCarInfo *catalog_info(int car)
{
	return &s_info[(unsigned)car < F3DS_CAR_COUNT ? car : 0];
}

const F3DSCarVisual *catalog_visual(int car)
{
	return &s_visual[(unsigned)car < F3DS_CAR_COUNT ? car : 0];
}

bool catalog_open_top(const F3DSCarVisual *v)
{
	return v->style == F3DS_BODY_ROADSTER || v->style == F3DS_BODY_BUGGY;
}

/* Mechanical data, one row per car. Suspension values are front/rear. */
typedef struct {
	float mass, weightFront, wheelRadius, wheelBase, trackWidth, cgHeight;
	float maxSteer, drag, brake, handbrake;
	float ride[2], spring[2], bump[2], rebound[2];
	F3DSTireType tire;
	float grip, frontShare, lockFront, lockCenter, lockRear;
	float finalDrive, shiftTime;
	int gears;
	float idle, max, torque, peak, boost; /* boost 0 = no turbo */
	int cylinders;
	float litres;
} Spec;

static const Spec s_spec[F3DS_CAR_COUNT] = {
	/* 0 Kitsune S */
	{ 1280, 0.52f, 0.315f, 2.62f, 1.55f, 0.50f, 0.58f, 0.42f, 14500, 6500,
	  { 0.14f, 0.14f }, { 32000, 32000 }, { 2800, 2800 }, { 3900, 3900 },
	  F3DS_TIRE_ROAD, 1.0f, 0.0f, 0.0f, 0.0f, 0.28f, 3.73f, 0.18f, 6, 850, 7200, 245, 4600, 0, 4, 2.0f },
	/* 1 Tanuki 4x4 */
	{ 1390, 0.57f, 0.325f, 2.54f, 1.55f, 0.55f, 0.58f, 0.42f, 14500, 6500,
	  { 0.19f, 0.19f }, { 29000, 29000 }, { 3200, 3200 }, { 4300, 4300 },
	  F3DS_TIRE_RALLY, 1.0f, 0.45f, 0.20f, 0.45f, 0.35f, 4.10f, 0.18f, 6, 850, 7000, 215, 3800, 0.7f, 4, 2.0f },
	/* 2 Suzume */
	{ 920, 0.62f, 0.290f, 2.38f, 1.42f, 0.52f, 0.58f, 0.42f, 10200, 4800,
	  { 0.14f, 0.14f }, { 23000, 23000 }, { 2000, 2000 }, { 2800, 2800 },
	  F3DS_TIRE_ROAD, 1.0f, 1.0f, 0.10f, 0.0f, 0.0f, 4.06f, 0.18f, 5, 900, 6800, 128, 4400, 0, 4, 1.3f },
	/* 3 Raijin V8 */
	{ 1640, 0.54f, 0.340f, 2.80f, 1.64f, 0.50f, 0.55f, 0.46f, 18500, 8000,
	  { 0.14f, 0.14f }, { 40000, 40000 }, { 3800, 3800 }, { 5200, 5200 },
	  F3DS_TIRE_SPORT, 1.0f, 0.0f, 0.0f, 0.0f, 0.35f, 3.15f, 0.22f, 6, 700, 6500, 475, 3900, 0, 8, 5.0f },
	/* 4 Mame K */
	{ 760, 0.60f, 0.270f, 2.30f, 1.28f, 0.58f, 0.62f, 0.50f, 8400, 3900,
	  { 0.14f, 0.14f }, { 21000, 19000 }, { 1700, 1600 }, { 2400, 2300 },
	  F3DS_TIRE_ROAD, 0.96f, 1.0f, 0.12f, 0.0f, 0.0f, 4.75f, 0.20f, 5, 950, 7400, 74, 3600, 0.6f, 3, 0.66f },
	/* 5 Hayabusa GT */
	{ 1390, 0.42f, 0.345f, 2.66f, 1.70f, 0.40f, 0.52f, 0.36f, 22000, 8000,
	  { 0.10f, 0.11f }, { 62000, 70000 }, { 5200, 5600 }, { 7200, 7800 },
	  F3DS_TIRE_SEMISLICK, 1.02f, 0.0f, 0.0f, 0.0f, 0.45f, 3.55f, 0.09f, 7, 950, 8600, 540, 6200, 0, 10, 5.2f },
	/* 6 Shika Roadster */
	{ 980, 0.50f, 0.300f, 2.33f, 1.47f, 0.45f, 0.62f, 0.44f, 11500, 5600,
	  { 0.13f, 0.13f }, { 27000, 25000 }, { 2400, 2300 }, { 3400, 3200 },
	  F3DS_TIRE_SPORT, 0.98f, 0.0f, 0.0f, 0.0f, 0.30f, 4.10f, 0.17f, 6, 900, 7600, 150, 5200, 0, 4, 1.6f },
	/* 7 Kuma 4x4 */
	{ 2150, 0.53f, 0.400f, 2.85f, 1.68f, 0.82f, 0.60f, 0.72f, 21000, 8500,
	  { 0.27f, 0.27f }, { 42000, 44000 }, { 4600, 4800 }, { 6200, 6500 },
	  F3DS_TIRE_OFFROAD, 1.0f, 0.45f, 0.35f, 0.70f, 0.55f, 4.30f, 0.30f, 6, 750, 5200, 430, 2600, 0.5f, 6, 3.0f },
	/* 8 Yagi Pick-up */
	{ 1890, 0.58f, 0.385f, 3.15f, 1.62f, 0.76f, 0.57f, 0.70f, 18500, 8000,
	  { 0.24f, 0.26f }, { 34000, 48000 }, { 3800, 4800 }, { 5200, 6200 },
	  F3DS_TIRE_OFFROAD, 0.98f, 0.0f, 0.0f, 0.0f, 0.60f, 3.90f, 0.32f, 6, 750, 4600, 300, 2200, 0.8f, 4, 2.8f },
	/* 9 Tsubame R */
	{ 1480, 0.58f, 0.325f, 2.68f, 1.58f, 0.49f, 0.58f, 0.40f, 17500, 7000,
	  { 0.13f, 0.13f }, { 42000, 39000 }, { 3700, 3500 }, { 5100, 4900 },
	  F3DS_TIRE_SPORT, 1.02f, 0.40f, 0.20f, 0.35f, 0.40f, 3.90f, 0.14f, 6, 850, 7200, 270, 3600, 0.9f, 4, 2.0f },
	/* 10 Shiba Break */
	{ 1560, 0.55f, 0.320f, 2.74f, 1.55f, 0.55f, 0.57f, 0.45f, 15500, 6800,
	  { 0.15f, 0.15f }, { 30000, 34000 }, { 2900, 3200 }, { 4000, 4400 },
	  F3DS_TIRE_ROAD, 1.0f, 0.40f, 0.10f, 0.25f, 0.20f, 3.75f, 0.24f, 6, 800, 6500, 210, 3000, 0.6f, 4, 2.0f },
	/* 11 Ryu V12 */
	{ 1790, 0.52f, 0.350f, 2.90f, 1.66f, 0.46f, 0.54f, 0.38f, 22500, 8500,
	  { 0.12f, 0.12f }, { 48000, 50000 }, { 4400, 4600 }, { 6000, 6300 },
	  F3DS_TIRE_SPORT, 1.04f, 0.0f, 0.0f, 0.0f, 0.38f, 3.30f, 0.12f, 7, 750, 7800, 620, 5000, 0, 12, 6.0f },
	/* 12 Inago Buggy */
	{ 660, 0.40f, 0.360f, 2.25f, 1.52f, 0.62f, 0.66f, 0.78f, 9000, 5200,
	  { 0.30f, 0.32f }, { 15000, 19000 }, { 1900, 2300 }, { 2700, 3200 },
	  F3DS_TIRE_OFFROAD, 1.02f, 0.0f, 0.0f, 0.0f, 0.50f, 4.90f, 0.16f, 5, 1100, 8200, 112, 5600, 0, 4, 1.0f },
	/* 13 Koi Van */
	{ 1960, 0.55f, 0.330f, 3.20f, 1.65f, 0.86f, 0.60f, 0.80f, 16000, 6500,
	  { 0.18f, 0.19f }, { 33000, 42000 }, { 3200, 4000 }, { 4400, 5400 },
	  F3DS_TIRE_ROAD, 0.92f, 1.0f, 0.10f, 0.0f, 0.0f, 4.20f, 0.35f, 6, 750, 4800, 250, 2300, 0.4f, 4, 2.4f },
	/* 14 Kame 1200 */
	{ 830, 0.53f, 0.285f, 2.30f, 1.34f, 0.54f, 0.60f, 0.52f, 7200, 3600,
	  { 0.16f, 0.16f }, { 18000, 20000 }, { 1400, 1500 }, { 1900, 2100 },
	  F3DS_TIRE_ROAD, 0.86f, 0.0f, 0.0f, 0.0f, 0.05f, 4.30f, 0.30f, 4, 850, 6200, 94, 3400, 0, 4, 1.2f },
	/* 15 Ookami Drift */
	{ 1270, 0.53f, 0.318f, 2.57f, 1.56f, 0.46f, 0.74f, 0.45f, 15000, 11000,
	  { 0.11f, 0.11f }, { 46000, 40000 }, { 3900, 3500 }, { 5300, 4800 },
	  F3DS_TIRE_SPORT, 0.98f, 0.0f, 0.0f, 0.0f, 0.90f, 4.08f, 0.13f, 6, 900, 7600, 262, 4200, 0.6f, 6, 2.5f },
};

void catalog_def(int car, F3DSVehicleDef *d)
{
	const Spec *s = &s_spec[(unsigned)car < F3DS_CAR_COUNT ? car : 0];
	memset(d, 0, sizeof(*d));
	d->mass = s->mass; d->weightFront = s->weightFront; d->wheelRadius = s->wheelRadius;
	d->wheelBase = s->wheelBase; d->trackWidth = s->trackWidth; d->cgHeight = s->cgHeight;
	d->maxSteerAngle = s->maxSteer;
	d->steerRate = s->maxSteer > 0.65f ? 4.2f : 3.2f;
	d->drag = s->drag; d->rollResist = 13.0f * s->mass / 1280.0f;
	d->brakeForce = s->brake; d->brakeBiasFront = s->weightFront < 0.46f ? 0.58f : 0.64f;
	d->handbrakeForce = s->handbrake;
	for (int a = 0; a < 2; ++a) {
		d->rideHeight[a] = s->ride[a];
		d->springRate[a] = s->spring[a];
		d->damperCompression[a] = s->bump[a];
		d->damperRebound[a] = s->rebound[a];
	}
	d->tireType = s->tire; d->tireGrip = s->grip;
	d->frontTorqueShare = s->frontShare; d->diffFrontLock = s->lockFront;
	d->diffCenterLock = s->lockCenter; d->diffRearLock = s->lockRear;
	d->finalDrive = s->finalDrive; d->gearRatioR = -3.25f; d->drivetrainEff = 0.88f;
	d->shiftTime = s->shiftTime; d->gearCount = s->gears;
	/* Close ratios for the sporty 7-speed boxes, wide ones for old 4-speeds. */
	static const float six[F3DS_MAX_GEARS] = {
		3.55f, 2.18f, 1.52f, 1.18f, 0.94f, 0.78f, 0.66f, 0.58f, 0.52f, 0.47f
	};
	static const float seven[F3DS_MAX_GEARS] = {
		3.30f, 2.20f, 1.68f, 1.33f, 1.08f, 0.89f, 0.73f, 0.62f, 0.54f, 0.48f
	};
	static const float four[F3DS_MAX_GEARS] = {
		3.60f, 2.10f, 1.40f, 1.00f, 0.82f, 0.70f, 0.61f, 0.55f, 0.50f, 0.46f
	};
	memcpy(d->gearRatios, s->gears == 7 ? seven : s->gears == 4 ? four : six, sizeof(six));
	d->idleRpm = s->idle; d->maxRpm = s->max;
	d->torqueBase = s->torque; d->torquePeakRpm = s->peak;
	d->turboInstalled = s->boost > 0.0f; d->turboBoostBar = s->boost;
	d->cylinders = s->cylinders; d->displacementLitres = s->litres;
}

static const char *s_paintNames[F3DS_PAINT_COUNT] = {
	"Rouge", "Bleu", "Blanc", "Jaune", "Vert", "Noir", "Orange", "Violet",
	"Gris", "Crème", "Turquoise", "Brun"
};
static const float s_paints[F3DS_PAINT_COUNT][3] = {
	{ 0.85f, 0.12f, 0.10f }, { 0.10f, 0.32f, 0.85f }, { 0.90f, 0.91f, 0.93f }, { 0.95f, 0.78f, 0.08f },
	{ 0.08f, 0.55f, 0.28f }, { 0.12f, 0.12f, 0.14f }, { 0.95f, 0.46f, 0.05f }, { 0.48f, 0.18f, 0.70f },
	{ 0.46f, 0.49f, 0.53f }, { 0.93f, 0.87f, 0.68f }, { 0.07f, 0.72f, 0.74f }, { 0.42f, 0.26f, 0.15f },
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
