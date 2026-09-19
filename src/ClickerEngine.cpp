#include "ClickerEngine.h"

#include <algorithm>
#include <deque>

namespace ezac {

ClickerEngine::ClickerEngine(PlatformInput* input) : input_(input) {
    worker_ = std::thread(&ClickerEngine::run, this);
}

ClickerEngine::~ClickerEngine() {
    stop_.store(true, std::memory_order_relaxed);
    if (worker_.joinable()) worker_.join();
}

void ClickerEngine::setTargetCps(int cps) {
    targetCps_.store(std::clamp(cps, kMinCps, kMaxCps), std::memory_order_relaxed);
}

void ClickerEngine::run() {
    using clock = std::chrono::steady_clock;

    // Sliding window of recent click timestamps, used only to report a live
    // measured CPS back to the UI (separate from the target rate).
    std::deque<clock::time_point> recentClicks;

    while (!stop_.load(std::memory_order_relaxed)) {
        if (!input_->isKeyHeld(triggerKeyCode_.load(std::memory_order_relaxed))) {
            active_.store(false, std::memory_order_relaxed);
            measuredCps_.store(0.0, std::memory_order_relaxed);
            recentClicks.clear();
            // Idle: a coarse 1ms poll costs effectively nothing and is far
            // faster than any human can react, so releasing F feels instant.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        active_.store(true, std::memory_order_relaxed);
        const int cps = targetCps_.load(std::memory_order_relaxed);
        const auto interval = std::chrono::nanoseconds(1'000'000'000LL / cps);
        auto target = clock::now();

        input_->sendClick(MouseButton::Left);
        totalClicks_.fetch_add(1, std::memory_order_relaxed);

        const auto now = clock::now();
        recentClicks.push_back(now);
        while (!recentClicks.empty() &&
               now - recentClicks.front() > std::chrono::milliseconds(300)) {
            recentClicks.pop_front();
        }
        if (recentClicks.size() > 1) {
            const double windowSec =
                std::chrono::duration<double>(now - recentClicks.front()).count();
            if (windowSec > 0.0) {
                measuredCps_.store((recentClicks.size() - 1) / windowSec,
                                    std::memory_order_relaxed);
            }
        }

        target += interval;
        preciseSleepUntil(target);
    }
}

void ClickerEngine::preciseSleepUntil(std::chrono::steady_clock::time_point target) {
    using clock = std::chrono::steady_clock;

    // Hybrid sleep: let the OS scheduler handle the bulk of the wait (near-zero
    // CPU cost), then busy-spin only the last fraction of a millisecond for
    // precision. This is what keeps 900 CPS accurate without pegging a core.
    constexpr auto kSpinMargin = std::chrono::microseconds(300);

    const auto now = clock::now();
    if (target - now > kSpinMargin) {
        std::this_thread::sleep_for(target - now - kSpinMargin);
    }
    while (clock::now() < target) {
        // intentional busy-wait, bounded to ~kSpinMargin
    }
}

} // namespace ezac
