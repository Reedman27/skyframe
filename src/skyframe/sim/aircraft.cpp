#include "skyframe/sim/aircraft.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace skyframe {
namespace {
constexpr double kGravity = 9.81;

std::uint32_t nextRandom(std::uint32_t& state) noexcept {
    if (state == 0U) state = 0xA341316CU;
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return state;
}
} // namespace

Aircraft::Aircraft(AircraftConfig config) : config_(std::move(config)) {
    reset();
}

void Aircraft::reset() {
    controls_ = {};
    controls_.throttle = 0.72;
    controls_.flaps = 0.15;
    controls_.gearDown = true;
    systems_ = {};
    telemetry_ = {};
    report_ = {};
    position_ = {-250.0, 115.0, 0.0};
    velocity_ = {58.0, 0.0, 0.0};
    angularVelocityLocal_ = {};
    orientation_ = Quat::identity();
    passengers_ = std::min(145, config_.passengerCapacity);
    elapsed_ = 0.0;
    onGround_ = false;
}

void Aircraft::update(double dt, const WeatherState& weather, double timeSeconds) {
    if (report_.valid || dt <= 0.0) return;
    dt = clamp(dt, 0.0, 0.025);
    elapsed_ += dt;
    controls_.pitch = clamp(controls_.pitch, -1.0, 1.0);
    controls_.roll = clamp(controls_.roll, -1.0, 1.0);
    controls_.yaw = clamp(controls_.yaw, -1.0, 1.0);
    controls_.throttle = clamp(controls_.throttle, 0.0, 1.0);
    controls_.flaps = clamp(controls_.flaps, 0.0, 1.0);

    const Vec3 wind = weather.windAt(position_, timeSeconds);
    const Vec3 relativeWind = velocity_ - wind;
    const double speed = relativeWind.length();

    double enginePower = 0.0;
    for (EngineState& engine : systems_.engines) {
        const double target = engine.running ? controls_.throttle : 0.0;
        engine.spool += (target - engine.spool) * clamp(dt * 2.2, 0.0, 1.0);
        if (engine.overheated) engine.power = std::max(0.28, engine.power - dt * 0.018);
        enginePower += engine.power * engine.spool;
    }
    enginePower *= 0.5;

    const double controlHealth = systems_.flightControlsLocked ? 0.12
        : systems_.hydraulicIntegrity;
    const double tailResponse = systems_.tailIntegrity * controlHealth;
    const double rollInput = controls_.roll * std::min(systems_.leftWingIntegrity, systems_.rightWingIntegrity) * controlHealth;
    const double pitchInput = controls_.pitch * tailResponse;
    const double yawInput = controls_.yaw * tailResponse;

    // This is deliberately an arcade flight model, tuned for game feel rather than
    // calibrated to any real aircraft or intended to predict real-world outcomes.
    const double handling = std::max(0.25, config_.handling);
    angularVelocityLocal_.x += (rollInput * 1.10 * handling - angularVelocityLocal_.x * 2.0) * dt;
    angularVelocityLocal_.y += (yawInput * 0.42 * handling - angularVelocityLocal_.y * 1.8) * dt;
    angularVelocityLocal_.z += (pitchInput * 0.78 * handling - angularVelocityLocal_.z * 1.7) * dt;
    angularVelocityLocal_.x = clamp(angularVelocityLocal_.x, -1.4, 1.4);
    angularVelocityLocal_.y = clamp(angularVelocityLocal_.y, -0.65, 0.65);
    angularVelocityLocal_.z = clamp(angularVelocityLocal_.z, -0.9, 0.9);
    orientation_.integrateLocalAngularVelocity(angularVelocityLocal_, dt);

    const Vec3 forward = orientation_.rotate({1.0, 0.0, 0.0}).normalized();
    const Vec3 up = orientation_.rotate({0.0, 1.0, 0.0}).normalized();
    const double flapSetting = clamp(controls_.flaps, 0.0, 1.0);
    const double drag = speed * 0.075 + (controls_.gearDown ? 1.5 : 0.0)
        + flapSetting * 2.0 + (controls_.spoilers ? 8.0 : 0.0);
    const double forwardAcceleration = (controls_.throttle * 17.0 * enginePower
        - drag) * std::max(0.35, config_.acceleration);
    const double speedLift = clamp((speed - 16.0) / 40.0, 0.0, 1.0);
    const double wingCondition = 0.5 * (systems_.leftWingIntegrity + systems_.rightWingIntegrity);
    const double flapLift = flapSetting * 1.8 * (1.0 - speedLift);
    const double arcadeLift = (kGravity * speedLift + flapLift) * wingCondition;
    const double turbulence = weather.turbulence * std::sin(timeSeconds * 2.1 + position_.x * 0.007) * 1.35;

    Vec3 acceleration = forward * forwardAcceleration;
    acceleration += up * arcadeLift;
    acceleration.y -= kGravity;
    acceleration += Vec3{0.0, turbulence, 0.0};
    acceleration += Vec3{wind.x * 0.012, 0.0, wind.z * 0.012};

    velocity_ += acceleration * dt;
    const double totalSpeed = velocity_.length();
    if (totalSpeed > 155.0) velocity_ *= 155.0 / totalSpeed;
    position_ += velocity_ * dt;

    const double groundClearance = controls_.gearDown && !systems_.gearStuck ? 1.9 : 0.65;
    onGround_ = position_.y <= groundClearance + 0.12;
    if (controls_.parkingBrake && position_.y <= groundClearance + 0.5) {
        const double brakeFactor = std::max(0.0, 1.0 - dt * 4.0);
        velocity_.x *= brakeFactor;
        velocity_.z *= brakeFactor;
    }
    if (position_.y <= groundClearance) {
        position_.y = groundClearance;
        const bool insideRunway = std::abs(position_.z) < 28.0
            && position_.x > -850.0 && position_.x < 1100.0;
        const bool stableTouchdown = controls_.gearDown && !systems_.gearStuck
            && velocity_.y > -13.0 && velocity_.length() < 100.0;
        reportLanding(insideRunway && stableTouchdown,
            insideRunway ? "The aircraft reached the runway area. Passengers and crew evacuated." :
                           "The aircraft stopped outside the runway. Passengers and crew evacuated.");
        velocity_.y = 0.0;
        velocity_.x *= 0.12;
        velocity_.z *= 0.12;
    }

    updateTelemetry(relativeWind);
}

