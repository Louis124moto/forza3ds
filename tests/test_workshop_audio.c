#include "test.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "synth.h"
#include "workshop.h"

#define RATE 22050.0f
#define N 22050

static void workshop(void)
{
	F3DSGarage g;
	garage_defaults(&g);
	for (int car = 0; car < F3DS_CAR_COUNT; ++car) {
		g.selected = car;
		for (int p = 0; p < F3DS_PAGE_COUNT; ++p) {
			CHECK(workshop_page_name((F3DSWorkshopPage)p)[0] != '?');
			for (int i = 0; i < workshop_item_count((F3DSWorkshopPage)p); ++i) {
				char before[80], after[80];
				workshop_item_text(&g, (F3DSWorkshopPage)p, i, before, sizeof(before));
				CHECKF(before[0] != '?', "page %d item %d", p, i);
				/* Each item reacts to at least one direction. */
				F3DSGarage copy = g;
				workshop_adjust(&g, (F3DSWorkshopPage)p, i, 1);
				workshop_item_text(&g, (F3DSWorkshopPage)p, i, after, sizeof(after));
				if (!strcmp(before, after)) {
					g = copy;
					workshop_adjust(&g, (F3DSWorkshopPage)p, i, -1);
					workshop_item_text(&g, (F3DSWorkshopPage)p, i, after, sizeof(after));
				}
				CHECKF(strcmp(before, after) != 0, "car %d page %d item %d '%s'", car, p, i, before);
				CHECK(vehicle_def_valid(&g.def[car]));
				/* Floors hold, and nothing breaks far in either direction. */
				for (int k = 0; k < 300; ++k) workshop_adjust(&g, (F3DSWorkshopPage)p, i, -1);
				CHECKF(vehicle_def_valid(&g.def[car]), "floor page %d item %d", p, i);
				for (int k = 0; k < 600; ++k) workshop_adjust(&g, (F3DSWorkshopPage)p, i, 1);
				CHECKF(vehicle_def_valid(&g.def[car]), "high page %d item %d", p, i);
				g = copy;
			}
		}
	}
	garage_defaults(&g);
	for (int k = 0; k < 500; ++k) {
		workshop_adjust(&g, F3DS_PAGE_ENGINE, 3, 1); /* boost */
		workshop_adjust(&g, F3DS_PAGE_ENGINE, 1, 1); /* rev limit */
		workshop_adjust(&g, F3DS_PAGE_ENGINE, 0, 1); /* torque */
	}
	CHECK(g.def[0].turboBoostBar > 40.0f && g.def[0].maxRpm > 50000.0f && g.def[0].torqueBase > 2500.0f);
	CHECK(!workshop_adjust(&g, F3DS_PAGE_ENGINE, 99, 1));
	CHECK(!workshop_adjust(&g, F3DS_PAGE_ENGINE, 0, 0));
}

static void render(const F3DSSynthParams *p, int16_t *out)
{
	F3DSSynth s;
	synth_init(&s);
	for (int i = 0; i < N; i += 368)
		synth_render(&s, p, out + i, N - i < 368 ? N - i : 368, RATE);
}

static double energy(const int16_t *x)
{
	double e = 0.0;
	for (int i = 0; i < N; ++i) e += (double)x[i] * x[i];
	return sqrt(e / N);
}

/* High-frequency share: energy of the second difference over total. */
static double brightness(const int16_t *x)
{
	double e = 0.0;
	for (int i = 2; i < N; ++i) {
		const double d = (double)x[i] - 2.0 * x[i - 1] + x[i - 2];
		e += d * d;
	}
	return sqrt(e / N) / (energy(x) + 1e-9);
}

static double tone(const int16_t *x, double hz)
{
	double re = 0.0, im = 0.0;
	for (int i = 0; i < N; ++i) {
		re += x[i] * cos(2.0 * 3.14159265358979 * hz * i / RATE);
		im += x[i] * sin(2.0 * 3.14159265358979 * hz * i / RATE);
	}
	return sqrt(re * re + im * im) / N;
}

static double difference(const int16_t *a, const int16_t *b)
{
	double e = 0.0;
	for (int i = 0; i < N; ++i) e += ((double)a[i] - b[i]) * ((double)a[i] - b[i]);
	return sqrt(e / N);
}

