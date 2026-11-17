cmake_minimum_required(VERSION 3.21)
project(parvaz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_COMPONENTS Core Gui OpenGL Quick QuickControls2 Sql Network)
if(NOT WIN32)
    list(APPEND QT_COMPONENTS DBus)
endif()
find_package(Qt6 6.5 REQUIRED COMPONENTS ${QT_COMPONENTS})

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MPV IMPORTED_TARGET mpv)
endif()

if(NOT TARGET PkgConfig::MPV)
    find_path(MPV_INCLUDE_DIR NAMES mpv/client.h)
    find_library(MPV_LIBRARY NAMES mpv mpv-2 libmpv)
    if(MPV_INCLUDE_DIR AND MPV_LIBRARY)
        add_library(PkgConfig::MPV UNKNOWN IMPORTED)
        set_target_properties(PkgConfig::MPV PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
            IMPORTED_LOCATION "${MPV_LIBRARY}"
        )
    else()
        message(FATAL_ERROR "libmpv development files not found. Set MPV_ROOT or install libmpv-dev.")
    endif()
endif()

qt_standard_project_setup()

set(PLATFORM_SOURCES
    src/platform/IdleInhibitor.h src/platform/IdleInhibitor.cpp
)
if(NOT WIN32)
    list(APPEND PLATFORM_SOURCES
        src/platform/MprisService.h  src/platform/MprisService.cpp
    )
endif()

qt_add_executable(parvaz
    src/main.cpp
    src/config/ConfigManager.h   src/config/ConfigManager.cpp
    src/mpv/MpvObject.h  src/mpv/MpvObject.cpp
    src/mpv/MpvFboItem.h src/mpv/MpvFboItem.cpp
    src/mpv/SubtitleController.h src/mpv/SubtitleController.cpp
    src/subs/SubtitleSearchService.h src/subs/SubtitleSearchService.cpp
    src/subs/SubtitleSyncService.h src/subs/SubtitleSyncService.cpp
    ${PLATFORM_SOURCES}
    src/playback/PlaybackHistory.h src/playback/PlaybackHistory.cpp
    src/playback/ThumbnailProvider.h src/playback/ThumbnailProvider.cpp
    src/playback/CoverArtProvider.h src/playback/CoverArtProvider.cpp
    src/playback/ScreenshotController.h src/playback/ScreenshotController.cpp
    src/playback/CacheMaintenance.h src/playback/CacheMaintenance.cpp
    src/playback/ShaderManager.h src/playback/ShaderManager.cpp
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
        qml/components/AudioVisualizer.qml
        qml/components/ContextMenu.qml
        qml/components/ControlBar.qml
        qml/components/ControlCenter.qml
        qml/components/DropOverlay.qml
        qml/components/IconButton.qml
        qml/components/LoadingSpinner.qml
        qml/components/MusicPlayerView.qml
        qml/components/NextEpisodePrompt.qml
        qml/components/OpenUrlDialog.qml
        qml/components/OsdOverlay.qml
        qml/components/PlaylistDrawer.qml
        qml/components/SeekBar.qml
        qml/components/SubtitleSearchDialog.qml
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

qt_add_resources(parvaz "parvaz-app-icon"
    PREFIX "/"
    FILES
        assets/icons/io.github.parvaz.ParvazPlayer.svg
)

target_include_directories(parvaz PRIVATE
    src
    3rdparty
)
target_compile_definitions(parvaz PRIVATE
    PARVAZ_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)

# Warnings are part of the contract: new code that trips -Wall/-Wextra must
# be fixed before merge, not silenced per-line.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(parvaz PRIVATE -Wall -Wextra)
endif()

target_link_libraries(parvaz PRIVATE
    Qt6::Core Qt6::Gui Qt6::OpenGL Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Network
    PkgConfig::MPV
)
if(TARGET Qt6::DBus)
    target_link_libraries(parvaz PRIVATE Qt6::DBus)
endif()
if(WIN32)
    target_link_libraries(parvaz PRIVATE user32 shell32)
endif()

# Installation rules for desktop Linux and packages
include(GNUInstallDirs)

install(TARGETS parvaz
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES io.github.parvaz.ParvazPlayer.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES io.github.parvaz.ParvazPlayer.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(FILES assets/icons/io.github.parvaz.ParvazPlayer.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

install(DIRECTORY assets/icons/hicolor/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor
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
