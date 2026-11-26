#pragma once

#include "skyframe/sim/replay.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace skyframe {

struct SavedReplayInfo {
    std::string name;
    std::string fileName;
    std::size_t frameCount{0};
    double durationSeconds{0.0};
    int aircraftPresetIndex{0};
};

class ReplayArchive {
public:
    explicit ReplayArchive(std::filesystem::path directory = defaultDirectory());

    [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }
    [[nodiscard]] std::vector<SavedReplayInfo> list() const;
    [[nodiscard]] bool save(std::string name, const ReplayRecorder& replay, std::string* error = nullptr) const;
    [[nodiscard]] bool load(const std::string& fileName, ReplayRecorder& replay, std::string* error = nullptr) const;
    [[nodiscard]] bool rename(const std::string& fileName, std::string newName, std::string* error = nullptr) const;
    [[nodiscard]] bool remove(const std::string& fileName, std::string* error = nullptr) const;

    [[nodiscard]] static std::filesystem::path defaultDirectory();

private:
    [[nodiscard]] std::filesystem::path safePath(const std::string& fileName) const;
    std::filesystem::path directory_;
};

} // namespace skyframe
