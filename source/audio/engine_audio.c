#include "engine_audio.h"
#include <3ds.h>
#include <math.h>
#include <string.h>

#define ENGINE_RATE 22050
#define ENGINE_SAMPLES 512
#define ENGINE_BUFFERS 2

static ndspWaveBuf s_wave[ENGINE_BUFFERS];
static s16 *s_pcm[ENGINE_BUFFERS];
static float s_phase;
static float s_turboPhase;
static bool s_ready;
static bool s_enabled = true;

static float clamp_audio(float v)
{
	if (v < -0.95f) return -0.95f;
	if (v > 0.95f) return 0.95f;
	return v;
}

bool engine_audio_init(void)
{
	if (R_FAILED(ndspInit()))
		return false;
	for (int i = 0; i < ENGINE_BUFFERS; ++i) {
		s_pcm[i] = (s16 *)linearAlloc(sizeof(s16) * ENGINE_SAMPLES);
		if (!s_pcm[i]) {
			for (int j = 0; j < i; ++j) linearFree(s_pcm[j]);
			ndspExit();
			return false;
		}
		memset(&s_wave[i], 0, sizeof(s_wave[i]));
	}
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnReset(0);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, ENGINE_RATE);
	ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
	float mix[12] = { 0.72f, 0.72f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	ndspChnSetMix(0, mix);
	s_ready = true;
	return true;
}

static void generate_engine_buffer(const F3DSVehicleDef *def,
	const F3DSVehicleState *car, s16 *dst)
{
	const float rpm = fmaxf(car->rpm, def->idleRpm);
	const float cylinders = (float)(def->cylinders > 0 ? def->cylinders : 4);
	const float firingHz = rpm * cylinders / 120.0f;
	const float pitch = vehicle_sound_pitch(def, rpm);
	const float timbre = vehicle_sound_timbre(def);
	const float load = clampf(car->engineLoad, 0.0f, 1.0f);
	const float amplitude = 0.12f + load * 0.48f;
	const float turboHz = 520.0f + fmaxf(def->turboBoostBar, 0.0f) * 170.0f;
	for (int i = 0; i < ENGINE_SAMPLES; ++i) {
		const float p = s_phase * 6.28318530718f;
		const float t = s_turboPhase * 6.28318530718f;
		float sample = sinf(p) * 0.48f;
		sample += sinf(p * 2.0f) * (0.18f + timbre * 0.15f);
		sample += sinf(p * 3.0f) * (0.08f + timbre * 0.12f);
		sample += sinf(p * 4.0f) * (0.035f + load * 0.04f);
		if (def->turboInstalled)
			sample += sinf(t) * (0.015f + load * (0.035f + def->turboBoostBar * 0.002f));
		sample *= amplitude * pitch * 0.45f;
		sample = clamp_audio(sample);
		dst[i] = (s16)(sample * 32767.0f);
		s_phase += firingHz / (float)ENGINE_RATE;
		s_turboPhase += turboHz / (float)ENGINE_RATE;
		if (s_phase >= 1.0f) s_phase -= floorf(s_phase);
		if (s_turboPhase >= 1.0f) s_turboPhase -= floorf(s_turboPhase);
	}
}

void engine_audio_update(const F3DSVehicleDef *def, const F3DSVehicleState *car)
{
	if (!s_ready || !def || !car)
		return;
	for (int i = 0; i < ENGINE_BUFFERS; ++i) {
		if (s_wave[i].status != NDSP_WBUF_FREE && s_wave[i].status != NDSP_WBUF_DONE)
			continue;
		if (!s_enabled) {
			memset(s_pcm[i], 0, sizeof(s16) * ENGINE_SAMPLES);
		} else {
			generate_engine_buffer(def, car, s_pcm[i]);
		}
		DSP_FlushDataCache(s_pcm[i], sizeof(s16) * ENGINE_SAMPLES);
		memset(&s_wave[i], 0, sizeof(s_wave[i]));
		s_wave[i].data_vaddr = s_pcm[i];
		s_wave[i].nsamples = ENGINE_SAMPLES;
		s_wave[i].looping = false;
		ndspChnWaveBufAdd(0, &s_wave[i]);
	}
}

void engine_audio_set_enabled(bool enabled)
{
	s_enabled = enabled;
}

void engine_audio_exit(void)
{
	if (!s_ready)
		return;
	ndspChnWaveBufClear(0);
	ndspChnReset(0);
	for (int i = 0; i < ENGINE_BUFFERS; ++i) {
		if (s_pcm[i]) linearFree(s_pcm[i]);
		s_pcm[i] = NULL;
	}
	ndspExit();
	s_ready = false;
}
