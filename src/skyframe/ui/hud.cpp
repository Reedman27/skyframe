#include "skyframe/ui/hud.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

namespace skyframe {
namespace {
constexpr Color kPanel = {8, 11, 15, 238};
constexpr Color kPanelInner = {16, 22, 29, 255};
constexpr Color kBorder = {93, 104, 115, 255};
constexpr Color kCyan = {40, 198, 231, 255};
constexpr Color kBlue = {0, 128, 202, 255};
constexpr Color kRed = {170, 43, 53, 255};
constexpr Color kText = {225, 231, 238, 255};
constexpr Color kMuted = {137, 151, 166, 255};
bool gUiInputLocked = false;

bool button(Rectangle rect, const std::string& label, bool active = false, bool danger = false) {
    const bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
    const Color fill = danger ? (hover ? Color{208, 58, 66, 255} : kRed)
        : active ? Color{0, 111, 180, 255}
        : hover ? Color{67, 79, 91, 255} : Color{48, 55, 63, 255};
    DrawRectangleRounded(rect, 0.16f, 6, fill);
    DrawRectangleLinesEx(rect, 1.0f, hover ? kCyan : kBorder);
    const int fontSize = 14;
    const int textWidth = MeasureText(label.c_str(), fontSize);
    DrawText(label.c_str(), static_cast<int>(rect.x + (rect.width - static_cast<float>(textWidth)) * 0.5f),
        static_cast<int>(rect.y + (rect.height - fontSize) * 0.5f), fontSize, kText);
    return !gUiInputLocked && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Truncate text with "..." so it never spills past maxWidth pixels.
std::string fitText(const std::string& text, int fontSize, float maxWidth) {
    if (static_cast<float>(MeasureText(text.c_str(), fontSize)) <= maxWidth) return text;
    std::string out = text;
    while (!out.empty() && static_cast<float>(MeasureText((out + "...").c_str(), fontSize)) > maxWidth) out.pop_back();
    return out + "...";
}

void panel(Rectangle rect, const char* title, const char* subtitle = nullptr) {
    DrawRectangleRounded(rect, 0.035f, 8, kPanel);
    DrawRectangleLinesEx(rect, 1.5f, kBorder);
    Rectangle inner{rect.x + 5.0f, rect.y + 5.0f, rect.width - 10.0f, rect.height - 10.0f};
    DrawRectangleLinesEx(inner, 1.0f, {42, 52, 62, 255});
    DrawText(title, static_cast<int>(rect.x + 17.0f), static_cast<int>(rect.y + 14.0f), 17, kText);
    if (subtitle) DrawText(fitText(subtitle, 10, rect.width - 34.0f).c_str(), static_cast<int>(rect.x + 17.0f), static_cast<int>(rect.y + 37.0f), 10, kMuted);
}

void labelValue(int x, int y, const char* label, const std::string& value, Color valueColor = kText) {
    DrawText(label, x, y, 12, kMuted);
    const int width = MeasureText(value.c_str(), 14);
    DrawText(value.c_str(), x + 286 - width, y - 1, 14, valueColor);
}

std::string fixed(double value, int decimals = 0) {
    char buffer[48]{};
    const std::string format = "%0." + std::to_string(decimals) + "f";
    std::snprintf(buffer, sizeof(buffer), format.c_str(), value);
    return buffer;
}

std::string statusLabel(const Aircraft& aircraft) {
    return std::string(aircraft.status());
}

std::string controlsLabel(bool gearDown) {
    return gearDown ? "GEAR DOWN" : "GEAR UP";
}

} // namespace

void Hud::refreshSavedReplays() {
    savedReplays_ = replayArchive_.list();
    const std::size_t pageCount = std::max<std::size_t>(1U, (savedReplays_.size() + 2U) / 3U);
    if (replayPage_ >= pageCount) replayPage_ = pageCount - 1U;
    if (savedReplays_.empty()) selectedReplay_ = 0U;
    else if (selectedReplay_ >= savedReplays_.size()) selectedReplay_ = savedReplays_.size() - 1U;
    replayArchiveLoaded_ = true;
}

void Hud::draw(Simulation& simulation, bool& cockpitView, const ReplayFrame* replayFrame) {
    if (!replayArchiveLoaded_) refreshSavedReplays();
    if (renameEditing_) {
        int character = GetCharPressed();
        while (character > 0) {
            if (character >= 32 && character <= 126 && renameBuffer_.size() < 48U) {
                renameBuffer_.push_back(static_cast<char>(character));
            }
            character = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !renameBuffer_.empty()) renameBuffer_.pop_back();
        if (IsKeyPressed(KEY_ENTER)) {
            std::string error;
            if (replayArchive_.rename(renameFileName_, renameBuffer_, &error)) {
                replayMessage_ = "Replay renamed.";
                renameError_.clear();
                renameEditing_ = false;
                refreshSavedReplays();
            } else {
                renameError_ = error;
            }
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            renameEditing_ = false;
            renameError_.clear();
        }
    }
    gUiInputLocked = renameEditing_;
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    const Aircraft& aircraft = simulation.aircraft();
    const bool replaying = replayFrame != nullptr;
    const AircraftConfig& displayedConfig = replayFrame
        ? aircraftPresetConfig(replayFrame->aircraftPresetIndex) : aircraft.config();
    const FlightTelemetry& telemetry = replayFrame ? replayFrame->aircraft.telemetry : aircraft.telemetry();
    const AircraftSystems& systems = replayFrame ? replayFrame->aircraft.systems : aircraft.systems();
    const FlightControls& displayedControls = replayFrame ? replayFrame->aircraft.controls : aircraft.controls();
    const int displayedPassengers = replayFrame ? replayFrame->aircraft.passengers : aircraft.passengers();
    const WeatherState& displayedWeather = replayFrame ? replayFrame->weather : simulation.weather();
    const MapConfig& displayedMapConfig = replayFrame ? replayFrame->mapConfig : simulation.mapConfig();
    const TimeOfDayState& displayedTimeOfDay = replayFrame ? replayFrame->timeOfDay : simulation.timeOfDay();

    panel({12.0f, 14.0f, 320.0f, 76.0f}, "SKYFRAME  /  FLIGHT SANDBOX", "FLY  |  RUN  |  MANAGE SYSTEMS  |  REPLAY");

    const std::array<const char*, 7> navLabels{"Failures", "Fleet", "Weather", "Maps", "Replay", "Ordnance", "Settings"};
    const std::array<Page, 7> navPages{Page::Failures, Page::Fleet, Page::Weather, Page::Maps, Page::Replay, Page::Ordnance, Page::Settings};
    panel({12.0f, 102.0f, 82.0f, 430.0f}, "MENU");
    for (std::size_t i = 0; i < navLabels.size(); ++i) {
        const float y = 142.0f + static_cast<float>(i) * 54.0f;
        if (button({20.0f, y, 66.0f, 48.0f}, navLabels[i], page_ == navPages[i])) page_ = navPages[i];
    }

    const Rectangle pageRect{104.0f, 102.0f, 392.0f, 430.0f};
    switch (page_) {
        case Page::Failures: {
            panel(pageRect, "FAILURES", "Arcade gameplay faults - not real-world modelling.");
            const std::array<std::pair<const char*, FailureType>, 10> entries{{
                {"Left engine power loss", FailureType::LeftEnginePowerLoss},
                {"Right engine power loss", FailureType::RightEnginePowerLoss},
                {"Left engine overheat", FailureType::LeftEngineOverheat},
                {"Right engine overheat", FailureType::RightEngineOverheat},
                {"Left wing panel", FailureType::LeftWingPanelDamage},
                {"Right wing panel", FailureType::RightWingPanelDamage},
                {"Flight control jam", FailureType::FlightControlJam},
                {"Hydraulic fault", FailureType::HydraulicFault},
                {"Electrical fault", FailureType::ElectricalFault},
                {"Landing gear fault", FailureType::LandingGearFault}
            }};
            for (std::size_t i = 0; i < entries.size(); ++i) {
                const int col = static_cast<int>(i % 2);
                const int row = static_cast<int>(i / 2);
                Rectangle r{pageRect.x + 16.0f + static_cast<float>(col) * 184.0f,
                    pageRect.y + 74.0f + static_cast<float>(row) * 57.0f, 174.0f, 45.0f};
                if (!replaying && button(r, entries[i].first, false, false)) simulation.applyFailure(entries[i].second);
                else if (replaying) button(r, entries[i].first, false, false);
            }
            const Rectangle randomRect{pageRect.x + 16.0f, pageRect.y + 365.0f, 358.0f, 43.0f};
            if (!replaying && button(randomRect, "Random system fault", false, true)) {
                simulation.applyFailure(FailureType::Random);
            } else if (replaying) button(randomRect, "Random system fault", false, true);
            break;
        }
        case Page::Fleet: {
            panel(pageRect, "SPAWN AN AIRCRAFT", "All aircraft are free and unlocked. Fictional designs.");
            const std::array<FleetCategory, 4> tabs{FleetCategory::Small, FleetCategory::Medium, FleetCategory::Large, FleetCategory::Military};
            for (std::size_t t = 0; t < tabs.size(); ++t) {
                const Rectangle tr{pageRect.x + 16.0f + static_cast<float>(t) * 90.0f, pageRect.y + 62.0f, 84.0f, 30.0f};
                const std::string label = std::string(fleetCategoryName(tabs[t]));
                if (button(tr, label, fleetTab_ == tabs[t])) fleetTab_ = tabs[t];
            }
            const int inTab = fleetCountInCategory(fleetTab_);
            for (int n = 0; n < inTab; ++n) {
                const int index = fleetIndexInCategory(fleetTab_, n);
                if (index < 0) break;
                const FleetEntry& entry = fleetEntry(index);
                const Rectangle r{pageRect.x + 16.0f + static_cast<float>(n % 2) * 183.0f,
                    pageRect.y + 104.0f + static_cast<float>(n / 2) * 76.0f, 175.0f, 68.0f};
                const bool selected = displayedConfig.name == entry.config.name;
                const bool clicked = button(r, "", selected);
                if (clicked && !replaying) simulation.selectAircraft(index);
                DrawText(fitText(entry.config.name, 13, r.width - 22.0f).c_str(), static_cast<int>(r.x + 12.0f), static_cast<int>(r.y + 9.0f), 13, kText);
                DrawText(fitText(std::string(entry.blurb), 9, r.width - 22.0f).c_str(), static_cast<int>(r.x + 12.0f), static_cast<int>(r.y + 29.0f), 9, selected ? kText : kCyan);
                DrawText(TextFormat("%d passengers  -  %.0f m long", entry.config.passengerCapacity, static_cast<double>(entry.design.length)),
                    static_cast<int>(r.x + 12.0f), static_cast<int>(r.y + 46.0f), 9, kMuted);
            }
            break;
        }
        case Page::Weather: {
            panel(pageRect, "WEATHER + TIME", "Choose weather conditions and the local lighting cycle.");
            const std::array<std::pair<const char*, WeatherPreset>, 8> choices{{
                {"Auto", WeatherPreset::Auto}, {"Clear", WeatherPreset::Clear},
                {"Cloudy", WeatherPreset::Cloudy}, {"Rain", WeatherPreset::Rain},
                {"Storm", WeatherPreset::Storm}, {"Fog", WeatherPreset::Fog},
                {"Snow", WeatherPreset::Snow}, {"Windy", WeatherPreset::Windy}
            }};
            for (std::size_t i = 0; i < choices.size(); ++i) {
                const int col = static_cast<int>(i % 2U);
                const int row = static_cast<int>(i / 2U);
                const Rectangle r{pageRect.x + 16.0f + static_cast<float>(col) * 184.0f,
                    pageRect.y + 64.0f + static_cast<float>(row) * 49.0f, 174.0f, 42.0f};
                if (!replaying && button(r, choices[i].first, displayedWeather.preset == choices[i].second)) {
                    simulation.setWeather(choices[i].second);
                } else if (replaying) button(r, choices[i].first, displayedWeather.preset == choices[i].second);
            }
            DrawText(TextFormat("ACTIVE: %.*s   WIND: %.1f m/s   VIS: %.1f km",
                static_cast<int>(displayedWeather.activeName().size()), displayedWeather.activeName().data(),
                displayedWeather.wind.length(), displayedWeather.visibilityKm),
                static_cast<int>(pageRect.x + 18.0f), static_cast<int>(pageRect.y + 265.0f), 10, kCyan);
            DrawText("TIME OF DAY", static_cast<int>(pageRect.x + 18.0f), static_cast<int>(pageRect.y + 290.0f), 10, kMuted);
            const std::array<TimeOfDay, 5> times{TimeOfDay::Morning, TimeOfDay::Day,
                TimeOfDay::Evening, TimeOfDay::Night, TimeOfDay::Cycle};
            for (std::size_t i = 0; i < times.size(); ++i) {
                const int col = static_cast<int>(i % 2U);
                const int row = static_cast<int>(i / 2U);
                const float width = i == 4U ? 358.0f : 174.0f;
                const float x = i == 4U ? pageRect.x + 16.0f : pageRect.x + 16.0f + static_cast<float>(col) * 184.0f;
                const Rectangle r{x, pageRect.y + 308.0f + static_cast<float>(row) * 37.0f, width, 31.0f};
                if (!replaying && button(r, std::string(timeOfDayName(times[i])), displayedTimeOfDay.mode == times[i])) {
                    simulation.setTimeOfDay(times[i]);
                } else if (replaying) button(r, std::string(timeOfDayName(times[i])), displayedTimeOfDay.mode == times[i]);
            }
            break;
        }
        case Page::Maps: {
            panel(pageRect, "WORLD GENERATOR", "Randomly generate a world, then choose which features appear.");
            const std::array<MapPreset, 11> terrains{MapPreset::Random, MapPreset::TrainingField,
                MapPreset::Coast, MapPreset::Highlands, MapPreset::Desert, MapPreset::Hills,
                MapPreset::Plains, MapPreset::City, MapPreset::Ocean, MapPreset::Tundra, MapPreset::Mountains};
            for (std::size_t i = 0; i < terrains.size(); ++i) {
                const int col = static_cast<int>(i % 3U);
                const int row = static_cast<int>(i / 3U);
                const Rectangle r{pageRect.x + 16.0f + static_cast<float>(col) * 121.0f,
                    pageRect.y + 62.0f + static_cast<float>(row) * 32.0f, 114.0f, 27.0f};
                const std::string label(mapName(terrains[i]));
                if (!replaying && button(r, label, displayedMapConfig.terrain == terrains[i])) {
                    simulation.setMap(terrains[i]);
                } else if (replaying) {
                    button(r, label, displayedMapConfig.terrain == terrains[i]);
                }
            }
            const MapPreset generatedTerrain = resolveMapTerrain(displayedMapConfig);
            DrawText(TextFormat("GENERATED WORLD: %s", std::string(mapName(generatedTerrain)).c_str()),
                static_cast<int>(pageRect.x + 18.0f), static_cast<int>(pageRect.y + 192.0f), 10, kCyan);
            DrawText("MAP SIZE", static_cast<int>(pageRect.x + 18.0f), static_cast<int>(pageRect.y + 208.0f), 9, kMuted);
            const std::array<MapSize, 3> sizes{MapSize::Small, MapSize::Medium, MapSize::Large};
            for (std::size_t i = 0; i < sizes.size(); ++i) {
                const Rectangle r{pageRect.x + 16.0f + static_cast<float>(i) * 122.0f,
                    pageRect.y + 220.0f, 114.0f, 25.0f};
                if (!replaying && button(r, std::string(mapSizeName(sizes[i])), displayedMapConfig.size == sizes[i])) {
                    simulation.setMapSize(sizes[i]);
                } else if (replaying) {
                    button(r, std::string(mapSizeName(sizes[i])), displayedMapConfig.size == sizes[i]);
                }
            }
            if (!replaying && button({pageRect.x + 16.0f, pageRect.y + 252.0f, 174.0f, 27.0f},
                displayedMapConfig.airportEnabled ? "AIRPORT: ON" : "AIRPORT: OFF", displayedMapConfig.airportEnabled)) {
                simulation.setAirportEnabled(!displayedMapConfig.airportEnabled);
            } else if (replaying) {
                button({pageRect.x + 16.0f, pageRect.y + 252.0f, 174.0f, 27.0f},
                    displayedMapConfig.airportEnabled ? "AIRPORT: ON" : "AIRPORT: OFF", displayedMapConfig.airportEnabled);
            }
            if (!replaying && button({pageRect.x + 198.0f, pageRect.y + 252.0f, 176.0f, 27.0f},
                std::string(airportClassName(displayedMapConfig.airportClass)))) {
                const auto next = static_cast<unsigned int>(displayedMapConfig.airportClass);
                simulation.setAirportClass(static_cast<AirportClass>((next + 1U) % 6U));
            } else if (replaying) {
                button({pageRect.x + 198.0f, pageRect.y + 252.0f, 176.0f, 27.0f},
                    std::string(airportClassName(displayedMapConfig.airportClass)));
            }
            DrawText(TextFormat("SEED: %u", displayedMapConfig.seed), static_cast<int>(pageRect.x + 18.0f),
                static_cast<int>(pageRect.y + 292.0f), 11, kCyan);
            if (!replaying && button({pageRect.x + 218.0f, pageRect.y + 284.0f, 156.0f, 27.0f}, "NEW RANDOM SEED")) {
                simulation.randomizeMapSeed();
            } else if (replaying) {
                button({pageRect.x + 218.0f, pageRect.y + 284.0f, 156.0f, 27.0f}, "NEW RANDOM SEED");
            }
            const std::array<std::pair<const char*, bool>, 2> infrastructure{{
                {displayedMapConfig.towersEnabled ? "TOWERS: ON" : "TOWERS: OFF", displayedMapConfig.towersEnabled},
                {displayedMapConfig.powerLinesEnabled ? "POWER LINES: ON" : "POWER LINES: OFF", displayedMapConfig.powerLinesEnabled}
            }};
            for (std::size_t i = 0; i < infrastructure.size(); ++i) {
                const Rectangle r{pageRect.x + 16.0f + static_cast<float>(i) * 184.0f,
                    pageRect.y + 316.0f, 174.0f, 27.0f};
                if (!replaying && button(r, infrastructure[i].first, infrastructure[i].second)) {
                    if (i == 0U) simulation.setTowersEnabled(!displayedMapConfig.towersEnabled);
                    else simulation.setPowerLinesEnabled(!displayedMapConfig.powerLinesEnabled);
                } else if (replaying) {
                    button(r, infrastructure[i].first, infrastructure[i].second);
                }
            }
            if (!replaying && button({pageRect.x + 16.0f, pageRect.y + 348.0f, 358.0f, 27.0f},
                displayedMapConfig.terrainSceneryEnabled ? "NATURAL SCENERY: ON" : "NATURAL SCENERY: OFF",
                displayedMapConfig.terrainSceneryEnabled)) {
                simulation.setTerrainSceneryEnabled(!displayedMapConfig.terrainSceneryEnabled);
            } else if (replaying) {
                button({pageRect.x + 16.0f, pageRect.y + 348.0f, 358.0f, 27.0f},
                    displayedMapConfig.terrainSceneryEnabled ? "NATURAL SCENERY: ON" : "NATURAL SCENERY: OFF",
                    displayedMapConfig.terrainSceneryEnabled);
            }
            break;
        }
        case Page::Replay: {
            panel(pageRect, "REPLAY ARCHIVE", "Recordings are saved locally and remain after the game closes.");
            if (!replaying && button({pageRect.x + 16.0f, pageRect.y + 62.0f, 358.0f, 36.0f}, "SAVE CURRENT REPLAY", false)) {
                const std::string name = "Flight " + std::to_string(savedReplays_.size() + 1U);
                std::string error;
                if (replayArchive_.save(name, simulation.replay(), &error)) {
                    replayMessage_ = "Saved " + name + ".";
                    refreshSavedReplays();
                } else {
                    replayMessage_ = error;
                }
            }
            if (button({pageRect.x + 16.0f, pageRect.y + 105.0f, 174.0f, 32.0f}, "PLAY BUFFER", simulation.replay().isPlaying())) {
                simulation.startReplay();
            }
            if (button({pageRect.x + 200.0f, pageRect.y + 105.0f, 174.0f, 32.0f}, "STOP REPLAY")) simulation.stopReplay();
            if (button({pageRect.x + 16.0f, pageRect.y + 143.0f, 358.0f, 30.0f}, "RESET FLIGHT", false, true)) {
                simulation.stopReplay();
                simulation.reset();
            }
            DrawText(TextFormat("ROLLING BUFFER  %.1f SEC", simulation.replay().duration()),
                static_cast<int>(pageRect.x + 18.0f), static_cast<int>(pageRect.y + 183.0f), 10, kCyan);
            DrawText("SAVED REPLAYS", static_cast<int>(pageRect.x + 18.0f), static_cast<int>(pageRect.y + 203.0f), 10, kMuted);
            const std::size_t pageCount = std::max<std::size_t>(1U, (savedReplays_.size() + 2U) / 3U);
            if (button({pageRect.x + 270.0f, pageRect.y + 192.0f, 34.0f, 22.0f}, "<")) {
                if (replayPage_ > 0U) --replayPage_;
            }
            DrawText(TextFormat("%d / %d", static_cast<int>(replayPage_ + 1U), static_cast<int>(pageCount)),
                static_cast<int>(pageRect.x + 309.0f), static_cast<int>(pageRect.y + 198.0f), 9, kMuted);
            if (button({pageRect.x + 354.0f, pageRect.y + 192.0f, 24.0f, 22.0f}, ">")) {
                if (replayPage_ + 1U < pageCount) ++replayPage_;
            }
            if (savedReplays_.empty()) {
                DrawText("No saved recordings yet.", static_cast<int>(pageRect.x + 18.0f),
                    static_cast<int>(pageRect.y + 233.0f), 11, kMuted);
            }
            const std::size_t firstReplay = replayPage_ * 3U;
            const std::size_t visibleCount = firstReplay < savedReplays_.size()
                ? std::min<std::size_t>(3U, savedReplays_.size() - firstReplay) : 0U;
            for (std::size_t row = 0; row < visibleCount; ++row) {
                const std::size_t i = firstReplay + row;
                const SavedReplayInfo& entry = savedReplays_[i];
                const float y = pageRect.y + 215.0f + static_cast<float>(row) * 55.0f;
                const Rectangle nameRect{pageRect.x + 16.0f, y, 150.0f, 42.0f};
                if (button(nameRect, fitText(entry.name, 10, 138.0f), selectedReplay_ == i)) selectedReplay_ = i;
                if (button({pageRect.x + 171.0f, y, 60.0f, 42.0f}, "LOAD")) {
                    std::string error;
                    if (replayArchive_.load(entry.fileName, simulation.replay(), &error)) {
                        selectedReplay_ = i;
                        simulation.startReplay();
                        replayMessage_ = "Loaded " + entry.name + ".";
                    } else replayMessage_ = error;
                }
                if (button({pageRect.x + 236.0f, y, 64.0f, 42.0f}, "RENAME")) {
                    selectedReplay_ = i;
                    renameEditing_ = true;
                    renameBuffer_ = entry.name;
                    renameFileName_ = entry.fileName;
                    renameError_.clear();
                }
                if (button({pageRect.x + 305.0f, y, 69.0f, 42.0f}, "DELETE", false, true)) {
                    std::string error;
                    if (replayArchive_.remove(entry.fileName, &error)) {
                        replayMessage_ = "Deleted " + entry.name + ".";
                        refreshSavedReplays();
                    } else replayMessage_ = error;
                }
            }
            if (!replayMessage_.empty()) {
                DrawText(fitText(replayMessage_, 9, pageRect.width - 32.0f).c_str(),
                    static_cast<int>(pageRect.x + 18.0f), static_cast<int>(pageRect.y + 390.0f), 9, kCyan);
            }
            break;
        }
        case Page::Ordnance: {
            panel(pageRect, "ORDNANCE", "Arcade sandbox bombs - static targets and crash-test dummies only.");
            const int tier = simulation.ordnanceTier();
            if (tier == 0) {
                DrawText("This aircraft has no ordnance bay.", static_cast<int>(pageRect.x + 20.0f), static_cast<int>(pageRect.y + 90.0f), 14, kText);
                DrawText("Pick the F-22 Raptor or B-52 in Fleet > Military.", static_cast<int>(pageRect.x + 20.0f), static_cast<int>(pageRect.y + 116.0f), 11, kMuted);
            } else {
                for (int i = 0; i < kBombKindCount; ++i) {
                    const BombKind kind = static_cast<BombKind>(i);
                    const Rectangle r{pageRect.x + 16.0f + static_cast<float>(i % 2) * 184.0f,
                        pageRect.y + 76.0f + static_cast<float>(i / 2) * 52.0f, 174.0f, 44.0f};
                    const bool allowed = ordnanceAllows(tier, kind);
                    const bool clicked = button(r, std::string(bombSpec(kind).name), allowed && simulation.selectedBomb() == kind);
                    if (!allowed) DrawRectangleRounded(r, 0.12f, 6, {10, 14, 20, 170});
                    if (clicked && allowed && !replaying) simulation.selectBomb(kind);
                }
                DrawText(fitText(std::string(bombSpec(simulation.selectedBomb()).blurb), 10, 350.0f).c_str(),
                    static_cast<int>(pageRect.x + 20.0f), static_cast<int>(pageRect.y + 236.0f), 10, kCyan);
                if (button({pageRect.x + 16.0f, pageRect.y + 256.0f, 358.0f, 50.0f}, "DROP BOMB  (X)", false) && !replaying) {
                    simulation.dropBomb();
                }
                DrawText(TextFormat("TARGETS DESTROYED   %d / %d", simulation.ordnance().destroyedTargets(), simulation.ordnance().totalTargets()),
                    static_cast<int>(pageRect.x + 20.0f), static_cast<int>(pageRect.y + 322.0f), 12, kText);
                DrawText(TextFormat("BOMBS DROPPED       %d", simulation.ordnance().bombsDropped()),
                    static_cast<int>(pageRect.x + 20.0f), static_cast<int>(pageRect.y + 342.0f), 12, kText);
                if (button({pageRect.x + 16.0f, pageRect.y + 366.0f, 358.0f, 38.0f}, "REBUILD TARGET RANGE", false) && !replaying) {
                    simulation.resetRange();
                }
            }
            break;
        }
        case Page::Settings: {
            panel(pageRect, "SETTINGS", "Graphics defaults are tuned for low-memory integrated GPUs.");
            if (button({pageRect.x + 18.0f, pageRect.y + 82.0f, 356.0f, 52.0f},
                cockpitView ? "Switch to chase camera" : "Switch to cockpit camera", false)) cockpitView = !cockpitView;
            DrawText("FRAME LIMIT      60 FPS", static_cast<int>(pageRect.x + 20.0f), static_cast<int>(pageRect.y + 171.0f), 13, kText);
            DrawText("SHADOWS          OFF", static_cast<int>(pageRect.x + 20.0f), static_cast<int>(pageRect.y + 203.0f), 13, kText);
            DrawText("WORLD DETAIL     LOW / PROCEDURAL", static_cast<int>(pageRect.x + 20.0f), static_cast<int>(pageRect.y + 235.0f), 13, kText);
            DrawText("RENDERER         RAYLIB / OPENGL", static_cast<int>(pageRect.x + 20.0f), static_cast<int>(pageRect.y + 267.0f), 13, kText);
            break;
        }
    }

    const Rectangle planeRect{static_cast<float>(screenW - 340), 14.0f, 328.0f, 516.0f};
    panel(planeRect, "SELECTED AIRCRAFT", nullptr);
    DrawText(displayedConfig.name.c_str(), static_cast<int>(planeRect.x + 18.0f), static_cast<int>(planeRect.y + 43.0f), 18, kText);
    const Color statusColor = replaying ? kCyan : aircraft.flightEnded() ? (aircraft.report().safeLanding ? GREEN : ORANGE)
        : telemetry.pullUpWarning || telemetry.stallWarning || telemetry.sinkRateWarning ? RED : kCyan;
    const std::string status = replaying ? "REPLAY PLAYBACK" : statusLabel(aircraft);
    DrawText(status.c_str(), static_cast<int>(planeRect.x + 18.0f), static_cast<int>(planeRect.y + 69.0f), 13, statusColor);

    DrawRectangleRounded({planeRect.x + 16.0f, planeRect.y + 100.0f, 296.0f, 35.0f}, 0.12f, 6, kPanelInner);
    DrawText("PASSENGERS", static_cast<int>(planeRect.x + 28.0f), static_cast<int>(planeRect.y + 111.0f), 11, kCyan);
    DrawText(TextFormat("%d / %d", displayedPassengers, displayedConfig.passengerCapacity),
        static_cast<int>(planeRect.x + 205.0f), static_cast<int>(planeRect.y + 109.0f), 14, kText);

    labelValue(static_cast<int>(planeRect.x + 18.0f), static_cast<int>(planeRect.y + 153.0f), "SPEED", fixed(telemetry.speed, 0) + " u/s");
    labelValue(static_cast<int>(planeRect.x + 18.0f), static_cast<int>(planeRect.y + 178.0f), "ALTITUDE", fixed(telemetry.altitude, 0) + " u");
    labelValue(static_cast<int>(planeRect.x + 18.0f), static_cast<int>(planeRect.y + 203.0f), "VERTICAL SPEED", fixed(telemetry.verticalSpeed, 1) + " u/s");
    labelValue(static_cast<int>(planeRect.x + 18.0f), static_cast<int>(planeRect.y + 228.0f), "HEADING", fixed(telemetry.headingDegrees, 0) + "°");

    DrawText("ENGINE STATUS", static_cast<int>(planeRect.x + 18.0f), static_cast<int>(planeRect.y + 267.0f), 10, kCyan);
    for (int i = 0; i < 2; ++i) {
        const EngineState& engine = systems.engines[static_cast<std::size_t>(i)];
        const float y = planeRect.y + 288.0f + static_cast<float>(i) * 25.0f;
        DrawRectangleRounded({planeRect.x + 18.0f, y, 292.0f, 18.0f}, 0.2f, 4, kPanelInner);
        DrawRectangleRounded({planeRect.x + 20.0f, y + 2.0f, 288.0f * static_cast<float>(engine.power), 14.0f},
            0.2f, 4, engine.overheated ? ORANGE : engine.running ? kBlue : kRed);
        DrawText(i == 0 ? "LEFT" : "RIGHT", static_cast<int>(planeRect.x + 26.0f), static_cast<int>(y + 4.0f), 9, WHITE);
        DrawText(engine.running ? (engine.overheated ? "HOT" : "ONLINE") : "OFFLINE",
            static_cast<int>(planeRect.x + 232.0f), static_cast<int>(y + 4.0f), 9, WHITE);
    }

    const float buttonY = planeRect.y + 350.0f;
    if (button({planeRect.x + 16.0f, buttonY, 92.0f, 36.0f}, controlsLabel(displayedControls.gearDown)) && !replaying) {
        simulation.aircraft().controls().gearDown = !simulation.aircraft().controls().gearDown;
    }
    const std::string flapsLabel = "FLAPS " + fixed(displayedControls.flaps * 100.0, 0) + "%";
    if (button({planeRect.x + 116.0f, buttonY, 92.0f, 36.0f}, flapsLabel) && !replaying) {
        simulation.aircraft().controls().flaps = nextFlapSetting(simulation.aircraft().controls().flaps);
    }
    if (button({planeRect.x + 216.0f, buttonY, 96.0f, 36.0f}, displayedControls.spoilers ? "SPOILERS ON" : "SPOILERS") && !replaying) {
        simulation.aircraft().controls().spoilers = !simulation.aircraft().controls().spoilers;
    }

    if (button({planeRect.x + 16.0f, planeRect.y + 397.0f, 140.0f, 34.0f}, "LOAD +10") && !replaying) {
        simulation.setPassengerCount(aircraft.passengers() + 10);
    }
    if (button({planeRect.x + 168.0f, planeRect.y + 397.0f, 144.0f, 34.0f}, "UNLOAD -10") && !replaying) {
        simulation.setPassengerCount(aircraft.passengers() - 10);
    }
    if (button({planeRect.x + 16.0f, planeRect.y + 441.0f, 296.0f, 39.0f}, cockpitView ? "CHASE VIEW" : "COCKPIT VIEW")) cockpitView = !cockpitView;

    const Rectangle controlsRect{12.0f, static_cast<float>(screenH - 132), 484.0f, 118.0f};
    panel(controlsRect, "FLIGHT CONTROLS", "W / S PITCH     A / D BANK     Q / E YAW     + / - THROTTLE");
    DrawText("G GEAR   F FLAPS   V SPOILERS   B BRAKES   SPACE PAUSE   R RESET   C CAMERA",
        static_cast<int>(controlsRect.x + 17.0f), static_cast<int>(controlsRect.y + 67.0f), 10, kText);
    const Rectangle pauseRect{controlsRect.x + 350.0f, controlsRect.y + 84.0f, 116.0f, 28.0f};
    if (!replaying && button(pauseRect, simulation.paused() ? "RUN" : "PAUSE", !simulation.paused())) {
        simulation.setPaused(!simulation.paused());
    } else if (replaying) {
        button(pauseRect, "REPLAY", true);
    }
    const std::string simulationState = replaying ? "REPLAY PLAYBACK"
        : simulation.paused() ? "SIMULATION PAUSED" : "SIMULATION RUNNING";
    DrawText(simulationState.c_str(), static_cast<int>(controlsRect.x + 17.0f),
        static_cast<int>(controlsRect.y + 91.0f), 10,
        replaying ? kCyan : simulation.paused() ? ORANGE : GREEN);

    const Rectangle statsRect{static_cast<float>(screenW - 340), static_cast<float>(screenH - 185), 328.0f, 171.0f};
    panel(statsRect, "SESSION ACTIVITY", nullptr);
    DrawText(TextFormat("COMPLETED FLIGHTS   %d", simulation.completedFlights()),
        static_cast<int>(statsRect.x + 16.0f), static_cast<int>(statsRect.y + 48.0f), 11, kCyan);
    DrawText(TextFormat("EMERGENCY LANDINGS  %d", simulation.emergencyLandings()),
        static_cast<int>(statsRect.x + 16.0f), static_cast<int>(statsRect.y + 70.0f), 11, ORANGE);
    int eventY = static_cast<int>(statsRect.y + 100.0f);
    for (auto it = simulation.events().rbegin(); it != simulation.events().rend() && eventY < statsRect.y + 160.0f; ++it) {
        DrawText(fitText(it->message, 9, statsRect.width - 32.0f).c_str(), static_cast<int>(statsRect.x + 16.0f), eventY, 9, kMuted);
        eventY += 16;
    }

    if (aircraft.flightEnded() && !replaying) {
        const Rectangle reportRect{static_cast<float>(screenW) * 0.5f - 225.0f,
            static_cast<float>(screenH) * 0.5f - 174.0f, 450.0f, 348.0f};
        DrawRectangle(0, 0, screenW, screenH, {0, 0, 0, 75});
        panel(reportRect, "FLIGHT REPORT", "FLIGHT SUMMARY  /  EVACUATION STATUS");
        const FlightReport& report = aircraft.report();
        DrawText(report.aircraftName.c_str(), static_cast<int>(reportRect.x + 22.0f), static_cast<int>(reportRect.y + 78.0f), 18, kText);
        DrawText(report.outcome.c_str(), static_cast<int>(reportRect.x + 22.0f), static_cast<int>(reportRect.y + 111.0f), 23,
            report.safeLanding ? GREEN : ORANGE);
        DrawText(TextFormat("PASSENGERS EVACUATED     %d / %d", report.passengersEvacuated, report.passengersOnBoard),
            static_cast<int>(reportRect.x + 22.0f), static_cast<int>(reportRect.y + 166.0f), 13, kText);
        DrawText(TextFormat("CREW EVACUATED           %d / %d", report.crewEvacuated, report.crewOnBoard),
            static_cast<int>(reportRect.x + 22.0f), static_cast<int>(reportRect.y + 195.0f), 13, kText);
        DrawText(TextFormat("SYSTEMS AFFECTED         %d", report.affectedSystems),
            static_cast<int>(reportRect.x + 22.0f), static_cast<int>(reportRect.y + 224.0f), 13, kText);
        DrawText(report.notes.c_str(), static_cast<int>(reportRect.x + 22.0f), static_cast<int>(reportRect.y + 254.0f), 10, kMuted);
        if (button({reportRect.x + 22.0f, reportRect.y + 292.0f, 406.0f, 40.0f}, "RESET FLIGHT", true)) simulation.reset();
    }

    if (simulation.replay().isPlaying()) {
        DrawRectangle(0, screenH - 34, screenW, 34, {5, 9, 13, 220});
        DrawText("REPLAY PLAYBACK", 18, screenH - 25, 12, kCyan);
        const float progress = simulation.replay().duration() > 0.0
            ? static_cast<float>(simulation.replay().playbackProgress()) : 0.0f;
        DrawRectangle(180, screenH - 21, screenW - 220, 7, {47, 57, 67, 255});
        DrawRectangle(180, screenH - 21, static_cast<int>((screenW - 220) * progress), 7, kBlue);
    }

    if (renameEditing_) {
        const Rectangle modal{static_cast<float>(screenW) * 0.5f - 210.0f,
            static_cast<float>(screenH) * 0.5f - 78.0f, 420.0f, 156.0f};
        DrawRectangle(0, 0, screenW, screenH, {0, 0, 0, 165});
        panel(modal, "RENAME SAVED REPLAY", "Type a name, then press ENTER to save or ESC to cancel.");
        DrawRectangleRounded({modal.x + 18.0f, modal.y + 66.0f, 384.0f, 36.0f}, 0.12f, 5, kPanelInner);
        const std::string shownName = renameBuffer_.empty() ? "Type a replay name..." : renameBuffer_ + "_";
        DrawText(fitText(shownName, 16, 360.0f).c_str(),
            static_cast<int>(modal.x + 30.0f), static_cast<int>(modal.y + 76.0f), 16, kText);
        if (!renameError_.empty()) {
            DrawText(fitText(renameError_, 9, 390.0f).c_str(), static_cast<int>(modal.x + 20.0f),
                static_cast<int>(modal.y + 124.0f), 9, ORANGE);
        }
    }
    gUiInputLocked = false;
}

} // namespace skyframe
