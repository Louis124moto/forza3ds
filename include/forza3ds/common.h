#ifndef F3DS_COMMON_H
#define F3DS_COMMON_H

#include <3ds.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define F3DS_PI 3.14159265358979323846f
#define F3DS_GRAVITY 9.81f

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

/* World axes (right-handed, +Y up). Yaw 0 faces +Z; increasing yaw turns LEFT
 * as seen by the driver. See docs/COORDINATE_CONVENTIONS.md. */
static inline FVec3 yaw_forward(float yaw) { return fvec3(sinf(yaw), 0.0f, cosf(yaw)); }
static inline FVec3 yaw_left(float yaw) { return fvec3(cosf(yaw), 0.0f, -sinf(yaw)); }

static inline float clampf(float v, float lo, float hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

static inline float smoothstepf(float t)
{
	t = clampf(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

static inline float approachf(float current, float target, float maxDelta)
{
	if (current < target) return clampf(current + maxDelta, current, target);
	return clampf(current - maxDelta, target, current);
}

/* Wrap an angle to [-pi, pi]. */
static inline float wrap_angle(float a)
{
	if (!isfinite(a)) return 0.0f;
	a = fmodf(a + F3DS_PI, 2.0f * F3DS_PI);
	if (a < 0.0f) a += 2.0f * F3DS_PI;
	return a - F3DS_PI;
}

#endif
