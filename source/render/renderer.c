#include "renderer.h"
#include "meshes.h"
#include <string.h>
#include <math.h>
#include "vshader_shbin.h"

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static DVLB_s *s_dvlb;
static shaderProgram_s s_program;
static int uLoc_projection, uLoc_modelView;
static C3D_Mtx s_projection;
static C3D_RenderTarget *s_target;
static F3DSMeshes s_meshes;
static bool s_garage;
static float s_dayPhase = 0.5f;
static C3D_LightEnv s_lightEnv;
static C3D_Light s_sun;
static C3D_LightLut s_specularLut;

static const C3D_Material s_worldMaterial = {
	{0.20f, 0.22f, 0.20f}, {0.76f, 0.76f, 0.72f},
	{0.08f, 0.08f, 0.08f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}
};
static const C3D_Material s_carMaterial = {
	{0.18f, 0.18f, 0.20f}, {0.68f, 0.68f, 0.72f},
	{0.95f, 0.95f, 1.0f}, {0.24f, 0.24f, 0.28f}, {0.0f, 0.0f, 0.0f}
};

static C3D_FVec to_c3d(FVec3 v)
{
	C3D_FVec out = { .x = v.x, .y = v.y, .z = v.z, .w = 0.0f };
	return out;
}

static float clamp01(float v)
{
	return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float mixf(float a, float b, float t)
{
	return a + (b - a) * t;
}

static void sky_color(float p, float *r, float *g, float *b)
{
	static const float key[5][3] = {
		{0.025f, 0.055f, 0.12f},
		{0.88f, 0.39f, 0.22f},
		{0.34f, 0.68f, 0.84f},
		{0.82f, 0.30f, 0.19f},
		{0.025f, 0.055f, 0.12f}
	};
	float scaled = p * 4.0f;
	int segment = (int)scaled;
	if (segment > 3) segment = 3;
	float t = scaled - (float)segment;
	*r = mixf(key[segment][0], key[segment + 1][0], t);
	*g = mixf(key[segment][1], key[segment + 1][1], t);
	*b = mixf(key[segment][2], key[segment + 1][2], t);
}

static u32 pack_rgba(float r, float g, float b)
{
	const u32 rr = (u32)(clamp01(r) * 255.0f);
	const u32 gg = (u32)(clamp01(g) * 255.0f);
	const u32 bb = (u32)(clamp01(b) * 255.0f);
	return (rr << 24) | (gg << 16) | (bb << 8) | 0xFFu;
}

static void update_lighting(const F3DSCamera *cam)
{
	const float angle = s_dayPhase * (2.0f * F3DS_PI);
	const float elevation = sinf(angle - F3DS_PI * 0.5f);
	const float horizontal = cosf(angle - F3DS_PI * 0.5f);
	const float azimuth = angle;
	FVec3 sun = fvec3(cosf(azimuth) * horizontal,
		fmaxf(0.10f, elevation), sinf(azimuth) * horizontal);
	const float sl = fvec3_len(sun);
	if (sl > 0.0001f) sun = fvec3_scale(sun, 1.0f / sl);

	FVec3 forward = fvec3_sub(cam->target, cam->eye);
	float fl = fvec3_len(forward);
	if (fl > 0.0001f) forward = fvec3_scale(forward, 1.0f / fl);
	FVec3 worldUp = fvec3(0.0f, 1.0f, 0.0f);
	FVec3 right = fvec3(forward.y * worldUp.z - forward.z * worldUp.y,
		forward.z * worldUp.x - forward.x * worldUp.z,
		forward.x * worldUp.y - forward.y * worldUp.x);
	float rl = fvec3_len(right);
	if (rl > 0.0001f) right = fvec3_scale(right, 1.0f / rl);
	FVec3 up = fvec3(right.y * forward.z - right.z * forward.y,
		right.z * forward.x - right.x * forward.z,
		right.x * forward.y - right.y * forward.x);
	const float vx = fvec3_dot(sun, right);
	const float vy = fvec3_dot(sun, up);
	const float vz = -fvec3_dot(sun, forward);
	C3D_FVec lightPos = { .x = vx, .y = vy, .z = vz, .w = 0.0f };
	C3D_LightPosition(&s_sun, &lightPos);

	const float daylight = clamp01((elevation + 0.12f) / 0.88f);
	const float warm = clamp01(1.0f - fabsf(elevation) * 5.0f);
	const float night = 1.0f - daylight;
	const float lr = mixf(0.24f, 1.0f, daylight) * (1.0f - 0.10f * warm);
	const float lg = mixf(0.34f, 0.72f, daylight) + 0.12f * warm;
	const float lb = mixf(0.58f, 0.90f, daylight) - 0.34f * warm;
	C3D_LightColor(&s_sun, lr, lg, lb);
	C3D_LightEnvAmbient(&s_lightEnv,
		mixf(0.055f, 0.30f, daylight) + 0.025f * warm,
		mixf(0.075f, 0.29f, daylight) + 0.018f * warm,
		mixf(0.13f, 0.27f, daylight) - 0.035f * warm);
	(void)night;
}

void renderer_set_time(float day_phase)
{
	if (!isfinite(day_phase)) return;
	s_dayPhase = day_phase - floorf(day_phase);
}

void renderer_init(void)
{
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	s_target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(s_target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	s_dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&s_program);
	shaderProgramSetVsh(&s_program, &s_dvlb->DVLE[0]);
	C3D_BindProgram(&s_program);
	uLoc_projection = shaderInstanceGetUniformLocation(s_program.vertexShader, "projection");
	uLoc_modelView = shaderInstanceGetUniformLocation(s_program.vertexShader, "modelView");

	C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 4);
	AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3);

	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_FRAGMENT_PRIMARY_COLOR, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

	C3D_LightEnvInit(&s_lightEnv);
	C3D_LightEnvBind(&s_lightEnv);
	C3D_LightEnvAmbient(&s_lightEnv, 0.22f, 0.24f, 0.25f);
	LightLut_Phong(&s_specularLut, 28.0f);
	C3D_LightEnvLut(&s_lightEnv, GPU_LUT_D0, GPU_LUTINPUT_LN, false, &s_specularLut);
	C3D_LightInit(&s_sun, &s_lightEnv);
	C3D_LightEnable(&s_sun, true);
	C3D_LightTwoSideDiffuse(&s_sun, true);
	C3D_LightColor(&s_sun, 1.0f, 0.96f, 0.88f);

	C3D_CullFace(GPU_CULL_NONE);
	C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
	Mtx_PerspTilt(&s_projection, 62.0f * F3DS_PI / 180.0f, 400.0f / 240.0f, 0.15f, 600.0f, false);
	memset(&s_meshes, 0, sizeof(s_meshes));
}

