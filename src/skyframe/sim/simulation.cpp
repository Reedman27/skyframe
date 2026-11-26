#include "skyframe/sim/simulation.hpp"

#include "skyframe/sim/fleet.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <random>
#include <utility>

namespace skyframe {
namespace {
constexpr double kFixedStep = 1.0 / 120.0;

} // namespace

Simulation::Simulation() {
    weather_.setPreset(WeatherPreset::Clear);
    timeOfDay_.setMode(TimeOfDay::Day);
    replay_.record(0.0, aircraft_, aircraftPresetIndex_, weather_, mapConfig_, timeOfDay_);
    ordnance_.generate(mapConfig_);
    addEvent("Sandbox ready - keyboard or on-screen controls are active");
}

void Simulation::update(double frameDt) {
    const double dt = clamp(frameDt, 0.0, 0.05);
    if (replay_.isPlaying()) {
        replay_.updatePlayback(dt);
        return;
    }
    if (paused_) return;

    accumulator_ += dt;
    int steps = 0;
    while (accumulator_ >= kFixedStep && steps < 8) {
        weather_.update(kFixedStep);
        timeOfDay_.update(kFixedStep);
        aircraft_.update(kFixedStep, weather_, timeSeconds_);
        ordnance_.update(kFixedStep, mapConfig_);
        bombCooldown_ = std::max(0.0, bombCooldown_ - kFixedStep);
        timeSeconds_ += kFixedStep;
        accumulator_ -= kFixedStep;
        ++steps;
        replay_.record(timeSeconds_, aircraft_, aircraftPresetIndex_, weather_, mapConfig_, timeOfDay_);

        if (aircraft_.flightEnded() && !previousReportState_) {
            previousReportState_ = true;
            ++completedFlights_;
            if (!aircraft_.report().safeLanding) ++emergencyLandings_;
            addEvent(aircraft_.report().outcome + " - " + aircraft_.config().name);
            paused_ = true;
            break;
        }
    }
    if (steps == 8 && accumulator_ >= kFixedStep) accumulator_ = 0.0;
}

void Simulation::reset() {
    aircraft_.reset();
    replay_.clear();
    timeSeconds_ = 0.0;
    accumulator_ = 0.0;
    previousReportState_ = false;
    failureSequence_ = 0;
    paused_ = false;
    bombCooldown_ = 0.0;
    ordnance_.generate(mapConfig_);
    replay_.record(0.0, aircraft_, aircraftPresetIndex_, weather_, mapConfig_, timeOfDay_);
    addEvent("Aircraft reset");
}

void Simulation::selectAircraft(int presetIndex) {
    aircraftPresetIndex_ = clamp(presetIndex, 0, fleetCount() - 1);
    aircraft_ = Aircraft{fleetEntry(aircraftPresetIndex_).config};
    replay_.clear();
    replay_.record(0.0, aircraft_, aircraftPresetIndex_, weather_, mapConfig_, timeOfDay_);
    timeSeconds_ = 0.0;
    accumulator_ = 0.0;
    previousReportState_ = false;
    failureSequence_ = 0;
    paused_ = false;
    if (ordnanceTier() > 0 && !ordnanceAllows(ordnanceTier(), selectedBomb_)) selectedBomb_ = BombKind::Light;
    bombCooldown_ = 0.0;
    addEvent("Aircraft selected: " + std::string(aircraft_.config().name));
}

int Simulation::ordnanceTier() const noexcept { return fleetEntry(aircraftPresetIndex_).ordnanceTier; }

void Simulation::selectBomb(BombKind kind) { selectedBomb_ = kind; }

void Simulation::cycleBomb(int direction) {
    const int count = kBombKindCount;
    int index = static_cast<int>(selectedBomb_);
    for (int tries = 0; tries < count; ++tries) {
        index = ((index + (direction >= 0 ? 1 : -1)) % count + count) % count;
        if (ordnanceAllows(ordnanceTier(), static_cast<BombKind>(index)) || ordnanceTier() == 0) break;
    }
    selectedBomb_ = static_cast<BombKind>(index);
}

void Simulation::resetRange() {
    ordnance_.generate(mapConfig_);
    addEvent("Target range rebuilt");
}

bool Simulation::dropBomb() {
    if (replay_.isPlaying() || paused_ || aircraft_.flightEnded() || bombCooldown_ > 0.0) return false;
    if (ordnanceTier() == 0) {
        addEvent("No ordnance bay on " + aircraft_.config().name);
        return false;
    }
    if (!ordnanceAllows(ordnanceTier(), selectedBomb_)) {
        addEvent(std::string(bombSpec(selectedBomb_).name) + " not carried by " + aircraft_.config().name);
        return false;
    }
    if (!ordnance_.drop(selectedBomb_, aircraft_.position() - Vec3{0.0, 2.0, 0.0}, aircraft_.velocity())) return false;
    bombCooldown_ = 0.35;
    addEvent("Bomb away: " + std::string(bombSpec(selectedBomb_).name));
    return true;
}

void Simulation::setWeather(WeatherPreset preset) {
    weather_.setPreset(preset);
    addEvent("Weather changed: " + std::string(weather_.name()));
}

void Simulation::setMap(MapPreset preset) {
    mapConfig_.terrain = preset;
    ordnance_.generate(mapConfig_);
    addEvent("Terrain changed: " + std::string(mapName(mapConfig_.terrain)));
}

void Simulation::setMapSize(MapSize size) {
    mapConfig_.size = size;
    ordnance_.generate(mapConfig_);
    addEvent("Map size: " + std::string(mapSizeName(size)));
}

void Simulation::setMapSeed(std::uint32_t seed) {
    mapConfig_.seed = seed == 0U ? 1U : seed;
    ordnance_.generate(mapConfig_);
    addEvent("Map seed changed: " + std::to_string(mapConfig_.seed));
}

void Simulation::randomizeMapSeed() {
    std::uint32_t seed = static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        std::random_device entropy;
        seed ^= static_cast<std::uint32_t>(entropy());
        seed ^= static_cast<std::uint32_t>(entropy()) * 0x9E3779B9U;
    } catch (...) {
        // The monotonic clock still gives a changing seed on systems without random_device.
    }
    setMapSeed(seed == 0U ? 1U : seed);
}

