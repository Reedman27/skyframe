#include "skyframe/sim/environment.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace skyframe {
namespace {

class DeterministicRandom {
public:
    explicit DeterministicRandom(std::uint32_t seed) noexcept : state_(seed == 0U ? 0x6D2B79F5U : seed) {}

    [[nodiscard]] std::uint32_t next() noexcept {
        state_ ^= state_ << 13U;
        state_ ^= state_ >> 17U;
        state_ ^= state_ << 5U;
        return state_;
    }

    [[nodiscard]] double unit() noexcept {
        return static_cast<double>(next()) / static_cast<double>(0xFFFFFFFFU);
    }

    [[nodiscard]] double range(double low, double high) noexcept {
        return low + (high - low) * unit();
    }

    [[nodiscard]] int integer(int low, int high) noexcept {
        const auto width = static_cast<std::uint32_t>(high - low + 1);
        return low + static_cast<int>(next() % width);
    }

private:
    std::uint32_t state_;
};

[[nodiscard]] double extentFor(MapSize size) noexcept {
    switch (size) {
        case MapSize::Small: return 950.0;
        case MapSize::Medium: return 1550.0;
        case MapSize::Large: return 2350.0;
    }
    return 1550.0;
}

[[nodiscard]] AirportClass resolveAirportClass(AirportClass requested, MapSize size, DeterministicRandom& random) noexcept {
    if (requested == AirportClass::ByMap) {
        if (size == MapSize::Large) return AirportClass::International;
        if (size == MapSize::Medium) return AirportClass::Regional;
        return AirportClass::Airfield;
    }
    if (requested == AirportClass::Random) {
        return static_cast<AirportClass>(random.integer(2, 5));
    }
    return requested;
}

[[nodiscard]] std::uint64_t hashMap(const GeneratedMap& map) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    mix(static_cast<std::uint64_t>(map.config.terrain));
    mix(static_cast<std::uint64_t>(map.config.size));
    mix(map.config.seed);
    mix(map.config.airportEnabled ? 1U : 0U);
    mix(static_cast<std::uint64_t>(map.config.airportClass));
    mix(map.config.towersEnabled ? 1U : 0U);
    mix(map.config.powerLinesEnabled ? 1U : 0U);
    mix(map.config.terrainSceneryEnabled ? 1U : 0U);
    mix(static_cast<std::uint64_t>(map.resolvedTerrain));
    mix(map.heightGridCells);
    mix(map.heightField.size());
    for (double height : map.heightField) {
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(height * 100.0)));
    }
    mix(map.features.size());
    for (const TerrainFeature& feature : map.features) {
        mix(static_cast<std::uint64_t>(feature.kind));
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(feature.position.x * 100.0)));
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(feature.position.y * 100.0)));
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(feature.position.z * 100.0)));
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(feature.scale.x * 100.0)));
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(feature.scale.y * 100.0)));
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(feature.scale.z * 100.0)));
        mix(feature.palette);
        mix(feature.variant);
    }
    mix(map.hasAirport ? 1U : 0U);
    mix(static_cast<std::uint64_t>(map.resolvedAirportClass));
    return hash;
}

[[nodiscard]] TerrainFeatureKind primaryFeature(MapPreset terrain) noexcept {
    switch (terrain) {
        case MapPreset::TrainingField: return TerrainFeatureKind::Tree;
        case MapPreset::Coast: return TerrainFeatureKind::Tree;
        case MapPreset::Highlands: return TerrainFeatureKind::Mountain;
        case MapPreset::Desert: return TerrainFeatureKind::Dune;
        case MapPreset::Hills: return TerrainFeatureKind::Hill;
        case MapPreset::Plains: return TerrainFeatureKind::Tree;
        case MapPreset::City: return TerrainFeatureKind::Building;
        case MapPreset::Ocean: return TerrainFeatureKind::Water;
        case MapPreset::Tundra: return TerrainFeatureKind::Iceberg;
        case MapPreset::Mountains: return TerrainFeatureKind::Mountain;
        case MapPreset::Random: return TerrainFeatureKind::Tree;
    }
    return TerrainFeatureKind::Rock;
}

