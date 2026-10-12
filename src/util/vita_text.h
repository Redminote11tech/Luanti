// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vita_text {
// Vita wchar_t is UTF-32; the native keyboard uses UTF-16 code units.
inline std::vector<uint16_t> encode(const std::wstring &text, size_t limit)
{
	std::vector<uint16_t> out;
	for (uint32_t c : text) {
		if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
			c = 0xfffd;
		if (!c || out.size() + (c > 0xffff ? 2 : 1) > limit)
			break;
		if (c > 0xffff) {
			c -= 0x10000;
			out.push_back(0xd800 + (c >> 10));
			out.push_back(0xdc00 + (c & 0x3ff));
		} else {
			out.push_back(c);
		}
	}
	out.push_back(0);
	return out;
}

inline std::wstring decode(const uint16_t *text, size_t limit)
{
	std::wstring out;
	for (size_t i = 0; i < limit && text[i]; ++i) {
		uint32_t c = text[i];
		if (c >= 0xd800 && c <= 0xdbff) {
			if (i + 1 < limit && text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff) {
				c = 0x10000 + ((c - 0xd800) << 10) + (text[++i] - 0xdc00);
			} else {
				c = 0xfffd;
			}
		} else if (c >= 0xdc00 && c <= 0xdfff) {
			c = 0xfffd;
		}
		out.push_back(c);
	}
	return out;
}
}
