/* Forza3DS - software 2D drawing into an RGB565 buffer with the lower LCD
 * framebuffer layout (column major, 240 pixels per column). */
#ifndef F3DS_CANVAS_H
#define F3DS_CANVAS_H

#include "common.h"

#define CANVAS_W 320
#define CANVAS_H 240
#define CANVAS_PIXELS (CANVAS_W * CANVAS_H)

typedef struct {
	uint16_t *px;          /* CANVAS_PIXELS entries */
	const uint8_t *font;   /* 256 glyphs, 8x8, CP437, one byte per row */
} F3DSCanvas;

#define RGB565C(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

static inline int canvas_index(int x, int y) { return x * CANVAS_H + (CANVAS_H - 1 - y); }

void canvas_fill(F3DSCanvas *c, int x, int y, int w, int h, uint16_t color);
void canvas_line(F3DSCanvas *c, float x0, float y0, float x1, float y1, int thickness, uint16_t color);
void canvas_disc(F3DSCanvas *c, float cx, float cy, float radius, uint16_t color);
/* UTF-8 text (Latin letters with French accents); returns the width drawn. */
int canvas_text(F3DSCanvas *c, int x, int y, int scale, uint16_t color, const char *text);
int canvas_text_width(const char *text, int scale);

#endif
