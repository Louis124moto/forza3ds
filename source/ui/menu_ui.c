#include "menu_ui.h"
#include <stddef.h>

#define UI_W 320
#define UI_H 240

static u16 *s_framebuffer;
static u16 s_fb_width, s_fb_height;

static void acquire_framebuffer(void)
{
	s_framebuffer = (u16 *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &s_fb_width, &s_fb_height);
}

/* consoleInit(GFX_BOTTOM) switches the lower LCD to RGB565 and disables
 * double buffering. Its framebuffer is column-major: x * 240 + (239 - y). */
static void pixel(int x, int y, u8 r, u8 g, u8 b)
{
	if (!s_framebuffer || x < 0 || x >= UI_W || y < 0 || y >= UI_H ||
	    s_fb_width != 240 || s_fb_height != 320)
		return;
	const size_t offset = (size_t)x * s_fb_width + (size_t)(s_fb_width - 1u - (u16)y);
	s_framebuffer[offset] = RGB8_to_565(r, g, b);
}

static void rect(int x, int y, int w, int h, u8 r, u8 g, u8 b)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > UI_W) w = UI_W - x;
	if (y + h > UI_H) h = UI_H - y;
	if (w <= 0 || h <= 0) return;
	for (int py = y; py < y + h; ++py)
		for (int px = x; px < x + w; ++px)
			pixel(px, py, r, g, b);
}

static void diagonal_stripe(int x, int y, int length, int thickness, u8 r, u8 g, u8 b)
{
	for (int i = 0; i < length; ++i)
		rect(x + i, y + i / 2, thickness, 2, r, g, b);
}

void menu_ui_begin(void)
{
	acquire_framebuffer();
	if (!s_framebuffer || s_fb_width != 240 || s_fb_height != 320)
		return;

	/* Motorsport-inspired charcoal dashboard with cyan telemetry accents. */
	rect(0, 0, UI_W, UI_H, 11, 17, 25);
	rect(0, 0, UI_W, 34, 17, 27, 38);
	rect(0, 0, UI_W, 3, 0, 205, 225);
	rect(0, 33, UI_W, 1, 54, 75, 88);
	rect(0, 218, UI_W, 22, 7, 11, 17);
	rect(0, 218, UI_W, 2, 0, 175, 198);

	/* Subtle technical grid and right-side racing slashes. */
	for (int x = 8; x < UI_W; x += 32)
		rect(x, 39, 1, 170, 17, 27, 36);
	for (int y = 47; y < 210; y += 32)
		rect(8, y, 304, 1, 17, 27, 36);
	diagonal_stripe(276, 5, 28, 2, 0, 178, 205);
	diagonal_stripe(287, 5, 28, 2, 210, 77, 48);

	/* Side rails and header/footer separators. */
	rect(8, 42, 2, 166, 0, 145, 174);
	rect(310, 42, 2, 166, 0, 77, 95);
	rect(12, 35, 44, 2, 210, 77, 48);
	rect(260, 214, 48, 2, 0, 190, 215);
}

void menu_ui_item(int terminal_row, int selected)
{
	if (!s_framebuffer) acquire_framebuffer();
	const int y = (terminal_row - 1) * 8 + 1;
	if (selected) {
		rect(18, y - 1, 284, 13, 0, 77, 98);
		rect(18, y - 1, 3, 13, 0, 220, 235);
		rect(21, y - 1, 281, 1, 76, 174, 190);
		rect(21, y + 11, 281, 1, 0, 125, 150);
		diagonal_stripe(280, y + 1, 8, 1, 0, 145, 164);
	} else {
		rect(18, y - 1, 284, 13, 15, 25, 35);
		rect(18, y + 11, 284, 1, 31, 45, 56);
	}
}

void menu_ui_panel(int y, int height)
{
	if (!s_framebuffer) acquire_framebuffer();
	rect(18, y, 284, height, 15, 25, 35);
	rect(18, y, 3, height, 0, 165, 190);
	rect(21, y, 281, 1, 54, 75, 88);
	rect(21, y + height - 1, 281, 1, 31, 45, 56);
}
