#include "core.h"

/* libctru system ticks run at 268111856 Hz on the 3DS. */
#define F3DS_TICK_FREQ 268111856ull

void core_loop_init(F3DSLoopState *st, float dt, float maxCatchUp)
{
	st->dt = dt;
	st->maxCatchUp = maxCatchUp;
	st->accumulator = 0.0f;
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

	st->frameTime = elapsed;
	st->fpsAcc += elapsed;
	st->frameCount++;
	if (st->fpsAcc >= 1.0f) {
		st->fps = st->frameCount;
		st->frameCount = 0;
		st->fpsAcc -= 1.0f;
	}

	/* Preserve fractional frame time between calls. A long stall is bounded
	 * and excess whole simulation steps are discarded after max catch-up. */
		st->accumulator += elapsed > 1.0f ? 1.0f : elapsed;
	const int maxSteps = st->dt > 0.0f ? (int)(st->maxCatchUp / st->dt) : 0;
	int steps = 0;
	while (st->dt > 0.0f && st->accumulator >= st->dt && steps < maxSteps) {
		st->accumulator -= st->dt;
		st->simTime += st->dt;
		steps++;
	}
	if (steps == maxSteps && st->dt > 0.0f && st->accumulator >= st->dt)
		st->accumulator = fmodf(st->accumulator, st->dt);
	return steps;
}

void core_loop_end_frame(F3DSLoopState *st)
{
	(void)st;
}
