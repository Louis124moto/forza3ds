#include "screens.h"
#include <stdio.h>
#include <string.h>

#define C_BG      RGB565C(11, 17, 25)
#define C_HEADER  RGB565C(17, 27, 38)
#define C_CYAN    RGB565C(0, 214, 232)
#define C_ACCENT  RGB565C(222, 84, 50)
#define C_TEXT    RGB565C(232, 238, 242)
#define C_DIM     RGB565C(130, 150, 164)
#define C_ROW     RGB565C(18, 30, 42)
#define C_SELECT  RGB565C(0, 82, 106)
#define C_LINE    RGB565C(44, 64, 78)
#define C_GREEN   RGB565C(60, 210, 110)
#define C_YELLOW  RGB565C(245, 196, 60)
#define C_ROAD    RGB565C(150, 160, 170)
#define C_GRAVEL  RGB565C(150, 112, 72)

#define ROW_H 15
#define LIST_TOP 38
#define LIST_ROWS 10

static void frame(F3DSCanvas *c, const char *title, const char *help)
{
	canvas_fill(c, 0, 0, CANVAS_W, CANVAS_H, C_BG);
	canvas_fill(c, 0, 0, CANVAS_W, 32, C_HEADER);
	canvas_fill(c, 0, 0, CANVAS_W, 3, C_CYAN);
	canvas_text(c, 10, 8, 1, C_CYAN, "FORZA3DS");
	canvas_text(c, 10, 20, 1, C_TEXT, title);
	for (int i = 0; i < 28; ++i) {
		canvas_fill(c, 280 + i, 5 + i / 2, 2, 2, C_CYAN);
		canvas_fill(c, 291 + i, 5 + i / 2, 2, 2, C_ACCENT);
	}
	canvas_fill(c, 0, 222, CANVAS_W, 18, C_HEADER);
	canvas_fill(c, 0, 221, CANVAS_W, 1, C_CYAN);
	canvas_text(c, 8, 227, 1, C_DIM, help);
}

typedef void (*RowText)(const void *ctx, int row, char *out, int size);

/* Scrolling list keeping the selection visible. */
static void list(F3DSCanvas *c, int count, int selected, int visible, RowText text, const void *ctx)
{
	int first = selected - visible / 2;
	if (first > count - visible) first = count - visible;
	if (first < 0) first = 0;
	for (int i = 0; i < visible && first + i < count; ++i) {
		const int row = first + i;
		const int y = LIST_TOP + i * ROW_H;
		const bool sel = row == selected;
		canvas_fill(c, 10, y, 300, ROW_H - 1, sel ? C_SELECT : C_ROW);
		if (sel) canvas_fill(c, 10, y, 3, ROW_H - 1, C_CYAN);
		char buf[64];
		text(ctx, row, buf, sizeof(buf));
		canvas_text(c, 18, y + 3, 1, sel ? C_TEXT : C_DIM, buf);
	}
	if (first > 0) canvas_text(c, 300, LIST_TOP + 3, 1, C_CYAN, "^");
	if (first + visible < count) canvas_text(c, 300, LIST_TOP + (visible - 1) * ROW_H + 3, 1, C_CYAN, "v");
}

static void names_row(const void *ctx, int row, char *out, int size)
{
	snprintf(out, size, "%s", ((const char *const *)ctx)[row]);
}

static void draw_main(F3DSCanvas *c, const F3DSApp *app)
{
	static const char *items[MAIN_COUNT] = { "Mode Libre", "Mode Progression", "Réglages", "Quitter" };
	frame(c, "Menu principal", "Croix : choisir   A : valider");
	list(c, MAIN_COUNT, app->selected, MAIN_COUNT, names_row, items);
	canvas_text(c, 18, 128, 1, C_TEXT, "Mode Libre : toutes les voitures,");
	canvas_text(c, 18, 140, 1, C_TEXT, "atelier complet, sans économie.");
	canvas_text(c, 18, 160, 1, C_DIM, "SELECT : carte de l'île");
}

static void car_row(const void *ctx, int row, char *out, int size)
{
	(void)ctx;
	snprintf(out, size, "%s", row < F3DS_CAR_COUNT ? catalog_info(row)->name : "Retour");
}

