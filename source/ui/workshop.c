#include "workshop.h"
#include <stdio.h>

const char *workshop_page_name(F3DSWorkshopPage page)
{
	static const char *names[F3DS_PAGE_COUNT] = {
		"Moteur", "Boîte de vitesses", "Différentiels", "Suspension", "Pneus et freins", "Apparence"
	};
	return (unsigned)page < F3DS_PAGE_COUNT ? names[page] : "?";
}

enum { E_TORQUE, E_RPM, E_TURBO, E_BOOST, E_INTAKE, E_EXHAUST, E_CAM, E_INJECTORS, E_PISTONS, E_COUNT };
enum { G_MODE, G_COUNT_GEARS, G_FINAL, G_RATIO, G_COUNT = G_RATIO + F3DS_MAX_GEARS };
enum { D_SPLIT, D_FRONT, D_CENTER, D_REAR, D_COUNT };
enum { S_COUNT = 8 };
enum { T_TYPE, T_GRIP, T_BRAKE, T_BIAS, T_HAND, T_COUNT };
enum { L_PAINT, L_RIM, L_COUNT };

int workshop_item_count(F3DSWorkshopPage page)
{
	switch (page) {
	case F3DS_PAGE_ENGINE: return E_COUNT;
	case F3DS_PAGE_GEARBOX: return G_COUNT;
	case F3DS_PAGE_DIFF: return D_COUNT;
	case F3DS_PAGE_SUSPENSION: return S_COUNT;
	case F3DS_PAGE_TIRES: return T_COUNT;
	case F3DS_PAGE_LOOK: return L_COUNT;
	default: return 0;
	}
}

static int pct(float v) { return (int)lroundf(v * 100.0f); }

void workshop_item_text(const F3DSGarage *g, F3DSWorkshopPage page, int item, char *out, int size)
{
	const int car = g->selected;
	const F3DSVehicleDef *d = &g->def[car];
	snprintf(out, size, "?");
	switch (page) {
	case F3DS_PAGE_ENGINE:
		switch (item) {
		case E_TORQUE: snprintf(out, size, "Couple de base : %.0f Nm", d->torqueBase); break;
		case E_RPM: snprintf(out, size, "Régime maximal : %.0f tr/min", d->maxRpm); break;
		case E_TURBO: snprintf(out, size, "Turbo : %s", d->turboInstalled ? "monté" : "absent"); break;
		case E_BOOST: snprintf(out, size, "Pression turbo : %.1f bar", d->turboBoostBar); break;
		case E_INTAKE: snprintf(out, size, "Admission : niveau %d", d->intakeLevel); break;
		case E_EXHAUST: snprintf(out, size, "Échappement : niveau %d", d->exhaustLevel); break;
		case E_CAM: snprintf(out, size, "Arbre à cames : niveau %d", d->camshaftLevel); break;
		case E_INJECTORS: snprintf(out, size, "Injecteurs : niveau %d", d->injectorLevel); break;
		case E_PISTONS: snprintf(out, size, "Pistons : niveau %d", d->pistonLevel); break;
		default: break;
		}
		break;
	case F3DS_PAGE_GEARBOX:
		if (item == G_MODE) snprintf(out, size, "Boîte : %s", d->manualTransmission ? "manuelle" : "automatique");
		else if (item == G_COUNT_GEARS) snprintf(out, size, "Nombre de rapports : %d", d->gearCount);
		else if (item == G_FINAL) snprintf(out, size, "Rapport de pont : %.2f", d->finalDrive);
		else if (item < G_COUNT)
			snprintf(out, size, "Rapport %d : %.2f%s", item - G_RATIO + 1, d->gearRatios[item - G_RATIO],
				item - G_RATIO < d->gearCount ? "" : " (inactif)");
		break;
	case F3DS_PAGE_DIFF:
		if (item == D_SPLIT) snprintf(out, size, "Couple avant / arrière : %d / %d", pct(d->frontTorqueShare), 100 - pct(d->frontTorqueShare));
		else if (item == D_FRONT) snprintf(out, size, "Blocage avant : %d %%", pct(d->diffFrontLock));
		else if (item == D_CENTER) snprintf(out, size, "Blocage central : %d %%", pct(d->diffCenterLock));
		else if (item == D_REAR) snprintf(out, size, "Blocage arrière : %d %%", pct(d->diffRearLock));
		break;
	case F3DS_PAGE_SUSPENSION: {
		const int a = item / 4;
		const char *side = a == F3DS_FRONT ? "AV" : "AR";
		switch (item % 4) {
		case 0: snprintf(out, size, "Hauteur %s : %.0f mm", side, d->rideHeight[a] * 1000.0f); break;
		case 1: snprintf(out, size, "Ressort %s : %.0f N/mm", side, d->springRate[a] / 1000.0f); break;
		case 2: snprintf(out, size, "Compression %s : %.0f Ns/m", side, d->damperCompression[a]); break;
		default: snprintf(out, size, "Détente %s : %.0f Ns/m", side, d->damperRebound[a]); break;
		}
		break;
	}
	case F3DS_PAGE_TIRES:
		if (item == T_TYPE) snprintf(out, size, "Pneus : %s", tire_name(d->tireType));
		else if (item == T_GRIP) snprintf(out, size, "Gomme (adhérence) : %d %%", pct(d->tireGrip));
		else if (item == T_BRAKE) snprintf(out, size, "Force de freinage : %.1f kN", d->brakeForce / 1000.0f);
		else if (item == T_BIAS) snprintf(out, size, "Répartition avant : %d %%", pct(d->brakeBiasFront));
		else if (item == T_HAND) snprintf(out, size, "Frein à main : %.1f kN", d->handbrakeForce / 1000.0f);
		break;
	case F3DS_PAGE_LOOK:
		if (item == L_PAINT) snprintf(out, size, "Peinture : %s", catalog_paint_name(g->paint[car]));
		else if (item == L_RIM) snprintf(out, size, "Jantes : %s", catalog_rim_name(g->rim[car]));
		break;
	default:
		break;
	}
}

