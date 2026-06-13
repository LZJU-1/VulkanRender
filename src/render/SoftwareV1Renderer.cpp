#include "render/SoftwareV1Renderer.hpp"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

namespace vr {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

enum class MaterialKind {
    Simple,
    Environment,
    Mirror,
    Lambertian,
    Pbr
};

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
    Vec2 uv{};
    Vec3 baseColor{1.0f, 1.0f, 1.0f};
    float roughness = 0.7f;
    float metalness = 0.0f;
    MaterialKind materialKind = MaterialKind::Simple;
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

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float length(Vec3 v) {
    return std::sqrt(dot(v, v));
}

Vec3 normalize(Vec3 v) {
    const float len = length(v);
    if (len <= 0.00001f) {
        return {};
    }
    return v * (1.0f / len);
}

Vec3 lerp(Vec3 a, Vec3 b, float t) {
    return a * (1.0f - t) + b * t;
}

Vec3 clamp01(Vec3 v) {
    return {
        std::clamp(v.x, 0.0f, 1.0f),
        std::clamp(v.y, 0.0f, 1.0f),
        std::clamp(v.z, 0.0f, 1.0f),
    };
}

Vec3 reflect(Vec3 v, Vec3 n) {
    return v - n * (2.0f * dot(v, n));
}

float luminance(Vec3 color) {
    return color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
}

Vec3 colorToVec(Color color) {
    return {
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
    };
}

Color vecToColor(Vec3 color) {
    const Vec3 mapped = clamp01(color);
    return {
        static_cast<std::uint8_t>(mapped.x * 255.0f + 0.5f),
        static_cast<std::uint8_t>(mapped.y * 255.0f + 0.5f),
        static_cast<std::uint8_t>(mapped.z * 255.0f + 0.5f),
    };
}

Color toneMap(Vec3 hdr) {
    hdr = hdr * 1.35f;
    const Vec3 mapped{
        hdr.x / (1.0f + hdr.x),
        hdr.y / (1.0f + hdr.y),
        hdr.z / (1.0f + hdr.z),
    };
    const Vec3 gamma{
        std::pow(std::clamp(mapped.x, 0.0f, 1.0f), 1.0f / 2.2f),
        std::pow(std::clamp(mapped.y, 0.0f, 1.0f), 1.0f / 2.2f),
        std::pow(std::clamp(mapped.z, 0.0f, 1.0f), 1.0f / 2.2f),
    };
    return vecToColor(gamma);
}

Quat normalize(Quat q) {
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len <= 0.00001f) {
        return {};
    }
    return {q.x / len, q.y / len, q.z / len, q.w / len};
}

Quat operator*(Quat q, float s) {
    return {q.x * s, q.y * s, q.z * s, q.w * s};
}

Quat operator+(Quat a, Quat b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

float dot(Quat a, Quat b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Quat slerp(Quat a, Quat b, float t) {
    a = normalize(a);
    b = normalize(b);
    float cosTheta = dot(a, b);
    if (cosTheta < 0.0f) {
        b = b * -1.0f;
        cosTheta = -cosTheta;
    }
    if (cosTheta > 0.9995f) {
        return normalize(a * (1.0f - t) + b * t);
    }
    const float theta = std::acos(std::clamp(cosTheta, -1.0f, 1.0f));
    const float sinTheta = std::sin(theta);
    return normalize(a * (std::sin((1.0f - t) * theta) / sinTheta) + b * (std::sin(t * theta) / sinTheta));
}

Vec3 minVec(Vec3 a, Vec3 b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

Vec3 maxVec(Vec3 a, Vec3 b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

Mat4 identity() {
    Mat4 out{};
    for (int i = 0; i < 4; ++i) {
        out.m[i][i] = 1.0f;
    }
    return out;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            for (int k = 0; k < 4; ++k) {
                out.m[r][c] += a.m[r][k] * b.m[k][c];
            }
        }
    }
    return out;
}

Vec3 transformPoint(const Mat4& m, Vec3 p) {
    return {
        m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3],
        m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3],
        m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3],
    };
}

Vec3 transformVector(const Mat4& m, Vec3 v) {
    return {
        m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z,
        m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z,
        m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z,
    };
}

Vec3 rotateBy(Quat q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    return u * (2.0f * dot(u, v)) + v * (q.w * q.w - dot(u, u)) + cross(u, v) * (2.0f * q.w);
}

Mat4 composeTransform(Vec3 translation, Quat rotation, Vec3 scale) {
    const float x = rotation.x;
    const float y = rotation.y;
    const float z = rotation.z;
    const float w = rotation.w;

    Mat4 out = identity();
    out.m[0][0] = (1.0f - 2.0f * y * y - 2.0f * z * z) * scale.x;
    out.m[0][1] = (2.0f * x * y - 2.0f * z * w) * scale.y;
    out.m[0][2] = (2.0f * x * z + 2.0f * y * w) * scale.z;
    out.m[1][0] = (2.0f * x * y + 2.0f * z * w) * scale.x;
    out.m[1][1] = (1.0f - 2.0f * x * x - 2.0f * z * z) * scale.y;
    out.m[1][2] = (2.0f * y * z - 2.0f * x * w) * scale.z;
    out.m[2][0] = (2.0f * x * z - 2.0f * y * w) * scale.x;
    out.m[2][1] = (2.0f * y * z + 2.0f * x * w) * scale.y;
    out.m[2][2] = (1.0f - 2.0f * x * x - 2.0f * y * y) * scale.z;
    out.m[0][3] = translation.x;
    out.m[1][3] = translation.y;
    out.m[2][3] = translation.z;
    return out;
}

Color toColor(Vec3 color, float shade) {
    const auto convert = [shade](float value) {
        const float mapped = std::clamp(value * shade, 0.0f, 1.0f) * 255.0f;
        return static_cast<std::uint8_t>(mapped + 0.5f);
    };
    return {convert(color.x), convert(color.y), convert(color.z)};
}

