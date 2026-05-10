# Common library
add_library(pchatcommon STATIC
    cfgfiles.c
    chanopt.c
    ctcp.c
    dcc.c
    debug-log.c
    pchat.c
    history.c
    ignore.c
    inbound.c
    marshal.c
    modes.c
    network.c
    notify.c
    outbound.c
    plugin.c
    plugin-identd.c
    plugin-timer.c
    proto-irc.c
    scram.c
    server.c
    servlist.c
    text.c
    tree.c
    url.c
    userlist.c
    util.c
)

# Platform-specific sources
if(WIN32)
    target_sources(pchatcommon PRIVATE
        sysinfo/win32/backend.c
    )
    target_include_directories(pchatcommon PRIVATE sysinfo)
endif()

# TLS / crypto backend
if(PCHAT_SSL_BACKEND STREQUAL "openssl")
    target_sources(pchatcommon PRIVATE ssl_openssl.c pchat_crypto_openssl.c)
    target_include_directories(pchatcommon PUBLIC ${OPENSSL_INCLUDE_DIRS})
    target_link_libraries(pchatcommon PUBLIC ${OPENSSL_LIBRARIES})
    target_link_directories(pchatcommon PUBLIC ${OPENSSL_LIBRARY_DIRS})
elseif(PCHAT_SSL_BACKEND STREQUAL "schannel")
    target_sources(pchatcommon PRIVATE ssl_schannel.c pchat_crypto_schannel.c)
    target_link_libraries(pchatcommon PUBLIC ${SCHANNEL_LIBRARIES})
endif()

# Compile definitions
target_compile_definitions(pchatcommon PRIVATE HAVE_CONFIG_H)

# Include directories
target_include_directories(pchatcommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(pchatcommon PRIVATE
    ${GLIB_INCLUDE_DIRS}
)

target_link_libraries(pchatcommon PRIVATE
    ${GLIB_LIBRARIES}
)

# Windows socket library
if(WIN32)
    target_link_libraries(pchatcommon PRIVATE ws2_32)
endif()

if(LIBPROXY_FOUND)
    target_include_directories(pchatcommon PRIVATE ${LIBPROXY_INCLUDE_DIRS})
    target_link_libraries(pchatcommon PRIVATE ${LIBPROXY_LIBRARIES})
endif()

# Generate text events using Python script
find_program(PYTHON_EXECUTABLE python3 REQUIRED)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/textevents.h ${CMAKE_CURRENT_BINARY_DIR}/textenums.h
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/make-te.py ${CMAKE_CURRENT_SOURCE_DIR}/textevents.in ${CMAKE_CURRENT_BINARY_DIR}/textevents.h ${CMAKE_CURRENT_BINARY_DIR}/textenums.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/make-te.py ${CMAKE_CURRENT_SOURCE_DIR}/textevents.in
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Generate marshal files
find_program(GLIB_GENMARSHAL glib-genmarshal REQUIRED)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/marshal.h
    COMMAND ${GLIB_GENMARSHAL} --prefix=_pchat_marshal --header ${CMAKE_CURRENT_SOURCE_DIR}/marshalers.list > ${CMAKE_CURRENT_BINARY_DIR}/marshal.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/marshalers.list
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/marshal.c
    COMMAND ${GLIB_GENMARSHAL} --prefix=_pchat_marshal --body ${CMAKE_CURRENT_SOURCE_DIR}/marshalers.list > ${CMAKE_CURRENT_BINARY_DIR}/marshal.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/marshalers.list
)

# Create a custom target for marshal generation
add_custom_target(generate_marshal
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/marshal.h ${CMAKE_CURRENT_BINARY_DIR}/marshal.c
)

# Add generated files as dependencies
target_sources(pchatcommon PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/textevents.h
    ${CMAKE_CURRENT_BINARY_DIR}/textenums.h
    ${CMAKE_CURRENT_BINARY_DIR}/marshal.h
    ${CMAKE_CURRENT_BINARY_DIR}/marshal.c
)

# Ensure marshal is generated before pchatcommon is built
add_dependencies(pchatcommon generate_marshal)

# D-Bus support
if(USE_DBUS)
    add_subdirectory(dbus)
    target_link_libraries(pchatcommon PRIVATE pchat-dbus)
endif()

# Install headers if plugin support is enabled
if(ENABLE_PLUGIN)
    install(FILES pchat-plugin.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pchat)
endif()