static void draw_cars(F3DSCanvas *c, const F3DSApp *app)
{
	frame(c, "Mode Libre - choix de la voiture", "A : atelier  B : retour  R : moteur");
	list(c, F3DS_CAR_COUNT + 1, app->selected, F3DS_CAR_COUNT + 1, car_row, NULL);
	const int car = app->garage.selected;
	const F3DSVehicleDef *d = &app->garage.def[car];
	char buf[64];
	canvas_text(c, 18, 124, 1, C_CYAN, catalog_info(car)->summary);
	snprintf(buf, sizeof(buf), "Puissance %.0f kW   Masse %.0f kg", vehicle_peak_power_kw(d), d->mass);
	canvas_text(c, 18, 142, 1, C_TEXT, buf);
	const char *drive = d->frontTorqueShare <= 0.001f ? "propulsion" :
		d->frontTorqueShare >= 0.999f ? "traction" : "4 roues motrices";
	snprintf(buf, sizeof(buf), "%s, %d cylindres, %.1f L", drive, d->cylinders, d->displacementLitres);
	canvas_text(c, 18, 156, 1, C_TEXT, buf);
	snprintf(buf, sizeof(buf), "Pneus %s%s", tire_name(d->tireType), d->turboInstalled ? ", turbo" : "");
	canvas_text(c, 18, 170, 1, C_TEXT, buf);
}

static void home_row(const void *ctx, int row, char *out, int size)
{
	const F3DSApp *app = ctx;
	if (row < F3DS_PAGE_COUNT) {
		snprintf(out, size, "%s", workshop_page_name((F3DSWorkshopPage)row));
		return;
	}
	switch (row) {
	case HOME_DRIVE: snprintf(out, size, "ESSAYER EN CONDUITE"); break;
	case HOME_SAVE: snprintf(out, size, "Sauvegarder le garage%s", app->garageDirty ? " *" : ""); break;
	case HOME_RESTORE: snprintf(out, size, "Restaurer le réglage d'origine"); break;
	default: snprintf(out, size, "Changer de voiture"); break;
	}
}

static void draw_workshop(F3DSCanvas *c, const F3DSApp *app)
{
	char title[64];
	snprintf(title, sizeof(title), "Atelier - %s", catalog_info(app->garage.selected)->name);
	frame(c, title, "A : ouvrir  B : voitures  R : moteur");
	list(c, HOME_COUNT, app->selected, HOME_COUNT, home_row, app);
	const F3DSVehicleDef *d = &app->garage.def[app->garage.selected];
	char buf[64];
	snprintf(buf, sizeof(buf), "%.0f kW  %.0f tr/min  %s", vehicle_peak_power_kw(d), d->maxRpm,
		tire_name(d->tireType));
	canvas_text(c, 18, 192, 1, C_TEXT, buf);
}

static void page_row(const void *ctx, int row, char *out, int size)
{
	const F3DSApp *app = ctx;
	if (row < workshop_item_count(app->page))
		workshop_item_text(&app->garage, app->page, row, out, size);
	else
		snprintf(out, size, "Retour");
}

static void draw_workshop_page(F3DSCanvas *c, const F3DSApp *app)
{
	char title[64];
	snprintf(title, sizeof(title), "Atelier - %s", workshop_page_name(app->page));
	frame(c, title, "Gauche/Droite : régler  B : retour");
	list(c, workshop_item_count(app->page) + 1, app->selected, LIST_ROWS, page_row, app);
	const F3DSVehicleDef *d = &app->garage.def[app->garage.selected];
	char buf[64];
	snprintf(buf, sizeof(buf), "Puissance max : %.0f kW", vehicle_peak_power_kw(d));
	canvas_text(c, 18, 196, 1, C_CYAN, buf);
	canvas_text(c, 18, 209, 1, C_DIM, "R : faire monter en régime");
}

static void pause_row(const void *ctx, int row, char *out, int size)
{
	static const char *items[PAUSE_COUNT] = {
		"Reprendre", "Replacer au départ", "Carte", "Réglages", "Retour à l'atelier",
		"Changer de voiture", "Menu principal"
	};
	(void)ctx;
	snprintf(out, size, "%s", items[row]);
}

static void draw_pause(F3DSCanvas *c, const F3DSApp *app)
{
	frame(c, "Pause", "A : valider   B/START : reprendre");
	list(c, PAUSE_COUNT, app->selected, PAUSE_COUNT, pause_row, NULL);
	if (app->garageDirty)
		canvas_text(c, 18, 160, 1, C_YELLOW, "Garage modifié non sauvegardé");
}

static void settings_row(const void *ctx, int row, char *out, int size)
{
	const F3DSApp *app = ctx;
	if (row < settings_item_count())
		settings_item_text(&app->settings, row, out, size);
	else
		snprintf(out, size, "Retour");
}

static void draw_settings(F3DSCanvas *c, const F3DSApp *app)
{
	frame(c, "Réglages", app->rebindAction >= 0 ? "Appuie sur une touche  START : annuler"
		: "Gauche/Droite : régler  B : retour");
	list(c, settings_item_count() + 1, app->selected, 11, settings_row, app);
	if (app->rebindAction >= 0) {
		canvas_fill(c, 20, 90, 280, 50, C_HEADER);
		canvas_fill(c, 20, 90, 280, 2, C_ACCENT);
		canvas_text(c, 32, 100, 1, C_TEXT, input_action_name((F3DSAction)app->rebindAction));
		canvas_text(c, 32, 118, 1, C_DIM, "A B X Y L R ZL ZR");
	} else {
		canvas_text(c, 18, 208, 1, C_DIM, "Sauvegarde automatique à la sortie");
	}
}