Color mixColor(Color a, Color b, Color c, float shade) {
    const auto convert = [shade](std::uint32_t value) {
        const float averaged = static_cast<float>(value) / (3.0f * 255.0f);
        return static_cast<std::uint8_t>(std::clamp(averaged * shade, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return {
        convert(static_cast<std::uint32_t>(a.r) + b.r + c.r),
        convert(static_cast<std::uint32_t>(a.g) + b.g + c.g),
        convert(static_cast<std::uint32_t>(a.b) + b.b + c.b),
    };
}

Vec3 proceduralEnvironmentRadiance(Vec3 direction) {
    direction = normalize(direction);
    const float horizon = std::clamp(direction.z * 0.5f + 0.5f, 0.0f, 1.0f);
    const Vec3 ground{0.45f, 0.40f, 0.34f};
    const Vec3 horizonColor{1.10f, 0.93f, 0.72f};
    const Vec3 sky{0.36f, 0.54f, 0.92f};
    Vec3 color = lerp(ground, lerp(horizonColor, sky, horizon), horizon);
    const float sun = std::pow(std::max(0.0f, dot(direction, normalize(Vec3{-0.30f, -0.62f, 0.72f}))), 120.0f);
    return color + Vec3{5.0f, 4.3f, 3.3f} * sun;
}

V1CameraSettings toCameraSettings(const Camera& camera) {
    return {
        true,
        camera.eye.x,
        camera.eye.y,
        camera.eye.z,
        camera.target.x,
        camera.target.y,
        camera.target.z,
        camera.up.x,
        camera.up.y,
        camera.up.z,
        camera.fovY,
        camera.nearPlane,
        camera.farPlane,
    };
}

Camera cameraFromSettings(const V1CameraSettings& settings, const Camera& fallback) {
    if (!settings.enabled) {
        return fallback;
    }
    Camera camera;
    camera.eye = {settings.eyeX, settings.eyeY, settings.eyeZ};
    camera.target = {settings.targetX, settings.targetY, settings.targetZ};
    camera.up = {settings.upX, settings.upY, settings.upZ};
    camera.fovY = settings.fovY > 0.0f ? settings.fovY : fallback.fovY;
    camera.nearPlane = settings.nearPlane > 0.0f ? settings.nearPlane : fallback.nearPlane;
    camera.farPlane = settings.farPlane > 0.0f ? settings.farPlane : fallback.farPlane;
    return camera;
}

class Json {
public:
    enum class Type {
        Null,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    double number = 0.0;
    std::string string;
    std::vector<Json> array;
    std::map<std::string, Json> object;

    const Json* find(const std::string& key) const {
        const auto found = object.find(key);
        return found == object.end() ? nullptr : &found->second;
    }

    double numberOr(double fallback) const {
        return type == Type::Number ? number : fallback;
    }

    std::string stringOr(std::string fallback) const {
        return type == Type::String ? string : fallback;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    Json parse() {
        Json value = parseValue();
        skipWhitespace();
        if (position_ != text_.size()) {
            throw std::runtime_error("Unexpected trailing JSON data");
        }
        return value;
    }

private:
    Json parseValue() {
        skipWhitespace();
        if (position_ >= text_.size()) {
            throw std::runtime_error("Unexpected end of JSON");
        }

        const char ch = text_[position_];
        if (ch == '"') {
            Json value;
            value.type = Json::Type::String;
            value.string = parseString();
            return value;
        }
        if (ch == '[') {
            return parseArray();
        }
        if (ch == '{') {
            return parseObject();
        }
        if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+') {
            return parseNumber();
        }
        if (text_.compare(position_, 4, "null") == 0) {
            position_ += 4;
            return {};
        }
        throw std::runtime_error("Unexpected JSON token");
    }

    Json parseArray() {
        expect('[');
        Json value;
        value.type = Json::Type::Array;
        skipWhitespace();
        if (peek(']')) {
            ++position_;
            return value;
        }
        while (true) {
            value.array.push_back(parseValue());
            skipWhitespace();
            if (peek(']')) {
                ++position_;
                break;
            }
            expect(',');
        }
        return value;
    }

    Json parseObject() {
        expect('{');
        Json value;
        value.type = Json::Type::Object;
        skipWhitespace();
        if (peek('}')) {
            ++position_;
            return value;
        }
        while (true) {
            skipWhitespace();
            std::string key = parseString();
            expect(':');
            value.object.emplace(std::move(key), parseValue());
            skipWhitespace();
            if (peek('}')) {
                ++position_;
                break;
            }
            expect(',');
        }
        return value;
    }

    Json parseNumber() {
        const std::size_t begin = position_;
        while (position_ < text_.size()) {
            const char ch = text_[position_];
            if (!((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) {
                break;
            }
            ++position_;
        }

        Json value;
        value.type = Json::Type::Number;
        value.number = std::stod(text_.substr(begin, position_ - begin));
        return value;
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (position_ < text_.size()) {
            const char ch = text_[position_++];
            if (ch == '"') {
                return out;
            }
            if (ch == '\\') {
                if (position_ >= text_.size()) {
                    throw std::runtime_error("Invalid JSON escape");
                }
                const char escaped = text_[position_++];
                switch (escaped) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: throw std::runtime_error("Unsupported JSON escape");
                }
            } else {
                out.push_back(ch);
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    }

    void skipWhitespace() {
        while (position_ < text_.size()) {
            const char ch = text_[position_];
            if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
                break;
            }
            ++position_;
        }
    }

    bool peek(char ch) const {
        return position_ < text_.size() && text_[position_] == ch;
    }

    void expect(char ch) {
        skipWhitespace();
        if (!peek(ch)) {
            throw std::runtime_error(std::string("Expected JSON token '") + ch + "'");
        }
        ++position_;
    }

    std::string text_;
    std::size_t position_ = 0;
};

struct S72Attribute {
    std::filesystem::path src;
    std::uint32_t offset = 0;
    std::uint32_t stride = 0;
    std::string format;
};

struct S72Mesh {
    std::string name;
    std::uint32_t count = 0;
    S72Attribute position;
    S72Attribute normal;
    S72Attribute tangent;
    S72Attribute texcoord;
    S72Attribute color;
    int material = -1;
};

struct S72Material {
    std::string name;
    MaterialKind kind = MaterialKind::Simple;
    Vec3 albedo{1.0f, 1.0f, 1.0f};
    std::filesystem::path albedoTexture;
    float roughness = 0.7f;
    std::filesystem::path roughnessTexture;
    float metalness = 0.0f;
};

struct S72Node {
    std::string name;
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Quat rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    int mesh = -1;
    int camera = -1;
    std::vector<int> children;
};

struct S72Camera {
    float vfov = 52.0f * kPi / 180.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

struct S72Driver {
    int node = -1;
    std::string channel;
    std::string interpolation = "LINEAR";
    std::vector<float> times;
    std::vector<float> values;
};

struct S72Scene {
    std::vector<S72Mesh> meshes;
    std::vector<S72Material> materials;
    std::vector<S72Node> nodes;
    std::vector<S72Camera> cameras;
    std::vector<S72Driver> drivers;
    std::vector<int> roots;
    std::vector<Triangle3> triangles;
    Camera camera;
    std::filesystem::path environmentTexture;
    bool hasCamera = false;
};

struct Texture2D {
    int width = 0;
    int height = 0;
    std::vector<Vec3> pixels;

    bool empty() const {
        return width <= 0 || height <= 0 || pixels.empty();
    }

    Vec3 sample(Vec2 uv) const {
        if (empty()) {
            return {1.0f, 1.0f, 1.0f};
        }
        const float u = uv.x - std::floor(uv.x);
        const float v = uv.y - std::floor(uv.y);
        const int x = std::clamp(static_cast<int>(u * static_cast<float>(width)), 0, width - 1);
        const int y = std::clamp(static_cast<int>((1.0f - v) * static_cast<float>(height)), 0, height - 1);
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    }
};

Texture2D loadTexture(const std::filesystem::path& path) {
    Texture2D texture;
    int channels = 0;
    stbi_uc* data = stbi_load(path.string().c_str(), &texture.width, &texture.height, &channels, 4);
    if (!data) {
        throw std::runtime_error("Could not load texture: " + path.string());
    }
    texture.pixels.resize(static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height));
    for (int y = 0; y < texture.height; ++y) {
        for (int x = 0; x < texture.width; ++x) {
            const std::size_t src = (static_cast<std::size_t>(y) * static_cast<std::size_t>(texture.width) + static_cast<std::size_t>(x)) * 4u;
            texture.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(texture.width) + static_cast<std::size_t>(x)] = {
                static_cast<float>(data[src + 0u]) / 255.0f,
                static_cast<float>(data[src + 1u]) / 255.0f,
                static_cast<float>(data[src + 2u]) / 255.0f,
            };
        }
    }
    stbi_image_free(data);
    return texture;
}

const Texture2D& cachedTexture(
    std::map<std::filesystem::path, Texture2D>& cache,
    const std::filesystem::path& basePath,
    const std::filesystem::path& relativePath
) {
    const std::filesystem::path resolved = relativePath.is_absolute() ? relativePath : basePath / relativePath;
    const auto found = cache.find(resolved);
    if (found != cache.end()) {
        return found->second;
    }
    return cache.emplace(resolved, loadTexture(resolved)).first->second;
}

Vec3 environmentRadiance(Vec3 direction, const Texture2D* texture = nullptr) {
    direction = normalize(direction);
    if (texture && !texture->empty()) {
        const float u = std::atan2(direction.y, direction.x) / (2.0f * kPi) + 0.5f;
        const float v = std::acos(std::clamp(direction.z, -1.0f, 1.0f)) / kPi;
        return texture->sample({u, 1.0f - v}) * 2.2f;
    }
    return proceduralEnvironmentRadiance(direction);
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open text file: " + path.string());
    }
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open binary file: " + path.string());
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

float readF32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("Scene'72 attribute read is out of bounds");
    }
    float value = 0.0f;
    std::memcpy(&value, bytes.data() + offset, sizeof(float));
    return value;
}

std::uint32_t toU32(const Json* value, std::uint32_t fallback = 0) {
    return value && value->type == Json::Type::Number ? static_cast<std::uint32_t>(value->number) : fallback;
}

int toRef(const Json* value) {
    return value && value->type == Json::Type::Number ? static_cast<int>(value->number) : -1;
}

Vec3 toVec3(const Json* value, Vec3 fallback) {
    if (!value || value->type != Json::Type::Array || value->array.size() < 3) {
        return fallback;
    }
    return {
        static_cast<float>(value->array[0].numberOr(fallback.x)),
        static_cast<float>(value->array[1].numberOr(fallback.y)),
        static_cast<float>(value->array[2].numberOr(fallback.z)),
    };
}

Quat toQuat(const Json* value, Quat fallback = {}) {
    if (!value || value->type != Json::Type::Array || value->array.size() < 4) {
        return fallback;
    }
    return {
        static_cast<float>(value->array[0].numberOr(fallback.x)),
        static_cast<float>(value->array[1].numberOr(fallback.y)),
        static_cast<float>(value->array[2].numberOr(fallback.z)),
        static_cast<float>(value->array[3].numberOr(fallback.w)),
    };
}

std::vector<int> toRefs(const Json* value) {
    std::vector<int> out;
    if (!value || value->type != Json::Type::Array) {
        return out;
    }
    for (const Json& item : value->array) {
        if (item.type == Json::Type::Number) {
            out.push_back(static_cast<int>(item.number));
        }
    }
    return out;
}

std::vector<float> toFloats(const Json* value) {
    std::vector<float> out;
    if (!value || value->type != Json::Type::Array) {
        return out;
    }
    out.reserve(value->array.size());
    for (const Json& item : value->array) {
        if (item.type == Json::Type::Number) {
            out.push_back(static_cast<float>(item.number));
        }
    }
    return out;
}

Vec2 toVec2(const Json* value, Vec2 fallback = {}) {
    if (!value || value->type != Json::Type::Array || value->array.size() < 2) {
        return fallback;
    }
    return {
        static_cast<float>(value->array[0].numberOr(fallback.x)),
        static_cast<float>(value->array[1].numberOr(fallback.y)),
    };
}

std::filesystem::path textureSource(const Json* value) {
    if (!value || value->type != Json::Type::Object) {
        return {};
    }
    if (const Json* src = value->find("src")) {
        return src->stringOr("");
    }
    return {};
}

Vec3 materialColorOrTexture(const Json* value, Vec3 fallback, std::filesystem::path& texture) {
    if (!value) {
        return fallback;
    }
    if (value->type == Json::Type::Array) {
        return toVec3(value, fallback);
    }
    if (value->type == Json::Type::Object) {
        texture = textureSource(value);
    }
    return fallback;
}

float materialScalarOrTexture(const Json* value, float fallback, std::filesystem::path& texture) {
    if (!value) {
        return fallback;
    }
    if (value->type == Json::Type::Number) {
        return static_cast<float>(value->number);
    }
    if (value->type == Json::Type::Object) {
        texture = textureSource(value);
    }
    return fallback;
}

S72Attribute parseAttribute(const Json* value) {
    S72Attribute attr;
    if (!value || value->type != Json::Type::Object) {
        return attr;
    }
    if (const Json* src = value->find("src")) {
        attr.src = src->stringOr("");
    }
    attr.offset = toU32(value->find("offset"));
    attr.stride = toU32(value->find("stride"));
    if (const Json* format = value->find("format")) {
        attr.format = format->stringOr("");
    }
    return attr;
}

S72Mesh parseMesh(const Json& object) {
    S72Mesh mesh;
    if (const Json* name = object.find("name")) {
        mesh.name = name->stringOr("");
    }
    mesh.count = toU32(object.find("count"));
    const Json* attributes = object.find("attributes");
    if (attributes && attributes->type == Json::Type::Object) {
        mesh.position = parseAttribute(attributes->find("POSITION"));
        mesh.normal = parseAttribute(attributes->find("NORMAL"));
        mesh.tangent = parseAttribute(attributes->find("TANGENT"));
        mesh.texcoord = parseAttribute(attributes->find("TEXCOORD"));
        mesh.color = parseAttribute(attributes->find("COLOR"));
    }
    mesh.material = toRef(object.find("material"));
    return mesh;
}

S72Material parseMaterial(const Json& object) {
    S72Material material;
    if (const Json* name = object.find("name")) {
        material.name = name->stringOr("");
    }

    if (object.find("environment")) {
        material.kind = MaterialKind::Environment;
    } else if (object.find("mirror")) {
        material.kind = MaterialKind::Mirror;
    } else if (const Json* lambertian = object.find("lambertian")) {
        material.kind = MaterialKind::Lambertian;
        if (lambertian->type == Json::Type::Object) {
            material.albedo = materialColorOrTexture(lambertian->find("albedo"), material.albedo, material.albedoTexture);
        }
    } else if (const Json* pbr = object.find("pbr")) {
        material.kind = MaterialKind::Pbr;
        if (pbr->type == Json::Type::Object) {
            material.albedo = materialColorOrTexture(pbr->find("albedo"), material.albedo, material.albedoTexture);
            material.roughness = materialScalarOrTexture(pbr->find("roughness"), material.roughness, material.roughnessTexture);
            if (const Json* metalness = pbr->find("metalness")) {
                material.metalness = static_cast<float>(metalness->numberOr(material.metalness));
            }
        }
    }

    material.roughness = std::clamp(material.roughness, 0.03f, 1.0f);
    material.metalness = std::clamp(material.metalness, 0.0f, 1.0f);
    return material;
}

std::filesystem::path parseEnvironmentTexture(const Json& object) {
    const Json* radiance = object.find("radiance");
    if (!radiance || radiance->type != Json::Type::Object) {
        return {};
    }
    return textureSource(radiance);
}

S72Node parseNode(const Json& object) {
    S72Node node;
    if (const Json* name = object.find("name")) {
        node.name = name->stringOr("");
    }
    node.translation = toVec3(object.find("translation"), node.translation);
    node.rotation = toQuat(object.find("rotation"), node.rotation);
    node.scale = toVec3(object.find("scale"), node.scale);
    node.mesh = toRef(object.find("mesh"));
    node.camera = toRef(object.find("camera"));
    node.children = toRefs(object.find("children"));
    return node;
}

S72Camera parseCamera(const Json& object) {
    S72Camera camera;
    const Json* perspective = object.find("perspective");
    if (perspective && perspective->type == Json::Type::Object) {
        if (const Json* vfov = perspective->find("vfov")) {
            camera.vfov = static_cast<float>(vfov->numberOr(camera.vfov));
        }
        if (const Json* nearPlane = perspective->find("near")) {
            camera.nearPlane = static_cast<float>(nearPlane->numberOr(camera.nearPlane));
        }
        if (const Json* farPlane = perspective->find("far")) {
            camera.farPlane = static_cast<float>(farPlane->numberOr(camera.farPlane));
        }
    }
    return camera;
}

S72Driver parseDriver(const Json& object) {
    S72Driver driver;
    driver.node = toRef(object.find("node"));
    if (const Json* channel = object.find("channel")) {
        driver.channel = channel->stringOr("");
    }
    if (const Json* interpolation = object.find("interpolation")) {
        driver.interpolation = interpolation->stringOr("LINEAR");
    }
    driver.times = toFloats(object.find("times"));
    driver.values = toFloats(object.find("values"));
    return driver;
}

float animationDuration(const std::vector<S72Driver>& drivers) {
    float duration = 0.0f;
    for (const S72Driver& driver : drivers) {
        if (!driver.times.empty()) {
            duration = std::max(duration, driver.times.back());
        }
    }
    return duration;
}

float animationTimeForFrame(std::uint32_t frameIndex, const std::vector<S72Driver>& drivers) {
    const float duration = animationDuration(drivers);
    if (duration <= 0.0f) {
        return 0.0f;
    }
    const float seconds = static_cast<float>(frameIndex) / 24.0f;
    return std::fmod(seconds, duration);
}

std::size_t keyframeIndex(const std::vector<float>& times, float time) {
    if (times.size() < 2 || time <= times.front()) {
        return 0;
    }
    for (std::size_t i = 0; i + 1 < times.size(); ++i) {
        if (time < times[i + 1]) {
            return i;
        }
    }
    return times.size() - 1;
}

Vec3 sampleVec3(const S72Driver& driver, float time) {
    if (driver.times.empty() || driver.values.size() < 3) {
        return {};
    }
    const std::size_t i = keyframeIndex(driver.times, time);
    const std::size_t aIndex = std::min(i, driver.times.size() - 1) * 3;
    const Vec3 a{driver.values[aIndex + 0], driver.values[aIndex + 1], driver.values[aIndex + 2]};
    if (i + 1 >= driver.times.size() || driver.interpolation == "STEP") {
        return a;
    }
    const std::size_t bIndex = (i + 1) * 3;
    const Vec3 b{driver.values[bIndex + 0], driver.values[bIndex + 1], driver.values[bIndex + 2]};
    const float interval = std::max(0.00001f, driver.times[i + 1] - driver.times[i]);
    const float t = std::clamp((time - driver.times[i]) / interval, 0.0f, 1.0f);
    return a * (1.0f - t) + b * t;
}

Quat sampleQuat(const S72Driver& driver, float time) {
    if (driver.times.empty() || driver.values.size() < 4) {
        return {};
    }
    const std::size_t i = keyframeIndex(driver.times, time);
    const std::size_t aIndex = std::min(i, driver.times.size() - 1) * 4;
    const Quat a{driver.values[aIndex + 0], driver.values[aIndex + 1], driver.values[aIndex + 2], driver.values[aIndex + 3]};
    if (i + 1 >= driver.times.size() || driver.interpolation == "STEP") {
        return normalize(a);
    }
    const std::size_t bIndex = (i + 1) * 4;
    const Quat b{driver.values[bIndex + 0], driver.values[bIndex + 1], driver.values[bIndex + 2], driver.values[bIndex + 3]};
    const float interval = std::max(0.00001f, driver.times[i + 1] - driver.times[i]);
    const float t = std::clamp((time - driver.times[i]) / interval, 0.0f, 1.0f);
    if (driver.interpolation == "SLERP") {
        return slerp(a, b, t);
    }
    return normalize(a * (1.0f - t) + b * t);
}

void applyDrivers(std::vector<S72Node>& nodes, const std::vector<S72Driver>& drivers, float time) {
    for (const S72Driver& driver : drivers) {
        if (driver.node < 0 || static_cast<std::size_t>(driver.node) >= nodes.size()) {
            continue;
        }
        S72Node& node = nodes[static_cast<std::size_t>(driver.node)];
        if (driver.channel == "translation") {
            node.translation = sampleVec3(driver, time);
        } else if (driver.channel == "scale") {
            node.scale = sampleVec3(driver, time);
        } else if (driver.channel == "rotation") {
            node.rotation = sampleQuat(driver, time);
        }
    }
}

void appendMeshTriangles(
    const S72Mesh& mesh,
    const Mat4& transform,
    const std::filesystem::path& basePath,
    const std::vector<S72Material>& materials,
    std::map<std::filesystem::path, Texture2D>& textureCache,
    std::vector<Triangle3>& triangles
) {
    if (mesh.count < 3 || mesh.position.src.empty()) {
        return;
    }
    if (mesh.position.format != "R32G32B32_SFLOAT") {
        throw std::runtime_error("Scene'72 loader requires POSITION float3");
    }
    if (!mesh.color.src.empty() && mesh.color.format != "R8G8B8A8_UNORM") {
        throw std::runtime_error("Scene'72 loader currently supports COLOR rgba8 only");
    }
    if (!mesh.normal.src.empty() && mesh.normal.format != "R32G32B32_SFLOAT") {
        throw std::runtime_error("Scene'72 loader currently supports NORMAL float3 only");
    }
    if (!mesh.texcoord.src.empty() && mesh.texcoord.format != "R32G32_SFLOAT") {
        throw std::runtime_error("Scene'72 loader currently supports TEXCOORD float2 only");
    }

    const std::filesystem::path positionPath = basePath / mesh.position.src;
    const std::vector<std::uint8_t> positionBytes = readBinaryFile(positionPath);

    const std::filesystem::path colorPath = basePath / mesh.color.src;
    const std::filesystem::path normalPath = basePath / mesh.normal.src;
    const std::filesystem::path texcoordPath = basePath / mesh.texcoord.src;
    const std::vector<std::uint8_t> colorBytes = mesh.color.src.empty()
        ? std::vector<std::uint8_t>{}
        : (colorPath == positionPath ? positionBytes : readBinaryFile(colorPath));
    const std::vector<std::uint8_t> normalBytes = mesh.normal.src.empty()
        ? std::vector<std::uint8_t>{}
        : (normalPath == positionPath ? positionBytes : readBinaryFile(normalPath));
    const std::vector<std::uint8_t> texcoordBytes = mesh.texcoord.src.empty()
        ? std::vector<std::uint8_t>{}
        : (texcoordPath == positionPath ? positionBytes : readBinaryFile(texcoordPath));

    std::vector<Vec3> positions(mesh.count);
    std::vector<Color> colors(mesh.count);
    std::vector<Vec3> normals(mesh.count);
    std::vector<Vec2> uvs(mesh.count);
    for (std::uint32_t i = 0; i < mesh.count; ++i) {
        const std::size_t p = static_cast<std::size_t>(mesh.position.offset) + static_cast<std::size_t>(i) * mesh.position.stride;
        positions[i] = transformPoint(transform, {readF32(positionBytes, p + 0), readF32(positionBytes, p + 4), readF32(positionBytes, p + 8)});

        colors[i] = {230, 230, 230};
        if (!mesh.color.src.empty()) {
            const std::size_t c = static_cast<std::size_t>(mesh.color.offset) + static_cast<std::size_t>(i) * mesh.color.stride;
            if (c + 4 > colorBytes.size()) {
                throw std::runtime_error("Scene'72 color attribute read is out of bounds");
            }
            colors[i] = {colorBytes[c + 0], colorBytes[c + 1], colorBytes[c + 2]};
        }

        normals[i] = {0.0f, 0.0f, 1.0f};
        if (!mesh.normal.src.empty()) {
            const std::size_t n = static_cast<std::size_t>(mesh.normal.offset) + static_cast<std::size_t>(i) * mesh.normal.stride;
            normals[i] = normalize(transformVector(transform, {readF32(normalBytes, n + 0), readF32(normalBytes, n + 4), readF32(normalBytes, n + 8)}));
        }

        if (!mesh.texcoord.src.empty()) {
            const std::size_t t = static_cast<std::size_t>(mesh.texcoord.offset) + static_cast<std::size_t>(i) * mesh.texcoord.stride;
            uvs[i] = {readF32(texcoordBytes, t + 0), readF32(texcoordBytes, t + 4)};
        }
    }

    S72Material material;
    if (mesh.material >= 0 && static_cast<std::size_t>(mesh.material) < materials.size()) {
        material = materials[static_cast<std::size_t>(mesh.material)];
    }

    for (std::uint32_t i = 0; i + 2 < mesh.count; i += 3) {
        const Color vertexColor = mixColor(colors[i], colors[i + 1], colors[i + 2], 1.0f);
        const Vec2 uv{
            (uvs[i].x + uvs[i + 1].x + uvs[i + 2].x) / 3.0f,
            (uvs[i].y + uvs[i + 1].y + uvs[i + 2].y) / 3.0f,
        };
        Vec3 baseColor = material.kind == MaterialKind::Simple ? colorToVec(vertexColor) : material.albedo;
        if (!material.albedoTexture.empty()) {
            baseColor = cachedTexture(textureCache, basePath, material.albedoTexture).sample(uv);
        }

        float roughness = material.roughness;
        if (!material.roughnessTexture.empty()) {
            roughness = std::clamp(luminance(cachedTexture(textureCache, basePath, material.roughnessTexture).sample(uv)), 0.03f, 1.0f);
        }

        Triangle3 tri;
        tri.a = positions[i];
        tri.b = positions[i + 1];
        tri.c = positions[i + 2];
        tri.color = vertexColor;
        tri.normal = normalize(normals[i] + normals[i + 1] + normals[i + 2]);
        if (length(tri.normal) <= 0.00001f) {
            tri.normal = normalize(cross(tri.b - tri.a, tri.c - tri.a));
        }
        tri.uv = uv;
        tri.baseColor = baseColor;
        tri.roughness = roughness;
        tri.metalness = material.metalness;
        tri.materialKind = material.kind;
        triangles.push_back(tri);
    }
}

void traverseS72Node(
    S72Scene& scene,
    const std::filesystem::path& basePath,
    std::map<std::filesystem::path, Texture2D>& textureCache,
    int objectIndex,
    const Mat4& parent
) {
    if (objectIndex < 0 || static_cast<std::size_t>(objectIndex) >= scene.nodes.size()) {
        return;
    }

    const S72Node& node = scene.nodes[static_cast<std::size_t>(objectIndex)];
    const Mat4 local = composeTransform(node.translation, node.rotation, node.scale);
    const Mat4 world = multiply(parent, local);

    if (node.mesh >= 0 && static_cast<std::size_t>(node.mesh) < scene.meshes.size()) {
        appendMeshTriangles(
            scene.meshes[static_cast<std::size_t>(node.mesh)],
            world,
            basePath,
            scene.materials,
            textureCache,
            scene.triangles
        );
    }

    if (!scene.hasCamera && node.camera >= 0 && static_cast<std::size_t>(node.camera) < scene.cameras.size()) {
        const S72Camera& source = scene.cameras[static_cast<std::size_t>(node.camera)];
        const Vec3 eye = transformPoint(world, {0.0f, 0.0f, 0.0f});
        const Vec3 forward = normalize(rotateBy(node.rotation, {0.0f, 0.0f, -1.0f}));
        const Vec3 up = normalize(rotateBy(node.rotation, {0.0f, 1.0f, 0.0f}));
        scene.camera.eye = eye;
        scene.camera.target = eye + forward;
        scene.camera.up = up;
        scene.camera.fovY = source.vfov;
        scene.camera.nearPlane = source.nearPlane;
        scene.camera.farPlane = source.farPlane;
        scene.hasCamera = true;
    }

    for (int child : node.children) {
        traverseS72Node(scene, basePath, textureCache, child, world);
    }
}

Camera autoCameraFor(const std::vector<Triangle3>& triangles) {
    if (triangles.empty()) {
        return {};
    }

    Vec3 lo{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
    Vec3 hi{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
    for (const Triangle3& tri : triangles) {
        lo = minVec(lo, minVec(tri.a, minVec(tri.b, tri.c)));
        hi = maxVec(hi, maxVec(tri.a, maxVec(tri.b, tri.c)));
    }

    const Vec3 center = (lo + hi) * 0.5f;
    const float radius = std::max(0.5f, length(hi - lo) * 0.5f);
    Camera camera;
    camera.eye = center + Vec3{radius * 1.2f, -radius * 2.0f, radius * 1.25f};
    camera.target = center;
    camera.up = {0.0f, 0.0f, 1.0f};
    camera.fovY = 42.0f * kPi / 180.0f;
    camera.nearPlane = 0.02f;
    camera.farPlane = radius * 10.0f;
    return camera;
}

S72Scene loadS72Scene(const std::filesystem::path& path, std::uint32_t frameIndex) {
    Json root = JsonParser(readTextFile(path)).parse();
    if (root.type != Json::Type::Array || root.array.size() < 2 || root.array.front().stringOr("") != "s72-v1") {
        throw std::runtime_error("Not a Scene'72 v1 file: " + path.string());
    }

    S72Scene scene;
    scene.meshes.resize(root.array.size());
    scene.materials.resize(root.array.size());
    scene.nodes.resize(root.array.size());
    scene.cameras.resize(root.array.size());
    const std::filesystem::path basePath = path.parent_path();

    for (std::size_t i = 1; i < root.array.size(); ++i) {
        const Json& object = root.array[i];
        if (object.type != Json::Type::Object) {
            continue;
        }
        const std::string type = object.find("type") ? object.find("type")->stringOr("") : "";
        if (type == "MESH") {
            scene.meshes[i] = parseMesh(object);
        } else if (type == "MATERIAL") {
            scene.materials[i] = parseMaterial(object);
        } else if (type == "ENVIRONMENT") {
            scene.environmentTexture = parseEnvironmentTexture(object);
        } else if (type == "NODE") {
            scene.nodes[i] = parseNode(object);
        } else if (type == "CAMERA") {
            scene.cameras[i] = parseCamera(object);
        } else if (type == "DRIVER") {
            scene.drivers.push_back(parseDriver(object));
        } else if (type == "SCENE") {
            scene.roots = toRefs(object.find("roots"));
        }
    }

    applyDrivers(scene.nodes, scene.drivers, animationTimeForFrame(frameIndex, scene.drivers));

    std::map<std::filesystem::path, Texture2D> textureCache;
    for (int rootIndex : scene.roots) {
        traverseS72Node(scene, basePath, textureCache, rootIndex, identity());
    }

    scene.camera = autoCameraFor(scene.triangles);
    scene.hasCamera = true;
    return scene;
}

Vec3 transformGltfPoint(const float* m, Vec3 p) {
    return {
        m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
        m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
        m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14],
    };
}

Color colorFromFactor(const cgltf_primitive& primitive) {
    if (primitive.material && primitive.material->has_pbr_metallic_roughness) {
        const float* base = primitive.material->pbr_metallic_roughness.base_color_factor;
        return {
            static_cast<std::uint8_t>(std::clamp(base[0], 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<std::uint8_t>(std::clamp(base[1], 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<std::uint8_t>(std::clamp(base[2], 0.0f, 1.0f) * 255.0f + 0.5f),
        };
    }
    return {190, 196, 205};
}

const cgltf_accessor* findAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type, cgltf_int index = 0) {
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i) {
        const cgltf_attribute& attr = primitive.attributes[i];
        if (attr.type == type && attr.index == index) {
            return attr.data;
        }
    }
    return nullptr;
}

Vec3 readAccessorVec3(const cgltf_accessor* accessor, cgltf_size index, Vec3 fallback = {}) {
    if (!accessor) {
        return fallback;
    }
    float value[4] = {fallback.x, fallback.y, fallback.z, 1.0f};
    if (!cgltf_accessor_read_float(accessor, index, value, 4)) {
        return fallback;
    }
    return {value[0], value[1], value[2]};
}

Color readAccessorColor(const cgltf_accessor* accessor, cgltf_size index, Color fallback) {
    if (!accessor) {
        return fallback;
    }
    float value[4] = {
        static_cast<float>(fallback.r) / 255.0f,
        static_cast<float>(fallback.g) / 255.0f,
        static_cast<float>(fallback.b) / 255.0f,
        1.0f,
    };
    if (!cgltf_accessor_read_float(accessor, index, value, 4)) {
        return fallback;
    }
    return {
        static_cast<std::uint8_t>(std::clamp(value[0], 0.0f, 1.0f) * 255.0f + 0.5f),
        static_cast<std::uint8_t>(std::clamp(value[1], 0.0f, 1.0f) * 255.0f + 0.5f),
        static_cast<std::uint8_t>(std::clamp(value[2], 0.0f, 1.0f) * 255.0f + 0.5f),
    };
}

struct GltfScene {
    std::vector<Triangle3> triangles;
    Camera camera;
};

struct CachedS72Scene {
    std::vector<Triangle3> triangles;
    Camera camera;
    std::filesystem::path environmentTexture;
};

const CachedS72Scene& cachedStaticS72Scene(const std::filesystem::path& path) {
    static std::map<std::filesystem::path, CachedS72Scene> cache;
    const std::filesystem::path key = std::filesystem::absolute(path).lexically_normal();
    const auto found = cache.find(key);
    if (found != cache.end()) {
        return found->second;
    }

    S72Scene scene = loadS72Scene(path, 0);
    CachedS72Scene cached;
    cached.triangles = std::move(scene.triangles);
    cached.camera = scene.camera;
    cached.environmentTexture = std::move(scene.environmentTexture);
    return cache.emplace(key, std::move(cached)).first->second;
}

void appendGltfPrimitive(const cgltf_node& node, const cgltf_primitive& primitive, std::vector<Triangle3>& triangles) {
    if (primitive.type != cgltf_primitive_type_triangles) {
        return;
    }
    const cgltf_accessor* positions = findAttribute(primitive, cgltf_attribute_type_position);
    if (!positions) {
        return;
    }
    const cgltf_accessor* colors = findAttribute(primitive, cgltf_attribute_type_color);
    const Color materialColor = colorFromFactor(primitive);
    float world[16]{};
    cgltf_node_transform_world(&node, world);

    const auto readIndex = [&primitive](cgltf_size i) {
        return primitive.indices ? cgltf_accessor_read_index(primitive.indices, i) : i;
    };
    const cgltf_size indexCount = primitive.indices ? primitive.indices->count : positions->count;
    for (cgltf_size i = 0; i + 2 < indexCount; i += 3) {
        const cgltf_size ia = readIndex(i + 0);
        const cgltf_size ib = readIndex(i + 1);
        const cgltf_size ic = readIndex(i + 2);
        const Vec3 a = transformGltfPoint(world, readAccessorVec3(positions, ia));
        const Vec3 b = transformGltfPoint(world, readAccessorVec3(positions, ib));
        const Vec3 c = transformGltfPoint(world, readAccessorVec3(positions, ic));
        const Color ca = readAccessorColor(colors, ia, materialColor);
        const Color cb = readAccessorColor(colors, ib, materialColor);
        const Color cc = readAccessorColor(colors, ic, materialColor);
        const Color color = mixColor(ca, cb, cc, 1.0f);
        Triangle3 tri;
        tri.a = a;
        tri.b = b;
        tri.c = c;
        tri.color = color;
        tri.normal = normalize(cross(b - a, c - a));
        tri.baseColor = colorToVec(color);
        triangles.push_back(tri);
    }
}

GltfScene loadGltfScene(const std::filesystem::path& path) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    const std::string pathString = path.string();
    cgltf_result result = cgltf_parse_file(&options, pathString.c_str(), &data);
    if (result != cgltf_result_success || !data) {
        throw std::runtime_error("Could not parse glTF/GLB: " + path.string());
    }

    struct DataGuard {
        cgltf_data* data = nullptr;
        ~DataGuard() { if (data) cgltf_free(data); }
    } guard{data};

    result = cgltf_load_buffers(&options, data, pathString.c_str());
    if (result != cgltf_result_success) {
        throw std::runtime_error("Could not load glTF buffers: " + path.string());
    }

    GltfScene scene;
    for (cgltf_size nodeIndex = 0; nodeIndex < data->nodes_count; ++nodeIndex) {
        const cgltf_node& node = data->nodes[nodeIndex];
        if (!node.mesh) {
            continue;
        }
        for (cgltf_size primitiveIndex = 0; primitiveIndex < node.mesh->primitives_count; ++primitiveIndex) {
            appendGltfPrimitive(node, node.mesh->primitives[primitiveIndex], scene.triangles);
        }
    }

    scene.camera = autoCameraFor(scene.triangles);
    return scene;
}

std::vector<CubeInstance> defaultScene() {
    return {
        {"center", {-0.45f, 0.55f, 0.0f}, 1.0f, {0.86f, 0.32f, 0.24f}, 1.0f},
        {"left", {-1.65f, 0.45f, -0.45f}, 0.72f, {0.16f, 0.58f, 0.88f}, -0.65f},
        {"right", {1.35f, 0.5f, 0.3f}, 0.82f, {0.22f, 0.72f, 0.42f}, 0.8f},
        {"culled-marker", {8.0f, 0.45f, 0.0f}, 0.9f, {0.9f, 0.9f, 0.2f}, 1.2f},
    };
}

std::vector<CubeInstance> loadScene(const std::filesystem::path& path) {
    if (path.empty()) {
        return defaultScene();
    }

    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Could not open v1 scene: " + path.string());
    }

    std::vector<CubeInstance> cubes;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream row(line);
        std::string type;
        CubeInstance cube;
        row >> type;
        if (type != "cube") {
            continue;
        }
        row >> cube.name
            >> cube.center.x >> cube.center.y >> cube.center.z
            >> cube.size
            >> cube.color.x >> cube.color.y >> cube.color.z
            >> cube.spin;
        if (!row.fail()) {
            cubes.push_back(cube);
        }
    }

    if (cubes.empty()) {
        throw std::runtime_error("Scene contained no cube entries: " + path.string());
    }
    return cubes;
}

class Image {
public:
    Image(std::uint32_t width, std::uint32_t height)
        : width_(width), height_(height), pixels_(width * height), depth_(width * height, std::numeric_limits<float>::infinity()) {}

    void clear() {
        for (std::uint32_t y = 0; y < height_; ++y) {
            const float t = static_cast<float>(y) / static_cast<float>(height_ - 1);
            const Color top{32, 43, 59};
            const Color bottom{188, 203, 216};
            const Color color{
                static_cast<std::uint8_t>(static_cast<float>(top.r) * t + static_cast<float>(bottom.r) * (1.0f - t)),
                static_cast<std::uint8_t>(static_cast<float>(top.g) * t + static_cast<float>(bottom.g) * (1.0f - t)),
                static_cast<std::uint8_t>(static_cast<float>(top.b) * t + static_cast<float>(bottom.b) * (1.0f - t)),
            };
            for (std::uint32_t x = 0; x < width_; ++x) {
                pixels_[y * width_ + x] = color;
            }
        }
    }

    void clearV2Sky() {
        for (std::uint32_t y = 0; y < height_; ++y) {
            for (std::uint32_t x = 0; x < width_; ++x) {
                const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width_);
                const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height_);
                const Vec3 direction = normalize(Vec3{(u - 0.5f) * 1.8f, 1.0f, (0.5f - v) * 1.35f});
                pixels_[y * width_ + x] = toneMap(proceduralEnvironmentRadiance(direction));
            }
        }
    }

    void drawTriangle(Vertex2 a, Vertex2 b, Vertex2 c, Color color) {
        const float area = edge(a, b, c);
        if (std::abs(area) < 0.00001f) {
            return;
        }

        const int minX = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
        const int maxX = std::min(static_cast<int>(width_) - 1, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
        const int minY = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
        const int maxY = std::min(static_cast<int>(height_) - 1, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const Vertex2 p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, 0.0f};
                float w0 = edge(b, c, p);
                float w1 = edge(c, a, p);
                float w2 = edge(a, b, p);
                if ((w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) || (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f)) {
                    w0 /= area;
                    w1 /= area;
                    w2 /= area;
                    const float z = w0 * a.z + w1 * b.z + w2 * c.z;
                    const std::size_t index = static_cast<std::size_t>(y) * width_ + static_cast<std::size_t>(x);
                    if (z < depth_[index]) {
                        depth_[index] = z;
                        pixels_[index] = color;
                    }
                }
            }
        }
    }

    void writeBmp(const std::filesystem::path& path) const {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream out(path, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Could not write image: " + path.string());
        }

        const std::uint32_t rowStride = ((width_ * 3u + 3u) / 4u) * 4u;
        const std::uint32_t pixelBytes = rowStride * height_;
        const std::uint32_t fileBytes = 54u + pixelBytes;

        writeU8(out, 'B');
        writeU8(out, 'M');
        writeU32(out, fileBytes);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU32(out, 54);
        writeU32(out, 40);
        writeI32(out, static_cast<std::int32_t>(width_));
        writeI32(out, static_cast<std::int32_t>(height_));
        writeU16(out, 1);
        writeU16(out, 24);
        writeU32(out, 0);
        writeU32(out, pixelBytes);
        writeI32(out, 2835);
        writeI32(out, 2835);
        writeU32(out, 0);
        writeU32(out, 0);

        std::vector<std::uint8_t> row(rowStride);
        for (std::uint32_t srcY = 0; srcY < height_; ++srcY) {
            const std::uint32_t y = height_ - 1u - srcY;
            std::fill(row.begin(), row.end(), std::uint8_t{0});
            for (std::uint32_t x = 0; x < width_; ++x) {
                const Color pixel = pixels_[y * width_ + x];
                row[x * 3u + 0u] = pixel.b;
                row[x * 3u + 1u] = pixel.g;
                row[x * 3u + 2u] = pixel.r;
            }
            out.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size()));
        }
    }

    std::vector<std::uint32_t> toBgra() const {
        std::vector<std::uint32_t> bgra(width_ * height_);
        for (std::size_t i = 0; i < pixels_.size(); ++i) {
            const Color pixel = pixels_[i];
            bgra[i] = 0xff000000u
                | (static_cast<std::uint32_t>(pixel.r) << 16u)
                | (static_cast<std::uint32_t>(pixel.g) << 8u)
                | static_cast<std::uint32_t>(pixel.b);
        }
        return bgra;
    }

