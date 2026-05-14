cmake_minimum_required(VERSION 3.12)

# Enable Objective-C on macOS for notification support
if(APPLE)
    project(pchat VERSION 2.0.0 LANGUAGES C OBJC)
else()
    project(pchat VERSION 2.0.0 LANGUAGES C)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(ENABLE_IPV6 "Enable IPv6 support" ON)

# TLS/SSL backend selection.
#   openssl  - use OpenSSL (default on non-Windows)
#   schannel - use Windows Schannel / CNG (default on Windows)
#   none/off - disable TLS support entirely
if(WIN32)
    set(_PCHAT_DEFAULT_SSL "schannel")
else()
    set(_PCHAT_DEFAULT_SSL "openssl")
endif()
set(ENABLE_SSL "${_PCHAT_DEFAULT_SSL}" CACHE STRING
    "TLS backend to use: openssl, schannel, or none/off")
set_property(CACHE ENABLE_SSL PROPERTY STRINGS openssl schannel none off OFF)

# Normalise to lowercase
string(TOLOWER "${ENABLE_SSL}" _enable_ssl_lc)
if(_enable_ssl_lc STREQUAL "off" OR _enable_ssl_lc STREQUAL "none" OR _enable_ssl_lc STREQUAL "")
    set(PCHAT_SSL_BACKEND "none")
elseif(_enable_ssl_lc STREQUAL "openssl")
    set(PCHAT_SSL_BACKEND "openssl")
elseif(_enable_ssl_lc STREQUAL "schannel")
    if(NOT WIN32)
        message(FATAL_ERROR "ENABLE_SSL=schannel is only supported on Windows.")
    endif()
    set(PCHAT_SSL_BACKEND "schannel")
else()
    message(FATAL_ERROR "Invalid ENABLE_SSL value: '${ENABLE_SSL}'. Use openssl, schannel, or none.")
endif()

# Reject the legacy flag with a clear message.
if(DEFINED ENABLE_OPENSSL)
    message(FATAL_ERROR
        "ENABLE_OPENSSL has been removed. Use -DENABLE_SSL=openssl or -DENABLE_SSL=schannel (or none).")
endif()

option(ENABLE_GTKFE "Build GTK3 frontend" ON)
option(ENABLE_TEXTFE "Build text frontend" OFF)
option(ENABLE_PYTHON "Build Python plugin" OFF)
option(ENABLE_AUDIOPLAYER "Enable AudioPlayer plugin" ON)
option(ENABLE_CHECKSUM "Enable Checksum plugin" ON)
option(ENABLE_FISHLIM "Enable FiSHLiM encryption plugin" ON)
option(ENABLE_LUA "Enable Lua scripting plugin" ON)
option(ENABLE_SYSINFO "Enable SysInfo plugin" ON)
option(ENABLE_EXEC "Enable Exec plugin (Windows only)" ON)
option(ENABLE_WINAMP "Enable Winamp plugin (Windows only)" ON)
option(ENABLE_UPD "Enable Update Checker plugin (Windows only)" ON)
option(ENABLE_PLUGIN "Enable plugin support" ON)
if(WIN32 OR APPLE)
    option(ENABLE_DBUS "Enable D-Bus support" OFF)
else()
    option(ENABLE_DBUS "Enable D-Bus support" ON)
endif()
option(ENABLE_LIBNOTIFY "Enable libnotify support" ON)
option(ENABLE_LIBCANBERRA "Enable libcanberra support" ON)
option(ENABLE_LIBPROXY "Enable libproxy support" OFF)
option(ENABLE_PORTABLE "Build in portable mode (stores config beside executable)" OFF)
option(ENABLE_WINRT_NOTIFICATIONS "Use WinRT Toast notifications (Windows 8+, requires C++)" ON)

# Built-in update checker (disabled in portable mode)
if(NOT ENABLE_PORTABLE)
    option(ENABLE_UPDATE_CHECKER "Enable built-in update checker (Windows/macOS)" ON)