/* World (x, z) to screen: north (+Z) up, and +X to the left as the driver
 * facing north sees it. */
static void map_point(float wx, float wz, float cx, float cy, float scale, float *sx, float *sy)
{
	*sx = cx - wx * scale;
	*sy = cy - wz * scale;
}

static void draw_track_map(F3DSCanvas *c, const F3DSApp *app, float cx, float cy, float scale,
	int road, bool detail)
{
	for (int i = 0; i < TRACK_POINTS; ++i) {
		float x0, y0, x1, y1;
		map_point(app->track.pts[i].x, app->track.pts[i].z, cx, cy, scale, &x0, &y0);
		map_point(app->track.pts[i + 1].x, app->track.pts[i + 1].z, cx, cy, scale, &x1, &y1);
		canvas_line(c, x0, y0, x1, y1, road, C_ROAD);
	}
	for (int i = 0; i < app->terrain.slickCount; ++i) {
		float x, y;
		map_point(app->terrain.slicks[i].x, app->terrain.slicks[i].z, cx, cy, scale, &x, &y);
		canvas_disc(c, x, y, fmaxf(app->terrain.slicks[i].radius * scale, 2.0f), C_GRAVEL);
	}
	if (detail) {
		for (int i = 0; i < app->terrain.rampCount; ++i) {
			float x, y;
			map_point(app->terrain.ramps[i].origin.x, app->terrain.ramps[i].origin.z, cx, cy, scale, &x, &y);
			canvas_fill(c, (int)x - 3, (int)y - 3, 7, 7, C_YELLOW);
		}
	}
	/* Car position and heading. */
	float x, y;
	map_point(app->car.pos.x, app->car.pos.z, cx, cy, scale, &x, &y);
	const FVec3 f = yaw_forward(app->car.yaw);
	canvas_disc(c, x, y, detail ? 4.0f : 3.0f, C_ACCENT);
	canvas_line(c, x, y, x - f.x * (detail ? 12.0f : 8.0f), y - f.z * (detail ? 12.0f : 8.0f), 2, C_CYAN);
}

static void draw_map(F3DSCanvas *c, const F3DSApp *app)
{
	frame(c, "Carte de l'île", "SELECT : fermer la carte");
	draw_track_map(c, app, 160.0f, 127.0f, 0.40f, 2, true);
	canvas_text(c, 12, 40, 1, C_TEXT, "N");
	canvas_line(c, 15, 62, 15, 50, 1, C_CYAN);
	canvas_text(c, 230, 196, 1, C_YELLOW, "Rampe");
	canvas_text(c, 230, 208, 1, C_GRAVEL, "Gravier");
}

static void arc_gauge(F3DSCanvas *c, float cx, float cy, float radius, float value, float redline)
{
	const int segments = 36;
	const float start = 0.75f * F3DS_PI, sweep = 1.5f * F3DS_PI;
	for (int i = 0; i < segments; ++i) {
		const float t = ((float)i + 0.5f) / segments;
		const float a = start + sweep * t;
		uint16_t color = t > redline ? C_ACCENT : C_CYAN;
		if (t > value) color = t > redline ? RGB565C(80, 32, 24) : C_LINE;
		canvas_line(c, cx + cosf(a) * (radius - 10.0f), cy + sinf(a) * (radius - 10.0f),
			cx + cosf(a) * radius, cy + sinf(a) * radius, 4, color);
	}
}

static void indicator(F3DSCanvas *c, int x, int y, const char *label, bool on, uint16_t color)
{
	canvas_fill(c, x, y, 34, 13, on ? color : C_ROW);
	canvas_text(c, x + (34 - canvas_text_width(label, 1)) / 2, y + 3, 1, on ? C_BG : C_DIM, label);
}

