#include "scene.h"
#include <string.h>

#define SPEED_DIAL_MAX_KMH 280.0f

static const float WHITE[3] = { 1.0f, 1.0f, 1.0f };

static void add(F3DSScene *s, F3DSRange range, const C3D_Mtx *projView, const C3D_Mtx *model,
	const float tint[3])
{
	if (!range.count || s->count >= F3DS_SCENE_MAX_DRAWS)
		return;
	F3DSDraw *d = &s->draws[s->count++];
	d->range = range;
	if (model)
		Mtx_Multiply(&d->mvp, projView, model);
	else
		Mtx_Copy(&d->mvp, projView);
	memcpy(d->tint, tint, sizeof(d->tint));
	s->vertices += range.count;
}

static void add_world(F3DSScene *s, const F3DSMeshes *m, const F3DSView *v, const C3D_Mtx *pv)
{
	static const float limits[F3DS_DRAW_COUNT] = { 130.0f, 220.0f, 700.0f };
	const float limit = limits[(unsigned)v->drawDistance < F3DS_DRAW_COUNT ? v->drawDistance : 1];
	float fx = v->target.x - v->eye.x, fz = v->target.z - v->eye.z;
	const float len = sqrtf(fx * fx + fz * fz);
	if (len > 0.001f) { fx /= len; fz /= len; }

	/* Near geometry first helps the depth test reject hidden pixels. */
	for (int i = 0; i < F3DS_CHUNK_COUNT; ++i) {
		const F3DSChunk *c = &m->chunks[i];
		const float dx = c->cx - v->eye.x, dz = c->cz - v->eye.z;
		if (sqrtf(dx * dx + dz * dz) - c->radius > limit)
			continue;
		if (dx * fx + dz * fz < -c->radius)
			continue; /* entirely behind the camera */
		add(s, c->range, pv, NULL, WHITE);
	}
	add(s, m->horizon, pv, NULL, WHITE);
	add(s, m->sea, pv, NULL, WHITE);
}

