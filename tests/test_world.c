#include "test.h"
#include <math.h>
#include <string.h>
#include "catalog.h"
#include "physics.h"

#define DT (1.0f / 60.0f)

static F3DSTrack track;
static F3DSTerrain terrain;

static float heading_of(FVec3 f) { return atan2f(f.x, f.z); }

static void reset(F3DSVehicleDef *d, F3DSVehicleState *car, int id, FVec3 pos, float yaw)
{
	catalog_def(id, d);
	memset(car, 0, sizeof(*car));
	vehicle_state_reset(car, d, pos, yaw);
}

static void layout(void)
{
	CHECK(track.pts[0].x == track.pts[TRACK_POINTS].x && track.pts[0].z == track.pts[TRACK_POINTS].z);
	CHECK(track.obstacleCount > 10 && track.wallCount > 4 && track.treeCount > 100);
	CHECK(terrain.rampCount > 0 && terrain.slickCount > 0);
	int seg = -1;
	float along = -1.0f;
	CHECK(track_nearest(&track, track.pts[0], &seg, &along) < 1e-3f);
	for (int i = 0; i < TRACK_POINTS; ++i) {
		const float r = sqrtf(track.pts[i].x * track.pts[i].x + track.pts[i].z * track.pts[i].z);
		CHECK(r + 10.0f < track_coast_radius(track.pts[i].x, track.pts[i].z));
	}
	for (int i = 0; i < track.treeCount; ++i)
		CHECK(track_nearest(&track, fvec3(track.trees[i].x, 0, track.trees[i].z), NULL, NULL) >
			TRACK_HALF_WIDTH + 3.0f);

	/* The centre of the road is free everywhere, for the widest car. */
	for (int i = 0; i < TRACK_POINTS; ++i) {
		for (int k = 0; k < 4; ++k) {
			for (int off = -1; off <= 1; ++off) {
				F3DSVehicleDef d;
				F3DSVehicleState car;
				FVec3 p = fvec3_add(track.pts[i], fvec3_scale(track.fwd[i], track.segLen[i] * k / 4.0f));
				p = fvec3_add(p, fvec3_scale(track.right[i], off * 1.0f));
				reset(&d, &car, 3, p, heading_of(track.fwd[i]));
				CHECKF(!collision_resolve(&d, &car, &track), "segment %d.%d offset %d", i, k, off);
			}
		}
	}
	CHECK(terrain_surface(&terrain, &track, track.pts[0].x, track.pts[0].z) == F3DS_SURFACE_ASPHALT);
	CHECK(terrain_surface(&terrain, &track, 0.0f, 0.0f) == F3DS_SURFACE_GRASS);
	CHECK(terrain_surface(&terrain, &track, terrain.slicks[0].x, terrain.slicks[0].z) == F3DS_SURFACE_GRAVEL);
	CHECK(terrain_surface(NULL, NULL, 0.0f, 0.0f) == F3DS_SURFACE_ASPHALT);
}

/* Drive straight at a target from "distance" metres away and report how close
 * the car centre ever gets. */
static float charge(FVec3 target, FVec3 from, float speed, float *impact)
{
	F3DSVehicleDef d;
	F3DSVehicleState car;
	const FVec3 dir = fvec3_sub(target, from);
	const float len = fvec3_len(dir);
	reset(&d, &car, 0, from, atan2f(dir.x, dir.z));
	car.vx = dir.x / len * speed;
	car.vz = dir.z / len * speed;
	F3DSInputState in;
	memset(&in, 0, sizeof(in));
	in.throttle = 1.0f;
	car.gear = 2;
	float closest = 1e9f;
	*impact = 0.0f;
	for (int i = 0; i < 90; ++i) {
		physics_step(&d, &car, &in, &track, NULL, DT);
		*impact = fmaxf(*impact, car.impact);
		const float along = ((car.pos.x - from.x) * dir.x + (car.pos.z - from.z) * dir.z) / len;
		closest = fminf(closest, len - along);
	}
	CHECK(isfinite(car.pos.x) && isfinite(car.pos.z) && isfinite(car.yawRate));
	return closest;
}

