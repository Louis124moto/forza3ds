#include "meshes.h"
#include <math.h>
#include <string.h>

#define MESH_MAX_VERTS 60000

static unsigned int map_rng(unsigned int *state)
{
	*state = *state * 1664525u + 1013904223u;
	return *state;
}

typedef struct {
	F3DSVertex *v;
	uint32_t n, cap;
} Builder;

static void push(Builder *b, FVec3 p, float r, float g, float bl, float a, float u, float v)
{
	if (b->n >= b->cap)
		return;
	F3DSVertex *out = &b->v[b->n++];
	out->x = p.x; out->y = p.y; out->z = p.z;
	out->r = r; out->g = g; out->b = bl; out->a = a;
	out->u = u; out->v = v;
}

static FVec3 at_y(FVec3 p, float y)
{
	p.y = y;
	return p;
}

static void push_tri_uv(Builder *b, FVec3 p0, FVec3 p1, FVec3 p2,
			float u0, float v0, float u1, float v1, float u2, float v2,
			float r, float g, float bl)
{
	push(b, p0, r, g, bl, 1.0f, u0, v0);
	push(b, p1, r, g, bl, 1.0f, u1, v1);
	push(b, p2, r, g, bl, 1.0f, u2, v2);
}

static void push_quad_uv(Builder *b, FVec3 p0, FVec3 p1, FVec3 p2, FVec3 p3,
			 float u0, float v0, float u1, float v1,
			 float u2, float v2, float u3, float v3,
			 float r, float g, float bl)
{
	push_tri_uv(b, p0, p1, p2, u0, v0, u1, v1, u2, v2, r, g, bl);
	push_tri_uv(b, p0, p2, p3, u0, v0, u2, v2, u3, v3, r, g, bl);
}

/* Untextured helpers: uv = (0,0) samples the plain white texture so the
 * per-vertex color passes through the MODULATE stage unchanged. */
static void push_tri(Builder *b, FVec3 p0, FVec3 p1, FVec3 p2, float r, float g, float bl)
{
	push_tri_uv(b, p0, p1, p2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, bl);
}

static void push_quad(Builder *b, FVec3 p0, FVec3 p1, FVec3 p2, FVec3 p3, float r, float g, float bl)
{
	push_quad_uv(b, p0, p1, p2, p3, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, bl);
}

static void push_box(Builder *b, float x0, float y0, float z0, float x1, float y1, float z1,
		     float r, float g, float bl)
{
	const float shade = 0.8f;
	push_quad(b, fvec3(x0,y1,z0), fvec3(x1,y1,z0), fvec3(x1,y1,z1), fvec3(x0,y1,z1), r, g, bl);
	push_quad(b, fvec3(x0,y0,z0), fvec3(x0,y0,z1), fvec3(x1,y0,z1), fvec3(x1,y0,z0), r*shade, g*shade, bl*shade);
	push_quad(b, fvec3(x0,y0,z1), fvec3(x1,y0,z1), fvec3(x1,y1,z1), fvec3(x0,y1,z1), r*shade, g*shade, bl*shade);
	push_quad(b, fvec3(x1,y0,z0), fvec3(x0,y0,z0), fvec3(x0,y1,z0), fvec3(x1,y1,z0), r*shade, g*shade, bl*shade);
	push_quad(b, fvec3(x1,y0,z1), fvec3(x1,y0,z0), fvec3(x1,y1,z0), fvec3(x1,y1,z1), r*shade, g*shade, bl*shade);
	push_quad(b, fvec3(x0,y0,z0), fvec3(x0,y0,z1), fvec3(x0,y1,z1), fvec3(x0,y1,z0), r*shade, g*shade, bl*shade);
}