private:
    static float edge(Vertex2 a, Vertex2 b, Vertex2 p) {
        return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
    }

    static void writeU8(std::ostream& out, std::uint8_t value) {
        out.put(static_cast<char>(value));
    }

    static void writeU16(std::ostream& out, std::uint16_t value) {
        writeU8(out, static_cast<std::uint8_t>(value & 0xffu));
        writeU8(out, static_cast<std::uint8_t>((value >> 8u) & 0xffu));
    }

    static void writeU32(std::ostream& out, std::uint32_t value) {
        writeU16(out, static_cast<std::uint16_t>(value & 0xffffu));
        writeU16(out, static_cast<std::uint16_t>((value >> 16u) & 0xffffu));
    }

    static void writeI32(std::ostream& out, std::int32_t value) {
        writeU32(out, static_cast<std::uint32_t>(value));
    }

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<Color> pixels_;
    std::vector<float> depth_;
};

struct CameraBasis {
    Vec3 right;
    Vec3 up;
    Vec3 forward;
};

CameraBasis makeBasis(const Camera& camera) {
    CameraBasis basis;
    basis.forward = normalize(camera.target - camera.eye);
    basis.right = normalize(cross(basis.forward, camera.up));
    basis.up = cross(basis.right, basis.forward);
    return basis;
}