static void collisions(void)
{
	float impact;
	/* Barriers: approached from the road side at up to 200 km/h. */
	for (int i = 0; i < track.wallCount; i += 3) {
		const F3DSWall *w = &track.walls[i];
		const FVec3 mid = fvec3_scale(fvec3_add(w->a, w->b), 0.5f);
		int seg;
		float s;
		track_nearest(&track, mid, &seg, &s);
		const FVec3 road = fvec3_add(track.pts[seg], fvec3_scale(track.fwd[seg], s));
		const FVec3 out = fvec3_sub(mid, road);
		const FVec3 from = fvec3_sub(mid, fvec3_scale(out, 4.5f / fvec3_len(out)));
		const float speed = i % 2 ? 55.0f : 12.0f;
		const float closest = charge(mid, from, speed, &impact);
		CHECKF(closest > 0.5f && impact > 3.0f, "wall %d: %f impact %f", i, closest, impact);
	}
	/* Boxes and trees stop the car. */
	for (int i = 0; i < track.obstacleCount; ++i) {
		const F3DSObstacle *o = &track.obstacles[i];
		int seg;
		float s;
		track_nearest(&track, o->center, &seg, &s);
		const FVec3 from = fvec3_sub(o->center, fvec3_scale(track.fwd[seg], 9.0f));
		const float closest = charge(o->center, from, 15.0f, &impact);
		CHECKF(impact > 2.0f, "obstacle %d: impact %f closest %f", i, impact, closest);
	}
	int stopped = 0;
	for (int i = 0; i < 20; ++i) {
		const F3DSTree *t = &track.trees[i];
		const FVec3 target = fvec3(t->x, 0.0f, t->z);
		charge(target, fvec3(t->x, 0.0f, t->z - 4.0f), 15.0f, &impact);
		stopped += impact > 2.0f;
	}
	CHECKF(stopped >= 19, "%d/20 trees stop the car", stopped);

	/* The coast keeps the car on the island. */
	for (int k = 0; k < 12; ++k) {
		const float a = k * F3DS_PI / 6.0f;
		F3DSVehicleDef d;
		F3DSVehicleState car;
		reset(&d, &car, 3, fvec3(cosf(a) * 215.0f, 0, sinf(a) * 215.0f), atan2f(cosf(a), sinf(a)));
		car.vx = cosf(a) * 70.0f;
		car.vz = sinf(a) * 70.0f;
		F3DSInputState in;
		memset(&in, 0, sizeof(in));
		in.throttle = 1.0f;
		for (int i = 0; i < 240; ++i) physics_step(&d, &car, &in, &track, &terrain, DT);
		const float r = sqrtf(car.pos.x * car.pos.x + car.pos.z * car.pos.z);
		CHECKF(r <= track_coast_radius(car.pos.x, car.pos.z), "k=%d r=%f", k, r);
	}
}

