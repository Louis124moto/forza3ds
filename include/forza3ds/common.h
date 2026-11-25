#ifndef F3DS_COMMON_H
#define F3DS_COMMON_H

#include <3ds.h>
#include <math.h>

#define F3DS_PI 3.14159265358979323846f

typedef struct { float x, y, z; } FVec3;

static inline FVec3 fvec3(float x, float y, float z)
{
	FVec3 v = { x, y, z };
	return v;
}

static inline FVec3 fvec3_add(FVec3 a, FVec3 b) { return fvec3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline FVec3 fvec3_sub(FVec3 a, FVec3 b) { return fvec3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline FVec3 fvec3_scale(FVec3 a, float s) { return fvec3(a.x * s, a.y * s, a.z * s); }
static inline float fvec3_dot(FVec3 a, FVec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float fvec3_len(FVec3 a) { return sqrtf(fvec3_dot(a, a)); }

static inline float clampf(float v, float lo, float hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static inline float approachf(float current, float target, float maxDelta)
{
	if (current < target) return clampf(current + maxDelta, current, target);
	return clampf(current - maxDelta, target, current);
}

#endif
