/* Forza3DS - native New 3DS driving prototype.
 * New 3DS only: 804 MHz boost enabled at startup, 80% app CPU time for
 * multi-core scheduling. Implements roadmap stages 1-4 (base app, Citro3D
 * scene, camera, vehicle physics, demo track, collisions, HUD).
 */
#include <3ds.h>
#include <stdio.h>

#include "camera.h"
#include "core.h"
#include "hud.h"
#include "input.h"
#include "physics.h"
#include "platform.h"
#include "renderer.h"
#include "track.h"
#include "vehicle.h"

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);
	consoleClear();

	/* New 3DS hardware bring-up; refuse to run on Old 3DS. */
	F3DSPlatformInfo platform;
	if (!platform_init(&platform)) {
		printf("Forza3DS\n");
		printf("This game requires a New Nintendo 3DS.\n");
		printf("Press START to exit.\n");
		while (aptMainLoop()) {
			hidScanInput();
			if (hidKeysDown() & KEY_START)
				break;
			gfxFlushBuffers();
			gfxSwapBuffers();
			gspWaitForVBlank();
		}
		gfxExit();
		return 0;
	}

	renderer_init();

	F3DSLoopState loop;
	core_loop_init(&loop, 1.0f / 60.0f, 5.0f * (1.0f / 60.0f));

	F3DSInputConfig inputCfg;
	input_config_defaults(&inputCfg);
	F3DSInputState input;
	{
		F3DSInputState zero = { 0 };
		input = zero;
	}

	F3DSVehicleDef def;
	vehicle_def_default(&def);

	F3DSTrack track;
	track_init(&track);
	renderer_set_track(&track);

	F3DSVehicleState car;
	{
		F3DSVehicleState zero = { 0 };
		car = zero;
	}
	car.visualModelId = 0;
	car.gear = 1;
	car.rpm = def.idleRpm;
	car.pos = fvec3_add(track.pts[0], fvec3_scale(track.fwd[0], 5.0f));
	car.yaw = atan2f(track.fwd[0].x, track.fwd[0].z);

	F3DSCamera cam;
	camera_init(&cam);
	cam.eye = fvec3(car.pos.x, car.pos.y + 5.0f, car.pos.z - 10.0f);
	cam.target = car.pos;

	while (aptMainLoop()) {
		const int steps = core_loop_begin_frame(&loop);
		input_poll(&input, &inputCfg);

		if (input.exit)
			break;

		/* Fixed timestep simulation with catch-up cap. */
		for (int i = 0; i < steps; i++) {
			physics_step(&def, &car, &input, &track, loop.dt);

			/* One-shot camera actions must not repeat across steps. */
			input.toggleView = 0;
			input.camDistanceUp = input.camDistanceDown = 0;
			input.camHeightUp = input.camHeightDown = 0;
		}

		/* Camera runs at frame rate, using the real frame time. */
		camera_update(&cam, &car, &input, loop.frameTime);

		renderer_render(&cam, &car, &track);
		hud_render(&car, &cam, &loop);

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	gfxExit();
	return 0;
}
