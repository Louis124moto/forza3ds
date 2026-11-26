/* Forza3DS - Citro3D renderer for the top screen (3DS only). */
#ifndef F3DS_RENDERER_H
#define F3DS_RENDERER_H

#include "app.h"

typedef struct {
	float cpuMs, gpuMs;      /* last frame, from citro3d */
	int draws;
	uint32_t vertices;
	float cmdBufUsage;       /* 0..1 */
} F3DSRenderStats;

/* Builds all geometry. Returns false on allocation or GPU failure. */
bool renderer_init(const F3DSTrack *track, const F3DSTerrain *terrain);
void renderer_exit(void);
/* Waits for the frame slot of the chosen limit (30 or 60) and draws. */
void renderer_render(const F3DSView *view, int fpsLimit);
void renderer_stats(F3DSRenderStats *out);

#endif