/* Increment on a fixed grid, with a floor and no ceiling. */
static float raise(float v, int dir, float inc, float floor_)
{
	const float next = roundf((v + dir * inc) / inc) * inc;
	return isfinite(next) ? fmaxf(next, floor_) : v;
}

static float ratio01(float v, int dir) { return clampf(raise(v, dir, 0.05f, 0.0f), 0.0f, 1.0f); }

static int level(int v, int dir)
{
	if (dir < 0) return v > 0 ? v - 1 : 0;
	return v < 1000000 ? v + 1 : v;
}

bool workshop_adjust(F3DSGarage *g, F3DSWorkshopPage page, int item, int dir)
{
	const int car = g->selected;
	F3DSVehicleDef *d = &g->def[car];
	if (dir == 0 || item < 0 || item >= workshop_item_count(page))
		return false;
	dir = dir > 0 ? 1 : -1;
	const F3DSGarage before = *g;
	switch (page) {
	case F3DS_PAGE_ENGINE:
		switch (item) {
		case E_TORQUE: d->torqueBase = raise(d->torqueBase, dir, 5.0f, 40.0f); break;
		case E_RPM: d->maxRpm = raise(d->maxRpm, dir, 100.0f, d->idleRpm + 1000.0f); break;
		case E_TURBO:
			d->turboInstalled = !d->turboInstalled;
			if (d->turboInstalled && d->turboBoostBar <= 0.0f) d->turboBoostBar = 0.5f;
			break;
		case E_BOOST: d->turboBoostBar = raise(d->turboBoostBar, dir, 0.1f, 0.0f); break;
		case E_INTAKE: d->intakeLevel = level(d->intakeLevel, dir); break;
		case E_EXHAUST: d->exhaustLevel = level(d->exhaustLevel, dir); break;
		case E_CAM: d->camshaftLevel = level(d->camshaftLevel, dir); break;
		case E_INJECTORS: d->injectorLevel = level(d->injectorLevel, dir); break;
		case E_PISTONS: d->pistonLevel = level(d->pistonLevel, dir); break;
		default: break;
		}
		break;
	case F3DS_PAGE_GEARBOX:
		if (item == G_MODE) d->manualTransmission = !d->manualTransmission;
		else if (item == G_COUNT_GEARS) {
			const int n = d->gearCount + dir;
			d->gearCount = n < 1 ? 1 : n > F3DS_MAX_GEARS ? F3DS_MAX_GEARS : n;
		} else if (item == G_FINAL) d->finalDrive = raise(d->finalDrive, dir, 0.05f, 0.5f);
		else d->gearRatios[item - G_RATIO] = raise(d->gearRatios[item - G_RATIO], dir, 0.02f, 0.2f);
		break;
	case F3DS_PAGE_DIFF:
		if (item == D_SPLIT) d->frontTorqueShare = ratio01(d->frontTorqueShare, dir);
		else if (item == D_FRONT) d->diffFrontLock = ratio01(d->diffFrontLock, dir);
		else if (item == D_CENTER) d->diffCenterLock = ratio01(d->diffCenterLock, dir);
		else d->diffRearLock = ratio01(d->diffRearLock, dir);
		break;
	case F3DS_PAGE_SUSPENSION: {
		const int a = item / 4;
		switch (item % 4) {
		case 0: d->rideHeight[a] = raise(d->rideHeight[a], dir, 0.01f, 0.04f); break;
		case 1: d->springRate[a] = raise(d->springRate[a], dir, 2000.0f, 6000.0f); break;
		case 2: d->damperCompression[a] = raise(d->damperCompression[a], dir, 200.0f, 0.0f); break;
		default: d->damperRebound[a] = raise(d->damperRebound[a], dir, 200.0f, 0.0f); break;
		}
		break;
	}
	case F3DS_PAGE_TIRES:
		if (item == T_TYPE)
			d->tireType = (F3DSTireType)(((int)d->tireType + dir + F3DS_TIRE_COUNT) % F3DS_TIRE_COUNT);
		else if (item == T_GRIP) d->tireGrip = raise(d->tireGrip, dir, 0.05f, 0.2f);
		else if (item == T_BRAKE) d->brakeForce = raise(d->brakeForce, dir, 500.0f, 1000.0f);
		else if (item == T_BIAS) d->brakeBiasFront = ratio01(d->brakeBiasFront, dir);
		else d->handbrakeForce = raise(d->handbrakeForce, dir, 500.0f, 0.0f);
		break;
	case F3DS_PAGE_LOOK:
		if (item == L_PAINT) g->paint[car] = (g->paint[car] + dir + F3DS_PAINT_COUNT) % F3DS_PAINT_COUNT;
		else g->rim[car] = (g->rim[car] + dir + F3DS_RIM_COUNT) % F3DS_RIM_COUNT;
		break;
	default:
		break;
	}
	if (!vehicle_def_valid(d)) {
		*g = before;
		return false;
	}
	return true;
}
