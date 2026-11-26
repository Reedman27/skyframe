#include "skyframe/sim/replay_archive.hpp"

#include "skyframe/sim/fleet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>

namespace skyframe {
namespace {
constexpr std::string_view kMagic = "SKYFRAME_REPLAY";
constexpr int kFormatVersion = 2;
constexpr int kOldestSupportedFormatVersion = 1;
constexpr std::size_t kMaximumFrames = 1200;
constexpr std::uintmax_t kMaximumFileBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumSavedReplays = 30;

void setError(std::string* error, std::string message) {
    if (error != nullptr) *error = std::move(message);
}

[[nodiscard]] std::string cleanName(std::string name) {
    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char c) {
        return c < 0x20U || c == '\n' || c == '\r';
    }), name.end());
    if (name.size() > 48U) name.resize(48U);
    const auto first = name.find_first_not_of(" \t");
    if (first == std::string::npos) return "Untitled flight";
    const auto last = name.find_last_not_of(" \t");
    return name.substr(first, last - first + 1U);
}

[[nodiscard]] std::string makeSlug(const std::string& name) {
    std::string slug;
    slug.reserve(name.size());
    bool previousUnderscore = false;
    for (unsigned char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            slug.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
            previousUnderscore = false;
        } else if (!previousUnderscore && !slug.empty()) {
            slug.push_back('_');
            previousUnderscore = true;
        }
    }
    while (!slug.empty() && slug.back() == '_') slug.pop_back();
    if (slug.empty()) slug = "flight";
    if (slug.size() > 40U) slug.resize(40U);
    return slug;
}

void writeSnapshot(std::ostream& out, const AircraftSnapshot& s) {
    out << s.position.x << ' ' << s.position.y << ' ' << s.position.z << ' '
        << s.velocity.x << ' ' << s.velocity.y << ' ' << s.velocity.z << ' '
        << s.orientation.w << ' ' << s.orientation.x << ' ' << s.orientation.y << ' ' << s.orientation.z << ' '
        << s.controls.pitch << ' ' << s.controls.roll << ' ' << s.controls.yaw << ' '
        << s.controls.throttle << ' ' << s.controls.flaps << ' '
        << static_cast<int>(s.controls.gearDown) << ' ' << static_cast<int>(s.controls.spoilers) << ' '
        << static_cast<int>(s.controls.parkingBrake) << ' '
        << s.telemetry.speed << ' ' << s.telemetry.altitude << ' ' << s.telemetry.verticalSpeed << ' '
        << s.telemetry.headingDegrees << ' ' << s.telemetry.pitchDegrees << ' ' << s.telemetry.bankDegrees << ' '
        << s.telemetry.throttlePercent << ' ' << static_cast<int>(s.telemetry.stallWarning) << ' '
        << static_cast<int>(s.telemetry.sinkRateWarning) << ' ' << static_cast<int>(s.telemetry.pullUpWarning) << ' '
        << static_cast<int>(s.telemetry.onGround) << ' ';
    for (const EngineState& engine : s.systems.engines) {
        out << engine.power << ' ' << engine.spool << ' ' << static_cast<int>(engine.running) << ' '
            << static_cast<int>(engine.overheated) << ' ';
    }
    out << s.systems.leftWingIntegrity << ' ' << s.systems.rightWingIntegrity << ' '
        << s.systems.tailIntegrity << ' ' << s.systems.hydraulicIntegrity << ' '
        << s.systems.electricalIntegrity << ' ' << s.systems.landingGearIntegrity << ' '
        << static_cast<int>(s.systems.leftWingPanelVisible) << ' '
        << static_cast<int>(s.systems.rightWingPanelVisible) << ' '
        << static_cast<int>(s.systems.flightControlsLocked) << ' '
        << static_cast<int>(s.systems.gearStuck) << ' ' << s.systems.affectedSystems << ' '
        << s.passengers << ' ' << s.elapsedSeconds << ' ' << static_cast<int>(s.onGround) << ' '
        << static_cast<int>(s.reportValid) << ' ' << static_cast<int>(s.reportSafeLanding);
}

[[nodiscard]] bool readBool(std::istream& in, bool& value) {
    int raw = 0;
    if (!(in >> raw) || (raw != 0 && raw != 1)) return false;
    value = raw != 0;
    return true;
}

