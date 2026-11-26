#include "skyframe/sim/fleet.hpp"

#include <algorithm>
#include <vector>

namespace skyframe {
namespace {
using C = FleetCategory;
using M = WingMount;
using K = EngineKind;
using L = EngineLayout;
using T = TailKind;

// Order matters for the first three: existing indices (0 regional, 1 jumbo, 2 cargo) are kept stable.
const std::vector<FleetEntry>& table() {
    static const std::vector<FleetEntry> fleet = [] {
        std::vector<FleetEntry> f;
        auto add = [&f](C cat, const char* name, int pax, int crew, double handling, double accel,
                        const char* blurb, const AircraftDesign& d) {
            f.push_back({cat, AircraftConfig{name, pax, crew, handling, accel}, blurb, d});
        };
        // length, bw, bh, nose, tail, span, root, tip, sweep, dih, wingX, mount, winglets,
        // engines, kind, layout, engR, engL, tail, finH, finC, stab, hump, gear, body, accent
        add(C::Medium, "Northstar Regional", 84, 4, 1.12, 1.12, "Twin-jet regional airliner",
            {34.0f, 3.2f, 3.4f, 5.5f, 9.5f, 28.0f, 5.2f, 1.7f, 24.0f, 4.0f, 0.5f, M::Low, false,
             2, K::Jet, L::RearFuselage, 0.85f, 3.4f, T::TTail, 5.4f, 5.0f, 9.5f, false, 1.7f,
             {212, 218, 226}, {0, 122, 190}});
        add(C::Large, "Skyline Jumbo #3", 300, 12, 0.88, 0.88, "Four-engine double-deck jumbo",
            {70.0f, 6.4f, 7.2f, 11.0f, 17.0f, 60.0f, 11.5f, 3.2f, 35.0f, 5.0f, -2.0f, M::Low, false,
             4, K::Jet, L::UnderWing, 1.45f, 5.2f, T::Conventional, 14.0f, 10.0f, 20.0f, true, 3.0f,
             {208, 214, 222}, {0, 96, 170}});
        add(C::Large, "Meridian Cargo", 24, 4, 0.96, 1.02, "Four-engine freighter with raised cockpit",
            {64.0f, 6.0f, 6.6f, 10.5f, 15.0f, 58.0f, 11.0f, 3.2f, 33.0f, 5.0f, -1.0f, M::Low, false,
             4, K::Jet, L::UnderWing, 1.4f, 5.0f, T::Conventional, 13.0f, 9.5f, 19.0f, true, 3.0f,
             {226, 228, 230}, {200, 120, 20}});

        add(C::Small, "Skyline Cub", 3, 1, 1.40, 0.80, "Single-piston high-wing trainer",
            {8.0f, 1.2f, 1.4f, 2.2f, 3.6f, 11.0f, 1.7f, 1.3f, 0.0f, 1.0f, 0.4f, M::High, false,
             1, K::Piston, L::Nose, 0.45f, 1.1f, T::Conventional, 1.6f, 1.5f, 3.5f, false, 0.9f,
             {236, 214, 80}, {200, 40, 40}});
        add(C::Small, "Skyline Sparrow", 3, 1, 1.42, 0.85, "Single-piston low-wing tourer",
            {8.5f, 1.2f, 1.4f, 2.4f, 3.8f, 10.5f, 1.8f, 1.1f, 2.0f, 5.0f, 0.2f, M::Low, false,
             1, K::Piston, L::Nose, 0.45f, 1.1f, T::Conventional, 1.5f, 1.4f, 3.3f, false, 1.0f,
             {232, 236, 240}, {30, 130, 90}});
        add(C::Small, "Skyline Courier", 8, 1, 1.30, 0.95, "Single-turboprop utility plane",
            {11.0f, 1.7f, 1.9f, 3.0f, 4.4f, 14.0f, 2.2f, 1.4f, 2.0f, 4.0f, 0.3f, M::Low, false,
             1, K::Turboprop, L::Nose, 0.55f, 1.8f, T::Conventional, 2.2f, 2.0f, 4.6f, false, 1.1f,
             {240, 240, 236}, {0, 110, 170}});
        add(C::Small, "Skyline Islander", 9, 1, 1.28, 0.90, "Twin-piston high-wing utility plane",
            {11.0f, 1.7f, 1.9f, 2.8f, 4.6f, 15.0f, 2.0f, 1.5f, 0.0f, 1.0f, 0.4f, M::High, false,
             2, K::Piston, L::UnderWing, 0.5f, 1.8f, T::Conventional, 2.4f, 2.2f, 5.0f, false, 1.1f,
             {236, 232, 220}, {190, 70, 40}});
        add(C::Small, "Skyline Vantage", 12, 2, 1.30, 1.25, "Twin-jet executive aircraft",
            {19.0f, 2.1f, 2.2f, 4.2f, 6.8f, 18.8f, 3.2f, 1.2f, 28.0f, 3.0f, 0.5f, M::Low, true,
             2, K::Jet, L::RearFuselage, 0.55f, 2.2f, T::TTail, 3.4f, 3.0f, 5.4f, false, 1.2f,
             {244, 244, 246}, {40, 60, 120}});

        add(C::Medium, "Skyline Breeze", 70, 3, 1.10, 1.00, "Twin-turboprop regional airliner",
            {27.0f, 2.9f, 3.0f, 4.6f, 8.6f, 27.0f, 3.9f, 2.0f, 2.0f, 2.0f, 0.4f, M::High, false,
             2, K::Turboprop, L::UnderWing, 0.8f, 3.4f, T::TTail, 4.4f, 3.8f, 8.2f, false, 1.6f,
             {236, 238, 240}, {0, 140, 130}});
        add(C::Medium, "Skyline Express", 80, 4, 1.10, 1.15, "Twin-engine regional jet",
            {33.5f, 3.2f, 3.4f, 5.4f, 9.2f, 26.0f, 4.8f, 1.6f, 25.0f, 4.0f, 0.5f, M::Low, false,
             2, K::Jet, L::RearFuselage, 0.8f, 3.2f, T::TTail, 5.2f, 4.8f, 9.0f, false, 1.7f,
             {222, 226, 232}, {190, 40, 60}});
        add(C::Medium, "Skyline Quartet", 100, 4, 1.00, 1.00, "Four-engine high-wing jet",
            {31.0f, 3.3f, 3.5f, 4.8f, 8.8f, 26.5f, 5.0f, 2.2f, 15.0f, 0.0f, 0.5f, M::High, false,
             4, K::Jet, L::UnderWing, 0.65f, 2.6f, T::TTail, 5.4f, 4.6f, 10.0f, false, 1.8f,
             {232, 232, 228}, {220, 150, 30}});
        add(C::Medium, "Skyline Citylink", 112, 5, 1.00, 1.05, "Twin-engine single-aisle airliner",
            {40.0f, 3.9f, 4.1f, 6.4f, 11.0f, 34.0f, 6.4f, 1.9f, 25.0f, 5.0f, 0.5f, M::Low, true,
             2, K::Jet, L::UnderWing, 1.1f, 4.0f, T::Conventional, 6.8f, 5.8f, 12.5f, false, 1.9f,
             {226, 230, 236}, {0, 100, 180}});
        add(C::Medium, "Skyline Trijet", 150, 5, 0.98, 1.05, "Three-engine airliner with tail-mounted centre engine",
            {49.0f, 3.9f, 4.1f, 7.0f, 14.0f, 33.0f, 7.0f, 2.2f, 30.0f, 3.0f, 1.0f, M::Low, false,
             3, K::Jet, L::RearFuselage, 1.1f, 4.4f, T::TTail, 8.2f, 6.8f, 12.0f, false, 1.9f,
             {230, 228, 220}, {130, 50, 140}});

        add(C::Large, "Skyline Wideliner", 380, 12, 0.90, 0.95, "Twin-engine wide-body with winglets",
            {64.0f, 6.0f, 6.4f, 10.0f, 16.0f, 60.0f, 11.0f, 3.0f, 32.0f, 6.0f, -1.0f, M::Low, true,
             2, K::Jet, L::UnderWing, 2.1f, 6.8f, T::Conventional, 12.5f, 9.0f, 19.0f, false, 2.9f,
             {236, 238, 242}, {20, 140, 100}});
        add(C::Large, "Skyline Titan", 520, 16, 0.84, 0.85, "Four-engine full double-deck super-jumbo",
            {73.0f, 7.4f, 8.6f, 12.0f, 18.0f, 79.0f, 13.5f, 3.8f, 34.0f, 5.0f, -2.0f, M::Low, false,
             4, K::Jet, L::UnderWing, 1.6f, 5.8f, T::Conventional, 17.0f, 11.5f, 24.0f, true, 3.4f,
             {240, 240, 242}, {120, 20, 60}});

        add(C::Military, "Meridian Hauler", 90, 5, 0.92, 0.95, "Unarmed four-turboprop transport with rear ramp",
            {29.0f, 3.9f, 4.0f, 5.0f, 9.0f, 40.0f, 5.5f, 2.6f, 4.0f, 1.0f, 0.5f, M::High, false,
             4, K::Turboprop, L::UnderWing, 0.9f, 3.6f, T::TTail, 6.2f, 5.4f, 14.0f, false, 2.0f,
             {96, 108, 84}, {60, 70, 50}});
        add(C::Military, "Falcon Trainer", 1, 1, 1.45, 1.30, "Unarmed two-seat jet trainer",
            {11.5f, 1.5f, 1.4f, 3.2f, 3.8f, 10.0f, 2.4f, 1.0f, 18.0f, 0.0f, -0.2f, M::Low, false,
             2, K::Jet, L::Internal, 0.4f, 2.4f, T::Conventional, 2.2f, 2.0f, 3.8f, false, 1.1f,
             {200, 204, 210}, {220, 90, 20}});
        add(C::Military, "Skyline Vanguard", 1, 1, 1.50, 1.45, "Unarmed twin-tail show jet",
            {19.0f, 2.6f, 1.6f, 5.5f, 5.0f, 13.5f, 6.2f, 1.6f, 42.0f, 0.0f, -2.2f, M::Mid, false,
             2, K::Jet, L::Internal, 0.6f, 4.2f, T::Twin, 3.4f, 3.4f, 6.2f, false, 1.2f,
             {120, 128, 140}, {170, 180, 200}});
        add(C::Military, "F-22 Raptor", 1, 1, 1.55, 1.55, "Stealth air-superiority fighter with internal bay (light and medium bombs)",
            {18.9f, 2.9f, 1.7f, 5.8f, 5.2f, 13.6f, 6.0f, 1.5f, 42.0f, 0.0f, -2.0f, M::Mid, false,
             2, K::Jet, L::Internal, 0.62f, 4.4f, T::Twin, 3.3f, 3.3f, 6.4f, false, 1.2f,
             {104, 111, 120}, {72, 80, 92}});
        f.back().ordnanceTier = 1;
        add(C::Military, "B-52 Stratofortress", 1, 5, 0.80, 0.80, "Eight-engine heavy bomber with a full bomb bay (every bomb type)",
            {48.0f, 3.4f, 3.6f, 7.0f, 10.0f, 56.0f, 7.5f, 2.4f, 35.0f, -2.0f, 0.5f, M::High, false,
             8, K::Jet, L::UnderWing, 0.85f, 4.2f, T::Conventional, 8.5f, 7.5f, 18.0f, false, 2.6f,
             {112, 118, 106}, {82, 88, 76}});
        f.back().ordnanceTier = 2;
        return f;
    }();
    return fleet;
}
} // namespace

int fleetCount() noexcept { return static_cast<int>(table().size()); }

const FleetEntry& fleetEntry(int index) noexcept {
    const int safe = std::clamp(index, 0, fleetCount() - 1);
    return table()[static_cast<std::size_t>(safe)];
}

std::string_view fleetCategoryName(FleetCategory category) noexcept {
    switch (category) {
        case FleetCategory::Small: return "Small";
        case FleetCategory::Medium: return "Medium";
        case FleetCategory::Large: return "Large";
        case FleetCategory::Military: return "Military";
    }
    return "Unknown";
}

int fleetCountInCategory(FleetCategory category) noexcept {
    return static_cast<int>(std::count_if(table().begin(), table().end(),
        [category](const FleetEntry& e) { return e.category == category; }));
}

int fleetIndexInCategory(FleetCategory category, int nth) noexcept {
    int seen = 0;
    for (int i = 0; i < fleetCount(); ++i) {
        if (table()[static_cast<std::size_t>(i)].category != category) continue;
        if (seen++ == nth) return i;
    }
    return -1;
}

} // namespace skyframe
