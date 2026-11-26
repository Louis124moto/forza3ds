/* Forza3DS - Free mode garage: one mechanical build and look per car.
 * Saved separately from any future progression data. */
#ifndef F3DS_GARAGE_H
#define F3DS_GARAGE_H

#include "catalog.h"

typedef struct {
	F3DSVehicleDef def[F3DS_CAR_COUNT];
	int32_t paint[F3DS_CAR_COUNT];
	int32_t rim[F3DS_CAR_COUNT];
	int32_t selected;
} F3DSGarage;

void garage_defaults(F3DSGarage *g);
void garage_restore_car(F3DSGarage *g, int car);
bool garage_load(F3DSGarage *g, const char *path);
bool garage_save(const F3DSGarage *g, const char *path);

#endif
