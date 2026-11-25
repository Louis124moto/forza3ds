#include "renderer.h"
#include "meshes.h"
#include <string.h>
#include "vshader_shbin.h"

#define CLEAR_COLOR 0x6FC3E8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static DVLB_s *s_dvlb;
static shaderProgram_s s_program;
static int uLoc_projection;
static C3D_Mtx s_projection;
static C3D_RenderTarget *s_target;
static F3DSMeshes s_meshes;
static C3D_BufInfo s_bufInfo;
static bool s_garage;

static C3D_FVec to_c3d(FVec3 v)
{
	C3D_FVec out = {{ v.x, v.y, v.z, 0.0f }};
	return out;
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

	C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 4);

	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	C3D_CullFace(GPU_CULL_NONE);

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
	if (!s_meshes.data || s_meshes.total == 0)
		return;

	C3D_BufInfo *bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, s_meshes.data, sizeof(F3DSVertex), 2, 0x10);
}

static void draw(const F3DSMeshes *m, uint32_t start, uint32_t count)
{
	if (count > 0 && start + count <= m->total)
		C3D_DrawArrays(GPU_TRIANGLES, start, count);
}

static void set_car_matrix(const F3DSVehicleState *car, const C3D_Mtx *view)
{
	C3D_Mtx model, view_model, final;
	Mtx_Identity(&model);
	Mtx_Translate(&model, car->pos.x, car->pos.y, car->pos.z, false);
	Mtx_RotateY(&model, car->yaw, false);
	Mtx_Multiply(&view_model, view, &model);
	Mtx_Multiply(&final, &s_projection, &view_model);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &final);
}

void renderer_render(const F3DSCamera *cam, const F3DSVehicleState *car)
{
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C3D_RenderTargetClear(s_target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_FrameDrawOn(s_target);

	if (!s_meshes.data || s_meshes.total == 0) {
		C3D_FrameEnd(0);
		return;
	}

	C3D_Mtx view;
	Mtx_LookAt(&view, to_c3d(cam->eye), to_c3d(cam->target), (C3D_FVec){{ 0.0f, 1.0f, 0.0f, 0.0f }}, false);

	C3D_Mtx projView;
	Mtx_Multiply(&projView, &s_projection, &view);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projView);

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
	draw(&s_meshes, s_meshes.cockpitStart, s_meshes.cockpitCount);
	draw(&s_meshes, s_meshes.carStart, s_meshes.carCount);

	C3D_FrameEnd(0);
}

void renderer_exit(void)
{
	meshes_free(&s_meshes);
	shaderProgramFree(&s_program);
	DVLB_Free(s_dvlb);
	C3D_Fini();
}
