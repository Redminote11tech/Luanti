// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <string>

namespace porting {
void initVita();
bool startVitaTextInput(const std::wstring &text, bool password, bool multiline = false);
// 0: pending, 1: accepted, -1: cancelled/failed.
int pollVitaTextInput(std::wstring &text);
void cancelVitaTextInput();
int vitaSelfTest(unsigned short echo_port);
}
