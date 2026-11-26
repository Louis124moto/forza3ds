#include "synth.h"
#include <string.h>

#define LUT_SIZE 1024
static float s_sine[LUT_SIZE + 1];

void synth_init(F3DSSynth *s)
{
	memset(s, 0, sizeof(*s));
	s->rng = 0x1234ABCDu;
	for (int i = 0; i <= LUT_SIZE; ++i)
		s_sine[i] = sinf((float)i * (2.0f * F3DS_PI / LUT_SIZE));
}

/* phase in cycles, any non-negative value. */
static inline float osc(float phase)
{
	const float x = (phase - (float)(int)phase) * LUT_SIZE;
	const int i = (int)x;
	return s_sine[i] + (s_sine[i + 1] - s_sine[i]) * (x - (float)i);
}

static inline float noise(F3DSSynth *s)
{
	s->rng ^= s->rng << 13;
	s->rng ^= s->rng >> 17;
	s->rng ^= s->rng << 5;
	return (float)(int32_t)s->rng * (1.0f / 2147483648.0f);
}

/* One-pole low-pass coefficient for a cut-off frequency. */
static float pole(float hz, float rate)
{
	return clampf(1.0f - expf(-2.0f * F3DS_PI * hz / rate), 0.0f, 1.0f);
}

void synth_params_from_car(F3DSSynthParams *p, const F3DSVehicleDef *d, const F3DSVehicleState *car)
{
	memset(p, 0, sizeof(*p));
	p->active = true;
	p->rpm = car->rpm;
	p->idleRpm = d->idleRpm;
	p->maxRpm = d->maxRpm;
	p->load = car->engineLoad;
	p->boost = car->boost;
	p->turbo = d->turboInstalled;
	p->cylinders = d->cylinders;
	p->displacementLitres = d->displacementLitres;
	p->intake = d->intakeLevel;
	p->exhaust = d->exhaustLevel;
	p->camshaft = d->camshaftLevel;
	p->injectors = d->injectorLevel;
	p->pistons = d->pistonLevel;
	p->speed = fabsf(car->speed);
	p->slip = fmaxf(car->slip[0], fmaxf(car->slip[1], car->wheelSpin));
	p->impact = car->impact;
	p->surface = car->surface;
	p->grounded = !car->airborne;
}

float synth_firing_hz(const F3DSSynthParams *p, float rpm)
{
	/* Four stroke: each cylinder fires once every two revolutions. */
	return rpm * (float)(p->cylinders > 0 ? p->cylinders : 4) / 120.0f;
}

/* Part levels are unbounded; their effect on the tone saturates smoothly. */
static float soft(int level) { return (float)level / ((float)level + 3.0f); }

