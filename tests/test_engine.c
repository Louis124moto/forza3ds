#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "core.h"
#include "physics.h"

u64 svcGetSystemTick(void) { return 0; }

static void test_fixed_timestep(void)
{
 F3DSLoopState s;
 core_loop_init(&s,1.0f/60.0f,5.0f/60.0f);
 assert(core_loop_advance(&s,1.0f/120.0f)==0);
 assert(core_loop_advance(&s,1.0f/120.0f)==1);
 assert(fabsf(s.simTime-1.0f/60.0f)<1e-5f);
 assert(core_loop_advance(&s,1.0f)==5);
 assert(s.accumulator<s.dt);
 assert(core_loop_advance(&s,-1.0f)==0);
 assert(core_loop_advance(&s,NAN)==0);
}

static void test_track_terrain_collision(void)
{
 F3DSTrack track; F3DSTerrain terrain;
 track_init(&track); terrain_init(&terrain,&track);
 assert(track.obstacleCount>0);
 assert(fabsf(track.pts[0].x-track.pts[TRACK_POINTS].x)<1e-5f);
 assert(fabsf(track.pts[0].z-track.pts[TRACK_POINTS].z)<1e-5f);
 int seg=-1; float along=-1.0f;
 assert(track_nearest(&track,track.pts[0],&seg,&along)<1e-4f);
 assert(seg==0 && along>=0.0f && along<=track.segLen[0]);
 assert(terrain.rampCount>0 && terrain.slickCount>0);
 assert(terrain_grip_factor(&terrain,0.0f,0.0f)>0.0f);
 FVec3 p=fvec3(track.pts[0].x+track.right[0].x*30.0f,0.0f,
               track.pts[0].z+track.right[0].z*30.0f);
 float vx=10.0f,vz=0.0f;
 assert(track_resolve_collision(&track,&p,&vx,&vz)==0);
 p=fvec3(320.0f,0.0f,0.0f);
 assert(track_resolve_collision(&track,&p,&vx,&vz)==1);
 assert(sqrtf(p.x*p.x+p.z*p.z)<260.0f);
 p=track.obstacles[0].center;
 assert(track_resolve_collision(&track,&p,&vx,&vz)==1);
}

static void test_vehicle_acceleration_and_braking(void)
{
 F3DSVehicleDef def; F3DSVehicleState car; F3DSInputState in;
 F3DSTrack track; F3DSTerrain terrain;
 vehicle_def_default(&def); track_init(&track); terrain_init(&terrain,&track);
 memset(&car,0,sizeof(car)); memset(&in,0,sizeof(in));
 car.gear=1; car.rpm=def.idleRpm; car.pos=track.pts[0]; in.throttle=1.0f;
 for(int i=0;i<120;i++) physics_step(&def,&car,&in,&track,&terrain,1.0f/60.0f);
 assert(car.speed>0.0f);
 assert(car.rpm>=def.idleRpm && car.rpm<=def.maxRpm);
 assert(isfinite(car.pos.x)&&isfinite(car.pos.z));
 float before=car.speed; in.throttle=0.0f; in.brake=1.0f;
 for(int i=0;i<60;i++) physics_step(&def,&car,&in,&track,&terrain,1.0f/60.0f);
 assert(car.speed<before);
}

static void test_tuning_changes_vehicle_response(void)
{
	F3DSVehicleDef stock, tuned;
	F3DSVehicleState a, b;
	F3DSInputState in;
	F3DSTrack track; F3DSTerrain terrain;
	vehicle_def_default(&stock);
	tuned = stock;
	tuned.turboInstalled = 1;
	tuned.turboBoostBar = 1.2f;
	tuned.intakeLevel = 2;
	tuned.exhaustLevel = 2;
	assert(vehicle_engine_torque(&tuned, 3500.0f) > vehicle_engine_torque(&stock, 3500.0f));
	assert(vehicle_sound_timbre(&tuned) > vehicle_sound_timbre(&stock));
	track_init(&track); terrain_init(&terrain, &track);
	memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b)); memset(&in, 0, sizeof(in));
	a.gear = b.gear = 1;
	a.rpm = b.rpm = stock.idleRpm;
	a.pos = b.pos = track.pts[0];
	in.throttle = 1.0f;
	for (int i = 0; i < 60; ++i) {
		physics_vehicle_step(&stock, &a, &in, &terrain, 1.0f / 60.0f);
		physics_vehicle_step(&tuned, &b, &in, &terrain, 1.0f / 60.0f);
	}
	assert(b.speed > a.speed);
	assert(isfinite(b.rpm) && isfinite(b.speed));
}

static void test_manual_gearbox(void)
{
	F3DSVehicleDef def;
	F3DSVehicleState car;
	F3DSInputState in;
	F3DSTerrain terrain;
	F3DSTrack track;
	vehicle_def_default(&def);
	def.manualTransmission = 1;
	track_init(&track); terrain_init(&terrain, &track);
	memset(&car, 0, sizeof(car)); memset(&in, 0, sizeof(in));
	car.gear = 1; car.rpm = def.idleRpm; car.pos = track.pts[0];
	in.gearUp = 1;
	physics_vehicle_step(&def, &car, &in, &terrain, 1.0f / 60.0f);
	assert(car.gear == 2);
}

int main(void)
{
 test_fixed_timestep();
 test_track_terrain_collision();
 test_vehicle_acceleration_and_braking();
 test_tuning_changes_vehicle_response();
 test_manual_gearbox();
 puts("Forza3DS deterministic engine tests: PASS");
 return 0;
}
