#include "settings.h"
#include "camera.h"
#include "save_io.h"
#include <stdio.h>
#include <string.h>

#define SETTINGS_MAGIC 0x46334453u
#define SETTINGS_VERSION 3u

/* On-disk layout written by the previous release. */
typedef struct {
	uint32_t magic, version;
	float steerSensitivity, deadzone;
	float chaseDistance, chaseHeight, recenterSpeed;
	int32_t fpsLimit, hudVisible;
	uint32_t keyThrottle, keyBrake, keyHandbrake, keyCamera;
	float orbitYaw, orbitPitch;
	int32_t cameraMode;
} SettingsV2;

void settings_defaults(F3DSSettings *s)
{
	memset(s, 0, sizeof(*s));
	input_config_defaults(&s->input);
	s->chaseDistance = 7.5f;
	s->chaseHeight = 2.8f;
	s->recenterSpeed = 2.5f;
	s->cameraMode = F3DS_CAM_CHASE;
	s->fpsLimit = 30;
	s->showHud = 1;
	s->showFps = 0;
	s->simProfile = F3DS_SIM_PERFORMANCE;
	s->abs = 1;
	s->tcs = 1;
	s->drawDistance = F3DS_DRAW_MEDIUM;
	s->shadows = 1;
	s->engineVolume = 0.8f;
	s->ambienceVolume = 0.6f;
}

static bool in_range(float v, float lo, float hi) { return isfinite(v) && v >= lo && v <= hi; }
static bool flag(int v) { return v == 0 || v == 1; }

bool settings_valid(const F3DSSettings *s)
{
	return input_config_valid(&s->input) &&
		in_range(s->chaseDistance, 3.0f, 20.0f) && in_range(s->chaseHeight, 1.0f, 12.0f) &&
		in_range(s->recenterSpeed, 0.2f, 8.0f) &&
		(s->cameraMode == F3DS_CAM_CHASE || s->cameraMode == F3DS_CAM_COCKPIT) &&
		(s->fpsLimit == 30 || s->fpsLimit == 60) && flag(s->showHud) && flag(s->showFps) &&
		(s->simProfile == F3DS_SIM_PERFORMANCE || s->simProfile == F3DS_SIM_PRECISE) &&
		flag(s->abs) && flag(s->tcs) && flag(s->shadows) &&
		s->drawDistance >= 0 && s->drawDistance < F3DS_DRAW_COUNT &&
		in_range(s->engineVolume, 0.0f, 1.0f) && in_range(s->ambienceVolume, 0.0f, 1.0f);
}

static bool valid_payload(const void *p) { return settings_valid(p); }

static bool migrate_v2(F3DSSettings *s, const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	SettingsV2 v;
	const bool read = fread(&v, sizeof(v), 1, f) == 1 && fgetc(f) == EOF;
	fclose(f);
	if (!read || v.magic != SETTINGS_MAGIC || v.version != 2u)
		return false;
	F3DSSettings out;
	settings_defaults(&out);
	out.input.steerSensitivity = v.steerSensitivity;
	out.input.deadzone = v.deadzone;
	/* Swap-based rebinding keeps the gear keys unique. */
	input_rebind(&out.input, F3DS_ACT_THROTTLE, v.keyThrottle);
	input_rebind(&out.input, F3DS_ACT_BRAKE, v.keyBrake);
	input_rebind(&out.input, F3DS_ACT_HANDBRAKE, v.keyHandbrake);
	input_rebind(&out.input, F3DS_ACT_CAMERA, v.keyCamera);
	out.chaseDistance = v.chaseDistance;
	out.chaseHeight = v.chaseHeight;
	out.recenterSpeed = v.recenterSpeed;
	out.cameraMode = v.cameraMode;
	out.fpsLimit = v.fpsLimit;
	out.showHud = v.hudVisible;
	if (!settings_valid(&out))
		return false;
	*s = out;
	return true;
}

bool settings_load(F3DSSettings *s, const char *path)
{
	return save_read(path, SETTINGS_MAGIC, SETTINGS_VERSION, s, sizeof(*s), valid_payload) ||
		migrate_v2(s, path);
}

bool settings_save(const F3DSSettings *s, const char *path)
{
	return save_write(path, SETTINGS_MAGIC, SETTINGS_VERSION, s, sizeof(*s), valid_payload);
}

float settings_sim_dt(const F3DSSettings *s)
{
	return s->simProfile == F3DS_SIM_PRECISE ? 1.0f / 120.0f : 1.0f / 60.0f;
}

enum {
	ITEM_VIEW, ITEM_DISTANCE, ITEM_HEIGHT, ITEM_RECENTER, ITEM_SENSITIVITY, ITEM_DEADZONE,
	ITEM_BIND_FIRST,
	ITEM_ABS = ITEM_BIND_FIRST + F3DS_ACT_COUNT, ITEM_TCS, ITEM_SIM, ITEM_FPSLIMIT, ITEM_FPS,
	ITEM_HUD, ITEM_DRAW, ITEM_SHADOWS, ITEM_ENGINE_VOLUME, ITEM_AMBIENCE_VOLUME, ITEM_COUNT
};

int settings_item_count(void) { return ITEM_COUNT; }

static const char *yes_no(int v) { return v ? "oui" : "non"; }

