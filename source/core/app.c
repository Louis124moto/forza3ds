#include "app.h"
#include "physics.h"
#include <stdio.h>
#include <string.h>

#define MAX_CATCH_UP (6.0f / 60.0f)

static void touch(F3DSApp *app) { app->uiRevision++; }

static void notify(F3DSApp *app, const char *text)
{
	snprintf(app->message, sizeof(app->message), "%s", text);
	app->messageTime = 3.0f;
	touch(app);
}

static F3DSVehicleDef *current_def(F3DSApp *app) { return &app->garage.def[app->garage.selected]; }

static void place_car_at_start(F3DSApp *app)
{
	const float yaw = atan2f(app->track.fwd[0].x, app->track.fwd[0].z);
	app->car.visualModelId = app->garage.selected;
	vehicle_state_reset(&app->car, current_def(app), app->track.pts[0], yaw);
	app->prevPos = app->car.pos;
	app->prevYaw = app->car.yaw;
	app->pendingGearUp = app->pendingGearDown = 0;
	app->camera.orbitYaw = 0.0f;
	app->camera.wasStopped = true;
	app->camera.recentering = false;
	app->loop.accumulator = 0.0f;
}

static void camera_from_settings(F3DSApp *app)
{
	app->camera.mode = (F3DSCameraMode)app->settings.cameraMode;
	app->camera.chaseDistance = app->settings.chaseDistance;
	app->camera.chaseHeight = app->settings.chaseHeight;
	app->camera.recenterSpeed = app->settings.recenterSpeed;
}

/* Camera changes made while driving become the saved custom position. */
static void camera_to_settings(F3DSApp *app)
{
	if (app->settings.cameraMode != (int)app->camera.mode ||
	    fabsf(app->settings.chaseDistance - app->camera.chaseDistance) > 0.001f ||
	    fabsf(app->settings.chaseHeight - app->camera.chaseHeight) > 0.001f) {
		app->settings.cameraMode = app->camera.mode;
		app->settings.chaseDistance = app->camera.chaseDistance;
		app->settings.chaseHeight = app->camera.chaseHeight;
		app->settingsDirty = true;
	}
}

static void autosave_settings(F3DSApp *app)
{
	camera_to_settings(app);
	if (!app->settingsDirty)
		return;
	if (settings_save(&app->settings, app->settingsPath))
		app->settingsDirty = false;
	else
		notify(app, "Réglages non sauvegardés (carte SD)");
}

static void go(F3DSApp *app, F3DSScreen screen, int selected)
{
	app->screen = screen;
	app->selected = selected;
	app->rebindAction = -1;
	app->loop.accumulator = 0.0f;
	app->pendingGearUp = app->pendingGearDown = 0;
	touch(app);
}

void app_init(F3DSApp *app, const char *dataDir)
{
	memset(app, 0, sizeof(*app));
	snprintf(app->settingsPath, sizeof(app->settingsPath), "%s/settings.bin", dataDir);
	snprintf(app->garagePath, sizeof(app->garagePath), "%s/saves/free_garage.bin", dataDir);
	settings_defaults(&app->settings);
	settings_load(&app->settings, app->settingsPath);
	garage_defaults(&app->garage);
	F3DSGarage saved;
	if (garage_load(&saved, app->garagePath))
		app->garage = saved;
	else {
		FILE *f = fopen(app->garagePath, "rb");
		if (f) {
			fclose(f);
			snprintf(app->message, sizeof(app->message), "Garage illisible : réglages d'origine");
			app->messageTime = 6.0f;
		}
	}
	track_init(&app->track);
	terrain_init(&app->terrain, &app->track);
	core_loop_init(&app->loop, settings_sim_dt(&app->settings), MAX_CATCH_UP);
	camera_init(&app->camera);
	camera_from_settings(app);
	app->rebindAction = -1;
	app->inspectAngle = 0.7f;
	place_car_at_start(app);
	app->uiRevision = 1;
}

