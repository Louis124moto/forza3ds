/* Island outline, mountains and secondary relief.
 *
 * The outline is a union of a central mass and tapering peninsulas, minus a
 * few bays, distorted by noise. Heights add coastal plains, hills, mountain
 * ranges along spines running into the peninsulas (their height varies, which
 * makes passes) and one conical volcano. Smooth, broad fields are sampled
 * every 32 m and interpolated; crests and detail every 8 m. */
#include "gen.h"
#include <string.h>

#define COARSE 257
#define COARSE_CELL 32.0f
#define PENINSULAS 5
#define BAYS 3
#define SPINES 3
#define SPINE_POINTS 5

typedef struct {
	float cx, cz, ra, rb, cosR, sinR;
	float px[PENINSULAS][3], pz[PENINSULAS][3], pw[PENINSULAS][3];
	float bx[BAYS], bz[BAYS], br[BAYS];
	float sx[SPINES][SPINE_POINTS], sz[SPINES][SPINE_POINTS];
	float sw[SPINES], sh[SPINES];
	float volcanoHeight;
} Shape;

static void make_shape(F3DSGen *g, Shape *s)
{
	F3DSRng r = rng_make(hash_seed(g->relief, 1));
	s->cx = rng_range(&r, -250.0f, 250.0f);
	s->cz = rng_range(&r, -250.0f, 250.0f);
	s->ra = rng_range(&r, 2150.0f, 2550.0f);
	s->rb = rng_range(&r, 1750.0f, 2100.0f);
	const float rot = rng_range(&r, 0.0f, 2.0f * F3DS_PI);
	s->cosR = cosf(rot);
	s->sinR = sinf(rot);

	const float base = rng_range(&r, 0.0f, 2.0f * F3DS_PI);
	float angle[PENINSULAS];
	float longest = 0.0f;
	for (int i = 0; i < PENINSULAS; ++i) {
		angle[i] = base + (float)i * 2.0f * F3DS_PI / PENINSULAS + rng_range(&r, -0.28f, 0.28f);
		const float len = rng_range(&r, 3250.0f, 3720.0f);
		const float bend = rng_range(&r, -0.30f, 0.30f);
		s->px[i][0] = s->cx + cosf(angle[i]) * 1100.0f;
		s->pz[i][0] = s->cz + sinf(angle[i]) * 1100.0f;
		s->pw[i][0] = rng_range(&r, 720.0f, 980.0f);
		s->px[i][1] = cosf(angle[i] + bend * 0.4f) * len * 0.74f;
		s->pz[i][1] = sinf(angle[i] + bend * 0.4f) * len * 0.74f;
		s->pw[i][1] = rng_range(&r, 420.0f, 640.0f);
		s->px[i][2] = cosf(angle[i] + bend) * len;
		s->pz[i][2] = sinf(angle[i] + bend) * len;
		s->pw[i][2] = rng_range(&r, 190.0f, 320.0f);
		if (len > longest) {
			longest = len;
			g->lighthouseDirX = cosf(angle[i] + bend);
			g->lighthouseDirZ = sinf(angle[i] + bend);
		}
	}
	for (int i = 0; i < BAYS; ++i) {
		const float a = base + ((float)(i * 2) + 0.5f) * 2.0f * F3DS_PI / PENINSULAS +
			rng_range(&r, -0.15f, 0.15f);
		const float d = rng_range(&r, 2550.0f, 3050.0f);
		s->bx[i] = cosf(a) * d;
		s->bz[i] = sinf(a) * d;
		s->br[i] = rng_range(&r, 420.0f, 720.0f);
	}
	/* Ranges start near the middle and run towards alternate peninsulas. */
	for (int i = 0; i < SPINES; ++i) {
		const int p = (i * 2) % PENINSULAS;
		const float sx = s->cx + rng_range(&r, -600.0f, 600.0f);
		const float sz = s->cz + rng_range(&r, -600.0f, 600.0f);
		for (int k = 0; k < SPINE_POINTS; ++k) {
			const float t = (float)k / (SPINE_POINTS - 1);
			s->sx[i][k] = lerpf(sx, s->px[p][1], t) + rng_range(&r, -330.0f, 330.0f);
			s->sz[i][k] = lerpf(sz, s->pz[p][1], t) + rng_range(&r, -330.0f, 330.0f);
		}
		s->sw[i] = rng_range(&r, 430.0f, 640.0f);
		s->sh[i] = i == 0 ? rng_range(&r, 360.0f, 430.0f) : rng_range(&r, 230.0f, 340.0f);
	}
	/* The volcano sits between two ranges. */
	const float va = angle[1] + rng_range(&r, -0.2f, 0.2f);
	const float vd = rng_range(&r, 1300.0f, 1850.0f);
	g->volcanoX = cosf(va) * vd;
	g->volcanoZ = sinf(va) * vd;
	g->volcanoRadius = rng_range(&r, 850.0f, 1050.0f);
	s->volcanoHeight = rng_range(&r, 400.0f, 470.0f);
}