static void draw_hud(F3DSCanvas *c, const F3DSApp *app)
{
	const F3DSVehicleState *car = &app->car;
	const F3DSVehicleDef *d = &app->garage.def[app->garage.selected];
	canvas_fill(c, 0, 0, CANVAS_W, CANVAS_H, C_BG);
	if (app->settings.showFps) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%d FPS", app->loop.fps);
		canvas_text(c, 4, 4, 1, C_YELLOW, buf);
	}
	if (!app->settings.showHud) {
		canvas_text(c, 76, 225, 1, C_LINE, "START : pause");
		return;
	}

	const float rpmMax = fmaxf(d->maxRpm * 1.05f, 1.0f);
	arc_gauge(c, 100.0f, 108.0f, 88.0f, car->rpm / rpmMax, d->maxRpm * 0.92f / rpmMax);
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", (int)lroundf(fabsf(car->speed) * 3.6f));
	canvas_text(c, 100 - canvas_text_width(buf, 4) / 2, 72, 4, C_TEXT, buf);
	canvas_text(c, 84, 108, 1, C_DIM, "km/h");
	if (car->gear == 0) snprintf(buf, sizeof(buf), "R");
	else snprintf(buf, sizeof(buf), "%d", car->gear);
	canvas_text(c, 100 - canvas_text_width(buf, 4) / 2, 128, 4,
		car->revLimited ? C_ACCENT : C_CYAN, buf);
	snprintf(buf, sizeof(buf), "%d tr/min", (int)car->rpm);
	canvas_text(c, 100 - canvas_text_width(buf, 1) / 2, 172, 1, C_DIM, buf);
	canvas_text(c, 4, 172, 1, C_DIM, d->manualTransmission ? "MAN" : "AUTO");

	if (d->turboInstalled) {
		const float fill = clampf(car->boost / fmaxf(d->turboBoostBar, 0.1f), 0.0f, 1.0f);
		canvas_fill(c, 20, 190, 160, 8, C_ROW);
		canvas_fill(c, 20, 190, (int)(160.0f * fill), 8, C_YELLOW);
		snprintf(buf, sizeof(buf), "Turbo %.1f bar", car->boost);
		canvas_text(c, 20, 202, 1, C_DIM, buf);
	}

	indicator(c, 4, 218, "ABS", car->absActive, C_YELLOW);
	indicator(c, 42, 218, "TCS", car->tcsActive, C_YELLOW);
	indicator(c, 80, 218, "DRIFT", car->slip[1] > 0.25f, C_ACCENT);
	indicator(c, 118, 218, "AIR", car->airborne, C_CYAN);

	/* Mini map. */
	canvas_fill(c, 196, 8, 120, 128, C_HEADER);
	draw_track_map(c, app, 256.0f, 72.0f, 0.23f, 1, false);
	static const char *surfaces[F3DS_SURFACE_COUNT] = { "Asphalte", "Gravier", "Herbe" };
	canvas_text(c, 200, 144, 1, car->surface == F3DS_SURFACE_ASPHALT ? C_GREEN : C_YELLOW,
		surfaces[(unsigned)car->surface % F3DS_SURFACE_COUNT]);
	canvas_text(c, 200, 160, 1, C_DIM, catalog_info(app->garage.selected)->name);
	canvas_text(c, 200, 196, 1, C_DIM, "SELECT carte");
	canvas_text(c, 200, 208, 1, C_DIM, "START pause");
}

void screens_draw(F3DSCanvas *c, const F3DSApp *app)
{
	if (app->mapVisible) {
		draw_map(c, app);
		return;
	}
	switch (app->screen) {
	case F3DS_SCREEN_MAIN: draw_main(c, app); break;
	case F3DS_SCREEN_CARS: draw_cars(c, app); break;
	case F3DS_SCREEN_WORKSHOP: draw_workshop(c, app); break;
	case F3DS_SCREEN_WORKSHOP_PAGE: draw_workshop_page(c, app); break;
	case F3DS_SCREEN_DRIVE: draw_hud(c, app); break;
	case F3DS_SCREEN_PAUSE: draw_pause(c, app); break;
	case F3DS_SCREEN_SETTINGS: draw_settings(c, app); break;
	case F3DS_SCREEN_PROGRESSION:
		frame(c, "Mode Progression", "B : retour au menu");
		canvas_text(c, 18, 60, 1, C_TEXT, "Pas encore disponible.");
		canvas_text(c, 18, 80, 1, C_DIM, "Collection, découvertes, courses");
		canvas_text(c, 18, 92, 1, C_DIM, "et économie arrivent à l'étape 11.");
		canvas_text(c, 18, 116, 1, C_TEXT, "Le Mode Libre est jouable.");
		break;
	case F3DS_SCREEN_CONFIRM_QUIT:
		frame(c, "Quitter ?", "A : quitter   B : retour");
		canvas_text(c, 18, 70, 1, C_YELLOW, "Le garage Libre a des changements");
		canvas_text(c, 18, 84, 1, C_YELLOW, "non sauvegardés.");
		canvas_text(c, 18, 110, 1, C_TEXT, "Pour les garder : B, Mode Libre,");
		canvas_text(c, 18, 124, 1, C_TEXT, "puis Sauvegarder le garage.");
		break;
	default: break;
	}
	if (app->message[0]) {
		canvas_fill(c, 0, 204, CANVAS_W, 16, C_SELECT);
		canvas_text(c, 8, 208, 1, C_TEXT, app->message);
	}
}