Vec3 toCamera(Vec3 world, const Camera& camera, const CameraBasis& basis) {
    const Vec3 offset = world - camera.eye;
    return {dot(offset, basis.right), dot(offset, basis.up), dot(offset, basis.forward)};
}

bool isVisible(Vec3 cameraCenter, float radius, const Camera& camera, float aspect) {
    if (cameraCenter.z + radius < camera.nearPlane || cameraCenter.z - radius > camera.farPlane) {
        return false;
    }
    const float halfY = cameraCenter.z * std::tan(camera.fovY * 0.5f);
    const float halfX = halfY * aspect;
    return std::abs(cameraCenter.x) <= halfX + radius && std::abs(cameraCenter.y) <= halfY + radius;
}

bool project(Vec3 cameraPoint, const Camera& camera, std::uint32_t width, std::uint32_t height, Vertex2& out) {
    if (cameraPoint.z <= camera.nearPlane) {
        return false;
    }

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float tanHalf = std::tan(camera.fovY * 0.5f);
    const float ndcX = cameraPoint.x / (cameraPoint.z * tanHalf * aspect);
    const float ndcY = cameraPoint.y / (cameraPoint.z * tanHalf);
    if (std::abs(ndcX) > 1.6f || std::abs(ndcY) > 1.6f) {
        return false;
    }

    out.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width - 1u);
    out.y = (0.5f - ndcY * 0.5f) * static_cast<float>(height - 1u);
    out.z = cameraPoint.z;
    return true;
}

