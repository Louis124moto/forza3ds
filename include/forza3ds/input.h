#ifndef F3DS_INPUT_H
#define F3DS_INPUT_H

#include "common.h"

typedef struct {
	float throttle;
	float brake;
	float steer;
	int handbrake;
	int toggleView;
	float orbitYaw;
	float orbitPitch;
	int camDistanceUp;
	int camDistanceDown;
	int camHeightUp;
	int camHeightDown;
	int pausePressed;
	int menuUp;
	int menuDown;
	int menuLeft;
	int menuRight;
	int menuConfirm;
	int menuBack;
	int exit;
} F3DSInputState;

typedef struct {
	float steerSensitivity;
	float deadzone;
} F3DSInputConfig;

void input_config_defaults(F3DSInputConfig *cfg);
void input_poll(F3DSInputState *in, const F3DSInputConfig *cfg);

#endif