[[nodiscard]] std::uint32_t latticeHash(std::uint32_t seed, std::int64_t x, std::int64_t z) noexcept {
    std::uint32_t value = seed ^ static_cast<std::uint32_t>(x) * 0x9E3779B9U
        ^ static_cast<std::uint32_t>(z) * 0x85EBCA6BU;
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    return value;
}

[[nodiscard]] double latticeValue(std::uint32_t seed, std::int64_t x, std::int64_t z) noexcept {
    return static_cast<double>(latticeHash(seed, x, z)) / 2147483647.5 - 1.0;
}

[[nodiscard]] double fade(double value) noexcept {
    return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
}

[[nodiscard]] double valueNoise(std::uint32_t seed, double x, double z, double scale) noexcept {
    const double sx = x / scale;
    const double sz = z / scale;
    const auto ix = static_cast<std::int64_t>(std::floor(sx));
    const auto iz = static_cast<std::int64_t>(std::floor(sz));
    const double fx = fade(sx - static_cast<double>(ix));
    const double fz = fade(sz - static_cast<double>(iz));
    const double a = latticeValue(seed, ix, iz);
    const double b = latticeValue(seed, ix + 1, iz);
    const double c = latticeValue(seed, ix, iz + 1);
    const double d = latticeValue(seed, ix + 1, iz + 1);
    const double north = a + (b - a) * fx;
    const double south = c + (d - c) * fx;
    return north + (south - north) * fz;
}

[[nodiscard]] double fractalNoise(const MapConfig& config, double x, double z) noexcept {
    const std::uint32_t seed = config.seed ^ (static_cast<std::uint32_t>(config.terrain) * 0xA511E9B3U);
    return valueNoise(seed, x, z, 520.0) * 0.58
        + valueNoise(seed ^ 0x68BC21EBU, x, z, 210.0) * 0.29
        + valueNoise(seed ^ 0x02E5BE93U, x, z, 82.0) * 0.13;
}

[[nodiscard]] double airportRunwayLength(const MapConfig& config) noexcept {
    AirportClass selected = config.airportClass;
    if (selected == AirportClass::ByMap) {
        selected = config.size == MapSize::Large ? AirportClass::International
            : config.size == MapSize::Medium ? AirportClass::Regional : AirportClass::Airfield;
    } else if (selected == AirportClass::Random) {
        DeterministicRandom random(config.seed);
        (void)random.range(-extentFor(config.size) * 0.08, extentFor(config.size) * 0.08);
        selected = static_cast<AirportClass>(random.integer(2, 5));
    }
    double length = 1500.0;
    switch (selected) {
        case AirportClass::International: length = 3000.0; break;
        case AirportClass::Regional: length = 2050.0; break;
        case AirportClass::Military: length = 2450.0; break;
        case AirportClass::Airfield: length = 1050.0; break;
        case AirportClass::ByMap:
        case AirportClass::Random: break;
    }
    if (config.size == MapSize::Small) length = std::min(length, 1250.0);
    return length;
}

} // namespace

void setWeatherPresetValues(WeatherState& state, WeatherPreset value) noexcept {
    state.activePreset = value == WeatherPreset::Auto ? WeatherPreset::Clear : value;
    state.wind = {1.5, 0.0, 0.0};
    state.windDirectionDegrees = 0.0;
    state.turbulence = 0.0;
    state.gustStrength = 0.0;
    state.visibilityKm = 25.0;
    state.cloudBaseMeters = 2800.0;
    state.rainIntensity = 0.0;
    state.snowIntensity = 0.0;
    state.fogDensity = 0.0;
    state.lightningIntensity = 0.0;

    switch (state.activePreset) {
        case WeatherPreset::Auto:
        case WeatherPreset::Clear:
            break;
        case WeatherPreset::Cloudy:
        case WeatherPreset::Overcast:
            state.wind = {4.0, 0.0, 1.5};
            state.turbulence = 0.08;
            state.visibilityKm = 18.0;
            state.cloudBaseMeters = 1450.0;
            break;
        case WeatherPreset::Rain:
            state.wind = {9.0, 0.0, 5.0};
            state.turbulence = 0.28;
            state.gustStrength = 3.0;
            state.visibilityKm = 5.0;
            state.cloudBaseMeters = 700.0;
            state.rainIntensity = 0.72;
            break;
        case WeatherPreset::Storm:
            state.wind = {17.0, 0.0, 9.0};
            state.turbulence = 0.68;
            state.gustStrength = 12.0;
            state.visibilityKm = 2.5;
            state.cloudBaseMeters = 420.0;
            state.rainIntensity = 0.95;
            state.lightningIntensity = 0.65;
            break;
        case WeatherPreset::Fog:
            state.wind = {2.0, 0.0, 0.5};
            state.visibilityKm = 0.65;
            state.cloudBaseMeters = 230.0;
            state.fogDensity = 0.92;
            break;
        case WeatherPreset::Snow:
            state.wind = {6.0, 0.0, 4.0};
            state.turbulence = 0.24;
            state.gustStrength = 4.5;
            state.visibilityKm = 3.5;
            state.cloudBaseMeters = 550.0;
            state.snowIntensity = 0.78;
            break;
        case WeatherPreset::Windy:
        case WeatherPreset::Gusty:
            state.wind = {13.0, 0.0, -7.0};
            state.turbulence = 0.5;
            state.gustStrength = 9.0;
            state.visibilityKm = 14.0;
            state.cloudBaseMeters = 1750.0;
            break;
    }
    state.windDirectionDegrees = wrapDegrees(degrees(std::atan2(state.wind.z, state.wind.x)));
}

