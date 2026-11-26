/* Forza3DS - workshop pages: the tunable items of one car. Values have no
 * artificial upper ceiling; only physically impossible values are refused. */
#ifndef F3DS_WORKSHOP_H
#define F3DS_WORKSHOP_H

#include "garage.h"

typedef enum {
	F3DS_PAGE_ENGINE = 0,
	F3DS_PAGE_GEARBOX,
	F3DS_PAGE_DIFF,
	F3DS_PAGE_SUSPENSION,
	F3DS_PAGE_TIRES,
	F3DS_PAGE_LOOK,
	F3DS_PAGE_COUNT
} F3DSWorkshopPage;

const char *workshop_page_name(F3DSWorkshopPage page);
int workshop_item_count(F3DSWorkshopPage page);
void workshop_item_text(const F3DSGarage *g, F3DSWorkshopPage page, int item, char *out, int size);
bool workshop_adjust(F3DSGarage *g, F3DSWorkshopPage page, int item, int dir);

#endif
