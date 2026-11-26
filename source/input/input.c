/* Pure mapping from a raw hardware sample to game actions. */
#include "input.h"
#include <string.h>

void input_config_defaults(F3DSInputConfig *cfg)
{
	cfg->steerSensitivity = 1.0f;
	cfg->deadzone = 0.12f;
	cfg->keys[F3DS_ACT_THROTTLE] = KEY_A;
	cfg->keys[F3DS_ACT_BRAKE] = KEY_B;
	cfg->keys[F3DS_ACT_HANDBRAKE] = KEY_Y;
	cfg->keys[F3DS_ACT_CAMERA] = KEY_X;
	cfg->keys[F3DS_ACT_GEAR_UP] = KEY_R;
	cfg->keys[F3DS_ACT_GEAR_DOWN] = KEY_L;
}

static float pad_axis(int raw, float range) { return clampf((float)raw / range, -1.0f, 1.0f); }

static float apply_deadzone(float v, float dz)
{
	dz = clampf(dz, 0.0f, 0.9f);
	if (v > dz) return (v - dz) / (1.0f - dz);
	if (v < -dz) return (v + dz) / (1.0f - dz);
	return 0.0f;
}

void input_map(F3DSInputState *in, const F3DSInputConfig *cfg, const F3DSRawInput *raw)
{
	memset(in, 0, sizeof(*in));
	in->keysDown = raw->down;
	in->throttle = (raw->held & cfg->keys[F3DS_ACT_THROTTLE]) ? 1.0f : 0.0f;
	in->brake = (raw->held & cfg->keys[F3DS_ACT_BRAKE]) ? 1.0f : 0.0f;
	in->handbrake = (raw->held & cfg->keys[F3DS_ACT_HANDBRAKE]) ? 1 : 0;
	in->gearUp = (raw->down & cfg->keys[F3DS_ACT_GEAR_UP]) ? 1 : 0;
	in->gearDown = (raw->down & cfg->keys[F3DS_ACT_GEAR_DOWN]) ? 1 : 0;
	in->toggleView = (raw->down & cfg->keys[F3DS_ACT_CAMERA]) ? 1 : 0;

	const float stick = apply_deadzone(pad_axis(raw->circleX, 150.0f), cfg->deadzone);
	const float mag = fabsf(stick);
	/* Half linear, half smoothstep: precise near the centre, full lock at the edge. */
	const float curve = 0.5f * mag + 0.5f * mag * mag * (3.0f - 2.0f * mag);
	in->steer = clampf((stick < 0.0f ? -curve : curve) * cfg->steerSensitivity, -1.0f, 1.0f);

	in->orbit = apply_deadzone(pad_axis(raw->cstickX, 140.0f), 0.12f);
	in->camDistance = ((raw->held & KEY_DDOWN) ? 1 : 0) - ((raw->held & KEY_DUP) ? 1 : 0);
	in->camHeight = ((raw->held & KEY_DRIGHT) ? 1 : 0) - ((raw->held & KEY_DLEFT) ? 1 : 0);

	in->pausePressed = (raw->down & KEY_START) ? 1 : 0;
	in->mapPressed = (raw->down & KEY_SELECT) ? 1 : 0;
	/* Menus accept D-pad and Circle Pad, with key repeat. */
	in->menuUp = (raw->repeat & KEY_UP) ? 1 : 0;
	in->menuDown = (raw->repeat & KEY_DOWN) ? 1 : 0;
	in->menuLeft = (raw->repeat & KEY_LEFT) ? 1 : 0;
	in->menuRight = (raw->repeat & KEY_RIGHT) ? 1 : 0;
	in->menuConfirm = (raw->down & KEY_A) ? 1 : 0;
	in->menuBack = (raw->down & KEY_B) ? 1 : 0;
}

const char *input_key_name(u32 key)
{
	switch (key) {
	case KEY_A: return "A";
	case KEY_B: return "B";
	case KEY_X: return "X";
	case KEY_Y: return "Y";
	case KEY_L: return "L";
	case KEY_R: return "R";
	case KEY_ZL: return "ZL";
	case KEY_ZR: return "ZR";
	default: return "?";
	}
}

const char *input_action_name(F3DSAction a)
{
	static const char *names[F3DS_ACT_COUNT] = {
		"Accélérateur", "Frein", "Frein à main", "Changer de vue", "Rapport +", "Rapport -"
	};
	return (unsigned)a < F3DS_ACT_COUNT ? names[a] : "?";
}

static bool single_bindable(u32 key)
{
	return key && !(key & (key - 1u)) && !(key & ~(u32)F3DS_REBIND_KEYS);
}

void input_rebind(F3DSInputConfig *cfg, F3DSAction action, u32 key)
{
	if ((unsigned)action >= F3DS_ACT_COUNT || !single_bindable(key))
		return;
	for (int i = 0; i < F3DS_ACT_COUNT; ++i)
		if (i != (int)action && cfg->keys[i] == key)
			cfg->keys[i] = cfg->keys[action];
	cfg->keys[action] = key;
}

bool input_config_valid(const F3DSInputConfig *cfg)
{
	if (!isfinite(cfg->steerSensitivity) || cfg->steerSensitivity < 0.1f || cfg->steerSensitivity > 3.0f ||
	    !isfinite(cfg->deadzone) || cfg->deadzone < 0.0f || cfg->deadzone > 0.5f)
		return false;
	for (int i = 0; i < F3DS_ACT_COUNT; ++i) {
		if (!single_bindable(cfg->keys[i])) return false;
		for (int j = 0; j < i; ++j)
			if (cfg->keys[i] == cfg->keys[j]) return false;
	}
	return true;
}
