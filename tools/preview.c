/* Host preview: draws both screens of chosen game states into PPM images with
 * a small software rasteriser that follows the PICA200 conventions used by
 * renderer.c (back-face culling, depth near=1/far=0, greater-or-equal).
 * It checks geometry, camera and layout; it is not a hardware emulator. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "scene.h"
#include "screens.h"

#define W 400
#define H 240

static F3DSVertex s_buffer[F3DS_MESH_CAPACITY];
static F3DSMeshes s_meshes;
static F3DSScene s_scene;
static F3DSApp s_app;
static uint8_t s_color[H][W][3];
static float s_depth[H][W];
static uint8_t s_font[256 * 8];
static uint16_t s_lower[CANVAS_PIXELS];

typedef struct { float x, y, z, w, r, g, b; } Clip;

static Clip lerp_clip(Clip a, Clip b, float t)
{
	Clip c;
	const float *pa = &a.x, *pb = &b.x;
	float *pc = &c.x;
	for (int i = 0; i < 7; ++i) pc[i] = pa[i] + (pb[i] - pa[i]) * t;
	return c;
}

/* plane 0: z + w >= 0 (near); plane 1: -z >= 0 (far). */
static int clip_plane(const Clip *in, int n, Clip *out, int plane)
{
	int m = 0;
	for (int i = 0; i < n; ++i) {
		const Clip a = in[i], b = in[(i + 1) % n];
		const float da = plane ? -a.z : a.z + a.w;
		const float db = plane ? -b.z : b.z + b.w;
		if (da >= 0.0f) out[m++] = a;
		if ((da >= 0.0f) != (db >= 0.0f)) out[m++] = lerp_clip(a, b, da / (da - db));
	}
	return m;
}

static float edge(float ax, float ay, float bx, float by, float px, float py)
{
	return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

static void raster(Clip a, Clip b, Clip c)
{
	const Clip v[3] = { a, b, c };
	float sx[3], sy[3], d[3];
	for (int i = 0; i < 3; ++i) {
		const float iw = 1.0f / v[i].w;
		sx[i] = (-v[i].y * iw * 0.5f + 0.5f) * W; /* right */
		sy[i] = (v[i].x * iw * 0.5f + 0.5f) * H;  /* up */
		d[i] = -v[i].z * iw;
	}
	const float area = edge(sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
	if (area <= 0.0f)
		return; /* back face */
	int x0 = (int)floorf(fminf(sx[0], fminf(sx[1], sx[2])));
	int x1 = (int)ceilf(fmaxf(sx[0], fmaxf(sx[1], sx[2])));
	int y0 = (int)floorf(fminf(sy[0], fminf(sy[1], sy[2])));
	int y1 = (int)ceilf(fmaxf(sy[0], fmaxf(sy[1], sy[2])));
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > W) x1 = W;
	if (y1 > H) y1 = H;
	for (int y = y0; y < y1; ++y) {
		for (int x = x0; x < x1; ++x) {
			const float px = x + 0.5f, py = y + 0.5f;
			const float l0 = edge(sx[1], sy[1], sx[2], sy[2], px, py) / area;
			const float l1 = edge(sx[2], sy[2], sx[0], sy[0], px, py) / area;
			const float l2 = 1.0f - l0 - l1;
			if (l0 < 0.0f || l1 < 0.0f || l2 < -1e-5f) continue;
			const float depth = l0 * d[0] + l1 * d[1] + l2 * d[2];
			const int row = H - 1 - y;
			if (depth < s_depth[row][x]) continue;
			s_depth[row][x] = depth;
			s_color[row][x][0] = (uint8_t)clampf((l0 * v[0].r + l1 * v[1].r + l2 * v[2].r) * 255.0f, 0, 255);
			s_color[row][x][1] = (uint8_t)clampf((l0 * v[0].g + l1 * v[1].g + l2 * v[2].g) * 255.0f, 0, 255);
			s_color[row][x][2] = (uint8_t)clampf((l0 * v[0].b + l1 * v[1].b + l2 * v[2].b) * 255.0f, 0, 255);
		}
	}
}

static void render_top(void)
{
	F3DSView view;
	app_view(&s_app, &view);
	scene_build(&s_scene, &s_meshes, &view);
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x) {
			s_color[y][x][0] = s_scene.clearColor >> 24;
			s_color[y][x][1] = s_scene.clearColor >> 16;
			s_color[y][x][2] = s_scene.clearColor >> 8;
			s_depth[y][x] = 0.0f;
		}
	for (int i = 0; i < s_scene.count; ++i) {
		const F3DSDraw *dr = &s_scene.draws[i];
		for (uint32_t k = 0; k + 2 < dr->range.count; k += 3) {
			Clip tri[3], a[8], b[8];
			for (int j = 0; j < 3; ++j) {
				const F3DSVertex *p = &s_buffer[dr->range.start + k + j];
				const C3D_FVec v = FVec4_New(p->x, p->y, p->z, 1.0f);
				tri[j] = (Clip){ FVec4_Dot(dr->mvp.r[0], v), FVec4_Dot(dr->mvp.r[1], v),
					FVec4_Dot(dr->mvp.r[2], v), FVec4_Dot(dr->mvp.r[3], v),
					p->r / 255.0f * dr->tint[0], p->g / 255.0f * dr->tint[1],
					p->b / 255.0f * dr->tint[2] };
			}
			int n = clip_plane(tri, 3, a, 0);
			n = clip_plane(a, n, b, 1);
			for (int j = 1; j + 1 < n; ++j)
				raster(b[0], b[j], b[j + 1]);
		}
	}
}

