#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include "platform/PlatformInput.h"

namespace ezac {

// Runs a dedicated background thread that watches the trigger key and, while
// it is held, fires mouse clicks at the configured rate. Decoupled from the
// UI thread so render/UI work never competes with click timing precision.
class ClickerEngine {
public:
    explicit ClickerEngine(PlatformInput* input);
    ~ClickerEngine();

    ClickerEngine(const ClickerEngine&) = delete;
    ClickerEngine& operator=(const ClickerEngine&) = delete;

    // Clamped to [kMinCps, kMaxCps]. Safe to call from the UI thread.
    void setTargetCps(int cps);
    int targetCps() const { return targetCps_.load(std::memory_order_relaxed); }

    // Platform-native key code (see PlatformInput.h) that triggers clicking
    // while held. Safe to call from the UI thread.
    void setTriggerKeyCode(int code) { triggerKeyCode_.store(code, std::memory_order_relaxed); }
    int triggerKeyCode() const { return triggerKeyCode_.load(std::memory_order_relaxed); }

    bool isActive() const { return active_.load(std::memory_order_relaxed); }
    double measuredCps() const { return measuredCps_.load(std::memory_order_relaxed); }
    long long totalClicks() const { return totalClicks_.load(std::memory_order_relaxed); }

    static constexpr int kMinCps = 100;
    static constexpr int kMaxCps = 900;
    static constexpr int kDefaultCps = 800;

private:
    void run();
    static void preciseSleepUntil(std::chrono::steady_clock::time_point target);

    PlatformInput* input_;
    std::atomic<int> targetCps_{kDefaultCps};
    std::atomic<int> triggerKeyCode_{defaultTriggerKeyCode()};
    std::atomic<bool> active_{false};
    std::atomic<bool> stop_{false};
    std::atomic<double> measuredCps_{0.0};
    std::atomic<long long> totalClicks_{0};
    std::thread worker_;
};

} // namespace ezac
