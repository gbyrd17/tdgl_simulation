cmake_minimum_required(VERSION 3.21)

# Set vcpkg toolchain file if not already set
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(TDGL_Simulation 
    VERSION 1.0.0
    DESCRIPTION "Time-Dependent Ginzburg-Landau Simulation"
    LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_COMPILER g++)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_COMPILER gcc)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Options
option(TDGL_BUILD_TESTS "Build unit tests" OFF)
option(TDGL_BUILD_DOCS "Build documentation" OFF)

# Find required packages via vcpkg
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

# Platform-specific settings
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
endif()

if(UNIX AND NOT APPLE)
    # Linux-specific settings
    find_package(X11 REQUIRED)
endif()

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/imgui
    ${PROJECT_SOURCE_DIR}/imgui/backends
)

# Collect source files
file(GLOB_RECURSE TDGL_SOURCES 
    "${PROJECT_SOURCE_DIR}/src/*.c"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/imgui/imgui.cpp"
    "${PROJECT_SOURCE_DIR}/imgui/imgui_draw.cpp"
    "${PROJECT_SOURCE_DIR}/imgui/imgui_tables.cpp"
    "${PROJECT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
    "${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp"
    "${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
)

file(GLOB_RECURSE TDGL_HEADERS 
    "${PROJECT_SOURCE_DIR}/include/*.h"
    "${PROJECT_SOURCE_DIR}/include/*.hpp"
)

# Create executable
add_executable(tdgl_simulation main.cpp ${TDGL_SOURCES} ${TDGL_HEADERS})

# Link libraries
target_link_libraries(tdgl_simulation PRIVATE
    OpenGL::GL
    glfw
    glm::glm
    glad::glad
)

# Platform-specific linking
if(UNIX AND NOT APPLE)
    target_link_libraries(tdgl_simulation PRIVATE
        ${X11_LIBRARIES}
        ${CMAKE_DL_LIBS}
        pthread
    )
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(tdgl_simulation PRIVATE /W4)
else()
    target_compile_options(tdgl_simulation PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation
install(TARGETS tdgl_simulation
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Resources
install(DIRECTORY ${PROJECT_SOURCE_DIR}/resources
    DESTINATION share/tdgl_simulation
    OPTIONAL
)

# Print configuration summary
message(STATUS "")
message(STATUS "TDGL Simulation Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "  Vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()
message(STATUS "")
