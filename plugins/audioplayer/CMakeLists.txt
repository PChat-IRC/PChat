cmake_minimum_required(VERSION 3.10)

project(pchat-audioplayer C)

# Find required packages
find_package(PkgConfig REQUIRED)

# FFmpeg components
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

# FAudio
pkg_check_modules(FAUDIO REQUIRED FAudio)

# GTK3
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Find threading library
find_package(Threads REQUIRED)

# Plugin source files
set(PLUGIN_SOURCES
    plugin.c
    audioplayer.c
    playlist_parser.c
    gui.c
)

# Create shared library
add_library(audioplayer MODULE ${PLUGIN_SOURCES})

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
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${FAUDIO_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
)

# Link directories
target_link_directories(audioplayer PRIVATE
    ${AVCODEC_LIBRARY_DIRS}
    ${AVFORMAT_LIBRARY_DIRS}
    ${AVUTIL_LIBRARY_DIRS}
    ${SWRESAMPLE_LIBRARY_DIRS}
    ${FAUDIO_LIBRARY_DIRS}
    ${GTK3_LIBRARY_DIRS}
)

# Link libraries
target_link_libraries(audioplayer
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${FAUDIO_LIBRARIES}
    ${GTK3_LIBRARIES}
    Threads::Threads
)

# Compiler flags
target_compile_options(audioplayer PRIVATE
    -Wall
    -Wextra
    -fPIC
    ${AVCODEC_CFLAGS_OTHER}
    ${AVFORMAT_CFLAGS_OTHER}
    ${AVUTIL_CFLAGS_OTHER}
    ${SWRESAMPLE_CFLAGS_OTHER}
    ${FAUDIO_CFLAGS_OTHER}
    ${GTK3_CFLAGS_OTHER}
)

# Install plugin
install(TARGETS audioplayer
    LIBRARY DESTINATION ${PCHAT_LIBDIR}
    COMPONENT Plugins
)
