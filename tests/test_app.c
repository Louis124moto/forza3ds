#include "test.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app.h"
#include "screens.h"
#include "workshop.h"

#define FRAME (1.0f / 30.0f)

static uint16_t s_pixels[CANVAS_PIXELS];
static uint8_t s_font[256 * 8];

static void frame(F3DSApp *a, u32 down, u32 held, float elapsed)
{
	F3DSRawInput raw;
	memset(&raw, 0, sizeof(raw));
	raw.down = raw.repeat = down;
	raw.held = down | held;
	app_update(a, &raw, elapsed);
}

static void press(F3DSApp *a, u32 key)
{
	frame(a, key, 0, FRAME);
	frame(a, 0, 0, FRAME);
}

static void select_row(F3DSApp *a, int row)
{
	for (int i = 0; i < 80 && a->selected != row; ++i)
		press(a, KEY_DDOWN);
	CHECKF(a->selected == row, "row %d screen %d", row, a->screen);
}

static void choose(F3DSApp *a, int row)
{
	select_row(a, row);
	press(a, KEY_A);
}

static void draw_checked(const F3DSApp *a)
{
	F3DSCanvas c = { s_pixels, s_font, 0 };
	screens_draw(&c, a);
	CHECKF(c.clipped == 0, "screen %d page %d row %d map %d clipped %d", a->screen, a->page,
		a->selected, a->mapVisible, c.clipped);
}

