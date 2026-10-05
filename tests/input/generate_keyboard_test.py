"""Compile the production mapping/state expressions without Qt/session side effects.

This focused harness checks wire codes/extended flags and release-state encoding;
it does not simulate OS keyboard capture, SDL event delivery or a remote host.
"""
from pathlib import Path
import sys

source = (Path(__file__).parents[2] / 'app/streaming/input/keyboard.cpp').read_text()
macros = source[source.index('#define VK_0'):source.index('void SdlInputHandler::performSpecialKeyCombo')]
mapping = source[source.index('    if (event->keysym.scancode >= SDL_SCANCODE_1'):
                 source.index('    // Track the key state')]
harness = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <SDL_scancode.h>
#include <Limelight.h>
#define Q_FALLTHROUGH() [[fallthrough]]
#define SDL_LogInfo(...) ((void)0)
MACROS
struct Event { struct { SDL_Scancode scancode; } keysym; };
struct Key { short code = 0; char modifiers = 0, flags = 0; };
bool isSystemKeyCaptureActive() { return true; }
void map(Event* event, Key& result) {
    short keyCode = 0;
    char modifiers = 0, flags = 0;
    bool shouldNotConvertToScanCodeOnServer = false;
MAPPING
    result = {keyCode, modifiers, flags};
}
void check(SDL_Scancode scan, int expectedCode, bool extended, int expectedFlags=0) {
    Event event{{scan}};
    Key key;
    map(&event, key);
    if (uint16_t(key.code) != (expectedCode | 0x8000) ||
        bool(key.modifiers & MODIFIER_EXTENDED) != extended || key.flags != expectedFlags) {
        std::fprintf(stderr, "Mapping failure for scan %d\n", int(scan)); std::abort();
    }
    // release-all must retain extended/non-normalized flags and omit transient Shift/Ctrl.
    auto saved = MAKE_KEYPRESS_STATE(key.code, key.modifiers | MODIFIER_SHIFT | MODIFIER_CTRL, key.flags);
    if (GET_KEYPRESS_CODE(saved) != key.code ||
        GET_KEYPRESS_EXTENDED_MODIFIER(saved) != key.modifiers || GET_KEYPRESS_FLAGS(saved) != key.flags)
        std::abort();
}
int main() {
    check(SDL_SCANCODE_RETURN, 0x0D, false);
    check(SDL_SCANCODE_KP_ENTER, 0x0D, true);
    check(SDL_SCANCODE_LEFT, 0x25, true);
    check(SDL_SCANCODE_KP_4, 0x64, false);
    check(SDL_SCANCODE_A, 0x41, false);
    check(SDL_SCANCODE_RCTRL, 0xA3, true);
    check(SDL_SCANCODE_LCTRL, 0xA2, false);
    check(SDL_SCANCODE_INTERNATIONAL3, 0xDC, false, SS_KBE_FLAG_NON_NORMALIZED);
    Event enter{{SDL_SCANCODE_RETURN}}, numpad{{SDL_SCANCODE_KP_ENTER}};
    Key a, b; map(&enter, a); map(&numpad, b);
    if (MAKE_KEYPRESS_STATE(a.code,a.modifiers,a.flags) == MAKE_KEYPRESS_STATE(b.code,b.modifiers,b.flags))
        std::abort();
    std::puts("Keyboard mapping and release-state regressions passed");
}
'''
Path(sys.argv[1]).write_text(harness.replace('MACROS',macros).replace('MAPPING',mapping))
