#pragma once

namespace summer {

class Rng {
public:
    static double Next();  // [0, 1)
    static int RangeInt(int a, int b);
    static double Range(double a, double b);
    static bool Chance(double percent);
    static double Gauss(double mean, double sigma);
};

}  // namespace summer