void WeatherState::setPreset(WeatherPreset value) noexcept {
    preset = value;
    autoTransitionSeconds = 0.0;
    autoPhase = 0;
    setWeatherPresetValues(*this, value);
}

void WeatherState::update(double dt) noexcept {
    if (!std::isfinite(dt) || dt <= 0.0 || preset != WeatherPreset::Auto) return;
    const double elapsed = autoTransitionSeconds + std::min(dt, 600.0);
    const auto transitions = static_cast<unsigned int>(elapsed / 150.0);
    if (transitions == 0U) {
        autoTransitionSeconds = elapsed;
        return;
    }
    autoTransitionSeconds = std::fmod(elapsed, 150.0);
    constexpr WeatherPreset rotation[]{WeatherPreset::Clear, WeatherPreset::Cloudy,
        WeatherPreset::Windy, WeatherPreset::Rain, WeatherPreset::Fog, WeatherPreset::Snow};
    autoPhase = static_cast<std::uint8_t>((autoPhase + transitions) % std::size(rotation));
    setWeatherPresetValues(*this, rotation[autoPhase]);
}

std::string_view WeatherState::name() const noexcept {
    return weatherPresetName(preset);
}

std::string_view WeatherState::activeName() const noexcept {
    return weatherPresetName(activePreset);
}

Vec3 WeatherState::windAt(const Vec3& position, double timeSeconds) const noexcept {
    const double phase = timeSeconds * 0.73 + position.x * 0.0017 + position.z * 0.0021;
    const double gust = turbulence * (std::sin(phase) * 2.3 + std::sin(phase * 2.61) * 1.2) * 3.0
        + gustStrength * std::sin(phase * 0.41);
    return wind + Vec3{gust * 0.25, gust * 0.08, gust};
}

bool MapConfig::operator==(const MapConfig& other) const noexcept {
    return terrain == other.terrain && size == other.size && seed == other.seed
        && airportEnabled == other.airportEnabled && airportClass == other.airportClass
        && towersEnabled == other.towersEnabled && powerLinesEnabled == other.powerLinesEnabled
        && terrainSceneryEnabled == other.terrainSceneryEnabled;
}

void TimeOfDayState::setMode(TimeOfDay value) noexcept {
    mode = value;
    switch (value) {
        case TimeOfDay::Morning: hours = 8.0; break;
        case TimeOfDay::Day: hours = 13.0; break;
        case TimeOfDay::Evening: hours = 19.0; break;
        case TimeOfDay::Night: hours = 1.0; break;
        case TimeOfDay::Cycle: if (!std::isfinite(hours)) hours = 13.0; break;
    }
}

void TimeOfDayState::update(double dt) noexcept {
    if (mode != TimeOfDay::Cycle || !std::isfinite(dt) || dt <= 0.0) return;
    hours = std::fmod(hours + std::min(dt, 1.0) * 0.04, 24.0);
}

