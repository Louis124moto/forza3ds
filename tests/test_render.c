#include "test.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "canvas.h"
#include "scene.h"

static F3DSVertex s_buffer[F3DS_MESH_CAPACITY];
static F3DSMeshes s_meshes;
static F3DSScene s_scene;
static F3DSApp s_app;

static bool inside(F3DSRange r) { return r.count % 3 == 0 && r.start + r.count <= s_meshes.count; }

static bool contains(F3DSRange r)
{
	for (int i = 0; i < s_scene.count; ++i)
		if (s_scene.draws[i].range.start == r.start && s_scene.draws[i].range.count == r.count)
			return true;
	return false;
}

/* Screen position (-1..1, right and up) of a world point through a draw. */
static bool project(const C3D_Mtx *m, float x, float y, float z, float *right, float *up)
{
	const C3D_FVec v = FVec4_New(x, y, z, 1.0f);
	const C3D_FVec p = FVec4_New(FVec4_Dot(m->r[0], v), FVec4_Dot(m->r[1], v),
		FVec4_Dot(m->r[2], v), FVec4_Dot(m->r[3], v));
	if (p.w <= 0.0f) return false;
	*right = -p.y / p.w;
	*up = p.x / p.w;
	return true;
}

static void check_scene(void)
{
	CHECK(s_scene.count > 0 && s_scene.count < F3DS_SCENE_MAX_DRAWS);
	uint32_t total = 0;
	for (int i = 0; i < s_scene.count; ++i) {
		const F3DSDraw *d = &s_scene.draws[i];
		CHECK(d->range.count > 0 && inside(d->range));
		for (int k = 0; k < 16; ++k) CHECK(isfinite(d->mvp.m[k]));
		total += d->range.count;
	}
	CHECK(total == s_scene.vertices);
}

static void geometry(void)
{
	CHECK(meshes_build(&s_meshes, s_buffer, F3DS_MESH_CAPACITY, &s_app.track, &s_app.terrain));
	CHECK(!s_meshes.overflow && s_meshes.count > 5000);
	printf("  meshes: %u vertices (%u KiB of %u)\n", s_meshes.count,
		(unsigned)(s_meshes.count * sizeof(F3DSVertex) / 1024),
		(unsigned)(F3DS_MESH_CAPACITY * sizeof(F3DSVertex) / 1024));
	CHECK(inside(s_meshes.sea) && inside(s_meshes.horizon) && inside(s_meshes.garage));
	CHECK(inside(s_meshes.tyre) && inside(s_meshes.rim) && inside(s_meshes.needle) &&
		inside(s_meshes.shadow));
	for (int i = 0; i < F3DS_CAR_COUNT; ++i) {
		const F3DSCarMesh *c = &s_meshes.cars[i];
		CHECK(c->paint.count && c->trim.count && c->cockpitPaint.count && c->cockpitTrim.count &&
			c->steering.count);
		CHECK(inside(c->paint) && inside(c->trim) && inside(c->cockpitPaint) &&
			inside(c->cockpitTrim) && inside(c->steering));
		/* Bodies stay within the collision footprint. */
		const F3DSCarVisual *v = catalog_visual(i);
		int outside = 0;
		const F3DSRange ranges[2] = { c->paint, c->trim };
		for (int r = 0; r < 2; ++r)
			for (uint32_t k = 0; k < ranges[r].count; ++k) {
				const F3DSVertex *p = &s_buffer[ranges[r].start + k];
				if (fabsf(p->x) > v->halfWidth + 0.25f || fabsf(p->z) > v->halfLength + 0.2f ||
				    p->y < 0.0f || p->y > v->roofY + 0.35f)
					outside++;
			}
		CHECKF(outside == 0, "car %d: %d vertices", i, outside);
	}
	int broken = 0;
	for (uint32_t i = 0; i < s_meshes.count; ++i)
		if (!isfinite(s_buffer[i].x) || !isfinite(s_buffer[i].y) || !isfinite(s_buffer[i].z))
			broken++;
	CHECK(broken == 0);
	for (int i = 0; i < F3DS_CHUNK_COUNT; ++i) {
		const F3DSChunk *c = &s_meshes.chunks[i];
		CHECK(inside(c->range));
		int out = 0;
		for (uint32_t k = 0; k < c->range.count; ++k) {
			const F3DSVertex *p = &s_buffer[c->range.start + k];
			if (hypotf(p->x - c->cx, p->z - c->cz) > c->radius + 0.01f) out++;
		}
		CHECKF(out == 0, "chunk %d: %d", i, out);
	}
	static F3DSMeshes small;
	static F3DSVertex tiny[1000];
	CHECK(!meshes_build(&small, tiny, 1000, &s_app.track, &s_app.terrain));
	CHECK(small.count <= 1000);
}

