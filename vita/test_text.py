#!/usr/bin/env python3
"""Check Unicode edge cases in the actual native keyboard conversion helper."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "test.cpp"
    binary = Path(directory) / "test"
    source.write_text(r'''
#include "util/vita_text.h"
#include <cassert>
int main() {
    auto check = [](std::wstring input) {
        auto encoded = vita_text::encode(input, 2048);
        assert(vita_text::decode(encoded.data(), encoded.size()) == input);
    };
    check(L""); check(L"30000"); check(L"é日本語😀");
    check(std::wstring(2048, L'x'));
    assert(vita_text::encode(L"😀", 1) == std::vector<uint16_t>{0});
    assert(vita_text::encode(L"😀", 2).size() == 3);
    auto clipped = vita_text::encode(L"x😀", 2);
    assert(vita_text::decode(clipped.data(), clipped.size()) == L"x");
    uint16_t lone_high[] = {0xd800, 0};
    uint16_t lone_low[] = {0xdc00, 0};
    uint16_t bad_pair[] = {0xd800, 'x', 0};
    assert(vita_text::decode(lone_high, 2) == L"\ufffd");
    assert(vita_text::decode(lone_low, 2) == L"\ufffd");
    assert(vita_text::decode(bad_pair, 3) == L"\ufffdx");
    assert(vita_text::decode(lone_high, 1) == L"\ufffd");
    assert(vita_text::encode(std::wstring(1, 0x110000), 2)[0] == 0xfffd);
}
''')
    subprocess.run(["c++", "-std=c++17", "-fsanitize=address,undefined",
        "-I" + str(root / "src"), source, "-o", binary], check=True)
    subprocess.run([binary], check=True)
print("PASS native keyboard Unicode, malformed surrogates and length boundaries")
