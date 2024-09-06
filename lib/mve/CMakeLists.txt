cmake_minimum_required(VERSION 3.24)

project(mve)
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw-3.4)

add_library(mve STATIC
        "${CMAKE_SOURCE_DIR}/external/ThreadedLoggerForCPPV0.5.2/src/LoggerThread.cpp"
        "${CMAKE_SOURCE_DIR}/lib/mve/external/spirv-reflect-1.3.290.0/src/spirv_reflect.cpp"
        "${CMAKE_SOURCE_DIR}/lib/mve/src/renderer.cpp"
        "${CMAKE_SOURCE_DIR}/lib/mve/src/window.cpp"
        "${CMAKE_SOURCE_DIR}/lib/mve/src/vertex_data.cpp"
        "${CMAKE_SOURCE_DIR}/lib/mve/src/shader.cpp"
        "${CMAKE_SOURCE_DIR}/lib/mve/src/monitor.cpp"
        "${CMAKE_SOURCE_DIR}/lib/mve/src/descriptor_set_allocator.cpp")

target_link_libraries(mve Vulkan::Vulkan glfw)

target_include_directories(mve SYSTEM PUBLIC
        "${CMAKE_SOURCE_DIR}/lib/mve/external/vma-3.1.0/include")

target_include_directories(mve PUBLIC
        "${CMAKE_SOURCE_DIR}/external/game_performance_profiler"
        "${CMAKE_SOURCE_DIR}/lib/mve/external/nnm-0.2.0/include"
        "${CMAKE_SOURCE_DIR}/lib/mve/external/spirv-reflect-1.3.290.0/include"
        "${CMAKE_SOURCE_DIR}/lib/mve/external/stb-image-2.30/include"
        "${CMAKE_SOURCE_DIR}/external/ThreadedLoggerForCPPV0.5.2/libs/include"
        include)

target_include_directories(mve PRIVATE external/spdlog-1.14.1/include)