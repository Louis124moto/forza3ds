#include "meshes.h"

#define MESH_MAX_VERTS 30000

typedef struct {
	F3DSVertex *v;
	uint32_t n, cap;
} Builder;

static void push(Builder *b, FVec3 p, float r, float g, float bl, float a)
{
	if (b->n >= b->cap)
		return;
	F3DSVertex *out = &b->v[b->n++];
	out->x = p.x; out->y = p.y; out->z = p.z;
	out->r = r; out->g = g; out->b = bl; out->a = a;
}

static void push_tri(Builder *b, FVec3 p0, FVec3 p1, FVec3 p2, float r, float g, float bl)
{
	push(b, p0, r, g, bl, 1.0f);
	push(b, p1, r, g, bl, 1.0f);
	push(b, p2, r, g, bl, 1.0f);
}

static void push_quad(Builder *b, FVec3 p0, FVec3 p1, FVec3 p2, FVec3 p3, float r, float g, float bl)
{
	push_tri(b, p0, p1, p2, r, g, bl);
	push_tri(b, p0, p2, p3, r, g, bl);
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

void meshes_build(F3DSMeshes *m, const F3DSTrack *track, const F3DSTerrain *terrain)
{
	const uint32_t cap = MESH_MAX_VERTS;
	Builder b;
	b.v = (F3DSVertex *)linearAlloc(cap * sizeof(F3DSVertex));
	b.n = 0;
	b.cap = cap;

	/* --- Ground plane (grass) --- */
	m->groundStart = 0;
	push_quad(&b, fvec3(-300, -0.05f, -300), fvec3(-300, -0.05f, 300), fvec3(300, -0.05f, 300), fvec3(300, -0.05f, -300),
		  0.16f, 0.42f, 0.15f);
	m->groundCount = b.n - m->groundStart;

	/* --- Road --- */
	m->roadStart = b.n;
	for (int i = 0; i < TRACK_POINTS; i++) {
		const float w = TRACK_HALF_WIDTH;
		const FVec3 r = track->right[i];
		const FVec3 p0 = fvec3_add(track->pts[i], fvec3_scale(r, -w));
		const FVec3 p1 = fvec3_add(track->pts[i], fvec3_scale(r, w));
		const FVec3 p2 = fvec3_add(track->pts[i + 1], fvec3_scale(r, w));
		const FVec3 p3 = fvec3_add(track->pts[i + 1], fvec3_scale(r, -w));
		const float shade = (i & 1) ? 0.28f : 0.32f;
		push_quad(&b, p0, p1, p2, p3, shade, shade, shade + 0.02f);
		const float w2 = w - 0.4f;
		const FVec3 e0 = fvec3_add(track->pts[i], fvec3_scale(r, -w2));
		const FVec3 e1 = fvec3_add(track->pts[i], fvec3_scale(r, w2));
		const FVec3 ee0 = fvec3_add(track->pts[i + 1], fvec3_scale(r, -w2));
		const FVec3 ee1 = fvec3_add(track->pts[i + 1], fvec3_scale(r, w2));
		for (int side = 0; side < 2; side++) {
			const FVec3 a0 = side ? e1 : e0;
			const FVec3 a1 = side ? ee1 : ee0;
			const FVec3 o = fvec3_scale(r, side ? 0.4f : -0.4f);
			push_quad(&b,
				  fvec3_add(a0, o), fvec3_add(a1, o),
				  fvec3_add(a1, fvec3_scale(o, -1.0f)), fvec3_add(a0, fvec3_scale(o, -1.0f)),
				  0.85f, 0.85f, 0.85f);
		}
	}
	m->roadCount = b.n - m->roadStart;

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

	/* --- Decor barrier wall --- */
	m->decorStart = b.n;
	for (int i = 0; i < TRACK_POINTS; i += 3) {
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
	m->decorCount = b.n - m->decorStart;

	/* --- Terrain features: ramps (stepped wedges) + gravel zones --- */
	m->terrainStart = b.n;
	for (int ri = 0; ri < terrain->rampCount; ri++) {
		const F3DSTerrainRamp *r = &terrain->ramps[ri];
		const int steps = 8;
		for (int s = 0; s < steps; s++) {
			const float u0 = r->length * (float)s / steps;
			const float u1 = r->length * (float)(s + 1) / steps;
			const float h0 = r->height * (float)s / steps;
			const float h1 = r->height * (float)(s + 1) / steps;
			const FVec3 c0 = fvec3_add(r->origin, fvec3_scale(r->fwd, u0));
			const FVec3 c1 = fvec3_add(r->origin, fvec3_scale(r->fwd, u1));
			const float half = r->width * 0.5f;
			const FVec3 l0 = fvec3_add(c0, fvec3_scale(r->right, -half));
			const FVec3 r0 = fvec3_add(c0, fvec3_scale(r->right, half));
			const FVec3 l1 = fvec3_add(c1, fvec3_scale(r->right, -half));
			const FVec3 r1 = fvec3_add(c1, fvec3_scale(r->right, half));
			const float mix = (float)s / steps;
			push_quad(&b,
				  fvec3(l0.x, h0, l0.z), fvec3(r0.x, h0, r0.z),
				  fvec3(r1.x, h1, r1.z), fvec3(l1.x, h1, l1.z),
				  0.9f, 0.45f + 0.4f * mix, 0.1f);
		}
	}
	for (int si = 0; si < terrain->slickCount; si++) {
		/* Flat gravel disc, slightly above the road. */
		const F3DSTerrainSlick *s = &terrain->slicks[si];
		const int seg = 14;
		for (int j = 0; j < seg; j++) {
			const float a0 = (float)j / seg * 2.0f * F3DS_PI;
			const float a1 = (float)(j + 1) / seg * 2.0f * F3DS_PI;
			push_tri(&b,
				 fvec3(s->x, 0.02f, s->z),
				 fvec3(s->x + cosf(a0) * s->radius, 0.02f, s->z + sinf(a0) * s->radius),
				 fvec3(s->x + cosf(a1) * s->radius, 0.02f, s->z + sinf(a1) * s->radius),
				 0.45f, 0.36f, 0.26f);
		}
	}
	m->terrainCount = b.n - m->terrainStart;

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
