/* Forza3DS - NDSP output for the synthesiser (3DS only). */
#ifndef F3DS_ENGINE_AUDIO_H
#define F3DS_ENGINE_AUDIO_H

#include "synth.h"

/* False when the DSP cannot be started (usually a missing dspfirm.cdc). */
bool engine_audio_init(void);
void engine_audio_set(const F3DSSynthParams *p);
void engine_audio_pause(bool paused);
void engine_audio_exit(void);

#endif
