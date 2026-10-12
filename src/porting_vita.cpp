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
#include "util/vita_text.h"
#include <psp2/ime_dialog.h>
#include <array>
#include "log.h"

extern "C" {
// Leave space outside the libc heap for GXM, threads and network pools.
unsigned int _newlib_heap_size_user = 192 * 1024 * 1024;
}

namespace porting {
namespace {
bool ime_active = false;
std::vector<uint16_t> ime_initial;
std::array<uint16_t, SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1> ime_output{};
const uint16_t ime_title[] = {'L', 'u', 'a', 'n', 't', 'i', 0};
}

bool startVitaTextInput(const std::wstring &text, bool password, bool multiline)
{
	if (ime_active)
		return false;
	ime_initial = vita_text::encode(text, SCE_IME_DIALOG_MAX_TEXT_LENGTH);
	ime_output.fill(0);
	SceImeDialogParam param;
	sceImeDialogParamInit(&param);
	param.type = SCE_IME_TYPE_DEFAULT;
	param.option = SCE_IME_OPTION_NO_AUTO_CAPITALIZATION;
	if (password)
		param.option |= SCE_IME_OPTION_NO_ASSISTANCE;
	else if (multiline)
		param.option |= SCE_IME_OPTION_MULTILINE;
	param.dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
	param.textBoxMode = password ? SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD :
		SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
	param.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
	param.title = ime_title;
	param.initialText = ime_initial.data();
	param.inputTextBuffer = ime_output.data();
	int result = sceImeDialogInit(&param);
	if (result < 0) {
		errorstream << "Vita keyboard initialization failed: " << result << std::endl;
		return false;
	}
	ime_active = true;
	return true;
}

int pollVitaTextInput(std::wstring &text)
{
	if (!ime_active)
		return -1;
	if (sceImeDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return 0;
	SceImeDialogResult result{};
	bool accepted = sceImeDialogGetResult(&result) >= 0 && result.result == 0 &&
		result.button == SCE_IME_DIALOG_BUTTON_ENTER;
	if (accepted)
		text = vita_text::decode(ime_output.data(), ime_output.size());
	sceImeDialogTerm();
	ime_active = false;
	return accepted ? 1 : -1;
}

void cancelVitaTextInput()
{
	if (!ime_active)
		return;
	sceImeDialogAbort();
	sceImeDialogTerm();
	ime_active = false;
}

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
