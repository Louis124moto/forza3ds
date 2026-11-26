#include "test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "camera.h"
#include "garage.h"
#include "save_io.h"
#include "settings.h"

static void corrupt(const char *path, long offset)
{
	FILE *f = fopen(path, "r+b");
	CHECK(f != NULL);
	if (!f) return;
	fseek(f, offset, SEEK_SET);
	const int c = fgetc(f);
	fseek(f, offset, SEEK_SET);
	fputc(c ^ 0x5A, f);
	fclose(f);
}

static bool exists(const char *path) { return access(path, F_OK) == 0; }

static void settings_files(const char *dir)
{
	char path[160], bak[170], tmp[170];
	snprintf(path, sizeof(path), "%s/new/deep/settings.bin", dir);
	snprintf(bak, sizeof(bak), "%s.bak", path);
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);

	F3DSSettings a, b;
	settings_defaults(&a);
	CHECK(settings_valid(&a));
	CHECK(a.fpsLimit == 30);
	settings_defaults(&b);
	b.chaseDistance = 11.0f;
	CHECK(!settings_load(&b, path)); /* missing file */
	CHECK(b.chaseDistance == 11.0f);

	a.chaseDistance = 9.5f;
	input_rebind(&a.input, F3DS_ACT_THROTTLE, KEY_ZR);
	CHECK(settings_save(&a, path)); /* creates the folders */
	CHECK(!exists(tmp));
	memset(&b, 0, sizeof(b));
	CHECK(settings_load(&b, path) && memcmp(&a, &b, sizeof(a)) == 0);

	/* A second save keeps the first as backup; damage falls back to it. */
	F3DSSettings c = a;
	c.chaseDistance = 12.0f;
	CHECK(settings_save(&c, path) && exists(bak));
	corrupt(path, 24);
	CHECK(settings_load(&b, path) && b.chaseDistance == 9.5f);
	/* Saving over a damaged file must not replace the good backup with it. */
	c.chaseDistance = 13.0f;
	CHECK(settings_save(&c, path));
	corrupt(path, 24);
	CHECK(settings_load(&b, path) && b.chaseDistance == 9.5f);
	/* Both damaged: refuse, leaving the caller's data alone. */
	corrupt(bak, 5);
	b.chaseDistance = 4.0f;
	CHECK(!settings_load(&b, path) && b.chaseDistance == 4.0f);

	/* Invalid values are never written. */
	CHECK(settings_save(&a, path));
	c = a;
	c.fpsLimit = 45;
	CHECK(!settings_save(&c, path));
	c = a;
	c.chaseHeight = NAN;
	CHECK(!settings_save(&c, path));
	c = a;
	c.input.keys[0] = c.input.keys[1];
	CHECK(!settings_save(&c, path));
	CHECK(settings_load(&b, path) && b.chaseDistance == a.chaseDistance);
	/* Truncated and oversized files. */
	remove(bak);
	CHECK(truncate(path, 12) == 0);
	CHECK(!settings_load(&b, path));
	CHECK(settings_save(&a, path));
	FILE *f = fopen(path, "ab");
	fputc(0, f);
	fclose(f);
	remove(bak);
	CHECK(!settings_load(&b, path));

	/* Version 2 files from the previous release are migrated. */
	snprintf(path, sizeof(path), "%s/v2.bin", dir);
	struct {
		unsigned magic, version;
		float sens, dead, dist, height, recenter;
		int fps, hud;
		unsigned throttle, brake, hand, camera;
		float orbitYaw, orbitPitch;
		int mode;
	} old = { 0x46334453u, 2u, 1.4f, 0.2f, 10.0f, 4.0f, 3.0f, 60, 0,
		KEY_R, KEY_L, KEY_X, KEY_Y, 0.0f, 0.3f, 1 };
	f = fopen(path, "wb");
	fwrite(&old, sizeof(old), 1, f);
	fclose(f);
	CHECK(settings_load(&b, path));
	CHECK(b.input.steerSensitivity == 1.4f && b.input.deadzone == 0.2f && b.chaseDistance == 10.0f);
	CHECK(b.fpsLimit == 60 && !b.showHud && b.cameraMode == F3DS_CAM_COCKPIT);
	CHECK(b.input.keys[F3DS_ACT_THROTTLE] == KEY_R && b.input.keys[F3DS_ACT_BRAKE] == KEY_L);
	CHECK(input_config_valid(&b.input));

	/* Every settings entry can be shown and adjusted without leaving bounds. */
	settings_defaults(&a);
	for (int i = 0; i < settings_item_count(); ++i) {
		char text[64] = "";
		settings_item_text(&a, i, text, sizeof(text));
		CHECKF(text[0] && text[0] != '?', "item %d", i);
		for (int k = 0; k < 60; ++k) settings_adjust(&a, i, k < 30 ? 1 : -1);
		CHECKF(settings_valid(&a), "item %d", i);
	}
}

static void garage_files(const char *dir)
{
	char path[160];
	snprintf(path, sizeof(path), "%s/saves/free_garage.bin", dir);
	F3DSGarage a, b;
	garage_defaults(&a);
	garage_defaults(&b);
	a.selected = 2;
	a.def[1].turboBoostBar = 25.0f; /* no ceiling survives a round trip */
	a.def[1].maxRpm = 18000.0f;
	a.def[2].rideHeight[1] = 0.31f;
	a.def[3].gearCount = 10;
	a.def[3].gearRatios[9] = 0.30f;
	a.def[0].intakeLevel = 77;
	a.paint[0] = 6;
	a.rim[0] = 2;
	CHECK(garage_save(&a, path));
	CHECK(garage_load(&b, path));
	CHECK(memcmp(&a, &b, sizeof(a)) == 0);
	a.paint[0] = 5;
	CHECK(garage_save(&a, path));
	corrupt(path, 40);
	CHECK(garage_load(&b, path) && b.paint[0] == 6);
	a.def[0].springRate[0] = -1.0f;
	CHECK(!garage_save(&a, path));
	/* A settings file is not a garage. */
	snprintf(path, sizeof(path), "%s/other.bin", dir);
	F3DSSettings s;
	settings_defaults(&s);
	CHECK(settings_save(&s, path));
	b.paint[0] = 3;
	CHECK(!garage_load(&b, path) && b.paint[0] == 3);
	CHECK(save_crc32("123456789", 9) == 0xCBF43926u);
}

void test_saves(const char *dir)
{
	settings_files(dir);
	garage_files(dir);
}