void Aircraft::updateTelemetry(const Vec3& relativeWind) noexcept {
    const Vec3 forward = orientation_.rotate({1.0, 0.0, 0.0});
    const Vec3 up = orientation_.rotate({0.0, 1.0, 0.0});
    telemetry_.speed = relativeWind.length();
    telemetry_.altitude = std::max(0.0, position_.y);
    telemetry_.verticalSpeed = velocity_.y;
    telemetry_.headingDegrees = wrapDegrees(degrees(std::atan2(forward.z, forward.x)));
    telemetry_.pitchDegrees = degrees(std::asin(clamp(forward.y, -1.0, 1.0)));
    telemetry_.bankDegrees = degrees(std::atan2(up.z, up.y));
    telemetry_.throttlePercent = controls_.throttle * 100.0;
    telemetry_.stallWarning = telemetry_.speed < 27.0 && telemetry_.altitude > 12.0;
    telemetry_.sinkRateWarning = telemetry_.verticalSpeed < -10.0 && telemetry_.altitude < 420.0;
    telemetry_.pullUpWarning = telemetry_.verticalSpeed < -7.0 && telemetry_.altitude < 80.0;
    telemetry_.onGround = onGround_;
}

void Aircraft::applyFailure(FailureType failure, std::uint32_t seed) {
    if (report_.valid) return;
    if (failure != FailureType::Random) {
        applyOneFailure(failure);
        return;
    }

    constexpr FailureType choices[] = {
        FailureType::LeftEnginePowerLoss, FailureType::RightEnginePowerLoss,
        FailureType::LeftEngineOverheat, FailureType::RightEngineOverheat,
        FailureType::LeftWingPanelDamage, FailureType::RightWingPanelDamage,
        FailureType::FlightControlJam, FailureType::HydraulicFault,
        FailureType::ElectricalFault, FailureType::LandingGearFault
    };
    const auto index = nextRandom(seed) % static_cast<std::uint32_t>(std::size(choices));
    applyOneFailure(choices[index]);
}

void Aircraft::applyOneFailure(FailureType failure) noexcept {
    switch (failure) {
        case FailureType::LeftEnginePowerLoss:
            systems_.engines[0].running = false;
            systems_.engines[0].power = 0.0;
            break;
        case FailureType::RightEnginePowerLoss:
            systems_.engines[1].running = false;
            systems_.engines[1].power = 0.0;
            break;
        case FailureType::LeftEngineOverheat:
            systems_.engines[0].overheated = true;
            systems_.engines[0].power = std::min(systems_.engines[0].power, 0.72);
            break;
        case FailureType::RightEngineOverheat:
            systems_.engines[1].overheated = true;
            systems_.engines[1].power = std::min(systems_.engines[1].power, 0.72);
            break;
        case FailureType::LeftWingPanelDamage:
            systems_.leftWingIntegrity = 0.55;
            systems_.leftWingPanelVisible = false;
            break;
        case FailureType::RightWingPanelDamage:
            systems_.rightWingIntegrity = 0.55;
            systems_.rightWingPanelVisible = false;
            break;
        case FailureType::FlightControlJam:
            systems_.flightControlsLocked = true;
            systems_.tailIntegrity = 0.4;
            break;
        case FailureType::HydraulicFault:
            systems_.hydraulicIntegrity = 0.28;
            break;
        case FailureType::ElectricalFault:
            systems_.electricalIntegrity = 0.0;
            break;
        case FailureType::LandingGearFault:
            systems_.gearStuck = true;
            systems_.landingGearIntegrity = 0.15;
            break;
        case FailureType::Random:
            break;
    }
    ++systems_.affectedSystems;
}

