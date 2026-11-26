/* Buffers are refilled on a dedicated high-priority thread woken by the DSP
 * frame callback, so audio does not depend on the video frame rate. */
#include "engine_audio.h"
#include <3ds.h>
#include <string.h>

#define RATE 22050.0f
#define SAMPLES 368          /* about 16.7 ms */
#define BUFFERS 4
#define STACK_SIZE (8 * 1024)

static ndspWaveBuf s_wave[BUFFERS];
static s16 *s_pcm;
static F3DSSynth s_synth;
static F3DSSynthParams s_params;
static LightLock s_lock;
static LightEvent s_event;
static Thread s_thread;
static volatile bool s_quit;
static bool s_ready;

static void fill_free_buffers(void)
{
	for (int i = 0; i < BUFFERS; ++i) {
		if (s_wave[i].status != NDSP_WBUF_FREE && s_wave[i].status != NDSP_WBUF_DONE)
			continue;
		F3DSSynthParams p;
		LightLock_Lock(&s_lock);
		p = s_params;
		s_params.impact = 0.0f; /* an impact is heard once */
		LightLock_Unlock(&s_lock);
		s16 *dst = s_pcm + i * SAMPLES;
		synth_render(&s_synth, &p, dst, SAMPLES, RATE);
		DSP_FlushDataCache(dst, SAMPLES * sizeof(s16));
		ndspChnWaveBufAdd(0, &s_wave[i]);
	}
}

static void audio_thread(void *arg)
{
	(void)arg;
	while (!s_quit) {
		fill_free_buffers();
		LightEvent_Wait(&s_event);
	}
}

static void on_dsp_frame(void *data)
{
	(void)data;
	LightEvent_Signal(&s_event);
}

bool engine_audio_init(void)
{
	if (R_FAILED(ndspInit()))
		return false;
	s_pcm = linearAlloc(sizeof(s16) * SAMPLES * BUFFERS);
	if (!s_pcm) {
		ndspExit();
		return false;
	}
	synth_init(&s_synth);
	memset(&s_params, 0, sizeof(s_params));
	memset(s_wave, 0, sizeof(s_wave));
	for (int i = 0; i < BUFFERS; ++i) {
		s_wave[i].data_vaddr = s_pcm + i * SAMPLES;
		s_wave[i].nsamples = SAMPLES;
	}
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnReset(0);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, RATE);
	ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
	float mix[12] = { 0.9f, 0.9f };
	ndspChnSetMix(0, mix);

	LightLock_Init(&s_lock);
	LightEvent_Init(&s_event, RESET_ONESHOT);
	s_quit = false;
	s32 priority = 0x30;
	svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
	s_thread = threadCreate(audio_thread, NULL, STACK_SIZE, priority - 1, -2, false);
	if (!s_thread) {
		linearFree(s_pcm);
		s_pcm = NULL;
		ndspExit();
		return false;
	}
	ndspSetCallback(on_dsp_frame, NULL);
	s_ready = true;
	return true;
}

void engine_audio_set(const F3DSSynthParams *p)
{
	if (!s_ready)
		return;
	LightLock_Lock(&s_lock);
	const float pending = s_params.impact;
	s_params = *p;
	s_params.impact = fmaxf(p->impact, pending);
	LightLock_Unlock(&s_lock);
}

void engine_audio_pause(bool paused)
{
	if (s_ready)
		ndspChnSetPaused(0, paused);
}

void engine_audio_exit(void)
{
	if (!s_ready)
		return;
	ndspSetCallback(NULL, NULL);
	s_quit = true;
	LightEvent_Signal(&s_event);
	threadJoin(s_thread, U64_MAX);
	threadFree(s_thread);
	ndspChnWaveBufClear(0);
	ndspChnReset(0);
	linearFree(s_pcm);
	s_pcm = NULL;
	ndspExit();
	s_ready = false;
}
