/* Forza3DS - 3DS entry point: hardware glue around the application. */
#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "engine_audio.h"
#include "platform.h"
#include "renderer.h"
#include "screens.h"

#define DATA_DIR "sdmc:/3ds/forza3ds"
#define HUD_PERIOD (1.0f / 30.0f)

static F3DSApp s_app;
static uint16_t s_lower[CANVAS_PIXELS];
static bool s_resumed;

static void apt_hook(APT_HookType type, void *param)
{
	(void)param;
	if (type == APTHOOK_ONSUSPEND || type == APTHOOK_ONSLEEP) {
		/* Lid closed or HOME: stop the game and stay paused afterwards. */
		app_suspend(&s_app);
		engine_audio_pause(true);
	} else if (type == APTHOOK_ONRESTORE || type == APTHOOK_ONWAKEUP) {
		s_resumed = true;
	}
}

static void present_lower(void)
{
	F3DSCanvas canvas = { s_lower, consoleGetDefault()->font.gfx, 0 };
	screens_draw(&canvas, &s_app);
	u16 width = 0, height = 0;
	u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &width, &height);
	memcpy(fb, s_lower, sizeof(s_lower));
	GSPGPU_FlushDataCache(fb, sizeof(s_lower));
	gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

/* Shows a message and waits for START; used when the game cannot run. */
static void fatal(const char *message)
{
	consoleInit(GFX_BOTTOM, NULL);
	printf("Forza3DS\n\n%s\n\nSTART : quitter\n", message);
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START)
			break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
}

#ifdef PROFILE
/* Developer build only (make PROFILE=1): one CSV line per second. */
static void profile_frame(float elapsed, float updateMs, float lowerMs)
{
	static FILE *log;
	static float time, worst, sumUpdate, sumLower;
	static int frames;
	if (!log) {
		log = fopen(DATA_DIR "/perf.csv", "w");
		if (!log)
			return;
		fputs("screen,car,fps,worst_ms,update_ms,lower_ms,gpu_cpu_ms,gpu_draw_ms,draws,"
			"vertices,cmdbuf,linear_free\n", log);
	}
	frames++;
	time += elapsed;
	worst = fmaxf(worst, elapsed);
	sumUpdate += updateMs;
	sumLower += lowerMs;
	if (time < 1.0f)
		return;
	F3DSRenderStats st;
	renderer_stats(&st);
	fprintf(log, "%d,%d,%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%d,%lu,%.2f,%lu\n", (int)s_app.screen,
		(int)s_app.garage.selected, frames / time, worst * 1000.0f, sumUpdate / frames,
		sumLower / frames, st.cpuMs, st.gpuMs, st.draws, (unsigned long)st.vertices,
		st.cmdBufUsage, (unsigned long)linearSpaceFree());
	fflush(log);
	time = worst = sumUpdate = sumLower = 0.0f;
	frames = 0;
}
#endif

static float ms_since(u64 start)
{
	return (float)(svcGetSystemTick() - start) / (float)(SYSCLOCK_ARM11 / 1000);
}

int main(void)
{
	gfxInitDefault();
	F3DSPlatformInfo platform;
	if (!platform_init(&platform)) {
		fatal("Une New Nintendo 3DS est requise.");
		gfxExit();
		return 0;
	}

	app_init(&s_app, DATA_DIR);
	if (!renderer_init(&s_app.track, &s_app.terrain)) {
		fatal("Impossible d'initialiser le rendu 3D\n(mémoire insuffisante).");
		gfxExit();
		return 1;
	}
	gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB565_OES);
	input_hw_init();
	if (!engine_audio_init())
		snprintf(s_app.message, sizeof(s_app.message), "Son indisponible : dspfirm.cdc manquant");
	if (s_app.message[0])
		s_app.messageTime = 6.0f;

	aptHookCookie hook;
	aptHook(&hook, apt_hook, NULL);

	u64 previous = svcGetSystemTick();
	uint32_t drawnRevision = 0;
	float hudTimer = 0.0f;
	while (aptMainLoop()) {
		const u64 now = svcGetSystemTick();
		float elapsed = (float)(now - previous) / (float)SYSCLOCK_ARM11;
		previous = now;
		if (s_resumed) {
			/* Time spent suspended is not simulated. */
			s_resumed = false;
			elapsed = 0.0f;
			drawnRevision = 0;
		}

		F3DSRawInput raw;
		input_hw_read(&raw);
		app_update(&s_app, &raw, elapsed);
		if (s_app.quit)
			break;

		F3DSSynthParams sound;
		app_audio_params(&s_app, &sound);
		engine_audio_set(&sound);
		engine_audio_pause(app_audio_paused(&s_app));
		const float updateMs = ms_since(now);

		F3DSView view;
		app_view(&s_app, &view);
		renderer_render(&view, s_app.settings.fpsLimit);

		/* Menus redraw on change, the instruments about 30 times a second. */
		const u64 lowerStart = svcGetSystemTick();
		hudTimer += elapsed;
		const bool live = s_app.screen == F3DS_SCREEN_DRIVE;
		if (drawnRevision != s_app.uiRevision || (live && hudTimer >= HUD_PERIOD * 0.9f)) {
			drawnRevision = s_app.uiRevision;
			hudTimer = 0.0f;
			present_lower();
		}
#ifdef PROFILE
		profile_frame(elapsed, updateMs, ms_since(lowerStart));
#else
		(void)updateMs;
		(void)lowerStart;
#endif
	}

	app_shutdown(&s_app);
	aptUnhook(&hook);
	engine_audio_exit();
	renderer_exit();
	gfxExit();
	return 0;
}
