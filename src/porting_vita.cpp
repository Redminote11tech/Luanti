// SPDX-License-Identifier: LGPL-2.1-or-later
#include "porting_vita.h"
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/power.h>
#include <psp2/apputil.h>
#include <psp2/common_dialog.h>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include "filesys.h"

extern "C" {
// Leave space outside the libc heap for GXM, threads and network pools.
unsigned int _newlib_heap_size_user = 192 * 1024 * 1024;
}

namespace porting {
void initVita()
{
	if (!fs::CreateAllDirs("ux0:/data/luanti"))
		throw std::runtime_error("Cannot create ux0:/data/luanti");
	std::freopen("ux0:/data/luanti/startup.log", "w", stderr);
	setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);
	SceAppUtilInitParam app_init{};
	SceAppUtilBootParam app_boot{};
	if (sceAppUtilInit(&app_init, &app_boot) < 0)
		throw std::runtime_error("Cannot initialize Vita app utilities");
	SceCommonDialogConfigParam dialog_config;
	sceCommonDialogConfigParamInit(&dialog_config);
	if (sceCommonDialogSetConfigParam(&dialog_config) < 0)
		throw std::runtime_error("Cannot configure Vita text input dialogs");
	if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0)
		throw std::runtime_error("Cannot load Vita networking");
	constexpr int pool_size = 4 * 1024 * 1024;
	void *pool = std::malloc(pool_size);
	if (!pool)
		throw std::bad_alloc();
	SceNetInitParam param{};
	param.memory = pool;
	param.size = pool_size;
	if (sceNetInit(&param) < 0 || sceNetCtlInit() < 0)
		throw std::runtime_error("Cannot initialize Vita networking");
	// Standard supported clocks. No overclock plugin is required.
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
}
}
