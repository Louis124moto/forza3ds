#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "core.h"
#include "physics.h"
#include "terrain.h"
#include "track.h"
#include "vehicle.h"

u64 svcGetSystemTick(void) { return 0; }

static void test_fixed_timestep(void)
{
	F3DSLoopState s;
	core_loop_init(&s, 1.0f / 60.0f, 5.0f / 60.0f);
	assert(core_loop_advance(&s, 1.0f / 120.0f) == 0);
	assert(core_loop_advance(&s, 1.0f / 120.0f) == 1);
	assert(fabsf(s.simTime - 1.0f / 60.0f) < 1e-5f);
	assert(core_loop_advance(&s, 1.0f) == 5);
	assert(s.accumulator < s.dt);
	assert(core_loop_advance(&s, -1.0f) == 0);
	assert(core_loop_advance(&s, NAN) == 0);
}

static void test_track_and_terrain(void)
{
	F3DSTrack track;
	F3DSTerrain terrain;
	track_init(&track);
	terrain_init(&terrain, &track);
	assert(track.obstacleCount > 0);
	assert(fabsf(track.pts[0].x - track.pts[TRACK_POINTS].x) < 1e-5f);
	assert(fabsf(track.pts[0].z - track.pts[TRACK_POINTS].z) < 1e-5f);
	int seg = -1;
	float along = -1.0f;
	assert(track_nearest(&track, track.pts[0], &seg, &along) < 1e-4f);
	assert(seg == 0 && along >= 0.0f && along <= track.segLen[0]);
	assert(terrain.rampCount == 2 && terrain.slickCount == 3);
	assert(terrain_grip_factor(&terrain, track.pts[12].x, track.pts[12].z) < 1.0f);
	assert(terrain_grip_factor(&terrain, 0.0f, 0.0f) == 1.0f);
}

static void test_collision(void)
{
	F3DSTrack track;
	track_init(&track);
	FVec3 p = fvec3(track.pts[0].x + track.right[0].x * (TRACK_HALF_WIDTH + 2.0f),
			0.0f,
			track.pts[0].z + track.right[0].z * (TRACK_HALF_WIDTH + 2.0f));
	float vx = 10.0f, vz = 0.0f;
	assert(track_resolve_collision(&track, &p, &vx, &vz) == 1);
	assert(track_nearest(&track, p, NULL, NULL) <= TRACK_HALF_WIDTH + 1e-3f);
}

static void test_vehicle_physics(void)
{
	F3DSVehicleDef def;
	F3DSVehicleState car;
	F3DSInputState in;
	F3DSTrack track;
	F3DSTerrain terrain;
	vehicle_def_default(&def);
	track_init(&track);
	terrain_init(&terrain, &track);
	memset(&car, 0, sizeof(car));
	memset(&in, 0, sizeof(in));
	car.gear = 1;
	car.rpm = def.idleRpm;
	car.pos = track.pts[0];
	in.throttle = 1.0f;
	for (int i = 0; i < 120; ++i)
		physics_step(&def, &car, &in, &track, &terrain, 1.0f / 60.0f);
	assert(car.speed > 0.0f);
	assert(car.rpm >= def.idleRpm && car.rpm <= def.maxRpm);
	assert(isfinite(car.pos.x) && isfinite(car.pos.z));
	const float beforeBrake = car.speed;
	in.throttle = 0.0f;
	in.brake = 1.0f;
	for (int i = 0; i < 60; ++i)
		physics_step(&def, &car, &in, &track, &terrain, 1.0f / 60.0f);
	assert(car.speed < beforeBrake);
}

int main(void)
{
	test_fixed_timestep();
	test_track_and_terrain();
	test_collision();
	test_vehicle_physics();
	puts("Forza3DS deterministic engine tests: PASS");
	return 0;
}
