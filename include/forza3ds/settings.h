/* Forza3DS - controls, camera, display and audio settings (saved
 * automatically; never part of the progression save). */
#ifndef F3DS_SETTINGS_H
#define F3DS_SETTINGS_H

#include "input.h"

enum { F3DS_SIM_PERFORMANCE = 0, F3DS_SIM_PRECISE = 1 };
enum { F3DS_DRAW_NEAR = 0, F3DS_DRAW_MEDIUM, F3DS_DRAW_FAR, F3DS_DRAW_COUNT };

typedef struct {
	F3DSInputConfig input;
	float chaseDistance, chaseHeight, recenterSpeed;
	int32_t cameraMode;
	int32_t fpsLimit;      /* 30 or 60 */
	int32_t showHud, showFps;
	int32_t simProfile;
	int32_t abs, tcs;
	int32_t drawDistance, shadows;
	float engineVolume, ambienceVolume;
} F3DSSettings;

void settings_defaults(F3DSSettings *s);
bool settings_valid(const F3DSSettings *s);
/* Loads the current format, or migrates the version 2 file. */
bool settings_load(F3DSSettings *s, const char *path);
bool settings_save(const F3DSSettings *s, const char *path);
float settings_sim_dt(const F3DSSettings *s);

int settings_item_count(void);
void settings_item_text(const F3DSSettings *s, int item, char *out, int size);
/* Returns true when the item is an input binding; *action receives it. */
bool settings_item_binding(int item, F3DSAction *action);
bool settings_adjust(F3DSSettings *s, int item, int dir);

#endif
