#pragma once

#include <chrono>

namespace summer {

class Timer {
public:
    Timer() { Reset(); }
    void Reset() { start_ = std::chrono::steady_clock::now(); }
    double ElapsedMs() const {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - start_)
            .count();
    }
    bool Elapsed(double ms) const { return ElapsedMs() >= ms; }

private:
    std::chrono::steady_clock::time_point start_;
};

}  // namespace summer
