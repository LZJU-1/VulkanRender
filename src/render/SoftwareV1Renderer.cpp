#include "render/SoftwareV1Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
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

Color toColor(Vec3 color, float shade) {
    const auto convert = [shade](float value) {
        const float mapped = std::clamp(value * shade, 0.0f, 1.0f) * 255.0f;
        return static_cast<std::uint8_t>(mapped + 0.5f);
    };
    return {convert(color.x), convert(color.y), convert(color.z)};
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

struct RenderedImage {
    Image image;
    V1RenderStats stats;
};

RenderedImage renderImage(const V1RenderSettings& settings) {
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
