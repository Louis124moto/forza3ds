/* Forza3DS - small deterministic random numbers (identical on every target). */
#ifndef F3DS_RNG_H
#define F3DS_RNG_H

#include <stdint.h>

static inline uint32_t hash_u32(uint32_t v)
{
	v ^= v >> 16;
	v *= 0x7FEB352Du;
	v ^= v >> 15;
	v *= 0x846CA68Bu;
	return v ^ (v >> 16);
}

/* Independent sub-seed for a named purpose. */
static inline uint32_t hash_seed(uint32_t seed, uint32_t salt)
{
	return hash_u32(seed ^ hash_u32(salt + 0x9E3779B9u));
}

static inline uint32_t hash_2d(uint32_t seed, int x, int y)
{
	return hash_u32(seed ^ (hash_u32((uint32_t)x * 0x27D4EB2Du) + (uint32_t)y * 0x165667B1u));
}

typedef struct { uint32_t state; } F3DSRng;

static inline F3DSRng rng_make(uint32_t seed)
{
	F3DSRng r = { hash_u32(seed ^ 0xA511E9B3u) };
	return r;
}

static inline uint32_t rng_next(F3DSRng *r)
{
	r->state += 0x6D2B79F5u;
	return hash_u32(r->state);
}

/* [0, 1) */
static inline float rng_float(F3DSRng *r) { return (float)(rng_next(r) >> 8) * (1.0f / 16777216.0f); }

static inline float rng_range(F3DSRng *r, float lo, float hi) { return lo + (hi - lo) * rng_float(r); }

/* [0, n) */
static inline int rng_int(F3DSRng *r, int n) { return n > 0 ? (int)(rng_next(r) % (uint32_t)n) : 0; }

#endif
