#ifndef F3DS_CORE_H
#define F3DS_CORE_H

#include "common.h"

/* Fixed-step accumulator with bounded catch-up, plus a presented-FPS meter. */
typedef struct {
	float dt;
	float maxCatchUp;
	float accumulator;
	float simTime;
	float frameTime;
	int fps;
	float fpsAcc;
	int frameCount;
} F3DSLoopState;

void core_loop_init(F3DSLoopState *st, float dt, float maxCatchUp);
/* Adds one rendered frame of "elapsed" seconds; returns simulation steps due. */
int core_loop_advance(F3DSLoopState *st, float elapsed);
/* Fraction of a step left over, for render interpolation. */
float core_loop_alpha(const F3DSLoopState *st);

#endif
