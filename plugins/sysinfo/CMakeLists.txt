cmake_minimum_required(VERSION 3.10)

project(pchat-sysinfo C)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pchat_use_static_pkg_vars(GLIB)

# Plugin source files - common to all platforms
set(PLUGIN_SOURCES
    sysinfo.c
    format.c
    shared/df.c
)

# Platform-specific backend sources
if(WIN32)
    list(APPEND PLUGIN_SOURCES
        win32/backend.c
        ${CMAKE_SOURCE_DIR}/src/common/sysinfo/win32/backend.c
    )
elseif(APPLE)
    list(APPEND PLUGIN_SOURCES osx/backend.m)
    enable_language(OBJC)
elseif(UNIX)
    list(APPEND PLUGIN_SOURCES
        unix/backend.c
        unix/parse.c
        unix/pci.c
        unix/match.c
    )
endif()

# Create shared library
add_library(sysinfo MODULE ${PLUGIN_SOURCES})
pchat_apply_static_compile_defs(sysinfo)

# Set output name and properties
set_target_properties(sysinfo PROPERTIES
    PREFIX ""
    OUTPUT_NAME "sysinfo"
)

# Set appropriate suffix based on platform
if(WIN32)
    set_target_properties(sysinfo PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(sysinfo PROPERTIES SUFFIX ".dylib")
else()
    set_target_properties(sysinfo PROPERTIES SUFFIX ".so")
endif()

# Include directories
target_include_directories(sysinfo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/common/sysinfo
    ${GLIB_INCLUDE_DIRS}
)

# Link directories
target_link_directories(sysinfo PRIVATE
    ${GLIB_LIBRARY_DIRS}
)

# Link libraries: GLib symbols come from pchat.exe at runtime on Windows
# static builds.
pchat_link_plugin_runtime(sysinfo GLIB)

# Platform-specific dependencies and flags
if(APPLE)
    # macOS specific frameworks
    target_link_libraries(sysinfo PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
    )
elseif(WIN32)
    # Windows specific libraries for WMI
    target_link_libraries(sysinfo PRIVATE
        wbemuuid
        ole32
        oleaut32
    )
elseif(UNIX AND NOT APPLE)
    # Linux specific libraries
    pkg_check_modules(LIBPCI libpci)
    if(LIBPCI_FOUND)
        target_include_directories(sysinfo PRIVATE ${LIBPCI_INCLUDE_DIRS})
        target_link_libraries(sysinfo PRIVATE ${LIBPCI_LIBRARIES})
        target_compile_definitions(sysinfo PRIVATE HAVE_LIBPCI)
    endif()
endif()

# Compiler flags
target_compile_options(sysinfo PRIVATE
    -Wall
    -Wextra
    ${GLIB_CFLAGS_OTHER}
)

# Platform-specific link options
if(APPLE)
    target_link_options(sysinfo PRIVATE 
        "-Wl,-undefined,dynamic_lookup"
    )
elseif(UNIX)
    target_link_options(sysinfo PRIVATE 
        "-Wl,--unresolved-symbols=ignore-all"
    )
endif()

# Install plugin
install(TARGETS sysinfo
    LIBRARY DESTINATION ${PCHAT_LIBDIR}
    RUNTIME DESTINATION ${PCHAT_LIBDIR}
    COMPONENT Plugins
)