static void move_selection(F3DSApp *app, int count)
{
	if (count <= 0)
		return;
	if (app->input.menuUp) { app->selected = (app->selected + count - 1) % count; touch(app); }
	if (app->input.menuDown) { app->selected = (app->selected + 1) % count; touch(app); }
}

static void open_settings(F3DSApp *app)
{
	camera_to_settings(app);
	app->settingsReturn = app->screen;
	go(app, F3DS_SCREEN_SETTINGS, 0);
}

static void close_settings(F3DSApp *app)
{
	autosave_settings(app);
	go(app, app->settingsReturn, app->settingsReturn == F3DS_SCREEN_MAIN ? MAIN_SETTINGS : PAUSE_SETTINGS);
}

static void update_settings(F3DSApp *app)
{
	const F3DSInputState *in = &app->input;
	if (app->rebindAction >= 0) {
		const u32 key = in->keysDown & F3DS_REBIND_KEYS;
		if (in->pausePressed) {
			app->rebindAction = -1;
			touch(app);
		} else if (key && !(key & (key - 1u))) {
			input_rebind(&app->settings.input, (F3DSAction)app->rebindAction, key);
			app->settingsDirty = true;
			app->rebindAction = -1;
			touch(app);
		}
		return;
	}
	const int count = settings_item_count() + 1; /* + back */
	move_selection(app, count);
	const bool onBack = app->selected == count - 1;
	if (in->menuBack || in->pausePressed || (in->menuConfirm && onBack)) {
		close_settings(app);
		return;
	}
	F3DSAction action;
	if (settings_item_binding(app->selected, &action)) {
		if (in->menuConfirm) { app->rebindAction = action; touch(app); }
		return;
	}
	const int dir = in->menuLeft ? -1 : (in->menuRight || in->menuConfirm) ? 1 : 0;
	if (dir && settings_adjust(&app->settings, app->selected, dir)) {
		app->settingsDirty = true;
		camera_from_settings(app);
		app->loop.dt = settings_sim_dt(&app->settings);
		touch(app);
	}
}

static void update_main(F3DSApp *app)
{
	move_selection(app, MAIN_COUNT);
	if (!app->input.menuConfirm)
		return;
	switch (app->selected) {
	case MAIN_FREE: go(app, F3DS_SCREEN_CARS, app->garage.selected); break;
	case MAIN_PROGRESSION: go(app, F3DS_SCREEN_PROGRESSION, 0); break;
	case MAIN_SETTINGS: open_settings(app); break;
	default:
		if (app->garageDirty) go(app, F3DS_SCREEN_CONFIRM_QUIT, 0);
		else app->quit = true;
		break;
	}
}

static void select_car(F3DSApp *app, int car)
{
	if (app->garage.selected != car) {
		app->garage.selected = car;
		place_car_at_start(app);
	}
}

static void update_cars(F3DSApp *app)
{
	move_selection(app, F3DS_CAR_COUNT + 1);
	/* Left and right jump a whole page of the list. */
	const int page = app->input.menuRight ? 5 : app->input.menuLeft ? -5 : 0;
	if (page) {
		const int next = app->selected + page;
		app->selected = next < 0 ? 0 : next > F3DS_CAR_COUNT ? F3DS_CAR_COUNT : next;
		touch(app);
	}
	if (app->selected < F3DS_CAR_COUNT)
		select_car(app, app->selected); /* live preview in the garage */
	if (app->input.menuBack || (app->input.menuConfirm && app->selected == F3DS_CAR_COUNT)) {
		go(app, F3DS_SCREEN_MAIN, MAIN_FREE);
	} else if (app->input.menuConfirm) {
		app->homeSelected = HOME_ENGINE;
		go(app, F3DS_SCREEN_WORKSHOP, HOME_ENGINE);
	}
}