static void flow(const char *dir)
{
	static F3DSApp a;
	app_init(&a, dir);
	CHECK(a.screen == F3DS_SCREEN_MAIN && !a.quit && !a.message[0]);
	F3DSSynthParams audio;
	app_audio_params(&a, &audio);
	CHECK(!audio.active);
	draw_checked(&a);

	/* Progression is an information screen. */
	choose(&a, MAIN_PROGRESSION);
	CHECK(a.screen == F3DS_SCREEN_PROGRESSION);
	draw_checked(&a);
	press(&a, KEY_B);
	CHECK(a.screen == F3DS_SCREEN_MAIN && a.selected == MAIN_PROGRESSION);

	choose(&a, MAIN_FREE);
	CHECK(a.screen == F3DS_SCREEN_CARS);
	for (int car = 0; car <= F3DS_CAR_COUNT; ++car) {
		select_row(&a, car);
		draw_checked(&a);
		if (car < F3DS_CAR_COUNT) CHECK(a.garage.selected == car);
	}
	choose(&a, 1);
	CHECK(a.screen == F3DS_SCREEN_WORKSHOP && a.garage.selected == 1);
	app_audio_params(&a, &audio);
	CHECK(audio.active && audio.speed == 0.0f);
	/* The garage engine revs while R is held. */
	const float idle = a.car.rpm;
	for (int i = 0; i < 45; ++i) frame(&a, 0, KEY_R, FRAME);
	CHECK(a.car.rpm > idle + 2000.0f);
	for (int i = 0; i < 90; ++i) frame(&a, 0, 0, FRAME);
	CHECK(a.car.rpm < idle + 200.0f);

	/* Every workshop page and row draws; B returns to the same entry. */
	for (int p = 0; p < F3DS_PAGE_COUNT; ++p) {
		choose(&a, p);
		CHECK(a.screen == F3DS_SCREEN_WORKSHOP_PAGE && (int)a.page == p);
		for (int r = 0; r <= workshop_item_count(a.page); ++r) {
			select_row(&a, r);
			draw_checked(&a);
		}
		press(&a, KEY_A); /* "back" row */
		CHECK(a.screen == F3DS_SCREEN_WORKSHOP && a.selected == p);
	}
	CHECK(!a.garageDirty);
	choose(&a, F3DS_PAGE_ENGINE);
	const float boost = a.garage.def[1].turboBoostBar;
	select_row(&a, 3);
	for (int i = 0; i < 5; ++i) press(&a, KEY_DRIGHT);
	CHECK(a.garage.def[1].turboBoostBar > boost && a.garageDirty);
	press(&a, KEY_B);

	/* Save, then reload in a second session. */
	choose(&a, HOME_SAVE);
	CHECK(!a.garageDirty && a.message[0]);
	draw_checked(&a);
	static F3DSApp b;
	app_init(&b, dir);
	CHECK(b.garage.selected == 1 && b.garage.def[1].turboBoostBar == a.garage.def[1].turboBoostBar);
	for (int i = 0; i < 150; ++i) frame(&a, 0, 0, FRAME);
	CHECK(!a.message[0]);

	/* Drive. */
	choose(&a, HOME_DRIVE);
	CHECK(a.screen == F3DS_SCREEN_DRIVE);
	const FVec3 start = a.car.pos;
	for (int i = 0; i < 90; ++i) frame(&a, 0, KEY_A, FRAME);
	CHECK(a.car.speed > 5.0f);
	CHECK(fvec3_len(fvec3_sub(a.car.pos, start)) > 5.0f);
	draw_checked(&a);
	app_audio_params(&a, &audio);
	CHECK(audio.active && audio.speed > 5.0f);

	/* A gear request on a frame without a simulation step is not lost. */
	a.garage.def[1].manualTransmission = 1;
	a.car.gear = 2;
	a.car.shiftTimer = 0.0f;
	a.loop.accumulator = 0.0f;
	frame(&a, KEY_R, 0, 0.0f);
	CHECK(a.car.gear == 2);
	for (int i = 0; i < 30; ++i) frame(&a, 0, 0, FRAME);
	CHECKF(a.car.gear == 3, "gear %d", a.car.gear);
	a.garage.def[1].manualTransmission = 0;

	/* Camera toggle works while driving. */
	CHECK(a.camera.mode == F3DS_CAM_CHASE);
	press(&a, KEY_X);
	CHECK(a.camera.mode == F3DS_CAM_COCKPIT);
	app_audio_params(&a, &audio);
	CHECK(audio.cockpit);
	press(&a, KEY_X);

	/* A long stall does not teleport the car; bad time steps are ignored. */
	const FVec3 before = a.car.pos;
	frame(&a, 0, 0, 5.0f);
	CHECK(fvec3_len(fvec3_sub(a.car.pos, before)) < 0.15f * fabsf(a.car.speed) + 0.5f);
	frame(&a, 0, 0, NAN);
	frame(&a, 0, 0, -1.0f);
	CHECK(isfinite(a.car.pos.x) && isfinite(a.car.speed));

	/* Map over the drive screen. */
	press(&a, KEY_SELECT);
	CHECK(a.mapVisible && a.screen == F3DS_SCREEN_DRIVE);
	draw_checked(&a);
	press(&a, KEY_SELECT);
	CHECK(!a.mapVisible);

	/* Lid closed: pause, and stay paused until asked. */
	app_suspend(&a);
	CHECK(a.screen == F3DS_SCREEN_PAUSE && app_audio_paused(&a));
	const FVec3 paused = a.car.pos;
	const float pausedSpeed = a.car.speed;
	for (int i = 0; i < 60; ++i) frame(&a, 0, 0, FRAME);
	CHECK(a.screen == F3DS_SCREEN_PAUSE && a.car.pos.x == paused.x && a.car.speed == pausedSpeed);
	app_audio_params(&a, &audio);
	CHECK(!audio.active);
	draw_checked(&a);
	press(&a, KEY_B);
	CHECK(a.screen == F3DS_SCREEN_DRIVE);
	press(&a, KEY_START);
	CHECK(a.screen == F3DS_SCREEN_PAUSE);

	/* Settings from pause, saved on exit; every row draws. */
	choose(&a, PAUSE_SETTINGS);
	CHECK(a.screen == F3DS_SCREEN_SETTINGS && app_audio_paused(&a));
	F3DSView view;
	app_view(&a, &view);
	CHECK(!view.garageScene);
	for (int r = 0; r <= settings_item_count(); ++r) {
		select_row(&a, r);
		draw_checked(&a);
	}
	int adjustable = -1, binding = -1;
	for (int r = settings_item_count() - 1; r >= 0; --r) {
		F3DSAction act;
		if (settings_item_binding(r, &act)) { if (act == F3DS_ACT_THROTTLE) binding = r; }
		else adjustable = r;
	}
	CHECK(adjustable >= 0 && binding >= 0);
	const F3DSSettings old = a.settings;
	select_row(&a, adjustable);
	press(&a, KEY_DRIGHT);
	if (memcmp(&old, &a.settings, sizeof(old)) == 0) press(&a, KEY_DLEFT);
	CHECK(memcmp(&old, &a.settings, sizeof(old)) != 0 && a.settingsDirty);

	/* Rebinding: START cancels, a key assigns, SELECT does not open the map. */
	choose(&a, binding);
	CHECK(a.rebindAction == F3DS_ACT_THROTTLE);
	draw_checked(&a);
	press(&a, KEY_START);
	CHECK(a.rebindAction < 0 && a.screen == F3DS_SCREEN_SETTINGS);
	press(&a, KEY_A);
	press(&a, KEY_SELECT);
	CHECK(a.rebindAction == F3DS_ACT_THROTTLE && !a.mapVisible);
	press(&a, KEY_ZR);
	CHECK(a.rebindAction < 0 && a.settings.input.keys[F3DS_ACT_THROTTLE] == KEY_ZR);
	press(&a, KEY_B);
	CHECK(a.screen == F3DS_SCREEN_PAUSE && a.selected == PAUSE_SETTINGS && !a.settingsDirty);
	app_init(&b, dir);
	CHECK(b.settings.input.keys[F3DS_ACT_THROTTLE] == KEY_ZR);

	choose(&a, PAUSE_MAP);
	CHECK(a.mapVisible);
	const int sel = a.selected;
	press(&a, KEY_DDOWN);
	CHECK(a.selected == sel);
	press(&a, KEY_B);
	CHECK(!a.mapVisible && a.screen == F3DS_SCREEN_PAUSE);

	/* The new binding drives the car. */
	choose(&a, PAUSE_RESET);
	CHECK(a.screen == F3DS_SCREEN_DRIVE && fabsf(a.car.speed) < 0.5f);
	CHECK(fvec3_len(fvec3_sub(a.car.pos, a.track.pts[0])) < 1.0f);
	for (int i = 0; i < 60; ++i) frame(&a, 0, KEY_ZR, FRAME);
	CHECK(a.car.speed > 3.0f);

	/* Pause -> workshop -> immediate change -> drive. */
	press(&a, KEY_START);
	choose(&a, PAUSE_WORKSHOP);
	CHECK(a.screen == F3DS_SCREEN_WORKSHOP && a.selected == HOME_DRIVE && a.car.speed == 0.0f);
	app_view(&a, &view);
	CHECK(view.garageScene && !view.cockpit);
	press(&a, KEY_A);
	CHECK(a.screen == F3DS_SCREEN_DRIVE);
	press(&a, KEY_START);
	choose(&a, PAUSE_CARS);
	CHECK(a.screen == F3DS_SCREEN_CARS && a.selected == 1);
	press(&a, KEY_B);
	CHECK(a.screen == F3DS_SCREEN_MAIN);

	/* Restore factory settings. */
	choose(&a, MAIN_FREE);
	choose(&a, 1);
	choose(&a, HOME_RESTORE);
	CHECK(a.garage.def[1].turboBoostBar == boost && a.garageDirty);

	/* Leaving the workshop does not save; quitting warns. */
	press(&a, KEY_B);
	press(&a, KEY_B);
	choose(&a, MAIN_QUIT);
	CHECK(a.screen == F3DS_SCREEN_CONFIRM_QUIT && !a.quit);
	draw_checked(&a);
	press(&a, KEY_B);
	CHECK(a.screen == F3DS_SCREEN_MAIN);
	choose(&a, MAIN_QUIT);
	press(&a, KEY_A);
	CHECK(a.quit);
	app_shutdown(&a);
	app_init(&b, dir);
	CHECK(b.garage.def[1].turboBoostBar != boost); /* the saved build survived */
	choose(&b, MAIN_QUIT);
	CHECK(b.quit);

	/* A damaged garage is reported, not silently accepted. */
	char path[200];
	snprintf(path, sizeof(path), "%s/saves/free_garage.bin", dir);
	FILE *f = fopen(path, "wb");
	fputs("garbage", f);
	fclose(f);
	strcat(path, ".bak");
	remove(path);
	app_init(&b, dir);
	CHECK(b.message[0] && b.garage.def[1].turboBoostBar == boost);
	draw_checked(&b);
}