/* Oriented slab (five faces, no bottom) aligned on (right, fwd) ground axes. */
static void push_slab(Builder *b, FVec3 c, FVec3 right, FVec3 fwd,
		      float hw, float hl, float y0, float y1, float r, float g, float bl)
{
	const float shade = 0.8f;
	const FVec3 xr = fvec3_scale(right, hw);
	const FVec3 zf = fvec3_scale(fwd, hl);
	const FVec3 p0 = fvec3_add(c, fvec3_add(fvec3_scale(xr, -1.0f), fvec3_scale(zf, -1.0f)));
	const FVec3 p1 = fvec3_add(c, fvec3_add(xr, fvec3_scale(zf, -1.0f)));
	const FVec3 p2 = fvec3_add(c, fvec3_add(xr, zf));
	const FVec3 p3 = fvec3_add(c, fvec3_add(fvec3_scale(xr, -1.0f), zf));
	push_quad(b, at_y(p0, y1), at_y(p1, y1), at_y(p2, y1), at_y(p3, y1), r, g, bl);
	push_quad(b, at_y(p0, y0), at_y(p1, y0), at_y(p1, y1), at_y(p0, y1), r*shade, g*shade, bl*shade);
	push_quad(b, at_y(p3, y0), at_y(p2, y0), at_y(p2, y1), at_y(p3, y1), r*shade, g*shade, bl*shade);
	push_quad(b, at_y(p0, y0), at_y(p3, y0), at_y(p3, y1), at_y(p0, y1), r*shade, g*shade, bl*shade);
	push_quad(b, at_y(p1, y0), at_y(p2, y0), at_y(p2, y1), at_y(p1, y1), r*shade, g*shade, bl*shade);
}

/* Squared distance from a point to the nearest track centerline point. */
static float track_dist2(const F3DSTrack *track, float x, float z)
{
	float best = 1e30f;
	for (int i = 0; i < TRACK_POINTS; i++) {
		const float dx = track->pts[i].x - x;
		const float dz = track->pts[i].z - z;
		const float d2 = dx * dx + dz * dz;
		if (d2 < best)
			best = d2;
	}
	return best;
}