static void synth(void)
{
	static int16_t a[N], b[N];
	F3DSVehicleDef d;
	F3DSVehicleState car;
	memset(&car, 0, sizeof(car));
	catalog_def(0, &d);
	vehicle_state_reset(&car, &d, fvec3(0, 0, 0), 0);
	car.rpm = 3000.0f;
	car.engineLoad = 0.7f;
	F3DSSynthParams p;
	synth_params_from_car(&p, &d, &car);
	p.engineVolume = 0.8f;
	p.ambienceVolume = 0.6f;

	render(&p, a);
	render(&p, b);
	CHECK(memcmp(a, b, sizeof(a)) == 0);
	CHECK(energy(a) > 600.0);
	/* The firing frequency follows rpm and cylinder count. */
	const double f = synth_firing_hz(&p, 3000.0f);
	CHECK(fabs(f - 100.0) < 0.01);
	CHECK(tone(a, 100.0) > tone(a, 137.0) * 4.0);
	F3DSSynthParams q = p;
	q.rpm = 5200.0f;
	render(&q, b);
	CHECK(tone(b, synth_firing_hz(&q, 5200.0f)) > tone(b, 100.0) * 2.0);
	/* A raised rev limit does not lower the pitch at the same revs. */
	q = p;
	q.maxRpm = 14000.0f;
	render(&q, b);
	CHECK(tone(b, 100.0) > tone(b, 137.0) * 4.0);
	/* Load makes it louder. */
	q = p;
	q.load = 0.0f;
	render(&q, b);
	CHECK(energy(a) > energy(b) * 1.3);
	/* Every part changes the sound. */
	for (int part = 0; part < 5; ++part) {
		q = p;
		int *level[5] = { &q.intake, &q.exhaust, &q.camshaft, &q.injectors, &q.pistons };
		*level[part] = 3;
		if (part == 2) { q.rpm = 900.0f; }
		F3DSSynthParams base = p;
		base.rpm = q.rpm;
		render(&base, a);
		render(&q, b);
		CHECKF(difference(a, b) > energy(a) * 0.05, "part %d: %f vs %f", part, difference(a, b), energy(a));
	}
	render(&p, a);
	q = p;
	q.turbo = 1;
	q.boost = 1.2f;
	render(&q, b);
	CHECK(difference(a, b) > energy(a) * 0.05);
	/* Distinct signatures per engine. */
	F3DSVehicleDef v8;
	catalog_def(3, &v8);
	F3DSSynthParams r;
	synth_params_from_car(&r, &v8, &car);
	r.engineVolume = 0.8f;
	render(&r, b);
	CHECK(difference(a, b) > energy(a) * 0.3);
	CHECK(fabs(synth_firing_hz(&r, 3000.0f) - 200.0) < 0.01);
	/* Cockpit muffling. */
	q = p;
	q.cockpit = 1;
	render(&q, b);
	CHECKF(brightness(b) < brightness(a) * 0.6, "%f %f", brightness(b), brightness(a));
	/* Wind grows with speed, surfaces differ, squeal with slip. */
	q = p;
	q.engineVolume = 0.0f;
	q.speed = 5.0f;
	render(&q, a);
	q.speed = 55.0f;
	render(&q, b);
	CHECK(energy(b) > energy(a) * 2.0);
	q.speed = 20.0f;
	q.surface = F3DS_SURFACE_ASPHALT;
	render(&q, a);
	q.surface = F3DS_SURFACE_GRAVEL;
	render(&q, b);
	CHECK(difference(a, b) > energy(a) * 0.3);
	q.surface = F3DS_SURFACE_ASPHALT;
	q.slip = 1.0f;
	render(&q, b);
	CHECK(energy(b) > energy(a) * 1.3);
	/* Volumes and pause. */
	q = p;
	q.engineVolume = q.ambienceVolume = 0.0f;
	render(&q, a);
	CHECK(energy(a) == 0.0);
	q = p;
	q.active = false;
	render(&q, a);
	CHECK(energy(a) == 0.0);
	/* Extreme tuning stays bounded without harsh wrapping. */
	q = p;
	q.rpm = 60000.0f;
	q.maxRpm = 60000.0f;
	q.boost = 50.0f;
	q.turbo = 1;
	q.intake = q.exhaust = q.camshaft = q.injectors = q.pistons = 1000000;
	q.impact = 100.0f;
	q.speed = 500.0f;
	q.slip = 1.0f;
	render(&q, a);
	int peak = 0;
	for (int i = 0; i < N; ++i) if (abs(a[i]) > peak) peak = abs(a[i]);
	CHECK(peak <= 32112 && energy(a) > 100.0);
}

void test_workshop_audio(void)
{
	workshop();
	synth();
}
