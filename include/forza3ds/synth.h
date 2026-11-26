/* Forza3DS - procedural engine, tire, surface and wind synthesis.
 * Hardware independent: renders signed 16-bit mono samples. */
#ifndef F3DS_SYNTH_H
#define F3DS_SYNTH_H

#include "common.h"
#include "vehicle.h"

typedef struct {
	bool active;             /* false renders silence */
	float rpm, idleRpm, maxRpm;
	float load;              /* 0..1 */
	float boost;             /* bar */
	int turbo;
	int cylinders;
	float displacementLitres;
	int intake, exhaust, camshaft, injectors, pistons;
	int cockpit;             /* muffled, heard from inside */
	float speed;             /* m/s */
	float slip;              /* 0..1 tire squeal */
	float impact;            /* collision strength, m/s */
	int surface;             /* F3DSSurface */
	int grounded;
	float engineVolume, ambienceVolume; /* 0..1 */
} F3DSSynthParams;

typedef struct {
	float phase, subPhase, lopePhase, turboPhase, squealPhase;
	float rpm, load, boost, wind, squeal, roll, thump, blowoff;
	float lpEngine, lpEngine2, lpWind, lpRoll, hpPrev, lpOut;
	uint32_t rng;
	bool primed;
} F3DSSynth;

void synth_init(F3DSSynth *s);
/* Fills sound parameters derived from a car definition and its state. */
void synth_params_from_car(F3DSSynthParams *p, const F3DSVehicleDef *d, const F3DSVehicleState *car);
float synth_firing_hz(const F3DSSynthParams *p, float rpm);
void synth_render(F3DSSynth *s, const F3DSSynthParams *p, int16_t *out, int count, float sampleRate);

#endif
