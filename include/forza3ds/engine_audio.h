#ifndef F3DS_ENGINE_AUDIO_H
#define F3DS_ENGINE_AUDIO_H

#include "vehicle.h"

bool engine_audio_init(void);
void engine_audio_update(const F3DSVehicleDef *def, const F3DSVehicleState *car);
void engine_audio_set_enabled(bool enabled);
void engine_audio_exit(void);

#endif
