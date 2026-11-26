#pragma once

#include "skyframe/core/math.hpp"
#include "skyframe/sim/environment.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace skyframe {

enum class FailureType : std::uint8_t {
    LeftEnginePowerLoss,
    RightEnginePowerLoss,
    LeftEngineOverheat,
    RightEngineOverheat,
    LeftWingPanelDamage,
    RightWingPanelDamage,
    FlightControlJam,
    HydraulicFault,
    ElectricalFault,
    LandingGearFault,
    Random
};

enum class FailureGroup : std::uint8_t { Engines, Wings, Controls, Systems, Gear };

struct AircraftConfig {
    std::string name{"Skyline Jumbo #3"};
    int passengerCapacity{180};
    int crewCapacity{2};
    double handling{1.0};
    double acceleration{1.0};
};

struct FlightControls {
    double pitch{0.0};
    double roll{0.0};
    double yaw{0.0};
    double throttle{0.72};
    double flaps{0.15};
    bool gearDown{true};
    bool spoilers{false};
    bool parkingBrake{false};
};

struct EngineState {
    double power{1.0};
    double spool{0.72};
    bool running{true};
    bool overheated{false};
};

struct AircraftSystems {
    std::array<EngineState, 2> engines{};
    double leftWingIntegrity{1.0};
    double rightWingIntegrity{1.0};
    double tailIntegrity{1.0};
    double hydraulicIntegrity{1.0};
    double electricalIntegrity{1.0};
    double landingGearIntegrity{1.0};
    bool leftWingPanelVisible{true};
    bool rightWingPanelVisible{true};
    bool flightControlsLocked{false};
    bool gearStuck{false};
    int affectedSystems{0};
};

struct FlightTelemetry {
    double speed{0.0};
    double altitude{0.0};
    double verticalSpeed{0.0};
    double headingDegrees{0.0};
    double pitchDegrees{0.0};
    double bankDegrees{0.0};
    double throttlePercent{72.0};
    bool stallWarning{false};
    bool sinkRateWarning{false};
    bool pullUpWarning{false};
    bool onGround{false};
};

struct FlightReport {
    bool valid{false};
    bool safeLanding{false};
    std::string aircraftName{"-"};
    std::string outcome{"No flight report"};
    std::string notes{"-"};
    int passengersOnBoard{0};
    int passengersEvacuated{0};
    int crewOnBoard{0};
    int crewEvacuated{0};
    int affectedSystems{0};
};

struct AircraftSnapshot {
    Vec3 position{};
    Vec3 velocity{};
    Quat orientation{};
    FlightControls controls{};
    FlightTelemetry telemetry{};
    AircraftSystems systems{};
    int passengers{0};
    double elapsedSeconds{0.0};
    bool onGround{false};
    bool reportValid{false};
    bool reportSafeLanding{false};
};

class Aircraft {
public:
    explicit Aircraft(AircraftConfig config = {});

    void reset();
    void update(double dt, const WeatherState& weather, double timeSeconds);
    void applyFailure(FailureType failure, std::uint32_t seed = 0U);
    void setPassengerCount(int count) noexcept;
    void reportLanding(bool runwayLanding, std::string notes);

    [[nodiscard]] const AircraftConfig& config() const noexcept { return config_; }
    [[nodiscard]] const FlightControls& controls() const noexcept { return controls_; }
    [[nodiscard]] FlightControls& controls() noexcept { return controls_; }
    [[nodiscard]] const AircraftSystems& systems() const noexcept { return systems_; }
    [[nodiscard]] const FlightTelemetry& telemetry() const noexcept { return telemetry_; }
    [[nodiscard]] const FlightReport& report() const noexcept { return report_; }
    [[nodiscard]] const Vec3& position() const noexcept { return position_; }
    [[nodiscard]] const Vec3& velocity() const noexcept { return velocity_; }
    [[nodiscard]] const Quat& orientation() const noexcept { return orientation_; }
    [[nodiscard]] int passengers() const noexcept { return passengers_; }
    [[nodiscard]] bool flightEnded() const noexcept { return report_.valid; }
    [[nodiscard]] std::string_view status() const noexcept;

    [[nodiscard]] AircraftSnapshot snapshot() const noexcept;
    void restore(const AircraftSnapshot& snapshot);

private:
    void updateTelemetry(const Vec3& relativeWind) noexcept;
    void applyOneFailure(FailureType failure) noexcept;

    AircraftConfig config_;
    FlightControls controls_{};
    AircraftSystems systems_{};
    FlightTelemetry telemetry_{};
    FlightReport report_{};
    Vec3 position_{-250.0, 115.0, 0.0};
    Vec3 velocity_{58.0, 0.0, 0.0};
    Vec3 angularVelocityLocal_{};
    Quat orientation_{};
    int passengers_{145};
    double elapsed_{0.0};
    bool onGround_{false};
};

[[nodiscard]] double nextFlapSetting(double current) noexcept;
[[nodiscard]] std::string_view failureName(FailureType failure) noexcept;
[[nodiscard]] FailureGroup failureGroup(FailureType failure) noexcept;
[[nodiscard]] std::string_view failureGroupName(FailureGroup group) noexcept;

} // namespace skyframe