[[nodiscard]] bool readFinite(std::istream& in, double& value) {
    return static_cast<bool>(in >> value) && std::isfinite(value);
}

[[nodiscard]] bool inRange(double value, double minimum, double maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

[[nodiscard]] bool readSnapshot(std::istream& in, AircraftSnapshot& s) {
    if (!readFinite(in, s.position.x) || !readFinite(in, s.position.y) || !readFinite(in, s.position.z)
        || !readFinite(in, s.velocity.x) || !readFinite(in, s.velocity.y) || !readFinite(in, s.velocity.z)
        || !readFinite(in, s.orientation.w) || !readFinite(in, s.orientation.x)
        || !readFinite(in, s.orientation.y) || !readFinite(in, s.orientation.z)
        || !readFinite(in, s.controls.pitch) || !readFinite(in, s.controls.roll)
        || !readFinite(in, s.controls.yaw) || !readFinite(in, s.controls.throttle)
        || !readFinite(in, s.controls.flaps)) return false;
    if (!readBool(in, s.controls.gearDown) || !readBool(in, s.controls.spoilers)
        || !readBool(in, s.controls.parkingBrake)) return false;
    if (!readFinite(in, s.telemetry.speed) || !readFinite(in, s.telemetry.altitude)
        || !readFinite(in, s.telemetry.verticalSpeed) || !readFinite(in, s.telemetry.headingDegrees)
        || !readFinite(in, s.telemetry.pitchDegrees) || !readFinite(in, s.telemetry.bankDegrees)
        || !readFinite(in, s.telemetry.throttlePercent) || !readBool(in, s.telemetry.stallWarning)
        || !readBool(in, s.telemetry.sinkRateWarning) || !readBool(in, s.telemetry.pullUpWarning)
        || !readBool(in, s.telemetry.onGround)) return false;
    for (EngineState& engine : s.systems.engines) {
        if (!readFinite(in, engine.power) || !readFinite(in, engine.spool)
            || !readBool(in, engine.running) || !readBool(in, engine.overheated)) return false;
    }
    if (!readFinite(in, s.systems.leftWingIntegrity) || !readFinite(in, s.systems.rightWingIntegrity)
        || !readFinite(in, s.systems.tailIntegrity) || !readFinite(in, s.systems.hydraulicIntegrity)
        || !readFinite(in, s.systems.electricalIntegrity) || !readFinite(in, s.systems.landingGearIntegrity)
        || !readBool(in, s.systems.leftWingPanelVisible) || !readBool(in, s.systems.rightWingPanelVisible)
        || !readBool(in, s.systems.flightControlsLocked) || !readBool(in, s.systems.gearStuck)
        || !(in >> s.systems.affectedSystems >> s.passengers) || !readFinite(in, s.elapsedSeconds)
        || !readBool(in, s.onGround) || !readBool(in, s.reportValid) || !readBool(in, s.reportSafeLanding)) return false;

    const double orientationNormSquared = s.orientation.w * s.orientation.w + s.orientation.x * s.orientation.x
        + s.orientation.y * s.orientation.y + s.orientation.z * s.orientation.z;
    if (std::abs(s.position.x) > 1.0e7 || std::abs(s.position.y) > 1.0e7 || std::abs(s.position.z) > 1.0e7
        || s.velocity.lengthSquared() > 1.0e12 || s.passengers < 0 || s.passengers > 10000
        || s.elapsedSeconds < 0.0 || s.elapsedSeconds > 1.0e9
        || s.systems.affectedSystems < 0 || s.systems.affectedSystems > 65535
        || !std::isfinite(orientationNormSquared) || orientationNormSquared < 0.5 || orientationNormSquared > 1.5) return false;
    if (!inRange(s.controls.pitch, -1.0, 1.0) || !inRange(s.controls.roll, -1.0, 1.0)
        || !inRange(s.controls.yaw, -1.0, 1.0) || !inRange(s.controls.throttle, 0.0, 1.0)
        || !inRange(s.controls.flaps, 0.0, 1.0)
        || !inRange(s.orientation.w, -1.0, 1.0) || !inRange(s.orientation.x, -1.0, 1.0)
        || !inRange(s.orientation.y, -1.0, 1.0) || !inRange(s.orientation.z, -1.0, 1.0)
        || !inRange(s.telemetry.speed, 0.0, 5000.0) || !inRange(s.telemetry.altitude, -10000.0, 10000000.0)
        || !inRange(s.telemetry.verticalSpeed, -5000.0, 5000.0)
        || !inRange(s.telemetry.headingDegrees, -360.0, 360.0)
        || !inRange(s.telemetry.pitchDegrees, -180.0, 180.0)
        || !inRange(s.telemetry.bankDegrees, -180.0, 180.0)
        || !inRange(s.telemetry.throttlePercent, 0.0, 100.0)) return false;
    for (const EngineState& engine : s.systems.engines) {
        if (!inRange(engine.power, 0.0, 1.0) || !inRange(engine.spool, 0.0, 1.0)) return false;
    }
    if (!inRange(s.systems.leftWingIntegrity, 0.0, 1.0)
        || !inRange(s.systems.rightWingIntegrity, 0.0, 1.0)
        || !inRange(s.systems.tailIntegrity, 0.0, 1.0)
        || !inRange(s.systems.hydraulicIntegrity, 0.0, 1.0)
        || !inRange(s.systems.electricalIntegrity, 0.0, 1.0)
        || !inRange(s.systems.landingGearIntegrity, 0.0, 1.0)) return false;
    s.orientation = s.orientation.normalized();
    return true;
}

void writeFrame(std::ostream& out, const ReplayFrame& frame) {
    out << "FRAME " << frame.timeSeconds << ' ' << frame.aircraftPresetIndex << ' '
        << static_cast<int>(frame.mapConfig.terrain) << ' ' << static_cast<int>(frame.mapConfig.size) << ' '
        << frame.mapConfig.seed << ' ' << static_cast<int>(frame.mapConfig.airportEnabled) << ' '
        << static_cast<int>(frame.mapConfig.airportClass) << ' '
        << static_cast<int>(frame.mapConfig.towersEnabled) << ' '
        << static_cast<int>(frame.mapConfig.powerLinesEnabled) << ' '
        << static_cast<int>(frame.mapConfig.terrainSceneryEnabled) << ' '
        << static_cast<int>(frame.timeOfDay.mode) << ' ' << frame.timeOfDay.hours << ' '
        << static_cast<int>(frame.weather.preset) << ' ' << static_cast<int>(frame.weather.activePreset) << ' '
        << frame.weather.wind.x << ' ' << frame.weather.wind.y << ' ' << frame.weather.wind.z << ' '
        << frame.weather.windDirectionDegrees << ' ' << frame.weather.turbulence << ' '
        << frame.weather.gustStrength << ' ' << frame.weather.visibilityKm << ' '
        << frame.weather.cloudBaseMeters << ' ' << frame.weather.rainIntensity << ' '
        << frame.weather.snowIntensity << ' ' << frame.weather.fogDensity << ' '
        << frame.weather.lightningIntensity << ' ' << frame.weather.autoTransitionSeconds << ' '
        << static_cast<int>(frame.weather.autoPhase) << ';';
    writeSnapshot(out, frame.aircraft);
    out << '\n';
}

[[nodiscard]] bool readFrame(std::istream& in, ReplayFrame& frame, int version) {
    std::string marker;
    int terrain = 0, size = 0, airportEnabled = 0, airportClass = 0;
    int towersEnabled = 0, powerLinesEnabled = 0, sceneryEnabled = 1;
    int tod = 0, preset = 0, activePreset = 0, autoPhase = 0;
    if (!(in >> marker) || marker != "FRAME" || !readFinite(in, frame.timeSeconds)
        || !(in >> frame.aircraftPresetIndex >> terrain >> size >> frame.mapConfig.seed >> airportEnabled
            >> airportClass >> towersEnabled)) return false;
    if (version == 1) {
        if (!(in >> tod)) return false;
        powerLinesEnabled = towersEnabled;
    } else if (!(in >> powerLinesEnabled >> sceneryEnabled >> tod)) {
        return false;
    }
    if (!readFinite(in, frame.timeOfDay.hours) || !(in >> preset >> activePreset)) return false;
    const int maxTerrain = version == 1
        ? static_cast<int>(MapPreset::Mountains) : static_cast<int>(MapPreset::Random);
    if (terrain < 0 || terrain > maxTerrain
        || size < 0 || size > static_cast<int>(MapSize::Large)
        || (airportEnabled != 0 && airportEnabled != 1)
        || airportClass < 0 || airportClass > static_cast<int>(AirportClass::Airfield)
        || (towersEnabled != 0 && towersEnabled != 1)
        || (powerLinesEnabled != 0 && powerLinesEnabled != 1)
        || (sceneryEnabled != 0 && sceneryEnabled != 1)
        || tod < 0 || tod > static_cast<int>(TimeOfDay::Cycle)
        || preset < 0 || preset > static_cast<int>(WeatherPreset::Gusty)
        || activePreset < 0 || activePreset > static_cast<int>(WeatherPreset::Gusty)) return false;
    frame.mapConfig.terrain = static_cast<MapPreset>(terrain);
    frame.mapConfig.size = static_cast<MapSize>(size);
    frame.mapConfig.airportEnabled = airportEnabled != 0;
    frame.mapConfig.airportClass = static_cast<AirportClass>(airportClass);
    frame.mapConfig.towersEnabled = towersEnabled != 0;
    frame.mapConfig.powerLinesEnabled = powerLinesEnabled != 0;
    frame.mapConfig.terrainSceneryEnabled = sceneryEnabled != 0;
    frame.timeOfDay.mode = static_cast<TimeOfDay>(tod);
    frame.weather.preset = static_cast<WeatherPreset>(preset);
    frame.weather.activePreset = static_cast<WeatherPreset>(activePreset);
    if (!readFinite(in, frame.weather.wind.x) || !readFinite(in, frame.weather.wind.y)
        || !readFinite(in, frame.weather.wind.z) || !readFinite(in, frame.weather.windDirectionDegrees)
        || !readFinite(in, frame.weather.turbulence) || !readFinite(in, frame.weather.gustStrength)
        || !readFinite(in, frame.weather.visibilityKm) || !readFinite(in, frame.weather.cloudBaseMeters)
        || !readFinite(in, frame.weather.rainIntensity) || !readFinite(in, frame.weather.snowIntensity)
        || !readFinite(in, frame.weather.fogDensity) || !readFinite(in, frame.weather.lightningIntensity)
        || !readFinite(in, frame.weather.autoTransitionSeconds) || !(in >> autoPhase)) return false;
    if (autoPhase < 0 || autoPhase > 5 || frame.timeSeconds < 0.0
        || frame.aircraftPresetIndex < 0 || frame.aircraftPresetIndex >= fleetCount()
        || frame.timeOfDay.hours < 0.0 || frame.timeOfDay.hours >= 24.0
        || !inRange(frame.weather.wind.x, -100.0, 100.0)
        || !inRange(frame.weather.wind.y, -100.0, 100.0)
        || !inRange(frame.weather.wind.z, -100.0, 100.0)
        || !inRange(frame.weather.windDirectionDegrees, 0.0, 360.0)
        || !inRange(frame.weather.turbulence, 0.0, 1.0)
        || !inRange(frame.weather.gustStrength, 0.0, 50.0)
        || !inRange(frame.weather.visibilityKm, 0.0, 1000.0)
        || !inRange(frame.weather.cloudBaseMeters, 0.0, 30000.0)
        || !inRange(frame.weather.rainIntensity, 0.0, 1.0)
        || !inRange(frame.weather.snowIntensity, 0.0, 1.0)
        || !inRange(frame.weather.fogDensity, 0.0, 1.0)
        || !inRange(frame.weather.lightningIntensity, 0.0, 1.0)
        || !inRange(frame.weather.autoTransitionSeconds, 0.0, 150.0)) return false;
    frame.weather.autoPhase = static_cast<std::uint8_t>(autoPhase);
    if (!(in >> std::ws) || in.peek() != ';') return false;
    in.get();
    if (!readSnapshot(in, frame.aircraft)) return false;
    // Validate after parsing the aircraft snapshot. Checking this before readSnapshot()
    // would only inspect the default-initialized passenger count and miss bad archive data.
    return frame.aircraft.passengers <= fleetEntry(frame.aircraftPresetIndex).config.passengerCapacity;

}

[[nodiscard]] bool writeReplayFile(const std::filesystem::path& path, const std::string& name,
    const std::vector<ReplayFrame>& frames, std::string* error) {
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::out | std::ios::trunc);
        out.imbue(std::locale::classic());
        if (!out) {
            setError(error, "Could not open a temporary replay file for writing.");
            return false;
        }
        out << kMagic << ' ' << kFormatVersion << '\n'
            << "NAME " << std::quoted(name) << '\n'
            << "FRAME_COUNT " << frames.size() << '\n'
            << std::setprecision(17);
        for (const ReplayFrame& frame : frames) writeFrame(out, frame);
        out.flush();
        if (!out) {
            setError(error, "Replay write failed before the file was complete.");
            out.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        // Windows does not replace an existing destination with rename(). Move the old
        // file aside first and restore it if the new file cannot be installed.
        std::error_code existsError;
        const bool destinationExists = std::filesystem::exists(path, existsError);
        if (!existsError && destinationExists) {
            const std::filesystem::path backup = path.string() + ".backup.tmp";
            std::error_code backupError;
            std::filesystem::remove(backup, backupError);
            backupError.clear();
            std::filesystem::rename(path, backup, backupError);
            if (!backupError) {
                ec.clear();
                std::filesystem::rename(temporary, path, ec);
                if (ec) {
                    std::error_code restoreError;
                    std::filesystem::rename(backup, path, restoreError);
                    if (restoreError) {
                        ec = restoreError;
                        setError(error, "Replay replacement failed, and the previous file could not be restored: "
                            + restoreError.message());
                    }
                } else {
                    std::error_code ignored;
                    std::filesystem::remove(backup, ignored);
                }
            } else {
                ec = backupError;
            }
        }
    }
    if (ec) {
        setError(error, "Could not safely finish the replay file: " + ec.message());
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

[[nodiscard]] bool readReplayFile(const std::filesystem::path& path, std::string& name,
    std::vector<ReplayFrame>& frames, std::string* error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaximumFileBytes) {
        setError(error, "Replay file is unavailable or exceeds the 16 MiB safety limit.");
        return false;
    }
    std::error_code statusError;
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, statusError);
    if (statusError || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status)) {
        setError(error, "Replay entry is not a regular file in the archive.");
        return false;
    }
    std::ifstream in(path);
    in.imbue(std::locale::classic());
    if (!in) {
        setError(error, "Could not open the replay file.");
        return false;
    }
    std::string magic;
    int version = 0;
    std::string nameTag;
    std::string countTag;
    std::size_t count = 0;
    if (!(in >> magic >> version) || magic != kMagic
        || version < kOldestSupportedFormatVersion || version > kFormatVersion
        || !(in >> nameTag >> std::quoted(name)) || nameTag != "NAME"
        || !(in >> countTag >> count) || countTag != "FRAME_COUNT"
        || count < 2 || count > kMaximumFrames) {
        setError(error, "Replay header is invalid or uses an unsupported format version.");
        return false;
    }
    name = cleanName(name);
    frames.clear();
    frames.reserve(count);
    double previousTime = -1.0;
    for (std::size_t i = 0; i < count; ++i) {
        ReplayFrame frame;
        if (!readFrame(in, frame, version) || (previousTime >= 0.0 && frame.timeSeconds <= previousTime)) {
            setError(error, "Replay data is truncated, invalid, or has non-monotonic timestamps.");
            frames.clear();
            return false;
        }
        previousTime = frame.timeSeconds;
        frames.push_back(std::move(frame));
    }
    std::string extra;
    if (in >> extra) {
        setError(error, "Replay contains unexpected trailing data.");
        frames.clear();
        return false;
    }
    return true;
}

