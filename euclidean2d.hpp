#pragma once

#include <cmath>

struct vec2
{
    float x, y;

    vec2 operator+(vec2 b) const { return { x + b.x, y + b.y }; }
    vec2 operator-() const { return { -x, -y }; }
    vec2 operator-(vec2 b) const { return *this + (-b); }
    vec2 operator*(float s) const { return { x * s, y * s }; }
};

inline vec2 operator*(float s, const vec2 &v)
{
    return v * s;
}

struct point2
{
    vec2 coords;

    explicit point2(float x = 0, float y = 0) : coords({ x, y }) {}
    explicit point2(const vec2 &v) : coords(v) {}

    vec2 operator-(const point2 &q) const { return coords - q.coords; }
    point2 operator+(const vec2 &v) const { return point2(coords + v); }
};

inline point2
operator+(const vec2 &v, const point2 &p)
{
    return p + v;
}

struct AffineTrafo2
{
    vec2 x, y;
    point2 origin;

    AffineTrafo2(vec2 x = { 1, 0 },
                 vec2 y = { 0, 1 },
                 point2 o = point2{ 0, 0 })
      : x(x), y(y), origin(o)
    {}

    vec2 operator()(const vec2 &v) const { return v.x * x + v.y * y; }

    point2 operator()(const point2 &p) const
    {
        return origin + operator()(p.coords);
    }

    static AffineTrafo2 rotation(float phi)
    {
        float c = std::cos(phi);
        float s = std::sin(phi);

        vec2 x = { c, s };
        vec2 y = { -s, c };

        return { x, y };
    }
};

inline AffineTrafo2 operator*(const AffineTrafo2 &T, const AffineTrafo2 &U)
{
    return { T(U.x), T(U.y), T(U.origin) };
}
