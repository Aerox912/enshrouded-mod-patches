// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include "activation.h"

namespace vein_controls {
constexpr bool hide_preview(Mode mode) { return mode != Mode::Hold; }

// Shared under the input lock; the overlay thread reads it on its own timer.
struct Notice {
    static constexpr std::uint64_t duration_ms = 1500;
    bool pending = false;
    bool on = false;
    std::uint64_t changed_at = 0;

    void changed(Mode mode, bool before, bool after, std::uint64_t now) {
        if (mode != Mode::Toggle || before == after) return;
        pending = true;
        on = after;
        changed_at = now;
    }
    bool visible(std::uint64_t now, bool focused) {
        if (!focused || now - changed_at >= duration_ms) pending = false;
        return pending;
    }
};
}
