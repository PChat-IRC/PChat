cmake_minimum_required(VERSION 3.10)

project(pchat-fishlim C)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# fishlim has its own OpenSSL dependency for Diffie-Hellman 1080-bit key
# exchange. This is independent of the main application's TLS backend
# (PCHAT_SSL_BACKEND); BCrypt cannot perform DH1080 because Windows CNG
# requires DH key sizes to be a multiple of 64 bits.
pkg_check_modules(OPENSSL openssl)
if(NOT OPENSSL_FOUND)
    message(WARNING
        "fishlim plugin requested but OpenSSL was not found via pkg-config. "
        "Skipping. Install OpenSSL development headers to enable fishlim.")
    return()
endif()

# Plugin source files
set(PLUGIN_SOURCES
    plugin_pchat.c
    fish.c
    dh1080.c
    irc.c
    keystore.c
    utils.c
)

# Create shared library
add_library(fishlim MODULE ${PLUGIN_SOURCES})

# Set output name and properties
set_target_properties(fishlim PROPERTIES
    PREFIX ""
    OUTPUT_NAME "fishlim"
)

# Set appropriate suffix based on platform
if(WIN32)
    set_target_properties(fishlim PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(fishlim PROPERTIES SUFFIX ".dylib")
else()
    set_target_properties(fishlim PROPERTIES SUFFIX ".so")
endif()

# Include directories
target_include_directories(fishlim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${GLIB_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

# Link directories
target_link_directories(fishlim PRIVATE
    ${GLIB_LIBRARY_DIRS}
    ${OPENSSL_LIBRARY_DIRS}
)

# Link libraries
target_link_libraries(fishlim
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

# Compiler flags
target_compile_options(fishlim PRIVATE
    -Wall
    -Wextra
    ${GLIB_CFLAGS_OTHER}
    ${OPENSSL_CFLAGS_OTHER}
)

# Enable modern OpenSSL 1.1+ / 3.x API accessors
target_compile_definitions(fishlim PRIVATE
    HAVE_DH_SET0_PQG
    HAVE_DH_SET0_KEY
    HAVE_DH_GET0_KEY
    HAVE_DH_GET0_PQG
)

# Platform-specific link options
if(APPLE)
    target_link_options(fishlim PRIVATE 
        "-Wl,-undefined,dynamic_lookup"
    )
elseif(UNIX)
    target_link_options(fishlim PRIVATE 
        "-Wl,--unresolved-symbols=ignore-all"
    )
endif()

# Install plugin
install(TARGETS fishlim
    LIBRARY DESTINATION ${PCHAT_LIBDIR}
    RUNTIME DESTINATION ${PCHAT_LIBDIR}
    COMPONENT Plugins
)
