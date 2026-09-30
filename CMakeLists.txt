cmake_minimum_required(VERSION 3.20)

project(DesktopVPet
    VERSION 0.1.0
    DESCRIPTION "Desktop buddy pet, ported from Godot C# to C++/SDL3"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

# Keep the executable, its assets and (on shared builds) its libraries together.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE PATH "Output directory")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(DESKTOPVPET_USE_SYSTEM_SDL "Use system SDL3/SDL3_image instead of the vendored submodules" OFF)
option(DESKTOPVPET_SDL_SHARED     "Build/link SDL as shared libraries"               OFF)
option(DESKTOPVPET_COPY_ASSETS    "Copy assets/ next to the built executable"        ON)

# ---------------------------------------------------------------------------
# SDL3 + SDL3_image (vendored submodules by default)
# ---------------------------------------------------------------------------
if(DESKTOPVPET_USE_SYSTEM_SDL)
    find_package(SDL3 3.2.0 REQUIRED)
    find_package(SDL3_image 3.2.0 REQUIRED)
    find_package(SDL3_ttf 3.2.0 REQUIRED)
    set(DESKTOPVPET_SDL_TARGET     SDL3::SDL3)
    set(DESKTOPVPET_SDLIMG_TARGET  SDL3_image::SDL3_image)
    set(DESKTOPVPET_SDLTTF_TARGET  SDL3_ttf::SDL3_ttf)
else()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdl/SDL/CMakeLists.txt")
        message(FATAL_ERROR
            "Vendored SDL3 submodule missing. Run:\n"
            "  git submodule update --init --recursive\n"
            "or configure with -DDESKTOPVPET_USE_SYSTEM_SDL=ON")
    endif()

    # SDL3
    if(DESKTOPVPET_SDL_SHARED)
        set(SDL_SHARED ON  CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    else()
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    endif()
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)
    add_subdirectory(sdl/SDL EXCLUDE_FROM_ALL)

    # SDL3_image
    set(BUILD_SHARED_LIBS    ${DESKTOPVPET_SDL_SHARED} CACHE BOOL "" FORCE)
    set(SDLIMAGE_VENDORED    ON  CACHE BOOL "" FORCE)
    set(SDLIMAGE_SAMPLES     OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TESTS       OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_INSTALL     OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
    # DesktopVPet only needs PNG/JPG/GIF-class images; disabling the heavier
    # codecs avoids pulling their (large) vendored submodules.
    set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JXL  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TIF  OFF CACHE BOOL "" FORCE)
    add_subdirectory(sdl/SDL_image EXCLUDE_FROM_ALL)

    # SDL3_ttf: use system FreeType/HarfBuzz on native builds; on cross builds
    # (e.g. MinGW for Windows) use the vendored FreeType so no target packages
    # are required.
    if(CMAKE_CROSSCOMPILING)
        set(SDLTTF_VENDORED ON  CACHE BOOL "" FORCE)
        set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
    else()
        set(SDLTTF_VENDORED OFF CACHE BOOL "" FORCE)
        set(SDLTTF_HARFBUZZ ON  CACHE BOOL "" FORCE)
    endif()
    set(SDLTTF_PLUTOSVG  OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES   OFF CACHE BOOL "" FORCE)
    set(SDLTTF_TESTS     OFF CACHE BOOL "" FORCE)
    set(SDLTTF_INSTALL   OFF CACHE BOOL "" FORCE)
    add_subdirectory(sdl/SDL_ttf EXCLUDE_FROM_ALL)

    set(DESKTOPVPET_SDL_TARGET    SDL3::SDL3)
    set(DESKTOPVPET_SDLIMG_TARGET SDL3_image::SDL3_image)
    set(DESKTOPVPET_SDLTTF_TARGET SDL3_ttf::SDL3_ttf)
endif()

# ---------------------------------------------------------------------------
# Shared JSON reader/writer (used by the app and the editor)
# ---------------------------------------------------------------------------
add_library(dvp_json STATIC src/config.cpp)
target_include_directories(dvp_json PUBLIC src)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(desktop_vpet
    src/main.cpp
    src/app.cpp
    src/window.cpp
    src/texture.cpp
    src/animator.cpp
    src/character.cpp
    src/pet.cpp
    src/dialogue.cpp
    src/platform_clickthrough.cpp
    src/text_renderer.cpp
    src/menu.cpp
)

target_include_directories(desktop_vpet PRIVATE src)
target_link_libraries(desktop_vpet PRIVATE
    dvp_json
    ${DESKTOPVPET_SDL_TARGET}
    ${DESKTOPVPET_SDLIMG_TARGET}
    ${DESKTOPVPET_SDLTTF_TARGET}
)

if(MSVC)
    target_compile_options(desktop_vpet PRIVATE /W4 /permissive-)
else()
    target_compile_options(desktop_vpet PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MINGW)
    # Keep the .exe self-contained (no libstdc++/libgcc/libwinpthread DLLs).
    target_link_options(desktop_vpet PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Wayland per-pixel click-through (optional; needs wayland-client dev files).
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND_CLIENT QUIET wayland-client)
    endif()
    if(WAYLAND_CLIENT_FOUND)
        target_compile_definitions(desktop_vpet PRIVATE DE_HAVE_WAYLAND=1)
        target_include_directories(desktop_vpet PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
        target_link_libraries(desktop_vpet PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
        message(STATUS "Wayland click-through: enabled (${WAYLAND_CLIENT_VERSION})")
    else()
        message(STATUS "Wayland click-through: disabled (wayland-client not found)")
    endif()
endif()

# ---------------------------------------------------------------------------
# Runtime layout: keep the exe beside its libraries and assets
# ---------------------------------------------------------------------------
if(DESKTOPVPET_COPY_ASSETS)
    add_custom_command(TARGET desktop_vpet POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:desktop_vpet>/assets"
        COMMENT "Copying assets next to desktop_vpet"
    )
endif()

# Stage shared SDL libraries next to the executable so the output directory is
# self-contained and relocatable.
if(UNIX AND NOT APPLE AND DESKTOPVPET_SDL_SHARED)
    set_target_properties(desktop_vpet PROPERTIES
        BUILD_RPATH   "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

if(DESKTOPVPET_SDL_SHARED)
    add_custom_command(TARGET desktop_vpet POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${DESKTOPVPET_SDL_TARGET}>"
            "$<TARGET_FILE_DIR:desktop_vpet>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${DESKTOPVPET_SDLIMG_TARGET}>"
            "$<TARGET_FILE_DIR:desktop_vpet>"
        COMMENT "Staging SDL shared libraries"
    )
endif()

if(WIN32)
    # CreateRectRgn/CombineRgn/SetWindowRgn live in gdi32.
    target_link_libraries(desktop_vpet PRIVATE gdi32 user32)
endif()

# ---------------------------------------------------------------------------
# Character editor (wxWidgets; optional)
#
# wxWidgets source selection:
#   * system   - fast; used by default when found (native builds only).
#   * vendored - the wxWidgets/ submodule; used for cross builds, when the
#                system library is missing, or with -DDESKTOPVPET_USE_SYSTEM_WX=OFF.
# Vendored builds are static (self-contained executable).
# ---------------------------------------------------------------------------
# The editor is on by default for native builds; cross builds must opt in
# (a vendored wxWidgets cross build is large and needs its bundled submodules).
if(CMAKE_CROSSCOMPILING)
    option(DESKTOPVPET_BUILD_EDITOR "Build the wxWidgets character editor" OFF)
else()
    option(DESKTOPVPET_BUILD_EDITOR "Build the wxWidgets character editor" ON)
endif()
option(DESKTOPVPET_USE_SYSTEM_WX "Prefer system wxWidgets over the vendored submodule" ON)

if(DESKTOPVPET_BUILD_EDITOR)
    set(DVP_WX_TARGETS "")
    set(DVP_WX_SOURCE "")

    if(DESKTOPVPET_USE_SYSTEM_WX AND NOT CMAKE_CROSSCOMPILING)
        find_package(wxWidgets QUIET COMPONENTS core base)
        if(wxWidgets_FOUND)
            include(${wxWidgets_USE_FILE})
            set(DVP_WX_TARGETS ${wxWidgets_LIBRARIES})
            set(DVP_WX_SOURCE "system")
        endif()
    endif()

    if(DVP_WX_TARGETS STREQUAL "" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wxWidgets/CMakeLists.txt")
        set(wxBUILD_SHARED    OFF CACHE BOOL "" FORCE)
        set(wxBUILD_MONOLITHIC OFF CACHE BOOL "" FORCE)
        set(wxBUILD_SAMPLES   OFF CACHE BOOL "" FORCE)
        set(wxBUILD_TESTS     OFF CACHE BOOL "" FORCE)
        set(wxBUILD_DEMOS     OFF CACHE BOOL "" FORCE)
        set(wxBUILD_INSTALL   OFF CACHE BOOL "" FORCE)
        set(wxBUILD_PRECOMP   ON  CACHE BOOL "" FORCE)
        # STC needs wxWidgets' nested scintilla/lexilla submodules; not needed
        # for the editor, so skip it (keeps the vendored checkout self-contained).
        set(wxUSE_STC          OFF CACHE BOOL "" FORCE)
        add_subdirectory(wxWidgets EXCLUDE_FROM_ALL)
        set(DVP_WX_TARGETS wx::core wx::base)
        set(DVP_WX_SOURCE "vendored")
    endif()

    if(NOT DVP_WX_TARGETS STREQUAL "")
        add_executable(desktop_vpet_editor
            editor/main.cpp
            editor/editor_frame.cpp
            editor/character_model.cpp
            editor/preview_panel.cpp
        )
        target_include_directories(desktop_vpet_editor PRIVATE src editor)
        target_compile_definitions(desktop_vpet_editor PRIVATE
            DVP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(desktop_vpet_editor PRIVATE dvp_json ${DVP_WX_TARGETS})
        if(MSVC)
            target_compile_options(desktop_vpet_editor PRIVATE /W4 /permissive-)
        else()
            target_compile_options(desktop_vpet_editor PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        if(MINGW)
            # Self-contained editor on Windows.
            target_link_options(desktop_vpet_editor PRIVATE
                -static-libgcc -static-libstdc++ -static)
        endif()
        message(STATUS "Character editor: enabled (${DVP_WX_SOURCE} wxWidgets)")
    else()
        message(STATUS
            "Character editor: disabled (wxWidgets not found; run "
            "'git submodule update --init wxWidgets' for the vendored build)")
    endif()
endif()
