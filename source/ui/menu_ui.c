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
 * double buffering. Its framebuffer is stored column-major, exactly like
 * libctru's console renderer: x * 240 + (239 - y). */
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

void menu_ui_begin(void)
{
	acquire_framebuffer();
	if (!s_framebuffer || s_fb_width != 240 || s_fb_height != 320)
		return;
	rect(0, 0, UI_W, UI_H, 10, 17, 29);
	rect(0, 0, UI_W, 3, 0, 190, 220);
	rect(0, 31, UI_W, 1, 33, 65, 83);
	rect(0, 218, UI_W, 22, 7, 12, 21);
	rect(0, 218, UI_W, 2, 0, 155, 190);
	rect(12, 43, 3, 164, 0, 135, 170);
	rect(305, 43, 3, 164, 0, 75, 100);
}

void menu_ui_item(int terminal_row, int selected)
{
	if (!s_framebuffer) acquire_framebuffer();
	const int y = (terminal_row - 1) * 8 + 1;
	if (selected) {
		rect(22, y, 276, 11, 0, 93, 119);
		rect(22, y, 4, 11, 0, 220, 245);
		rect(26, y, 272, 1, 60, 170, 190);
	} else {
		rect(22, y, 276, 11, 16, 29, 43);
		rect(22, y + 10, 276, 1, 28, 48, 62);
	}
}

void menu_ui_panel(int y, int height)
{
	if (!s_framebuffer) acquire_framebuffer();
	rect(22, y, 276, height, 16, 29, 43);
	rect(22, y, 3, height, 0, 135, 170);
}
