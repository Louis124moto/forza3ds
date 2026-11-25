#ifndef F3DS_CORE_H
#define F3DS_CORE_H

#include "common.h"

typedef struct {
	float dt;
	float maxCatchUp;
	float simTime;
	float frameTime;
	u64 lastTick;
	int fps;
	float fpsAcc;
	int frameCount;
} F3DSLoopState;

void core_loop_init(F3DSLoopState *st, float dt, float maxCatchUp);
int core_loop_begin_frame(F3DSLoopState *st);
void core_loop_end_frame(F3DSLoopState *st);

#endif