else()
    set(ENABLE_UPDATE_CHECKER OFF CACHE BOOL "Update checker disabled in portable mode" FORCE)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Shared helpers for static-link plumbing (used by main exe and plugins).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(PChatStaticDeps)

# When building under vcpkg, the surrounding shell (e.g. MSYS2/zsh) often
# exports PKG_CONFIG_PATH pointing at the system MinGW/clang64 .pc files.
# Without intervention, pkgconf will then resolve gtk+-3.0/glib/etc. against
# the system shared libraries instead of vcpkg's static install, producing a
# broken mix of -L paths in the final link line. Force pkgconf to look at
# *only* the vcpkg installed pkgconfig dirs whenever a vcpkg installed tree
# is present in the build directory.
if(DEFINED VCPKG_INSTALLED_DIR AND EXISTS "${VCPKG_INSTALLED_DIR}")
    set(_pchat_pc_libdir
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/pkgconfig")
    set(_pchat_pc_sharedir
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/pkgconfig")
    if(WIN32)
        # pkgconf.exe on Windows uses ';' as the path separator.
        set(ENV{PKG_CONFIG_LIBDIR}
            "${_pchat_pc_libdir};${_pchat_pc_sharedir}")
    else()
        set(ENV{PKG_CONFIG_LIBDIR}
            "${_pchat_pc_libdir}:${_pchat_pc_sharedir}")
    endif()
    set(ENV{PKG_CONFIG_PATH} "")
    message(STATUS "Forcing PKG_CONFIG_LIBDIR to vcpkg paths: $ENV{PKG_CONFIG_LIBDIR}")
endif()

# On macOS, help find packages installed via Homebrew or MacPorts
if(APPLE)
    # Homebrew paths (try Apple Silicon location first, then Intel)
    if(EXISTS /opt/homebrew)
        list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /opt/homebrew/opt/openssl@3 /opt/homebrew/opt/openssl@1.1)
        set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/openssl@3/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    elseif(EXISTS /usr/local/Homebrew)
        list(APPEND CMAKE_PREFIX_PATH /usr/local /usr/local/opt/openssl@3 /usr/local/opt/openssl@1.1)
        set(ENV{PKG_CONFIG_PATH} "/usr/local/lib/pkgconfig:/usr/local/opt/openssl@3/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()
    
    # MacPorts paths
    if(EXISTS /opt/local)
        list(APPEND CMAKE_PREFIX_PATH /opt/local)
        set(ENV{PKG_CONFIG_PATH} "/opt/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()
endif()

# Detect static linkage (vcpkg's *-static triplets, or VCPKG_LIBRARY_LINKAGE=static).
# Static GLib/GTK on Windows requires a few preprocessor defines so headers don't
# annotate every public symbol as __declspec(dllimport) (which would produce
# undefined references to __imp_g_*, __imp_gtk_*, fribidi __imp_*, etc.).
set(PCHAT_STATIC_LINK FALSE)
if(VCPKG_TARGET_TRIPLET MATCHES "static" OR
   "$ENV{VCPKG_TARGET_TRIPLET}" MATCHES "static" OR
   (DEFINED VCPKG_LIBRARY_LINKAGE AND VCPKG_LIBRARY_LINKAGE STREQUAL "static"))
    set(PCHAT_STATIC_LINK TRUE)
endif()
if(WIN32 AND PCHAT_STATIC_LINK)
    add_compile_definitions(
        GLIB_STATIC_COMPILATION
        GOBJECT_STATIC_COMPILATION
        GIO_STATIC_COMPILATION
        GMODULE_STATIC_COMPILATION
        FRIBIDI_LIB_STATIC
        PCRE2_STATIC
    )

    # When building with MinGW (GCC), also pull in libgcc, libstdc++ and
    # winpthreads statically so the resulting .exe has no DLL dependencies
    # outside of system DLLs. Without these flags GCC links libgcc_s_seh-1.dll,
    # libstdc++-6.dll and libwinpthread-1.dll dynamically.
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_link_options(
            -static-libgcc
            -static-libstdc++
            "LINKER:-Bstatic,--whole-archive"
            "-lwinpthread"
            "LINKER:--no-whole-archive,-Bdynamic"
        )
        # The executable is linked through the C driver (gcc), which does not
        # auto-add libstdc++. cairo's DirectWrite backend (cairo-dwrite-font.cpp)
        # however contains C++ symbols (operators new[]/delete[],
        # __gxx_personality_seh0, vtable for __cxxabiv1::*). Append a *static*
        # libstdc++ to the very end of the link line by injecting into the
        # implicit standard libraries — this guarantees it appears after
        # libcairo.a, satisfying its undefined references.
        string(APPEND CMAKE_C_STANDARD_LIBRARIES
            " -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic")
        string(APPEND CMAKE_CXX_STANDARD_LIBRARIES
            " -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic")
    endif()
endif()

# GLib and GTK
pkg_check_modules(GLIB REQUIRED glib-2.0>=2.34.0 gobject-2.0 gmodule-2.0 gio-2.0)

if(ENABLE_GTKFE)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0>=3.10.0)
    # Get GTK prefix for bundling
    pkg_get_variable(GTK3_PREFIX gtk+-3.0 prefix)
endif()

# When linking statically, replace the default (shared) pkg-config flag/dir/lib
# variables with their _STATIC_ counterparts so the transitive dependency chain
# (iconv, intl, gmodule, fribidi, epoxy, opengl32, ...) is actually pulled in
# at link time. Without this, libgtk-3.a and libglib-2.0.a leave many
# `undefined reference to ...` symbols at the final executable link.
pchat_use_static_pkg_vars(GLIB GTK3)

# TLS backend dependencies
if(PCHAT_SSL_BACKEND STREQUAL "openssl")
    pkg_check_modules(OPENSSL REQUIRED openssl)
    if(OPENSSL_FOUND)
        set(USE_SSL 1)
        set(USE_OPENSSL 1)
        message(STATUS "Found OpenSSL: ${OPENSSL_INCLUDE_DIRS}")
    endif()
elseif(PCHAT_SSL_BACKEND STREQUAL "schannel")
    set(USE_SSL 1)
    set(USE_SCHANNEL 1)
    # Schannel/CNG come with the Windows SDK; no pkg-config needed.
    set(SCHANNEL_LIBRARIES secur32 crypt32 bcrypt ncrypt ws2_32)
    message(STATUS "Using Schannel TLS backend (Windows native)")
else()
    message(STATUS "TLS support disabled (ENABLE_SSL=none)")
endif()

if(ENABLE_DBUS)
    pkg_check_modules(DBUS dbus-1>=0.60 dbus-glib-1>=0.70)
    if(DBUS_FOUND)
        set(USE_DBUS 1)
    endif()
endif()

if(ENABLE_LIBNOTIFY)
    pkg_check_modules(LIBNOTIFY libnotify>=0.4)
    if(LIBNOTIFY_FOUND)
        set(USE_LIBNOTIFY 1)
    endif()
endif()

if(ENABLE_LIBCANBERRA)
    pkg_check_modules(LIBCANBERRA libcanberra>=0.22)
    if(LIBCANBERRA_FOUND)
        set(USE_LIBCANBERRA 1)
    endif()
endif()

if(ENABLE_LIBPROXY)
    pkg_check_modules(LIBPROXY libproxy-1.0)
    if(LIBPROXY_FOUND)
        set(USE_LIBPROXY 1)
    endif()
endif()

# macOS integration
if(APPLE)
    pkg_check_modules(GTK_MAC_INTEGRATION gtk-mac-integration-gtk3)
    if(GTK_MAC_INTEGRATION_FOUND)
        set(HAVE_GTK_MAC 1)
        message(STATUS "Found gtk-mac-integration: ${GTK_MAC_INTEGRATION_VERSION}")
    else()
        message(STATUS "gtk-mac-integration not found - install with: brew install gtk-mac-integration")
    endif()
endif()

# Update checker dependencies (Windows and macOS only, disabled in portable mode)
if(ENABLE_UPDATE_CHECKER AND (WIN32 OR APPLE))
    pkg_check_modules(LIBSOUP libsoup-3.0>=3.0)
    pkg_check_modules(LIBXML2 libxml-2.0>=2.6)
    
    if(LIBSOUP_FOUND AND LIBXML2_FOUND)
        set(HAVE_UPDATE_CHECKER 1)
        message(STATUS "Update checker enabled (libsoup: ${LIBSOUP_VERSION}, libxml2: ${LIBXML2_VERSION})")
    else()
        set(ENABLE_UPDATE_CHECKER OFF)
        message(WARNING "Update checker disabled - missing dependencies (libsoup-3.0 or libxml-2.0)")
    endif()

    if(PCHAT_STATIC_LINK)
        pchat_use_static_pkg_vars(LIBSOUP LIBXML2)
    endif()
elseif(ENABLE_PORTABLE)
    message(STATUS "Update checker disabled (portable mode)")
else()
    message(STATUS "Update checker not available on this platform")
endif()

# Gettext
find_package(Gettext REQUIRED)
set(GETTEXT_PACKAGE "pchat")

# iso-codes for locale data
pkg_check_modules(ISO_CODES iso-codes)
if(ISO_CODES_FOUND)
    pkg_get_variable(ISO_CODES_PREFIX iso-codes prefix)
    set(ISO_CODES_LOCALEDIR "${ISO_CODES_PREFIX}/share/locale")
else()
    # Fallback to standard location
    set(ISO_CODES_LOCALEDIR "${CMAKE_INSTALL_PREFIX}/share/locale")
endif()

# System checks
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)

check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(memory.h HAVE_MEMORY_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(unistd.h HAVE_UNISTD_H)

check_function_exists(memset HAVE_MEMSET)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strtoul HAVE_STRTOUL)

# memrchr is a GNU extension, need to check with feature test macros
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(memrchr "string.h" HAVE_MEMRCHR)
set(CMAKE_REQUIRED_DEFINITIONS)

# Detect system
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(USING_LINUX 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(USING_FREEBSD 1)
endif()

if(WIN32)
    set(OS_W32 1)
endif()

if(ENABLE_IPV6)
    set(USE_IPV6 1)
endif()

if(ENABLE_PLUGIN)
    set(USE_PLUGIN 1)
    set(USE_GMODULE 1)
endif()

if(ENABLE_PORTABLE)
    set(PORTABLE_BUILD 1)
    message(STATUS "Building in portable mode - config will be stored beside executable")
endif()

# Installation directories
include(GNUInstallDirs)

if(ENABLE_PORTABLE AND WIN32)
    # Portable mode: flat directory structure
    set(CMAKE_INSTALL_BINDIR ".")
    set(CMAKE_INSTALL_LIBDIR "lib")
    set(CMAKE_INSTALL_DATADIR "share")
    set(CMAKE_INSTALL_INCLUDEDIR "include")
    set(CMAKE_INSTALL_MANDIR "share/man")
    set(PCHAT_LIBDIR "plugins")
    set(PCHAT_SHAREDIR "share/pchat")
else()
    # Standard FHS layout
    set(PCHAT_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}/pchat")
    set(PCHAT_SHAREDIR "${CMAKE_INSTALL_FULL_DATADIR}/pchat")
endif()

# Configure config.h
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Subdirectories
add_subdirectory(po)
add_subdirectory(src)
add_subdirectory(data)

if(ENABLE_PLUGIN)
    add_subdirectory(plugins)
endif()

# Summary
message(STATUS "")
message(STATUS "PChat ${PROJECT_VERSION} configuration summary:")
message(STATUS "")
message(STATUS "  Installation prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  IPv6 support: ${ENABLE_IPV6}")
message(STATUS "  TLS backend: ${PCHAT_SSL_BACKEND}")
message(STATUS "  GTK3 frontend: ${ENABLE_GTKFE}")
message(STATUS "  Text frontend: ${ENABLE_TEXTFE}")
message(STATUS "  Plugin support: ${ENABLE_PLUGIN}")
message(STATUS "  Python plugin: ${ENABLE_PYTHON}")
message(STATUS "  AudioPlayer plugin: ${ENABLE_AUDIOPLAYER}")
message(STATUS "  Checksum plugin: ${ENABLE_CHECKSUM}")
message(STATUS "  FiSHLiM plugin: ${ENABLE_FISHLIM}")
message(STATUS "  Lua plugin: ${ENABLE_LUA}")
message(STATUS "  SysInfo plugin: ${ENABLE_SYSINFO}")
if(WIN32)
    message(STATUS "  Exec plugin: ${ENABLE_EXEC}")
    message(STATUS "  Winamp plugin: ${ENABLE_WINAMP}")
    message(STATUS "  Update Checker plugin: ${ENABLE_UPD}")
endif()
message(STATUS "  D-Bus support: ${ENABLE_DBUS}")
message(STATUS "")

# Windows dependency bundling and NSIS installer
if(WIN32)
    # Configure dependency bundling script (needed for both portable mode and installer)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/BundleDependencies.cmake.in"
        "${CMAKE_BINARY_DIR}/BundleDependencies.cmake"
        @ONLY
    )

    # Always run the bundling script on `cmake --install` so essential
    # runtime data (GSettings schemas + compiled gschemas.compiled, gdk-pixbuf
    # loaders + loaders.cache, GTK theme/icon assets, GTK locale .mo files,
    # and DLL deps for non-static builds) ends up next to the installed
    # executable. Without the compiled gschemas.compiled, GTK aborts the
    # process the first time it opens a widget that reads GSettings (e.g. the
    # GtkFileChooserDialog used by the plugin browser).
    install(SCRIPT "${CMAKE_BINARY_DIR}/BundleDependencies.cmake"
            COMPONENT Runtime)
    if(ENABLE_PORTABLE)
        message(STATUS "Portable mode: dependencies will be bundled during install")
    else()
        message(STATUS "Windows install: dependencies (schemas, loaders, themes) will be bundled during install")
    endif()

    # Find NSIS for installer
    find_program(NSIS_EXECUTABLE makensis
        PATHS "$ENV{MSYSTEM_PREFIX}/bin"
        DOC "NSIS executable"
    )

    if(NSIS_EXECUTABLE)
        # Configure the NSIS script
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/installer.nsi.in"
            "${CMAKE_BINARY_DIR}/installer.nsi"
            @ONLY
        )

        # Add custom target to build installer
        add_custom_target(installer
            COMMAND ${CMAKE_COMMAND} -E echo "Installing to staging directory..."
            COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${CMAKE_BINARY_DIR}/installer_staging"
            COMMAND ${CMAKE_COMMAND} -E echo "Bundling dependencies..."
            COMMAND ${CMAKE_COMMAND} 
                -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/installer_staging
                -P "${CMAKE_BINARY_DIR}/BundleDependencies.cmake"
            COMMAND ${CMAKE_COMMAND} -E echo "Creating installer..."
            COMMAND "${NSIS_EXECUTABLE}" "${CMAKE_BINARY_DIR}/installer.nsi"
            DEPENDS pchat
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Building NSIS installer..."
        )
        
        message(STATUS "NSIS installer support enabled")
        message(STATUS "  Run 'cmake --build . --target installer' to create installer")
    else()
        message(STATUS "NSIS not found - installer target not available")
    endif()
endif()
