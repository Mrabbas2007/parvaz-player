cmake_minimum_required(VERSION 3.21)
project(parvaz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui OpenGL Quick QuickControls2 DBus Sql)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

qt_standard_project_setup()

qt_add_executable(parvaz
    src/main.cpp
    src/mpv/MpvObject.h  src/mpv/MpvObject.cpp
    src/mpv/MpvFboItem.h src/mpv/MpvFboItem.cpp
    src/platform/IdleInhibitor.h src/platform/IdleInhibitor.cpp
    src/platform/MprisService.h  src/platform/MprisService.cpp
    src/playback/PlaybackHistory.h src/playback/PlaybackHistory.cpp
    src/playback/ThumbnailProvider.h src/playback/ThumbnailProvider.cpp
)

# QML lives inside the binary: the executable must run from anywhere (install
# tree, Flatpak) without a checkout next to it. Resource layout mirrors qml/:
#   qrc:/Parvaz/qml/Main.qml, qrc:/Parvaz/qml/components/*.qml, qrc:/Parvaz/qmldir
# so the existing relative imports ("components", "..") and relative asset
# paths ("../icons/x.svg", "fonts/…") keep working unchanged.
#
# NOTE: qt_add_qml_module must stay in THIS directory scope. Calling it from a
# qml/CMakeLists.txt subdirectory silently drops the moc --collect-json /
# cmake_automoc_parser rules from the build graph (broken registrar
# dependency on an empty metatypes json), reproducible with Qt 6.11 /
# CMake 4.3.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(parvaz
    URI Parvaz
    VERSION 1.0
    NO_PLUGIN                    # C++ types are registered at runtime (main.cpp)
    RESOURCE_PREFIX "/"
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/components/ContextMenu.qml
        qml/components/ControlBar.qml
        qml/components/DropOverlay.qml
        qml/components/IconButton.qml
        qml/components/MusicPlayerView.qml
        qml/components/OsdOverlay.qml
        qml/components/PlaylistDrawer.qml
        qml/components/SeekBar.qml
        qml/components/TechnicalInfoOverlay.qml
        qml/components/TitleBar.qml
        qml/components/VolumeControl.qml
)

# Fonts and icons as resources under the module's qml/ subtree, relative paths
# preserved via BASE (qrc:/Parvaz/qml/fonts/…, qrc:/Parvaz/qml/icons/…).
file(GLOB PARVAZ_ICONS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/qml/icons/*.svg)
qt_add_resources(parvaz "parvaz-assets"
    PREFIX "/Parvaz/qml"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    FILES
        qml/fonts/Vazirmatn.ttf
        qml/fonts/JetBrainsMono-Medium.otf
        qml/fonts/JetBrainsMono-Regular.otf
        ${PARVAZ_ICONS}
)

# Fonts and icons as resources under the same module prefix, relative paths
# preserved via BASE (qrc:/Parvaz/fonts/…, qrc:/Parvaz/icons/…).
file(GLOB PARVAZ_ICONS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/qml/icons/*.svg)
qt_add_resources(parvaz "parvaz-assets"
    PREFIX "/Parvaz"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    FILES
        qml/fonts/Vazirmatn.ttf
        qml/fonts/JetBrainsMono-Medium.otf
        qml/fonts/JetBrainsMono-Regular.otf
        ${PARVAZ_ICONS}
)

target_include_directories(parvaz PRIVATE src)
target_compile_definitions(parvaz PRIVATE
    PARVAZ_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)

# Warnings are part of the contract: new code that trips -Wall/-Wextra must
# be fixed before merge, not silenced per-line.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(parvaz PRIVATE -Wall -Wextra)
endif()

target_link_libraries(parvaz PRIVATE
    Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::Quick Qt6::QuickControls2 Qt6::DBus Qt6::Sql
    PkgConfig::MPV
)

# make check : build + generate a test clip with one audio and one video track
# (so the track-list assertions have something to find) + a matching UTF-8
# sidecar (mpv's default sub-auto=exact loads <video>.srt) + a 3s smoke run.
add_custom_target(check
    COMMAND ffmpeg -y
            -f lavfi -i "testsrc=size=640x360:rate=30:duration=3"
            -f lavfi -i "sine=frequency=440:duration=3"
            -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest
            ${CMAKE_BINARY_DIR}/test.mp4
    COMMAND ffmpeg -y
            -f lavfi -i "sine=frequency=440:duration=2"
            -f lavfi -i "testsrc=size=300x300:duration=1"
            -map 0:a -map 1:v -c:a mp3 -b:a 128k -c:v png
            -metadata "title=Test Music"
            -metadata "artist=Antigravity"
            -metadata "album=Parvaz Album"
            -disposition:v attached_pic -shortest
            ${CMAKE_BINARY_DIR}/music_test.mp3
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fa.srt ${CMAKE_BINARY_DIR}/test.srt
    COMMAND ${CMAKE_BINARY_DIR}/parvaz --smoke ${CMAKE_BINARY_DIR}/test.mp4
    DEPENDS parvaz
    USES_TERMINAL
    COMMENT "build + smoke test"
)
