/* Forza3DS: menu -> free mode -> vehicle -> workshop -> driving loop. */
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include "camera.h"
#include "core.h"
#include "engine_audio.h"
#include "hud.h"
#include "input.h"
#include "menu_ui.h"
#include "physics.h"
#include "platform.h"
#include "renderer.h"
#include "terrain.h"
#include "track.h"
#include "vehicle.h"

#define SETTINGS_PATH "sdmc:/3ds/forza3ds/settings.bin"
#define SETTINGS_MAGIC 0x46334453u
#define SETTINGS_VERSION 2u
#define REBIND_KEY_MASK (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R | KEY_ZL | KEY_ZR)

typedef struct {
	u32 magic, version;
	float steerSensitivity, deadzone;
	float chaseDistance, chaseHeight, recenterSpeed;
	int fpsLimit, hudVisible;
	u32 keyThrottle, keyBrake, keyHandbrake, keyCamera;
	float orbitYaw, orbitPitch;
	int cameraMode;
} F3DSSettings;

typedef enum {
	SCREEN_MAIN = 0, SCREEN_CAR, SCREEN_WORKSHOP, SCREEN_DRIVE,
	SCREEN_PAUSE, SCREEN_PROGRESSION_INFO
} F3DSScreen;

typedef enum {
	WORKSHOP_HOME = 0, WORKSHOP_ENGINE, WORKSHOP_GEARBOX,
	WORKSHOP_DIFF, WORKSHOP_SUSPENSION, WORKSHOP_TIRES
} F3DSWorkshopPage;

static void settings_defaults(F3DSSettings *s)
{
	memset(s, 0, sizeof(*s));
	s->magic = SETTINGS_MAGIC; s->version = SETTINGS_VERSION;
	s->steerSensitivity = 1.0f; s->deadzone = 0.12f;
	s->chaseDistance = 7.5f; s->chaseHeight = 3.2f; s->recenterSpeed = 2.5f;
	s->fpsLimit = 60; s->hudVisible = 1;
	s->keyThrottle = KEY_A; s->keyBrake = KEY_B;
	s->keyHandbrake = KEY_Y; s->keyCamera = KEY_X;
	s->orbitPitch = 0.35f; s->cameraMode = F3DS_CAM_CHASE;
}

