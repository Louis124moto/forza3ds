/* Forza3DS native New 3DS driving demo. */
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include "camera.h"
#include "core.h"
#include "hud.h"
#include "input.h"
#include "physics.h"
#include "platform.h"
#include "renderer.h"
#include "terrain.h"
#include "track.h"
#include "vehicle.h"

#define SETTINGS_PATH "sdmc:/3ds/forza3ds/settings.bin"
#define SETTINGS_MAGIC 0x46334453u
#define SETTINGS_VERSION 1u
#define MENU_ITEMS 10

typedef struct {
	u32 magic;
	u32 version;
	float steerSensitivity;
	float deadzone;
	float chaseDistance;
	float chaseHeight;
	float recenterSpeed;
	int fpsLimit;
	int hudVisible;
} F3DSSettings;

static void settings_defaults(F3DSSettings *s)
{
	memset(s, 0, sizeof(*s));
	s->magic = SETTINGS_MAGIC;
	s->version = SETTINGS_VERSION;
	s->steerSensitivity = 1.0f;
	s->deadzone = 0.12f;
	s->chaseDistance = 7.5f;
	s->chaseHeight = 3.2f;
	s->recenterSpeed = 2.5f;
	s->fpsLimit = 60;
	s->hudVisible = 1;
}

static void settings_load(F3DSSettings *s)
{
	FILE *f = fopen(SETTINGS_PATH, "rb");
	if (!f) return;
	F3DSSettings loaded;
	const size_t n = fread(&loaded, 1, sizeof(loaded), f);
	fclose(f);
	if (n != sizeof(loaded) || loaded.magic != SETTINGS_MAGIC ||
	    loaded.version != SETTINGS_VERSION ||
	    !isfinite(loaded.steerSensitivity) || !isfinite(loaded.deadzone) ||
	    !isfinite(loaded.chaseDistance) || !isfinite(loaded.chaseHeight) ||
	    !isfinite(loaded.recenterSpeed) ||
	    loaded.steerSensitivity < 0.1f || loaded.steerSensitivity > 3.0f ||
	    loaded.deadzone < 0.0f || loaded.deadzone > 0.5f ||
	    loaded.chaseDistance < 3.0f || loaded.chaseDistance > 20.0f ||
	    loaded.chaseHeight < 1.0f || loaded.chaseHeight > 12.0f ||
	    loaded.recenterSpeed < 0.2f || loaded.recenterSpeed > 8.0f ||
	    (loaded.fpsLimit != 30 && loaded.fpsLimit != 60) ||
	    (loaded.hudVisible != 0 && loaded.hudVisible != 1))
		return;
	*s = loaded;
}

static int settings_save(const F3DSSettings *s)
{
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/forza3ds", 0777);
	FILE *f = fopen(SETTINGS_PATH, "wb");
	if (!f) return 0;
	const int ok = fwrite(s, sizeof(*s), 1, f) == 1;
	if (fclose(f) != 0) return 0;
	return ok;
}

static void draw_pause_menu(const F3DSSettings *s, int selected, int saveStatus)
{
	consoleClear();
	printf("\x1b[1;3HFORZA3DS  |  PAUSE\n");
	printf("\x1b[3;3H%s  Reprendre\n", selected == 0 ? ">" : " ");
	printf("\x1b[4;3H%s  Vue : %s\n", selected == 1 ? ">" : " ", "X pour changer");
	printf("\x1b[5;3H%s  Distance camera : %.1f m\n", selected == 2 ? ">" : " ", s->chaseDistance);
	printf("\x1b[6;3H%s  Hauteur camera : %.1f m\n", selected == 3 ? ">" : " ", s->chaseHeight);
	printf("\x1b[7;3H%s  Vitesse recentrage : %.1f\n", selected == 4 ? ">" : " ", s->recenterSpeed);
	printf("\x1b[8;3H%s  Sensibilite direction : %.1f\n", selected == 5 ? ">" : " ", s->steerSensitivity);
	printf("\x1b[9;3H%s  Zone morte : %.2f\n", selected == 6 ? ">" : " ", s->deadzone);
	printf("\x1b[10;3H%s  Limiteur : %d FPS\n", selected == 7 ? ">" : " ", s->fpsLimit);
	printf("\x1b[11;3H%s  HUD : %s\n", selected == 8 ? ">" : " ", s->hudVisible ? "visible" : "masque");
	printf("\x1b[12;3H%s  Sauvegarder les reglages\n", selected == 9 ? ">" : " ");
	printf("\x1b[14;3HA confirmer  B retour  START reprendre\n");
	printf("\x1b[15;3HCroix directionnelle : naviguer/modifier\n");
	if (saveStatus) printf("\x1b[17;3HReglages sauvegardes.");
}

