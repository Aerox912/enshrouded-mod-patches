// SPDX-License-Identifier: MIT
#pragma once
namespace vein_controls {
struct RadiusSelection {
    unsigned multiplier = 2;
    bool down = false;
    bool armed = false;
    explicit RadiusSelection(unsigned saved = 2) : multiplier(saved >= 1 && saved <= 3 ? saved : 2) {}
    bool sample(short physical, bool focused) {
        const bool pressed = (static_cast<unsigned short>(physical) & 0x8000u) != 0;
        if (!focused) { down = pressed; armed = false; return false; }
        if (!pressed) armed = true;
        const bool changed = armed && pressed && !down;
        if (changed) multiplier = multiplier % 3 + 1;
        down = pressed;
        return changed;
    }
};
}
