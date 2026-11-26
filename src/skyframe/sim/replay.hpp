#pragma once

#include "skyframe/sim/aircraft.hpp"

#include <cstddef>
#include <vector>

namespace skyframe {

struct ReplayFrame {
    double timeSeconds{0.0};
    AircraftSnapshot aircraft{};
    int aircraftPresetIndex{1};
    WeatherState weather{};
    MapConfig mapConfig{};
    TimeOfDayState timeOfDay{};
};

class ReplayRecorder {
public:
    explicit ReplayRecorder(double captureInterval = 0.1, std::size_t maximumFrames = 1200);

    void clear() noexcept;
    void record(double timeSeconds, const Aircraft& aircraft, int aircraftPresetIndex,
        const WeatherState& weather, const MapConfig& mapConfig, const TimeOfDayState& timeOfDay);
    void beginPlayback() noexcept;
    void stopPlayback() noexcept;
    void updatePlayback(double dt) noexcept;
    [[nodiscard]] bool replaceFrames(std::vector<ReplayFrame> frames) noexcept;

    [[nodiscard]] bool isPlaying() const noexcept { return playing_; }
    [[nodiscard]] bool hasRecording() const noexcept { return !frames_.empty(); }
    [[nodiscard]] double playbackTime() const noexcept { return playbackTime_; }
    [[nodiscard]] double duration() const noexcept;
    [[nodiscard]] double playbackProgress() const noexcept;
    [[nodiscard]] const ReplayFrame* currentFrame() const noexcept;
    [[nodiscard]] const std::vector<ReplayFrame>& frames() const noexcept { return frames_; }

private:
    [[nodiscard]] ReplayFrame sample(double absoluteTime) const noexcept;

    double captureInterval_{0.1};
    std::size_t maximumFrames_{1200};
    double lastCaptureTime_{-1.0};
    double playbackTime_{0.0};
    bool playing_{false};
    mutable ReplayFrame sampledFrame_{};
    std::vector<ReplayFrame> frames_;
};

} // namespace skyframe
