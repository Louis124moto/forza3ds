#ifndef F3DS_INPUT_H
#define F3DS_INPUT_H

#include "common.h"

/* Rebindable driving actions. */
typedef enum {
	F3DS_ACT_THROTTLE = 0,
	F3DS_ACT_BRAKE,
	F3DS_ACT_HANDBRAKE,
	F3DS_ACT_CAMERA,
	F3DS_ACT_GEAR_UP,
	F3DS_ACT_GEAR_DOWN,
	F3DS_ACT_COUNT
} F3DSAction;

#define F3DS_REBIND_KEYS (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R | KEY_ZL | KEY_ZR)

/* Raw hardware sample, filled by input_hw.c (or by tests). */
typedef struct {
	u32 down, held, repeat;
	int circleX, circleY;
	int cstickX, cstickY;
} F3DSRawInput;

typedef struct {
	float steerSensitivity;
	float deadzone;
	u32 keys[F3DS_ACT_COUNT];
} F3DSInputConfig;

typedef struct {
	/* Driving. steer is positive to the RIGHT. */
	float throttle, brake, steer;
	int handbrake;
	int gearUp, gearDown;
	int absEnabled, tcsEnabled;
	/* Camera. */
	int toggleView;
	float orbit;                 /* C-stick horizontal, positive = right */
	int camDistance, camHeight;  /* held D-pad: -1, 0, +1 */
	/* Menus. */
	u32 keysDown;
	int pausePressed, mapPressed;
	int menuUp, menuDown, menuLeft, menuRight, menuConfirm, menuBack;
} F3DSInputState;

void input_config_defaults(F3DSInputConfig *cfg);
void input_map(F3DSInputState *in, const F3DSInputConfig *cfg, const F3DSRawInput *raw);
const char *input_key_name(u32 key);
const char *input_action_name(F3DSAction action);
/* Bind key to action; if another action already uses it, the two are swapped. */
void input_rebind(F3DSInputConfig *cfg, F3DSAction action, u32 key);
bool input_config_valid(const F3DSInputConfig *cfg);

/* Hardware (3DS only). */
void input_hw_init(void);
void input_hw_read(F3DSRawInput *raw);

#endif
