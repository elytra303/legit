#pragma once
#include <cmath>
#include <algorithm>

struct Vec3d {
    double x = 0.0, y = 0.0, z = 0.0;
};

struct Box3d {
    double minX, minY, minZ, maxX, maxY, maxZ;
};

inline double radians(double deg) { return deg * 0.017453292519943295; }
inline double degrees(double rad) { return rad * 57.29577951308232; }

inline Vec3d dir_from_yaw_pitch(double yawDeg, double pitchDeg) {
    double y = radians(yawDeg);
    double p = radians(pitchDeg);
    double cy = std::cos(y), sy = std::sin(y);
    double cp = std::cos(p), sp = std::sin(p);
    return { -sy * cp, -sp, cy * cp };
}

inline bool ray_aabb(const Vec3d& o, const Vec3d& d, const Box3d& b, double max_dist, double* out_t) {
    double tmin = 0.0, tmax = max_dist;
    for (int i = 0; i < 3; i++) {
        double oi = (i == 0) ? o.x : (i == 1) ? o.y : o.z;
        double di = (i == 0) ? d.x : (i == 1) ? d.y : d.z;
        double lo = (i == 0) ? b.minX : (i == 1) ? b.minY : b.minZ;
        double hi = (i == 0) ? b.maxX : (i == 1) ? b.maxY : b.maxZ;
        if (std::fabs(di) < 1e-9) {
            if (oi < lo || oi > hi) return false;
            continue;
        }
        double t1 = (lo - oi) / di;
        double t2 = (hi - oi) / di;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    if (out_t) *out_t = tmin;
    return tmin <= max_dist;
}

inline double box_center_y(const Box3d& b) { return (b.minY + b.maxY) * 0.5; }

inline bool world_to_screen(const Vec3d& cam_pos, double yawDeg, double pitchDeg,
                            double vfovDeg, int sw, int sh, const Vec3d& wp,
                            double& sx, double& sy) {
    Vec3d f = dir_from_yaw_pitch(yawDeg, pitchDeg);
    Vec3d r = { f.y * 0.0 - f.z * 1.0, f.z * 0.0 - f.x * 0.0, f.x * 1.0 - f.y * 0.0 };
    double rl = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
    if (rl < 1e-9) return false;
    r.x /= rl; r.y /= rl; r.z /= rl;
    Vec3d u = { r.y * f.z - r.z * f.y, r.z * f.x - r.x * f.z, r.x * f.y - r.y * f.x };
    Vec3d rel = { wp.x - cam_pos.x, wp.y - cam_pos.y, wp.z - cam_pos.z };
    double z = f.x * rel.x + f.y * rel.y + f.z * rel.z;
    if (z <= 0.05) return false;
    double x = r.x * rel.x + r.y * rel.y + r.z * rel.z;
    double y = u.x * rel.x + u.y * rel.y + u.z * rel.z;
    double scale = (sh * 0.5) / std::tan(radians(vfovDeg) * 0.5);
    sx = sw * 0.5 + (x / z) * scale;
    sy = sh * 0.5 - (y / z) * scale;
    return true;
}

inline void angles_to(const Vec3d& from, const Vec3d& to, float& yawDeg, float& pitchDeg) {
    double dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
    double horiz = std::sqrt(dx * dx + dz * dz);
    yawDeg = (float)degrees(std::atan2(-dx, dz));
    pitchDeg = (float)degrees(std::atan2(-dy, horiz));
}
