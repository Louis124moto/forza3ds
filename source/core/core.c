#include "core.h"

/*
 * libctru exposes svcGetSystemTick but no tick-frequency service on this
 * version. The ARM11 high-resolution timer runs at a fixed 268 MHz
 * (268111856.32 Hz, see the PICA hardware documentation). We use that
 * documented constant to convert ticks to seconds.
 */
#define F3DS_TICK_FREQ 268111856ull

void core_loop_init(F3DSLoopState *st, float dt, float maxCatchUp)
{
	st->dt = dt;
	st->maxCatchUp = maxCatchUp;
	st->simTime = 0.0f;
	st->frameTime = 0.0f;
	st->lastTick = svcGetSystemTick();
	st->fps = 0;
	st->fpsAcc = 0.0f;
	st->frameCount = 0;
}

int core_loop_begin_frame(F3DSLoopState *st)
{
	const u64 now = svcGetSystemTick();
	const float elapsed = (float)((double)(now - st->lastTick) / (double)F3DS_TICK_FREQ);
	st->lastTick = now;

	float frameDt = elapsed;
	st->frameTime = frameDt;
	st->fpsAcc += frameDt;
	st->frameCount++;
	if (st->fpsAcc >= 1.0f) {
		st->fps = st->frameCount;
		st->frameCount = 0;
		st->fpsAcc -= 1.0f;
	}

	if (frameDt > 1.0f)
		frameDt = 1.0f; /* long stall: reset the accumulator */

	float acc = frameDt;
	int steps = 0;
	const int maxSteps = (int)(st->maxCatchUp / st->dt);
	while (acc >= st->dt && steps < maxSteps) {
		acc -= st->dt;
		steps++;
		st->simTime += st->dt;
	}
	(void)acc;
	return steps;
}

void core_loop_end_frame(F3DSLoopState *st)
{
	(void)st;
}
