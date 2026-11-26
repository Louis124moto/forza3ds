/* Forza3DS - runtime-generated procedural textures (license-free).
 * Every texture is authored in C at startup: the game ships no binary
 * assets, and each generated image keeps its own deterministic pattern. */
#ifndef F3DS_TEXTURES_H
#define F3DS_TEXTURES_H

#include <3ds.h>
#include <citro3d.h>

typedef enum {
	F3DS_TEX_WHITE = 0,
	F3DS_TEX_GRASS,
	F3DS_TEX_SAND,
	F3DS_TEX_ASPHALT,
	F3DS_TEX_DIRT,
	F3DS_TEX_GRAVEL,
	F3DS_TEX_WATER,
	F3DS_TEX_COUNT
} F3DSTexId;

/* Build every texture; returns false when at least one allocation failed. */
bool textures_init(void);
void textures_exit(void);

/* Returns NULL when the texture is unavailable. */
C3D_Tex *textures_get(F3DSTexId id);

#endif