static void update_workshop(F3DSApp *app)
{
	move_selection(app, HOME_COUNT);
	if (app->input.menuBack) {
		go(app, F3DS_SCREEN_CARS, app->garage.selected);
		return;
	}
	if (!app->input.menuConfirm)
		return;
	app->homeSelected = app->selected;
	switch (app->selected) {
	case HOME_DRIVE:
		place_car_at_start(app);
		go(app, F3DS_SCREEN_DRIVE, 0);
		break;
	case HOME_SAVE:
		if (garage_save(&app->garage, app->garagePath)) {
			app->garageDirty = false;
			notify(app, "Garage Libre sauvegardé");
		} else {
			notify(app, "Échec de la sauvegarde (carte SD)");
		}
		break;
	case HOME_RESTORE:
		garage_restore_car(&app->garage, app->garage.selected);
		app->garageDirty = true;
		notify(app, "Réglage d'origine restauré");
		break;
	case HOME_CARS:
		go(app, F3DS_SCREEN_CARS, app->garage.selected);
		break;
	default:
		app->page = (F3DSWorkshopPage)app->selected;
		go(app, F3DS_SCREEN_WORKSHOP_PAGE, 0);
		break;
	}
}

static void update_workshop_page(F3DSApp *app)
{
	const int count = workshop_item_count(app->page) + 1;
	move_selection(app, count);
	if (app->input.menuBack || (app->input.menuConfirm && app->selected == count - 1)) {
		go(app, F3DS_SCREEN_WORKSHOP, app->homeSelected);
		return;
	}
	const int dir = app->input.menuLeft ? -1 : (app->input.menuRight || app->input.menuConfirm) ? 1 : 0;
	if (dir && app->selected < count - 1 &&
	    workshop_adjust(&app->garage, app->page, app->selected, dir)) {
		app->garageDirty = true;
		touch(app);
	}
}

static void resume_drive(F3DSApp *app)
{
	autosave_settings(app);
	go(app, F3DS_SCREEN_DRIVE, 0);
}

static void update_pause(F3DSApp *app)
{
	move_selection(app, PAUSE_COUNT);
	if (app->input.menuBack || app->input.pausePressed) {
		resume_drive(app);
		return;
	}
	if (!app->input.menuConfirm)
		return;
	switch (app->selected) {
	case PAUSE_RESUME: resume_drive(app); break;
	case PAUSE_RESET: place_car_at_start(app); resume_drive(app); break;
	case PAUSE_MAP: app->mapVisible = true; touch(app); break;
	case PAUSE_SETTINGS: open_settings(app); break;
	case PAUSE_WORKSHOP:
		autosave_settings(app);
		place_car_at_start(app);
		app->homeSelected = HOME_DRIVE;
		go(app, F3DS_SCREEN_WORKSHOP, HOME_DRIVE);
		break;
	case PAUSE_CARS:
		autosave_settings(app);
		place_car_at_start(app);
		go(app, F3DS_SCREEN_CARS, app->garage.selected);
		break;
	default:
		autosave_settings(app);
		place_car_at_start(app);
		go(app, F3DS_SCREEN_MAIN, MAIN_FREE);
		break;
	}
}

static void update_drive(F3DSApp *app, int steps)
{
	app->pendingGearUp |= app->input.gearUp;
	app->pendingGearDown |= app->input.gearDown;
	app->input.absEnabled = app->settings.abs;
	app->input.tcsEnabled = app->settings.tcs;
	float impact = 0.0f;
	for (int i = 0; i < steps; ++i) {
		F3DSInputState step = app->input;
		/* A shift request is delivered to exactly one simulation step, even
		 * if this frame had none. */
		step.gearUp = app->pendingGearUp;
		step.gearDown = app->pendingGearDown;
		app->pendingGearUp = app->pendingGearDown = 0;
		app->prevPos = app->car.pos;
		app->prevYaw = app->car.yaw;
		physics_step(current_def(app), &app->car, &step, &app->track, &app->terrain, app->loop.dt);
		impact = fmaxf(impact, app->car.impact);
	}
	if (steps > 0)
		app->car.impact = impact;
}