Vec3 rotateY(Vec3 point, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {point.x * c + point.z * s, point.y, -point.x * s + point.z * c};
}

void drawCube(Image& image, const CubeInstance& cube, const Camera& camera, const CameraBasis& basis, const V1RenderSettings& settings, V1RenderStats& stats) {
    const float half = cube.size * 0.5f;
    const std::array<Vec3, 8> local = {{
        {-half, -half, -half}, {half, -half, -half}, {half, half, -half}, {-half, half, -half},
        {-half, -half, half}, {half, -half, half}, {half, half, half}, {-half, half, half},
    }};
    const std::array<std::array<int, 3>, 12> triangles = {{
        {{0, 2, 1}}, {{0, 3, 2}}, {{4, 5, 6}}, {{4, 6, 7}},
        {{0, 1, 5}}, {{0, 5, 4}}, {{2, 3, 7}}, {{2, 7, 6}},
        {{1, 2, 6}}, {{1, 6, 5}}, {{0, 4, 7}}, {{0, 7, 3}},
    }};

    const float angle = (static_cast<float>(settings.frameIndex) * 0.05f + 0.65f) * cube.spin;
    std::array<Vec3, 8> world{};
    std::array<Vec3, 8> cameraSpace{};
    for (std::size_t i = 0; i < local.size(); ++i) {
        world[i] = rotateY(local[i], angle) + cube.center;
        cameraSpace[i] = toCamera(world[i], camera, basis);
    }

    const Vec3 light = normalize(Vec3{-0.45f, 0.75f, 0.55f});
    for (const auto& tri : triangles) {
        const Vec3 a = world[static_cast<std::size_t>(tri[0])];
        const Vec3 b = world[static_cast<std::size_t>(tri[1])];
        const Vec3 c = world[static_cast<std::size_t>(tri[2])];
        const Vec3 normal = normalize(cross(b - a, c - a));
        const float facing = dot(normal, normalize(camera.eye - a));
        if (facing <= 0.0f) {
            continue;
        }

        Vertex2 pa;
        Vertex2 pb;
        Vertex2 pc;
        if (!project(cameraSpace[static_cast<std::size_t>(tri[0])], camera, settings.width, settings.height, pa)
            || !project(cameraSpace[static_cast<std::size_t>(tri[1])], camera, settings.width, settings.height, pb)
            || !project(cameraSpace[static_cast<std::size_t>(tri[2])], camera, settings.width, settings.height, pc)) {
            continue;
        }

        const float shade = 0.28f + std::max(0.0f, dot(normal, light)) * 0.72f;
        image.drawTriangle(pa, pb, pc, toColor(cube.color, shade));
        ++stats.trianglesSubmitted;
    }
}

