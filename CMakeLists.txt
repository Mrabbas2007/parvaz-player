cmake_minimum_required(VERSION 3.21)
project(parvaz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_COMPONENTS Core Gui OpenGL Quick QuickControls2 Sql Network)
if(NOT WIN32)
    list(APPEND QT_COMPONENTS DBus)
endif()
find_package(Qt6 6.5 REQUIRED COMPONENTS ${QT_COMPONENTS})
find_package(Qt6 COMPONENTS Svg QUIET)
if(TARGET Qt6::Svg)
    # Qt6::Svg provides the QML image provider for qml/icons/*.svg; Qt6::Quick
    # does not pull it in as a hard dependency and distro Qt splits it into a
    # standalone module (Arch: qt6-svg, Debian/Ubuntu: qt6-svg-dev). Probe quietly
    # and wire the component in only when the Qt installation provides it.
    list(APPEND QT_COMPONENTS Svg)
    find_package(Qt6 6.5 REQUIRED COMPONENTS ${QT_COMPONENTS})
endif()

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

set(WINDOWS_RESOURCES "")
if(WIN32)
    set(WINDOWS_RESOURCES packaging/windows/parvaz.rc)
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
    ${WINDOWS_RESOURCES}
    src/playback/PlaybackHistory.h src/playback/PlaybackHistory.cpp
    src/playback/ThumbnailProvider.h src/playback/ThumbnailProvider.cpp
    src/playback/CoverArtProvider.h src/playback/CoverArtProvider.cpp
    src/playback/ScreenshotController.h src/playback/ScreenshotController.cpp
    src/playback/CacheMaintenance.h src/playback/CacheMaintenance.cpp
    src/playback/ShaderManager.h src/playback/ShaderManager.cpp
)

# Windows: build as a GUI-subsystem executable (no console window) for the
# desktop player, and deploy the libmpv runtime DLL next to the binary.
if(WIN32 AND MPV_LIBRARY)
    set_target_properties(parvaz PROPERTIES WIN32_EXECUTABLE TRUE)
    get_filename_component(MPV_BIN_DIR "${MPV_LIBRARY}" PATH)
    find_file(MPV_RUNTIME_DLL
        NAMES libmpv-2.dll mpv-2.dll mpv.dll libmpv.dll
        PATHS "${MPV_BIN_DIR}" "${MPV_BIN_DIR}/../bin"
        NO_DEFAULT_PATH
    )
    if(NOT MPV_RUNTIME_DLL)
        set(MPV_RUNTIME_DLL "${MPV_BIN_DIR}/mpv.dll")
    endif()
    get_filename_component(MPV_DLL_NAME "${MPV_RUNTIME_DLL}" NAME)

    add_custom_command(TARGET parvaz POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MPV_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:parvaz>/${MPV_DLL_NAME}"
        COMMENT "Deploying ${MPV_DLL_NAME} next to parvaz.exe"
        VERBATIM)

    # Portable release staging (Phase W4): flat folder with the GUI-subsystem
    # exe, the libmpv runtime DLL and the Qt runtime plugins (qml/quick/...).
    get_target_property(QT6_CORE_IMPORTED_LOCATION Qt6::Core IMPORTED_LOCATION)
    get_filename_component(QT6_ARCH_DIR "${QT6_CORE_IMPORTED_LOCATION}" DIRECTORY)
    get_filename_component(QT6_PREFIX "${QT6_ARCH_DIR}" DIRECTORY)
    set(PARVAZ_PORTABLE_DIR "${CMAKE_BINARY_DIR}/release")
    add_custom_command(TARGET parvaz POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PARVAZ_PORTABLE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:parvaz>"
            "${PARVAZ_PORTABLE_DIR}/parvaz.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MPV_RUNTIME_DLL}"
            "${PARVAZ_PORTABLE_DIR}/${MPV_DLL_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QT6_PREFIX}/plugins"
            "${PARVAZ_PORTABLE_DIR}/plugins"
        COMMENT "Staging Windows portable release bundle (parvaz.exe + ${MPV_DLL_NAME} + Qt plugins)"
        VERBATIM)
endif()

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
if(TARGET Qt6::Svg)
    target_link_libraries(parvaz PRIVATE Qt6::Svg)
endif()
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

# ---- Packaging (native RPM / DEB via CPack) --------------------------------
# Only active with -Dparvaz_packaging=ON so the regular build/smoke tree is
# untouched. CPack packages exactly the install rules above: the binary plus
# desktop entry, AppStream metainfo and hicolor icons — everything else
# (QML, fonts, shaders) is compiled into the binary as Qt resources.
#
# NOTE on dependencies: the app also shells out to the `ffmpeg` binary
# (thumbnails, cover art) and `unzip` (subtitle archive extraction); ffsubsync
# and yt-dlp are optional enhancements, not dependencies.
option(parvaz_packaging "Generate RPM/DEB packaging metadata via CPack" OFF)
if(parvaz_packaging)
    set(CPACK_PACKAGE_NAME parvaz)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VENDOR "Parvaz Player Community")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PotPlayer-style media player for Linux, powered by libmpv and Qt 6")
    set(CPACK_PACKAGE_DESCRIPTION "Parvaz is a native Wayland-first video and music player built on
libmpv and Qt 6 / QML. Zero-copy hardware rendering, PotPlayer ergonomics,
first-class Persian subtitles with Vazirmatn typography, online subtitle
search, a dedicated music mode with a 32-band visualizer, HDR tone mapping
and AMD FSR upscaling.")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Mrabbas2007/parvaz-player")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_CONTACT "Parvaz Player Community")

    # RPM (Fedora / RHEL / CachyOS-adjacent)
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
    set(CPACK_RPM_PACKAGE_REQUIRES
        "qt6-qtbase >= 6.5, qt6-qtdeclarative, mpv-libs, ffmpeg, unzip")
    # Keep the %{?dist} tag in the file name so Fedora/CachyOS builds are
    # distinguishable: parvaz-0.1.0-1.fc42.x86_64.rpm
    set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")

    # DEB (Ubuntu 25.04+ — 24.04's system Qt 6.4 is below the >= 6.5 floor)
    set(CPACK_DEBIAN_PACKAGE_SECTION "video")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_FILE_NAME "parvaz_${PROJECT_VERSION}_amd64.deb")
    # Qt/mpv shared libraries are discovered from the actual linked libs via
    # dpkg-shlibdeps; ffmpeg/unzip are PATH lookups and must be listed here.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "ffmpeg, unzip")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

    include(CPack)
endif()