/* Stationary engine preview in the garage; holding R revs it. */
static void update_preview_engine(F3DSApp *app, bool rev, float dt)
{
	const F3DSVehicleDef *d = current_def(app);
	F3DSVehicleState *c = &app->car;
	const float target = rev ? d->maxRpm * 0.96f : d->idleRpm;
	c->rpm = approachf(clampf(c->rpm, d->idleRpm, d->maxRpm), target, (rev ? 5200.0f : 3800.0f) * dt);
	c->engineLoad = rev ? 0.9f : 0.08f;
	const float spool = smoothstepf((c->rpm - d->idleRpm) / fmaxf((d->maxRpm - d->idleRpm) * 0.5f, 1.0f));
	c->boost = approachf(c->boost, d->turboInstalled && rev ? d->turboBoostBar * spool : 0.0f,
		(rev ? 2.0f : 6.0f) * fmaxf(d->turboBoostBar, 0.5f) * dt);
}

static bool in_garage(const F3DSApp *app)
{
	switch (app->screen) {
	case F3DS_SCREEN_DRIVE:
	case F3DS_SCREEN_PAUSE:
		return false;
	case F3DS_SCREEN_SETTINGS:
		return app->settingsReturn != F3DS_SCREEN_PAUSE;
	default:
		return true;
	}
}

void app_update(F3DSApp *app, const F3DSRawInput *raw, float elapsed)
{
	if (!isfinite(elapsed) || elapsed < 0.0f)
		elapsed = 0.0f;
	input_map(&app->input, &app->settings.input, raw);
	const int steps = core_loop_advance(&app->loop, elapsed);
	const float dt = fminf(elapsed, 0.1f);
	app->car.impact = 0.0f;
	if (app->messageTime > 0.0f) {
		app->messageTime -= dt;
		if (app->messageTime <= 0.0f) { app->message[0] = '\0'; touch(app); }
	}

	/* The map can be opened from anywhere. */
	const bool binding = app->screen == F3DS_SCREEN_SETTINGS && app->rebindAction >= 0;
	if (app->input.mapPressed && !binding) {
		app->mapVisible = !app->mapVisible;
		touch(app);
	} else if (app->mapVisible && app->screen != F3DS_SCREEN_DRIVE) {
		/* In menus the map takes the input until closed. */
		if (app->input.menuBack || app->input.menuConfirm || app->input.pausePressed) {
			app->mapVisible = false;
			touch(app);
		}
		return;
	}

	if (in_garage(app)) {
		app->inspectAngle = wrap_angle(app->inspectAngle +
			(fabsf(app->input.orbit) > 0.02f ? -app->input.orbit * 2.4f : 0.18f) * dt);
		update_preview_engine(app, (raw->held & KEY_R) != 0, dt);
	}

	switch (app->screen) {
	case F3DS_SCREEN_MAIN: update_main(app); break;
	case F3DS_SCREEN_CARS: update_cars(app); break;
	case F3DS_SCREEN_WORKSHOP: update_workshop(app); break;
	case F3DS_SCREEN_WORKSHOP_PAGE: update_workshop_page(app); break;
	case F3DS_SCREEN_DRIVE:
		if (app->input.pausePressed) {
			camera_to_settings(app);
			go(app, F3DS_SCREEN_PAUSE, PAUSE_RESUME);
			break;
		}
		update_drive(app, steps);
		break;
	case F3DS_SCREEN_PAUSE: update_pause(app); break;
	case F3DS_SCREEN_SETTINGS: update_settings(app); break;
	case F3DS_SCREEN_PROGRESSION:
		if (app->input.menuBack || app->input.menuConfirm)
			go(app, F3DS_SCREEN_MAIN, MAIN_PROGRESSION);
		break;
	case F3DS_SCREEN_CONFIRM_QUIT:
		if (app->input.menuConfirm) app->quit = true;
		else if (app->input.menuBack) go(app, F3DS_SCREEN_MAIN, MAIN_QUIT);
		break;
	default: break;
	}

	if (app->screen == F3DS_SCREEN_DRIVE) {
		F3DSView v;
		app_view(app, &v);
		camera_update(&app->camera, catalog_visual(v.carId), v.pos, v.yaw, app->car.speed,
			&app->input, dt);
	} else if (app->screen == F3DS_SCREEN_PAUSE ||
		   (app->screen == F3DS_SCREEN_SETTINGS && !in_garage(app))) {
		/* Preview camera settings without letting the pause inputs move it. */
		F3DSInputState still;
		memset(&still, 0, sizeof(still));
		F3DSCamera *cam = &app->camera;
		camera_update(cam, catalog_visual(app->garage.selected), app->car.pos, app->car.yaw,
			0.0f, &still, 0.0f);
	}
}