Color shadeV2Triangle(const Triangle3& tri, const Camera& camera, const Texture2D* environmentMap) {
    const Vec3 center = (tri.a + tri.b + tri.c) / 3.0f;
    Vec3 normal = normalize(tri.normal);
    if (length(normal) <= 0.00001f) {
        normal = normalize(cross(tri.b - tri.a, tri.c - tri.a));
    }
    const Vec3 view = normalize(camera.eye - center);
    if (dot(normal, view) < 0.0f) {
        normal = normal * -1.0f;
    }

    const Vec3 light = normalize(Vec3{-0.30f, -0.62f, 0.72f});
    const float ndotl = std::max(0.0f, dot(normal, light));
    const float ndotv = std::max(0.05f, dot(normal, view));
    const Vec3 reflected = reflect(view * -1.0f, normal);
    const Vec3 env = environmentRadiance(reflected, environmentMap);
    const Vec3 ambient = environmentRadiance(normal, environmentMap) * 0.28f;
    const Vec3 base = clamp01(tri.baseColor);

    Vec3 hdr{};
    switch (tri.materialKind) {
    case MaterialKind::Environment:
        hdr = env;
        break;
    case MaterialKind::Mirror:
        hdr = env * 1.15f;
        break;
    case MaterialKind::Lambertian:
        hdr = base * (ambient + Vec3{1.25f, 1.18f, 1.05f} * ndotl);
        break;
    case MaterialKind::Pbr: {
        const float roughness = std::clamp(tri.roughness, 0.03f, 1.0f);
        const float metalness = std::clamp(tri.metalness, 0.0f, 1.0f);
        const Vec3 halfVector = normalize(light + view);
        const float ndoth = std::max(0.0f, dot(normal, halfVector));
        const float specPower = std::max(3.0f, (1.0f - roughness) * 96.0f + 2.0f);
        const float spec = std::pow(ndoth, specPower) * (1.0f - roughness * 0.65f);
        const Vec3 dielectricF0{0.04f, 0.04f, 0.04f};
        const Vec3 f0 = lerp(dielectricF0, base, metalness);
        const float fresnel = std::pow(1.0f - ndotv, 5.0f);
        const Vec3 specular = (f0 + (Vec3{1.0f, 1.0f, 1.0f} - f0) * fresnel) * (spec + 0.35f) * env;
        const Vec3 diffuse = base * (1.0f - metalness) * (ambient + Vec3{1.0f, 0.96f, 0.88f} * ndotl);
        hdr = diffuse + specular;
        break;
    }
    case MaterialKind::Simple:
    default:
        hdr = colorToVec(tri.color) * (Vec3{0.35f, 0.38f, 0.42f} + Vec3{0.75f, 0.72f, 0.66f} * ndotl);
        break;
    }

    return toneMap(hdr);
}