static void add_car(F3DSScene *s, const F3DSMeshes *m, const F3DSView *v, const C3D_Mtx *pv)
{
	const F3DSCarVisual *vis = catalog_visual(v->carId);
	const F3DSCarMesh *cm = &m->cars[(unsigned)v->carId < F3DS_CAR_COUNT ? v->carId : 0];
	const F3DSVehicleState *st = v->state;
	const F3DSVehicleDef *def = v->def;
	F3DSVehicleDef stock;
	catalog_def(v->carId, &stock);

	float paint[3], rim[3];
	catalog_paint_color(v->paint, paint);
	catalog_rim_color(v->rim, rim);

	C3D_Mtx base;
	Mtx_Identity(&base);
	Mtx_Translate(&base, v->pos.x, v->pos.y, v->pos.z, true);
	Mtx_RotateY(&base, v->yaw, true);

	if (v->cockpit) {
		/* Rigidly attached to the fixed cockpit camera. */
		add(s, cm->cockpitPaint, pv, &base, paint);
		add(s, cm->cockpitTrim, pv, &base, WHITE);

		const float wheelTurn = def->maxSteerAngle > 0.01f ?
			clampf(st->steerAngle / def->maxSteerAngle, -1.0f, 1.0f) * 2.1f : 0.0f;
		C3D_Mtx model;
		Mtx_Copy(&model, &base);
		Mtx_Translate(&model, vis->seatX, vis->eyeY + F3DS_STEERING_Y, vis->eyeZ + F3DS_STEERING_Z, true);
		Mtx_RotateX(&model, F3DS_STEERING_TILT, true);
		Mtx_RotateZ(&model, -wheelTurn, true); /* left turn is anticlockwise */
		add(s, cm->steering, pv, &model, WHITE);

		const float value[2] = {
			clampf(fabsf(st->speed) * 3.6f / SPEED_DIAL_MAX_KMH, 0.0f, 1.0f),
			clampf(st->rpm / fmaxf(def->maxRpm * 1.02f, 1.0f), 0.0f, 1.0f)
		};
		for (int dial = 0; dial < 2; ++dial) {
			Mtx_Copy(&model, &base);
			Mtx_Translate(&model, vis->seatX + (dial ? -F3DS_DIAL_OFFSET_X : F3DS_DIAL_OFFSET_X),
				vis->eyeY + F3DS_DIAL_Y, vis->eyeZ + F3DS_DIAL_Z, true);
			Mtx_RotateZ(&model, -0.75f * F3DS_PI + 1.5f * F3DS_PI * value[dial], true);
			Mtx_Scale(&model, F3DS_DIAL_RADIUS * 0.82f, F3DS_DIAL_RADIUS * 0.82f, 1.0f);
			add(s, m->needle, pv, &model, WHITE);
		}
		return;
	}

	/* Ground slope lifts the whole car; the body then moves on its springs. */
	Mtx_RotateX(&base, -v->groundPitch, true);

	const float front = def->rideHeight[0] - stock.rideHeight[0];
	const float rear = def->rideHeight[1] - stock.rideHeight[1];
	const float lift = clampf(0.5f * (front + rear), -(vis->floorY - 0.10f), 0.8f);
	const float rake = clampf(atan2f(front - rear, def->wheelBase), -0.2f, 0.2f);
	const float pivot = def->cgHeight;

	C3D_Mtx body;
	Mtx_Copy(&body, &base);
	Mtx_Translate(&body, 0.0f, lift + pivot, 0.0f, true);
	Mtx_RotateZ(&body, clampf(st->roll, -0.25f, 0.25f), true);
	Mtx_RotateX(&body, -clampf(st->pitch + rake, -0.25f, 0.25f), true);
	Mtx_Translate(&body, 0.0f, -pivot, 0.0f, true);
	add(s, cm->paint, pv, &body, paint);
	add(s, cm->trim, pv, &body, WHITE);

	for (int axle = 0; axle < 2; ++axle) {
		for (int side = -1; side <= 1; side += 2) {
			C3D_Mtx model;
			Mtx_Copy(&model, &base);
			Mtx_Translate(&model, side * (vis->halfWidth - vis->wheelHalfWidth + 0.06f),
				def->wheelRadius, (axle ? -0.5f : 0.5f) * def->wheelBase, true);
			if (axle == 0)
				Mtx_RotateY(&model, st->steerAngle, true);
			Mtx_RotateX(&model, st->wheelAngle, true);
			Mtx_Scale(&model, vis->wheelHalfWidth, def->wheelRadius, def->wheelRadius);
			add(s, m->tyre, pv, &model, WHITE);
			add(s, m->rim, pv, &model, rim);
		}
	}

	C3D_Mtx shadow;
	Mtx_Identity(&shadow);
	Mtx_Translate(&shadow, v->pos.x, v->groundY + 0.045f, v->pos.z, true);
	Mtx_RotateY(&shadow, v->yaw, true);
	Mtx_RotateX(&shadow, -v->groundPitch, true);
	Mtx_Scale(&shadow, vis->halfWidth + 0.14f, 1.0f, vis->halfLength + 0.10f);
	if (v->shadows)
		add(s, m->shadow, pv, &shadow, WHITE);
}

void scene_build(F3DSScene *s, const F3DSMeshes *m, const F3DSView *v)
{
	s->count = 0;
	s->vertices = 0;
	s->clearColor = v->garageScene ? 0x20262EFF : 0x7EC0EEFF;

	C3D_Mtx projection, view, projView;
	Mtx_PerspTilt(&projection, F3DS_FOV_Y, 400.0f / 240.0f, F3DS_NEAR_PLANE, F3DS_FAR_PLANE, false);
	Mtx_LookAt(&view, FVec3_New(v->eye.x, v->eye.y, v->eye.z),
		FVec3_New(v->target.x, v->target.y, v->target.z), FVec3_New(0.0f, 1.0f, 0.0f), false);
	Mtx_Multiply(&projView, &projection, &view);

	/* The car goes first: it hides the most expensive pixels. */
	add_car(s, m, v, &projView);
	if (v->garageScene)
		add(s, m->garage, &projView, NULL, WHITE);
	else
		add_world(s, m, v, &projView);
}
