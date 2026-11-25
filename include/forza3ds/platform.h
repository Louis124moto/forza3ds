/* Forza3DS - New 3DS platform bring-up.
 * The game targets New Nintendo 3DS exclusively:
 *  - 804 MHz CPU clock via osSetSpeedupEnable (LGR2 mode).
 *  - 80 percent application CPU time so extra cores (core 2/3) can be
 *    scheduled by worker threads when real work exists (world generation,
 *    streaming, AI). No idle worker threads are created before then.
 *  - New 3DS memory layout detection through osGetApplicationMemType and
 *    osGetMemRegionSize (memtypes 6-8 are New 3DS layouts).
 */
#ifndef F3DS_PLATFORM_H
#define F3DS_PLATFORM_H

#include "common.h"

typedef struct {
	bool isNew3DS;
	bool clockBoosted;      /* 804 MHz speedup enabled */
	bool appCpuTimeSet;     /* APT CPU time limit accepted */
	bool multiCoreReady;    /* clock + CPU time both set */
	u32 appMemType;         /* 0-5 Old 3DS, 6-8 New 3DS */
	u32 appMemSizeBytes;    /* APPLICATION region size */
} F3DSPlatformInfo;

/* Detect the console and enable the New 3DS hardware features.
 * Returns false when running on an unsupported (Old 3DS) console;
 * in that case the caller must refuse to start the game. */
bool platform_init(F3DSPlatformInfo *info);

#endif
