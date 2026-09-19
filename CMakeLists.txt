cmake_minimum_required(VERSION 3.16)
project(EzAutoclicker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_executable(EzAutoclicker
    src/main.cpp
    src/ClickerEngine.cpp
)
target_include_directories(EzAutoclicker PRIVATE src)
target_link_libraries(EzAutoclicker PRIVATE imgui glfw OpenGL::GL Threads::Threads)

if(WIN32)
    target_sources(EzAutoclicker PRIVATE src/platform/input_windows.cpp)
    target_link_libraries(EzAutoclicker PRIVATE user32)
elseif(UNIX AND NOT APPLE)
    target_sources(EzAutoclicker PRIVATE src/platform/input_linux.cpp)
endif()

if(MSVC)
    target_compile_options(EzAutoclicker PRIVATE /W4)
else()
    target_compile_options(EzAutoclicker PRIVATE -Wall -Wextra)
endif()
