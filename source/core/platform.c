#include "platform.h"
#include <3ds.h>

bool platform_init(F3DSPlatformInfo *info)
{
	info->isNew3DS = false;
	info->clockBoosted = false;
	info->appCpuTimeSet = false;
	info->multiCoreReady = false;
	info->appMemType = 0;
	info->appMemSizeBytes = 0;

	if (R_FAILED(APT_CheckNew3DS(&info->isNew3DS)))
		info->isNew3DS = false;
	if (!info->isNew3DS)
		return false; /* New 3DS only */

	/* 804 MHz ARM11 (LGR2) boost. */
	osSetSpeedupEnable(true);
	info->clockBoosted = true;

	/* Application CPU time limit: 80 percent, required to schedule work
	 * on the extra cores (core 2 / core 3) of the New 3DS. */
	if (R_SUCCEEDED(APT_SetAppCpuTimeLimit(80)))
		info->appCpuTimeSet = true;

	info->multiCoreReady = info->clockBoosted && info->appCpuTimeSet;

	/* Memory layout report (New 3DS memtypes: 6, 7 or 8). */
	info->appMemType = osGetApplicationMemType();
	info->appMemSizeBytes = osGetMemRegionSize(MEMREGION_APPLICATION);

	return true;
}
