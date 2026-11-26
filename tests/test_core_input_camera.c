#include "test.h"
#include <math.h>
#include <string.h>
#include "camera.h"
#include "core.h"

static void fixed_step(void)
{
	F3DSLoopState s;
	core_loop_init(&s, 1.0f / 60.0f, 6.0f / 60.0f);
	CHECK(core_loop_advance(&s, 1.0f / 120.0f) == 0);
	CHECK(fabsf(core_loop_alpha(&s) - 0.5f) < 0.01f);
	CHECK(core_loop_advance(&s, 1.0f / 120.0f) == 1);
	CHECK(core_loop_advance(&s, 1.0f) == 6); /* catch-up is capped */
	CHECK(s.accumulator < s.dt);
	CHECK(core_loop_advance(&s, -1.0f) == 0);
	CHECK(core_loop_advance(&s, NAN) == 0);
	/* 30 FPS on the real 59.83 Hz display: two steps every frame, no judder. */
	core_loop_init(&s, 1.0f / 60.0f, 6.0f / 60.0f);
	int steps = 0, irregular = 0;
	for (int i = 0; i < 300; ++i) {
		const int n = core_loop_advance(&s, 2.0f / 59.834f);
		steps += n;
		irregular += n != 2;
	}
	CHECKF(irregular < 6 && steps >= 600 && steps <= 606, "irregular=%d steps=%d", irregular, steps);
}

static F3DSRawInput raw_none(void)
{
	F3DSRawInput r;
	memset(&r, 0, sizeof(r));
	return r;
}

static void input_mapping(void)
{
	F3DSInputConfig cfg;
	input_config_defaults(&cfg);
	CHECK(input_config_valid(&cfg));
	F3DSInputState in;
	F3DSRawInput raw = raw_none();
	raw.circleX = (int)(150 * cfg.deadzone) - 1;
	input_map(&in, &cfg, &raw);
	CHECK(in.steer == 0.0f);
	float previous = 0.0f;
	for (int x = 20; x <= 150; x += 10) {
		raw.circleX = x;
		input_map(&in, &cfg, &raw);
		CHECK(in.steer > previous && in.steer <= 1.0f);
		const float right = in.steer;
		raw.circleX = -x;
		input_map(&in, &cfg, &raw);
		CHECK(fabsf(in.steer + right) < 1e-6f);
		previous = right;
	}
	raw.circleX = 75;
	cfg.steerSensitivity = 0.5f;
	input_map(&in, &cfg, &raw);
	const float low = in.steer;
	cfg.steerSensitivity = 2.0f;
	input_map(&in, &cfg, &raw);
	CHECK(in.steer > low * 3.0f);

	/* Rebinding swaps instead of duplicating. */
	input_config_defaults(&cfg);
	input_rebind(&cfg, F3DS_ACT_THROTTLE, KEY_B);
	CHECK(cfg.keys[F3DS_ACT_THROTTLE] == KEY_B && cfg.keys[F3DS_ACT_BRAKE] == KEY_A);
	CHECK(input_config_valid(&cfg));
	input_rebind(&cfg, F3DS_ACT_THROTTLE, KEY_START); /* reserved */
	CHECK(cfg.keys[F3DS_ACT_THROTTLE] == KEY_B);
	input_rebind(&cfg, F3DS_ACT_HANDBRAKE, KEY_ZR);
	raw = raw_none();
	raw.held = KEY_ZR | KEY_B;
	input_map(&in, &cfg, &raw);
	CHECK(in.handbrake && in.throttle == 1.0f && in.brake == 0.0f);
}

static void camera(void)
{
	F3DSCamera cam;
	camera_init(&cam);
	const F3DSCarVisual *car = catalog_visual(0);
	F3DSInputState in;
	memset(&in, 0, sizeof(in));
	const FVec3 pos = fvec3(10.0f, 0.0f, 20.0f);
	camera_update(&cam, car, pos, 0.0f, 0.0f, &in, 0.016f);
	CHECK(fabsf(cam.eye.x - 10.0f) < 1e-3f && cam.eye.z < 20.0f - 7.0f);
	CHECK(fabsf(cam.eye.y - cam.chaseHeight) < 1e-3f);

	/* Orbit while stopped is kept: the player can look around the car. */
	in.orbit = 1.0f;
	for (int i = 0; i < 30; ++i) camera_update(&cam, car, pos, 0.0f, 0.0f, &in, 0.016f);
	const float angle = cam.orbitYaw;
	CHECK(fabsf(angle) > 0.8f);
	CHECKF(cam.eye.x > pos.x + 1.0f || cam.eye.x < pos.x - 1.0f, "eye.x=%f", cam.eye.x);
	CHECK(fabsf(cam.eye.y - cam.chaseHeight) < 1e-3f); /* horizontal only */
	in.orbit = 0.0f;
	for (int i = 0; i < 120; ++i) camera_update(&cam, car, pos, 0.0f, 0.0f, &in, 0.016f);
	CHECK(fabsf(cam.orbitYaw - angle) < 1e-5f);
	/* Setting off recentres. */
	for (int i = 0; i < 200; ++i) camera_update(&cam, car, pos, 0.0f, 5.0f, &in, 0.016f);
	CHECK(fabsf(cam.orbitYaw) < 0.01f);
	/* Releasing the stick while driving recentres. */
	in.orbit = -1.0f;
	for (int i = 0; i < 30; ++i) camera_update(&cam, car, pos, 0.0f, 20.0f, &in, 0.016f);
	CHECK(fabsf(cam.orbitYaw) > 0.8f);
	in.orbit = 0.0f;
	for (int i = 0; i < 200; ++i) camera_update(&cam, car, pos, 0.0f, 20.0f, &in, 0.016f);
	CHECK(fabsf(cam.orbitYaw) < 0.01f);
	/* A full turn stays wrapped. */
	in.orbit = 1.0f;
	for (int i = 0; i < 1000; ++i) camera_update(&cam, car, pos, 0.0f, 0.0f, &in, 0.016f);
	CHECK(fabsf(cam.orbitYaw) <= F3DS_PI + 1e-4f);

	/* Distance and height are adjustable and bounded. */
	memset(&in, 0, sizeof(in));
	in.camDistance = 1;
	in.camHeight = -1;
	for (int i = 0; i < 1000; ++i) camera_update(&cam, car, pos, 0.0f, 0.0f, &in, 0.016f);
	CHECK(cam.chaseDistance == 20.0f && cam.chaseHeight == 1.0f);

	/* Cockpit: right-hand drive eye, independent of the orbit settings. */
	memset(&in, 0, sizeof(in));
	in.toggleView = 1;
	camera_update(&cam, car, pos, 0.0f, 0.0f, &in, 0.016f);
	CHECK(cam.mode == F3DS_CAM_COCKPIT);
	CHECK(fabsf(cam.eye.x - (pos.x + car->seatX)) < 1e-4f);
	CHECK(fabsf(cam.eye.y - car->eyeY) < 1e-4f);
	CHECK(fabsf(cam.eye.z - (pos.z + car->eyeZ)) < 1e-4f);
	CHECK(cam.target.z > cam.eye.z);
}

void test_core_input_camera(void)
{
	fixed_step();
	input_mapping();
	camera();
}
