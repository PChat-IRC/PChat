cmake_minimum_required(VERSION 3.15)

# C for the existing plugin/decoder code, CXX for the wxWidgets GUI.
project(pchat-audioplayer C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(PkgConfig REQUIRED)

# Per-format decoders (replace FFmpeg)
pkg_check_modules(MPG123  REQUIRED libmpg123)
pkg_check_modules(FLAC    REQUIRED flac)
pkg_check_modules(VORBIS  REQUIRED vorbisfile)
pkg_check_modules(OGG     REQUIRED ogg)

# alac-decoder (David Hammerton's reverse-engineered ALAC decoder
# bundled with its own QuickTime/MP4 demuxer). The vcpkg port does
# not install a cmake config or pkg-config, so locate it manually.
find_path(ALAC_DECODER_INCLUDE_DIR
    NAMES alac_decoder/decomp.h
    DOC "Path containing alac_decoder/decomp.h")
find_library(ALAC_DECODER_LIBRARY
    NAMES libalac_decoder alac_decoder
    DOC "Path to the alac-decoder static library")
if(NOT ALAC_DECODER_INCLUDE_DIR OR NOT ALAC_DECODER_LIBRARY)
    message(FATAL_ERROR
        "alac-decoder not found. Install the 'alac-decoder' vcpkg port "
        "(provides include/alac_decoder/*.h and libalac_decoder).")
endif()

# FAudio
pkg_check_modules(FAUDIO REQUIRED FAudio)

# wxWidgets (GUI). vcpkg's wxwidgets port ships a CMake package config.
find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base aui)

# Self-contain the plugin DLL when the parent build is fully static
# (otherwise we'd link against e.g. libwx_*.dll, which doesn't exist
# in static vcpkg builds, and against libgtk-3-0.dll which we deliberately
# avoid here).
pchat_use_static_pkg_vars(MPG123 FLAC VORBIS OGG FAUDIO)

# Find threading library
find_package(Threads REQUIRED)

# Plugin source files
set(PLUGIN_SOURCES
    plugin.c
    audioplayer.c
    playlist_parser.c
    gui.cpp
)

# Create shared library
add_library(audioplayer MODULE ${PLUGIN_SOURCES})

# Mirror the static-link compile defs so any glib headers transitively
# pulled in by pchat-plugin.h don't annotate symbols as
# __declspec(dllimport).
pchat_apply_static_compile_defs(audioplayer)

# Set output name and allow undefined symbols (resolved by PChat at runtime)
set_target_properties(audioplayer PROPERTIES
    PREFIX ""
    OUTPUT_NAME "audioplayer"
)

# Set appropriate suffix based on platform
if(WIN32)
    set_target_properties(audioplayer PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(audioplayer PROPERTIES SUFFIX ".dylib")
else()
    set_target_properties(audioplayer PROPERTIES SUFFIX ".so")
endif()

# Allow undefined symbols - they'll be resolved when PChat loads the plugin
if(WIN32)
    # Use .def file to export plugin symbols on Windows
    target_sources(audioplayer PRIVATE audioplayer.def)
elseif(APPLE)
    target_link_options(audioplayer PRIVATE
        "-Wl,-undefined,dynamic_lookup"
    )
elseif(UNIX)
    target_link_options(audioplayer PRIVATE
        "-Wl,--unresolved-symbols=ignore-all"
    )
endif()

# Include directories
target_include_directories(audioplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${MPG123_INCLUDE_DIRS}
    ${FLAC_INCLUDE_DIRS}
    ${VORBIS_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${ALAC_DECODER_INCLUDE_DIR}
    ${FAUDIO_INCLUDE_DIRS}
)

# Link directories
target_link_directories(audioplayer PRIVATE
    ${MPG123_LIBRARY_DIRS}
    ${FLAC_LIBRARY_DIRS}
    ${VORBIS_LIBRARY_DIRS}
    ${OGG_LIBRARY_DIRS}
    ${FAUDIO_LIBRARY_DIRS}
)

# Link libraries. wxWidgets and the audio decoders are plugin-private:
# pchat.exe's GTK/GLib runtime is not used by this plugin anymore, so the
# duplicate-GType / .rsrc-merge problems with --whole-archive go away.
target_link_libraries(audioplayer PRIVATE
    ${MPG123_LDFLAGS}
    ${FLAC_LDFLAGS}
    ${VORBIS_LDFLAGS}
    ${OGG_LDFLAGS}
    ${ALAC_DECODER_LIBRARY}
    ${FAUDIO_LDFLAGS}
    Threads::Threads
    wx::core
    wx::base
    wx::aui
)

# Extra Windows system libs not advertised by static .pc files:
#   - FAudio's win32 backend uses Media Foundation (mfplat/mfreadwrite/mfuuid)
#     and the property system (propsys), which mingw provides via these libs.
#   - wxWidgets static MSW build commonly needs comctl32/winmm/ole32/...
if(WIN32)
    target_link_libraries(audioplayer PRIVATE
        mfplat
        mfreadwrite
        mfuuid
        propsys
        comctl32
        winmm
        ole32
        oleaut32
        uuid
        rpcrt4
    )
endif()

# Compiler flags (C only — wx targets carry their own CXX flags).
target_compile_options(audioplayer PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    -fPIC
    ${MPG123_CFLAGS_OTHER}
    ${FLAC_CFLAGS_OTHER}
    ${VORBIS_CFLAGS_OTHER}
    ${OGG_CFLAGS_OTHER}
    ${FAUDIO_CFLAGS_OTHER}
)

# Install plugin
install(TARGETS audioplayer
    LIBRARY DESTINATION ${PCHAT_LIBDIR}
    COMPONENT Plugins
)