std::string_view weatherPresetName(WeatherPreset value) noexcept {
    switch (value) {
        case WeatherPreset::Auto: return "Auto";
        case WeatherPreset::Clear: return "Clear";
        case WeatherPreset::Cloudy: return "Cloudy";
        case WeatherPreset::Rain: return "Rain";
        case WeatherPreset::Storm: return "Storm";
        case WeatherPreset::Fog: return "Fog";
        case WeatherPreset::Snow: return "Snow";
        case WeatherPreset::Windy: return "Windy";
        case WeatherPreset::Overcast: return "Overcast";
        case WeatherPreset::Gusty: return "Gusty";
    }
    return "Unknown";
}

std::string_view mapName(MapPreset value) noexcept {
    switch (value) {
        case MapPreset::TrainingField: return "Training field";
        case MapPreset::Coast: return "Coastal approach";
        case MapPreset::Highlands: return "Highlands";
        case MapPreset::Desert: return "Desert";
        case MapPreset::Hills: return "Hills";
        case MapPreset::Plains: return "Plains";
        case MapPreset::City: return "City";
        case MapPreset::Ocean: return "Ocean";
        case MapPreset::Tundra: return "Tundra";
        case MapPreset::Mountains: return "Mountains";
        case MapPreset::Random: return "Random";
    }
    return "Unknown map";
}

MapPreset resolveMapTerrain(const MapConfig& config) noexcept {
    if (config.terrain != MapPreset::Random) return config.terrain;
    DeterministicRandom random(config.seed ^ 0xD1B54A35U);
    return static_cast<MapPreset>(random.integer(
        static_cast<int>(MapPreset::TrainingField), static_cast<int>(MapPreset::Mountains)));
}

std::string_view mapSizeName(MapSize value) noexcept {
    switch (value) {
        case MapSize::Small: return "Small";
        case MapSize::Medium: return "Medium";
        case MapSize::Large: return "Large";
    }
    return "Unknown size";
}

std::string_view airportClassName(AirportClass value) noexcept {
    switch (value) {
        case AirportClass::ByMap: return "By map";
        case AirportClass::Random: return "Random";
        case AirportClass::International: return "International";
        case AirportClass::Regional: return "Regional";
        case AirportClass::Military: return "Military";
        case AirportClass::Airfield: return "Airfield";
    }
    return "Unknown airport";
}

std::string_view timeOfDayName(TimeOfDay value) noexcept {
    switch (value) {
        case TimeOfDay::Morning: return "Morning";
        case TimeOfDay::Day: return "Day";
        case TimeOfDay::Evening: return "Evening";
        case TimeOfDay::Night: return "Night";
        case TimeOfDay::Cycle: return "Cycle";
    }
    return "Unknown time";
}

double terrainHeightAt(const MapConfig& config, double x, double z) noexcept {
    const double extent = extentFor(config.size);
    const MapPreset terrain = resolveMapTerrain(config);
    MapConfig terrainConfig = config;
    terrainConfig.terrain = terrain;
    if (config.airportEnabled) {
        DeterministicRandom random(config.seed);
        const double airportX = random.range(-extent * 0.08, extent * 0.08);
        const double runwayHalf = airportRunwayLength(config) * 0.58;
        if (std::abs(x - airportX) < runwayHalf && std::abs(z) < 115.0) return 0.0;
    }

    const double noise = fractalNoise(terrainConfig, x, z);
    switch (terrain) {
        case MapPreset::TrainingField:
            return 5.0 + noise * 8.0;
        case MapPreset::Coast:
            return z > extent * 0.38 ? -4.0 + noise * 1.5 : 9.0 + noise * 12.0;
        case MapPreset::Highlands:
            return 34.0 + std::abs(noise) * 165.0 + valueNoise(config.seed ^ 0xB5297A4DU, x, z, 96.0) * 28.0;
        case MapPreset::Desert:
            return 9.0 + noise * 12.0 + std::sin((x + z * 0.36) * 0.0038) * 12.0;
        case MapPreset::Hills:
            return 18.0 + noise * 42.0 + std::abs(valueNoise(config.seed ^ 0x1B56C4E9U, x, z, 180.0)) * 20.0;
        case MapPreset::Plains:
            return 3.0 + noise * 5.5;
        case MapPreset::City:
            return 1.0 + noise * 1.8;
        case MapPreset::Ocean:
            return -1.0;
        case MapPreset::Tundra:
            return 8.0 + noise * 21.0 + std::abs(valueNoise(config.seed ^ 0xC2B2AE35U, x, z, 145.0)) * 12.0;
        case MapPreset::Mountains:
            return 72.0 + std::abs(noise) * 300.0
                + std::max(0.0, valueNoise(config.seed ^ 0x27D4EB2FU, x, z, 270.0)) * 105.0;
        case MapPreset::Random:
            break;
    }
    return 0.0;
}