/* Distance to a segment; *t receives the position along it. */
static float segment_distance(float x, float z, float ax, float az, float bx, float bz, float *t)
{
	const float dx = bx - ax, dz = bz - az;
	const float len2 = dx * dx + dz * dz;
	const float u = len2 > 0.0f ? clampf(((x - ax) * dx + (z - az) * dz) / len2, 0.0f, 1.0f) : 0.0f;
	if (t) *t = u;
	const float ex = x - ax - dx * u, ez = z - az - dz * u;
	return sqrtf(ex * ex + ez * ez);
}

static float smooth_max(float a, float b, float k)
{
	const float h = clampf(0.5f + 0.5f * (a - b) / k, 0.0f, 1.0f);
	return lerpf(b, a, h) + k * h * (1.0f - h);
}

/* Signed distance to the shore, metres, positive on land. */
static float land_distance(const Shape *s, uint32_t seed, float x, float z)
{
	const float wx = x + 260.0f * noise_fbm(seed + 3u, x / 1300.0f, z / 1300.0f, 3);
	const float wz = z + 260.0f * noise_fbm(seed + 5u, x / 1300.0f, z / 1300.0f, 3);
	const float ex = (wx - s->cx) * s->cosR + (wz - s->cz) * s->sinR;
	const float ez = -(wx - s->cx) * s->sinR + (wz - s->cz) * s->cosR;
	const float k = sqrtf(ex * ex / (s->ra * s->ra) + ez * ez / (s->rb * s->rb));
	float m = (1.0f - k) * 0.5f * (s->ra + s->rb);
	for (int i = 0; i < PENINSULAS; ++i)
		for (int j = 0; j < 2; ++j) {
			float t;
			const float d = segment_distance(wx, wz, s->px[i][j], s->pz[i][j],
				s->px[i][j + 1], s->pz[i][j + 1], &t);
			m = smooth_max(m, lerpf(s->pw[i][j], s->pw[i][j + 1], t) - d, 220.0f);
		}
	for (int i = 0; i < BAYS; ++i) {
		const float dx = wx - s->bx[i], dz = wz - s->bz[i];
		const float d = sqrtf(dx * dx + dz * dz) - s->br[i];
		m = fminf(m, d + 60.0f);
	}
	m += 190.0f * noise_fbm(seed + 7u, x / 640.0f, z / 640.0f, 4);
	m += 55.0f * noise_fbm(seed + 11u, x / 170.0f, z / 170.0f, 2);
	/* Keep a belt of open sea inside the world square. */
	return fminf(m, 3880.0f - sqrtf(x * x + z * z));
}

static float mountain_amplitude(const Shape *s, uint32_t seed, float x, float z)
{
	const float wx = x + 190.0f * noise_fbm(seed + 13u, x / 800.0f, z / 800.0f, 3);
	const float wz = z + 190.0f * noise_fbm(seed + 17u, x / 800.0f, z / 800.0f, 3);
	float best = 0.0f;
	for (int i = 0; i < SPINES; ++i) {
		float nearest = 1e9f, along = 0.0f;
		for (int k = 0; k + 1 < SPINE_POINTS; ++k) {
			float t;
			const float d = segment_distance(wx, wz, s->sx[i][k], s->sz[i][k],
				s->sx[i][k + 1], s->sz[i][k + 1], &t);
			if (d < nearest) {
				nearest = d;
				along = (float)k + t;
			}
		}
		const float q = nearest / s->sw[i];
		if (q > 2.6f)
			continue;
		/* Crests rise and fall along the range: peaks and passes. */
		const float crest = 0.72f + 0.55f * noise2(seed + 19u + (uint32_t)i, along * 1.7f, 0.37f);
		const float h = s->sh[i] * clampf(crest, 0.30f, 1.15f) * exp2f(-q * q * 1.44f);
		best = fmaxf(best, h);
	}
	return best;
}

