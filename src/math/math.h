#pragma once

#include <algorithm>
#include <cmath>

namespace summer {

constexpr double kPi = 3.14159265358979323846;

struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    double Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    double LengthSq() const { return x * x + y * y + z * z; }
    double Length() const { return std::sqrt(LengthSq()); }
    Vec3 Normalized() const {
        double l = Length();
        if (l < 1e-12) return {};
        return {x / l, y / l, z / l};
    }
    Vec3 Cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
};

struct AABB {
    double minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;

    Vec3 Center() const {
        return {(minX + maxX) * 0.5, (minY + maxY) * 0.5, (minZ + maxZ) * 0.5};
    }
    bool Contains(const Vec3& p) const {
        return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY &&
               p.z >= minZ && p.z <= maxZ;
    }
};

struct Ray {
    Vec3 origin;
    Vec3 dir;  // normalized
};

// Minecraft rotation conventions: yaw 0 faces -Z; pitch down is positive.
inline Vec3 DirectionFromRot(float yaw, float pitch) {
    float y = yaw * (float)kPi / 180.0f;
    float p = pitch * (float)kPi / 180.0f;
    return {-std::sin(y) * std::cos(p), -std::sin(p), std::cos(y) * std::cos(p)};
}

inline double AngleBetween(const Vec3& from, const Vec3& to, const Vec3& viewDir) {
    Vec3 d = to - from;
    double l = d.Length();
    if (l < 1e-9) return 0.0;
    double dot = d.Dot(viewDir) / l;
    dot = std::clamp(dot, -1.0, 1.0);
    return std::acos(dot) * 180.0 / kPi;
}

inline bool RayAABB(const Ray& ray, const AABB& b, double& tNear, double& tFar) {
    double tMin = 0.0, tMax = 1e30;
    for (int i = 0; i < 3; ++i) {
        double o = (i == 0) ? ray.origin.x : (i == 1) ? ray.origin.y : ray.origin.z;
        double d = (i == 0) ? ray.dir.x : (i == 1) ? ray.dir.y : ray.dir.z;
        double lo = (i == 0) ? b.minX : (i == 1) ? b.minY : b.minZ;
        double hi = (i == 0) ? b.maxX : (i == 1) ? b.maxY : b.maxZ;
        if (std::fabs(d) < 1e-9) {
            if (o < lo || o > hi) return false;
        } else {
            double inv = 1.0 / d;
            double t1 = (lo - o) * inv;
            double t2 = (hi - o) * inv;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }
    tNear = tMin;
    tFar = tMax;
    return true;
}

struct CameraState {
    Vec3 pos;
    float yaw = 0.f, pitch = 0.f;
    float fov = 70.f;  // vertical fov in degrees
    int w = 1, h = 1;
};

inline bool WorldToScreen(const CameraState& cam, const Vec3& p, float& sx,
                          float& sy, bool& behind) {
    Vec3 fwd = DirectionFromRot(cam.yaw, cam.pitch);
    Vec3 right = DirectionFromRot(cam.yaw + 90.f, 0.f);
    Vec3 up = right.Cross(fwd);
    Vec3 rel = p - cam.pos;
    double dx = rel.Dot(right);
    double dy = rel.Dot(up);
    double dz = rel.Dot(fwd);
    behind = dz < 0.05;
    if (dz < 0.05) dz = 0.05;
    double halfH = std::tan(cam.fov * 0.5 * kPi / 180.0);
    double halfW = halfH * (double)cam.w / (double)cam.h;
    sx = (float)((double)cam.w * 0.5 + dx / dz / halfW * (double)cam.w * 0.5);
    sy = (float)((double)cam.h * 0.5 - dy / dz / halfH * (double)cam.h * 0.5);
    return true;
}

}  // namespace summer