GeneratedMap generateMap(const MapConfig& config) {
    GeneratedMap map;
    map.config = config;
    map.resolvedTerrain = resolveMapTerrain(config);
    const MapPreset terrain = map.resolvedTerrain;
    map.worldExtent = extentFor(config.size);
    map.heightGridCells = kTerrainGridCells;
    const int heightSamples = map.heightGridCells + 1;
    map.heightField.reserve(static_cast<std::size_t>(heightSamples * heightSamples));
    for (int zIndex = 0; zIndex < heightSamples; ++zIndex) {
        const double z = -map.worldExtent + 2.0 * map.worldExtent
            * static_cast<double>(zIndex) / static_cast<double>(map.heightGridCells);
        for (int xIndex = 0; xIndex < heightSamples; ++xIndex) {
            const double x = -map.worldExtent + 2.0 * map.worldExtent
                * static_cast<double>(xIndex) / static_cast<double>(map.heightGridCells);
            map.heightField.push_back(terrainHeightAt(config, x, z));
        }
    }
    DeterministicRandom random(config.seed);
    const double airportOffset = random.range(-map.worldExtent * 0.08, map.worldExtent * 0.08);

    if (config.airportEnabled) {
        map.hasAirport = true;
        map.resolvedAirportClass = resolveAirportClass(config.airportClass, config.size, random);
        map.airportPosition = {airportOffset, 0.0, 0.0};
        switch (map.resolvedAirportClass) {
            case AirportClass::International: map.runwayLength = 3000.0; map.runwayWidth = 60.0; break;
            case AirportClass::Regional: map.runwayLength = 2050.0; map.runwayWidth = 48.0; break;
            case AirportClass::Military: map.runwayLength = 2450.0; map.runwayWidth = 54.0; break;
            case AirportClass::Airfield: map.runwayLength = 1050.0; map.runwayWidth = 32.0; break;
            case AirportClass::ByMap:
            case AirportClass::Random: map.runwayLength = 1500.0; map.runwayWidth = 40.0; break;
        }
        if (config.size == MapSize::Small) map.runwayLength = std::min(map.runwayLength, 1250.0);
    }

    const int baseCount = config.size == MapSize::Small ? 66 : config.size == MapSize::Medium ? 132 : 224;
    const int sceneryCount = config.terrainSceneryEnabled ? baseCount : 0;
    map.features.reserve(static_cast<std::size_t>(sceneryCount + 40));
    for (int i = 0; i < sceneryCount; ++i) {
        const double x = random.range(-map.worldExtent, map.worldExtent);
        const double z = random.range(-map.worldExtent, map.worldExtent);
        if (map.hasAirport && std::abs(z) < 155.0
            && std::abs(x - map.airportPosition.x) < map.runwayLength * 0.62) {
            continue;
        }
        TerrainFeature feature;
        feature.kind = primaryFeature(terrain);
        feature.position = {x, 0.0, z};
        feature.rotationDegrees = random.range(0.0, 360.0);
        feature.variant = static_cast<std::uint8_t>(random.integer(0, 3));
        feature.palette = static_cast<std::uint8_t>(random.integer(0, 4));

        switch (terrain) {
            case MapPreset::TrainingField:
                feature.kind = (i % 4 == 0) ? TerrainFeatureKind::Tree : TerrainFeatureKind::Rock;
                feature.scale = {random.range(5.0, 13.0), random.range(7.0, 19.0), random.range(5.0, 13.0)};
                break;
            case MapPreset::Coast:
                feature.kind = (i % 3 == 0) ? TerrainFeatureKind::Rock : TerrainFeatureKind::Tree;
                feature.scale = {random.range(8.0, 20.0), random.range(8.0, 30.0), random.range(8.0, 20.0)};
                break;
            case MapPreset::Highlands:
            case MapPreset::Mountains:
                feature.kind = TerrainFeatureKind::Mountain;
                feature.scale = {random.range(100.0, 280.0), random.range(160.0, 470.0), random.range(100.0, 280.0)};
                break;
            case MapPreset::Desert:
                feature.kind = TerrainFeatureKind::Dune;
                feature.scale = {random.range(65.0, 180.0), random.range(18.0, 48.0), random.range(55.0, 155.0)};
                break;
            case MapPreset::Hills:
                feature.kind = TerrainFeatureKind::Hill;
                feature.scale = {random.range(75.0, 190.0), random.range(35.0, 105.0), random.range(70.0, 185.0)};
                break;
            case MapPreset::Plains:
                feature.kind = (i % 5 == 0) ? TerrainFeatureKind::Rock : TerrainFeatureKind::Tree;
                feature.scale = {random.range(7.0, 15.0), random.range(8.0, 26.0), random.range(7.0, 15.0)};
                break;
            case MapPreset::City:
                feature.kind = TerrainFeatureKind::Building;
                feature.scale = {random.range(22.0, 70.0), random.range(35.0, 180.0), random.range(22.0, 70.0)};
                break;
            case MapPreset::Ocean:
                feature.kind = TerrainFeatureKind::Water;
                feature.scale = {random.range(100.0, 280.0), random.range(4.0, 12.0), random.range(100.0, 280.0)};
                break;
            case MapPreset::Tundra:
                feature.kind = (i % 2 == 0) ? TerrainFeatureKind::Iceberg : TerrainFeatureKind::Rock;
                feature.scale = {random.range(25.0, 80.0), random.range(20.0, 110.0), random.range(25.0, 80.0)};
                break;
            case MapPreset::Random:
                feature.kind = TerrainFeatureKind::Tree;
                feature.scale = {random.range(7.0, 15.0), random.range(8.0, 26.0), random.range(7.0, 15.0)};
                break;
        }
        feature.position.y = terrainHeightAt(config, feature.position.x, feature.position.z);
        map.features.push_back(feature);
    }

    if (config.towersEnabled) {
        const int towerCount = config.size == MapSize::Small ? 3 : config.size == MapSize::Medium ? 5 : 8;
        for (int i = 0; i < towerCount; ++i) {
            TerrainFeature feature;
            feature.kind = TerrainFeatureKind::Tower;
            feature.position = {
                random.range(-map.worldExtent * 0.82, map.worldExtent * 0.82),
                0.0,
                random.range(-map.worldExtent * 0.82, map.worldExtent * 0.82)};
            if (map.hasAirport && std::abs(feature.position.z) < 210.0
                && std::abs(feature.position.x - map.airportPosition.x) < map.runwayLength * 0.7) {
                feature.position.z += map.worldExtent * 0.18;
            }
            feature.scale = {random.range(7.0, 11.0), random.range(34.0, 62.0), random.range(7.0, 11.0)};
            feature.rotationDegrees = random.range(0.0, 360.0);
            feature.palette = 4;
            feature.position.y = terrainHeightAt(config, feature.position.x, feature.position.z);
            map.features.push_back(feature);
        }
    }

    if (config.powerLinesEnabled) {
        const int poleCount = config.size == MapSize::Small ? 8 : config.size == MapSize::Medium ? 14 : 22;
        for (int i = 0; i < poleCount; ++i) {
            TerrainFeature feature;
            feature.kind = TerrainFeatureKind::PowerPole;
            const double t = poleCount > 1 ? static_cast<double>(i) / static_cast<double>(poleCount - 1) : 0.0;
            feature.position = {
                -map.worldExtent * 0.82 + t * map.worldExtent * 1.64,
                0.0,
                map.worldExtent * 0.48 + std::sin(t * kPi * 2.0) * 42.0};
            feature.scale = {3.0, 18.0, 3.0};
            feature.rotationDegrees = random.range(-5.0, 5.0);
            feature.palette = 4;
            feature.variant = static_cast<std::uint8_t>(i);
            feature.position.y = terrainHeightAt(config, feature.position.x, feature.position.z);
            map.features.push_back(feature);
        }
    }

    map.fingerprint = hashMap(map);
    return map;
}

} // namespace skyframe