static void settings_load(F3DSSettings *s)
{
	FILE *f = fopen(SETTINGS_PATH, "rb");
	if (!f) return;
	F3DSSettings v;
	const size_t n = fread(&v, 1, sizeof(v), f);
	fclose(f);
	if (n != sizeof(v) || v.magic != SETTINGS_MAGIC || v.version != SETTINGS_VERSION ||
		!isfinite(v.steerSensitivity) || !isfinite(v.deadzone) ||
		!isfinite(v.chaseDistance) || !isfinite(v.chaseHeight) ||
		!isfinite(v.recenterSpeed) || !isfinite(v.orbitYaw) || !isfinite(v.orbitPitch) ||
		v.steerSensitivity < 0.1f || v.steerSensitivity > 3.0f ||
		v.deadzone < 0.0f || v.deadzone > 0.5f ||
		v.chaseDistance < 3.0f || v.chaseDistance > 20.0f ||
		v.chaseHeight < 1.0f || v.chaseHeight > 12.0f ||
		v.recenterSpeed < 0.2f || v.recenterSpeed > 8.0f ||
		(v.fpsLimit != 30 && v.fpsLimit != 60) ||
		(v.hudVisible != 0 && v.hudVisible != 1) ||
		(v.cameraMode != F3DS_CAM_CHASE && v.cameraMode != F3DS_CAM_COCKPIT) ||
		!(v.keyThrottle & REBIND_KEY_MASK) || !(v.keyBrake & REBIND_KEY_MASK) ||
		!(v.keyHandbrake & REBIND_KEY_MASK) || !(v.keyCamera & REBIND_KEY_MASK) ||
		(v.keyThrottle & ~REBIND_KEY_MASK) || (v.keyBrake & ~REBIND_KEY_MASK) ||
		(v.keyHandbrake & ~REBIND_KEY_MASK) || (v.keyCamera & ~REBIND_KEY_MASK) ||
		(v.keyThrottle & (v.keyThrottle - 1u)) || (v.keyBrake & (v.keyBrake - 1u)) ||
		(v.keyHandbrake & (v.keyHandbrake - 1u)) || (v.keyCamera & (v.keyCamera - 1u)) ||
		v.keyThrottle == v.keyBrake || v.keyThrottle == v.keyHandbrake ||
		v.keyThrottle == v.keyCamera || v.keyBrake == v.keyHandbrake ||
		v.keyBrake == v.keyCamera || v.keyHandbrake == v.keyCamera)
		return;
	*s = v;
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

static void menu_title(const char *title)
{
	consoleClear();
	menu_ui_begin();
	printf("\x1b[1;3HFORZA3DS");
	printf("\x1b[2;3H%s", title);
	printf("\x1b[27;3HA confirmer   B retour   START pause");
}

static void draw_main_menu(int selected)
{
	menu_title("MENU PRINCIPAL");
	menu_ui_item(7, selected == 0);
	menu_ui_item(9, selected == 1);
	menu_ui_item(11, selected == 2);
	printf("\x1b[7;5H%s MODE LIBRE", selected == 0 ? ">" : " ");
	printf("\x1b[9;5H%s MODE PROGRESSION", selected == 1 ? ">" : " ");
	printf("\x1b[11;5H%s QUITTER", selected == 2 ? ">" : " ");
	printf("\x1b[16;5HChoisis une activite.");
	printf("\x1b[18;5HLe Mode Libre donne acces a l'atelier");
	printf("\x1b[19;5Het aux reglages sans economie.");
}

static void draw_car_select(int selected)
{
	menu_title("CHOIX DU VEHICULE - MODE LIBRE");
	menu_ui_item(7, selected == 0);
	menu_ui_item(9, selected == 1);
	printf("\x1b[7;5H%s Prototype Sport", selected == 0 ? ">" : " ");
	printf("\x1b[9;5H%s Retour", selected == 1 ? ">" : " ");
	printf("\x1b[14;5HVehicule prototype actuellement integre.");
	printf("\x1b[15;5HLes autres voitures seront ajoutees au");
	printf("\x1b[16;5Hcatalogue au fur et a mesure du projet.");
}

static const char *tire_name(F3DSTireType t)
{
	static const char *names[F3DS_TIRE_COUNT] = {
		"Route", "Sport", "Semi-slick", "Slick", "Rallye", "Tout-terrain"
	};
	return (t >= 0 && t < F3DS_TIRE_COUNT) ? names[t] : "?";
}

static void draw_workshop_home(int selected, const F3DSVehicleDef *d)
{
	menu_title("ATELIER - PROTOTYPE SPORT");
	for (int i = 0; i < 7; ++i) menu_ui_item(5 + i, selected == i);
	printf("\x1b[5;5H%s Moteur et suralimentation", selected == 0 ? ">" : " ");
	printf("\x1b[6;5H%s Boite de vitesses", selected == 1 ? ">" : " ");
	printf("\x1b[7;5H%s Differentiels", selected == 2 ? ">" : " ");
	printf("\x1b[8;5H%s Suspension", selected == 3 ? ">" : " ");
	printf("\x1b[9;5H%s Pneus", selected == 4 ? ">" : " ");
	printf("\x1b[10;5H%s ESSAYER EN CONDUITE", selected == 5 ? ">" : " ");
	printf("\x1b[11;5H%s Choix du vehicule", selected == 6 ? ">" : " ");
	printf("\x1b[15;5HCouple : %.0f Nm  |  RPM max : %.0f", d->torqueBase, d->maxRpm);
	printf("\x1b[16;5HTurbo : %s  |  pression : %.1f bar",
		d->turboInstalled ? "monte" : "absent", d->turboBoostBar);
	printf("\x1b[17;5HPneus : %s  |  rapports : %d", tire_name(d->tireType), d->gearCount);
	printf("\x1b[20;5HLes changements sont appliques directement");
	printf("\x1b[21;5Ha la physique et au son du moteur.");
	printf("\x1b[24;5HFleches : choisir   Gauche/Droite : regler");
}

static int workshop_count(F3DSWorkshopPage page)
{
	switch (page) {
	case WORKSHOP_ENGINE: return 10;
	case WORKSHOP_GEARBOX: return 10;
	case WORKSHOP_DIFF: return 5;
	case WORKSHOP_SUSPENSION: return 5;
	case WORKSHOP_TIRES: return 3;
	default: return 7;
	}
}

static void workshop_adjust(F3DSVehicleDef *d, F3DSWorkshopPage page, int item, int dir)
{
	switch (page) {
	case WORKSHOP_ENGINE:
		switch (item) {
		case 0: d->torqueBase = fmaxf(50.0f, d->torqueBase + dir * 5.0f); break;
		case 1: d->turboInstalled = !d->turboInstalled; break;
		case 2: d->turboBoostBar = fmaxf(0.0f, d->turboBoostBar + dir * 0.1f); break;
		case 3: d->maxRpm = fmaxf(d->idleRpm + 500.0f, d->maxRpm + dir * 100.0f); break;
		case 4: d->intakeLevel = (d->intakeLevel + dir < 0) ? 0 : d->intakeLevel + dir; break;
		case 5: d->exhaustLevel = (d->exhaustLevel + dir < 0) ? 0 : d->exhaustLevel + dir; break;
		case 6: d->camshaftLevel = (d->camshaftLevel + dir < 0) ? 0 : d->camshaftLevel + dir; break;
		case 7: d->injectorLevel = (d->injectorLevel + dir < 0) ? 0 : d->injectorLevel + dir; break;
		case 8: d->pistonLevel = (d->pistonLevel + dir < 0) ? 0 : d->pistonLevel + dir; break;
		default: break;
		}
		break;
	case WORKSHOP_GEARBOX:
		if (item == 0) d->manualTransmission = !d->manualTransmission;
		else if (item == 1) d->gearCount = (int)clampf((float)d->gearCount + dir, 1.0f, F3DS_GEARS);
		else if (item == 2) d->finalDrive = fmaxf(0.5f, d->finalDrive + dir * 0.05f);
		else if (item >= 3 && item <= 8) d->gearRatios[item - 3] =
			fmaxf(0.2f, d->gearRatios[item - 3] + dir * 0.05f);
		break;
	case WORKSHOP_DIFF:
		if (item == 0) d->diffFrontLock = clampf(d->diffFrontLock + dir * 0.05f, 0.0f, 1.0f);
		else if (item == 1) d->diffCenterLock = clampf(d->diffCenterLock + dir * 0.05f, 0.0f, 1.0f);
		else if (item == 2) d->diffRearLock = clampf(d->diffRearLock + dir * 0.05f, 0.0f, 1.0f);
		else if (item == 3) d->frontTorqueShare = clampf(d->frontTorqueShare + dir * 0.05f, 0.0f, 1.0f);
		break;
	case WORKSHOP_SUSPENSION:
		if (item == 0) d->rideHeight = fmaxf(0.04f, d->rideHeight + dir * 0.01f);
		else if (item == 1) d->springRate = fmaxf(1000.0f, d->springRate + dir * 1000.0f);
		else if (item == 2) d->damperCompression = fmaxf(0.0f, d->damperCompression + dir * 100.0f);
		else if (item == 3) d->damperRebound = fmaxf(0.0f, d->damperRebound + dir * 100.0f);
		break;
	case WORKSHOP_TIRES:
		if (item == 0) {
			int t = ((int)d->tireType + dir + F3DS_TIRE_COUNT) % F3DS_TIRE_COUNT;
			static const float grip[F3DS_TIRE_COUNT] = { 1.0f, 1.12f, 1.24f, 1.32f, 0.92f, 0.82f };
			d->tireType = (F3DSTireType)t; d->tireGrip = grip[t];
		} else if (item == 1) d->tireGrip = fmaxf(0.2f, d->tireGrip + dir * 0.05f);
		break;
	default: break;
	}
}

static void draw_workshop_page(F3DSWorkshopPage page, int selected, const F3DSVehicleDef *d)
{
	menu_title("ATELIER - REGLAGES");
	if (page == WORKSHOP_ENGINE) {
		for (int i = 0; i < 10; ++i) menu_ui_item(i < 9 ? 6 + i : 16, selected == i);
		printf("\x1b[4;4HMoteur");
		printf("\x1b[6;4H%s Couple : %.0f Nm", selected == 0 ? ">" : " ", d->torqueBase);
		printf("\x1b[7;4H%s Turbo : %s", selected == 1 ? ">" : " ", d->turboInstalled ? "oui" : "non");
		printf("\x1b[8;4H%s Pression turbo : %.1f bar", selected == 2 ? ">" : " ", d->turboBoostBar);
		printf("\x1b[9;4H%s Regime maximal : %.0f RPM", selected == 3 ? ">" : " ", d->maxRpm);
		printf("\x1b[10;4H%s Admission niveau : %d", selected == 4 ? ">" : " ", d->intakeLevel);
		printf("\x1b[11;4H%s Echappement niveau : %d", selected == 5 ? ">" : " ", d->exhaustLevel);
		printf("\x1b[12;4H%s Arbre a cames : %d", selected == 6 ? ">" : " ", d->camshaftLevel);
		printf("\x1b[13;4H%s Injecteurs : %d", selected == 7 ? ">" : " ", d->injectorLevel);
		printf("\x1b[14;4H%s Pistons : %d", selected == 8 ? ">" : " ", d->pistonLevel);
		printf("\x1b[16;4H%s Retour", selected == 9 ? ">" : " ");
	} else if (page == WORKSHOP_GEARBOX) {
		for (int i = 0; i < 10; ++i) menu_ui_item(i < 9 ? 6 + i : 16, selected == i);
		printf("\x1b[4;4HBoite de vitesses");
		printf("\x1b[6;4H%s Transmission : %s", selected == 0 ? ">" : " ", d->manualTransmission ? "manuelle" : "automatique");
		printf("\x1b[7;4H%s Nombre de rapports : %d", selected == 1 ? ">" : " ", d->gearCount);
		printf("\x1b[8;4H%s Couple final : %.2f", selected == 2 ? ">" : " ", d->finalDrive);
		for (int i = 0; i < F3DS_GEARS; ++i)
			printf("\x1b[%d;4H%s Rapport %d : %.2f", 9 + i, selected == i + 3 ? ">" : " ", i + 1, d->gearRatios[i]);
		printf("\x1b[16;4H%s Retour", selected == 9 ? ">" : " ");
	} else if (page == WORKSHOP_DIFF) {
		for (int i = 0; i < 5; ++i) menu_ui_item(i < 4 ? 6 + i : 11, selected == i);
		printf("\x1b[4;4HDifferentiels");
		printf("\x1b[6;4H%s Blocage avant : %.2f", selected == 0 ? ">" : " ", d->diffFrontLock);
		printf("\x1b[7;4H%s Blocage central : %.2f", selected == 1 ? ">" : " ", d->diffCenterLock);
		printf("\x1b[8;4H%s Blocage arriere : %.2f", selected == 2 ? ">" : " ", d->diffRearLock);
		printf("\x1b[9;4H%s Couple vers avant : %.2f", selected == 3 ? ">" : " ", d->frontTorqueShare);
		printf("\x1b[11;4H%s Retour", selected == 4 ? ">" : " ");
	} else if (page == WORKSHOP_SUSPENSION) {
		for (int i = 0; i < 5; ++i) menu_ui_item(i < 4 ? 6 + i : 11, selected == i);
		printf("\x1b[4;4HSuspension");
		printf("\x1b[6;4H%s Hauteur : %.2f m", selected == 0 ? ">" : " ", d->rideHeight);
		printf("\x1b[7;4H%s Raideur ressort : %.0f N/m", selected == 1 ? ">" : " ", d->springRate);
		printf("\x1b[8;4H%s Compression : %.0f", selected == 2 ? ">" : " ", d->damperCompression);
		printf("\x1b[9;4H%s Detente : %.0f", selected == 3 ? ">" : " ", d->damperRebound);
		printf("\x1b[11;4H%s Retour", selected == 4 ? ">" : " ");
	} else {
		for (int i = 0; i < 3; ++i) menu_ui_item(i < 2 ? 6 + i : 9, selected == i);
		printf("\x1b[4;4HPneumatiques");
		printf("\x1b[6;4H%s Type : %s", selected == 0 ? ">" : " ", tire_name(d->tireType));
		printf("\x1b[7;4H%s Coefficient grip : %.2f", selected == 1 ? ">" : " ", d->tireGrip);
		printf("\x1b[9;4H%s Retour", selected == 2 ? ">" : " ");
	}
	printf("\x1b[21;4HGauche/Droite : modifier  A/B : retour");
}

static const char *key_name(u32 key)
{
	switch (key) {
	case KEY_A: return "A"; case KEY_B: return "B"; case KEY_X: return "X"; case KEY_Y: return "Y";
	case KEY_L: return "L"; case KEY_R: return "R"; case KEY_ZL: return "ZL"; case KEY_ZR: return "ZR";
	default: return "?";
	}
}

static int key_is_used(const F3DSSettings *s, int action, u32 key)
{
	if (action != 1 && s->keyThrottle == key) return 1;
	if (action != 2 && s->keyBrake == key) return 1;
	if (action != 3 && s->keyHandbrake == key) return 1;
	if (action != 4 && s->keyCamera == key) return 1;
	return 0;
}

static void assign_key(F3DSSettings *s, int action, u32 key)
{
	switch (action) {
	case 1: s->keyThrottle = key; break; case 2: s->keyBrake = key; break;
	case 3: s->keyHandbrake = key; break; case 4: s->keyCamera = key; break;
	default: break;
	}
}

static void draw_pause_menu(int selected, const F3DSSettings *s)
{
	menu_title("PAUSE");
	for (int i = 0; i < 17; ++i) menu_ui_item(4 + i, selected == i);
	printf("\x1b[4;5H%s Reprendre", selected == 0 ? ">" : " ");
	printf("\x1b[5;5H%s Retour a l'atelier", selected == 1 ? ">" : " ");
	printf("\x1b[6;5H%s Choix du vehicule", selected == 2 ? ">" : " ");
	printf("\x1b[7;5H%s Menu principal", selected == 3 ? ">" : " ");
	printf("\x1b[8;5H%s Vue : %s", selected == 4 ? ">" : " ", s->cameraMode == F3DS_CAM_CHASE ? "exterieure" : "cockpit");
	printf("\x1b[9;5H%s Distance camera : %.1f", selected == 5 ? ">" : " ", s->chaseDistance);
	printf("\x1b[10;5H%s Hauteur camera : %.1f", selected == 6 ? ">" : " ", s->chaseHeight);
	printf("\x1b[11;5H%s Recentrage : %.1f", selected == 7 ? ">" : " ", s->recenterSpeed);
	printf("\x1b[12;5H%s Sensibilite : %.1f", selected == 8 ? ">" : " ", s->steerSensitivity);
	printf("\x1b[13;5H%s Zone morte : %.2f", selected == 9 ? ">" : " ", s->deadzone);
	printf("\x1b[14;5H%s Limiteur : %d FPS", selected == 10 ? ">" : " ", s->fpsLimit);
	printf("\x1b[15;5H%s HUD : %s", selected == 11 ? ">" : " ", s->hudVisible ? "visible" : "masque");
	printf("\x1b[16;5H%s Accelerateur : %s", selected == 12 ? ">" : " ", key_name(s->keyThrottle));
	printf("\x1b[17;5H%s Frein : %s", selected == 13 ? ">" : " ", key_name(s->keyBrake));
	printf("\x1b[18;5H%s Frein a main : %s", selected == 14 ? ">" : " ", key_name(s->keyHandbrake));
	printf("\x1b[19;5H%s Changer de vue : %s", selected == 15 ? ">" : " ", key_name(s->keyCamera));
	printf("\x1b[20;5H%s Sauver reglages", selected == 16 ? ">" : " ");
}

static void pause_adjust(F3DSSettings *s, F3DSCamera *cam, int item, int dir)
{
	switch (item) {
	case 5: s->chaseDistance = clampf(s->chaseDistance + dir * 0.5f, 3.0f, 20.0f); cam->chaseDistance = s->chaseDistance; break;
	case 6: s->chaseHeight = clampf(s->chaseHeight + dir * 0.25f, 1.0f, 12.0f); cam->chaseHeight = s->chaseHeight; break;
	case 7: s->recenterSpeed = clampf(s->recenterSpeed + dir * 0.2f, 0.2f, 8.0f); cam->recenterSpeed = s->recenterSpeed; break;
	case 8: s->steerSensitivity = clampf(s->steerSensitivity + dir * 0.1f, 0.1f, 3.0f); break;
	case 9: s->deadzone = clampf(s->deadzone + dir * 0.02f, 0.0f, 0.5f); break;
	case 10: s->fpsLimit = s->fpsLimit == 30 ? 60 : 30; break;
	case 11: s->hudVisible = !s->hudVisible; break;
	default: break;
	}
}

static void car_place_in_garage(F3DSVehicleState *car, const F3DSVehicleDef *def,
	const F3DSTrack *track)
{
	car->pos = track->pts[0];
	car->yaw = atan2f(track->fwd[0].x, track->fwd[0].z);
	car->vx = car->vz = car->vy = car->speed = car->yawRate = 0.0f;
	car->steerAngle = car->suspensionTravel = car->shiftTimer = 0.0f;
	car->airborne = false;
	car->gear = 1;
	car->rpm = def->idleRpm;
	car->engineLoad = car->throttle = 0.0f;
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;
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
		gfxExit(); return 0;
	}

	renderer_init();
	F3DSSettings settings; settings_defaults(&settings); settings_load(&settings);
	F3DSLoopState loop; core_loop_init(&loop, 1.0f / 60.0f, 5.0f / 60.0f);
	F3DSInputConfig inputCfg; input_config_defaults(&inputCfg);
	inputCfg.steerSensitivity = settings.steerSensitivity; inputCfg.deadzone = settings.deadzone;
	inputCfg.keyThrottle = settings.keyThrottle; inputCfg.keyBrake = settings.keyBrake;
	inputCfg.keyHandbrake = settings.keyHandbrake; inputCfg.keyCamera = settings.keyCamera;
	F3DSInputState input; memset(&input, 0, sizeof(input));

	F3DSVehicleDef def; vehicle_def_default(&def);
	F3DSTrack track; track_init(&track);
	F3DSTerrain terrain; terrain_init(&terrain, &track);
	renderer_set_track(&track, &terrain);
	F3DSVehicleState car; memset(&car, 0, sizeof(car));
	car.visualModelId = 0; car.gear = 1; car.rpm = def.idleRpm;
	car.pos = fvec3(track.pts[0].x, 0.0f, track.pts[0].z);
	car.yaw = atan2f(track.fwd[0].x, track.fwd[0].z);
	F3DSCamera cam; camera_init(&cam);
	cam.chaseDistance = settings.chaseDistance; cam.chaseHeight = settings.chaseHeight;
	cam.recenterSpeed = settings.recenterSpeed; cam.orbitYaw = settings.orbitYaw;
	cam.orbitPitch = settings.orbitPitch; cam.mode = (F3DSCameraMode)settings.cameraMode;
	camera_update(&cam, &car, &input, 0.0f);

	(void)engine_audio_init();
	F3DSScreen screen = SCREEN_MAIN;
	F3DSWorkshopPage workshopPage = WORKSHOP_HOME;
	int mainSelected = 0, carSelected = 0, workshopSelected = 0, pauseSelected = 0, rebindAction = 0;
	unsigned int frameCounter = 0;
	while (aptMainLoop()) {
		int steps = core_loop_begin_frame(&loop);
		input_poll(&input, &inputCfg);
		if (input.exit) break;

		if (screen != SCREEN_DRIVE) loop.accumulator = 0.0f;
		if (screen == SCREEN_DRIVE || screen == SCREEN_PAUSE) renderer_set_garage(false);
		if (screen == SCREEN_MAIN) {
			engine_audio_set_enabled(false);
			if (input.menuUp) mainSelected = (mainSelected + 2) % 3;
			if (input.menuDown) mainSelected = (mainSelected + 1) % 3;
			if (input.menuConfirm) {
				if (mainSelected == 0) { screen = SCREEN_CAR; carSelected = 0; }
				else if (mainSelected == 1) screen = SCREEN_PROGRESSION_INFO;
				else break;
			}
			draw_main_menu(mainSelected);
		} else if (screen == SCREEN_PROGRESSION_INFO) {
			engine_audio_set_enabled(false);
			menu_title("MODE PROGRESSION");
			menu_ui_panel(48, 96);
			printf("\x1b[7;5HLe mode Progression n'est pas encore");
			printf("\x1b[8;5Himplemente : collection, decouvertes");
			printf("\x1b[9;5Het economie seront ajoutes ensuite.");
			printf("\x1b[13;5HLe Mode Libre est jouable maintenant.");
			printf("\x1b[16;5HB pour revenir au menu.");
			if (input.menuBack || input.menuConfirm) screen = SCREEN_MAIN;
		} else if (screen == SCREEN_CAR) {
			F3DSInputState cameraInput = input;
			cameraInput.camDistanceUp = cameraInput.camDistanceDown = 0;
			cameraInput.camHeightUp = cameraInput.camHeightDown = 0;
			camera_update(&cam, &car, &cameraInput, loop.frameTime);
			engine_audio_set_enabled(true);
			car.rpm = fmaxf(car.rpm, def.idleRpm);
			car.engineLoad = 0.0f;
			engine_audio_update(&def, &car);
			if (input.menuUp || input.menuDown) carSelected = 1 - carSelected;
			if (input.menuConfirm && carSelected == 0) {
				screen = SCREEN_WORKSHOP; workshopPage = WORKSHOP_HOME; workshopSelected = 0;
			} else if ((input.menuConfirm && carSelected == 1) || input.menuBack) screen = SCREEN_MAIN;
			draw_car_select(carSelected);
		} else if (screen == SCREEN_WORKSHOP) {
			F3DSInputState cameraInput = input;
			cameraInput.camDistanceUp = cameraInput.camDistanceDown = 0;
			cameraInput.camHeightUp = cameraInput.camHeightDown = 0;
			camera_update(&cam, &car, &cameraInput, loop.frameTime);
			engine_audio_set_enabled(true);
			car.engineLoad = 0.08f;
			car.rpm = fmaxf(car.rpm, def.idleRpm);
			engine_audio_update(&def, &car);
			if (workshopPage == WORKSHOP_HOME) {
				if (input.menuUp) workshopSelected = (workshopSelected + 6) % 7;
				if (input.menuDown) workshopSelected = (workshopSelected + 1) % 7;
				if (input.menuConfirm) {
					if (workshopSelected == 5) { screen = SCREEN_DRIVE; loop.accumulator = 0.0f; }
					else if (workshopSelected == 6) { screen = SCREEN_CAR; carSelected = 0; }
					else { workshopPage = (F3DSWorkshopPage)(workshopSelected + 1); workshopSelected = 0; }
				}
				if (input.menuBack) { screen = SCREEN_CAR; carSelected = 0; }
				draw_workshop_home(workshopSelected, &def);
			} else {
				const int count = workshop_count(workshopPage);
				if (input.menuUp) workshopSelected = (workshopSelected + count - 1) % count;
				if (input.menuDown) workshopSelected = (workshopSelected + 1) % count;
				if (input.menuLeft) workshop_adjust(&def, workshopPage, workshopSelected, -1);
				if (input.menuRight) workshop_adjust(&def, workshopPage, workshopSelected, 1);
				if (input.menuBack || (input.menuConfirm && workshopSelected == count - 1)) {
					workshopPage = WORKSHOP_HOME; workshopSelected = 0;
				}
				draw_workshop_page(workshopPage, workshopSelected, &def);
			}
		} else if (screen == SCREEN_PAUSE) {
			engine_audio_set_enabled(false);
			if (rebindAction) {
				const u32 candidate = input.keysDown & REBIND_KEY_MASK;
				if (candidate && !(candidate & (candidate - 1u))) {
					if (!key_is_used(&settings, rebindAction, candidate))
						assign_key(&settings, rebindAction, candidate);
					rebindAction = 0;
				} else if (input.menuBack) rebindAction = 0;
			} else {
				if (input.menuUp) pauseSelected = (pauseSelected + 16) % 17;
				if (input.menuDown) pauseSelected = (pauseSelected + 1) % 17;
				if (input.menuLeft) pause_adjust(&settings, &cam, pauseSelected, -1);
				if (input.menuRight) pause_adjust(&settings, &cam, pauseSelected, 1);
				if (input.menuConfirm) {
					if (pauseSelected == 0) { screen = SCREEN_DRIVE; loop.accumulator = 0.0f; }
					else if (pauseSelected == 1) { car_place_in_garage(&car, &def, &track); camera_update(&cam, &car, &input, 0.0f); screen = SCREEN_WORKSHOP; workshopPage = WORKSHOP_HOME; workshopSelected = 0; }
					else if (pauseSelected == 2) { car_place_in_garage(&car, &def, &track); camera_update(&cam, &car, &input, 0.0f); screen = SCREEN_CAR; carSelected = 0; }
					else if (pauseSelected == 3) { car_place_in_garage(&car, &def, &track); camera_update(&cam, &car, &input, 0.0f); screen = SCREEN_MAIN; }
					else if (pauseSelected == 4) {
						cam.mode = cam.mode == F3DS_CAM_CHASE ? F3DS_CAM_COCKPIT : F3DS_CAM_CHASE;
						settings.cameraMode = cam.mode;
					} else if (pauseSelected >= 12 && pauseSelected <= 15) rebindAction = pauseSelected - 11;
					else if (pauseSelected == 16) settings_save(&settings);
				}
				if (input.menuBack || input.pausePressed) { screen = SCREEN_DRIVE; loop.accumulator = 0.0f; }
			}
			inputCfg.steerSensitivity = settings.steerSensitivity; inputCfg.deadzone = settings.deadzone;
			inputCfg.keyThrottle = settings.keyThrottle; inputCfg.keyBrake = settings.keyBrake;
			inputCfg.keyHandbrake = settings.keyHandbrake; inputCfg.keyCamera = settings.keyCamera;
			draw_pause_menu(pauseSelected, &settings);
			if (rebindAction) printf("\x1b[22;5HAppuie sur une touche pour l'assigner (B annule).");
		} else if (screen == SCREEN_DRIVE) {
			engine_audio_set_enabled(true);
			if (input.pausePressed) {
				screen = SCREEN_PAUSE; pauseSelected = 0; loop.accumulator = 0.0f;
				engine_audio_set_enabled(false);
			} else {
				for (int i = 0; i < steps; ++i) {
					physics_step(&def, &car, &input, &track, &terrain, loop.dt);
					input.toggleView = 0;
					input.gearUp = input.gearDown = 0;
					input.camDistanceUp = input.camDistanceDown = 0;
					input.camHeightUp = input.camHeightDown = 0;
				}
				camera_update(&cam, &car, &input, loop.frameTime);
				engine_audio_update(&def, &car);
				if (settings.fpsLimit == 30 && (frameCounter++ & 1u)) {
					/* Simulation remains at 60 Hz; only presentation is skipped. */
				} else {
					renderer_render(&cam, &car);
					if (settings.hudVisible) hud_render(&car, &cam, &loop);
					else consoleClear();
				}
			}
		}

		if (screen == SCREEN_MAIN || screen == SCREEN_CAR || screen == SCREEN_WORKSHOP || screen == SCREEN_PROGRESSION_INFO)
			renderer_set_garage(true);
		else renderer_set_garage(false);
		if (screen != SCREEN_DRIVE) renderer_render(&cam, &car);
		gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
	}

	settings.steerSensitivity = inputCfg.steerSensitivity;
	settings.deadzone = inputCfg.deadzone;
	settings.chaseDistance = cam.chaseDistance; settings.chaseHeight = cam.chaseHeight;
	settings.recenterSpeed = cam.recenterSpeed; settings.orbitYaw = cam.orbitYaw;
	settings.orbitPitch = cam.orbitPitch; settings.cameraMode = cam.mode;
	settings.keyThrottle = inputCfg.keyThrottle; settings.keyBrake = inputCfg.keyBrake;
	settings.keyHandbrake = inputCfg.keyHandbrake; settings.keyCamera = inputCfg.keyCamera;
	settings_save(&settings);
	engine_audio_exit();
	renderer_exit(); gfxExit();
	return 0;
}
