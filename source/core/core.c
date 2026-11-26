#include "core.h"

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
	st->fps = 0;
	st->fpsAcc = 0.0f;
	st->frameCount = 0;
}

int core_loop_advance(F3DSLoopState *st, float elapsed)
{
	if (!st || !isfinite(elapsed) || elapsed < 0.0f || !isfinite(st->dt) || st->dt <= 0.0f ||
	    !isfinite(st->maxCatchUp) || st->maxCatchUp < st->dt)
		return 0;
	st->frameTime = elapsed;
	st->fpsAcc += elapsed;
	st->frameCount++;
	if (st->fpsAcc >= 1.0f) {
		st->fps = (int)lroundf((float)st->frameCount / st->fpsAcc);
		st->frameCount = 0;
		st->fpsAcc = 0.0f;
	}
	st->accumulator += elapsed;
	if (st->accumulator > st->maxCatchUp)
		st->accumulator = st->maxCatchUp;
	/* The display runs at 59.83 Hz; snap tiny timing jitter to a whole step. */
	const float slack = st->dt * 0.02f;
	const int maxSteps = (int)(st->maxCatchUp / st->dt + 0.5f);
	int steps = 0;
	while (st->accumulator + slack >= st->dt && steps < maxSteps) {
		st->accumulator -= st->dt;
		if (st->accumulator < 0.0f) st->accumulator = 0.0f;
		st->simTime += st->dt;
		steps++;
	}
	return steps;
}

float core_loop_alpha(const F3DSLoopState *st)
{
	return st && st->dt > 0.0f ? clampf(st->accumulator / st->dt, 0.0f, 1.0f) : 0.0f;
}
