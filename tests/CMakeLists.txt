add_executable(vulkan_render_tests
    profile_tests.cpp
    ../src/core/FeatureProfile.cpp
    ../src/core/RenderGraph.cpp
    ../src/core/ValidationPipeline.cpp
)

target_include_directories(vulkan_render_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_features(vulkan_render_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(vulkan_render_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(vulkan_render_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME profile_graph_tests COMMAND vulkan_render_tests)
