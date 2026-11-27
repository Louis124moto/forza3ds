/* Forza3DS - built-in car catalogue: identity, visual data and physical data
 * are kept in separate structures. All sixteen cars are fictional. */
#ifndef F3DS_CATALOG_H
#define F3DS_CATALOG_H

#include "vehicle.h"

#define F3DS_CAR_COUNT 16
#define F3DS_PAINT_COUNT 12
#define F3DS_RIM_COUNT 4

typedef enum {
	F3DS_BODY_COUPE = 0,
	F3DS_BODY_HATCH,
	F3DS_BODY_MUSCLE,
	F3DS_BODY_KEI,
	F3DS_BODY_SUPERCAR,
	F3DS_BODY_ROADSTER,
	F3DS_BODY_SUV,
	F3DS_BODY_PICKUP,
	F3DS_BODY_SEDAN,
	F3DS_BODY_WAGON,
	F3DS_BODY_GT,
	F3DS_BODY_BUGGY,
	F3DS_BODY_VAN,
	F3DS_BODY_CLASSIC,
	F3DS_BODY_STYLE_COUNT
} F3DSBodyStyle;

/* Body details. */
#define F3DS_TRIM_LIP         (1u << 0)  /* small boot lip spoiler */
#define F3DS_TRIM_WING        (1u << 1)  /* tall rear wing on stands */
#define F3DS_TRIM_ROOF_RACK   (1u << 2)
#define F3DS_TRIM_SPARE       (1u << 3)  /* spare wheel on the tailgate */
#define F3DS_TRIM_ROLL_BAR    (1u << 4)
#define F3DS_TRIM_FLARES      (1u << 5)  /* wheel arch extensions */
#define F3DS_TRIM_STRIPES     (1u << 6)  /* two stripes along the bonnet */
#define F3DS_TRIM_SCOOP       (1u << 7)  /* bonnet air scoop */
#define F3DS_TRIM_ROUND_LIGHTS (1u << 8)
#define F3DS_TRIM_TWO_TONE    (1u << 9)  /* dark lower body */
#define F3DS_TRIM_LIGHT_POD   (1u << 10) /* auxiliary lamps */

/* Visual description only. Car local frame: +Z forward, +X left, +Y up. */
typedef struct {
	float halfWidth, halfLength;
	float floorY, beltY, roofY;
	float cabinFront, cabinRear, roofFront, roofRear; /* z positions */
	float wheelHalfWidth;
	float seatX, eyeY, eyeZ;                          /* fixed cockpit eye */
	float noseDrop, tailDrop; /* bonnet and boot fall towards the ends, m */
	int style;                /* F3DSBodyStyle */
	unsigned trim;            /* F3DS_TRIM_* */
	int defaultPaint;
} F3DSCarVisual;

typedef struct {
	const char *name;
	const char *summary;
} F3DSCarInfo;

const F3DSCarInfo *catalog_info(int car);
const F3DSCarVisual *catalog_visual(int car);
void catalog_def(int car, F3DSVehicleDef *out);
/* Open cars have no roof panel nor side glass. */
bool catalog_open_top(const F3DSCarVisual *v);

const char *catalog_paint_name(int paint);
void catalog_paint_color(int paint, float rgb[3]);
const char *catalog_rim_name(int rim);
void catalog_rim_color(int rim, float rgb[3]);

#endif
