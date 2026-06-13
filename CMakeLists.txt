cmake_minimum_required(VERSION 3.24)

project(LZJUVulkanRender
    VERSION 0.1.0
    DESCRIPTION "A staged Vulkan renderer rewrite with an optional realtime ray tracing profile"
    LANGUAGES CXX
)

option(VR_ENABLE_VULKAN "Enable Vulkan runtime capability probing when the SDK is available" ON)
option(VR_BUILD_TESTS "Build smoke tests" ON)
option(VR_BUILD_SHADERS "Compile GLSL shaders with glslc when available" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(src)

if(VR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

