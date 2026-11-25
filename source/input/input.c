#include "input.h"

void input_config_defaults(F3DSInputConfig *cfg)
{
	cfg->steerSensitivity = 1.0f;
	cfg->deadzone = 0.12f;
}

static float pad_axis(int raw)
{
	return (float)raw / 156.0f;
}

static float apply_deadzone(float v, float dz)
{
	if (v > dz) {
		const float span = 1.0f - dz;
		return (v - dz) / span;
	}
	if (v < -dz) {
		const float span = 1.0f - dz;
		return (v + dz) / span;
	}
	return 0.0f;
}

void input_poll(F3DSInputState *in, const F3DSInputConfig *cfg)
{
	hidScanInput();
	const u32 kDown = hidKeysDown();
	const u32 kHeld = hidKeysHeld();

	in->throttle = (kHeld & KEY_A) ? 1.0f : 0.0f;
	in->brake = (kHeld & KEY_B) ? 1.0f : 0.0f;
	in->handbrake = (kHeld & KEY_Y) ? 1 : 0;

	/* Circle pad steering: deadzone + sensitivity, no left/right inversion. */
	circlePosition cp;
	hidCircleRead(&cp);
	const float raw = apply_deadzone(pad_axis(cp.dx), cfg->deadzone);
	float curve = fabsf(raw);
	curve = curve * curve * (3.0f - 2.0f * curve) * 0.5f + curve * 0.5f;
	in->steer = (raw < 0.0f ? -curve : curve) * cfg->steerSensitivity;
	in->steer = clampf(in->steer, -1.0f, 1.0f);

	/* New 3DS C-stick for camera orbit. */
	circlePosition cs;
	hidCstickRead(&cs);
	in->orbitYaw = apply_deadzone(pad_axis(cs.dx), 0.10f);
	in->orbitPitch = apply_deadzone(pad_axis(cs.dy), 0.10f);

	in->toggleView = (kDown & KEY_X) ? 1 : 0;
	in->camDistanceDown = (kDown & KEY_DUP) ? 1 : 0;
	in->camDistanceUp = (kDown & KEY_DDOWN) ? 1 : 0;
	in->camHeightDown = (kDown & KEY_DLEFT) ? 1 : 0;
	in->camHeightUp = (kDown & KEY_DRIGHT) ? 1 : 0;

	in->exit = (kDown & KEY_START) ? 1 : 0;
}