void renderer_set_garage(bool enabled)
{
	s_garage = enabled;
}

void renderer_set_track(const F3DSTrack *track, const F3DSTerrain *terrain)
{
	meshes_free(&s_meshes);
	meshes_build(&s_meshes, track, terrain);
	if (!s_meshes.data || s_meshes.total == 0) return;
	GSPGPU_FlushDataCache(s_meshes.data, s_meshes.total * sizeof(F3DSVertex));
	C3D_BufInfo *bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, s_meshes.data, sizeof(F3DSVertex), 3, 0x210);
}

static void draw(const F3DSMeshes *m, uint32_t start, uint32_t count)
{
	if (count > 0 && start + count <= m->total)
		C3D_DrawArrays(GPU_TRIANGLES, start, count);
}

static void set_car_matrix(const F3DSVehicleState *car, const C3D_Mtx *view)
{
	C3D_Mtx model, view_model;
	Mtx_Identity(&model);
	Mtx_Translate(&model, car->pos.x, car->pos.y, car->pos.z, false);
	Mtx_RotateY(&model, car->yaw, false);
	Mtx_Multiply(&view_model, view, &model);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &view_model);
}

void renderer_render(const F3DSCamera *cam, const F3DSVehicleState *car)
{
	float sr, sg, sb;
	sky_color(s_dayPhase, &sr, &sg, &sb);
	const u32 clearColor = pack_rgba(sr, sg, sb);
	C3D_RenderTargetClear(s_target, C3D_CLEAR_ALL, clearColor, 0);
	if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) return;
	C3D_FrameDrawOn(s_target);
	if (!s_meshes.data || s_meshes.total == 0) {
		C3D_FrameEnd(0);
		return;
	}

	C3D_Mtx view;
	Mtx_LookAt(&view, to_c3d(cam->eye), to_c3d(cam->target),
		(C3D_FVec){ .x = 0.0f, .y = 1.0f, .z = 0.0f, .w = 0.0f }, false);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &s_projection);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &view);
	update_lighting(cam);
	C3D_LightEnvMaterial(&s_lightEnv, &s_worldMaterial);

	if (s_garage) {
		draw(&s_meshes, s_meshes.garageStart, s_meshes.garageCount);
	} else {
		draw(&s_meshes, s_meshes.groundStart, s_meshes.groundCount);
		draw(&s_meshes, s_meshes.roadStart, s_meshes.roadCount);
		draw(&s_meshes, s_meshes.obstacleStart, s_meshes.obstacleCount);
		draw(&s_meshes, s_meshes.decorStart, s_meshes.decorCount);
		draw(&s_meshes, s_meshes.terrainStart, s_meshes.terrainCount);
	}

	set_car_matrix(car, &view);
	C3D_LightEnvMaterial(&s_lightEnv, &s_carMaterial);
	draw(&s_meshes, s_meshes.cockpitStart, s_meshes.cockpitCount);
	draw(&s_meshes, s_meshes.carStart, s_meshes.carCount);

	/* Transparent glass is rendered last, with depth testing but no depth writes. */
	if (s_meshes.glassCount) {
		C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
			GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
		C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_COLOR);
		draw(&s_meshes, s_meshes.glassStart, s_meshes.glassCount);
		C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
		C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
	}
	C3D_FrameEnd(0);
}

void renderer_exit(void)
{
	meshes_free(&s_meshes);
	shaderProgramFree(&s_program);
	DVLB_Free(s_dvlb);
	C3D_Fini();
}
