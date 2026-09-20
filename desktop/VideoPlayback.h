#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

// One controller per video run. All video capture/seek/read operations remain
// exclusively on the worker thread. GUI requests only change this small state.
// The stop flag belongs to FrameWorker and is always checked under the lock.
class VideoPlayback final {
public:
    struct Command {
        bool stop = false;
        bool seek = false;
        std::int64_t seekMs = 0;
    };

    void setPaused(bool paused) {
        { std::lock_guard lock(mutex_); paused_ = paused; }
        changed_.notify_all();
    }
    bool isPaused() const {
        std::lock_guard lock(mutex_);
        return paused_;
    }
    void seek(std::int64_t ms) {
        { std::lock_guard lock(mutex_); seekMs_ = std::max<std::int64_t>(0, ms); seekPending_ = true; }
        changed_.notify_all();
    }
    // Called on stop, source switch and close: never leave a paused worker asleep.
    void wake() { changed_.notify_all(); }

    // Return when it is time to read another frame or a seek must be applied.
    // Pausing/seek requests interrupt pacing immediately without polling.
    Command next(const std::atomic_bool& stop,
                 std::chrono::steady_clock::time_point earliest) {
        std::unique_lock lock(mutex_);
        changed_.wait_until(lock, earliest, [&] {
            return stop.load() || seekPending_ || paused_;
        });
        while (!stop.load() && paused_ && !seekPending_)
            changed_.wait(lock, [&] { return stop.load() || seekPending_ || !paused_; });
        if (stop.load()) return {true, false, 0};
        if (seekPending_) {
            seekPending_ = false;
            return {false, true, seekMs_};
        }
        return {};
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    bool paused_ = false;
    bool seekPending_ = false;
    std::int64_t seekMs_ = 0;
};

// Slider has fixed resolution irrespective of video duration. Avoid 64-bit
// multiplication overflow even on MSVC, where long double is only 64-bit.
inline int videoSliderValue(std::int64_t positionMs, std::int64_t durationMs) {
    if (durationMs <= 0) return 0;
    return static_cast<int>(std::clamp<long double>(
        10000.0L * std::clamp<std::int64_t>(positionMs, 0, durationMs) / durationMs,
        0, 10000));
}
inline std::int64_t videoPositionFromSlider(int slider, std::int64_t durationMs) {
    if (durationMs <= 0) return 0;
    const std::int64_t step = std::clamp(slider, 0, 10000);
    return (durationMs / 10000) * step + (durationMs % 10000) * step / 10000;
}
