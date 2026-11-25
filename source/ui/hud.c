#include "hud.h"
#include <stdio.h>

static const char *gear_label(int gear)
{
	static const char *labels[F3DS_GEARS_ALL] = { "R", "1", "2", "3", "4", "5", "6" };
	if (gear < 0 || gear > F3DS_GEARS)
		return "?";
	return labels[gear];
}

void hud_render(const F3DSVehicleState *car, const F3DSCamera *cam, const F3DSLoopState *loop)
{
	consoleClear();
	const float kmh = fabsf(car->speed) * 3.6f;
	int rpmBar = (int)(car->rpm / 8000.0f * 20.0f);
	if (rpmBar < 0) rpmBar = 0;
	if (rpmBar > 20) rpmBar = 20;

	printf("\x1b[1;3HForza3DS - engine/physics test map\n");
	printf("\x1b[3;3HSpeed: %5.0f km/h   Gear: %s %s\n", kmh, gear_label(car->gear), car->airborne ? "AIR" : "");
	printf("\x1b[5;3HRPM: %5.0f\n", car->rpm);
	printf("\x1b[6;3H[");
	for (int i = 0; i < 20; i++)
		putchar(i < rpmBar ? '#' : '.');
	printf("]\n");
	printf("\x1b[8;3HAltitude: %5.2f m   Y-Speed: %5.1f m/s\n", car->pos.y, car->vy);
	printf("\x1b[10;3HCamera: %s  dist %.1f m  h %.1f m\n",
	       cam->mode == F3DS_CAM_CHASE ? "chase " : "cockpit",
	       cam->chaseDistance, cam->chaseHeight);
	printf("\x1b[12;3HFrame: %5.2f ms   FPS: %d\n", loop->frameTime * 1000.0f, loop->fps);
	printf("\x1b[14;3HA accel  B brake  Y handbrake\n");
	printf("\x1b[15;3HX camera  C-stick orbit  D-pad dist/height\n");
	printf("\x1b[16;3HSTART exit\n");
}
