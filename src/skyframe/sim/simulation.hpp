#pragma once

#include "skyframe/sim/aircraft.hpp"
#include "skyframe/sim/environment.hpp"
#include "skyframe/sim/ordnance.hpp"
#include "skyframe/sim/replay.hpp"

#include <cstdint>
#include <deque>
#include <string>

namespace skyframe {

struct SimEvent {
    double timeSeconds{0.0};
    std::string message;
};

class Simulation {
public:
    Simulation();

    void update(double frameDt);
    void reset();
    void selectAircraft(int presetIndex);
    void setWeather(WeatherPreset preset);
    void setMap(MapPreset preset);
    void setMapSize(MapSize size);
    void setMapSeed(std::uint32_t seed);
    void randomizeMapSeed();
    void setAirportEnabled(bool enabled);
    void setAirportClass(AirportClass airportClass);
    void setTowersEnabled(bool enabled);
    void setPowerLinesEnabled(bool enabled);
    void setTerrainSceneryEnabled(bool enabled);
    void setTimeOfDay(TimeOfDay mode);
    void setPassengerCount(int count);
    void applyFailure(FailureType failure);
    // Drops the selected bomb if the aircraft has a bay that carries it. Returns true on release.
    bool dropBomb();
    void selectBomb(BombKind kind);
    void cycleBomb(int direction);
    void resetRange();
    void startReplay() noexcept;
    void stopReplay() noexcept;

    [[nodiscard]] Aircraft& aircraft() noexcept { return aircraft_; }
    [[nodiscard]] const Aircraft& aircraft() const noexcept { return aircraft_; }
    [[nodiscard]] WeatherState& weather() noexcept { return weather_; }
    [[nodiscard]] const WeatherState& weather() const noexcept { return weather_; }
    [[nodiscard]] ReplayRecorder& replay() noexcept { return replay_; }
    [[nodiscard]] const ReplayRecorder& replay() const noexcept { return replay_; }
    [[nodiscard]] const OrdnanceField& ordnance() const noexcept { return ordnance_; }
    [[nodiscard]] BombKind selectedBomb() const noexcept { return selectedBomb_; }
    [[nodiscard]] int ordnanceTier() const noexcept;
    [[nodiscard]] double timeSeconds() const noexcept { return timeSeconds_; }
    [[nodiscard]] const std::deque<SimEvent>& events() const noexcept { return events_; }
    [[nodiscard]] MapPreset map() const noexcept { return mapConfig_.terrain; }
    [[nodiscard]] const MapConfig& mapConfig() const noexcept { return mapConfig_; }
    [[nodiscard]] const TimeOfDayState& timeOfDay() const noexcept { return timeOfDay_; }
    [[nodiscard]] int aircraftPresetIndex() const noexcept { return aircraftPresetIndex_; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
    [[nodiscard]] int completedFlights() const noexcept { return completedFlights_; }
    [[nodiscard]] int emergencyLandings() const noexcept { return emergencyLandings_; }
    void setPaused(bool value) noexcept { paused_ = value; }

private:
    void addEvent(std::string message);

    Aircraft aircraft_{};
    WeatherState weather_{};
    ReplayRecorder replay_{};
    std::deque<SimEvent> events_;
    OrdnanceField ordnance_{};
    BombKind selectedBomb_{BombKind::Light};
    double bombCooldown_{0.0};
    MapConfig mapConfig_{};
    TimeOfDayState timeOfDay_{};
    double timeSeconds_{0.0};
    double accumulator_{0.0};
    bool paused_{false};
    bool previousReportState_{false};
    int aircraftPresetIndex_{1};
    int completedFlights_{0};
    int emergencyLandings_{0};
    std::uint32_t failureSequence_{0};
};

[[nodiscard]] int aircraftPresetCount() noexcept;
[[nodiscard]] std::string_view aircraftPresetName(int index) noexcept;
[[nodiscard]] const AircraftConfig& aircraftPresetConfig(int index) noexcept;

} // namespace skyframe