void synth_render(F3DSSynth *s, const F3DSSynthParams *p, int16_t *out, int count, float rate)
{
	if (count <= 0)
		return;
	if (!p->active) {
		memset(out, 0, sizeof(int16_t) * (size_t)count);
		s->primed = false;
		return;
	}
	if (!s->primed) {
		s->rpm = p->rpm;
		s->load = p->load;
		s->boost = p->boost;
		s->primed = true;
	}
	/* Parameters ramp across the buffer to avoid zipper noise. */
	const float inv = 1.0f / (float)count;
	const float dRpm = (p->rpm - s->rpm) * inv;
	const float dLoad = (p->load - s->load) * inv;
	const float dBoost = (p->boost - s->boost) * inv;
	/* Lifting off with pressure in the intake vents it. */
	if (p->turbo && s->load > 0.5f && p->load < 0.2f && s->boost > 0.25f)
		s->blowoff = fminf(0.25f + s->boost * 0.25f, 0.8f);

	const float intake = soft(p->intake), exhaust = soft(p->exhaust), cam = soft(p->camshaft);
	const float injectors = soft(p->injectors), pistons = soft(p->pistons);
	const float size = clampf(p->displacementLitres / 5.0f, 0.1f, 1.6f);
	const bool uneven = p->cylinders >= 8; /* cross-plane burble */

	const float cutoff = p->cockpit ? 700.0f : 3200.0f + exhaust * 3500.0f + intake * 1200.0f;
	const float aEngine = pole(cutoff, rate);
	const float aWind = pole(p->cockpit ? 450.0f : 900.0f + p->speed * 12.0f, rate);
	const float rollCut = p->surface == F3DS_SURFACE_GRAVEL ? 2600.0f :
		p->surface == F3DS_SURFACE_GRASS ? 700.0f : 240.0f;
	const float aRoll = pole(p->cockpit ? rollCut * 0.5f : rollCut, rate);
	const float aOut = pole(p->cockpit ? 1600.0f : 9000.0f, rate);

	const float speedN = clampf(p->speed / 60.0f, 0.0f, 1.3f);
	const float windTarget = speedN * speedN * (p->cockpit ? 0.22f : 0.45f);
	const float rollGain = p->surface == F3DS_SURFACE_ASPHALT ? 0.35f :
		p->surface == F3DS_SURFACE_GRAVEL ? 0.65f : 0.45f;
	const float rollTarget = p->grounded ? clampf(p->speed / 35.0f, 0.0f, 1.0f) * rollGain : 0.0f;
	const float squealTarget = (p->grounded && p->speed > 3.0f) ?
		clampf(p->slip, 0.0f, 1.0f) * (p->surface == F3DS_SURFACE_ASPHALT ? 0.30f : 0.06f) : 0.0f;
	if (p->impact > 1.0f)
		s->thump = fmaxf(s->thump, clampf(p->impact / 15.0f, 0.1f, 0.9f));

	const float engineVol = clampf(p->engineVolume, 0.0f, 1.0f);
	const float ambienceVol = clampf(p->ambienceVolume, 0.0f, 1.0f);

	for (int i = 0; i < count; ++i) {
		s->rpm += dRpm;
		s->load += dLoad;
		s->boost += dBoost;
		const float load = clampf(s->load, 0.0f, 1.0f);
		const float revN = clampf((s->rpm - p->idleRpm) / fmaxf(p->maxRpm - p->idleRpm, 1.0f), 0.0f, 1.2f);
		const float firing = synth_firing_hz(p, s->rpm);

		/* Exhaust note: harmonic series of the firing frequency. */
		float e = osc(s->phase) * (0.50f + 0.14f * size);
		e += osc(s->phase * 2.0f) * (0.20f + 0.22f * exhaust);
		e += osc(s->phase * 3.0f) * (0.08f + 0.16f * exhaust + 0.06f * load);
		e += osc(s->phase * 4.0f) * (0.03f + 0.14f * pistons);
		e += osc(s->phase * 5.0f) * (0.02f + 0.12f * pistons + 0.05f * revN);
		/* Crankshaft half orders give large engines their weight. */
		e += osc(s->subPhase) * (0.10f + 0.30f * size);
		if (uneven)
			e += osc(s->subPhase * 3.0f) * 0.18f;
		/* Combustion roughness and intake roar. */
		const float n = noise(s);
		const float pulse = 0.5f + 0.5f * osc(s->phase);
		e += n * pulse * pulse * (0.06f + load * (0.07f + 0.50f * intake));
		/* Induction honk of a freer intake. */
		e += osc(s->phase * 1.5f) * 0.24f * intake * load;
		/* Injector tick. */
		if (injectors > 0.0f && s->phase - (float)(int)s->phase < 0.10f)
			e += n * 0.90f * injectors;
		/* A wild camshaft makes the idle lope. */
		const float lope = 1.0f - (0.08f + 0.55f * cam) * clampf(1.0f - revN * 3.0f, 0.0f, 1.0f) *
			(0.5f + 0.5f * osc(s->lopePhase));
		const float loud = (0.20f + 0.46f * load + 0.18f * revN) * (1.0f + 0.35f * exhaust);
		e *= loud * lope * 0.42f;
		s->lpEngine += aEngine * (e - s->lpEngine);
		s->lpEngine2 += aEngine * (s->lpEngine - s->lpEngine2);
		float engine = s->lpEngine2;

		if (p->turbo) {
			const float pressure = fmaxf(s->boost, 0.0f);
			const float amount = pressure / (pressure + 0.8f);
			engine += osc(s->turboPhase) * amount * (p->cockpit ? 0.035f : 0.085f);
			const float whistle = 900.0f + 2600.0f * amount + 1800.0f * revN;
			s->turboPhase += fminf(whistle, rate * 0.45f) / rate;
		}
		if (s->blowoff > 0.001f) {
			engine += (n - s->hpPrev) * 0.5f * s->blowoff * (p->cockpit ? 0.4f : 0.8f);
			s->blowoff *= 1.0f - 9.0f / rate;
		}
		s->hpPrev = n;

		/* Environment: wind, rolling, squeal and impacts. */
		s->wind += (windTarget - s->wind) * 0.002f;
		s->roll += (rollTarget - s->roll) * 0.002f;
		s->squeal += (squealTarget - s->squeal) * 0.003f;
		s->lpWind += aWind * (noise(s) - s->lpWind);
		float rollNoise = noise(s);
		if (p->surface == F3DS_SURFACE_GRAVEL && (s->rng & 31u) == 0u)
			rollNoise *= 4.0f; /* stones */
		s->lpRoll += aRoll * (rollNoise - s->lpRoll);
		float amb = s->lpWind * s->wind + s->lpRoll * s->roll;
		amb += (osc(s->squealPhase) + 0.4f * osc(s->squealPhase * 2.02f)) * s->squeal *
			(p->cockpit ? 0.5f : 1.0f);
		amb += s->lpRoll * s->thump * 2.5f + osc(s->lopePhase * 3.0f) * s->thump * 0.35f;
		s->thump *= 1.0f - 12.0f / rate;

		const float mixed = engine * engineVol + amb * ambienceVol;
		s->lpOut += aOut * (mixed - s->lpOut);
		/* Soft clipper: extreme tuning stays loud without harsh wrap-around. */
		const float y = s->lpOut / (1.0f + 0.45f * fabsf(s->lpOut));
		out[i] = (int16_t)(clampf(y, -0.98f, 0.98f) * 32767.0f);

		s->phase += firing / rate;
		s->subPhase += firing * 0.5f / rate;
		s->lopePhase += (s->rpm / 480.0f) / rate;
		s->squealPhase += (780.0f + 120.0f * s->squeal) / rate;
		/* Keep phases small; multiples used above stay continuous. */
		if (s->phase >= 64.0f) s->phase -= 64.0f;
		if (s->subPhase >= 64.0f) s->subPhase -= 64.0f;
		if (s->lopePhase >= 64.0f) s->lopePhase -= 64.0f;
		if (s->turboPhase >= 64.0f) s->turboPhase -= 64.0f;
		if (s->squealPhase >= 50.0f) s->squealPhase -= 50.0f;
	}
	s->rpm = p->rpm;
	s->load = p->load;
	s->boost = p->boost;
}
