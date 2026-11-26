#include "skyframe/sim/replay.hpp"

#include "skyframe/sim/fleet.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace skyframe {
namespace {

[[nodiscard]] double lerp(double a, double b, double t) noexcept {
    return a + (b - a) * t;
}

[[nodiscard]] Vec3 lerp(const Vec3& a, const Vec3& b, double t) noexcept {
    return {lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t)};
}

[[nodiscard]] Quat slerp(Quat a, Quat b, double t) noexcept {
    a = a.normalized();
    b = b.normalized();
    double cosine = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (cosine < 0.0) {
        b = {-b.w, -b.x, -b.y, -b.z};
        cosine = -cosine;
    }
    if (cosine > 0.9995) {
        return Quat{
            lerp(a.w, b.w, t), lerp(a.x, b.x, t),
            lerp(a.y, b.y, t), lerp(a.z, b.z, t)
        }.normalized();
    }
    const double angle = std::acos(clamp(cosine, -1.0, 1.0));
    const double sine = std::sin(angle);
    if (std::abs(sine) < 1.0e-9) return a;
    const double left = std::sin((1.0 - t) * angle) / sine;
    const double right = std::sin(t * angle) / sine;
    return Quat{
        a.w * left + b.w * right, a.x * left + b.x * right,
        a.y * left + b.y * right, a.z * left + b.z * right
    }.normalized();
}

[[nodiscard]] double lerpAngleDegrees(double a, double b, double t) noexcept {
    double difference = std::fmod(b - a + 540.0, 360.0) - 180.0;
    return wrapDegrees(a + difference * t);
}

[[nodiscard]] AircraftSnapshot interpolateAircraft(
    const AircraftSnapshot& a, const AircraftSnapshot& b, double t) noexcept {
    AircraftSnapshot out = t < 0.5 ? a : b;
    out.position = lerp(a.position, b.position, t);
    out.velocity = lerp(a.velocity, b.velocity, t);
    out.orientation = slerp(a.orientation, b.orientation, t);

    out.controls.pitch = lerp(a.controls.pitch, b.controls.pitch, t);
    out.controls.roll = lerp(a.controls.roll, b.controls.roll, t);
    out.controls.yaw = lerp(a.controls.yaw, b.controls.yaw, t);
    out.controls.throttle = lerp(a.controls.throttle, b.controls.throttle, t);
    out.controls.flaps = lerp(a.controls.flaps, b.controls.flaps, t);

    out.telemetry.speed = lerp(a.telemetry.speed, b.telemetry.speed, t);
    out.telemetry.altitude = lerp(a.telemetry.altitude, b.telemetry.altitude, t);
    out.telemetry.verticalSpeed = lerp(a.telemetry.verticalSpeed, b.telemetry.verticalSpeed, t);
    out.telemetry.headingDegrees = lerpAngleDegrees(a.telemetry.headingDegrees, b.telemetry.headingDegrees, t);
    out.telemetry.pitchDegrees = lerp(a.telemetry.pitchDegrees, b.telemetry.pitchDegrees, t);
    out.telemetry.bankDegrees = lerp(a.telemetry.bankDegrees, b.telemetry.bankDegrees, t);
    out.telemetry.throttlePercent = lerp(a.telemetry.throttlePercent, b.telemetry.throttlePercent, t);

    for (std::size_t i = 0; i < out.systems.engines.size(); ++i) {
        out.systems.engines[i].power = lerp(a.systems.engines[i].power, b.systems.engines[i].power, t);
        out.systems.engines[i].spool = lerp(a.systems.engines[i].spool, b.systems.engines[i].spool, t);
    }
    out.systems.leftWingIntegrity = lerp(a.systems.leftWingIntegrity, b.systems.leftWingIntegrity, t);
    out.systems.rightWingIntegrity = lerp(a.systems.rightWingIntegrity, b.systems.rightWingIntegrity, t);
    out.systems.tailIntegrity = lerp(a.systems.tailIntegrity, b.systems.tailIntegrity, t);
    out.systems.hydraulicIntegrity = lerp(a.systems.hydraulicIntegrity, b.systems.hydraulicIntegrity, t);
    out.systems.electricalIntegrity = lerp(a.systems.electricalIntegrity, b.systems.electricalIntegrity, t);
    out.systems.landingGearIntegrity = lerp(a.systems.landingGearIntegrity, b.systems.landingGearIntegrity, t);
    return out;
}

} // namespace

ReplayRecorder::ReplayRecorder(double captureInterval, std::size_t maximumFrames)
    : captureInterval_(std::max(0.02, captureInterval)), maximumFrames_(std::max<std::size_t>(2, maximumFrames)) {
    frames_.reserve(maximumFrames_);
}

void ReplayRecorder::clear() noexcept {
    frames_.clear();
    lastCaptureTime_ = -1.0;
    playbackTime_ = 0.0;
    playing_ = false;
}