bool gen_relief(F3DSGen *g)
{
	const uint32_t seed = g->relief;
	Shape shape;
	make_shape(g, &shape);

	typedef struct { float shore, base, mountain; } Coarse;
	Coarse *c = malloc(sizeof(Coarse) * COARSE * COARSE);
	if (!c)
		return false;
	for (int j = 0; j < COARSE; ++j) {
		const float z = (float)j * COARSE_CELL - WORLD_HALF;
		for (int i = 0; i < COARSE; ++i) {
			const float x = (float)i * COARSE_CELL - WORLD_HALF;
			Coarse *p = &c[j * COARSE + i];
			p->shore = land_distance(&shape, seed, x, z);
			const float d = p->shore;
			/* Coastal plain rising gently inland, plus rolling hills that
			 * fade out on the plains. */
			const float amp = mountain_amplitude(&shape, seed, x, z);
			const float flat = smoothstepf((noise_fbm(seed + 23u, x / 1100.0f, z / 1100.0f, 3) - 0.02f) / 0.28f);
			const float hills = (noise_fbm(seed + 29u, x / 520.0f, z / 520.0f, 4) * 0.5f + 0.5f) * 46.0f *
				smoothstepf(d / 380.0f) * (1.0f - 0.86f * flat);
			p->base = 26.0f * smoothstepf((d - 40.0f) / 1100.0f) + hills;
			p->mountain = amp * smoothstepf((d - 30.0f) / 620.0f) * (1.0f - 0.55f * flat * (amp < 140.0f));
		}
		if ((j & 31) == 0)
			gen_report(g, 2 + j * 14 / COARSE, "Contour de l'île");
	}

	float peak = 0.0f;
	for (int j = 0; j < WORLD_N; ++j) {
		const float z = gen_x(j);
		const int cj = gen_clampi(j / 4, 0, COARSE - 2);
		const float fz = (float)(j - cj * 4) * 0.25f;
		for (int i = 0; i < WORLD_N; ++i) {
			const float x = gen_x(i);
			const int ci = gen_clampi(i / 4, 0, COARSE - 2);
			const float fx = (float)(i - ci * 4) * 0.25f;
			const Coarse *a = &c[cj * COARSE + ci], *b = a + 1, *d0 = a + COARSE, *e = d0 + 1;
#define MIX(field) lerpf(lerpf(a->field, b->field, fx), lerpf(d0->field, e->field, fx), fz)
			const float shore = MIX(shore);
			float h;
			if (shore <= 0.0f) {
				h = fmaxf(-28.0f, shore * 0.055f - 0.5f);
			} else {
				h = 0.35f + 2.1f * smoothstepf(shore / 55.0f) + MIX(base);
				const float mountain = MIX(mountain);
				if (mountain > 0.5f) {
					const float ridge = noise_ridged(seed + 31u, x / 560.0f, z / 560.0f, 4);
					h += mountain * (0.42f + 0.78f * ridge);
				}
				h += 3.2f * noise_fbm(seed + 37u, x / 120.0f, z / 120.0f, 3) *
					(0.35f + fminf(1.0f, mountain / 160.0f)) * smoothstepf(shore / 90.0f);
				/* Rocky headlands: stretches of low cliffs on the coast. */
				const float cliff = noise2(seed + 41u, x / 430.0f, z / 430.0f);
				if (cliff > 0.22f)
					h += 22.0f * smoothstepf((cliff - 0.22f) / 0.25f) * smoothstepf(shore / 30.0f) *
						(1.0f - smoothstepf((shore - 150.0f) / 300.0f));
				const float vx = x - g->volcanoX, vz = z - g->volcanoZ;
				const float vr = sqrtf(vx * vx + vz * vz) / g->volcanoRadius;
				if (vr < 1.0f) {
					const float cone = powf(1.0f - vr, 1.75f) * shape.volcanoHeight;
					const float crater = vr < 0.09f ? (1.0f - vr / 0.09f) * 38.0f : 0.0f;
					const float v = (cone - crater) * smoothstepf(shore / 260.0f) *
						(1.0f + 0.07f * noise2(seed + 43u, x / 95.0f, z / 95.0f));
					h = smooth_max(h, v + 12.0f * smoothstepf(shore / 200.0f), 25.0f);
				}
				h = fmaxf(h, 0.35f);
			}
			h = fminf(h, 960.0f);
			g->h[j * WORLD_N + i] = h;
			peak = fmaxf(peak, h);
#undef MIX
		}
		if ((j & 63) == 0)
			gen_report(g, 16 + j * 24 / WORLD_N, "Montagnes et vallées");
	}
	g->w->peakHeight = peak;
	free(c);
	return true;
}
