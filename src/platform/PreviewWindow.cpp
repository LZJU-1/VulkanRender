#include "platform/PreviewWindow.hpp"

#include <iostream>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>

namespace vr {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct PreviewVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct PreviewState {
    V1RenderSettings settings;
    V1Frame frame;
    V1CameraSettings camera;
    std::array<bool, 256> keys{};
    bool roaming = false;
    float yaw = 0.0f;
    float pitch = 0.0f;
    std::uint32_t frameIndex = 0;
    std::chrono::steady_clock::time_point lastTick = std::chrono::steady_clock::now();
};

PreviewState* stateFrom(HWND hwnd) {
    return reinterpret_cast<PreviewState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

PreviewVec3 operator+(PreviewVec3 a, PreviewVec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
PreviewVec3 operator-(PreviewVec3 a, PreviewVec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
PreviewVec3 operator*(PreviewVec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }

float dot(PreviewVec3 a, PreviewVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

PreviewVec3 cross(PreviewVec3 a, PreviewVec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

PreviewVec3 normalize(PreviewVec3 v) {
    const float len = std::sqrt(dot(v, v));
    if (len <= 0.00001f) {
        return {};
    }
    return v * (1.0f / len);
}

PreviewVec3 eyeOf(const V1CameraSettings& camera) {
    return {camera.eyeX, camera.eyeY, camera.eyeZ};
}

PreviewVec3 targetOf(const V1CameraSettings& camera) {
    return {camera.targetX, camera.targetY, camera.targetZ};
}

PreviewVec3 forwardFor(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return normalize({cp * std::cos(yaw), cp * std::sin(yaw), std::sin(pitch)});
}

PreviewVec3 rightFor(PreviewVec3 forward) {
    PreviewVec3 right = normalize(cross(forward, {0.0f, 0.0f, 1.0f}));
    if (dot(right, right) <= 0.00001f) {
        return {1.0f, 0.0f, 0.0f};
    }
    return right;
}

void deriveAnglesFromCamera(PreviewState& state) {
    const PreviewVec3 forward = normalize(targetOf(state.camera) - eyeOf(state.camera));
    state.yaw = std::atan2(forward.y, forward.x);
    state.pitch = std::asin(std::clamp(forward.z, -1.0f, 1.0f));
}

void writeCameraPose(PreviewState& state, PreviewVec3 eye) {
    const PreviewVec3 forward = forwardFor(state.yaw, state.pitch);
    const PreviewVec3 right = rightFor(forward);
    const PreviewVec3 up = normalize(cross(right, forward));

    state.camera.enabled = true;
    state.camera.eyeX = eye.x;
    state.camera.eyeY = eye.y;
    state.camera.eyeZ = eye.z;
    state.camera.targetX = eye.x + forward.x;
    state.camera.targetY = eye.y + forward.y;
    state.camera.targetZ = eye.z + forward.z;
    state.camera.upX = up.x;
    state.camera.upY = up.y;
    state.camera.upZ = up.z;
    state.settings.camera = state.camera;
}

bool keyDown(const PreviewState& state, int key) {
    return key >= 0 && key < static_cast<int>(state.keys.size()) && state.keys[static_cast<std::size_t>(key)];
}

void updateRoamingCamera(PreviewState& state, float dt) {
    if (!state.roaming) {
        state.settings.camera.enabled = false;
        return;
    }

    const float lookSpeed = 1.85f;
    if (keyDown(state, VK_LEFT) || keyDown(state, 'J')) {
        state.yaw += lookSpeed * dt;
    }
    if (keyDown(state, VK_RIGHT) || keyDown(state, 'L')) {
        state.yaw -= lookSpeed * dt;
    }
    if (keyDown(state, VK_UP) || keyDown(state, 'I')) {
        state.pitch = std::clamp(state.pitch + lookSpeed * dt, -1.45f, 1.45f);
    }
    if (keyDown(state, VK_DOWN) || keyDown(state, 'K')) {
        state.pitch = std::clamp(state.pitch - lookSpeed * dt, -1.45f, 1.45f);
    }

    const PreviewVec3 forward = forwardFor(state.yaw, state.pitch);
    const PreviewVec3 right = rightFor(forward);
    const PreviewVec3 up{0.0f, 0.0f, 1.0f};
    const float speed = (GetKeyState(VK_SHIFT) & 0x8000) ? 12.0f : 4.5f;
    PreviewVec3 movement{};
    if (keyDown(state, 'W')) movement = movement + forward;
    if (keyDown(state, 'S')) movement = movement - forward;
    if (keyDown(state, 'D')) movement = movement + right;
    if (keyDown(state, 'A')) movement = movement - right;
    if (keyDown(state, 'E') || keyDown(state, VK_SPACE)) movement = movement + up;
    if (keyDown(state, 'Q') || keyDown(state, VK_CONTROL)) movement = movement - up;
    if (dot(movement, movement) > 0.00001f) {
        movement = normalize(movement) * (speed * dt);
    }

    writeCameraPose(state, eyeOf(state.camera) + movement);
}

LRESULT CALLBACK previewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams));
        SetTimer(hwnd, 1, 16, nullptr);
        return 0;
    case WM_TIMER:
        if (PreviewState* state = stateFrom(hwnd)) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - state->lastTick).count();
            if (elapsedMs >= 16) {
                state->lastTick = now;
                updateRoamingCamera(*state, static_cast<float>(elapsedMs) / 1000.0f);
                state->settings.frameIndex = state->frameIndex++;
                state->frame = renderSoftwareV1Frame(state->settings);
                if (!state->roaming) {
                    state->camera = state->frame.camera;
                    deriveAnglesFromCamera(*state);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam < 256) {
            PreviewState* state = stateFrom(hwnd);
            if (state) {
                state->keys[static_cast<std::size_t>(wParam)] = true;
                if (wParam == 'R' && !(lParam & (1 << 30))) {
                    state->roaming = !state->roaming;
                    state->camera = state->frame.camera;
                    deriveAnglesFromCamera(*state);
                    if (state->roaming) {
                        writeCameraPose(*state, eyeOf(state->camera));
                        SetWindowTextW(hwnd, L"VulkanRender preview - roaming camera (R off, WASD/QE move, arrows/IJKL look, Esc exit)");
                        std::cout << "Camera roaming enabled. WASD move, Q/E or Ctrl/Space vertical, arrows or IJKL look, Shift fast, R toggles off.\n";
                    } else {
                        state->settings.camera.enabled = false;
                        SetWindowTextW(hwnd, L"VulkanRender preview - press R for roaming camera, Esc exits");
                        std::cout << "Camera roaming disabled; using scene camera.\n";
                    }
                }
            }
        }
        return 0;
    case WM_KEYUP:
        if (wParam < 256) {
            if (PreviewState* state = stateFrom(hwnd)) {
                state->keys[static_cast<std::size_t>(wParam)] = false;
            }
        }
        return 0;
    case WM_PAINT:
        if (PreviewState* state = stateFrom(hwnd)) {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT client{};
            GetClientRect(hwnd, &client);

            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = static_cast<LONG>(state->frame.width);
            info.bmiHeader.biHeight = -static_cast<LONG>(state->frame.height);
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;

            if (!state->frame.bgra.empty()) {
                StretchDIBits(
                    dc,
                    0,
                    0,
                    client.right - client.left,
                    client.bottom - client.top,
                    0,
                    0,
                    static_cast<int>(state->frame.width),
                    static_cast<int>(state->frame.height),
                    state->frame.bgra.data(),
                    &info,
                    DIB_RGB_COLORS,
                    SRCCOPY
                );
            }

            EndPaint(hwnd, &paint);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

} // namespace

int runV1PreviewWindow(V1RenderSettings settings) {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"LZJUVulkanRenderV1Preview";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = previewProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.lpszClassName = className;
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&windowClass);

    PreviewState state;
    state.settings = std::move(settings);
    state.frame = renderSoftwareV1Frame(state.settings);
    state.camera = state.frame.camera;
    deriveAnglesFromCamera(state);

    RECT rect{0, 0, static_cast<LONG>(state.settings.width), static_cast<LONG>(state.settings.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"VulkanRender preview - press R for roaming camera, Esc exits",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        &state
    );

    if (!hwnd) {
        std::cerr << "Could not create preview window.\n";
        return 1;
    }

    std::cout << "Opened preview window. Press R to toggle roaming camera; Esc or close the window to exit.\n";
    std::cout << "Scene objects=" << state.frame.stats.objectCount
              << " visible=" << state.frame.stats.visibleObjects << '\n';

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}

} // namespace vr

#else

namespace vr {

int runV1PreviewWindow(V1RenderSettings settings) {
    (void)settings;
    std::cerr << "Realtime preview window is currently implemented on Windows only.\n";
    return 1;
}

} // namespace vr

#endif
