cmake_minimum_required(VERSION 3.10)

project(pchat-checksum C)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pchat_use_static_pkg_vars(GLIB GIO)

# Plugin source files
set(PLUGIN_SOURCES
    checksum.c
)

# Create shared library
add_library(checksum MODULE ${PLUGIN_SOURCES})
pchat_apply_static_compile_defs(checksum)

# Set output name and properties
set_target_properties(checksum PROPERTIES
    PREFIX ""
    OUTPUT_NAME "checksum"
)

# Set appropriate suffix based on platform
if(WIN32)
    set_target_properties(checksum PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(checksum PROPERTIES SUFFIX ".dylib")
else()
    set_target_properties(checksum PROPERTIES SUFFIX ".so")
endif()

# Include directories
target_include_directories(checksum PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
)

# Link directories
target_link_directories(checksum PRIVATE
    ${GLIB_LIBRARY_DIRS}
    ${GIO_LIBRARY_DIRS}
)

# Link libraries: glib/gio symbols are resolved against pchat.exe on
# Windows static builds (see pchat_link_plugin_runtime).
pchat_link_plugin_runtime(checksum GLIB GIO)

# Compiler flags
target_compile_options(checksum PRIVATE
    -Wall
    -Wextra
    ${GLIB_CFLAGS_OTHER}
    ${GIO_CFLAGS_OTHER}
)

# Platform-specific link options
if(APPLE)
    target_link_options(checksum PRIVATE 
        "-Wl,-undefined,dynamic_lookup"
    )
elseif(UNIX)
    target_link_options(checksum PRIVATE 
        "-Wl,--unresolved-symbols=ignore-all"
    )
endif()

# Install plugin
install(TARGETS checksum
    LIBRARY DESTINATION ${PCHAT_LIBDIR}
    RUNTIME DESTINATION ${PCHAT_LIBDIR}
    COMPONENT Plugins
)
