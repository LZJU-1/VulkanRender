set(VR_SOURCES
    core/FeatureProfile.cpp
    core/RenderGraph.cpp
    core/RendererApp.cpp
    core/ValidationPipeline.cpp
    platform/CommandLine.cpp
    platform/PreviewWindow.cpp
    platform/VulkanPreviewWindow.cpp
    vulkan/VulkanLoader.cpp
    vulkan/VulkanRenderPassBuilder.cpp
    vulkan/VulkanPipelineBuilder.cpp
    vulkan/VulkanCommandRecorder.cpp
    render/SoftwareV1Renderer.cpp
    render/MathTypes.hpp
    rhi/RenderDevice.cpp
    rhi/VulkanDevice.cpp
    main.cpp
)

add_executable(vulkan_render ${VR_SOURCES})
target_include_directories(vulkan_render PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_include_directories(vulkan_render PRIVATE "${PROJECT_SOURCE_DIR}/third_party/cgltf")
target_include_directories(vulkan_render PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")
target_include_directories(vulkan_render PRIVATE "${PROJECT_SOURCE_DIR}/third_party/vulkan_headers/include")
target_compile_features(vulkan_render PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(vulkan_render PRIVATE /W4 /permissive-)
    target_compile_definitions(vulkan_render PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(vulkan_render PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(vulkan_render PRIVATE user32 gdi32)
endif()

if(VR_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
endif()

if(VR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_compile_definitions(vulkan_render PRIVATE VR_HAS_VULKAN=1)
    target_link_libraries(vulkan_render PRIVATE Vulkan::Vulkan)
    message(STATUS "Vulkan SDK found: ${Vulkan_LIBRARY}")
else()
    target_compile_definitions(vulkan_render PRIVATE VR_HAS_VULKAN=0)
    message(STATUS "Vulkan SDK not found; building mock capability backend")
endif()

if(VR_BUILD_SHADERS)
    find_program(GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
    if(NOT GLSLC_EXECUTABLE)
        message(FATAL_ERROR "VR_BUILD_SHADERS is ON, but glslc was not found")
    endif()
    file(GLOB_RECURSE VR_SHADER_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/shaders/*")
    foreach(shader ${VR_SHADER_SOURCES})
        get_filename_component(shader_name "${shader}" NAME)
        set(output "${PROJECT_BINARY_DIR}/spv/${shader_name}.spv")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/spv"
            COMMAND "${GLSLC_EXECUTABLE}" "${shader}" -o "${output}"
            DEPENDS "${shader}"
            VERBATIM
        )
        list(APPEND VR_SHADER_OUTPUTS "${output}")
    endforeach()
    add_custom_target(vulkan_render_shaders DEPENDS ${VR_SHADER_OUTPUTS})
    add_dependencies(vulkan_render vulkan_render_shaders)
endif()
