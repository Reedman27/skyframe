#include "skyframe/sim/fleet.hpp"
#include "skyframe/sim/simulation.hpp"
#include "skyframe/sim/replay_archive.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

bool near(double a, double b, double tolerance = 1.0e-3) {
    return std::abs(a - b) <= tolerance;
}

bool makeOverCapacityReplay(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::ifstream input(source);
    if (!input) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) lines.push_back(line);
    if (lines.size() < 4U) return false;

    const std::size_t delimiter = lines[3].find(';');
    if (delimiter == std::string::npos) return false;
    std::istringstream environmentInput(lines[3].substr(0, delimiter));
    std::vector<std::string> environmentTokens;
    while (environmentInput >> line) environmentTokens.push_back(line);
    if (environmentTokens.size() < 3U) return false;
    environmentTokens[2] = "0"; // Northstar Regional has a smaller capacity than this test load.

    std::istringstream snapshotInput(lines[3].substr(delimiter + 1U));
    std::vector<std::string> snapshotTokens;
    while (snapshotInput >> line) snapshotTokens.push_back(line);
    if (snapshotTokens.size() < 49U) return false;
    snapshotTokens[48] = "85"; // passenger count follows the affected-system bitmask.

    std::ostringstream rebuilt;
    for (std::size_t i = 0; i < environmentTokens.size(); ++i) {
        if (i != 0U) rebuilt << ' ';
        rebuilt << environmentTokens[i];
    }
    rebuilt << ';';
    for (const std::string& token : snapshotTokens) rebuilt << ' ' << token;
    lines[3] = rebuilt.str();

    std::ofstream output(destination, std::ios::trunc);
    if (!output) return false;
    for (const std::string& outputLine : lines) output << outputLine << '\n';
    return static_cast<bool>(output);
}
bool makeLegacyReplayFile(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::ifstream input(source);
    if (!input) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) lines.push_back(line);
    if (lines.size() < 4U) return false;
    lines[0] = "SKYFRAME_REPLAY 1";

    for (std::size_t lineIndex = 3U; lineIndex < lines.size(); ++lineIndex) {
        const std::size_t delimiter = lines[lineIndex].find(';');
        if (delimiter == std::string::npos) return false;
        std::istringstream environmentInput(lines[lineIndex].substr(0, delimiter));
        std::vector<std::string> tokens;
        while (environmentInput >> line) tokens.push_back(line);
        if (tokens.size() < 29U || tokens[0] != "FRAME") return false;

        const std::string legacyInfrastructure = (tokens[8] == "1" || tokens[9] == "1") ? "1" : "0";
        if (tokens[3] == std::to_string(static_cast<int>(skyframe::MapPreset::Random))) {
            tokens[3] = std::to_string(static_cast<int>(skyframe::MapPreset::Tundra));
        }
        std::ostringstream rebuilt;
        for (std::size_t i = 0; i < 8U; ++i) rebuilt << tokens[i] << ' ';
        rebuilt << legacyInfrastructure;
        for (std::size_t i = 11U; i < tokens.size(); ++i) rebuilt << ' ' << tokens[i];
        rebuilt << lines[lineIndex].substr(delimiter);
        lines[lineIndex] = rebuilt.str();
    }

    std::ofstream output(destination, std::ios::trunc);
    if (!output) return false;
    for (const std::string& outputLine : lines) output << outputLine << '\n';
    return static_cast<bool>(output);
}

} // namespace

