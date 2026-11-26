/* Forza3DS - application state machine. Hardware independent: it consumes raw
 * input and elapsed time, and exposes what to draw, play and save. */
#ifndef F3DS_APP_H
#define F3DS_APP_H

#include "camera.h"
#include "core.h"
#include "garage.h"
#include "settings.h"
#include "synth.h"
#include "terrain.h"
#include "track.h"
#include "workshop.h"

typedef enum {
	F3DS_SCREEN_MAIN = 0,
	F3DS_SCREEN_CARS,
	F3DS_SCREEN_WORKSHOP,
	F3DS_SCREEN_WORKSHOP_PAGE,
	F3DS_SCREEN_DRIVE,
	F3DS_SCREEN_PAUSE,
	F3DS_SCREEN_SETTINGS,
	F3DS_SCREEN_PROGRESSION,
	F3DS_SCREEN_CONFIRM_QUIT
} F3DSScreen;

enum { MAIN_FREE = 0, MAIN_PROGRESSION, MAIN_SETTINGS, MAIN_QUIT, MAIN_COUNT };
enum {
	HOME_ENGINE = 0, HOME_GEARBOX, HOME_DIFF, HOME_SUSPENSION, HOME_TIRES, HOME_LOOK,
	HOME_DRIVE, HOME_SAVE, HOME_RESTORE, HOME_CARS, HOME_COUNT
};
enum {
	PAUSE_RESUME = 0, PAUSE_RESET, PAUSE_MAP, PAUSE_SETTINGS, PAUSE_WORKSHOP, PAUSE_CARS,
	PAUSE_MAIN, PAUSE_COUNT
};

typedef struct {
	F3DSScreen screen;
	F3DSScreen settingsReturn;
	int selected;
	int homeSelected;
	F3DSWorkshopPage page;
	int rebindAction;          /* -1 when not waiting for a key */
	bool mapVisible;

	F3DSSettings settings;
	F3DSGarage garage;
	bool settingsDirty;
	bool garageDirty;
	char settingsPath[128];
	char garagePath[128];
	char message[64];
	float messageTime;

	F3DSTrack track;
	F3DSTerrain terrain;
	F3DSVehicleState car;
	FVec3 prevPos;
	float prevYaw;
	F3DSCamera camera;
	F3DSLoopState loop;
	F3DSInputState input;
	int pendingGearUp, pendingGearDown;
	float inspectAngle;

	bool quit;
	uint32_t uiRevision;       /* bumps when the lower screen must be redrawn */
} F3DSApp;

/* What the top screen shows this frame. */
typedef struct {
	bool garageScene;
	bool cockpit;
	FVec3 pos;                 /* interpolated car pose */
	float yaw;
	float groundY, groundPitch; /* ground under the car, nose-up positive */
	FVec3 eye, target;
	int carId, paint, rim;
	const F3DSVehicleDef *def;
	const F3DSVehicleState *state;
	int shadows;
	int drawDistance;
} F3DSView;

/* dataDir is the folder holding settings.bin and saves/. */
void app_init(F3DSApp *app, const char *dataDir);
void app_update(F3DSApp *app, const F3DSRawInput *raw, float elapsed);
/* Lid closed or HOME pressed: pause and wait for an explicit resume. */
void app_suspend(F3DSApp *app);
/* Saves controls, camera and graphics settings. Never saves the garage. */
void app_shutdown(F3DSApp *app);
void app_view(const F3DSApp *app, F3DSView *view);
bool app_audio_paused(const F3DSApp *app);
void app_audio_params(F3DSApp *app, F3DSSynthParams *p);

#endif