[[nodiscard]] bool safeFileName(const std::string& fileName) {
    if (fileName.size() < 5U || fileName.size() > 80U
        || fileName.substr(fileName.size() - 4U) != ".sfr" || fileName.front() == '.') return false;
    return std::all_of(fileName.begin(), fileName.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    });
}

} // namespace

ReplayArchive::ReplayArchive(std::filesystem::path directory) : directory_(std::move(directory)) {}

std::filesystem::path ReplayArchive::defaultDirectory() {
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"); appData != nullptr && *appData != '\0') {
        return std::filesystem::path(appData) / "Skyframe" / "replays";
    }
#else
    if (const char* xdgData = std::getenv("XDG_DATA_HOME"); xdgData != nullptr && *xdgData != '\0') {
        return std::filesystem::path(xdgData) / "skyframe" / "replays";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".local" / "share" / "skyframe" / "replays";
    }
#endif
    return std::filesystem::current_path() / "replays";
}

std::filesystem::path ReplayArchive::safePath(const std::string& fileName) const {
    if (!safeFileName(fileName)) return {};
    return directory_ / fileName;
}

std::vector<SavedReplayInfo> ReplayArchive::list() const {
    std::vector<SavedReplayInfo> entries;
    std::error_code ec;
    if (!std::filesystem::exists(directory_, ec) || ec) return entries;
    for (std::filesystem::directory_iterator it(directory_, ec), end; !ec && it != end; it.increment(ec)) {
        const std::filesystem::file_status entryStatus = it->symlink_status(ec);
        if (ec || std::filesystem::is_symlink(entryStatus) || !std::filesystem::is_regular_file(entryStatus)
            || it->path().extension() != ".sfr") continue;
        std::string name;
        std::vector<ReplayFrame> frames;
        if (!readReplayFile(it->path(), name, frames, nullptr)) continue;
        SavedReplayInfo info;
        info.name = std::move(name);
        info.fileName = it->path().filename().string();
        info.frameCount = frames.size();
        info.durationSeconds = frames.back().timeSeconds - frames.front().timeSeconds;
        info.aircraftPresetIndex = frames.front().aircraftPresetIndex;
        entries.push_back(std::move(info));
        if (entries.size() >= kMaximumSavedReplays) break;
    }
    std::sort(entries.begin(), entries.end(), [](const SavedReplayInfo& a, const SavedReplayInfo& b) {
        return a.fileName > b.fileName;
    });
    return entries;
}