/* Both screens stacked, as on the console (lower screen centred). */
static void save(const char *dir, const char *name)
{
	render_top();
	F3DSCanvas canvas = { s_lower, s_font, 0 };
	screens_draw(&canvas, &s_app);
	char path[512];
	snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
	FILE *f = fopen(path, "wb");
	if (!f) { perror(path); exit(1); }
	fprintf(f, "P6\n%d %d\n255\n", W, H * 2 + 4);
	fwrite(s_color, 1, sizeof(s_color), f);
	for (int i = 0; i < W * 4 * 3; ++i) fputc(0, f);
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x) {
			uint8_t rgb[3] = { 0, 0, 0 };
			const int cx = x - (W - CANVAS_W) / 2;
			if (cx >= 0 && cx < CANVAS_W) {
				const uint16_t c = s_lower[canvas_index(cx, y)];
				rgb[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
				rgb[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
				rgb[2] = (uint8_t)((c & 31) * 255 / 31);
			}
			fwrite(rgb, 1, 3, f);
		}
	fclose(f);
	printf("%-16s %3d draws %6u vertices, clipped text %d\n", name, s_scene.count,
		s_scene.vertices, canvas.clipped);
}

static void frames(int n, u32 held, int circle)
{
	for (int i = 0; i < n; ++i) {
		F3DSRawInput raw;
		memset(&raw, 0, sizeof(raw));
		raw.held = held;
		raw.circleX = circle;
		app_update(&s_app, &raw, 1.0f / 30.0f);
	}
}

/* Put the car on the road at a track point, with the camera behind it. */
static void drive_at(int point, F3DSCameraMode mode)
{
	s_app.screen = F3DS_SCREEN_DRIVE;
	s_app.mapVisible = false;
	const float yaw = atan2f(s_app.track.fwd[point].x, s_app.track.fwd[point].z);
	vehicle_state_reset(&s_app.car, &s_app.garage.def[s_app.garage.selected],
		s_app.track.pts[point], yaw);
	s_app.car.pos.y = terrain_height(&s_app.terrain, s_app.car.pos.x, s_app.car.pos.z);
	s_app.prevPos = s_app.car.pos;
	s_app.prevYaw = yaw;
	s_app.camera.mode = mode;
	s_app.camera.orbitYaw = 0.0f;
	frames(2, 0, 0);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: preview <default_font.bin> <output-dir>\n");
		return 2;
	}
	FILE *f = fopen(argv[1], "rb");
	if (!f || fread(s_font, 1, sizeof(s_font), f) != sizeof(s_font)) {
		fprintf(stderr, "cannot read font\n");
		return 1;
	}
	fclose(f);
	const char *out = argv[2];
	char data[512];
	snprintf(data, sizeof(data), "%s/data", out);
	app_init(&s_app, data);
	if (!meshes_build(&s_meshes, s_buffer, F3DS_MESH_CAPACITY, &s_app.track, &s_app.terrain))
		return 1;

	frames(1, 0, 0);
	save(out, "menu");
	for (int car = 0; car < F3DS_CAR_COUNT; ++car) {
		char name[32];
		s_app.screen = F3DS_SCREEN_CARS;
		s_app.selected = s_app.garage.selected = car;
		vehicle_state_reset(&s_app.car, &s_app.garage.def[car], s_app.track.pts[0], 0.0f);
		s_app.inspectAngle = 0.7f;
		snprintf(name, sizeof(name), "car%d_front", car);
		save(out, name);
		s_app.inspectAngle = 3.9f;
		s_app.screen = F3DS_SCREEN_WORKSHOP;
		s_app.selected = 0;
		snprintf(name, sizeof(name), "car%d_rear", car);
		save(out, name);
		drive_at(0, F3DS_CAM_CHASE);
		frames(75, KEY_A, 0);
		snprintf(name, sizeof(name), "car%d_chase", car);
		save(out, name);
		drive_at(0, F3DS_CAM_COCKPIT);
		frames(60, KEY_A, 0);
		frames(6, KEY_A, -110);
		snprintf(name, sizeof(name), "car%d_cockpit", car);
		save(out, name);
	}
	s_app.garage.selected = 0;
	for (int i = 0; i < TRACK_POINTS; i += TRACK_POINTS / 8) {
		char name[32];
		drive_at(i, F3DS_CAM_CHASE);
		snprintf(name, sizeof(name), "track%03d", i);
		save(out, name);
	}
	for (int r = 0; r < s_app.terrain.rampCount; ++r) {
		char name[32];
		drive_at(0, F3DS_CAM_CHASE);
		const F3DSTerrainRamp *ramp = &s_app.terrain.ramps[r];
		const FVec3 p = ramp->origin, fw = ramp->fwd;
		const float yaw = atan2f(fw.x, fw.z);
		vehicle_state_reset(&s_app.car, &s_app.garage.def[0], p, yaw);
		s_app.car.pos = fvec3(p.x - fw.x * 8.0f, 0.0f, p.z - fw.z * 8.0f);
		s_app.car.pos.y = terrain_height(&s_app.terrain, s_app.car.pos.x, s_app.car.pos.z);
		s_app.prevPos = s_app.car.pos;
		s_app.prevYaw = yaw;
		s_app.camera.chaseHeight = 4.0f;
		frames(1, 0, 0);
		snprintf(name, sizeof(name), "ramp%d", r);
		save(out, name);
	}
	s_app.screen = F3DS_SCREEN_PAUSE; s_app.selected = 0; save(out, "pause");
	s_app.mapVisible = true; save(out, "map"); s_app.mapVisible = false;
	s_app.screen = F3DS_SCREEN_SETTINGS; s_app.settingsReturn = F3DS_SCREEN_PAUSE;
	s_app.selected = 0; save(out, "settings");
	s_app.rebindAction = 0; save(out, "rebind"); s_app.rebindAction = -1;
	s_app.screen = F3DS_SCREEN_WORKSHOP_PAGE;
	for (int p = 0; p < F3DS_PAGE_COUNT; ++p) {
		char name[32];
		s_app.page = (F3DSWorkshopPage)p;
		s_app.selected = 1;
		snprintf(name, sizeof(name), "page%d", p);
		save(out, name);
	}
	s_app.screen = F3DS_SCREEN_PROGRESSION; save(out, "progression");
	s_app.screen = F3DS_SCREEN_CONFIRM_QUIT; save(out, "quit");
	return 0;
}
