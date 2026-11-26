#include "textures.h"
#include <math.h>

#define TEX_SIZE 128

static C3D_Tex s_textures[F3DS_TEX_COUNT];
static bool s_ready[F3DS_TEX_COUNT];

static u32 hash_grid(u32 ix, u32 iy, u32 seed)
{
	u32 h = ix * 374761393u + iy * 668265263u + seed * 1274126177u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h ^= h >> 16;
	return h & 0xFFFFu;
}

static float fade(float t)
{
	return t * t * (3.0f - 2.0f * t);
}

static float value_noise(float x, float y, u32 seed)
{
	const int ix = (int)floorf(x), iy = (int)floorf(y);
	const float fx = fade(x - (float)ix), fy = fade(y - (float)iy);
	const float a = (float)hash_grid((u32)ix, (u32)iy, seed) / 65535.0f;
	const float b = (float)hash_grid((u32)ix + 1u, (u32)iy, seed) / 65535.0f;
	const float c = (float)hash_grid((u32)ix, (u32)iy + 1u, seed) / 65535.0f;
	const float d = (float)hash_grid((u32)ix + 1u, (u32)iy + 1u, seed) / 65535.0f;
	return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

/* 3DS textures are stored as 8x8 tiles; inside a tile, pixel (px,py) sits at
 * the Morton interleave of px and py (verified against tex3ds swizzle.cpp). */
static u32 morton_xy(u32 x, u32 y)
{
	u32 r = 0;
	for (u32 i = 0; i < 3; ++i) {
		r |= ((x >> i) & 1u) << (2 * i);
		r |= ((y >> i) & 1u) << (2 * i + 1);
	}
	return r;
}

/* GPU_RGBA8 stores one pixel as the R,G,B,A byte sequence. */
static void put_texel(C3D_Tex *tex, int x, int y, u8 r, u8 g, u8 b, u8 a)
{
	const u32 tile = (u32)(y >> 3) * (TEX_SIZE >> 3) + (u32)(x >> 3);
	u8 *dst = (u8 *)tex->data + (tile * 64u + morton_xy((u32)(x & 7), (u32)(y & 7))) * 4u;
	dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = a;
}

static u8 to_u8(float v)
{
	if (v <= 0.0f) return 0;
	if (v >= 255.0f) return 255;
	return (u8)v;
}

static bool texture_fill(F3DSTexId id, u32 seed)
{
	C3D_Tex *tex = &s_textures[id];
	if (!C3D_TexInit(tex, TEX_SIZE, TEX_SIZE, GPU_RGBA8))
		return false;
	C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
	C3D_TexSetWrap(tex, GPU_REPEAT, GPU_REPEAT);

	for (int y = 0; y < TEX_SIZE; ++y) {
		for (int x = 0; x < TEX_SIZE; ++x) {
			const float fx = (float)x, fy = (float)y;
			float r = 255.0f, g = 255.0f, b = 255.0f;
			switch (id) {
			case F3DS_TEX_WHITE:
				break;
			case F3DS_TEX_GRASS: {
				const float n = 0.5f * value_noise(fx * 0.10f, fy * 0.10f, seed)
					+ 0.35f * value_noise(fx * 0.38f, fy * 0.38f, seed + 11u)
					+ 0.15f * value_noise(fx * 1.40f, fy * 1.40f, seed + 23u);
				const float lum = 0.74f + 0.52f * n;
				r = 74.0f * lum; g = 132.0f * lum; b = 58.0f * lum;
				if (hash_grid((u32)x, (u32)y, seed + 31u) > 61000u) {
					r *= 1.25f; g *= 1.25f; b *= 1.20f;
				}
				break;
			}
			case F3DS_TEX_SAND: {
				const float n = value_noise(fx * 0.22f, fy * 0.22f, seed);
				const float lum = 0.88f + 0.24f * n;
				r = 208.0f * lum; g = 186.0f * lum; b = 132.0f * lum;
				if (hash_grid((u32)x, (u32)y, seed + 7u) > 63500u) {
					r *= 0.82f; g *= 0.82f; b *= 0.85f;
				}
				break;
			}
			case F3DS_TEX_ASPHALT: {
				const float fine = value_noise(fx * 0.55f, fy * 0.55f, seed);
				const float patch = value_noise(fx * 0.06f, fy * 0.06f, seed + 5u);
				const float lum = 0.78f + 0.30f * fine + 0.22f * (patch - 0.5f);
				r = 96.0f * lum; g = 96.0f * lum; b = 104.0f * lum;
				break;
			}
			case F3DS_TEX_DIRT: {
				const float n = 0.6f * value_noise(fx * 0.18f, fy * 0.18f, seed)
					+ 0.4f * value_noise(fx * 0.90f, fy * 0.90f, seed + 13u);
				const float lum = 0.76f + 0.48f * n;
				r = 172.0f * lum; g = 112.0f * lum; b = 54.0f * lum;
				if (hash_grid((u32)x, (u32)y, seed + 17u) > 62500u) {
					r *= 0.70f; g *= 0.70f; b *= 0.70f;
				}
				break;
			}
			case F3DS_TEX_GRAVEL: {
				const float q = (float)hash_grid((u32)x, (u32)y, seed) / 65535.0f;
				const float n = value_noise(fx * 0.30f, fy * 0.30f, seed + 3u);
				const float lum = 0.62f + 0.28f * q + 0.30f * n;
				r = 138.0f * lum; g = 124.0f * lum; b = 102.0f * lum;
				break;
			}
			case F3DS_TEX_WATER: {
				const float warp = value_noise(fx * 0.05f, fy * 0.05f, seed);
				const float wave = sinf((fy + 26.0f * warp) * 0.42f);
				const float n = value_noise(fx * 0.50f, fy * 0.50f, seed + 9u);
				const float lum = 0.90f + 0.10f * wave + 0.14f * (n - 0.5f);
				r = 34.0f * lum; g = 98.0f * lum; b = 158.0f * lum;
				break;
			}
			default:
				break;
			}
			put_texel(tex, x, y, to_u8(r), to_u8(g), to_u8(b), 255);
		}
	}
	C3D_TexFlush(tex);
	return true;
}

bool textures_init(void)
{
	static const u32 seeds[F3DS_TEX_COUNT] = { 1u, 101u, 211u, 307u, 401u, 509u, 601u };
	bool ok = true;
	for (int i = 0; i < F3DS_TEX_COUNT; ++i) {
		s_ready[i] = texture_fill((F3DSTexId)i, seeds[i]);
		if (!s_ready[i]) ok = false;
	}
	return ok;
}

void textures_exit(void)
{
	for (int i = 0; i < F3DS_TEX_COUNT; ++i) {
		if (s_ready[i]) {
			C3D_TexDelete(&s_textures[i]);
			s_ready[i] = false;
		}
	}
}

C3D_Tex *textures_get(F3DSTexId id)
{
	return s_ready[id] ? &s_textures[id] : NULL;
}