static void scenes(void)
{
	F3DSView v;
	/* Garage. */
	app_view(&s_app, &v);
	scene_build(&s_scene, &s_meshes, &v);
	check_scene();
	CHECK(contains(s_meshes.garage) && !contains(s_meshes.sea));

	s_app.screen = F3DS_SCREEN_DRIVE;
	uint32_t worst[F3DS_DRAW_COUNT] = { 0 };
	for (int d = 0; d < F3DS_DRAW_COUNT; ++d) {
		for (int i = 0; i < TRACK_POINTS; ++i) {
			const float yaw = atan2f(s_app.track.fwd[i].x, s_app.track.fwd[i].z);
			s_app.car.pos = s_app.prevPos = s_app.track.pts[i];
			s_app.car.yaw = s_app.prevYaw = yaw;
			s_app.settings.drawDistance = d;
			s_app.camera.mode = F3DS_CAM_CHASE;
			F3DSInputState still;
			memset(&still, 0, sizeof(still));
			camera_update(&s_app.camera, catalog_visual(s_app.garage.selected), s_app.car.pos,
				yaw, 0.0f, &still, 0.0f);
			app_view(&s_app, &v);
			scene_build(&s_scene, &s_meshes, &v);
			if (i % 17 == 0) check_scene();
			if (s_scene.vertices > worst[d]) worst[d] = s_scene.vertices;
			if (i % 17) continue;
			/* The car is centred; the world to the driver's right shows right. */
			const C3D_Mtx *world = NULL;
			for (int k = 0; k < s_scene.count; ++k)
				if (s_scene.draws[k].range.start == s_meshes.sea.start)
					world = &s_scene.draws[k].mvp;
			CHECK(world != NULL);
			if (!world) continue;
			const FVec3 p = s_app.car.pos, f = yaw_forward(yaw);
			const FVec3 r = fvec3(-f.z, 0.0f, f.x);
			float sx = 9, sy = 9, rx = 9, ry = 9;
			CHECK(project(world, p.x, p.y + 0.6f, p.z, &sx, &sy));
			CHECKF(fabsf(sx) < 0.05f && fabsf(sy) < 0.7f, "%f %f", sx, sy);
			CHECK(project(world, p.x + r.x * 2.0f, p.y + 0.6f, p.z + r.z * 2.0f, &rx, &ry));
			CHECK(rx > sx + 0.05f);
		}
	}
	printf("  worst frame: %u / %u / %u vertices (near / medium / far)\n", worst[0], worst[1], worst[2]);
	CHECK(worst[0] <= worst[1] && worst[1] <= worst[2]);

	const F3DSCarMesh *car = &s_meshes.cars[s_app.garage.selected];
	app_view(&s_app, &v);
	scene_build(&s_scene, &s_meshes, &v);
	CHECK(contains(car->paint) && contains(car->trim) && contains(s_meshes.shadow));
	CHECK(!contains(car->steering));
	v.shadows = 0;
	scene_build(&s_scene, &s_meshes, &v);
	CHECK(!contains(s_meshes.shadow));

	s_app.camera.mode = F3DS_CAM_COCKPIT;
	F3DSInputState still;
	memset(&still, 0, sizeof(still));
	camera_update(&s_app.camera, catalog_visual(s_app.garage.selected), s_app.car.pos, s_app.car.yaw,
		0.0f, &still, 0.0f);
	app_view(&s_app, &v);
	scene_build(&s_scene, &s_meshes, &v);
	check_scene();
	CHECK(contains(car->steering) && contains(car->cockpitTrim) && !contains(car->trim));
	/* Both needles are on screen. */
	int needles = 0;
	for (int k = 0; k < s_scene.count; ++k) {
		if (s_scene.draws[k].range.start != s_meshes.needle.start) continue;
		float sx, sy;
		CHECK(project(&s_scene.draws[k].mvp, 0.0f, 0.0f, 0.0f, &sx, &sy));
		CHECKF(fabsf(sx) < 1.0f && fabsf(sy) < 1.0f, "needle at %f %f", sx, sy);
		needles++;
	}
	CHECK(needles == 2);
}

static void canvas(void)
{
	static uint16_t guard[CANVAS_PIXELS + 512];
	static uint8_t font[256 * 8];
	memset(font, 0xFF, sizeof(font));
	for (int i = 0; i < CANVAS_PIXELS + 512; ++i) guard[i] = 0xBEEF;
	F3DSCanvas c = { guard + 256, font, 0 };
	canvas_fill(&c, -50, -50, 500, 400, 1);
	canvas_line(&c, -900, -900, 900, 900, 5, 2);
	canvas_line(&c, 0, 0, INFINITY, 0, 1, 2);
	canvas_line(&c, NAN, 0, 1, 0, 1, 2);
	canvas_disc(&c, 0, 239, 30, 3);
	canvas_text(&c, 310, 236, 4, 4, "bord");
	CHECK(c.clipped > 0);
	int damaged = 0;
	for (int i = 0; i < 256; ++i)
		if (guard[i] != 0xBEEF || guard[CANVAS_PIXELS + 256 + i] != 0xBEEF) damaged++;
	CHECK(damaged == 0);
	CHECK(canvas_text_width("Réglage", 1) == 56);
}

void test_render(void)
{
	char dir[] = "/tmp/f3ds-render-XXXXXX";
	if (!mkdtemp(dir)) { CHECK(false); return; }
	app_init(&s_app, dir);
	geometry();
	scenes();
	canvas();
}
