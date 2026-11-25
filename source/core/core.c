#include "core.h"

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
	const u64 freq = svcGetSystemTickFrequency();
	const u64 now = svcGetSystemTick();
	const float elapsed = (float)(now - st->lastTick) / (float)freq;
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
