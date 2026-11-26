#pragma once

#include <algorithm>
#include <cmath>

namespace skyframe {

constexpr double kPi = 3.14159265358979323846;
constexpr double radians(double degrees) noexcept { return degrees * kPi / 180.0; }
constexpr double degrees(double radiansValue) noexcept { return radiansValue * 180.0 / kPi; }

template <typename T>
constexpr T clamp(T value, T low, T high) noexcept {
    return std::max(low, std::min(value, high));
}

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    constexpr Vec3() = default;
    constexpr Vec3(double xValue, double yValue, double zValue) : x(xValue), y(yValue), z(zValue) {}

    constexpr Vec3 operator+(const Vec3& rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    constexpr Vec3 operator-(const Vec3& rhs) const noexcept { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    constexpr Vec3 operator-() const noexcept { return {-x, -y, -z}; }
    constexpr Vec3 operator*(double scalar) const noexcept { return {x * scalar, y * scalar, z * scalar}; }
    constexpr Vec3 operator/(double scalar) const noexcept { return scalar == 0.0 ? Vec3{} : Vec3{x / scalar, y / scalar, z / scalar}; }
    Vec3& operator+=(const Vec3& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    Vec3& operator-=(const Vec3& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    Vec3& operator*=(double scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; return *this; }

    [[nodiscard]] constexpr double lengthSquared() const noexcept { return x * x + y * y + z * z; }
    [[nodiscard]] double length() const noexcept { return std::sqrt(lengthSquared()); }
    [[nodiscard]] Vec3 normalized() const noexcept {
        const double magnitude = length();
        return magnitude > 1.0e-9 ? *this / magnitude : Vec3{};
    }
    [[nodiscard]] bool finite() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
};

constexpr Vec3 operator*(double scalar, const Vec3& value) noexcept { return value * scalar; }
constexpr double dot(const Vec3& a, const Vec3& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

struct Quat {
    double w{1.0};
    double x{0.0};
    double y{0.0};
    double z{0.0};

    [[nodiscard]] static Quat identity() noexcept { return {}; }
    [[nodiscard]] static Quat axisAngle(const Vec3& axis, double angleRadians) noexcept {
        const Vec3 normal = axis.normalized();
        const double half = angleRadians * 0.5;
        const double sine = std::sin(half);
        return {std::cos(half), normal.x * sine, normal.y * sine, normal.z * sine};
    }
    [[nodiscard]] Quat conjugate() const noexcept { return {w, -x, -y, -z}; }
    [[nodiscard]] Quat normalized() const noexcept {
        const double magnitude = std::sqrt(w * w + x * x + y * y + z * z);
        if (magnitude < 1.0e-12) return {};
        return {w / magnitude, x / magnitude, y / magnitude, z / magnitude};
    }
    [[nodiscard]] Quat operator*(const Quat& rhs) const noexcept {
        return {
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w
        };
    }
    [[nodiscard]] Vec3 rotate(const Vec3& value) const noexcept {
        const Vec3 qv{x, y, z};
        const Vec3 t = 2.0 * cross(qv, value);
        return value + w * t + cross(qv, t);
    }
    [[nodiscard]] Vec3 inverseRotate(const Vec3& value) const noexcept { return conjugate().rotate(value); }
    void integrateLocalAngularVelocity(const Vec3& omega, double dt) noexcept {
        const double speed = omega.length();
        if (speed > 1.0e-9) *this = ((*this) * axisAngle(omega / speed, speed * dt)).normalized();
    }
    void toAxisAngle(Vec3& axis, double& angle) const noexcept {
        const Quat q = normalized();
        angle = 2.0 * std::acos(clamp(q.w, -1.0, 1.0));
        const double s = std::sqrt(std::max(0.0, 1.0 - q.w * q.w));
        axis = s < 1.0e-8 ? Vec3{0.0, 1.0, 0.0} : Vec3{q.x / s, q.y / s, q.z / s};
    }
};

inline double wrapDegrees(double value) noexcept {
    value = std::fmod(value, 360.0);
    return value < 0.0 ? value + 360.0 : value;
}

} // namespace skyframe
