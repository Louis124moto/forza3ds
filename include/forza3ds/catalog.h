/* Forza3DS - built-in car catalogue: identity, visual data and physical data
 * are kept in separate structures. */
#ifndef F3DS_CATALOG_H
#define F3DS_CATALOG_H

#include "vehicle.h"

#define F3DS_CAR_COUNT 4
#define F3DS_PAINT_COUNT 8
#define F3DS_RIM_COUNT 4

/* Visual description only. Car local frame: +Z forward, +X left, +Y up. */
typedef struct {
	float halfWidth, halfLength;
	float floorY, beltY, roofY;
	float cabinFront, cabinRear, roofFront, roofRear; /* z positions */
	float wheelHalfWidth;
	float seatX, eyeY, eyeZ;                          /* fixed cockpit eye */
	int spoiler;
	int defaultPaint;
} F3DSCarVisual;

typedef struct {
	const char *name;
	const char *summary;
} F3DSCarInfo;

const F3DSCarInfo *catalog_info(int car);
const F3DSCarVisual *catalog_visual(int car);
void catalog_def(int car, F3DSVehicleDef *out);

const char *catalog_paint_name(int paint);
void catalog_paint_color(int paint, float rgb[3]);
const char *catalog_rim_name(int rim);
void catalog_rim_color(int rim, float rgb[3]);

#endif
