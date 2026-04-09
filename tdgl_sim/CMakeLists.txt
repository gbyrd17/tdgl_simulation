cmake_minimum_required(VERSION 3.10) 
project(tdgl_sim)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui")
set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(glfw3 REQUIRED glm CONFIG REQUIRED)

set(SOURCES
    main.cpp
    ${SRC}/geometry.cpp
    ${SRC}/simulator.cpp
    ${SRC}/infoPane.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)



add_executable(tdgl_sim main.cpp glad.c ${SOURCES})

target_include_directories(tdgl_sim PRIVATE 
    ./deps/include 
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
    C:/msys64/ucrt64/include
)

target_link_libraries(tdgl_sim PRIVATE glfw opengl32)
