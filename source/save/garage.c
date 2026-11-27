#include "garage.h"
#include "save_io.h"
#include <string.h>

#define GARAGE_MAGIC 0x46334752u /* "F3GR" */
#define GARAGE_VERSION 2u
/* Version 1 held the first four cars of the catalogue. */
#define GARAGE_V1_CARS 4

/* Only workshop-tunable values are stored; the rest of each definition comes
 * from the catalogue. Every member is 4 bytes wide, so there is no padding. */
typedef struct {
	float brakeForce, brakeBiasFront, handbrakeForce;
	float rideHeight[2], springRate[2], damperCompression[2], damperRebound[2];
	int32_t tireType;
	float tireGrip;
	float frontTorqueShare, diffFrontLock, diffCenterLock, diffRearLock;
	float finalDrive, gearRatios[F3DS_MAX_GEARS];
	int32_t gearCount, manualTransmission;
	float maxRpm, torqueBase, turboBoostBar;
	int32_t turboInstalled, intake, exhaust, camshaft, injectors, pistons;
	int32_t paint, rim;
} Record;

typedef struct {
	int32_t selected;
	Record car[F3DS_CAR_COUNT];
} File;

typedef struct {
	int32_t selected;
	Record car[GARAGE_V1_CARS];
} FileV1;

#define COPY_FIELDS(dst, src) do { \
	(dst)->brakeForce = (src)->brakeForce; (dst)->brakeBiasFront = (src)->brakeBiasFront; \
	(dst)->handbrakeForce = (src)->handbrakeForce; \
	memcpy((dst)->rideHeight, (src)->rideHeight, sizeof((dst)->rideHeight)); \
	memcpy((dst)->springRate, (src)->springRate, sizeof((dst)->springRate)); \
	memcpy((dst)->damperCompression, (src)->damperCompression, sizeof((dst)->damperCompression)); \
	memcpy((dst)->damperRebound, (src)->damperRebound, sizeof((dst)->damperRebound)); \
	(dst)->tireGrip = (src)->tireGrip; (dst)->frontTorqueShare = (src)->frontTorqueShare; \
	(dst)->diffFrontLock = (src)->diffFrontLock; (dst)->diffCenterLock = (src)->diffCenterLock; \
	(dst)->diffRearLock = (src)->diffRearLock; (dst)->finalDrive = (src)->finalDrive; \
	memcpy((dst)->gearRatios, (src)->gearRatios, sizeof((dst)->gearRatios)); \
	(dst)->gearCount = (src)->gearCount; (dst)->manualTransmission = (src)->manualTransmission; \
	(dst)->maxRpm = (src)->maxRpm; (dst)->torqueBase = (src)->torqueBase; \
	(dst)->turboBoostBar = (src)->turboBoostBar; (dst)->turboInstalled = (src)->turboInstalled; \
} while (0)

void garage_restore_car(F3DSGarage *g, int car)
{
	if ((unsigned)car >= F3DS_CAR_COUNT)
		return;
	catalog_def(car, &g->def[car]);
	g->paint[car] = catalog_visual(car)->defaultPaint;
	g->rim[car] = 0;
}

void garage_defaults(F3DSGarage *g)
{
	memset(g, 0, sizeof(*g));
	for (int i = 0; i < F3DS_CAR_COUNT; ++i)
		garage_restore_car(g, i);
}

/* Applies "count" stored records on top of the catalogue defaults. */
static bool unpack(const Record *records, int count, int32_t selected, F3DSGarage *g)
{
	if (selected < 0 || selected >= count)
		return false;
	garage_defaults(g);
	g->selected = selected;
	for (int i = 0; i < count; ++i) {
		const Record *r = &records[i];
		F3DSVehicleDef *d = &g->def[i];
		COPY_FIELDS(d, r);
		d->tireType = (F3DSTireType)r->tireType;
		d->intakeLevel = r->intake; d->exhaustLevel = r->exhaust; d->camshaftLevel = r->camshaft;
		d->injectorLevel = r->injectors; d->pistonLevel = r->pistons;
		if (!vehicle_def_valid(d) || r->paint < 0 || r->paint >= F3DS_PAINT_COUNT ||
		    r->rim < 0 || r->rim >= F3DS_RIM_COUNT)
			return false;
		g->paint[i] = r->paint;
		g->rim[i] = r->rim;
	}
	return true;
}

static bool valid_payload(const void *p)
{
	const File *f = p;
	F3DSGarage tmp;
	return unpack(f->car, F3DS_CAR_COUNT, f->selected, &tmp);
}

static bool valid_v1(const void *p)
{
	const FileV1 *f = p;
	F3DSGarage tmp;
	return unpack(f->car, GARAGE_V1_CARS, f->selected, &tmp);
}

bool garage_load(F3DSGarage *g, const char *path)
{
	/* The records are too large for the 3DS main thread stack. */
	static File f;
	if (save_read(path, GARAGE_MAGIC, GARAGE_VERSION, &f, sizeof(f), valid_payload))
		return unpack(f.car, F3DS_CAR_COUNT, f.selected, g);
	FileV1 old;
	return save_read(path, GARAGE_MAGIC, 1u, &old, sizeof(old), valid_v1) &&
		unpack(old.car, GARAGE_V1_CARS, old.selected, g);
}

bool garage_save(const F3DSGarage *g, const char *path)
{
	static File f;
	memset(&f, 0, sizeof(f));
	f.selected = g->selected;
	for (int i = 0; i < F3DS_CAR_COUNT; ++i) {
		Record *r = &f.car[i];
		const F3DSVehicleDef *d = &g->def[i];
		COPY_FIELDS(r, d);
		r->tireType = d->tireType;
		r->intake = d->intakeLevel; r->exhaust = d->exhaustLevel; r->camshaft = d->camshaftLevel;
		r->injectors = d->injectorLevel; r->pistons = d->pistonLevel;
		r->paint = g->paint[i];
		r->rim = g->rim[i];
	}
	return save_write(path, GARAGE_MAGIC, GARAGE_VERSION, &f, sizeof(f), valid_payload);
}
