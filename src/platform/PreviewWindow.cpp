#include "platform/PreviewWindow.hpp"

#include <iostream>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <string>

namespace vr {
namespace {

struct PreviewState {
    V1RenderSettings settings;
    V1Frame frame;
    std::uint32_t frameIndex = 0;
    std::chrono::steady_clock::time_point lastTick = std::chrono::steady_clock::now();
};

PreviewState* stateFrom(HWND hwnd) {
    return reinterpret_cast<PreviewState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
                state->settings.frameIndex = state->frameIndex++;
                state->frame = renderSoftwareV1Frame(state->settings);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
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

    RECT rect{0, 0, static_cast<LONG>(state.settings.width), static_cast<LONG>(state.settings.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"VulkanRender v1.0 realtime preview - software backend",
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

    std::cout << "Opened v1 preview window. Press Esc or close the window to exit.\n";
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
