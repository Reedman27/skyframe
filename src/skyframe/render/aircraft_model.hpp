#pragma once

#include "skyframe/sim/aircraft.hpp"
#include "skyframe/sim/fleet.hpp"

namespace skyframe {

// World-units-per-metre used when drawing aircraft (stylised: smaller than life so the camera stays close).
constexpr float kAircraftRenderScale = 0.55f;

// Draws one aircraft in the current 3D mode from its design and live state. Immediate-mode, flat-shaded,
// no textures or per-frame allocations; roughly 1-3k triangles per aircraft.
void drawAircraftModel(const AircraftDesign& design, const AircraftSnapshot& state);

} // namespace skyframe
