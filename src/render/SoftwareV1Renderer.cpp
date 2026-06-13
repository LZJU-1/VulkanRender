#include "render/SoftwareV1Renderer.hpp"

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
    S72Attribute color;
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

struct S72Scene {
    std::vector<S72Mesh> meshes;
    std::vector<S72Node> nodes;
    std::vector<S72Camera> cameras;
    std::vector<int> roots;
    std::vector<Triangle3> triangles;
    Camera camera;
    bool hasCamera = false;
};

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
        mesh.color = parseAttribute(attributes->find("COLOR"));
    }
    return mesh;
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

void appendMeshTriangles(
    const S72Mesh& mesh,
    const Mat4& transform,
    const std::filesystem::path& basePath,
    std::vector<Triangle3>& triangles
) {
    if (mesh.count < 3 || mesh.position.src.empty()) {
        return;
    }
    if (mesh.position.format != "R32G32B32_SFLOAT" || mesh.color.format != "R8G8B8A8_UNORM") {
        throw std::runtime_error("v1 Scene'72 loader currently supports POSITION float3 and COLOR rgba8 only");
    }

    const std::filesystem::path positionPath = basePath / mesh.position.src;
    const std::filesystem::path colorPath = basePath / mesh.color.src;
    const std::vector<std::uint8_t> positionBytes = readBinaryFile(positionPath);
    const std::vector<std::uint8_t> colorBytes = colorPath == positionPath ? positionBytes : readBinaryFile(colorPath);

    std::vector<Vec3> positions(mesh.count);
    std::vector<Color> colors(mesh.count);
    for (std::uint32_t i = 0; i < mesh.count; ++i) {
        const std::size_t p = static_cast<std::size_t>(mesh.position.offset) + static_cast<std::size_t>(i) * mesh.position.stride;
        positions[i] = transformPoint(transform, {readF32(positionBytes, p + 0), readF32(positionBytes, p + 4), readF32(positionBytes, p + 8)});

        const std::size_t c = static_cast<std::size_t>(mesh.color.offset) + static_cast<std::size_t>(i) * mesh.color.stride;
        if (c + 4 > colorBytes.size()) {
            throw std::runtime_error("Scene'72 color attribute read is out of bounds");
        }
        colors[i] = {colorBytes[c + 0], colorBytes[c + 1], colorBytes[c + 2]};
    }

    for (std::uint32_t i = 0; i + 2 < mesh.count; i += 3) {
        triangles.push_back({positions[i], positions[i + 1], positions[i + 2], mixColor(colors[i], colors[i + 1], colors[i + 2], 1.0f)});
    }
}

void traverseS72Node(S72Scene& scene, const std::filesystem::path& basePath, int objectIndex, const Mat4& parent) {
    if (objectIndex < 0 || static_cast<std::size_t>(objectIndex) >= scene.nodes.size()) {
        return;
    }

    const S72Node& node = scene.nodes[static_cast<std::size_t>(objectIndex)];
    const Mat4 local = composeTransform(node.translation, node.rotation, node.scale);
    const Mat4 world = multiply(parent, local);

    if (node.mesh >= 0 && static_cast<std::size_t>(node.mesh) < scene.meshes.size()) {
        appendMeshTriangles(scene.meshes[static_cast<std::size_t>(node.mesh)], world, basePath, scene.triangles);
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
        traverseS72Node(scene, basePath, child, world);
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

S72Scene loadS72Scene(const std::filesystem::path& path) {
    Json root = JsonParser(readTextFile(path)).parse();
    if (root.type != Json::Type::Array || root.array.size() < 2 || root.array.front().stringOr("") != "s72-v1") {
        throw std::runtime_error("Not a Scene'72 v1 file: " + path.string());
    }

    S72Scene scene;
    scene.meshes.resize(root.array.size());
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
        } else if (type == "NODE") {
            scene.nodes[i] = parseNode(object);
        } else if (type == "CAMERA") {
            scene.cameras[i] = parseCamera(object);
        } else if (type == "SCENE") {
            scene.roots = toRefs(object.find("roots"));
        }
    }

    for (int rootIndex : scene.roots) {
        traverseS72Node(scene, basePath, rootIndex, identity());
    }

    scene.camera = autoCameraFor(scene.triangles);
    scene.hasCamera = true;
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

void drawTriangle3(Image& image, const Triangle3& tri, const Camera& camera, const CameraBasis& basis, const V1RenderSettings& settings, V1RenderStats& stats) {
    const Vec3 normal = normalize(cross(tri.b - tri.a, tri.c - tri.a));
    Vertex2 pa;
    Vertex2 pb;
    Vertex2 pc;
    if (!project(toCamera(tri.a, camera, basis), camera, settings.width, settings.height, pa)
        || !project(toCamera(tri.b, camera, basis), camera, settings.width, settings.height, pb)
        || !project(toCamera(tri.c, camera, basis), camera, settings.width, settings.height, pc)) {
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
};

RenderedImage renderImage(const V1RenderSettings& settings) {
    if (settings.scenePath.extension() == ".s72") {
        S72Scene scene = loadS72Scene(settings.scenePath);
        Image image(settings.width, settings.height);
        image.clear();

        V1RenderStats stats;
        stats.objectCount = static_cast<std::uint32_t>(scene.triangles.size());
        stats.outputPath = settings.outputPath;

        const CameraBasis basis = makeBasis(scene.camera);
        for (const Triangle3& tri : scene.triangles) {
            ++stats.visibleObjects;
            drawTriangle3(image, tri, scene.camera, basis, settings, stats);
        }

        return {std::move(image), stats};
    }

    const auto cubes = loadScene(settings.scenePath);
    const Camera camera;
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

    return {std::move(image), stats};
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
    frame.stats = rendered.stats;
    return frame;
}

V1RenderStats renderSoftwareV1(const V1RenderSettings& settings) {
    RenderedImage rendered = renderImage(settings);
    rendered.image.writeBmp(settings.outputPath);
    return rendered.stats;
}

} // namespace vr
