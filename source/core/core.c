#include "core.h"

#define F3DS_TICK_FREQ 268111856ull

void core_loop_init(F3DSLoopState *st, float dt, float maxCatchUp)
{
	if (!st) return;
	if (!isfinite(dt) || dt <= 0.0f) dt = 1.0f / 60.0f;
	if (!isfinite(maxCatchUp) || maxCatchUp < dt) maxCatchUp = dt;
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

int core_loop_advance(F3DSLoopState *st, float elapsed)
{
	if (!st || !isfinite(elapsed) || elapsed < 0.0f || !isfinite(st->dt) ||
	    st->dt <= 0.0f || !isfinite(st->maxCatchUp) || st->maxCatchUp < st->dt)
		return 0;
	st->frameTime = elapsed;
	st->fpsAcc += elapsed;
	st->frameCount++;
	if (st->fpsAcc >= 1.0f) {
		st->fps = st->frameCount;
		st->frameCount = 0;
		st->fpsAcc = fmodf(st->fpsAcc, 1.0f);
	}
	st->accumulator += elapsed;
	if (st->accumulator > st->maxCatchUp) st->accumulator = st->maxCatchUp;
	const int maxSteps = (int)(st->maxCatchUp / st->dt);
	int steps = 0;
	while (st->accumulator + 1e-7f >= st->dt && steps < maxSteps) {
		st->accumulator -= st->dt;
		if (st->accumulator < 0.0f) st->accumulator = 0.0f;
		st->simTime += st->dt;
		steps++;
	}
	return steps;
}

int core_loop_begin_frame(F3DSLoopState *st)
{
	if (!st) return 0;
	const u64 now = svcGetSystemTick();
	const float elapsed = (float)((double)(now - st->lastTick) / (double)F3DS_TICK_FREQ);
	st->lastTick = now;
	return core_loop_advance(st, elapsed);
}

void core_loop_end_frame(F3DSLoopState *st) { (void)st; }