void drawTriangle3(
    Image& image,
    const Triangle3& tri,
    const Camera& camera,
    const CameraBasis& basis,
    const V1RenderSettings& settings,
    V1RenderStats& stats,
    const Texture2D* environmentMap = nullptr
) {
    const Vec3 normal = normalize(cross(tri.b - tri.a, tri.c - tri.a));
    Vertex2 pa;
    Vertex2 pb;
    Vertex2 pc;
    if (!project(toCamera(tri.a, camera, basis), camera, settings.width, settings.height, pa)
        || !project(toCamera(tri.b, camera, basis), camera, settings.width, settings.height, pb)
        || !project(toCamera(tri.c, camera, basis), camera, settings.width, settings.height, pc)) {
        return;
    }

    if (settings.enableV2Shading) {
        image.drawTriangle(pa, pb, pc, shadeV2Triangle(tri, camera, environmentMap));
        ++stats.trianglesSubmitted;
        return;
    }

    const Vec3 light = normalize(Vec3{-0.35f, -0.45f, 0.82f});
    const float shade = 0.35f + std::abs(dot(normal, light)) * 0.65f;
    const Color shaded{
        static_cast<std::uint8_t>(std::clamp(static_cast<float>(tri.color.r) * shade, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp(static_cast<float>(tri.color.g) * shade, 0.0f, 255.0f)),
        static_cast<std::uint8_t>(std::clamp(static_cast<float>(tri.color.b) * shade, 0.0f, 255.0f)),
    };
    image.drawTriangle(pa, pb, pc, shaded);
    ++stats.trianglesSubmitted;
}

struct RenderedImage {
    Image image;
    V1RenderStats stats;
    V1CameraSettings camera;
};

RenderedImage renderImage(const V1RenderSettings& settings) {
    const std::string extension = settings.scenePath.extension().string();
    if (extension == ".s72" || extension == ".gltf" || extension == ".glb") {
        std::vector<Triangle3> ownedTriangles;
        const std::vector<Triangle3>* triangles = &ownedTriangles;
        Camera camera;
        std::filesystem::path environmentTexturePath;
        if (extension == ".s72") {
            if (settings.enableV2Shading) {
                const CachedS72Scene& scene = cachedStaticS72Scene(settings.scenePath);
                environmentTexturePath = scene.environmentTexture;
                triangles = &scene.triangles;
                camera = scene.camera;
            } else {
                S72Scene scene = loadS72Scene(settings.scenePath, settings.frameIndex);
                environmentTexturePath = scene.environmentTexture;
                ownedTriangles = std::move(scene.triangles);
                camera = scene.camera;
            }
        } else {
            GltfScene scene = loadGltfScene(settings.scenePath);
            ownedTriangles = std::move(scene.triangles);
            camera = scene.camera;
        }
        camera = cameraFromSettings(settings.camera, camera);

        Image image(settings.width, settings.height);
        static std::map<std::filesystem::path, Texture2D> environmentTextureCache;
        const Texture2D* environmentMapPtr = nullptr;
        if (settings.enableV2Shading && !environmentTexturePath.empty()) {
            environmentMapPtr = &cachedTexture(environmentTextureCache, settings.scenePath.parent_path(), environmentTexturePath);
        }
        if (settings.enableV2Shading) {
            image.clearV2Sky();
        } else {
            image.clear();
        }

        V1RenderStats stats;
        stats.objectCount = static_cast<std::uint32_t>(triangles->size());
        stats.outputPath = settings.outputPath;

        const CameraBasis basis = makeBasis(camera);
        for (const Triangle3& tri : *triangles) {
            ++stats.visibleObjects;
            drawTriangle3(image, tri, camera, basis, settings, stats, environmentMapPtr);
        }

        return {std::move(image), stats, toCameraSettings(camera)};
    }

    const auto cubes = loadScene(settings.scenePath);
    const Camera camera = cameraFromSettings(settings.camera, Camera{});
    const CameraBasis basis = makeBasis(camera);
    const float aspect = static_cast<float>(settings.width) / static_cast<float>(settings.height);

    Image image(settings.width, settings.height);
    image.clear();

    V1RenderStats stats;
    stats.objectCount = static_cast<std::uint32_t>(cubes.size());
    stats.outputPath = settings.outputPath;

    for (const CubeInstance& cube : cubes) {
        const Vec3 cameraCenter = toCamera(cube.center, camera, basis);
        const float radius = cube.size * 0.9f;
        if (!isVisible(cameraCenter, radius, camera, aspect)) {
            continue;
        }
        ++stats.visibleObjects;
        drawCube(image, cube, camera, basis, settings, stats);
    }

    return {std::move(image), stats, toCameraSettings(camera)};
}

} // namespace

