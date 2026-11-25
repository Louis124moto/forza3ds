#include "input.h"

void input_config_defaults(F3DSInputConfig *cfg)
{
	cfg->steerSensitivity = 1.0f;
	cfg->deadzone = 0.12f;
	cfg->keyThrottle = KEY_A;
	cfg->keyBrake = KEY_B;
	cfg->keyHandbrake = KEY_Y;
	cfg->keyCamera = KEY_X;
}

static float pad_axis(int raw) { return (float)raw / 156.0f; }

static float apply_deadzone(float v, float dz)
{
	if (v > dz) return (v - dz) / (1.0f - dz);
	if (v < -dz) return (v + dz) / (1.0f - dz);
	return 0.0f;
}

void input_poll(F3DSInputState *in, const F3DSInputConfig *cfg)
{
	hidScanInput();
	const u32 down = hidKeysDown();
	const u32 held = hidKeysHeld();
	in->keysDown = down;

	in->throttle = (held & cfg->keyThrottle) ? 1.0f : 0.0f;
	in->brake = (held & cfg->keyBrake) ? 1.0f : 0.0f;
	in->handbrake = (held & cfg->keyHandbrake) ? 1 : 0;
	in->gearUp = (down & KEY_R) ? 1 : 0;
	in->gearDown = (down & KEY_L) ? 1 : 0;

	circlePosition cp;
	hidCircleRead(&cp);
	const float raw = apply_deadzone(pad_axis(cp.dx), cfg->deadzone);
	float curve = fabsf(raw);
	curve = curve * curve * (3.0f - 2.0f * curve) * 0.5f + curve * 0.5f;
	in->steer = clampf((raw < 0.0f ? -curve : curve) * cfg->steerSensitivity, -1.0f, 1.0f);

	circlePosition cs;
	hidCstickRead(&cs);
	in->orbitYaw = apply_deadzone(pad_axis(cs.dx), 0.10f);
	in->orbitPitch = apply_deadzone(pad_axis(cs.dy), 0.10f);

	in->toggleView = (down & cfg->keyCamera) ? 1 : 0;
	in->camDistanceDown = (down & KEY_DUP) ? 1 : 0;
	in->camDistanceUp = (down & KEY_DDOWN) ? 1 : 0;
	in->camHeightDown = (down & KEY_DLEFT) ? 1 : 0;
	in->camHeightUp = (down & KEY_DRIGHT) ? 1 : 0;

	in->pausePressed = (down & KEY_START) ? 1 : 0;
	in->menuUp = (down & KEY_DUP) ? 1 : 0;
	in->menuDown = (down & KEY_DDOWN) ? 1 : 0;
	in->menuLeft = (down & KEY_DLEFT) ? 1 : 0;
	in->menuRight = (down & KEY_DRIGHT) ? 1 : 0;
	in->menuConfirm = (down & KEY_A) ? 1 : 0;
	in->menuBack = (down & KEY_B) ? 1 : 0;
	in->exit = (down & KEY_SELECT) ? 1 : 0;
}
