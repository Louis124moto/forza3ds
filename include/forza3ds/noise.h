/* Forza3DS - 2D gradient noise used by the world generator and textures. */
#ifndef F3DS_NOISE_H
#define F3DS_NOISE_H

#include <stdint.h>

/* About -1..1, smooth, zero at integer coordinates. */
float noise2(uint32_t seed, float x, float y);
/* Sum of octaves, normalised to about -1..1. */
float noise_fbm(uint32_t seed, float x, float y, int octaves);
/* Sharp crests, 0..1. */
float noise_ridged(uint32_t seed, float x, float y, int octaves);

#endif
