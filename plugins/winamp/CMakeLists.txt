cmake_minimum_required(VERSION 3.10)

project(pchat-winamp C)

# This plugin is Windows-only
if(NOT WIN32)
    message(STATUS "Winamp plugin is Windows-only, skipping")
    return()
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pchat_use_static_pkg_vars(GLIB)

# Plugin source files
set(PLUGIN_SOURCES
    winamp.c
)

# Create shared library
add_library(winamp MODULE ${PLUGIN_SOURCES})
pchat_apply_static_compile_defs(winamp)

# Set output name and properties
set_target_properties(winamp PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pchatwinamp"
    SUFFIX ".dll"
)

# Include directories
target_include_directories(winamp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${GLIB_INCLUDE_DIRS}
)

# Link libraries: GLib symbols come from pchat.exe at runtime on Windows
# static builds.
pchat_link_plugin_runtime(winamp GLIB)

# Compiler definitions
target_compile_definitions(winamp PRIVATE
    WIN32_LEAN_AND_MEAN
)

# Compiler flags
target_compile_options(winamp PRIVATE
    -Wall
    -Wextra
    ${GLIB_CFLAGS_OTHER}
)

# Install plugin
install(TARGETS winamp
    LIBRARY DESTINATION ${PCHAT_LIBDIR}
    RUNTIME DESTINATION ${PCHAT_LIBDIR}
    COMPONENT Plugins
)
