#include "random.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace summer {

static std::mt19937& Gen() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

double Rng::Next() {
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(Gen());
}

int Rng::RangeInt(int a, int b) {
    if (a > b) std::swap(a, b);
    std::uniform_int_distribution<int> d(a, b);
    return d(Gen());
}

double Rng::Range(double a, double b) {
    if (a > b) std::swap(a, b);
    std::uniform_real_distribution<double> d(a, b);
    return d(Gen());
}

bool Rng::Chance(double percent) {
    return Next() * 100.0 < percent;
}

double Rng::Gauss(double mean, double sigma) {
    std::normal_distribution<double> d(mean, sigma);
    return d(Gen());
}

}  // namespace summer
