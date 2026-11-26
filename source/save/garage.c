#include "garage.h"
#include "save_io.h"
#include <string.h>

#define GARAGE_MAGIC 0x46334752u /* "F3GR" */
#define GARAGE_VERSION 1u

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

static bool unpack(const File *f, F3DSGarage *g)
{
	if (f->selected < 0 || f->selected >= F3DS_CAR_COUNT)
		return false;
	garage_defaults(g);
	g->selected = f->selected;
	for (int i = 0; i < F3DS_CAR_COUNT; ++i) {
		const Record *r = &f->car[i];
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
	F3DSGarage tmp;
	return unpack(p, &tmp);
}

bool garage_load(F3DSGarage *g, const char *path)
{
	File f;
	return save_read(path, GARAGE_MAGIC, GARAGE_VERSION, &f, sizeof(f), valid_payload) && unpack(&f, g);
}

bool garage_save(const F3DSGarage *g, const char *path)
{
	File f;
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
