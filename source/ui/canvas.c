#include "canvas.h"

void canvas_fill(F3DSCanvas *c, int x, int y, int w, int h, uint16_t color)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > CANVAS_W) w = CANVAS_W - x;
	if (y + h > CANVAS_H) h = CANVAS_H - y;
	if (w <= 0 || h <= 0)
		return;
	for (int i = x; i < x + w; ++i) {
		/* Rows of one column are contiguous, bottom to top. */
		uint16_t *p = c->px + canvas_index(i, y + h - 1);
		for (int j = 0; j < h; ++j)
			p[j] = color;
	}
}

void canvas_line(F3DSCanvas *c, float x0, float y0, float x1, float y1, int thickness, uint16_t color)
{
	const float dx = x1 - x0, dy = y1 - y0;
	const float len = fmaxf(fabsf(dx), fabsf(dy));
	if (!isfinite(len) || len > 2000.0f)
		return;
	const int n = (int)len + 1;
	for (int i = 0; i <= n; ++i) {
		const float t = (float)i / (float)n;
		canvas_fill(c, (int)(x0 + dx * t) - thickness / 2, (int)(y0 + dy * t) - thickness / 2,
			thickness, thickness, color);
	}
}

void canvas_disc(F3DSCanvas *c, float cx, float cy, float radius, uint16_t color)
{
	for (int dy = -(int)radius; dy <= (int)radius; ++dy) {
		const int half = (int)sqrtf(fmaxf(radius * radius - (float)(dy * dy), 0.0f));
		canvas_fill(c, (int)cx - half, (int)cy + dy, half * 2 + 1, 1, color);
	}
}

/* Decode one UTF-8 character into the CP437 glyph index of the console font. */
static int next_glyph(const char **text)
{
	const unsigned char *s = (const unsigned char *)*text;
	unsigned int cp = *s++;
	if (cp >= 0xC0u) {
		int extra = cp >= 0xE0u ? (cp >= 0xF0u ? 3 : 2) : 1;
		cp &= 0x3Fu >> extra;
		while (extra-- && (*s & 0xC0u) == 0x80u)
			cp = (cp << 6) | (*s++ & 0x3Fu);
	}
	*text = (const char *)s;
	if (cp < 128u)
		return (int)cp;
	switch (cp) {
	case 0xC7: return 128; case 0xFC: return 129; case 0xE9: return 130; case 0xE2: return 131;
	case 0xE4: return 132; case 0xE0: return 133; case 0xE7: return 135; case 0xEA: return 136;
	case 0xEB: return 137; case 0xE8: return 138; case 0xEF: return 139; case 0xEE: return 140;
	case 0xC9: return 144; case 0xF4: return 147; case 0xF6: return 148; case 0xFB: return 150;
	case 0xF9: return 151; case 0xB0: return 248;
	case 0xC0: return 'A'; case 0xC8: return 'E'; case 0xCA: return 'E';
	default: return '?';
	}
}

int canvas_text_width(const char *text, int scale)
{
	int n = 0;
	while (*text) {
		next_glyph(&text);
		n++;
	}
	return n * 8 * scale;
}

int canvas_text(F3DSCanvas *c, int x, int y, int scale, uint16_t color, const char *text)
{
	const int start = x;
	while (*text) {
		const int g = next_glyph(&text);
		if (c->font) {
			const uint8_t *rows = c->font + g * 8;
			for (int row = 0; row < 8; ++row)
				for (int col = 0; col < 8; ++col)
					if (rows[row] & (0x80u >> col))
						canvas_fill(c, x + col * scale, y + row * scale, scale, scale, color);
		}
		x += 8 * scale;
	}
	return x - start;
}