int main() {
    using namespace skyframe;

    Simulation simulation;
    check(aircraftPresetCount() == 20, "the free fleet is registered");
    check(simulation.aircraft().passengers() <= simulation.aircraft().config().passengerCapacity,
        "initial passenger count fits capacity");
    check(aircraftPresetConfig(1).name == "Skyline Jumbo #3", "preset config lookup is stable");

    simulation.setPassengerCount(100000);
    check(simulation.aircraft().passengers() == simulation.aircraft().config().passengerCapacity,
        "passenger count clamps to aircraft capacity");
    simulation.setPassengerCount(-50);
    check(simulation.aircraft().passengers() == 0, "passenger count cannot become negative");
    simulation.setPassengerCount(125);

    simulation.applyFailure(FailureType::LeftEnginePowerLoss);
    check(!simulation.aircraft().systems().engines[0].running, "engine power loss disables selected engine");
    check(simulation.aircraft().systems().engines[0].power == 0.0, "engine power loss sets its game power to zero");

    simulation.setWeather(WeatherPreset::Rain);
    check(simulation.weather().preset == WeatherPreset::Rain, "weather preset changes");
    simulation.setMap(MapPreset::Highlands);
    check(simulation.map() == MapPreset::Highlands, "map preset changes");

    MapConfig deterministicMap;
    deterministicMap.terrain = MapPreset::Mountains;
    deterministicMap.size = MapSize::Large;
    deterministicMap.seed = 0x1234ABCDU;
    deterministicMap.airportClass = AirportClass::Random;
    deterministicMap.towersEnabled = true;
    deterministicMap.powerLinesEnabled = true;
    const GeneratedMap generatedA = generateMap(deterministicMap);
    const GeneratedMap generatedB = generateMap(deterministicMap);
    check(generatedA.fingerprint == generatedB.fingerprint && generatedA.heightField == generatedB.heightField,
        "map generation is deterministic for an identical terrain configuration and seed");
    check(generatedA.heightGridCells == kTerrainGridCells
        && generatedA.heightField.size() == static_cast<std::size_t>((kTerrainGridCells + 1) * (kTerrainGridCells + 1)),
        "map generator caches a bounded 33 by 33 terrain height field per world");
    check(generatedA.features.size() > 100 && generatedA.features.size() < 300,
        "generated map feature count is bounded for the large-map preset");
    check(generatedA.hasAirport && generatedA.runwayLength > 0.0 && generatedA.runwayWidth > 0.0,
        "map generator creates runway and airport dimensions when enabled");
    check(generatedA.resolvedAirportClass >= AirportClass::International
        && generatedA.resolvedAirportClass <= AirportClass::Airfield,
        "random airport class resolves to a concrete airport type");
    MapConfig changedSeed = deterministicMap;
    changedSeed.seed += 1U;
    check(generateMap(changedSeed).fingerprint != generatedA.fingerprint,
        "changing the map seed changes the generated world");
    MapConfig noAirport = deterministicMap;
    noAirport.airportEnabled = false;
    check(!generateMap(noAirport).hasAirport,
        "map generator omits the airport when the airport toggle is disabled");

    MapConfig randomTerrain;
    randomTerrain.terrain = MapPreset::Random;
    randomTerrain.seed = 0x51A7E123U;
    const GeneratedMap randomWorldA = generateMap(randomTerrain);
    const GeneratedMap randomWorldB = generateMap(randomTerrain);
    check(randomWorldA.resolvedTerrain != MapPreset::Random
        && randomWorldA.resolvedTerrain <= MapPreset::Mountains,
        "random terrain resolves to one concrete supported biome");
    check(randomWorldA.resolvedTerrain == randomWorldB.resolvedTerrain
        && randomWorldA.fingerprint == randomWorldB.fingerprint,
        "random terrain is repeatable for the same seed");
    bool randomSeedsVaryTerrain = false;
    const MapPreset initialRandomTerrain = randomWorldA.resolvedTerrain;
    for (std::uint32_t seed = 1U; seed <= 32U; ++seed) {
        randomTerrain.seed = seed;
        if (resolveMapTerrain(randomTerrain) != initialRandomTerrain) randomSeedsVaryTerrain = true;
    }
    check(randomSeedsVaryTerrain, "changing the seed can select a different random terrain biome");

    MapConfig noScenery;
    noScenery.terrainSceneryEnabled = false;
    noScenery.airportEnabled = false;
    const GeneratedMap emptySceneryWorld = generateMap(noScenery);
    check(emptySceneryWorld.features.empty(), "natural scenery can be disabled independently of terrain generation");
    MapConfig towerOnly = noScenery;
    towerOnly.towersEnabled = true;
    const GeneratedMap towerOnlyWorld = generateMap(towerOnly);
    check(std::count_if(towerOnlyWorld.features.begin(), towerOnlyWorld.features.end(), [](const TerrainFeature& feature) {
        return feature.kind == TerrainFeatureKind::Tower;
    }) == 5 && std::none_of(towerOnlyWorld.features.begin(), towerOnlyWorld.features.end(), [](const TerrainFeature& feature) {
        return feature.kind == TerrainFeatureKind::PowerPole;
    }), "tower toggle creates towers without enabling power lines");
    MapConfig powerLinesOnly = noScenery;
    powerLinesOnly.powerLinesEnabled = true;
    const GeneratedMap powerLinesOnlyWorld = generateMap(powerLinesOnly);
    check(std::count_if(powerLinesOnlyWorld.features.begin(), powerLinesOnlyWorld.features.end(), [](const TerrainFeature& feature) {
        return feature.kind == TerrainFeatureKind::PowerPole;
    }) == 14 && std::none_of(powerLinesOnlyWorld.features.begin(), powerLinesOnlyWorld.features.end(), [](const TerrainFeature& feature) {
        return feature.kind == TerrainFeatureKind::Tower;
    }), "power-line toggle creates poles without enabling towers");
    check(near(terrainHeightAt(deterministicMap, generatedA.airportPosition.x, 0.0), 0.0),
        "airport runway corridor is flattened deterministically into the terrain height field");
    const double mountainLow = terrainHeightAt(deterministicMap, -900.0, -760.0);
    const double mountainHigh = terrainHeightAt(deterministicMap, 870.0, 810.0);
    check(std::isfinite(mountainLow) && std::isfinite(mountainHigh)
        && std::abs(mountainLow - mountainHigh) > 15.0,
        "mountain terrain has deterministic non-flat elevation variation");
    bool allTerrainKindsValid = true;
    for (int terrain = 0; terrain <= static_cast<int>(MapPreset::Random); ++terrain) {
        MapConfig config;
        config.terrain = static_cast<MapPreset>(terrain);
        const GeneratedMap world = generateMap(config);
        if (world.features.empty() || world.fingerprint == 0U
            || world.heightField.size() != static_cast<std::size_t>((kTerrainGridCells + 1) * (kTerrainGridCells + 1))) {
            allTerrainKindsValid = false;
        }
        for (int sample = 0; sample < 12; ++sample) {
            const double x = -world.worldExtent + static_cast<double>(sample) * world.worldExtent / 6.0;
            const double z = world.worldExtent - static_cast<double>(sample) * world.worldExtent / 6.0;
            if (!std::isfinite(terrainHeightAt(config, x, z))) allTerrainKindsValid = false;
        }
        for (const TerrainFeature& feature : world.features) {
            if (!feature.position.finite() || !feature.scale.finite()
                || feature.scale.x <= 0.0 || feature.scale.y <= 0.0 || feature.scale.z <= 0.0) {
                allTerrainKindsValid = false;
            }
        }
    }
    check(allTerrainKindsValid, "all terrain themes generate finite positive bounded features");

    WeatherState automaticWeather;
    automaticWeather.setPreset(WeatherPreset::Auto);
    automaticWeather.update(151.0);
    check(automaticWeather.preset == WeatherPreset::Auto
        && automaticWeather.activePreset == WeatherPreset::Cloudy,
        "automatic weather advances from clear to cloudy without losing Auto mode");
    automaticWeather.update(151.0);
    check(automaticWeather.activePreset == WeatherPreset::Windy,
        "automatic weather follows its deterministic weather sequence");
    WeatherState stormWeather;
    stormWeather.setPreset(WeatherPreset::Storm);
    check(stormWeather.rainIntensity > 0.8 && stormWeather.lightningIntensity > 0.0
        && stormWeather.gustStrength > 0.0,
        "storm preset configures rain, gusts and lightning atmosphere");
    WeatherState fogWeather;
    fogWeather.setPreset(WeatherPreset::Fog);
    check(fogWeather.visibilityKm < 1.0 && fogWeather.fogDensity > 0.8,
        "fog preset configures reduced visibility and fog density");
    TimeOfDayState cycleTime;
    cycleTime.setMode(TimeOfDay::Cycle);
    const double cycleStart = cycleTime.hours;
    cycleTime.update(60.0);
    check(cycleTime.hours > cycleStart && cycleTime.hours < 24.0,
        "time-of-day cycle advances predictably and wraps within one day");

    for (int i = 0; i < 360; ++i) simulation.update(1.0 / 60.0);
    check(simulation.replay().duration() > 1.0, "replay recorder stores a rolling history");
    check(simulation.aircraft().position().finite(), "aircraft position remains finite");
    check(simulation.aircraft().velocity().finite(), "aircraft velocity remains finite");
    check(std::isfinite(simulation.aircraft().telemetry().headingDegrees), "heading telemetry remains finite");

    simulation.startReplay();
    check(simulation.replay().isPlaying(), "replay can start with recorded frames");
    check(simulation.replay().currentFrame() != nullptr, "replay exposes a current snapshot");
    check(simulation.replay().frames().back().mapConfig.terrain == MapPreset::Highlands, "replay captures map context");
    check(simulation.replay().frames().back().weather.preset == WeatherPreset::Rain, "replay captures weather context");
    simulation.update(0.25);
    check(simulation.replay().isPlaying(), "replay advances without changing simulation state");
    simulation.stopReplay();
    check(!simulation.replay().isPlaying(), "replay can stop");

    simulation.selectAircraft(0);
    check(simulation.aircraft().config().passengerCapacity == 84, "aircraft selection loads the new config");
    check(simulation.aircraft().config().name == "Northstar Regional", "selected aircraft has the expected name");

    simulation.applyFailure(FailureType::LeftWingPanelDamage);
    check(!simulation.aircraft().systems().leftWingPanelVisible, "wing fault updates the procedural visual state");
    check(simulation.aircraft().systems().leftWingIntegrity < 1.0, "wing fault lowers arcade handling integrity");

    Aircraft reportAircraft;
    reportAircraft.setPassengerCount(31);
    reportAircraft.reportLanding(true, "Test evacuation");
    check(reportAircraft.report().safeLanding, "landing report records a safe landing");
    check(reportAircraft.report().passengersEvacuated == 31, "landing report records passenger evacuation");
    check(reportAircraft.report().crewEvacuated == reportAircraft.config().crewCapacity,
        "landing report records crew evacuation");
    Aircraft restoredAircraft;
    restoredAircraft.restore(reportAircraft.snapshot());
    check(restoredAircraft.flightEnded(), "aircraft snapshot preserves completed-flight state");
    check(restoredAircraft.report().safeLanding, "aircraft snapshot preserves landing outcome");

    Aircraft randomizedA;
    Aircraft randomizedB;
    randomizedA.applyFailure(FailureType::Random, 12345U);
    randomizedB.applyFailure(FailureType::Random, 12345U);
    check(randomizedA.systems().affectedSystems == randomizedB.systems().affectedSystems,
        "random fault selection is deterministic for a fixed seed");
    check(randomizedA.systems().engines[0].running == randomizedB.systems().engines[0].running
        && randomizedA.systems().leftWingIntegrity == randomizedB.systems().leftWingIntegrity
        && randomizedA.systems().rightWingIntegrity == randomizedB.systems().rightWingIntegrity,
        "same random seed produces the same aircraft system state");

    Aircraft flapsUp;
    Aircraft flapsDown;
    auto upState = flapsUp.snapshot();
    auto downState = flapsDown.snapshot();
    upState.position.y = 50.0;
    downState.position.y = 50.0;
    upState.velocity = {28.0, 0.0, 0.0};
    downState.velocity = {28.0, 0.0, 0.0};
    upState.controls.flaps = 0.0;
    downState.controls.flaps = 1.0;
    flapsUp.restore(upState);
    flapsDown.restore(downState);
    WeatherState calmWeather;
    flapsUp.update(1.0 / 120.0, calmWeather, 0.0);
    flapsDown.update(1.0 / 120.0, calmWeather, 0.0);
    check(flapsDown.telemetry().verticalSpeed > flapsUp.telemetry().verticalSpeed,
        "flaps affect arcade lift instead of being a cosmetic-only control");

    Aircraft warningAircraft;
    auto warningState = warningAircraft.snapshot();
    warningState.position.y = 50.0;
    warningState.velocity.y = -8.0;
    warningAircraft.restore(warningState);
    warningAircraft.update(1.0 / 120.0, calmWeather, 0.0);
    check(warningAircraft.telemetry().pullUpWarning, "low-altitude descent activates the pull-up warning");

    Aircraft clampedControls;
    clampedControls.controls().pitch = 9.0;
    clampedControls.controls().roll = -9.0;
    clampedControls.controls().yaw = 4.0;
    clampedControls.controls().throttle = 3.0;
    clampedControls.controls().flaps = 2.0;
    clampedControls.update(1.0 / 120.0, calmWeather, 0.0);
    check(clampedControls.controls().pitch == 1.0 && clampedControls.controls().roll == -1.0
        && clampedControls.controls().yaw == 1.0, "simulation clamps control axes to valid input ranges");
    check(clampedControls.controls().throttle == 1.0 && clampedControls.controls().flaps == 1.0,
        "simulation clamps throttle and flap inputs to valid ranges");
    check(near(nextFlapSetting(0.0), 0.15) && near(nextFlapSetting(0.35), 0.65)
        && near(nextFlapSetting(1.0), 0.0), "flap control cycles through its detents and wraps to zero");

    Simulation unattendedSimulation;
    for (int i = 0; i < 30 * 60; ++i) unattendedSimulation.update(1.0 / 60.0);
    check(!unattendedSimulation.aircraft().flightEnded(),
        "default run-and-watch scenario remains airborne without player input");
    check(unattendedSimulation.aircraft().position().y > 110.0,
        "default run-and-watch scenario maintains its starting altitude");
    check(std::abs(unattendedSimulation.aircraft().telemetry().verticalSpeed) < 0.5,
        "default run-and-watch scenario settles into level flight without player input");

    ReplayRecorder boundedReplay(0.1, 3);
    Aircraft replayAircraft;
    WeatherState replayWeather;
    for (int i = 0; i < 4; ++i) {
        auto frame = replayAircraft.snapshot();
        frame.position.x = static_cast<double>(i * 10);
        replayAircraft.restore(frame);
        replayWeather.setPreset(i < 2 ? WeatherPreset::Clear : WeatherPreset::Rain);
        MapConfig frameMap;
        frameMap.terrain = i < 2 ? MapPreset::TrainingField : MapPreset::Coast;
        frameMap.seed = static_cast<std::uint32_t>(100 + i);
        TimeOfDayState frameTime;
        frameTime.setMode(i < 2 ? TimeOfDay::Day : TimeOfDay::Evening);
        boundedReplay.record(100.0 + static_cast<double>(i) * 0.11, replayAircraft, 1,
            replayWeather, frameMap, frameTime);
    }
    check(boundedReplay.frames().size() == 3, "replay recorder enforces its memory bound");
    check(near(boundedReplay.duration(), 0.22, 1.0e-6), "rolling replay duration is relative to retained frames");
    boundedReplay.beginPlayback();
    check(near(boundedReplay.playbackProgress(), 0.0), "replay playback starts at zero progress after buffer rollover");
    const ReplayFrame* firstReplayFrame = boundedReplay.currentFrame();
    check(firstReplayFrame != nullptr && near(firstReplayFrame->aircraft.position.x, 10.0),
        "replay begins at the oldest retained frame");
    check(firstReplayFrame != nullptr && firstReplayFrame->weather.preset == WeatherPreset::Clear,
        "replay initial frame carries its recorded weather");
    boundedReplay.updatePlayback(0.055);
    const ReplayFrame* middleReplayFrame = boundedReplay.currentFrame();
    check(middleReplayFrame != nullptr && near(middleReplayFrame->aircraft.position.x, 15.0),
        "replay interpolates aircraft position between snapshots");
    check(middleReplayFrame != nullptr && middleReplayFrame->weather.rainIntensity > 0.0
        && middleReplayFrame->weather.rainIntensity < 0.8,
        "replay smoothly interpolates weather intensity between captured conditions");
    check(middleReplayFrame != nullptr && near(middleReplayFrame->timeOfDay.hours, 16.0, 0.01),
        "replay smoothly interpolates recorded time of day");
    boundedReplay.updatePlayback(2.0);
    check(near(boundedReplay.playbackProgress(), 1.0), "replay clamps its playhead to the final frame");
    check(boundedReplay.isPlaying(), "replay keeps the final frame visible until stopped");
    check(boundedReplay.currentFrame() != nullptr && near(boundedReplay.currentFrame()->aircraft.position.x, 30.0),
        "final replay frame remains available at the end of playback");
    check(boundedReplay.currentFrame() != nullptr && boundedReplay.currentFrame()->mapConfig.terrain == MapPreset::Coast,
        "last replay frame retains its recorded map");
    boundedReplay.stopPlayback();
    check(!boundedReplay.isPlaying(), "bounded replay stops cleanly");

    const std::filesystem::path replayTestDirectory = std::filesystem::temp_directory_path() / "skyframe-replay-tests";
    std::error_code replayCleanupError;
    std::filesystem::remove_all(replayTestDirectory, replayCleanupError);
    ReplayArchive archive(replayTestDirectory);
    ReplayRecorder persistentReplay;
    Aircraft persistentAircraft;
    WeatherState persistentWeather;
    persistentWeather.setPreset(WeatherPreset::Snow);
    MapConfig persistentMap;
    persistentMap.terrain = MapPreset::Random;
    persistentMap.size = MapSize::Large;
    persistentMap.seed = 0xBADC0FFEU;
    persistentMap.airportClass = AirportClass::International;
    persistentMap.towersEnabled = true;
    persistentMap.powerLinesEnabled = false;
    persistentMap.terrainSceneryEnabled = false;
    TimeOfDayState persistentTime;
    persistentTime.setMode(TimeOfDay::Night);
    for (int i = 0; i < 5; ++i) {
        AircraftSnapshot frame = persistentAircraft.snapshot();
        frame.position.x = static_cast<double>(i * 12);
        frame.systems.engines[0].overheated = i >= 2;
        persistentAircraft.restore(frame);
        persistentReplay.record(static_cast<double>(i) * 0.11, persistentAircraft, 1,
            persistentWeather, persistentMap, persistentTime);
    }
    std::string archiveError;
    check(archive.save("Night flight test", persistentReplay, &archiveError),
        "replay archive saves a named replay to disk");
    auto savedEntries = archive.list();
    check(savedEntries.size() == 1 && savedEntries.front().name == "Night flight test"
        && savedEntries.front().frameCount == 5,
        "replay archive lists metadata for a saved replay");
    check(archive.save("Night flight test", persistentReplay, &archiveError),
        "replay archive permits duplicate names by choosing a unique filename");
    savedEntries = archive.list();
    check(savedEntries.size() == 2, "replay archive stores multiple replays with matching display names");
    if (!savedEntries.empty()) {
        const std::string selectedFile = savedEntries.front().fileName;
        check(archive.rename(selectedFile, "Renamed night scenario", &archiveError),
            "replay archive renames saved replay metadata without changing frames");
        ReplayRecorder loadedReplay;
        check(archive.load(selectedFile, loadedReplay, &archiveError),
            "replay archive loads a saved replay after a fresh recorder is created");
        check(loadedReplay.frames().size() == 5
            && loadedReplay.frames().back().mapConfig.terrain == MapPreset::Random
            && loadedReplay.frames().back().mapConfig.seed == persistentMap.seed
            && loadedReplay.frames().back().mapConfig.towersEnabled
            && !loadedReplay.frames().back().mapConfig.powerLinesEnabled
            && !loadedReplay.frames().back().mapConfig.terrainSceneryEnabled
            && loadedReplay.frames().back().weather.activePreset == WeatherPreset::Snow
            && loadedReplay.frames().back().timeOfDay.mode == TimeOfDay::Night
            && loadedReplay.frames().back().aircraft.systems.engines[0].overheated,
            "saved replay preserves aircraft, weather, terrain seed, independent map toggles and time-of-day context");
        const std::filesystem::path validReplayPath = replayTestDirectory / selectedFile;
        const std::string legacyFileName = "legacy_v1.sfr";
        const std::filesystem::path legacyReplayPath = replayTestDirectory / legacyFileName;
        check(makeLegacyReplayFile(validReplayPath, legacyReplayPath),
            "legacy replay compatibility fixture is generated from a current recording");
        ReplayRecorder legacyReplay;
        check(archive.load(legacyFileName, legacyReplay, &archiveError)
            && legacyReplay.frames().back().mapConfig.towersEnabled
            && legacyReplay.frames().back().mapConfig.powerLinesEnabled
            && legacyReplay.frames().back().mapConfig.terrainSceneryEnabled,
            "version-one replay files load with compatible combined-infrastructure defaults");
        check(archive.remove(legacyFileName, &archiveError), "legacy replay fixture can be removed after compatibility test");
        const std::filesystem::path overCapacityPath = replayTestDirectory / "over_capacity.sfr";
        check(makeOverCapacityReplay(validReplayPath, overCapacityPath),
            "replay archive invalid-capacity regression fixture is generated");
        check(!archive.load("over_capacity.sfr", loadedReplay, &archiveError),
            "replay archive rejects passenger counts above the selected aircraft capacity");
        check(archive.remove(selectedFile, &archiveError),
            "replay archive deletes a selected saved replay");
        check(!archive.load("../escape.sfr", loadedReplay, &archiveError),
            "replay archive rejects path traversal filenames");
    }
    {
        std::ofstream invalid(replayTestDirectory / "corrupt.sfr");
        invalid << "NOT_A_SKYFRAME_REPLAY\n";
        invalid.close();
        check(archive.list().size() == 1,
            "replay archive ignores corrupt files instead of exposing them to the UI");
    }
    std::filesystem::remove_all(replayTestDirectory, replayCleanupError);

    const std::filesystem::path capacityDirectory = std::filesystem::temp_directory_path() / "skyframe-replay-capacity-tests";
    std::filesystem::remove_all(capacityDirectory, replayCleanupError);
    ReplayArchive limitedArchive(capacityDirectory);
    bool capacityWritesSucceeded = true;
    for (int i = 0; i < 30; ++i) {
        if (!limitedArchive.save("Capacity flight " + std::to_string(i + 1), persistentReplay, &archiveError)) {
            capacityWritesSucceeded = false;
            break;
        }
    }
    check(capacityWritesSucceeded && limitedArchive.list().size() == 30,
        "replay archive permits the documented maximum of 30 recordings");
    check(!limitedArchive.save("One too many", persistentReplay, &archiveError)
        && limitedArchive.list().size() == 30,
        "replay archive rejects an additional save after reaching its storage bound");
    std::filesystem::remove_all(capacityDirectory, replayCleanupError);

    Simulation eventSimulation;
    for (int i = 0; i < 12; ++i) eventSimulation.setPassengerCount(i * 3);
    check(eventSimulation.events().size() <= 8, "session event history remains bounded");

    Simulation completedSimulation;
    completedSimulation.aircraft().reportLanding(false, "Test emergency evacuation");
    completedSimulation.update(1.0 / 60.0);
    check(completedSimulation.paused(), "completed flight automatically pauses the sandbox");
    check(completedSimulation.completedFlights() == 1, "completed flight increments the session counter once");
    check(completedSimulation.emergencyLandings() == 1, "emergency landing increments its session counter");
    completedSimulation.update(1.0 / 60.0);
    check(completedSimulation.completedFlights() == 1, "completed-flight counter does not increment repeatedly");

    // Fleet catalog: every aircraft is free/unlocked, valid, finite, uniquely named, and flyable headlessly.
    {
        bool valid = true;
        for (int i = 0; i < fleetCount(); ++i) {
            const FleetEntry& e = fleetEntry(i);
            const AircraftDesign& d = e.design;
            const float dims[] = {d.length, d.bodyWidth, d.bodyHeight, d.noseLength, d.tailLength, d.wingSpan, d.rootChord,
                d.tipChord, d.engineRadius, d.engineLength, d.finHeight, d.finChord, d.stabSpan, d.gearHeight};
            for (float v : dims) if (!(v > 0.0f) || !std::isfinite(v)) valid = false;
            if (d.noseLength + d.tailLength >= d.length) valid = false;
            if (d.engineCount < 1 || d.engineCount > 8) valid = false;
            if (e.config.passengerCapacity < 1 || e.config.crewCapacity < 1 || e.blurb.empty()) valid = false;
            for (int j = i + 1; j < fleetCount(); ++j) if (fleetEntry(j).config.name == e.config.name) valid = false;
        }
        check(valid, "every fleet entry has finite positive dimensions, sane engines and a unique name");
        check(fleetCountInCategory(FleetCategory::Small) >= 4 && fleetCountInCategory(FleetCategory::Medium) >= 4
            && fleetCountInCategory(FleetCategory::Large) >= 3 && fleetCountInCategory(FleetCategory::Military) >= 2,
            "each fleet category is populated");
        check(fleetIndexInCategory(FleetCategory::Military, 0) >= 0 && fleetIndexInCategory(FleetCategory::Military, 99) == -1,
            "category lookup handles in-range and out-of-range requests");
        bool allFlyable = true;
        for (int i = 0; i < fleetCount(); ++i) {
            Simulation sim;
            sim.selectAircraft(i);
            for (int f = 0; f < 600; ++f) sim.update(1.0 / 60.0);
            const FlightTelemetry& t = sim.aircraft().telemetry();
            if (!std::isfinite(t.speed) || !std::isfinite(t.altitude) || !std::isfinite(t.verticalSpeed)) allFlyable = false;
            if (sim.aircraft().flightEnded()) allFlyable = false;   // default unattended flight must not end in 10 s
        }
        check(allFlyable, "every aircraft stays finite and airborne for 10 s of unattended flight");
    }

    // Regression: raylib's default font is ASCII/Latin-1 only; non-Latin-1 glyphs (em dash, bullet)
    // rendered as '?' in the HUD. Keep user-visible core strings Latin-1 safe.
    {
        bool allAscii = true;
        const auto scan = [&allAscii](const std::string& text) {
            for (unsigned char c : text) if (c > 0x7E) allAscii = false;
        };
        for (const auto& event : completedSimulation.events()) scan(event.message);
        Simulation freshSimulation;
        for (const auto& event : freshSimulation.events()) scan(event.message);
        scan(freshSimulation.aircraft().report().aircraftName);
        scan(freshSimulation.aircraft().report().notes);
        check(allAscii, "user-visible event/report strings contain only ASCII (HUD font limitation)");
    }

    // --- Ordnance sandbox: arcade bombs vs static targets -----------------------------------
    {
        const auto indexOf = [](const char* name) {
            for (int i = 0; i < aircraftPresetCount(); ++i)
                if (aircraftPresetName(i) == name) return i;
            return -1;
        };
        const int raptor = indexOf("F-22 Raptor");
        const int bomber = indexOf("B-52 Stratofortress");
        const int hauler = indexOf("Meridian Hauler");
        check(raptor >= 0 && bomber >= 0, "F-22 Raptor and B-52 Stratofortress are in the fleet");
        check(fleetEntry(raptor).ordnanceTier == 1 && fleetEntry(bomber).ordnanceTier == 2, "ordnance tiers are F-22=1, B-52=2");
        check(fleetEntry(hauler).ordnanceTier == 0 && fleetEntry(1).ordnanceTier == 0, "non-bomber aircraft have no ordnance bay");
        check(ordnanceAllows(1, BombKind::Medium) && !ordnanceAllows(1, BombKind::Nuclear), "tier 1 carries light/medium only");
        check(ordnanceAllows(2, BombKind::Nuclear) && !ordnanceAllows(0, BombKind::Light), "tier 2 carries all, tier 0 none");

        // Same seed -> same range; different seed -> different dummy layout.
        MapConfig mapA; mapA.seed = 4242U;
        OrdnanceField a, b, c;
        a.generate(mapA); b.generate(mapA);
        MapConfig mapB = mapA; mapB.seed = 99U;
        c.generate(mapB);
        check(a.totalTargets() > 20 && a.totalTargets() == b.totalTargets(), "target range is generated");
        bool same = true, differs = false;
        for (int i = 0; i < a.totalTargets(); ++i) {
            const auto& ta = a.targets()[static_cast<std::size_t>(i)];
            const auto& tb = b.targets()[static_cast<std::size_t>(i)];
            const auto& tc = c.targets()[static_cast<std::size_t>(i)];
            if (ta.position.x != tb.position.x || ta.position.z != tb.position.z) same = false;
            if (ta.position.z != tc.position.z) differs = true;
        }
        check(same, "target range is deterministic for a seed");
        check(differs, "target range varies with the seed");

        // A heavy bomb dropped onto the range centre destroys nearby targets; a distant one does not.
        const Vec3 centre = a.rangeCenter();
        check(a.drop(BombKind::Heavy, {centre.x, centre.y + 300.0, centre.z}, {0.0, 0.0, 0.0}), "bomb can be released");
        for (int i = 0; i < 1200 && !a.bombs().empty(); ++i) a.update(1.0 / 120.0, mapA);
        check(a.bombs().empty() && !a.blasts().empty(), "bomb falls, lands and detonates");
        check(a.destroyedTargets() > 0 && a.destroyedTargets() < a.totalTargets(), "heavy bomb destroys some but not all targets");

        OrdnanceField far;
        far.generate(mapA);
        far.drop(BombKind::Light, {centre.x + 600.0, centre.y + 200.0, centre.z + 600.0}, {0.0, 0.0, 0.0});
        for (int i = 0; i < 1200 && !far.bombs().empty(); ++i) far.update(1.0 / 120.0, mapA);
        check(far.destroyedTargets() == 0, "a distant light bomb destroys nothing");

        OrdnanceField nuke;
        nuke.generate(mapA);
        nuke.drop(BombKind::Nuclear, {centre.x, centre.y + 400.0, centre.z}, {0.0, 0.0, 0.0});
        for (int i = 0; i < 1500 && !nuke.bombs().empty(); ++i) nuke.update(1.0 / 120.0, mapA);
        check(nuke.destroyedTargets() == nuke.totalTargets(), "nuclear arcade blast clears the whole range");
        for (int i = 0; i < 120 * 25; ++i) nuke.update(1.0 / 120.0, mapA);
        check(nuke.blasts().empty(), "blast visuals expire");

        // Cluster bombs split into bomblets above the ground.
        OrdnanceField cluster;
        cluster.generate(mapA);
        cluster.drop(BombKind::Cluster, {centre.x, centre.y + 500.0, centre.z}, {0.0, 0.0, 0.0});
        std::size_t peak = 0;
        for (int i = 0; i < 2400 && (i < 10 || !cluster.bombs().empty()); ++i) {
            cluster.update(1.0 / 120.0, mapA);
            peak = std::max(peak, cluster.bombs().size());
        }
        check(peak >= 10U, "cluster bomb splits into bomblets");

        // Bounded storage.
        OrdnanceField flood;
        flood.generate(mapA);
        int accepted = 0;
        for (int i = 0; i < 1000; ++i) if (flood.drop(BombKind::Light, {0.0, 5000.0, 0.0}, {})) ++accepted;
        check(accepted == static_cast<int>(OrdnanceField::kMaxBombs), "bomb count is bounded");

        // Simulation gating by aircraft.
        Simulation sim;
        sim.selectAircraft(hauler);
        check(!sim.dropBomb(), "transport without a bay cannot drop bombs");
        sim.selectAircraft(raptor);
        sim.selectBomb(BombKind::Nuclear);
        check(!sim.dropBomb(), "F-22 cannot carry the nuclear bomb");
        sim.selectBomb(BombKind::Medium);
        check(sim.dropBomb() && sim.ordnance().bombsDropped() == 1, "F-22 drops a medium bomb");
        check(!sim.dropBomb(), "release cooldown prevents instant re-drop");
        sim.selectAircraft(bomber);
        sim.selectBomb(BombKind::Nuclear);
        check(sim.dropBomb(), "B-52 drops the nuclear bomb");
        sim.reset();
        check(sim.ordnance().bombsDropped() == 0 && sim.ordnance().destroyedTargets() == 0, "reset rebuilds the target range");
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All Skyframe simulation tests passed.\n";
    return 0;
}
