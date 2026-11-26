#pragma once

#include "skyframe/core/math.hpp"
#include "skyframe/sim/environment.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace skyframe {

// Arcade sandbox ordnance. Everything here is a game mechanic: simple ballistics, blast
// radii and static targets (buildings, vehicles, crash-test dummies). No people are involved.

enum class BombKind : std::uint8_t { Light, Medium, Heavy, Cluster, Incendiary, Nuclear };
inline constexpr int kBombKindCount = 6;

struct BombSpec {
    std::string_view name;
    std::string_view blurb;
    double blastRadius;     // metres (game units)
    double blastSeconds;    // how long the explosion visuals last
    double drag;            // simple quadratic-ish drag coefficient
};

[[nodiscard]] const BombSpec& bombSpec(BombKind kind) noexcept;
// Ordnance tier of an aircraft: 0 = no bay, 1 = Light/Medium, 2 = everything.
[[nodiscard]] bool ordnanceAllows(int tier, BombKind kind) noexcept;

struct Bomb {
    Vec3 position{};
    Vec3 velocity{};
    BombKind kind{BombKind::Light};
    double age{0.0};
    bool bomblet{false};
};

struct Blast {
    Vec3 position{};
    BombKind kind{BombKind::Light};
    double radius{0.0};
    double age{0.0};
    double duration{2.0};
};

enum class TargetKind : std::uint8_t { Building, Tower, Truck, AntiAirVehicle, CrashDummy };

struct Target {
    TargetKind kind{TargetKind::Building};
    Vec3 position{};         // on the ground
    double yawDegrees{0.0};
    int variant{0};          // size/colour variation for the renderer
    bool destroyed{false};
    double destroyedAge{0.0};
    Vec3 fling{};            // displacement applied to a destroyed target
};

class OrdnanceField {
public:
    static constexpr std::size_t kMaxBombs = 160;
    static constexpr std::size_t kMaxBlasts = 48;

    // Rebuilds the deterministic target range for this map and clears bombs and blasts.
    void generate(const MapConfig& map);
    void update(double dt, const MapConfig& map);
    // Releases a bomb. Returns false when the bomb limit is reached.
    bool drop(BombKind kind, const Vec3& position, const Vec3& velocity);

    [[nodiscard]] const std::vector<Bomb>& bombs() const noexcept { return bombs_; }
    [[nodiscard]] const std::vector<Blast>& blasts() const noexcept { return blasts_; }
    [[nodiscard]] const std::vector<Target>& targets() const noexcept { return targets_; }
    [[nodiscard]] int destroyedTargets() const noexcept;
    [[nodiscard]] int totalTargets() const noexcept { return static_cast<int>(targets_.size()); }
    [[nodiscard]] int bombsDropped() const noexcept { return dropped_; }
    [[nodiscard]] Vec3 rangeCenter() const noexcept { return rangeCenter_; }

private:
    void detonate(const Bomb& bomb, const MapConfig& map);

    std::vector<Bomb> bombs_;
    std::vector<Blast> blasts_;
    std::vector<Target> targets_;
    Vec3 rangeCenter_{};
    int dropped_{0};
};

} // namespace skyframe
