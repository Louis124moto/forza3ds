/* Car footprint = three circles along the chassis, tested against barrier
 * segments, boxes and the coast, with an impulse response including yaw. */
#include "physics.h"

#define RESTITUTION 0.18f
#define WALL_FRICTION 0.25f

static float yaw_inertia(const F3DSVehicleDef *d)
{
	return d->mass * (d->wheelBase * d->wheelBase + d->trackWidth * d->trackWidth) / 7.0f;
}

/* n: unit normal pointing out of the obstacle, depth: penetration. */
static void respond(const F3DSVehicleDef *def, F3DSVehicleState *car,
	float cx, float cz, float radius, float nx, float nz, float depth)
{
	car->pos.x += nx * depth;
	car->pos.z += nz * depth;

	const float ax = cx - nx * radius - car->pos.x;
	const float az = cz - nz * radius - car->pos.z;
	const float invM = 1.0f / def->mass;
	const float invI = 1.0f / yaw_inertia(def);
	const float vpx = car->vx + car->yawRate * az;
	const float vpz = car->vz - car->yawRate * ax;
	const float vn = vpx * nx + vpz * nz;
	if (vn >= 0.0f)
		return;
	const float rn = az * nx - ax * nz;
	const float j = -(1.0f + RESTITUTION) * vn / (invM + rn * rn * invI);
	car->vx += j * nx * invM;
	car->vz += j * nz * invM;
	car->yawRate += j * rn * invI * 0.6f;

	const float tx = -nz, tz = nx;
	const float vt = (car->vx + car->yawRate * az) * tx + (car->vz - car->yawRate * ax) * tz;
	const float rt = az * tx - ax * tz;
	const float jt = clampf(-vt / (invM + rt * rt * invI), -WALL_FRICTION * j, WALL_FRICTION * j);
	car->vx += jt * tx * invM;
	car->vz += jt * tz * invM;
	car->yawRate += jt * rt * invI * 0.6f;
	car->impact = fmaxf(car->impact, -vn);
}

/* Circle against a point q (closest point of an obstacle) with extra margin. */
static bool circle_point(const F3DSVehicleDef *def, F3DSVehicleState *car,
	float cx, float cz, float radius, float qx, float qz, float margin, float fallbackX, float fallbackZ)
{
	float dx = cx - qx, dz = cz - qz;
	const float dist = sqrtf(dx * dx + dz * dz);
	const float limit = radius + margin;
	if (dist >= limit)
		return false;
	if (dist > 1e-4f) {
		dx /= dist;
		dz /= dist;
	} else {
		dx = fallbackX;
		dz = fallbackZ;
	}
	respond(def, car, cx, cz, radius, dx, dz, limit - dist);
	return true;
}

bool collision_resolve(const F3DSVehicleDef *def, F3DSVehicleState *car, const F3DSTrack *track)
{
	bool hit = false;
	if (!track)
		return false;
	const float radius = def->trackWidth * 0.5f + 0.15f;
	const float reach = fmaxf(def->wheelBase * 0.5f + 0.75f - radius, 0.0f);

	for (int c = -1; c <= 1; ++c) {
		const FVec3 f = yaw_forward(car->yaw);
		float cx = car->pos.x + f.x * reach * c;
		float cz = car->pos.z + f.z * reach * c;

		for (int i = 0; i < track->wallCount; ++i) {
			if (car->pos.y > TRACK_WALL_HEIGHT)
				break;
			const F3DSWall *w = &track->walls[i];
			const float sx = w->b.x - w->a.x, sz = w->b.z - w->a.z;
			const float len2 = sx * sx + sz * sz;
			const float u = len2 > 0.0f ?
				clampf(((cx - w->a.x) * sx + (cz - w->a.z) * sz) / len2, 0.0f, 1.0f) : 0.0f;
			const float inv = len2 > 0.0f ? 1.0f / sqrtf(len2) : 0.0f;
			if (circle_point(def, car, cx, cz, radius, w->a.x + sx * u, w->a.z + sz * u,
					TRACK_WALL_HALF_THICKNESS, -sz * inv, sx * inv)) {
				hit = true;
				cx = car->pos.x + f.x * reach * c;
				cz = car->pos.z + f.z * reach * c;
			}
		}

		for (int i = 0; i < track->obstacleCount; ++i) {
			const F3DSObstacle *o = &track->obstacles[i];
			if (car->pos.y > o->height)
				continue;
			const float dx = cx - o->center.x, dz = cz - o->center.z;
			if (fabsf(dx) < o->hx && fabsf(dz) < o->hz) {
				/* Centre inside the box: leave by the nearest face. */
				const float px = o->hx - fabsf(dx), pz = o->hz - fabsf(dz);
				if (px < pz)
					respond(def, car, cx, cz, radius, dx >= 0.0f ? 1.0f : -1.0f, 0.0f, px + radius);
				else
					respond(def, car, cx, cz, radius, 0.0f, dz >= 0.0f ? 1.0f : -1.0f, pz + radius);
				hit = true;
			} else if (circle_point(def, car, cx, cz, radius,
					o->center.x + clampf(dx, -o->hx, o->hx),
					o->center.z + clampf(dz, -o->hz, o->hz), 0.0f, 1.0f, 0.0f)) {
				hit = true;
			}
			cx = car->pos.x + f.x * reach * c;
			cz = car->pos.z + f.z * reach * c;
		}

		for (int i = 0; i < track->treeCount; ++i) {
			const F3DSTree *t = &track->trees[i];
			if (fabsf(cx - t->x) > 3.0f || fabsf(cz - t->z) > 3.0f || car->pos.y > t->height)
				continue;
			if (circle_point(def, car, cx, cz, radius, t->x, t->z, TRACK_TRUNK_RADIUS, 1.0f, 0.0f)) {
				hit = true;
				cx = car->pos.x + f.x * reach * c;
				cz = car->pos.z + f.z * reach * c;
			}
		}

		const float r = sqrtf(cx * cx + cz * cz);
		const float limit = track_coast_radius(cx, cz) - radius;
		if (r > limit && r > 0.001f) {
			respond(def, car, cx, cz, radius, -cx / r, -cz / r, r - limit);
			hit = true;
		}
	}
	return hit;
}
