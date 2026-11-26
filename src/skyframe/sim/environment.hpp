#pragma once

#include "skyframe/core/math.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace skyframe {

enum class WeatherPreset : std::uint8_t {
    Auto,
    Clear,
    Cloudy,
    Rain,
    Storm,
    Fog,
    Snow,
    Windy,
    Overcast,
    Gusty
};

enum class MapPreset : std::uint8_t {
    TrainingField,
    Coast,
    Highlands,
    Desert,
    Hills,
    Plains,
    City,
    Ocean,
    Tundra,
    Mountains,
    Random
};

enum class MapSize : std::uint8_t { Small, Medium, Large };
enum class AirportClass : std::uint8_t { ByMap, Random, International, Regional, Military, Airfield };
enum class TimeOfDay : std::uint8_t { Morning, Day, Evening, Night, Cycle };

enum class TerrainFeatureKind : std::uint8_t {
    Tree,
    Rock,
    Hill,
    Mountain,
    Building,
    Dune,
    Iceberg,
    Water,
    Tower,
    PowerPole
};

struct WeatherState {
    WeatherPreset preset{WeatherPreset::Clear};
    WeatherPreset activePreset{WeatherPreset::Clear};
    Vec3 wind{1.5, 0.0, 0.0};
    double windDirectionDegrees{0.0};
    double turbulence{0.0};
    double gustStrength{0.0};
    double visibilityKm{25.0};
    double cloudBaseMeters{2800.0};
    double rainIntensity{0.0};
    double snowIntensity{0.0};
    double fogDensity{0.0};
    double lightningIntensity{0.0};
    double autoTransitionSeconds{0.0};
    std::uint8_t autoPhase{0};

    void setPreset(WeatherPreset value) noexcept;
    void update(double dt) noexcept;
    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::string_view activeName() const noexcept;
    [[nodiscard]] Vec3 windAt(const Vec3& position, double timeSeconds) const noexcept;
};

struct MapConfig {
    MapPreset terrain{MapPreset::Random};
    MapSize size{MapSize::Medium};
    std::uint32_t seed{27011995U};
    bool airportEnabled{true};
    AirportClass airportClass{AirportClass::ByMap};
    bool towersEnabled{false};
    bool powerLinesEnabled{false};
    bool terrainSceneryEnabled{true};

    [[nodiscard]] bool operator==(const MapConfig& other) const noexcept;
    [[nodiscard]] bool operator!=(const MapConfig& other) const noexcept { return !(*this == other); }
};

struct TerrainFeature {
    TerrainFeatureKind kind{TerrainFeatureKind::Tree};
    Vec3 position{};
    Vec3 scale{1.0, 1.0, 1.0};
    double rotationDegrees{0.0};
    std::uint8_t palette{0};
    std::uint8_t variant{0};
};

inline constexpr int kTerrainGridCells = 32;

struct GeneratedMap {
    MapConfig config{};
    MapPreset resolvedTerrain{MapPreset::TrainingField};
    std::vector<TerrainFeature> features;
    int heightGridCells{kTerrainGridCells};
    std::vector<double> heightField;
    bool hasAirport{false};
    AirportClass resolvedAirportClass{AirportClass::Airfield};
    Vec3 airportPosition{};
    double runwayLength{0.0};
    double runwayWidth{0.0};
    double runwayHeadingDegrees{0.0};
    double worldExtent{1400.0};
    std::uint64_t fingerprint{0};
};

struct TimeOfDayState {
    TimeOfDay mode{TimeOfDay::Day};
    double hours{13.0};

    void setMode(TimeOfDay value) noexcept;
    void update(double dt) noexcept;
};

void setWeatherPresetValues(WeatherState& state, WeatherPreset value) noexcept;
[[nodiscard]] std::string_view weatherPresetName(WeatherPreset value) noexcept;
[[nodiscard]] std::string_view mapName(MapPreset value) noexcept;
[[nodiscard]] MapPreset resolveMapTerrain(const MapConfig& config) noexcept;
[[nodiscard]] std::string_view mapSizeName(MapSize value) noexcept;
[[nodiscard]] std::string_view airportClassName(AirportClass value) noexcept;
[[nodiscard]] std::string_view timeOfDayName(TimeOfDay value) noexcept;
[[nodiscard]] double terrainHeightAt(const MapConfig& config, double x, double z) noexcept;
[[nodiscard]] GeneratedMap generateMap(const MapConfig& config);

} // namespace skyframe
