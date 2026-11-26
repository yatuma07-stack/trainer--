#pragma once
// Strafe timing: measures the gap between letting go of one of A/D and pressing the other.
// Pure logic (no raylib), covered by tests/sim_test.cpp.
//   gap > 0  : dead time, no strafe key held between release and press (ms)
//   gap < 0  : overlap, the new key went down before the old one was released (ms)
#include <algorithm>
#include <cmath>
#include <deque>

namespace strafe {

inline float moveToward(float v, float target, float maxDelta) {
    if (std::fabs(target - v) <= maxDelta) return target;
    return v + (target > v ? maxDelta : -maxDelta);
}

class Tracker {
public:
    enum Key { A = 0, D = 1 };

    void setWindowMs(float ms) { windowS_ = std::max(0.f, ms) / 1000.0; }

    void press(Key k, double t) {
        const int o = 1 - k;
        down_[k] = true;
        if (down_[o]) {  // overlap: resolved when the other key is released
            pending_ = true;
            pendKey_ = k;
            pendT_ = t;
        } else if (t - releaseT_[o] <= windowS_) {
            record(static_cast<float>((t - releaseT_[o]) * 1000.0));
        }
    }

    void release(Key k, double t) {
        down_[k] = false;
        releaseT_[k] = t;
        if (pending_) {
            if (k == 1 - pendKey_) record(static_cast<float>(-(t - pendT_) * 1000.0));
            pending_ = false;  // either way the overlap is over
        }
    }

    void reset() { *this = Tracker{}; }

    bool hasGap() const { return !hist_.empty(); }
    float lastGapMs() const { return hist_.empty() ? 0.f : hist_.back(); }
    float avgAbsMs() const {
        if (hist_.empty()) return 0.f;
        float s = 0;
        for (float v : hist_) s += std::fabs(v);
        return s / static_cast<float>(hist_.size());
    }
    const std::deque<float>& history() const { return hist_; }

private:
    void record(float ms) {
        hist_.push_back(ms);
        while (hist_.size() > 10) hist_.pop_front();
    }
    bool down_[2] = {false, false};
    double releaseT_[2] = {-1e9, -1e9};
    bool pending_ = false;
    int pendKey_ = 0;
    double pendT_ = 0;
    double windowS_ = 0.6;
    std::deque<float> hist_;
};

}  // namespace strafe