void settings_item_text(const F3DSSettings *s, int item, char *out, int size)
{
	static const char *draw[F3DS_DRAW_COUNT] = { "courte", "moyenne", "longue" };
	F3DSAction a;
	if (settings_item_binding(item, &a)) {
		snprintf(out, size, "%s : %s", input_action_name(a), input_key_name(s->input.keys[a]));
		return;
	}
	switch (item) {
	case ITEM_VIEW: snprintf(out, size, "Vue : %s", s->cameraMode == F3DS_CAM_CHASE ? "extérieure" : "cockpit"); break;
	case ITEM_DISTANCE: snprintf(out, size, "Distance caméra : %.1f m", s->chaseDistance); break;
	case ITEM_HEIGHT: snprintf(out, size, "Hauteur caméra : %.2f m", s->chaseHeight); break;
	case ITEM_RECENTER: snprintf(out, size, "Vitesse de recentrage : %.1f", s->recenterSpeed); break;
	case ITEM_SENSITIVITY: snprintf(out, size, "Sensibilité direction : %.1f", s->input.steerSensitivity); break;
	case ITEM_DEADZONE: snprintf(out, size, "Zone morte : %.2f", s->input.deadzone); break;
	case ITEM_ABS: snprintf(out, size, "ABS : %s", yes_no(s->abs)); break;
	case ITEM_TCS: snprintf(out, size, "Antipatinage : %s", yes_no(s->tcs)); break;
	case ITEM_SIM: snprintf(out, size, "Simulation : %s", s->simProfile ? "précise 120 Hz" : "performance 60 Hz"); break;
	case ITEM_FPSLIMIT: snprintf(out, size, "Limiteur : %d FPS", (int)s->fpsLimit); break;
	case ITEM_FPS: snprintf(out, size, "Compteur FPS : %s", yes_no(s->showFps)); break;
	case ITEM_HUD: snprintf(out, size, "HUD : %s", s->showHud ? "visible" : "masqué"); break;
	case ITEM_DRAW: snprintf(out, size, "Distance d'affichage : %s", draw[(unsigned)s->drawDistance % F3DS_DRAW_COUNT]); break;
	case ITEM_SHADOWS: snprintf(out, size, "Ombres : %s", yes_no(s->shadows)); break;
	case ITEM_ENGINE_VOLUME: snprintf(out, size, "Volume moteur : %d %%", (int)lroundf(s->engineVolume * 100.0f)); break;
	case ITEM_AMBIENCE_VOLUME: snprintf(out, size, "Volume ambiance : %d %%", (int)lroundf(s->ambienceVolume * 100.0f)); break;
	default: snprintf(out, size, "?"); break;
	}
}

bool settings_item_binding(int item, F3DSAction *action)
{
	if (item < ITEM_BIND_FIRST || item >= ITEM_BIND_FIRST + F3DS_ACT_COUNT)
		return false;
	if (action)
		*action = (F3DSAction)(item - ITEM_BIND_FIRST);
	return true;
}

static float step(float v, int dir, float inc, float lo, float hi)
{
	/* Snap to the step grid so repeated changes do not accumulate error. */
	return clampf(roundf((v + dir * inc) / inc) * inc, lo, hi);
}

bool settings_adjust(F3DSSettings *s, int item, int dir)
{
	if (dir == 0)
		return false;
	dir = dir > 0 ? 1 : -1;
	switch (item) {
	case ITEM_VIEW: s->cameraMode = s->cameraMode == F3DS_CAM_CHASE ? F3DS_CAM_COCKPIT : F3DS_CAM_CHASE; break;
	case ITEM_DISTANCE: s->chaseDistance = step(s->chaseDistance, dir, 0.5f, 3.0f, 20.0f); break;
	case ITEM_HEIGHT: s->chaseHeight = step(s->chaseHeight, dir, 0.25f, 1.0f, 12.0f); break;
	case ITEM_RECENTER: s->recenterSpeed = step(s->recenterSpeed, dir, 0.2f, 0.2f, 8.0f); break;
	case ITEM_SENSITIVITY: s->input.steerSensitivity = step(s->input.steerSensitivity, dir, 0.1f, 0.1f, 3.0f); break;
	case ITEM_DEADZONE: s->input.deadzone = step(s->input.deadzone, dir, 0.02f, 0.0f, 0.5f); break;
	case ITEM_ABS: s->abs = !s->abs; break;
	case ITEM_TCS: s->tcs = !s->tcs; break;
	case ITEM_SIM: s->simProfile = !s->simProfile; break;
	case ITEM_FPSLIMIT: s->fpsLimit = s->fpsLimit == 30 ? 60 : 30; break;
	case ITEM_FPS: s->showFps = !s->showFps; break;
	case ITEM_HUD: s->showHud = !s->showHud; break;
	case ITEM_DRAW: s->drawDistance = (s->drawDistance + dir + F3DS_DRAW_COUNT) % F3DS_DRAW_COUNT; break;
	case ITEM_SHADOWS: s->shadows = !s->shadows; break;
	case ITEM_ENGINE_VOLUME: s->engineVolume = step(s->engineVolume, dir, 0.1f, 0.0f, 1.0f); break;
	case ITEM_AMBIENCE_VOLUME: s->ambienceVolume = step(s->ambienceVolume, dir, 0.1f, 0.0f, 1.0f); break;
	default: return false;
	}
	return true;
}
