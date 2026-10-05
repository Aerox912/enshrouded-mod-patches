// SPDX-License-Identifier: MIT
#pragma once
namespace vein_controls {
enum class Mode { Hold, Toggle, Always };
struct Activation {
    bool latched = false;
    bool was_down = false;
    bool armed = false;
    // The caller serializes reads from mining, networking and preview threads.
    short sample(Mode mode, short physical, bool focused) {
        if (mode == Mode::Hold) return physical;
        const bool down = (static_cast<unsigned short>(physical) & 0x8000u) != 0;
        if (!focused) { was_down = down; armed = false; return 0; }
        if (mode == Mode::Always) return static_cast<short>(0x8000u);
        if (!down) armed = true;
        if (armed && down && !was_down) latched = !latched;
        was_down = down;
        return latched ? static_cast<short>(0x8000u) : 0;
    }
};
}
