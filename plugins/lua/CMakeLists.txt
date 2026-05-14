cmake_minimum_required(VERSION 3.10)

project(pchat-lua C)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GMODULE REQUIRED gmodule-2.0)

# Try to find Lua (5.1, 5.2, 5.3, or 5.4)
find_package(Lua)

if(NOT LUA_FOUND)
    # Try with pkg-config
    pkg_check_modules(LUA lua5.4)
    if(NOT LUA_FOUND)
        pkg_check_modules(LUA lua5.3)
    endif()
    if(NOT LUA_FOUND)
        pkg_check_modules(LUA lua5.2)
    endif()
    if(NOT LUA_FOUND)
        pkg_check_modules(LUA lua5.1)
    endif()
    if(NOT LUA_FOUND)
        pkg_check_modules(LUA lua)
    endif()
endif()

if(NOT LUA_FOUND)
    message(STATUS "Lua not found, skipping Lua plugin")
    return()
endif()

message(STATUS "Found Lua: ${LUA_VERSION_STRING}")

pchat_use_static_pkg_vars(GLIB GMODULE LUA)

# Plugin source files
set(PLUGIN_SOURCES
    lua.c
)

# Create shared library
add_library(lua MODULE ${PLUGIN_SOURCES})
pchat_apply_static_compile_defs(lua)

# Set output name and properties
set_target_properties(lua PROPERTIES
    PREFIX ""
    OUTPUT_NAME "lua"
)

# Set appropriate suffix based on platform
if(WIN32)
    set_target_properties(lua PROPERTIES SUFFIX ".dll")
else()
    set_target_properties(lua PROPERTIES SUFFIX ".so")
endif()

# Include directories
target_include_directories(lua PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${GLIB_INCLUDE_DIRS}
    ${GMODULE_INCLUDE_DIRS}
)

# Add Lua include directories
if(LUA_INCLUDE_DIR)
    target_include_directories(lua PRIVATE ${LUA_INCLUDE_DIR})
endif()
if(LUA_INCLUDE_DIRS)
    target_include_directories(lua PRIVATE ${LUA_INCLUDE_DIRS})
endif()

# Link libraries. GLib/GModule symbols come from pchat.exe at runtime on
# Windows static builds; Lua is plugin-private.
target_link_libraries(lua PRIVATE
    ${LUA_LIBRARIES}
)
pchat_link_plugin_runtime(lua GLIB GMODULE)

# Compiler flags
target_compile_options(lua PRIVATE
    -Wall
    -Wextra
    ${GLIB_CFLAGS_OTHER}
    ${GMODULE_CFLAGS_OTHER}
    ${LUA_CFLAGS_OTHER}
)

# Platform-specific link options
if(UNIX AND NOT APPLE)
    target_link_options(lua PRIVATE 
        "-Wl,--unresolved-symbols=ignore-all"
    )
endif()

# Install plugin
install(TARGETS lua
    LIBRARY DESTINATION ${PCHAT_LIBDIR}
    RUNTIME DESTINATION ${PCHAT_LIBDIR}
    COMPONENT Plugins
)