void ReplayRecorder::record(double timeSeconds, const Aircraft& aircraft, int aircraftPresetIndex,
    const WeatherState& weather, const MapConfig& mapConfig, const TimeOfDayState& timeOfDay) {
    if (playing_ || !std::isfinite(timeSeconds)) return;
    if (lastCaptureTime_ >= 0.0 && timeSeconds < lastCaptureTime_) return;
    if (lastCaptureTime_ >= 0.0 && timeSeconds - lastCaptureTime_ < captureInterval_) return;

    frames_.push_back({timeSeconds, aircraft.snapshot(), aircraftPresetIndex, weather, mapConfig, timeOfDay});
    lastCaptureTime_ = timeSeconds;
    if (frames_.size() > maximumFrames_) frames_.erase(frames_.begin());
}

bool ReplayRecorder::replaceFrames(std::vector<ReplayFrame> frames) noexcept {
    if (frames.size() < 2 || frames.size() > maximumFrames_) return false;
    double previous = -1.0;
    for (const ReplayFrame& frame : frames) {
        if (!std::isfinite(frame.timeSeconds) || frame.timeSeconds < 0.0
            || (previous >= 0.0 && frame.timeSeconds <= previous)) return false;
        if (frame.aircraftPresetIndex < 0 || frame.aircraftPresetIndex >= fleetCount()) return false;
        previous = frame.timeSeconds;
    }
    frames_ = std::move(frames);
    lastCaptureTime_ = frames_.back().timeSeconds;
    playbackTime_ = 0.0;
    playing_ = false;
    return true;
}

void ReplayRecorder::beginPlayback() noexcept {
    if (frames_.size() < 2) return;
    playbackTime_ = 0.0;
    playing_ = true;
}

void ReplayRecorder::stopPlayback() noexcept {
    playing_ = false;
    playbackTime_ = 0.0;
}

void ReplayRecorder::updatePlayback(double dt) noexcept {
    if (!playing_ || !std::isfinite(dt)) return;
    playbackTime_ = clamp(playbackTime_ + std::max(0.0, dt), 0.0, duration());
}

double ReplayRecorder::duration() const noexcept {
    if (frames_.size() < 2) return 0.0;
    return std::max(0.0, frames_.back().timeSeconds - frames_.front().timeSeconds);
}

double ReplayRecorder::playbackProgress() const noexcept {
    const double total = duration();
    return total > 1.0e-9 ? clamp(playbackTime_ / total, 0.0, 1.0) : 0.0;
}

ReplayFrame ReplayRecorder::sample(double absoluteTime) const noexcept {
    if (frames_.empty()) return {};
    const auto it = std::lower_bound(frames_.begin(), frames_.end(), absoluteTime,
        [](const ReplayFrame& frame, double time) { return frame.timeSeconds < time; });
    if (it == frames_.begin()) return *it;
    if (it == frames_.end()) return frames_.back();

    const auto previous = std::prev(it);
    const double span = it->timeSeconds - previous->timeSeconds;
    if (span <= 1.0e-9) return *it;
    const double t = clamp((absoluteTime - previous->timeSeconds) / span, 0.0, 1.0);

    ReplayFrame result = t < 0.5 ? *previous : *it;
    result.timeSeconds = absoluteTime;
    result.aircraft = interpolateAircraft(previous->aircraft, it->aircraft, t);
    result.weather.wind = lerp(previous->weather.wind, it->weather.wind, t);
    result.weather.windDirectionDegrees = lerpAngleDegrees(previous->weather.windDirectionDegrees,
        it->weather.windDirectionDegrees, t);
    result.weather.turbulence = lerp(previous->weather.turbulence, it->weather.turbulence, t);
    result.weather.gustStrength = lerp(previous->weather.gustStrength, it->weather.gustStrength, t);
    result.weather.visibilityKm = lerp(previous->weather.visibilityKm, it->weather.visibilityKm, t);
    result.weather.cloudBaseMeters = lerp(previous->weather.cloudBaseMeters, it->weather.cloudBaseMeters, t);
    result.weather.rainIntensity = lerp(previous->weather.rainIntensity, it->weather.rainIntensity, t);
    result.weather.snowIntensity = lerp(previous->weather.snowIntensity, it->weather.snowIntensity, t);
    result.weather.fogDensity = lerp(previous->weather.fogDensity, it->weather.fogDensity, t);
    result.weather.lightningIntensity = lerp(previous->weather.lightningIntensity, it->weather.lightningIntensity, t);
    result.weather.autoTransitionSeconds = lerp(previous->weather.autoTransitionSeconds,
        it->weather.autoTransitionSeconds, t);
    const double hourDifference = std::fmod(it->timeOfDay.hours - previous->timeOfDay.hours + 36.0, 24.0) - 12.0;
    result.timeOfDay.hours = std::fmod(previous->timeOfDay.hours + hourDifference * t + 24.0, 24.0);
    return result;
}

const ReplayFrame* ReplayRecorder::currentFrame() const noexcept {
    if (frames_.empty()) return nullptr;
    const double absoluteTime = frames_.front().timeSeconds + playbackTime_;
    sampledFrame_ = sample(absoluteTime);
    return &sampledFrame_;
}

} // namespace skyframe
