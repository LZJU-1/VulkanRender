# Vulkan Headers

Vendored subset of Khronos Vulkan-Headers used by the native Vulkan GPU preview path.

- Source copy: local NTC dependency tree, `RTXNTC-main/build/_deps/vulkan_headers-src/include`.
- Upstream project: https://github.com/KhronosGroup/Vulkan-Headers
- License: see `LICENSE.md` and `LICENSES/`.

Only the C headers needed for Win32 Vulkan loading are copied here. The renderer dynamically loads `vulkan-1.dll`, so a global Vulkan SDK import library is not required for this preview path.