void meshes_build(F3DSMeshes *m, const F3DSTrack *track, const F3DSTerrain *terrain)
{
	const uint32_t cap = MESH_MAX_VERTS;
	Builder b;
	b.v = (F3DSVertex *)linearAlloc(cap * sizeof(F3DSVertex));
	b.n = 0;
	b.cap = cap;
	if (!b.v) {
		memset(m, 0, sizeof(*m));
		return;
	}

	/* --- Ocean plane (repeating water texture, ~8 m per tile). --- */
	m->oceanStart = 0;
	push_quad_uv(&b,
		     fvec3(-520, -1.2f, -520), fvec3(-520, -1.2f, 520),
		     fvec3(520, -1.2f, 520), fvec3(520, -1.2f, -520),
		     0.0f, 0.0f, 0.0f, 130.0f, 130.0f, 130.0f, 130.0f, 0.0f,
		     0.50f, 0.85f, 0.90f);
	m->oceanCount = b.n - m->oceanStart;

	/* --- Island: grass interior, then a sand ring along the shoreline. --- */
	const int grid = 32;
	const float cell = 560.0f / (float)grid;
	m->islandStart = b.n;
	for (int iz = 0; iz < grid; iz++) {
		for (int ix = 0; ix < grid; ix++) {
			const float x0 = -280.0f + (float)ix * cell;
			const float z0 = -280.0f + (float)iz * cell;
			const float x1 = x0 + cell, z1 = z0 + cell;
			const float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
			const float edge = 250.0f + 13.0f * sinf(cx * 0.018f) + 10.0f * cosf(cz * 0.021f);
			const float d2 = cx * cx + cz * cz;
			if (d2 > edge * edge) continue;
			if (d2 > (edge - 18.0f) * (edge - 18.0f)) continue; /* beach ring below */
			const float green = 0.65f + 0.10f * sinf(cx * 0.04f + cz * 0.02f);
			push_quad_uv(&b,
				     fvec3(x0, -0.05f, z0), fvec3(x0, -0.05f, z1),
				     fvec3(x1, -0.05f, z1), fvec3(x1, -0.05f, z0),
				     x0 / 6.0f, z0 / 6.0f, x0 / 6.0f, z1 / 6.0f,
				     x1 / 6.0f, z1 / 6.0f, x1 / 6.0f, z0 / 6.0f,
				     0.45f, green, 0.55f);
		}
	}
	m->islandCount = b.n - m->islandStart;

	m->beachStart = b.n;
	for (int iz = 0; iz < grid; iz++) {
		for (int ix = 0; ix < grid; ix++) {
			const float x0 = -280.0f + (float)ix * cell;
			const float z0 = -280.0f + (float)iz * cell;
			const float x1 = x0 + cell, z1 = z0 + cell;
			const float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
			const float edge = 250.0f + 13.0f * sinf(cx * 0.018f) + 10.0f * cosf(cz * 0.021f);
			const float d2 = cx * cx + cz * cz;
			if (d2 <= (edge - 18.0f) * (edge - 18.0f)) continue;
			if (d2 > edge * edge) continue;
			push_quad_uv(&b,
				     fvec3(x0, -0.05f, z0), fvec3(x0, -0.05f, z1),
				     fvec3(x1, -0.05f, z1), fvec3(x1, -0.05f, z0),
				     x0 / 4.0f, z0 / 4.0f, x0 / 4.0f, z1 / 4.0f,
				     x1 / 4.0f, z1 / 4.0f, x1 / 4.0f, z0 / 4.0f,
				     0.83f, 0.77f, 0.65f);
		}
	}
	m->beachCount = b.n - m->beachStart;

	/* --- Forests, rocks and skyline peaks (untextured, vertex colors). --- */
	m->forestStart = b.n;
	unsigned int seed = 0xF03A3D5u;
	for (int i = 0; i < 76; i++) {
		const float a = (float)(map_rng(&seed) & 0xFFFFu) * (2.0f * F3DS_PI / 65535.0f);
		const float radius = 188.0f + (float)(map_rng(&seed) & 0xFFFFu) * (48.0f / 65535.0f);
		const float x = cosf(a) * radius;
		const float z = sinf(a) * radius;
		const float h = 2.8f + (float)(map_rng(&seed) & 0xFFFFu) * (2.2f / 65535.0f);
		const float crown = 1.4f + (float)(map_rng(&seed) & 0xFFFFu) * (0.9f / 65535.0f);
		push_box(&b, x - 0.22f, 0.0f, z - 0.22f, x + 0.22f, h, z + 0.22f, 0.30f, 0.17f, 0.08f);
		push_box(&b, x - crown, h - 0.3f, z - crown, x + crown, h + crown * 1.4f, z + crown, 0.08f, 0.34f, 0.13f);
	}

	/* Inner forest patches, kept clear of the road. */
	unsigned int seed2 = 0x51DE30Fu;
	int placedTrees = 0;
	for (int tries = 0; tries < 260 && placedTrees < 90; tries++) {
		const float a = (float)(map_rng(&seed2) & 0xFFFFu) * (2.0f * F3DS_PI / 65535.0f);
		const float radius = 58.0f + (float)(map_rng(&seed2) & 0xFFFFu) * (150.0f / 65535.0f);
		const float x = cosf(a) * radius;
		const float z = sinf(a) * radius;
		if (track_dist2(track, x, z) < 196.0f)
			continue;
		const float h = 2.6f + (float)(map_rng(&seed2) & 0xFFFFu) * (2.4f / 65535.0f);
		const float crown = 1.3f + (float)(map_rng(&seed2) & 0xFFFFu) * (1.0f / 65535.0f);
		push_box(&b, x - 0.22f, 0.0f, z - 0.22f, x + 0.22f, h, z + 0.22f, 0.30f, 0.17f, 0.08f);
		push_box(&b, x - crown, h - 0.3f, z - crown, x + crown, h + crown * 1.4f, z + crown, 0.08f, 0.34f, 0.13f);
		placedTrees++;
	}

	/* Scattered coastal rocks. */
	unsigned int seed3 = 0x0C0CAC0u;
	int placedRocks = 0;
	for (int tries = 0; tries < 200 && placedRocks < 26; tries++) {
		const float a = (float)(map_rng(&seed3) & 0xFFFFu) * (2.0f * F3DS_PI / 65535.0f);
		const float radius = 200.0f + (float)(map_rng(&seed3) & 0xFFFFu) * (40.0f / 65535.0f);
		const float x = cosf(a) * radius;
		const float z = sinf(a) * radius;
		if (track_dist2(track, x, z) < 100.0f)
			continue;
		const float sx = 1.0f + (float)(map_rng(&seed3) & 0xFFFFu) * (1.8f / 65535.0f);
		const float sy = 0.8f + (float)(map_rng(&seed3) & 0xFFFFu) * (1.2f / 65535.0f);
		const float sz = 1.0f + (float)(map_rng(&seed3) & 0xFFFFu) * (1.8f / 65535.0f);
		const float tone = 0.42f + (float)(map_rng(&seed3) & 0xFFFFu) * (0.12f / 65535.0f);
		push_box(&b, x - sx, 0.0f, z - sz, x + sx, sy, z + sz, tone, tone, tone * 0.95f);
		placedRocks++;
	}

	/* Distant rocky peaks frame the northern and eastern skyline. */
	for (int i = 0; i < 9; i++) {
		const float a = 0.35f + (float)i * 0.31f;
		const float x = cosf(a) * 255.0f;
		const float z = sinf(a) * 255.0f;
		const float h = 18.0f + (float)(i % 4) * 5.0f;
		const float radius = 17.0f + (float)(i % 3) * 5.0f;
		for (int side = 0; side < 8; side++) {
			const float a0 = (float)side * F3DS_PI / 4.0f;
			const float a1 = (float)(side + 1) * F3DS_PI / 4.0f;
			push_tri(&b, fvec3(x + cosf(a0) * radius, 0.0f, z + sinf(a0) * radius),
				 fvec3(x + cosf(a1) * radius, 0.0f, z + sinf(a1) * radius),
				 fvec3(x, h, z), 0.30f, 0.34f, 0.32f);
		}
	}
	m->forestCount = b.n - m->forestStart;

	/* --- Road: repeating asphalt, ~3 m along, ~3 m across. --- */
	m->roadStart = b.n;
	float arc = 0.0f;
	for (int i = 0; i < TRACK_POINTS; i++) {
		const float w = TRACK_HALF_WIDTH;
		const FVec3 r0 = track->right[i];
		const FVec3 r1 = track->right[i + 1];
		const FVec3 p0 = fvec3_add(track->pts[i], fvec3_scale(r0, -w));
		const FVec3 p1 = fvec3_add(track->pts[i], fvec3_scale(r0, w));
		const FVec3 p2 = fvec3_add(track->pts[i + 1], fvec3_scale(r1, w));
		const FVec3 p3 = fvec3_add(track->pts[i + 1], fvec3_scale(r1, -w));
		const float shade = (i & 1) ? 0.72f : 0.82f;
		const float u0 = arc / 3.0f;
		const float u1 = (arc + track->segLen[i]) / 3.0f;
		push_quad_uv(&b, p0, p1, p2, p3,
			     u0, 0.0f, u0, 4.0f, u1, 4.0f, u1, 0.0f,
			     shade, shade, shade + 0.02f);
		arc += track->segLen[i];
	}
	m->roadCount = b.n - m->roadStart;

	/* --- Road paint: kerbs, center dashes and start checker. --- */
	m->roadLinesStart = b.n;
	for (int i = 0; i < TRACK_POINTS; i += 2) {
		const float w = TRACK_HALF_WIDTH;
		const int red = (i / 2) & 1;
		const float cr = red ? 0.85f : 0.92f;
		const float cg = red ? 0.12f : 0.92f;
		const float cb = red ? 0.10f : 0.92f;
		for (int side = 0; side < 2; side++) {
			const float sgn = side ? 1.0f : -1.0f;
			const FVec3 in0 = fvec3_add(track->pts[i], fvec3_scale(track->right[i], sgn * (w - 0.8f)));
			const FVec3 out0 = fvec3_add(track->pts[i], fvec3_scale(track->right[i], sgn * w));
			const FVec3 in1 = fvec3_add(track->pts[i + 1], fvec3_scale(track->right[i + 1], sgn * (w - 0.8f)));
			const FVec3 out1 = fvec3_add(track->pts[i + 1], fvec3_scale(track->right[i + 1], sgn * w));
			push_quad(&b, at_y(in0, in0.y + 0.02f), at_y(out0, out0.y + 0.02f),
				  at_y(out1, out1.y + 0.02f), at_y(in1, in1.y + 0.02f), cr, cg, cb);
		}
	}
	for (int i = 0; i < TRACK_POINTS; i++) {
		if (i % 6 >= 2)
			continue;
		const FVec3 l0 = fvec3_add(track->pts[i], fvec3_scale(track->right[i], -0.15f));
		const FVec3 r0 = fvec3_add(track->pts[i], fvec3_scale(track->right[i], 0.15f));
		const FVec3 l1 = fvec3_add(track->pts[i + 1], fvec3_scale(track->right[i + 1], -0.15f));
		const FVec3 r1 = fvec3_add(track->pts[i + 1], fvec3_scale(track->right[i + 1], 0.15f));
		push_quad(&b, at_y(l0, l0.y + 0.02f), at_y(r0, r0.y + 0.02f),
			  at_y(r1, r1.y + 0.02f), at_y(l1, l1.y + 0.02f), 0.92f, 0.92f, 0.90f);
	}
	for (int row = 0; row < 2; row++) {
		const FVec3 pa = track->pts[row];
		const FVec3 pb = track->pts[row + 1];
		for (int cIdx = 0; cIdx < 10; cIdx++) {
			const float t0 = -TRACK_HALF_WIDTH + 1.2f * (float)cIdx;
			const float t1 = t0 + 1.2f;
			const FVec3 a0 = fvec3_add(pa, fvec3_scale(track->right[row], t0));
			const FVec3 a1 = fvec3_add(pa, fvec3_scale(track->right[row], t1));
			const FVec3 b1 = fvec3_add(pb, fvec3_scale(track->right[row + 1], t1));
			const FVec3 b0 = fvec3_add(pb, fvec3_scale(track->right[row + 1], t0));
			if ((cIdx + row) & 1)
				push_quad(&b, at_y(a0, a0.y + 0.03f), at_y(a1, a1.y + 0.03f),
					  at_y(b1, b1.y + 0.03f), at_y(b0, b0.y + 0.03f), 0.92f, 0.92f, 0.92f);
			else
				push_quad(&b, at_y(a0, a0.y + 0.03f), at_y(a1, a1.y + 0.03f),
					  at_y(b1, b1.y + 0.03f), at_y(b0, b0.y + 0.03f), 0.12f, 0.12f, 0.14f);
		}
	}
	m->roadLinesCount = b.n - m->roadLinesStart;

	/* --- Obstacles --- */
	m->obstacleStart = b.n;
	for (int i = 0; i < track->obstacleCount; i++) {
		const FVec3 c = track->obstacles[i].center;
		const float hh = track->obstacles[i].height;
		push_box(&b, c.x - track->obstacles[i].hx, c.y - hh, c.z - track->obstacles[i].hz,
			 c.x + track->obstacles[i].hx, c.y + hh, c.z + track->obstacles[i].hz,
			 0.85f, 0.55f, 0.12f);
	}
	m->obstacleCount = b.n - m->obstacleStart;

	/* --- Decor: barrier walls, marker posts, grandstand and village. --- */
	m->decorStart = b.n;
	for (int i = 0; i + 3 < TRACK_POINTS; i += 3) {
		const FVec3 r = track->right[i];
		const float side = (i % 6) ? 1.0f : -1.0f;
		const FVec3 a = fvec3_add(track->pts[i], fvec3_scale(r, side * (TRACK_HALF_WIDTH + 1.5f)));
		const FVec3 bpos = fvec3_add(track->pts[i + 3], fvec3_scale(track->right[i + 3], side * (TRACK_HALF_WIDTH + 1.5f)));
		const float red = ((i / 3) & 1) ? 0.85f : 0.9f;
		const float grn = ((i / 3) & 1) ? 0.15f : 0.9f;
		push_box(&b, fminf(a.x, bpos.x) - 0.3f, 0.0f, fminf(a.z, bpos.z) - 0.3f,
			 fmaxf(a.x, bpos.x) + 0.3f, 1.1f, fmaxf(a.z, bpos.z) + 0.3f,
			 red, grn, 0.15f);
	}
	for (int i = 0; i < TRACK_POINTS; i += 12) {
		const float w = TRACK_HALF_WIDTH;
		for (int side = 0; side < 2; side++) {
			const float sgn = side ? 1.0f : -1.0f;
			const FVec3 p = fvec3_add(track->pts[i], fvec3_scale(track->right[i], sgn * (w + 1.0f)));
			push_box(&b, p.x - 0.12f, p.y, p.z - 0.12f, p.x + 0.12f, p.y + 1.1f, p.z + 0.12f,
				 0.92f, 0.92f, 0.90f);
			push_box(&b, p.x - 0.18f, p.y + 1.1f, p.z - 0.18f, p.x + 0.18f, p.y + 1.35f, p.z + 0.18f,
				 0.85f, 0.10f, 0.10f);
		}
	}
	{
		/* Grandstand on the outside of the main straight. */
		const FVec3 rt = track->right[6];
		const FVec3 fw = track->fwd[6];
		const FVec3 base = fvec3_add(track->pts[6], fvec3_scale(rt, TRACK_HALF_WIDTH + 7.0f));
		push_slab(&b, base, rt, fw, 7.0f, 3.0f, 0.0f, 0.5f, 0.55f, 0.57f, 0.60f);
		push_slab(&b, fvec3_add(base, fvec3_scale(fw, -1.0f)), rt, fw, 7.0f, 2.0f, 0.5f, 1.4f, 0.75f, 0.30f, 0.25f);
		push_slab(&b, fvec3_add(base, fvec3_scale(fw, -2.0f)), rt, fw, 7.0f, 1.0f, 1.4f, 2.3f, 0.75f, 0.30f, 0.25f);
		push_slab(&b, fvec3_add(base, fvec3_scale(fw, -1.0f)), rt, fw, 7.6f, 3.4f, 3.4f, 3.6f, 0.16f, 0.18f, 0.22f);
		const FVec3 pil0 = fvec3_add(base, fvec3_scale(rt, 6.5f));
		const FVec3 pil1 = fvec3_add(base, fvec3_scale(rt, -6.5f));
		push_box(&b, pil0.x - 0.15f, 0.0f, pil0.z - 0.15f, pil0.x + 0.15f, 3.4f, pil0.z + 0.15f, 0.30f, 0.32f, 0.36f);
		push_box(&b, pil1.x - 0.15f, 0.0f, pil1.z - 0.15f, pil1.x + 0.15f, 3.4f, pil1.z + 0.15f, 0.30f, 0.32f, 0.36f);
	}
	{
		/* Small village on the far side of the track near pts[48]. */
		unsigned int seedV = 0x600D51u;
		const FVec3 rt = track->right[48];
		const FVec3 fw = track->fwd[48];
		static const float wallR[3] = { 0.86f, 0.78f, 0.88f };
		static const float wallG[3] = { 0.80f, 0.72f, 0.84f };
		static const float wallB[3] = { 0.68f, 0.62f, 0.74f };
		static const float roofR[3] = { 0.45f, 0.30f, 0.38f };
		static const float roofG[3] = { 0.16f, 0.20f, 0.22f };
		static const float roofB[3] = { 0.14f, 0.26f, 0.20f };
		for (int i = 0; i < 6; i++) {
			const float across = 18.0f + (float)(map_rng(&seedV) & 0xFFFFu) * (22.0f / 65535.0f);
			const float along = -18.0f + (float)(map_rng(&seedV) & 0xFFFFu) * (36.0f / 65535.0f);
			const float hw = 2.2f + (float)(map_rng(&seedV) & 0xFFFFu) * (1.6f / 65535.0f);
			const float hl = 2.6f + (float)(map_rng(&seedV) & 0xFFFFu) * (2.0f / 65535.0f);
			const int variant = (int)(map_rng(&seedV) & 0x3u) % 3;
			const FVec3 c = fvec3_add(track->pts[48],
					fvec3_add(fvec3_scale(rt, -across), fvec3_scale(fw, along)));
			push_slab(&b, c, rt, fw, hw, hl, 0.0f, 2.6f, wallR[variant], wallG[variant], wallB[variant]);
			push_slab(&b, c, rt, fw, hw * 0.85f, hl * 0.85f, 2.6f, 3.1f, roofR[variant], roofG[variant], roofB[variant]);
			push_slab(&b, c, rt, fw, hw * 0.55f, hl * 0.55f, 3.1f, 3.5f, roofR[variant], roofG[variant], roofB[variant]);
		}
	}
	m->decorCount = b.n - m->decorStart;

	/* --- Terrain features: dirt ramps + gravel slick zones --- */
	m->terrainStart = b.n;
	for (int ri = 0; ri < terrain->rampCount; ri++) {
		const F3DSTerrainRamp *r = &terrain->ramps[ri];
		const int steps = 8;
		const float half = r->width * 0.5f;
		for (int s = 0; s < steps; s++) {
			const float u0 = r->length * (float)s / steps;
			const float u1 = r->length * (float)(s + 1) / steps;
			const float h0 = r->height * (float)s / steps;
			const float h1 = r->height * (float)(s + 1) / steps;
			const FVec3 c0 = fvec3_add(r->origin, fvec3_scale(r->fwd, u0));
			const FVec3 c1 = fvec3_add(r->origin, fvec3_scale(r->fwd, u1));
			const FVec3 l0 = fvec3_add(c0, fvec3_scale(r->right, -half));
			const FVec3 r0 = fvec3_add(c0, fvec3_scale(r->right, half));
			const FVec3 l1 = fvec3_add(c1, fvec3_scale(r->right, -half));
			const FVec3 r1 = fvec3_add(c1, fvec3_scale(r->right, half));
			const float mix = (float)s / steps;
			push_quad_uv(&b,
				     fvec3(l0.x, h0, l0.z), fvec3(r0.x, h0, r0.z),
				     fvec3(r1.x, h1, r1.z), fvec3(l1.x, h1, l1.z),
				     u0 / 3.0f, -half / 3.0f, u0 / 3.0f, half / 3.0f,
				     u1 / 3.0f, half / 3.0f, u1 / 3.0f, -half / 3.0f,
				     1.0f, 1.0f, 0.35f + 0.35f * mix);
		}
	}
	m->terrainCount = b.n - m->terrainStart;

	m->gravelStart = b.n;
	for (int si = 0; si < terrain->slickCount; si++) {
		/* Flat gravel disc, slightly above the road. */
		const F3DSTerrainSlick *s = &terrain->slicks[si];
		const int seg = 14;
		for (int j = 0; j < seg; j++) {
			const float a0 = (float)j / seg * 2.0f * F3DS_PI;
			const float a1 = (float)(j + 1) / seg * 2.0f * F3DS_PI;
			push_tri_uv(&b,
				    fvec3(s->x, 0.02f, s->z),
				    fvec3(s->x + cosf(a0) * s->radius, 0.02f, s->z + sinf(a0) * s->radius),
				    fvec3(s->x + cosf(a1) * s->radius, 0.02f, s->z + sinf(a1) * s->radius),
				    0.0f, 0.0f,
				    cosf(a0) * s->radius / 2.0f, sinf(a0) * s->radius / 2.0f,
				    cosf(a1) * s->radius / 2.0f, sinf(a1) * s->radius / 2.0f,
				    0.83f, 0.73f, 0.65f);
		}
	}
	m->gravelCount = b.n - m->gravelStart;

	/* --- Car --- */
	m->carStart = b.n;
	push_box(&b, -0.9f, 0.30f, -2.1f, 0.9f, 1.00f, 2.1f, 0.85f, 0.15f, 0.15f);
	push_box(&b, -0.7f, 1.00f, -1.2f, 0.7f, 1.55f, 0.6f, 0.12f, 0.12f, 0.16f);
	push_box(&b, -1.05f, 0.0f, -1.65f, -0.75f, 0.62f, -1.05f, 0.08f, 0.08f, 0.08f);
	push_box(&b, 0.75f, 0.0f, -1.65f, 1.05f, 0.62f, -1.05f, 0.08f, 0.08f, 0.08f);
	push_box(&b, -1.05f, 0.0f, 1.05f, -0.75f, 0.62f, 1.65f, 0.08f, 0.08f, 0.08f);
	push_box(&b, 0.75f, 0.0f, 1.05f, 1.05f, 0.62f, 1.65f, 0.08f, 0.08f, 0.08f);
	m->carCount = b.n - m->carStart;

	/* --- Cockpit interior --- */
	m->cockpitStart = b.n;
	push_box(&b, -0.7f, 0.85f, 0.6f, 0.7f, 1.25f, 1.1f, 0.15f, 0.15f, 0.2f);
	push_box(&b, -0.28f, 0.95f, 0.8f, 0.28f, 1.02f, 0.88f, 0.2f, 0.2f, 0.2f);
	push_box(&b, -0.045f, 0.80f, 0.8f, 0.045f, 1.17f, 0.88f, 0.2f, 0.2f, 0.2f);
	push_box(&b, -0.45f, 0.2f, -0.9f, 0.45f, 0.9f, -0.4f, 0.25f, 0.05f, 0.05f);
	m->cockpitCount = b.n - m->cockpitStart;

	/* --- Compact inspection garage used by the main menu and workshop. --- */
	m->garageStart = b.n;
	const float gx = track->pts[0].x;
	const float gz = track->pts[0].z;
	push_quad(&b, fvec3(gx - 12.0f, -0.03f, gz - 12.0f),
		fvec3(gx - 12.0f, -0.03f, gz + 12.0f),
		fvec3(gx + 12.0f, -0.03f, gz + 12.0f),
		fvec3(gx + 12.0f, -0.03f, gz - 12.0f), 0.28f, 0.30f, 0.33f);
	/* Side walls and rear wall; the front remains open for vehicle access. */
	push_box(&b, gx - 12.0f, 0.0f, gz - 12.0f, gx - 11.4f, 7.0f, gz + 12.0f, 0.20f, 0.23f, 0.28f);
	push_box(&b, gx + 11.4f, 0.0f, gz - 12.0f, gx + 12.0f, 7.0f, gz + 12.0f, 0.20f, 0.23f, 0.28f);
	push_box(&b, gx - 12.0f, 0.0f, gz - 12.0f, gx + 12.0f, 7.0f, gz - 11.4f, 0.24f, 0.27f, 0.31f);
	push_box(&b, gx - 12.0f, 6.7f, gz - 12.0f, gx + 12.0f, 7.2f, gz + 12.0f, 0.15f, 0.17f, 0.20f);
	/* Front pillars, inspection lights and a colored workshop strip. */
	push_box(&b, gx - 11.5f, 0.0f, gz + 10.8f, gx - 10.8f, 6.7f, gz + 11.5f, 0.32f, 0.34f, 0.38f);
	push_box(&b, gx + 10.8f, 0.0f, gz + 10.8f, gx + 11.5f, 6.7f, gz + 11.5f, 0.32f, 0.34f, 0.38f);
	push_box(&b, gx - 8.0f, 6.25f, gz - 4.0f, gx + 8.0f, 6.38f, gz - 3.5f, 0.92f, 0.90f, 0.72f);
	push_box(&b, gx - 8.0f, 6.25f, gz + 4.0f, gx + 8.0f, 6.38f, gz + 4.5f, 0.92f, 0.90f, 0.72f);
	push_box(&b, gx - 11.35f, 0.05f, gz + 9.9f, gx + 11.35f, 0.18f, gz + 10.15f, 0.78f, 0.18f, 0.10f);
	m->garageCount = b.n - m->garageStart;

	m->total = b.n;
	m->data = b.v;
}

void meshes_free(F3DSMeshes *m)
{
	if (m->data) {
		linearFree(m->data);
		m->data = NULL;
	}
	m->total = 0;
}