V1Frame renderSoftwareV1Frame(const V1RenderSettings& settings) {
    if (settings.width < 16 || settings.height < 16) {
        throw std::runtime_error("v1 render size is too small");
    }

    RenderedImage rendered = renderImage(settings);

    V1Frame frame;
    frame.width = settings.width;
    frame.height = settings.height;
    frame.bgra = rendered.image.toBgra();
    frame.camera = rendered.camera;
    frame.stats = rendered.stats;
    return frame;
}

V1RenderStats renderSoftwareV1(const V1RenderSettings& settings) {
    RenderedImage rendered = renderImage(settings);
    rendered.image.writeBmp(settings.outputPath);
    return rendered.stats;
}

GpuPreviewGeometry buildGpuPreviewGeometry(const V1RenderSettings& settings) {
    const std::string extension = settings.scenePath.extension().string();
    std::vector<Triangle3> ownedTriangles;
    const std::vector<Triangle3>* triangles = &ownedTriangles;
    Camera camera;

    if (extension == ".s72") {
        if (settings.enableV2Shading) {
            const CachedS72Scene& scene = cachedStaticS72Scene(settings.scenePath);
            triangles = &scene.triangles;
            camera = scene.camera;
        } else {
            S72Scene scene = loadS72Scene(settings.scenePath, settings.frameIndex);
            ownedTriangles = std::move(scene.triangles);
            camera = scene.camera;
        }
    } else if (extension == ".gltf" || extension == ".glb") {
        GltfScene scene = loadGltfScene(settings.scenePath);
        ownedTriangles = std::move(scene.triangles);
        camera = scene.camera;
    } else {
        throw std::runtime_error("GPU preview currently supports .s72, .gltf, and .glb scenes");
    }

    camera = cameraFromSettings(settings.camera, camera);

    GpuPreviewGeometry geometry;
    geometry.vertices.reserve(triangles->size() * 3u);
    for (const Triangle3& tri : *triangles) {
        const Vec3 color = settings.enableV2Shading ? clamp01(tri.baseColor) : colorToVec(tri.color);
        geometry.vertices.push_back({tri.a.x, tri.a.y, tri.a.z, color.x, color.y, color.z});
        geometry.vertices.push_back({tri.b.x, tri.b.y, tri.b.z, color.x, color.y, color.z});
        geometry.vertices.push_back({tri.c.x, tri.c.y, tri.c.z, color.x, color.y, color.z});
    }
    geometry.camera = toCameraSettings(camera);
    return geometry;
}

} // namespace vr
