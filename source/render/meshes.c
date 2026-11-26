#include "meshes.h"
#include <string.h>

typedef struct { float r, g, b; } Color;

#define RGB(r, g, b) ((Color){ (r), (g), (b) })

static const Color WHITE = { 1.0f, 1.0f, 1.0f };

static uint8_t to_byte(float v) { return (uint8_t)(clampf(v, 0.0f, 1.0f) * 255.0f + 0.5f); }

static void vertex(F3DSMeshes *m, FVec3 p, Color c)
{
	if (m->count >= m->capacity) {
		m->overflow = true;
		return;
	}
	F3DSVertex *v = &m->data[m->count++];
	v->x = p.x; v->y = p.y; v->z = p.z;
	v->r = to_byte(c.r); v->g = to_byte(c.g); v->b = to_byte(c.b); v->a = 255;
}

static FVec3 cross(FVec3 a, FVec3 b)
{
	return fvec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

/* Triangle whose front face looks along "out"; flat lighting baked in. */
static void tri(F3DSMeshes *m, FVec3 a, FVec3 b, FVec3 c, FVec3 out, Color col, bool lit)
{
	FVec3 n = cross(fvec3_sub(b, a), fvec3_sub(c, a));
	if (fvec3_dot(n, out) < 0.0f) {
		const FVec3 t = b; b = c; c = t;
		n = fvec3_scale(n, -1.0f);
	}
	const float len = fvec3_len(n);
	if (len < 1e-7f)
		return;
	if (lit) {
		const FVec3 sun = fvec3(0.38f, 0.86f, 0.34f);
		const float light = 0.62f + 0.38f * fmaxf(fvec3_dot(n, sun) / len, 0.0f);
		col.r *= light; col.g *= light; col.b *= light;
	}
	vertex(m, a, col);
	vertex(m, b, col);
	vertex(m, c, col);
}

static void quad(F3DSMeshes *m, FVec3 a, FVec3 b, FVec3 c, FVec3 d, FVec3 out, Color col, bool lit)
{
	tri(m, a, b, c, out, col, lit);
	tri(m, a, c, d, out, col, lit);
}

/* Horizontal surface seen from above. */
static void flat(F3DSMeshes *m, FVec3 a, FVec3 b, FVec3 c, FVec3 d, Color col)
{
	quad(m, a, b, c, d, fvec3(0.0f, 1.0f, 0.0f), col, false);
}

/* Surface seen from the driver's seat (facing -Z). */
static void panel(F3DSMeshes *m, float x0, float y0, float x1, float y1, float z, Color col)
{
	quad(m, fvec3(x0, y0, z), fvec3(x1, y0, z), fvec3(x1, y1, z), fvec3(x0, y1, z),
		fvec3(0.0f, 0.0f, -1.0f), col, false);
}

enum { FACE_BOTTOM = 1, FACE_TOP = 2, FACE_BACK = 4, FACE_FRONT = 8, FACE_LEFT = 16, FACE_RIGHT = 32,
	FACE_ALL = 63, FACE_SOLID = 62 /* no bottom */ };

/* Convex solid with corners 0-3 below and 4-7 above, each ring ordered
 * (-x,-z) (+x,-z) (+x,+z) (-x,+z). */
static void hexa(F3DSMeshes *m, const FVec3 c[8], Color col, int faces)
{
	static const int idx[6][4] = {
		{ 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 0, 1, 5, 4 }, { 3, 2, 6, 7 }, { 1, 2, 6, 5 }, { 0, 3, 7, 4 }
	};
	FVec3 mid = fvec3(0, 0, 0);
	for (int i = 0; i < 8; ++i) mid = fvec3_add(mid, fvec3_scale(c[i], 0.125f));
	for (int f = 0; f < 6; ++f) {
		if (!(faces & (1 << f)))
			continue;
		const FVec3 a = c[idx[f][0]], b = c[idx[f][1]], d = c[idx[f][2]], e = c[idx[f][3]];
		const FVec3 centre = fvec3_scale(fvec3_add(fvec3_add(a, b), fvec3_add(d, e)), 0.25f);
		quad(m, a, b, d, e, fvec3_sub(centre, mid), col, true);
	}
}

static void box(F3DSMeshes *m, float x0, float y0, float z0, float x1, float y1, float z1,
	Color col, int faces)
{
	const FVec3 c[8] = {
		{ x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 },
		{ x0, y1, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { x0, y1, z1 }
	};
	hexa(m, c, col, faces);
}

/* Symmetric tapered block between two rectangles. */
static void frustum(F3DSMeshes *m, float y0, float rear0, float front0, float w0,
	float y1, float rear1, float front1, float w1, Color col, int faces)
{
	const FVec3 c[8] = {
		{ -w0, y0, rear0 }, { w0, y0, rear0 }, { w0, y0, front0 }, { -w0, y0, front0 },
		{ -w1, y1, rear1 }, { w1, y1, rear1 }, { w1, y1, front1 }, { -w1, y1, front1 }
	};
	hexa(m, c, col, faces);
}

/* Square beam between two points. */
static void beam(F3DSMeshes *m, FVec3 a, FVec3 b, float half, Color col)
{
	const FVec3 c[8] = {
		{ a.x - half, a.y, a.z - half }, { a.x + half, a.y, a.z - half },
		{ a.x + half, a.y, a.z + half }, { a.x - half, a.y, a.z + half },
		{ b.x - half, b.y, b.z - half }, { b.x + half, b.y, b.z - half },
		{ b.x + half, b.y, b.z + half }, { b.x - half, b.y, b.z + half }
	};
	hexa(m, c, col, FACE_ALL);
}

/* Pyramid on a regular polygon; apex above or below the base. */
static void cone(F3DSMeshes *m, float x, float z, float base, float apex, float radius, int sides,
	Color col, bool cap)
{
	const FVec3 tip = fvec3(x, apex, z), centre = fvec3(x, base, z);
	const FVec3 mid = fvec3(x, (base * 2.0f + apex) / 3.0f, z);
	FVec3 prev = fvec3(x + radius, base, z);
	for (int i = 1; i <= sides; ++i) {
		const float a = (float)i / (float)sides * 2.0f * F3DS_PI;
		const FVec3 cur = fvec3(x + cosf(a) * radius, base, z + sinf(a) * radius);
		const FVec3 face = fvec3_scale(fvec3_add(fvec3_add(prev, cur), tip), 1.0f / 3.0f);
		tri(m, prev, cur, tip, fvec3_sub(face, mid), col, true);
		if (cap)
			tri(m, centre, prev, cur, fvec3(0.0f, base < apex ? -1.0f : 1.0f, 0.0f), col, true);
		prev = cur;
	}
}

static F3DSRange since(const F3DSMeshes *m, uint32_t start)
{
	const F3DSRange r = { start, m->count - start };
	return r;
}

/* ---------------------------------------------------------------- world -- */

static int chunk_of(float x, float z)
{
	const float cell = 576.0f / F3DS_CHUNK_AXIS;
	int ix = (int)((x + 288.0f) / cell), iz = (int)((z + 288.0f) / cell);
	if (ix < 0) ix = 0;
	if (ix >= F3DS_CHUNK_AXIS) ix = F3DS_CHUNK_AXIS - 1;
	if (iz < 0) iz = 0;
	if (iz >= F3DS_CHUNK_AXIS) iz = F3DS_CHUNK_AXIS - 1;
	return iz * F3DS_CHUNK_AXIS + ix;
}

#define GROUND_SECTORS 96
#define GROUND_RINGS 14

static void ground(F3DSMeshes *m, int chunk)
{
	for (int s = 0; s < GROUND_SECTORS; ++s) {
		const float a0 = (float)s / GROUND_SECTORS * 2.0f * F3DS_PI;
		const float a1 = (float)(s + 1) / GROUND_SECTORS * 2.0f * F3DS_PI;
		const float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
		/* Shoreline sits just outside the drivable limit. */
		const float r0 = track_coast_radius(c0, s0) + 6.0f, r1 = track_coast_radius(c1, s1) + 6.0f;
		for (int j = 0; j < GROUND_RINGS + 2; ++j) {
			float in0, in1, out0, out1, yIn = -0.05f, yOut = -0.05f;
			Color col;
			if (j < GROUND_RINGS) {
				in0 = (r0 - 12.0f) * (float)j / GROUND_RINGS;
				in1 = (r1 - 12.0f) * (float)j / GROUND_RINGS;
				out0 = (r0 - 12.0f) * (float)(j + 1) / GROUND_RINGS;
				out1 = (r1 - 12.0f) * (float)(j + 1) / GROUND_RINGS;
				const float v = 0.035f * sinf((float)(s * 7 + j * 13)) + ((s + j) & 1 ? 0.012f : 0.0f);
				col = RGB(0.24f + v, 0.50f + v * 1.4f, 0.20f + v);
			} else if (j == GROUND_RINGS) {
				in0 = r0 - 12.0f; in1 = r1 - 12.0f; out0 = r0; out1 = r1;
				col = RGB(0.82f, 0.74f, 0.52f);
			} else {
				in0 = r0; in1 = r1; out0 = r0 + 7.0f; out1 = r1 + 7.0f;
				yOut = -1.35f;
				col = RGB(0.64f, 0.57f, 0.40f);
			}
			const float mx = (c0 * (in0 + out0) + c1 * (in1 + out1)) * 0.25f;
			const float mz = (s0 * (in0 + out0) + s1 * (in1 + out1)) * 0.25f;
			if (chunk_of(mx, mz) != chunk)
				continue;
			const FVec3 a = fvec3(c0 * in0, yIn, s0 * in0), b = fvec3(c0 * out0, yOut, s0 * out0);
			const FVec3 c = fvec3(c1 * out1, yOut, s1 * out1), d = fvec3(c1 * in1, yIn, s1 * in1);
			if (j == 0)
				tri(m, a, b, c, fvec3(0, 1, 0), col, false);
			else
				flat(m, a, b, c, d, col);
		}
	}
}

static FVec3 road_point(const F3DSTrack *t, int i, float offset, float y)
{
	FVec3 p = fvec3_add(t->pts[i], fvec3_scale(t->jointRight[i], offset));
	p.y = y;
	return p;
}

static void road(F3DSMeshes *m, const F3DSTrack *t, int chunk)
{
	for (int i = 0; i < TRACK_POINTS; ++i) {
		const FVec3 mid = fvec3_scale(fvec3_add(t->pts[i], t->pts[i + 1]), 0.5f);
		if (chunk_of(mid.x, mid.z) != chunk)
			continue;
		const float w = TRACK_HALF_WIDTH;
		const float shade = (i & 1) ? 0.275f : 0.295f;
		flat(m, road_point(t, i, -w, 0.0f), road_point(t, i, w, 0.0f),
			road_point(t, i + 1, w, 0.0f), road_point(t, i + 1, -w, 0.0f),
			RGB(shade, shade, shade + 0.025f));
		for (int side = -1; side <= 1; side += 2) {
			const float a = side * (w - 0.55f), b = side * (w - 0.30f);
			flat(m, road_point(t, i, a, 0.015f), road_point(t, i, b, 0.015f),
				road_point(t, i + 1, b, 0.015f), road_point(t, i + 1, a, 0.015f),
				RGB(0.90f, 0.90f, 0.88f));
		}
		/* Dashed centre line; the first segment carries the start line. */
		const FVec3 f = t->fwd[i], r = t->right[i];
		const float len = t->segLen[i];
		const float from = i == 0 ? 0.0f : len * 0.25f, to = i == 0 ? 1.0f : len * 0.75f;
		const float half = i == 0 ? w - 0.6f : 0.12f;
		const FVec3 p0 = fvec3_add(t->pts[i], fvec3_scale(f, from));
		const FVec3 p1 = fvec3_add(t->pts[i], fvec3_scale(f, to));
		FVec3 q[4] = {
			fvec3_add(p0, fvec3_scale(r, -half)), fvec3_add(p0, fvec3_scale(r, half)),
			fvec3_add(p1, fvec3_scale(r, half)), fvec3_add(p1, fvec3_scale(r, -half))
		};
		for (int k = 0; k < 4; ++k) q[k].y = 0.018f;
		flat(m, q[0], q[1], q[2], q[3], i == 0 ? RGB(0.95f, 0.95f, 0.95f) : RGB(0.94f, 0.80f, 0.25f));
	}
}

static void gravel(F3DSMeshes *m, const F3DSTerrain *t, int chunk)
{
	for (int i = 0; i < t->slickCount; ++i) {
		const F3DSTerrainSlick *s = &t->slicks[i];
		if (chunk_of(s->x, s->z) != chunk)
			continue;
		const int seg = 20;
		for (int j = 0; j < seg; ++j) {
			const float a0 = (float)j / seg * 2.0f * F3DS_PI;
			const float a1 = (float)(j + 1) / seg * 2.0f * F3DS_PI;
			tri(m, fvec3(s->x, 0.03f, s->z),
				fvec3(s->x + cosf(a0) * s->radius, 0.03f, s->z + sinf(a0) * s->radius),
				fvec3(s->x + cosf(a1) * s->radius, 0.03f, s->z + sinf(a1) * s->radius),
				fvec3(0, 1, 0), (j & 1) ? RGB(0.56f, 0.45f, 0.30f) : RGB(0.50f, 0.40f, 0.27f), false);
		}
	}
}

static FVec3 ramp_point(const F3DSTerrainRamp *r, float u, float v, float y)
{
	FVec3 p = fvec3_add(r->origin, fvec3_add(fvec3_scale(r->fwd, u), fvec3_scale(r->right, v)));
	p.y = y;
	return p;
}

static void ramps(F3DSMeshes *m, const F3DSTerrain *t, int chunk)
{
	for (int i = 0; i < t->rampCount; ++i) {
		const F3DSTerrainRamp *r = &t->ramps[i];
		if (chunk_of(r->origin.x, r->origin.z) != chunk)
			continue;
		const float half = r->width * 0.5f;
		const int steps = 6;
		for (int s = 0; s < steps; ++s) {
			const float u0 = r->length * s / steps, u1 = r->length * (s + 1) / steps;
			const float h0 = r->height * s / steps + 0.025f, h1 = r->height * (s + 1) / steps + 0.025f;
			quad(m, ramp_point(r, u0, -half, h0), ramp_point(r, u0, half, h0),
				ramp_point(r, u1, half, h1), ramp_point(r, u1, -half, h1), fvec3(0, 1, 0),
				(s & 1) ? RGB(0.95f, 0.95f, 0.90f) : RGB(0.95f, 0.50f, 0.10f), true);
		}
		const Color side = RGB(0.48f, 0.30f, 0.16f);
		for (int e = -1; e <= 1; e += 2)
			tri(m, ramp_point(r, 0.0f, e * half, 0.0f), ramp_point(r, r->length, e * half, 0.0f),
				ramp_point(r, r->length, e * half, r->height), fvec3_scale(r->right, (float)e),
				side, true);
		quad(m, ramp_point(r, r->length, -half, 0.0f), ramp_point(r, r->length, half, 0.0f),
			ramp_point(r, r->length, half, r->height), ramp_point(r, r->length, -half, r->height),
			r->fwd, side, true);
	}
}

static void walls(F3DSMeshes *m, const F3DSTrack *t, int chunk)
{
	for (int i = 0; i < t->wallCount; ++i) {
		const F3DSWall *w = &t->walls[i];
		if (chunk_of((w->a.x + w->b.x) * 0.5f, (w->a.z + w->b.z) * 0.5f) != chunk)
			continue;
		const FVec3 d = fvec3_sub(w->b, w->a);
		const float len = fvec3_len(d);
		if (len < 0.01f)
			continue;
		const FVec3 u = fvec3_scale(d, 1.0f / len);
		const FVec3 side = fvec3(-u.z * TRACK_WALL_HALF_THICKNESS, 0.0f, u.x * TRACK_WALL_HALF_THICKNESS);
		const int pieces = 3;
		for (int k = 0; k < pieces; ++k) {
			const FVec3 p = fvec3_add(w->a, fvec3_scale(d, (float)k / pieces));
			const FVec3 q = fvec3_add(w->a, fvec3_scale(d, (float)(k + 1) / pieces));
			FVec3 c[8] = {
				fvec3_sub(p, side), fvec3_add(p, side), fvec3_add(q, side), fvec3_sub(q, side),
				fvec3_sub(p, side), fvec3_add(p, side), fvec3_add(q, side), fvec3_sub(q, side)
			};
			for (int v = 4; v < 8; ++v) c[v].y = TRACK_WALL_HEIGHT;
			/* Inner joints are hidden; only the run ends are closed. */
			int faces = FACE_TOP | FACE_LEFT | FACE_RIGHT;
			if (k == 0) faces |= FACE_BACK;
			if (k == pieces - 1) faces |= FACE_FRONT;
			hexa(m, c, (k & 1) ? RGB(0.88f, 0.16f, 0.12f) : RGB(0.92f, 0.92f, 0.90f), faces);
		}
	}
}

static void obstacles(F3DSMeshes *m, const F3DSTrack *t, int chunk)
{
	const F3DSObstacle *torii[2] = { NULL, NULL };
	int pillars = 0;
	for (int i = 0; i < t->obstacleCount; ++i) {
		const F3DSObstacle *o = &t->obstacles[i];
		if (o->kind == F3DS_OBSTACLE_TORII && pillars < 2)
			torii[pillars++] = o;
		if (chunk_of(o->center.x, o->center.z) != chunk)
			continue;
		const float x = o->center.x, z = o->center.z;
		if (o->kind == F3DS_OBSTACLE_CRATE) {
			box(m, x - o->hx, 0.0f, z - o->hz, x + o->hx, o->height, z + o->hz,
				RGB(0.72f, 0.48f, 0.22f), FACE_SOLID);
			box(m, x - o->hx - 0.02f, o->height * 0.42f, z - o->hz - 0.02f,
				x + o->hx + 0.02f, o->height * 0.58f, z + o->hz + 0.02f,
				RGB(0.38f, 0.22f, 0.10f), FACE_BACK | FACE_FRONT | FACE_LEFT | FACE_RIGHT);
		} else if (o->kind == F3DS_OBSTACLE_TORII) {
			box(m, x - o->hx, 0.0f, z - o->hz, x + o->hx, o->height, z + o->hz,
				RGB(0.85f, 0.18f, 0.10f), FACE_SOLID);
		} else {
			for (int band = 0; band < 4; ++band)
				box(m, x - o->hx, o->height * band / 4.0f, z - o->hz,
					x + o->hx, o->height * (band + 1) / 4.0f, z + o->hz,
					(band & 1) ? RGB(0.95f, 0.95f, 0.92f) : RGB(0.95f, 0.55f, 0.10f),
					band == 3 ? FACE_SOLID : FACE_SOLID & ~FACE_TOP);
		}
	}
	/* Lintels of the gate, high above the road. */
	if (pillars == 2 && chunk_of(torii[0]->center.x, torii[0]->center.z) == chunk) {
		const FVec3 a = torii[0]->center, b = torii[1]->center;
		const FVec3 d = fvec3_sub(b, a);
		const float len = fvec3_len(d);
		const FVec3 u = fvec3_scale(d, 1.0f / fmaxf(len, 0.01f));
		for (int l = 0; l < 2; ++l) {
			const float y = l ? 6.0f : 5.0f, thick = l ? 0.55f : 0.32f, over = l ? 1.3f : 0.5f;
			const FVec3 p = fvec3_sub(a, fvec3_scale(u, over)), q = fvec3_add(b, fvec3_scale(u, over));
			const FVec3 s = fvec3(-u.z * 0.3f, 0.0f, u.x * 0.3f);
			FVec3 c[8] = {
				fvec3_sub(p, s), fvec3_add(p, s), fvec3_add(q, s), fvec3_sub(q, s),
				fvec3_sub(p, s), fvec3_add(p, s), fvec3_add(q, s), fvec3_sub(q, s)
			};
			for (int v = 0; v < 8; ++v) c[v].y = v < 4 ? y : y + thick;
			hexa(m, c, l ? RGB(0.14f, 0.10f, 0.10f) : RGB(0.85f, 0.18f, 0.10f), FACE_ALL);
		}
	}
}

static void trees(F3DSMeshes *m, const F3DSTrack *t, int chunk)
{
	for (int i = 0; i < t->treeCount; ++i) {
		const F3DSTree *p = &t->trees[i];
		if (chunk_of(p->x, p->z) != chunk)
			continue;
		const float x = p->x, z = p->z, h = p->height, c = p->crown;
		const float w = TRACK_TRUNK_RADIUS * 0.7f;
		box(m, x - w, -0.05f, z - w, x + w, h * 0.55f, z + w, RGB(0.40f, 0.24f, 0.13f),
			FACE_BACK | FACE_FRONT | FACE_LEFT | FACE_RIGHT);
		if (p->blossom) {
			const Color pink = RGB(0.98f, 0.66f, 0.76f);
			cone(m, x, z, h * 0.72f, h + c * 0.5f, c * 1.15f, 6, pink, false);
			cone(m, x, z, h * 0.72f, h * 0.38f, c * 1.15f, 6, RGB(0.90f, 0.55f, 0.68f), false);
		} else {
			cone(m, x, z, h * 0.38f, h * 0.95f, c, 6, RGB(0.09f, 0.36f, 0.20f), true);
			cone(m, x, z, h * 0.68f, h * 1.30f, c * 0.72f, 6, RGB(0.12f, 0.43f, 0.23f), true);
		}
	}
}

static void world(F3DSMeshes *m, const F3DSTrack *track, const F3DSTerrain *terrain)
{
	uint32_t start = m->count;
	const float far = 900.0f;
	/* Split in a grid so no single triangle is enormous. */
	for (int ix = 0; ix < 4; ++ix)
		for (int iz = 0; iz < 4; ++iz) {
			const float x0 = -far + ix * far * 0.5f, z0 = -far + iz * far * 0.5f;
			const float x1 = x0 + far * 0.5f, z1 = z0 + far * 0.5f;
			flat(m, fvec3(x0, -1.2f, z0), fvec3(x0, -1.2f, z1), fvec3(x1, -1.2f, z1),
				fvec3(x1, -1.2f, z0), RGB(0.10f, 0.45f, 0.70f));
		}
	m->sea = since(m, start);

	/* Neighbouring islets and a snow-capped peak on the horizon. */
	start = m->count;
	for (int i = 0; i < 11; ++i) {
		const float a = 0.25f + (float)i * 0.571f;
		const float dist = 355.0f + (float)(i % 3) * 22.0f;
		const float x = cosf(a) * dist, z = sinf(a) * dist;
		const float h = 22.0f + (float)(i % 4) * 9.0f;
		const float radius = 34.0f + (float)(i % 3) * 12.0f;
		cone(m, x, z, -1.2f, h, radius, 8, RGB(0.27f, 0.42f, 0.40f), false);
		if (i == 4) {
			cone(m, x, z, -1.2f, 92.0f, 78.0f, 10, RGB(0.36f, 0.46f, 0.62f), false);
			cone(m, x, z, 62.0f, 93.0f, 26.0f, 10, RGB(0.95f, 0.97f, 1.0f), false);
		}
	}
	m->horizon = since(m, start);

	for (int c = 0; c < F3DS_CHUNK_COUNT; ++c) {
		start = m->count;
		ground(m, c);
		road(m, track, c);
		gravel(m, terrain, c);
		ramps(m, terrain, c);
		walls(m, track, c);
		obstacles(m, track, c);
		trees(m, track, c);
		m->chunks[c].range = since(m, start);
		float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
		for (uint32_t v = start; v < m->count; ++v) {
			x0 = fminf(x0, m->data[v].x); x1 = fmaxf(x1, m->data[v].x);
			z0 = fminf(z0, m->data[v].z); z1 = fmaxf(z1, m->data[v].z);
		}
		if (m->count > start) {
			m->chunks[c].cx = (x0 + x1) * 0.5f;
			m->chunks[c].cz = (z0 + z1) * 0.5f;
			m->chunks[c].radius = 0.5f * sqrtf((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0));
		}
	}
}

/* Workshop room around the start point. */
static void garage(F3DSMeshes *m, FVec3 o)
{
	const uint32_t start = m->count;
	const float R = 11.0f, H = 6.0f;
	for (int ix = 0; ix < 8; ++ix)
		for (int iz = 0; iz < 8; ++iz) {
			const float x0 = o.x - R + ix * R * 0.25f, z0 = o.z - R + iz * R * 0.25f;
			const float x1 = x0 + R * 0.25f, z1 = z0 + R * 0.25f;
			flat(m, fvec3(x0, -0.05f, z0), fvec3(x0, -0.05f, z1), fvec3(x1, -0.05f, z1),
				fvec3(x1, -0.05f, z0), ((ix + iz) & 1) ? RGB(0.34f, 0.36f, 0.40f) : RGB(0.30f, 0.32f, 0.36f));
		}
	/* Turntable ring under the car. */
	for (int i = 0; i < 32; ++i) {
		const float a0 = i / 32.0f * 2.0f * F3DS_PI, a1 = (i + 1) / 32.0f * 2.0f * F3DS_PI;
		flat(m, fvec3(o.x + cosf(a0) * 3.1f, 0.005f, o.z + sinf(a0) * 3.1f),
			fvec3(o.x + cosf(a0) * 3.35f, 0.005f, o.z + sinf(a0) * 3.35f),
			fvec3(o.x + cosf(a1) * 3.35f, 0.005f, o.z + sinf(a1) * 3.35f),
			fvec3(o.x + cosf(a1) * 3.1f, 0.005f, o.z + sinf(a1) * 3.1f),
			(i & 1) ? RGB(0.95f, 0.75f, 0.10f) : RGB(0.12f, 0.12f, 0.12f));
	}
	/* Walls seen from inside, with a coloured band and a roller door. */
	for (int w = 0; w < 4; ++w) {
		const float a = w * F3DS_PI * 0.5f;
		const FVec3 n = fvec3(sinf(a), 0.0f, cosf(a)), s = fvec3(cosf(a), 0.0f, -sinf(a));
		const FVec3 in = fvec3_scale(n, -1.0f);
		for (int band = 0; band < 3; ++band) {
			const float y0 = band == 0 ? -0.05f : band == 1 ? 1.15f : 1.45f;
			const float y1 = band == 0 ? 1.15f : band == 1 ? 1.45f : H;
			const Color col = band == 1 ? RGB(0.86f, 0.30f, 0.16f) :
				(w & 1) ? RGB(0.52f, 0.56f, 0.62f) : RGB(0.46f, 0.50f, 0.57f);
			FVec3 p0 = fvec3_add(o, fvec3_add(fvec3_scale(n, R), fvec3_scale(s, -R)));
			FVec3 p1 = fvec3_add(o, fvec3_add(fvec3_scale(n, R), fvec3_scale(s, R)));
			quad(m, fvec3(p0.x, y0, p0.z), fvec3(p1.x, y0, p1.z), fvec3(p1.x, y1, p1.z),
				fvec3(p0.x, y1, p0.z), in, col, false);
		}
		if (w == 0)
			for (int slat = 0; slat < 7; ++slat) {
				FVec3 p0 = fvec3_add(o, fvec3_add(fvec3_scale(n, R - 0.05f), fvec3_scale(s, -3.6f)));
				FVec3 p1 = fvec3_add(o, fvec3_add(fvec3_scale(n, R - 0.05f), fvec3_scale(s, 3.6f)));
				const float y0 = 0.05f + slat * 0.55f, y1 = y0 + 0.48f;
				quad(m, fvec3(p0.x, y0, p0.z), fvec3(p1.x, y0, p1.z), fvec3(p1.x, y1, p1.z),
					fvec3(p0.x, y1, p0.z), in, RGB(0.66f, 0.68f, 0.70f), false);
			}
	}
	quad(m, fvec3(o.x - R, H, o.z - R), fvec3(o.x + R, H, o.z - R), fvec3(o.x + R, H, o.z + R),
		fvec3(o.x - R, H, o.z + R), fvec3(0, -1, 0), RGB(0.16f, 0.18f, 0.22f), false);
	for (int i = -1; i <= 1; i += 2)
		quad(m, fvec3(o.x - 6.0f, H - 0.03f, o.z + i * 3.0f - 0.25f),
			fvec3(o.x + 6.0f, H - 0.03f, o.z + i * 3.0f - 0.25f),
			fvec3(o.x + 6.0f, H - 0.03f, o.z + i * 3.0f + 0.25f),
			fvec3(o.x - 6.0f, H - 0.03f, o.z + i * 3.0f + 0.25f),
			fvec3(0, -1, 0), RGB(1.0f, 0.98f, 0.82f), false);
	/* Tool chests and tyre stacks along the walls. */
	for (int i = -1; i <= 1; i += 2) {
		box(m, o.x + i * 9.4f - 0.7f, 0.0f, o.z - 4.0f, o.x + i * 9.4f + 0.7f, 1.2f, o.z - 1.2f,
			RGB(0.78f, 0.14f, 0.10f), FACE_SOLID);
		box(m, o.x + i * 9.4f - 0.75f, 1.2f, o.z - 4.1f, o.x + i * 9.4f + 0.75f, 1.3f, o.z - 1.1f,
			RGB(0.15f, 0.15f, 0.17f), FACE_ALL);
		for (int k = 0; k < 4; ++k)
			box(m, o.x + i * 9.2f - 0.4f, k * 0.28f, o.z + 4.0f, o.x + i * 9.2f + 0.4f, k * 0.28f + 0.24f,
				o.z + 4.8f, RGB(0.09f, 0.09f, 0.10f), FACE_ALL);
	}
	m->garage = since(m, start);
}

/* ----------------------------------------------------------------- cars -- */

static void car_exterior(F3DSMeshes *m, int car)
{
	const F3DSCarVisual *v = catalog_visual(car);
	F3DSVehicleDef d;
	catalog_def(car, &d);
	const float w = v->halfWidth, l = v->halfLength, f = v->floorY, belt = v->beltY, roof = v->roofY;
	const float axle = d.wheelBase * 0.5f, R = d.wheelRadius;

	uint32_t start = m->count;
	/* Main body and sloping nose. */
	frustum(m, f, -l, v->cabinFront, w, belt, -l + 0.08f, v->cabinFront, w * 0.95f,
		WHITE, FACE_ALL & ~FACE_FRONT);
	frustum(m, f, v->cabinFront, l, w, belt, v->cabinFront, l - 0.22f, w * 0.95f, WHITE,
		FACE_BOTTOM | FACE_LEFT | FACE_RIGHT | FACE_FRONT);
	quad(m, fvec3(-w * 0.95f, belt, v->cabinFront), fvec3(w * 0.95f, belt, v->cabinFront),
		fvec3(w * 0.95f, belt, l - 0.22f), fvec3(-w * 0.95f, belt, l - 0.22f), fvec3(0, 1, 0.1f), WHITE, true);
	/* Roof and pillars. */
	box(m, -w * 0.76f, roof - 0.07f, v->roofRear, w * 0.76f, roof, v->roofFront, WHITE, FACE_ALL);
	for (int s = -1; s <= 1; s += 2) {
		beam(m, fvec3(s * w * 0.90f, belt, v->cabinFront - 0.03f),
			fvec3(s * w * 0.75f, roof - 0.06f, v->roofFront), 0.04f, WHITE);
		beam(m, fvec3(s * w * 0.90f, belt, v->cabinRear + 0.03f),
			fvec3(s * w * 0.75f, roof - 0.06f, v->roofRear), 0.05f, WHITE);
		/* Mirrors. */
		box(m, s * (w + 0.09f) - 0.09f, belt + 0.01f, v->cabinFront - 0.18f,
			s * (w + 0.09f) + 0.09f, belt + 0.13f, v->cabinFront - 0.02f, WHITE, FACE_ALL);
	}
	if (v->spoiler)
		box(m, -w * 0.88f, belt + 0.20f, -l + 0.02f, w * 0.88f, belt + 0.25f, -l + 0.34f, WHITE, FACE_ALL);
	m->cars[car].paint = since(m, start);

	start = m->count;
	/* Glass. */
	frustum(m, belt, v->cabinRear, v->cabinFront, w * 0.89f,
		roof - 0.06f, v->roofRear, v->roofFront, w * 0.74f, RGB(0.20f, 0.34f, 0.46f),
		FACE_BACK | FACE_FRONT | FACE_LEFT | FACE_RIGHT);
	if (v->spoiler)
		for (int s = -1; s <= 1; s += 2)
			box(m, s * w * 0.6f - 0.04f, belt, -l + 0.12f, s * w * 0.6f + 0.04f, belt + 0.21f,
				-l + 0.22f, RGB(0.10f, 0.10f, 0.11f), FACE_ALL);
	for (int s = -1; s <= 1; s += 2) {
		/* Headlights follow the slope of the nose; tail lights at the back. */
		quad(m, fvec3(s * w * 0.50f, belt - 0.20f, l - 0.07f), fvec3(s * w * 0.86f, belt - 0.20f, l - 0.07f),
			fvec3(s * w * 0.86f, belt - 0.07f, l - 0.14f), fvec3(s * w * 0.50f, belt - 0.07f, l - 0.14f),
			fvec3(0, 0.4f, 1), RGB(1.0f, 0.96f, 0.72f), false);
		panel(m, s * w * 0.52f, belt - 0.22f, s * w * 0.88f, belt - 0.08f, -l - 0.005f,
			RGB(0.95f, 0.10f, 0.08f));
		/* Wheel arches. */
		for (int e = -1; e <= 1; e += 2) {
			const float z = e * axle, x = s * (w + 0.008f);
			quad(m, fvec3(x, f, z - R - 0.12f), fvec3(x, f, z + R + 0.12f),
				fvec3(x, 2.0f * R + 0.09f, z + R * 0.55f), fvec3(x, 2.0f * R + 0.09f, z - R * 0.55f),
				fvec3((float)s, 0, 0), RGB(0.05f, 0.05f, 0.06f), false);
		}
	}
	/* Grille, splitter, diffuser, number plate and exhaust. */
	quad(m, fvec3(-w * 0.40f, f + 0.05f, l + 0.006f), fvec3(w * 0.40f, f + 0.05f, l + 0.006f),
		fvec3(w * 0.40f, f + 0.19f, l - 0.04f), fvec3(-w * 0.40f, f + 0.19f, l - 0.04f),
		fvec3(0, 0.2f, 1), RGB(0.06f, 0.06f, 0.07f), false);
	box(m, -w, f - 0.05f, l - 0.25f, w, f + 0.02f, l + 0.05f, RGB(0.09f, 0.09f, 0.10f), FACE_ALL);
	box(m, -w * 0.9f, f - 0.05f, -l - 0.04f, w * 0.9f, f + 0.08f, -l + 0.2f,
		RGB(0.09f, 0.09f, 0.10f), FACE_ALL);
	panel(m, -0.22f, f + 0.18f, 0.22f, f + 0.32f, -l - 0.006f, RGB(0.92f, 0.94f, 0.90f));
	box(m, -w * 0.62f, f - 0.02f, -l - 0.12f, -w * 0.45f, f + 0.08f, -l, RGB(0.55f, 0.55f, 0.58f), FACE_ALL);
	m->cars[car].trim = since(m, start);
}

/* Flat ring in the XY plane facing -Z. */
static void annulus(F3DSMeshes *m, float cx, float cy, float z, float inner, float outer,
	float from, float sweep, int segments, Color col)
{
	for (int i = 0; i < segments; ++i) {
		const float a0 = from + sweep * i / segments, a1 = from + sweep * (i + 1) / segments;
		quad(m, fvec3(cx + sinf(a0) * inner, cy + cosf(a0) * inner, z),
			fvec3(cx + sinf(a0) * outer, cy + cosf(a0) * outer, z),
			fvec3(cx + sinf(a1) * outer, cy + cosf(a1) * outer, z),
			fvec3(cx + sinf(a1) * inner, cy + cosf(a1) * inner, z),
			fvec3(0, 0, -1), col, false);
	}
}

/* Right-hand drive cockpit built around the fixed eye position. */
static void car_cockpit(F3DSMeshes *m, int car)
{
	const F3DSCarVisual *v = catalog_visual(car);
	const float w = v->halfWidth, l = v->halfLength, belt = v->beltY, roof = v->roofY;
	const float ex = v->seatX, ey = v->eyeY, ez = v->eyeZ;
	const float dashTop = ey - 0.235f, dashNear = ez + 0.56f;

	uint32_t start = m->count;
	/* Bonnet seen through the windscreen, A pillars, roof header. */
	quad(m, fvec3(-w * 0.95f, belt, v->cabinFront), fvec3(w * 0.95f, belt, v->cabinFront),
		fvec3(w * 0.90f, belt - 0.16f, l), fvec3(-w * 0.90f, belt - 0.16f, l),
		fvec3(0, 1, 0.1f), WHITE, true);
	for (int s = -1; s <= 1; s += 2) {
		beam(m, fvec3(s * w * 0.93f, belt - 0.02f, v->cabinFront + 0.04f),
			fvec3(s * w * 0.80f, roof + 0.02f, v->roofFront + 0.12f), 0.028f, WHITE);
		box(m, s * (w + 0.09f) - 0.09f, belt + 0.01f, v->cabinFront - 0.12f,
			s * (w + 0.09f) + 0.09f, belt + 0.13f, v->cabinFront + 0.02f, WHITE, FACE_ALL);
	}
	m->cars[car].cockpitPaint = since(m, start);

	start = m->count;
	const Color dash = RGB(0.13f, 0.13f, 0.16f), dark = RGB(0.07f, 0.07f, 0.09f);
	/* Dashboard from the floor to the base of the windscreen. */
	box(m, -w * 0.93f, v->floorY, dashNear, w * 0.93f, dashTop, v->cabinFront + 0.04f, dash, FACE_SOLID);
	box(m, -w * 0.95f, dashTop - 0.015f, dashNear - 0.015f, w * 0.95f, dashTop + 0.012f,
		dashNear + 0.03f, RGB(0.24f, 0.24f, 0.29f), FACE_ALL);
	/* Cabin floor and door linings. */
	flat(m, fvec3(-w * 0.93f, v->floorY, v->cabinRear), fvec3(w * 0.93f, v->floorY, v->cabinRear),
		fvec3(w * 0.93f, v->floorY, dashNear), fvec3(-w * 0.93f, v->floorY, dashNear), dark);
	for (int s = -1; s <= 1; s += 2) {
		const float x = s * w * 0.93f;
		quad(m, fvec3(x, v->floorY, v->cabinRear), fvec3(x, v->floorY, v->cabinFront),
			fvec3(x, belt + 0.01f, v->cabinFront), fvec3(x, belt + 0.01f, v->cabinRear),
			fvec3((float)-s, 0, 0), RGB(0.17f, 0.17f, 0.21f), false);
	}
	/* Roof edge and interior mirror. */
	box(m, -w * 0.80f, roof + 0.0f, v->roofFront + 0.06f, w * 0.80f, roof + 0.06f, v->roofFront + 0.16f,
		dark, FACE_ALL);
	box(m, -0.12f, roof - 0.10f, v->roofFront + 0.08f, 0.12f, roof - 0.035f, v->roofFront + 0.11f,
		RGB(0.30f, 0.38f, 0.46f), FACE_ALL);
	/* Centre screen and air vents. */
	panel(m, -0.12f, ey - 0.40f, 0.12f, ey - 0.27f, dashNear - 0.017f, RGB(0.02f, 0.20f, 0.26f));
	/* Instrument binnacle with two dials. */
	const float iz = ez + F3DS_DIAL_Z + 0.012f;
	box(m, ex - 0.165f, ey - 0.40f, iz + 0.001f, ex + 0.165f, ey - 0.215f, dashNear, dash, FACE_ALL);
	panel(m, ex - 0.155f, ey - 0.39f, ex + 0.155f, ey - 0.225f, iz, dark);
	for (int dial = -1; dial <= 1; dial += 2) {
		const float cx = ex + dial * F3DS_DIAL_OFFSET_X, cy = ey + F3DS_DIAL_Y;
		annulus(m, cx, cy, iz - 0.002f, F3DS_DIAL_RADIUS, F3DS_DIAL_RADIUS + 0.006f,
			0.0f, 2.0f * F3DS_PI, 24, RGB(0.55f, 0.62f, 0.68f));
		/* Ticks from -135 to +135 degrees, clockwise for the driver. The
		 * driver's right is -X, so the angle is mirrored. */
		for (int t = 0; t <= 8; ++t) {
			const float a = -(-0.75f * F3DS_PI + 1.5f * F3DS_PI * t / 8.0f);
			annulus(m, cx, cy, iz - 0.003f, F3DS_DIAL_RADIUS - 0.017f, F3DS_DIAL_RADIUS - 0.004f,
				a - 0.035f, 0.07f, 1,
				/* "+X" dial is the speedometer, "-X" the tachometer. */
				dial < 0 && t >= 7 ? RGB(1.0f, 0.20f, 0.12f) : RGB(0.80f, 0.90f, 0.95f));
		}
	}
	m->cars[car].cockpitTrim = since(m, start);

	start = m->count;
	const Color rim = RGB(0.05f, 0.05f, 0.06f);
	annulus(m, 0.0f, 0.0f, 0.0f, 0.145f, 0.172f, 0.0f, 2.0f * F3DS_PI, 24, rim);
	annulus(m, 0.0f, 0.0f, -0.001f, 0.150f, 0.172f, -0.12f, 0.24f, 2, RGB(0.95f, 0.30f, 0.15f));
	/* Spokes low on the wheel so the dials stay readable. */
	panel(m, -0.150f, -0.018f, -0.040f, 0.006f, -0.002f, rim);
	panel(m, 0.040f, -0.018f, 0.150f, 0.006f, -0.002f, rim);
	panel(m, -0.018f, -0.150f, 0.018f, -0.040f, -0.002f, rim);
	annulus(m, 0.0f, 0.0f, -0.003f, 0.0f, 0.045f, 0.0f, 2.0f * F3DS_PI, 12, RGB(0.20f, 0.20f, 0.23f));
	m->cars[car].steering = since(m, start);
}

static void shared_parts(F3DSMeshes *m)
{
	const int seg = 12;
	uint32_t start = m->count;
	for (int i = 0; i < seg; ++i) {
		const float a0 = (float)i / seg * 2.0f * F3DS_PI, a1 = (float)(i + 1) / seg * 2.0f * F3DS_PI;
		const FVec3 o0 = fvec3(0, cosf(a0), sinf(a0)), o1 = fvec3(0, cosf(a1), sinf(a1));
		quad(m, fvec3(-1, o0.y, o0.z), fvec3(1, o0.y, o0.z), fvec3(1, o1.y, o1.z), fvec3(-1, o1.y, o1.z),
			fvec3_add(o0, o1), RGB(0.10f, 0.10f, 0.11f), true);
		for (int s = -1; s <= 1; s += 2)
			quad(m, fvec3((float)s, o0.y * 0.66f, o0.z * 0.66f), fvec3((float)s, o0.y, o0.z),
				fvec3((float)s, o1.y, o1.z), fvec3((float)s, o1.y * 0.66f, o1.z * 0.66f),
				fvec3((float)s, 0, 0), RGB(0.06f, 0.06f, 0.07f), false);
	}
	m->tyre = since(m, start);

	start = m->count;
	for (int i = 0; i < seg; ++i) {
		const float a0 = (float)i / seg * 2.0f * F3DS_PI, a1 = (float)(i + 1) / seg * 2.0f * F3DS_PI;
		/* Alternating spokes make the rotation visible. */
		const Color c = (i % 3 == 0) ? RGB(0.28f, 0.28f, 0.28f) : WHITE;
		for (int s = -1; s <= 1; s += 2)
			tri(m, fvec3((float)s, 0, 0), fvec3((float)s, cosf(a0) * 0.66f, sinf(a0) * 0.66f),
				fvec3((float)s, cosf(a1) * 0.66f, sinf(a1) * 0.66f), fvec3((float)s, 0, 0), c, false);
	}
	m->rim = since(m, start);

	start = m->count;
	tri(m, fvec3(-0.09f, -0.18f, 0), fvec3(0.09f, -0.18f, 0), fvec3(0.0f, 1.0f, 0),
		fvec3(0, 0, -1), RGB(1.0f, 0.35f, 0.10f), false);
	m->needle = since(m, start);

	start = m->count;
	for (int i = 0; i < 8; ++i) {
		const float a0 = (i + 0.5f) / 8.0f * 2.0f * F3DS_PI, a1 = (i + 1.5f) / 8.0f * 2.0f * F3DS_PI;
		tri(m, fvec3(0, 0, 0), fvec3(cosf(a0) * 1.08f, 0, sinf(a0) * 1.08f),
			fvec3(cosf(a1) * 1.08f, 0, sinf(a1) * 1.08f), fvec3(0, 1, 0),
			RGB(0.08f, 0.10f, 0.09f), false);
	}
	m->shadow = since(m, start);
}

bool meshes_build(F3DSMeshes *m, F3DSVertex *buffer, uint32_t capacity,
	const F3DSTrack *track, const F3DSTerrain *terrain)
{
	memset(m, 0, sizeof(*m));
	if (!buffer || !track || !terrain)
		return false;
	m->data = buffer;
	m->capacity = capacity;
	world(m, track, terrain);
	garage(m, track->pts[0]);
	for (int car = 0; car < F3DS_CAR_COUNT; ++car) {
		car_exterior(m, car);
		car_cockpit(m, car);
	}
	shared_parts(m);
	return !m->overflow;
}
