#pragma once

#include "skyframe/sim/simulation.hpp"

#include <raylib.h>

namespace skyframe {

class WorldRenderer {
public:
    void draw(const Simulation& simulation, const ReplayFrame* replayFrame, bool cockpitView);
    // Hold-to-look-back chase camera: sits ahead of the aircraft and looks back along its track
    // so bomb blasts behind the aircraft stay in view. Ignored in cockpit view.
    void setLookBack(bool value) noexcept { lookBack_ = value; }

private:
    Camera3D camera_{};
    GeneratedMap generatedMap_{};
    bool hasGeneratedMap_{false};
    bool lookBack_{false};
};

} // namespace skyframe