static void menu_adjust(F3DSSettings *s, int selected, int direction, F3DSCamera *cam)
{
	switch (selected) {
	case 2: s->chaseDistance = clampf(s->chaseDistance + direction * 0.5f, 3.0f, 20.0f); cam->chaseDistance = s->chaseDistance; break;
	case 3: s->chaseHeight = clampf(s->chaseHeight + direction * 0.25f, 1.0f, 12.0f); cam->chaseHeight = s->chaseHeight; break;
	case 4: s->recenterSpeed = clampf(s->recenterSpeed + direction * 0.2f, 0.2f, 8.0f); cam->recenterSpeed = s->recenterSpeed; break;
	case 5: s->steerSensitivity = clampf(s->steerSensitivity + direction * 0.1f, 0.1f, 3.0f); break;
	case 6: s->deadzone = clampf(s->deadzone + direction * 0.02f, 0.0f, 0.5f); break;
	case 7: s->fpsLimit = s->fpsLimit == 30 ? 60 : 30; break;
	case 8: s->hudVisible = !s->hudVisible; break;
	default: break;
	}
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);

	F3DSPlatformInfo platform;
	if (!platform_init(&platform)) {
		printf("Forza3DS\nNew Nintendo 3DS required.\nPress START to exit.");
		while (aptMainLoop()) {
			hidScanInput();
			if (hidKeysDown() & KEY_START) break;
			gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
		}
		gfxExit();
		return 0;
	}

	renderer_init();
	F3DSSettings settings;
	settings_defaults(&settings);
	settings_load(&settings);

	F3DSLoopState loop;
	core_loop_init(&loop, 1.0f / 60.0f, 5.0f / 60.0f);
	F3DSInputConfig inputCfg;
	input_config_defaults(&inputCfg);
	inputCfg.steerSensitivity = settings.steerSensitivity;
	inputCfg.deadzone = settings.deadzone;
	F3DSInputState input;
	memset(&input, 0, sizeof(input));

	F3DSVehicleDef def;
	vehicle_def_default(&def);
	F3DSTrack track;
	track_init(&track);
	F3DSTerrain terrain;
	terrain_init(&terrain, &track);
	renderer_set_track(&track, &terrain);

	F3DSVehicleState car;
	memset(&car, 0, sizeof(car));
	car.visualModelId = 0;
	car.gear = 1;
	car.rpm = def.idleRpm;
	car.pos = fvec3(track.pts[0].x, 0.0f, track.pts[0].z);
	car.yaw = atan2f(track.fwd[0].x, track.fwd[0].z);

	F3DSCamera cam;
	camera_init(&cam);
	cam.chaseDistance = settings.chaseDistance;
	cam.chaseHeight = settings.chaseHeight;
	cam.recenterSpeed = settings.recenterSpeed;

	int paused = 0, selected = 0, saveStatus = 0;
	unsigned int frameCounter = 0;
	while (aptMainLoop()) {
		const int steps = core_loop_begin_frame(&loop);
		input_poll(&input, &inputCfg);
		if (input.exit) break;

		if (input.pausePressed) paused = !paused;
		if (paused) {
			if (input.menuUp) selected = (selected + MENU_ITEMS - 1) % MENU_ITEMS;
			if (input.menuDown) selected = (selected + 1) % MENU_ITEMS;
			if (input.menuLeft) menu_adjust(&settings, selected, -1, &cam);
			if (input.menuRight) menu_adjust(&settings, selected, 1, &cam);
			if (input.menuConfirm) {
				if (selected == 0) paused = 0;
				else if (selected == 1) cam.mode = cam.mode == F3DS_CAM_CHASE ? F3DS_CAM_COCKPIT : F3DS_CAM_CHASE;
				else if (selected == 9) saveStatus = settings_save(&settings);
			}
			if (input.menuBack) paused = 0;
			inputCfg.steerSensitivity = settings.steerSensitivity;
			inputCfg.deadzone = settings.deadzone;
			draw_pause_menu(&settings, selected, saveStatus);
		} else {
			for (int i = 0; i < steps; i++) {
				physics_step(&def, &car, &input, &track, &terrain, loop.dt);
				input.toggleView = 0;
				input.camDistanceUp = input.camDistanceDown = 0;
				input.camHeightUp = input.camHeightDown = 0;
			}
			camera_update(&cam, &car, &input, loop.frameTime);
			if (settings.fpsLimit == 30 && (frameCounter++ & 1u)) {
				/* Keep simulation at 60 Hz while presenting at 30 FPS. */
			} else {
				renderer_render(&cam, &car);
				if (settings.hudVisible) hud_render(&car, &cam, &loop);
				else consoleClear();
			}
		}
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	settings.steerSensitivity = inputCfg.steerSensitivity;
	settings.deadzone = inputCfg.deadzone;
	settings.chaseDistance = cam.chaseDistance;
	settings.chaseHeight = cam.chaseHeight;
	settings.recenterSpeed = cam.recenterSpeed;
	settings_save(&settings);
	renderer_exit();
	gfxExit();
	return 0;
}
