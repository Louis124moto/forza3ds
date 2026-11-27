#include "noise.h"
#include <math.h>
#include "rng.h"

static float grad(uint32_t h, float x, float y)
{
	switch (h >> 29) {
	case 0: return x + y;
	case 1: return -x + y;
	case 2: return x - y;
	case 3: return -x - y;
	case 4: return 1.41421356f * x;
	case 5: return -1.41421356f * x;
	case 6: return 1.41421356f * y;
	default: return -1.41421356f * y;
	}
}

static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

float noise2(uint32_t seed, float x, float y)
{
	const float fx0 = floorf(x), fy0 = floorf(y);
	const int ix = (int)fx0, iy = (int)fy0;
	const float fx = x - fx0, fy = y - fy0;
	const float a = grad(hash_2d(seed, ix, iy), fx, fy);
	const float b = grad(hash_2d(seed, ix + 1, iy), fx - 1.0f, fy);
	const float c = grad(hash_2d(seed, ix, iy + 1), fx, fy - 1.0f);
	const float d = grad(hash_2d(seed, ix + 1, iy + 1), fx - 1.0f, fy - 1.0f);
	const float u = fade(fx), v = fade(fy);
	const float top = a + (b - a) * u, bottom = c + (d - c) * u;
	return top + (bottom - top) * v;
}

float noise_fbm(uint32_t seed, float x, float y, int octaves)
{
	float sum = 0.0f, amp = 1.0f, norm = 0.0f;
	for (int i = 0; i < octaves; ++i) {
		sum += amp * noise2(seed + (uint32_t)i * 1013u, x, y);
		norm += amp;
		amp *= 0.5f;
		/* Slight rotation hides the lattice. */
		const float nx = x * 1.6f - y * 1.2f, ny = x * 1.2f + y * 1.6f;
		x = nx + 17.3f;
		y = ny + 9.1f;
	}
	return norm > 0.0f ? sum / norm : 0.0f;
}

float noise_ridged(uint32_t seed, float x, float y, int octaves)
{
	float sum = 0.0f, amp = 1.0f, norm = 0.0f, weight = 1.0f;
	for (int i = 0; i < octaves; ++i) {
		float r = 1.0f - fabsf(noise2(seed + (uint32_t)i * 2029u, x, y));
		r *= r;
		sum += amp * r * weight;
		norm += amp;
		weight = r > 1.0f ? 1.0f : r;
		amp *= 0.5f;
		const float nx = x * 1.6f - y * 1.2f, ny = x * 1.2f + y * 1.6f;
		x = nx + 31.7f;
		y = ny + 5.3f;
	}
	return norm > 0.0f ? sum / norm : 0.0f;
}
