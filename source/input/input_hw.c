#include "input.h"
#include <3ds.h>

void input_hw_init(void)
{
	/* Delay then repeat interval, in frames, for menu navigation. */
	hidSetRepeatParameters(20, 4);
}

void input_hw_read(F3DSRawInput *raw)
{
	hidScanInput();
	raw->down = hidKeysDown();
	raw->held = hidKeysHeld();
	raw->repeat = hidKeysDownRepeat();
	circlePosition cp, cs;
	hidCircleRead(&cp);
	hidCstickRead(&cs);
	raw->circleX = cp.dx;
	raw->circleY = cp.dy;
	raw->cstickX = cs.dx;
	raw->cstickY = cs.dy;
}