void Aircraft::setPassengerCount(int count) noexcept {
    passengers_ = clamp(count, 0, std::max(0, config_.passengerCapacity));
}

void Aircraft::reportLanding(bool runwayLanding, std::string notes) {
    if (report_.valid) return;
    report_.valid = true;
    report_.safeLanding = runwayLanding;
    report_.aircraftName = config_.name;
    report_.outcome = runwayLanding ? "Safe landing" : "Emergency landing";
    report_.notes = std::move(notes);
    report_.passengersOnBoard = passengers_;
    report_.passengersEvacuated = passengers_;
    report_.crewOnBoard = config_.crewCapacity;
    report_.crewEvacuated = config_.crewCapacity;
    report_.affectedSystems = systems_.affectedSystems;
}

AircraftSnapshot Aircraft::snapshot() const noexcept {
    return {position_, velocity_, orientation_, controls_, telemetry_, systems_, passengers_,
        elapsed_, onGround_, report_.valid, report_.safeLanding};
}

void Aircraft::restore(const AircraftSnapshot& value) {
    position_ = value.position;
    velocity_ = value.velocity;
    orientation_ = value.orientation.normalized();
    controls_ = value.controls;
    telemetry_ = value.telemetry;
    systems_ = value.systems;
    passengers_ = clamp(value.passengers, 0, std::max(0, config_.passengerCapacity));
    elapsed_ = std::max(0.0, value.elapsedSeconds);
    onGround_ = value.onGround;
    report_ = {};
    if (value.reportValid) {
        reportLanding(value.reportSafeLanding, value.reportSafeLanding
            ? "Recorded flight completed with a safe evacuation."
            : "Recorded flight completed with an emergency evacuation.");
    }
}

std::string_view Aircraft::status() const noexcept {
    if (report_.valid) return report_.safeLanding ? "Flight complete" : "Emergency landing";
    if (telemetry_.pullUpWarning) return "PULL UP";
    if (telemetry_.stallWarning) return "STALL WARNING";
    if (telemetry_.sinkRateWarning) return "SINK RATE";
    if (telemetry_.onGround) return "On runway";
    if (systems_.engines[0].overheated || systems_.engines[1].overheated) return "Engine temperature";
    if (!systems_.engines[0].running || !systems_.engines[1].running) return "Reduced engine power";
    return "In flight";
}

double nextFlapSetting(double current) noexcept {
    constexpr std::array<double, 5> settings{0.0, 0.15, 0.35, 0.65, 1.0};
    for (const double setting : settings) {
        if (setting > current + 1.0e-6) return setting;
    }
    return settings.front();
}

std::string_view failureName(FailureType failure) noexcept {
    switch (failure) {
        case FailureType::LeftEnginePowerLoss: return "Left engine power loss";
        case FailureType::RightEnginePowerLoss: return "Right engine power loss";
        case FailureType::LeftEngineOverheat: return "Left engine overheating";
        case FailureType::RightEngineOverheat: return "Right engine overheating";
        case FailureType::LeftWingPanelDamage: return "Left wing panel damaged";
        case FailureType::RightWingPanelDamage: return "Right wing panel damaged";
        case FailureType::FlightControlJam: return "Flight control jam";
        case FailureType::HydraulicFault: return "Hydraulic system fault";
        case FailureType::ElectricalFault: return "Electrical system fault";
        case FailureType::LandingGearFault: return "Landing gear fault";
        case FailureType::Random: return "Random system fault";
    }
    return "Unknown system fault";
}

FailureGroup failureGroup(FailureType failure) noexcept {
    switch (failure) {
        case FailureType::LeftEnginePowerLoss:
        case FailureType::RightEnginePowerLoss:
        case FailureType::LeftEngineOverheat:
        case FailureType::RightEngineOverheat: return FailureGroup::Engines;
        case FailureType::LeftWingPanelDamage:
        case FailureType::RightWingPanelDamage: return FailureGroup::Wings;
        case FailureType::FlightControlJam: return FailureGroup::Controls;
        case FailureType::HydraulicFault:
        case FailureType::ElectricalFault: return FailureGroup::Systems;
        case FailureType::LandingGearFault: return FailureGroup::Gear;
        case FailureType::Random: return FailureGroup::Systems;
    }
    return FailureGroup::Systems;
}

std::string_view failureGroupName(FailureGroup group) noexcept {
    switch (group) {
        case FailureGroup::Engines: return "Engines";
        case FailureGroup::Wings: return "Wings";
        case FailureGroup::Controls: return "Controls";
        case FailureGroup::Systems: return "Systems";
        case FailureGroup::Gear: return "Gear";
    }
    return "Systems";
}

} // namespace skyframe
