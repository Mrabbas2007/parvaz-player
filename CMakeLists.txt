cmake_minimum_required(VERSION 3.21)
project(parvaz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui OpenGL Quick QuickControls2 DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

qt_standard_project_setup()

qt_add_executable(parvaz
    src/main.cpp
    src/mpv/MpvObject.h  src/mpv/MpvObject.cpp
    src/mpv/MpvFboItem.h src/mpv/MpvFboItem.cpp
    src/platform/IdleInhibitor.h src/platform/IdleInhibitor.cpp
    src/platform/MprisService.h  src/platform/MprisService.cpp
)

target_include_directories(parvaz PRIVATE src)
target_compile_definitions(parvaz PRIVATE
    PARVAZ_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)

target_link_libraries(parvaz PRIVATE
    Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::Quick Qt6::QuickControls2 Qt6::DBus
    PkgConfig::MPV
)

# make check : build + generate a test clip + 3s smoke run
add_custom_target(check
    COMMAND ffmpeg -y -f lavfi -i "testsrc=size=640x360:rate=30:duration=3"
            -c:v libx264 -pix_fmt yuv420p ${CMAKE_BINARY_DIR}/test.mp4
    COMMAND ${CMAKE_BINARY_DIR}/parvaz --smoke ${CMAKE_BINARY_DIR}/test.mp4
    DEPENDS parvaz
    USES_TERMINAL
    COMMENT "build + smoke test"
)
