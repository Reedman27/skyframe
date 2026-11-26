#pragma once

#include "skyframe/sim/aircraft.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace skyframe {

// Core-side description of an aircraft's look. No raylib types: the renderer builds geometry from this.
// Every aircraft in the fleet is free and unlocked. Ordnance-capable aircraft drop arcade sandbox
// bombs on static targets only (see ordnance.hpp); `ordnanceTier` says which bombs they carry.

enum class FleetCategory : std::uint8_t { Small, Medium, Large, Military };
enum class WingMount : std::uint8_t { Low, Mid, High };
enum class EngineKind : std::uint8_t { Jet, Turboprop, Piston };
enum class EngineLayout : std::uint8_t { UnderWing, RearFuselage, Nose, Internal };
enum class TailKind : std::uint8_t { Conventional, TTail, Twin };

struct AircraftDesign {
    // Dimensions in metres; +X is the nose, +Y up, +Z right wing.
    float length{30.0f};
    float bodyWidth{3.5f};
    float bodyHeight{3.5f};
    float noseLength{5.0f};
    float tailLength{9.0f};
    float wingSpan{30.0f};
    float rootChord{6.0f};
    float tipChord{2.0f};
    float sweepDegrees{25.0f};
    float dihedralDegrees{3.0f};
    float wingX{0.0f};              // wing root mid-chord, metres from fuselage centre (+ is forward)
    WingMount wingMount{WingMount::Low};
    bool winglets{false};
    int engineCount{2};
    EngineKind engineKind{EngineKind::Jet};
    EngineLayout engineLayout{EngineLayout::UnderWing};
    float engineRadius{1.0f};
    float engineLength{4.0f};
    TailKind tail{TailKind::Conventional};
    float finHeight{6.0f};
    float finChord{5.0f};
    float stabSpan{10.0f};
    bool humpDeck{false};           // raised forward deck (jumbo/freighter silhouette)
    float gearHeight{1.8f};         // fuselage underside to wheel bottom
    std::array<std::uint8_t, 3> body{205, 212, 220};
    std::array<std::uint8_t, 3> accent{0, 112, 186};
};

struct FleetEntry {
    FleetCategory category{FleetCategory::Medium};
    AircraftConfig config{};
    std::string_view blurb{};
    AircraftDesign design{};
    int ordnanceTier{0};   // 0 = no bay, 1 = Light/Medium bombs, 2 = every bomb type
};

[[nodiscard]] int fleetCount() noexcept;
[[nodiscard]] const FleetEntry& fleetEntry(int index) noexcept;
[[nodiscard]] std::string_view fleetCategoryName(FleetCategory category) noexcept;
// Index of the first aircraft in `category` at or after `from`, or -1.
[[nodiscard]] int fleetCountInCategory(FleetCategory category) noexcept;
[[nodiscard]] int fleetIndexInCategory(FleetCategory category, int nth) noexcept;

} // namespace skyframe
