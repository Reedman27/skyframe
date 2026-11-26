#include "skyframe/sim/ordnance.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace skyframe {
namespace {
constexpr double kGravity = 9.81;
constexpr double kClusterSplitHeight = 150.0;
constexpr int kBombletCount = 10;
constexpr double kBombletRadius = 14.0;

const std::array<BombSpec, kBombKindCount>& specs() {
    static const std::array<BombSpec, kBombKindCount> table{{
        {"Light bomb", "Small blast, quick reset", 16.0, 2.0, 0.0006},
        {"Medium bomb", "Mid-size blast", 26.0, 2.6, 0.0007},
        {"Heavy bomb", "Large crater and fireball", 42.0, 3.2, 0.0008},
        {"Cluster bomb", "Splits into 10 bomblets", 14.0, 2.2, 0.0009},
        {"Incendiary bomb", "Wide, long-burning fireball", 38.0, 7.0, 0.0008},
        {"Nuclear bomb", "Arcade mega-blast with mushroom cloud", 260.0, 20.0, 0.0006},
    }};
    return table;
}

// Tiny deterministic generator so the same map seed always yields the same range.
struct Lcg {
    std::uint32_t state;
    explicit Lcg(std::uint32_t seed) : state(seed == 0U ? 0x9E3779B9U : seed) {}
    double next() noexcept {
        state = state * 1664525U + 1013904223U;
        return static_cast<double>((state >> 8) & 0xFFFFU) / 65535.0;
    }
    double range(double lo, double hi) noexcept { return lo + (hi - lo) * next(); }
};
} // namespace

const BombSpec& bombSpec(BombKind kind) noexcept {
    const auto index = std::min<std::size_t>(static_cast<std::size_t>(kind), specs().size() - 1U);
    return specs()[index];
}

bool ordnanceAllows(int tier, BombKind kind) noexcept {
    if (tier >= 2) return true;
    if (tier == 1) return kind == BombKind::Light || kind == BombKind::Medium;
    return false;
}

void OrdnanceField::generate(const MapConfig& map) {
    bombs_.clear();
    blasts_.clear();
    targets_.clear();
    dropped_ = 0;

    const double extent = generateMap(map).worldExtent;
    const double cx = std::min(1000.0, extent * 0.6);
    rangeCenter_ = {cx, terrainHeightAt(map, cx, 0.0), 0.0};
    Lcg rng(map.seed ^ 0xB0B5U);

    auto place = [&](TargetKind kind, double dx, double dz, int variant) {
        Target t;
        t.kind = kind;
        t.position = {cx + dx, terrainHeightAt(map, cx + dx, dz), dz};
        t.yawDegrees = rng.range(-12.0, 12.0);
        t.variant = variant;
        targets_.push_back(t);
    };

    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 3; ++col)
            place(TargetKind::Building, -50.0 + col * 50.0, -25.0 + row * 50.0, row * 3 + col);
    place(TargetKind::Tower, -85.0, 55.0, 0);
    for (int i = 0; i < 4; ++i) place(TargetKind::Truck, -60.0 + i * 38.0, 90.0, i);
    place(TargetKind::AntiAirVehicle, 85.0, 75.0, 0);
    place(TargetKind::AntiAirVehicle, 85.0, -75.0, 1);
    for (int i = 0; i < 14; ++i)
        place(TargetKind::CrashDummy, rng.range(-70.0, 70.0), rng.range(-55.0, 55.0), i);
}

bool OrdnanceField::drop(BombKind kind, const Vec3& position, const Vec3& velocity) {
    if (bombs_.size() >= kMaxBombs) return false;
    Bomb bomb;
    bomb.position = position;
    bomb.velocity = velocity;
    bomb.kind = kind;
    bombs_.push_back(bomb);
    ++dropped_;
    return true;
}

void OrdnanceField::detonate(const Bomb& bomb, const MapConfig& map) {
    const BombSpec& spec = bombSpec(bomb.kind);
    const double radius = bomb.bomblet ? kBombletRadius : spec.blastRadius;
    if (blasts_.size() >= kMaxBlasts) blasts_.erase(blasts_.begin());
    Blast blast;
    blast.position = {bomb.position.x, terrainHeightAt(map, bomb.position.x, bomb.position.z), bomb.position.z};
    blast.kind = bomb.kind;
    blast.radius = radius;
    blast.duration = bomb.bomblet ? 1.6 : spec.blastSeconds;
    blasts_.push_back(blast);

    for (Target& t : targets_) {
        if (t.destroyed) continue;
        const Vec3 offset = t.position - blast.position;
        const double dist = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
        if (dist > radius) continue;
        t.destroyed = true;
        const double horizontal = std::sqrt(offset.x * offset.x + offset.z * offset.z);
        const double push = (radius - dist) * 0.35;
        if (horizontal > 1.0e-6) t.fling = {offset.x / horizontal * push, 0.0, offset.z / horizontal * push};
    }
}

void OrdnanceField::update(double dt, const MapConfig& map) {
    std::vector<Bomb> spawned;
    for (std::size_t i = 0; i < bombs_.size();) {
        Bomb& b = bombs_[i];
        const BombSpec& spec = bombSpec(b.kind);
        const double speed = std::sqrt(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y
            + b.velocity.z * b.velocity.z);
        b.velocity = b.velocity - b.velocity * (spec.drag * speed * dt);
        b.velocity.y -= kGravity * dt;
        b.position = b.position + b.velocity * dt;
        b.age += dt;

        const double ground = terrainHeightAt(map, b.position.x, b.position.z);
        const bool splits = b.kind == BombKind::Cluster && !b.bomblet && b.position.y - ground <= kClusterSplitHeight;
        if (splits) {
            Lcg rng(static_cast<std::uint32_t>(dropped_ * 7919 + static_cast<int>(b.age * 1000.0)));
            for (int n = 0; n < kBombletCount; ++n) {
                Bomb bl;
                bl.position = b.position;
                bl.velocity = b.velocity + Vec3{rng.range(-9.0, 9.0), rng.range(-2.0, 2.0), rng.range(-9.0, 9.0)};
                bl.kind = BombKind::Cluster;
                bl.bomblet = true;
                spawned.push_back(bl);
            }
            bombs_.erase(bombs_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (b.position.y <= ground) {
            const Bomb hit = b;
            bombs_.erase(bombs_.begin() + static_cast<std::ptrdiff_t>(i));
            detonate(hit, map);
            continue;
        }
        ++i;
    }
    for (const Bomb& bl : spawned) {
        if (bombs_.size() < kMaxBombs) bombs_.push_back(bl);
    }

    for (std::size_t i = 0; i < blasts_.size();) {
        blasts_[i].age += dt;
        if (blasts_[i].age > blasts_[i].duration) blasts_.erase(blasts_.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
    }
    for (Target& t : targets_) {
        if (t.destroyed) t.destroyedAge += dt;
    }
}

int OrdnanceField::destroyedTargets() const noexcept {
    return static_cast<int>(std::count_if(targets_.begin(), targets_.end(),
        [](const Target& t) { return t.destroyed; }));
}

} // namespace skyframe
