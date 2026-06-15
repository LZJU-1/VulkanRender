#pragma once

#include "render/SoftwareV1Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vr {

constexpr float kPi = 3.14159265358979323846f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 1.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

enum class MaterialKind {
    Simple,
    Environment,
    Mirror,
    Lambertian,
    Pbr,
    Emissive
};

inline float gpuMaterialKind(MaterialKind kind) {
    switch (kind) {
    case MaterialKind::Environment: return 1.0f;
    case MaterialKind::Mirror:      return 2.0f;
    case MaterialKind::Lambertian:  return 3.0f;
    case MaterialKind::Pbr:         return 4.0f;
    case MaterialKind::Emissive:    return 5.0f;
    case MaterialKind::Simple:
    default:                        return 0.0f;
    }
}

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct Triangle3 {
    Vec3 a;
    Vec3 b;
    Vec3 c;
    Color color;
    Vec3 normal{0.0f, 0.0f, 1.0f};
    Vec3 normalA{0.0f, 0.0f, 1.0f};
    Vec3 normalB{0.0f, 0.0f, 1.0f};
    Vec3 normalC{0.0f, 0.0f, 1.0f};
    Vec4 tangentA{};
    Vec4 tangentB{};
    Vec4 tangentC{};
    Vec2 uv{};
    Vec2 uvA{};
    Vec2 uvB{};
    Vec2 uvC{};
    Vec3 baseColor{1.0f, 1.0f, 1.0f};
    float roughness = 0.7f;
    float metalness = 0.0f;
    MaterialKind materialKind = MaterialKind::Simple;
    Vec3 emission{0.0f, 0.0f, 0.0f};
    bool hasAlbedoTexture = false;
    std::filesystem::path albedoTexturePath;
    std::filesystem::path roughnessTexturePath;
};

struct Mat4 {
    float m[4][4]{};
};

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Vertex2 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct CubeInstance {
    std::string name;
    Vec3 center;
    float size = 1.0f;
    Vec3 color;
    float spin = 1.0f;
};

struct Camera {
    Vec3 eye{0.0f, 1.1f, 5.7f};
    Vec3 target{0.0f, 0.55f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float fovY = 52.0f * kPi / 180.0f;
    float nearPlane = 0.05f;
    float farPlane = 40.0f;
};

// --- Vec3 operators ---
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }

inline Vec3 cross(Vec3 a, Vec3 b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline Vec3 normalize(Vec3 v) {
    const float len = length(v);
    return len <= 0.00001f ? Vec3{} : v * (1.0f / len);
}

inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a * (1.0f - t) + b * t; }
inline Vec3 clamp01(Vec3 v) { return { std::clamp(v.x, 0.f, 1.f), std::clamp(v.y, 0.f, 1.f), std::clamp(v.z, 0.f, 1.f) }; }
inline Vec3 reflect(Vec3 v, Vec3 n) { return v - n * (2.0f * dot(v, n)); }
inline float luminance(Vec3 c) { return c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f; }
inline Vec3 minVec(Vec3 a, Vec3 b) { return { std::min(a.x,b.x), std::min(a.y,b.y), std::min(a.z,b.z) }; }
inline Vec3 maxVec(Vec3 a, Vec3 b) { return { std::max(a.x,b.x), std::max(a.y,b.y), std::max(a.z,b.z) }; }

inline Vec3 colorToVec(Color c) { return { c.r/255.f, c.g/255.f, c.b/255.f }; }
inline Color vecToColor(Vec3 v) {
    v = clamp01(v);
    return { (uint8_t)(v.x*255.f+0.5f), (uint8_t)(v.y*255.f+0.5f), (uint8_t)(v.z*255.f+0.5f) };
}

inline Color toneMap(Vec3 hdr) {
    hdr = hdr * 1.35f;
    Vec3 m{ hdr.x/(1+hdr.x), hdr.y/(1+hdr.y), hdr.z/(1+hdr.z) };
    m = { std::pow(std::clamp(m.x,0.f,1.f), 1.f/2.2f), std::pow(std::clamp(m.y,0.f,1.f), 1.f/2.2f), std::pow(std::clamp(m.z,0.f,1.f), 1.f/2.2f) };
    return vecToColor(m);
}