void Simulation::setAirportEnabled(bool enabled) {
    mapConfig_.airportEnabled = enabled;
    addEvent(enabled ? "Airport enabled" : "Airport disabled");
}

void Simulation::setAirportClass(AirportClass airportClass) {
    mapConfig_.airportClass = airportClass;
    addEvent("Airport type: " + std::string(airportClassName(airportClass)));
}

void Simulation::setTowersEnabled(bool enabled) {
    mapConfig_.towersEnabled = enabled;
    addEvent(enabled ? "Towers enabled" : "Towers disabled");
}

void Simulation::setPowerLinesEnabled(bool enabled) {
    mapConfig_.powerLinesEnabled = enabled;
    addEvent(enabled ? "Power lines enabled" : "Power lines disabled");
}

void Simulation::setTerrainSceneryEnabled(bool enabled) {
    mapConfig_.terrainSceneryEnabled = enabled;
    addEvent(enabled ? "Terrain scenery enabled" : "Terrain scenery disabled");
}

void Simulation::setTimeOfDay(TimeOfDay mode) {
    timeOfDay_.setMode(mode);
    addEvent("Time of day: " + std::string(timeOfDayName(mode)));
}

void Simulation::setPassengerCount(int count) {
    aircraft_.setPassengerCount(count);
    addEvent("Passenger manifest updated: " + std::to_string(aircraft_.passengers()));
}

void Simulation::applyFailure(FailureType failure) {
    if (aircraft_.flightEnded()) return;
    const std::uint32_t seed = 0xA341316CU
        ^ (++failureSequence_ * 0x9E3779B9U)
        ^ static_cast<std::uint32_t>(timeSeconds_ * 1000.0);
    aircraft_.applyFailure(failure, seed);
    addEvent(std::string(failureName(failure)) + " triggered");
}

void Simulation::startReplay() noexcept {
    replay_.beginPlayback();
}

void Simulation::stopReplay() noexcept {
    replay_.stopPlayback();
}

void Simulation::addEvent(std::string message) {
    events_.push_back({timeSeconds_, std::move(message)});
    while (events_.size() > 8) events_.pop_front();
}

int aircraftPresetCount() noexcept {
    return fleetCount();
}

std::string_view aircraftPresetName(int index) noexcept {
    if (index < 0 || index >= fleetCount()) return "Unknown aircraft";
    return fleetEntry(index).config.name;
}

const AircraftConfig& aircraftPresetConfig(int index) noexcept {
    return fleetEntry(index).config;
}

} // namespace skyframe