/* Random input for a long time: nothing may leave its valid range. */
static void fuzz(const char *dir)
{
	static F3DSApp a;
	app_init(&a, dir);
	static const u32 keys[] = { KEY_A, KEY_A, KEY_A, KEY_B, KEY_START, KEY_SELECT, KEY_DUP,
		KEY_DDOWN, KEY_DDOWN, KEY_DLEFT, KEY_DRIGHT, KEY_DRIGHT, KEY_X, KEY_Y, KEY_L, KEY_R,
		KEY_ZL, KEY_ZR };
	srand(1234);
	int visited = 0, bad = 0;
	u32 held = 0;
	for (int i = 0; i < 200000; ++i) {
		F3DSRawInput raw;
		memset(&raw, 0, sizeof(raw));
		if (rand() % 4 == 0) {
			raw.down = raw.repeat = keys[rand() % (int)(sizeof(keys) / sizeof(keys[0]))];
		}
		if (rand() % 20 == 0) held = keys[rand() % (int)(sizeof(keys) / sizeof(keys[0]))];
		raw.held = held | raw.down;
		raw.circleX = rand() % 321 - 160;
		raw.cstickX = rand() % 321 - 160;
		app_update(&a, &raw, (float)(rand() % 100) / 1000.0f);
		if (a.quit) a.quit = false;
		visited |= 1 << a.screen;
		if (!isfinite(a.car.pos.x) || !isfinite(a.car.pos.y) || !isfinite(a.car.pos.z) ||
		    !isfinite(a.car.speed) || !isfinite(a.car.rpm) || !isfinite(a.camera.eye.x) ||
		    !settings_valid(&a.settings) || !vehicle_def_valid(&a.garage.def[a.garage.selected]) ||
		    a.screen > F3DS_SCREEN_CONFIRM_QUIT)
			bad++;
		if (i % 97 == 0) draw_checked(&a);
	}
	CHECKF(bad == 0, "%d invalid frames", bad);
	CHECKF(visited == (1 << (F3DS_SCREEN_CONFIRM_QUIT + 1)) - 1, "visited %x", visited);
}

void test_app(const char *dir)
{
	char sub[160], other[160];
	snprintf(sub, sizeof(sub), "%s/app", dir);
	snprintf(other, sizeof(other), "%s/fuzz", dir);
	memset(s_font, 0xAA, sizeof(s_font));
	flow(sub);
	fuzz(other);
}