bool ReplayArchive::save(std::string name, const ReplayRecorder& replay, std::string* error) const {
    const auto& frames = replay.frames();
    if (frames.size() < 2 || frames.size() > kMaximumFrames) {
        setError(error, "A replay needs between 2 and 1200 recorded frames before it can be saved.");
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    if (ec) {
        setError(error, "Could not create the replay directory: " + ec.message());
        return false;
    }
    if (list().size() >= kMaximumSavedReplays) {
        setError(error, "The replay archive is full (30 saved recordings). Delete one before saving another.");
        return false;
    }
    name = cleanName(std::move(name));
    const std::string slug = makeSlug(name);
    std::filesystem::path path = directory_ / (slug + ".sfr");
    for (int suffix = 2; std::filesystem::exists(path, ec) && suffix <= 999; ++suffix) {
        path = directory_ / (slug + "_" + std::to_string(suffix) + ".sfr");
    }
    if (ec || std::filesystem::exists(path, ec)) {
        setError(error, "Could not allocate a unique replay filename.");
        return false;
    }
    return writeReplayFile(path, name, frames, error);
}

bool ReplayArchive::load(const std::string& fileName, ReplayRecorder& replay, std::string* error) const {
    const std::filesystem::path path = safePath(fileName);
    if (path.empty()) {
        setError(error, "Replay filename is not a safe archive entry.");
        return false;
    }
    std::string name;
    std::vector<ReplayFrame> frames;
    if (!readReplayFile(path, name, frames, error)) return false;
    if (!replay.replaceFrames(std::move(frames))) {
        setError(error, "Replay contains more frames or invalid timestamps than this build accepts.");
        return false;
    }
    return true;
}

bool ReplayArchive::rename(const std::string& fileName, std::string newName, std::string* error) const {
    const std::filesystem::path oldPath = safePath(fileName);
    if (oldPath.empty()) {
        setError(error, "Replay filename is not a safe archive entry.");
        return false;
    }
    std::string previousName;
    std::vector<ReplayFrame> frames;
    if (!readReplayFile(oldPath, previousName, frames, error)) return false;
    return writeReplayFile(oldPath, cleanName(std::move(newName)), frames, error);
}

bool ReplayArchive::remove(const std::string& fileName, std::string* error) const {
    const std::filesystem::path path = safePath(fileName);
    if (path.empty()) {
        setError(error, "Replay filename is not a safe archive entry.");
        return false;
    }
    std::error_code ec;
    const bool removed = std::filesystem::remove(path, ec);
    if (ec || !removed) {
        setError(error, ec ? "Could not delete replay: " + ec.message() : "Replay file was not found.");
        return false;
    }
    return true;
}

} // namespace skyframe
