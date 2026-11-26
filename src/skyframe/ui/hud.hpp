#pragma once

#include "skyframe/sim/fleet.hpp"
#include "skyframe/sim/replay_archive.hpp"
#include "skyframe/sim/simulation.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace skyframe {

class Hud {
public:
    void draw(Simulation& simulation, bool& cockpitView, const ReplayFrame* replayFrame);
    [[nodiscard]] bool capturesKeyboard() const noexcept { return renameEditing_; }

private:
    enum class Page { Failures, Fleet, Weather, Maps, Replay, Ordnance, Settings };
    void refreshSavedReplays();

    Page page_{Page::Failures};
    FleetCategory fleetTab_{FleetCategory::Medium};
    ReplayArchive replayArchive_{};
    std::vector<SavedReplayInfo> savedReplays_;
    bool replayArchiveLoaded_{false};
    std::size_t selectedReplay_{0};
    std::size_t replayPage_{0};
    bool renameEditing_{false};
    std::string renameBuffer_;
    std::string renameError_;
    std::string renameFileName_;
    std::string replayMessage_;
};

} // namespace skyframe
