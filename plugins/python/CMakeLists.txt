cmake_minimum_required(VERSION 3.10)

project(pchat-python C)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development)

if(NOT Python3_FOUND)
    message(STATUS "Python3 not found, skipping Python plugin")
    return()
endif()

message(STATUS "Found Python: ${Python3_VERSION} (${Python3_EXECUTABLE})")

pkg_check_modules(GLIB REQUIRED glib-2.0)
pchat_use_static_pkg_vars(GLIB)

# Plugin source files
set(PLUGIN_SOURCES
    python.c
)

# Add .def file on Windows to export symbols
if(WIN32)
    list(APPEND PLUGIN_SOURCES python.def)
endif()

# Create shared library
add_library(python MODULE ${PLUGIN_SOURCES})
pchat_apply_static_compile_defs(python)

# Set output name and properties
set_target_properties(python PROPERTIES
    PREFIX ""
    OUTPUT_NAME "python"
)

# Set appropriate suffix based on platform
if(WIN32)
    set_target_properties(python PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(python PROPERTIES SUFFIX ".dylib")
else()
    set_target_properties(python PROPERTIES SUFFIX ".so")
endif()

# Include directories
target_include_directories(python PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_BINARY_DIR}
    ${Python3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

# Link directories
target_link_directories(python PRIVATE
    ${GLIB_LIBRARY_DIRS}
)

# Link libraries. GLib symbols come from pchat.exe at runtime on Windows
# static builds; Python is plugin-private.
target_link_libraries(python PRIVATE
    ${Python3_LIBRARIES}
)
pchat_link_plugin_runtime(python GLIB)

# Compiler definitions
target_compile_definitions(python PRIVATE
    HAVE_CONFIG_H
)

# Compiler flags
target_compile_options(python PRIVATE
    -Wall
    -Wextra
    ${GLIB_CFLAGS_OTHER}
)

# Platform-specific link options
if(APPLE)
    target_link_options(python PRIVATE 
        "-Wl,-undefined,dynamic_lookup"
    )
elseif(UNIX)
    target_link_options(python PRIVATE 
        "-Wl,--unresolved-symbols=ignore-all"
    )
endif()

# Install plugin
install(TARGETS python
    LIBRARY DESTINATION ${PCHAT_LIBDIR}
    RUNTIME DESTINATION ${PCHAT_LIBDIR}
    COMPONENT Plugins
)

# Install Python support files
# pchat.py / xchat.py / hexchat.py are thin shims that re-export the
# real bindings from _pchat.py, so scripts written for PChat, classic
# X-Chat, or HexChat all import successfully without modification.
install(FILES
    pchat.py
    python.py
    xchat.py
    hexchat.py
    _pchat.py
    DESTINATION ${PCHAT_LIBDIR}
)
