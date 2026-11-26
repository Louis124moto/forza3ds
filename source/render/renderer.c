#include "renderer.h"
#include <citro3d.h>
#include <string.h>
#include "meshes.h"
#include "scene.h"
#include "vshader_shbin.h"

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static DVLB_s *s_dvlb;
static shaderProgram_s s_program;
static int s_locProjection, s_locTint;
static C3D_RenderTarget *s_target;
static F3DSVertex *s_vertices;
static F3DSMeshes s_meshes;
static F3DSScene s_scene;
static int s_fpsLimit;

bool renderer_init(const F3DSTrack *track, const F3DSTerrain *terrain)
{
	if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE))
		return false;
	s_target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	s_vertices = linearAlloc(F3DS_MESH_CAPACITY * sizeof(F3DSVertex));
	s_dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
	if (!s_target || !s_vertices || !s_dvlb ||
	    !meshes_build(&s_meshes, s_vertices, F3DS_MESH_CAPACITY, track, terrain)) {
		renderer_exit();
		return false;
	}
	C3D_RenderTargetSetOutput(s_target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	shaderProgramInit(&s_program);
	shaderProgramSetVsh(&s_program, &s_dvlb->DVLE[0]);
	C3D_BindProgram(&s_program);
	s_locProjection = shaderInstanceGetUniformLocation(s_program.vertexShader, "projection");
	s_locTint = shaderInstanceGetUniformLocation(s_program.vertexShader, "tint");

	C3D_AttrInfo *attr = C3D_GetAttrInfo();
	AttrInfo_Init(attr);
	AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3);
	AttrInfo_AddLoader(attr, 1, GPU_UNSIGNED_BYTE, 4);

	/* The GPU reads linear memory directly: flush the CPU cache once. */
	GSPGPU_FlushDataCache(s_vertices, s_meshes.count * sizeof(F3DSVertex));
	C3D_BufInfo *buf = C3D_GetBufInfo();
	BufInfo_Init(buf);
	BufInfo_Add(buf, s_vertices, sizeof(F3DSVertex), 2, 0x10);

	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	C3D_CullFace(GPU_CULL_BACK_CCW);
	C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
	s_fpsLimit = 0;
	return true;
}

void renderer_render(const F3DSView *view, int fpsLimit)
{
	if (fpsLimit != s_fpsLimit) {
		s_fpsLimit = fpsLimit;
		C3D_FrameRate(fpsLimit == 60 ? 60.0f : 30.0f);
	}
	scene_build(&s_scene, &s_meshes, view);

	/* SYNCDRAW paces the loop to the limit and waits for the previous frame,
	 * so the clear below cannot race with the GPU. */
	if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))
		return;
	C3D_RenderTargetClear(s_target, C3D_CLEAR_ALL, s_scene.clearColor, 0);
	C3D_FrameDrawOn(s_target);
	for (int i = 0; i < s_scene.count; ++i) {
		const F3DSDraw *d = &s_scene.draws[i];
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, s_locProjection, &d->mvp);
		C3D_FVUnifSet(GPU_VERTEX_SHADER, s_locTint, d->tint[0], d->tint[1], d->tint[2], 1.0f);
		C3D_DrawArrays(GPU_TRIANGLES, (int)d->range.start, (int)d->range.count);
	}
	C3D_FrameEnd(0);
}

void renderer_stats(F3DSRenderStats *out)
{
	out->cpuMs = C3D_GetProcessingTime();
	out->gpuMs = C3D_GetDrawingTime();
	out->draws = s_scene.count;
	out->vertices = s_scene.vertices;
	out->cmdBufUsage = C3D_GetCmdBufUsage();
}

void renderer_exit(void)
{
	if (s_dvlb) {
		shaderProgramFree(&s_program);
		DVLB_Free(s_dvlb);
		s_dvlb = NULL;
	}
	if (s_vertices) {
		linearFree(s_vertices);
		s_vertices = NULL;
	}
	if (s_target) {
		C3D_RenderTargetDelete(s_target);
		s_target = NULL;
	}
	C3D_Fini();
}