static void ramps(void)
{
	for (int i = 0; i < terrain.rampCount; ++i) {
		const F3DSTerrainRamp *r = &terrain.ramps[i];
		F3DSVehicleDef d;
		F3DSVehicleState car;
		reset(&d, &car, 0, fvec3_sub(r->origin, fvec3_scale(r->fwd, 4.0f)), heading_of(r->fwd));
		car.vx = r->fwd.x * 22.0f;
		car.vz = r->fwd.z * 22.0f;
		car.gear = 3;
		F3DSInputState in;
		memset(&in, 0, sizeof(in));
		in.throttle = 0.6f;
		float top = 0.0f;
		bool flew = false;
		for (int k = 0; k < 180; ++k) {
			physics_step(&d, &car, &in, &track, &terrain, DT);
			top = fmaxf(top, car.pos.y);
			flew |= car.airborne;
		}
		CHECKF(top > r->height * 0.9f && top < r->height + 3.0f, "ramp %d top %f", i, top);
		CHECK(flew && !car.airborne && car.pos.y < 0.05f);

		/* Hitting the side is a wall, not a teleport on top. */
		const FVec3 mid = fvec3_add(r->origin, fvec3_scale(r->fwd, r->length * 0.8f));
		const FVec3 side = fvec3_add(mid, fvec3_scale(r->right, r->width * 0.5f + 2.5f));
		reset(&d, &car, 0, side, atan2f(-r->right.x, -r->right.z));
		car.vx = -r->right.x * 8.0f;
		car.vz = -r->right.z * 8.0f;
		memset(&in, 0, sizeof(in));
		float high = 0.0f;
		for (int k = 0; k < 60; ++k) {
			physics_step(&d, &car, &in, &track, &terrain, DT);
			high = fmaxf(high, car.pos.y);
		}
		CHECKF(high < 0.5f, "side climb %f", high);
	}
}

/* A simple driver follows the centre line for a whole lap: steering sign,
 * road clearance, ramps and gearbox all have to work together. */
static void lap(void)
{
	for (int id = 0; id < F3DS_CAR_COUNT; ++id) {
		F3DSVehicleDef d;
		F3DSVehicleState car;
		reset(&d, &car, id, track.pts[0], heading_of(track.fwd[0]));
		F3DSInputState in;
		memset(&in, 0, sizeof(in));
		in.absEnabled = in.tcsEnabled = 1;
		int seg = 0, progress = 0, hits = 0;
		float worst = 0.0f;
		int frame = 0;
		for (; frame < 60 * 240 && progress < TRACK_POINTS; ++frame) {
			int now;
			float s;
			const float off = track_nearest(&track, car.pos, &now, &s);
			worst = fmaxf(worst, off);
			const int step = (now - seg + TRACK_POINTS) % TRACK_POINTS;
			if (step > 0 && step < 6) {
				progress += step;
				seg = now;
			}
			/* Aim a speed-dependent distance ahead on the centre line. */
			const float look = 8.0f + fabsf(car.speed) * 0.45f;
			int k = now;
			float left = look - (track.segLen[k] - s);
			while (left > 0.0f) {
				k = (k + 1) % TRACK_POINTS;
				left -= track.segLen[k];
			}
			const FVec3 aim = fvec3_add(track.pts[k + 1], fvec3_scale(track.fwd[k], left));
			const float want = atan2f(aim.x - car.pos.x, aim.z - car.pos.z);
			const float err = wrap_angle(want - car.yaw); /* positive = to the left */
			in.steer = clampf(-err * 2.2f, -1.0f, 1.0f);
			/* Slow down for the bends coming up. */
			float bend = 0.0f;
			for (int a = 1; a <= 3; ++a)
				bend += fabsf(wrap_angle(heading_of(track.fwd[(now + a) % TRACK_POINTS]) -
					heading_of(track.fwd[(now + a - 1) % TRACK_POINTS])));
			const float target = 22.0f / (1.0f + bend * 0.8f + fabsf(err));
			in.throttle = car.speed < target ? 0.7f : 0.0f;
			in.brake = car.speed > target + 4.0f ? 0.5f : 0.0f;
			const bool flying = car.airborne;
			physics_step(&d, &car, &in, &track, &terrain, DT);
			hits += !flying && car.impact > 2.0f;
		}
		printf("  car %d lap: %.1f s, max offset %.1f m, %d hard contacts\n", id, frame * DT, worst, hits);
		CHECKF(progress >= TRACK_POINTS, "car %d only reached %d/%d", id, progress, TRACK_POINTS);
		CHECKF(worst < TRACK_HALF_WIDTH && hits == 0, "car %d offset %f hits %d", id, worst, hits);
	}
}

void test_world(void)
{
	track_init(&track);
	terrain_init(&terrain, &track);
	layout();
	collisions();
	ramps();
	lap();
}