void app_suspend(F3DSApp *app)
{
	if (app->screen == F3DS_SCREEN_DRIVE) {
		camera_to_settings(app);
		go(app, F3DS_SCREEN_PAUSE, PAUSE_RESUME);
	}
	app->loop.accumulator = 0.0f;
}

void app_shutdown(F3DSApp *app)
{
	autosave_settings(app);
}

void app_view(const F3DSApp *app, F3DSView *v)
{
	memset(v, 0, sizeof(*v));
	v->garageScene = in_garage(app);
	v->carId = app->garage.selected;
	v->paint = app->garage.paint[v->carId];
	v->rim = app->garage.rim[v->carId];
	v->def = &app->garage.def[v->carId];
	v->state = &app->car;
	v->shadows = app->settings.shadows;
	v->drawDistance = app->settings.drawDistance;
	if (v->garageScene) {
		/* Inspection camera orbiting the car inside the garage. */
		v->pos = app->track.pts[0];
		v->yaw = 0.0f;
		v->eye = fvec3(v->pos.x + sinf(app->inspectAngle) * 6.4f, 2.5f,
			v->pos.z + cosf(app->inspectAngle) * 6.4f);
		v->target = fvec3(v->pos.x, 0.6f, v->pos.z);
		return;
	}
	const float a = app->screen == F3DS_SCREEN_DRIVE ? core_loop_alpha(&app->loop) : 1.0f;
	v->pos = fvec3_add(app->prevPos, fvec3_scale(fvec3_sub(app->car.pos, app->prevPos), a));
	v->yaw = wrap_angle(app->prevYaw + wrap_angle(app->car.yaw - app->prevYaw) * a);
	v->cockpit = app->camera.mode == F3DS_CAM_COCKPIT;
	v->eye = app->camera.eye;
	v->target = app->camera.target;

	const FVec3 f = yaw_forward(v->yaw);
	const float half = v->def->wheelBase * 0.5f;
	const float front = terrain_height(&app->terrain, v->pos.x + f.x * half, v->pos.z + f.z * half);
	const float rear = terrain_height(&app->terrain, v->pos.x - f.x * half, v->pos.z - f.z * half);
	v->groundY = terrain_height(&app->terrain, v->pos.x, v->pos.z);
	/* Sides and backs of ramps are steps, not slopes. */
	v->groundPitch = fabsf(front - rear) < 0.8f ? atan2f(front - rear, v->def->wheelBase) : 0.0f;
}

bool app_audio_paused(const F3DSApp *app)
{
	if (app->screen == F3DS_SCREEN_PAUSE)
		return true;
	return app->screen == F3DS_SCREEN_SETTINGS && app->settingsReturn == F3DS_SCREEN_PAUSE;
}

void app_audio_params(F3DSApp *app, F3DSSynthParams *p)
{
	const F3DSVehicleDef *d = current_def(app);
	synth_params_from_car(p, d, &app->car);
	p->engineVolume = app->settings.engineVolume;
	p->ambienceVolume = app->settings.ambienceVolume;
	const bool driving = app->screen == F3DS_SCREEN_DRIVE;
	p->cockpit = driving && app->camera.mode == F3DS_CAM_COCKPIT;
	if (!driving) {
		p->speed = 0.0f;
		p->slip = 0.0f;
		p->impact = 0.0f;
	}
	/* Silence in the main menu, progression notice and quit prompt. */
	p->active = !app_audio_paused(app) && app->screen != F3DS_SCREEN_MAIN &&
		app->screen != F3DS_SCREEN_PROGRESSION && app->screen != F3DS_SCREEN_CONFIRM_QUIT &&
		!(app->screen == F3DS_SCREEN_SETTINGS && app->settingsReturn == F3DS_SCREEN_MAIN);
}