// --- Quat ---
inline Quat normalize(Quat q) {
    float l = std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return l <= 0.00001f ? Quat{} : Quat{q.x/l, q.y/l, q.z/l, q.w/l};
}
inline Quat operator*(Quat q, float s) { return {q.x*s, q.y*s, q.z*s, q.w*s}; }
inline Quat operator+(Quat a, Quat b) { return {a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w}; }
inline float dot(Quat a, Quat b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

inline Quat slerp(Quat a, Quat b, float t) {
    a = normalize(a); b = normalize(b);
    float ct = dot(a, b);
    if (ct < 0) { b = b * -1.f; ct = -ct; }
    if (ct > 0.9995f) return normalize(a*(1-t) + b*t);
    float theta = std::acos(std::clamp(ct, -1.f, 1.f));
    float st = std::sin(theta);
    return normalize(a * (std::sin((1-t)*theta)/st) + b * (std::sin(t*theta)/st));
}

inline Vec3 rotateBy(Quat q, Vec3 v) {
    Vec3 u{q.x, q.y, q.z};
    return u*(2*dot(u,v)) + v*(q.w*q.w-dot(u,u)) + cross(u,v)*(2*q.w);
}

// --- Mat4 ---
inline Mat4 identity() {
    Mat4 o{}; for (int i=0;i<4;++i) o.m[i][i]=1; return o;
}
inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 o{};
    for (int r=0;r<4;++r) for (int c=0;c<4;++c) for (int k=0;k<4;++k) o.m[r][c] += a.m[r][k]*b.m[k][c];
    return o;
}
inline Vec3 transformPoint(const Mat4& m, Vec3 p) {
    return { m.m[0][0]*p.x+m.m[0][1]*p.y+m.m[0][2]*p.z+m.m[0][3],
             m.m[1][0]*p.x+m.m[1][1]*p.y+m.m[1][2]*p.z+m.m[1][3],
             m.m[2][0]*p.x+m.m[2][1]*p.y+m.m[2][2]*p.z+m.m[2][3] };
}
inline Vec3 transformVector(const Mat4& m, Vec3 v) {
    return { m.m[0][0]*v.x+m.m[0][1]*v.y+m.m[0][2]*v.z,
             m.m[1][0]*v.x+m.m[1][1]*v.y+m.m[1][2]*v.z,
             m.m[2][0]*v.x+m.m[2][1]*v.y+m.m[2][2]*v.z };
}
inline Mat4 composeTransform(Vec3 t, Quat r, Vec3 s) {
    float x=r.x, y=r.y, z=r.z, w=r.w;
    Mat4 o = identity();
    o.m[0][0]=(1-2*y*y-2*z*z)*s.x; o.m[0][1]=(2*x*y-2*z*w)*s.y; o.m[0][2]=(2*x*z+2*y*w)*s.z;
    o.m[1][0]=(2*x*y+2*z*w)*s.x; o.m[1][1]=(1-2*x*x-2*z*z)*s.y; o.m[1][2]=(2*y*z-2*x*w)*s.z;
    o.m[2][0]=(2*x*z-2*y*w)*s.x; o.m[2][1]=(2*y*z+2*x*w)*s.y; o.m[2][2]=(1-2*x*x-2*y*y)*s.z;
    o.m[0][3]=t.x; o.m[1][3]=t.y; o.m[2][3]=t.z;
    return o;
}

// --- Tangent ---
inline Vec4 makeTangent(Vec3 t, float handedness=1) {
    t = normalize(t);
    if (length(t)<=0.00001f) t={1,0,0};
    return {t.x, t.y, t.z, handedness<0?-1.f:1.f};
}
inline Vec4 triangleTangent(Vec3 a, Vec3 b, Vec3 c, Vec2 uvA, Vec2 uvB, Vec2 uvC, Vec3 n) {
    Vec3 e1=b-a, e2=c-a;
    Vec2 d1{uvB.x-uvA.x, uvB.y-uvA.y}, d2{uvC.x-uvA.x, uvC.y-uvA.y};
    float det = d1.x*d2.y - d1.y*d2.x;
    if (std::abs(det)>1e-7f) {
        Vec3 t = (e1*d2.y - e2*d1.y)*(1/det);
        t = t - n*dot(n,t);
        return makeTangent(t, det<0?-1.f:1.f);
    }
    Vec3 t = std::abs(n.z)<0.9f ? cross({0,0,1},n) : cross({0,1,0},n);
    return makeTangent(t, 1);
}

// --- Misc helpers ---
inline Color toColor(Vec3 c, float shade) {
    auto f = [shade](float v) { return (uint8_t)(std::clamp(v*shade,0.f,1.f)*255+0.5f); };
    return {f(c.x), f(c.y), f(c.z)};
}
inline Color mixColor(Color a, Color b, Color c, float shade) {
    auto f = [shade](uint32_t v) { return (uint8_t)(std::clamp((v/(3.f*255))*shade,0.f,1.f)*255+0.5f); };
    return {f((uint32_t)a.r+b.r+c.r), f((uint32_t)a.g+b.g+c.g), f((uint32_t)a.b+b.b+c.b)};
}
inline Vec3 proceduralEnvironmentRadiance(Vec3 dir) {
    dir = normalize(dir);
    float h = std::clamp(dir.z*0.5f+0.5f, 0.f, 1.f);
    Vec3 g{0.45f,0.40f,0.34f}, hc{1.10f,0.93f,0.72f}, s{0.36f,0.54f,0.92f};
    Vec3 c = lerp(g, lerp(hc, s, h), h);
    float sun = std::pow(std::max(0.f, dot(dir, normalize(Vec3{-0.30f,-0.62f,0.72f}))), 120.f);
    return c + Vec3{5.f,4.3f,3.3f}*sun;
}

// --- Camera conversion ---
inline V1CameraSettings toCameraSettings(const Camera& cam) {
    return {true, cam.eye.x,cam.eye.y,cam.eye.z, cam.target.x,cam.target.y,cam.target.z,
            cam.up.x,cam.up.y,cam.up.z, cam.fovY, cam.nearPlane, cam.farPlane};
}
inline Camera cameraFromSettings(const V1CameraSettings& s, const Camera& fb) {
    if (!s.enabled) return fb;
    Camera c;
    c.eye={s.eyeX,s.eyeY,s.eyeZ}; c.target={s.targetX,s.targetY,s.targetZ}; c.up={s.upX,s.upY,s.upZ};
    c.fovY=s.fovY>0?s.fovY:fb.fovY; c.nearPlane=s.nearPlane>0?s.nearPlane:fb.nearPlane; c.farPlane=s.farPlane>0?s.farPlane:fb.farPlane;
    return c;
}

} // namespace vr
