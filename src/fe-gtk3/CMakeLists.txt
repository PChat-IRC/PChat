# GTK3 frontend executable

# Try to build WinRT notification DLL (MSVC only)
if(WIN32)
    add_subdirectory(notifications)
endif()

# Notification backend sources - platform specific
if(WIN32)
    if(MSVC AND HAVE_WINRT_NOTIFICATIONS)
        # On Windows with MSVC, try to use WinRT Toast notifications
        # notification-windows.c loads the WinRT DLL dynamically
        # and falls back to simple notifications if not available
        set(NOTIFICATION_SOURCES
            plugin-notification.c
            notifications/notification-windows.c
        )
        message(STATUS "Using WinRT notification backend with fallback")
    else()
        # On Windows with MinGW or if WinRT failed, use simple Shell_NotifyIcon
        set(NOTIFICATION_SOURCES
            plugin-notification.c
            notifications/notification-windows-simple.c
        )
        message(STATUS "Using simple Windows notification backend")
    endif()
elseif(UNIX AND NOT APPLE)
    # On Linux, use Freedesktop notifications (via DBus)
    set(NOTIFICATION_SOURCES
        plugin-notification.c
        notifications/notification-freedesktop.c
    )
elseif(APPLE)
    # On macOS, use native notifications (requires Objective-C)
    set(NOTIFICATION_SOURCES
        plugin-notification.c
        notifications/notification-osx.m
    )
else()
    # Fallback dummy backend
    set(NOTIFICATION_SOURCES
        plugin-notification.c
        notifications/notification-dummy.c
    )
endif()

if(WIN32)
    # Build as Windows GUI application (no console window)
    add_executable(pchat WIN32
        ascii.c
        banlist.c
        chanlist.c
        chanview.c
        custom-list.c
        dccgui.c
        editlist.c
        fe-gtk.c
        fkeys.c
        gtkutil.c
        ignoregui.c
        joind.c
        menu.c
        maingui.c
        notifygui.c
        palette.c
        pixmaps.c
        plugin-tray.c
        ${NOTIFICATION_SOURCES}
        rawlog.c
        servlistgui.c
        setup.c
        sexy-spell-entry.c
        textgui.c
        textview-chat.c
        urlgrab.c
        userlistgui.c
    )
else()
    add_executable(pchat
        ascii.c
        banlist.c
        chanlist.c
        chanview.c
        custom-list.c
        dccgui.c
        editlist.c
        fe-gtk.c
        fkeys.c
        gtkutil.c
        ignoregui.c
        joind.c
        menu.c
        maingui.c
        notifygui.c
        palette.c
        pixmaps.c
        plugin-tray.c
        ${NOTIFICATION_SOURCES}
        rawlog.c
        servlistgui.c
        setup.c
        sexy-spell-entry.c
        textgui.c
        textview-chat.c
        urlgrab.c
        userlistgui.c
    )
endif()

# Windows resource file
if(WIN32)
    # Convert version to comma-separated format (e.g., "1.5.4" -> "1,5,4")
    string(REPLACE "." "," VERSION_COMMAS "${PROJECT_VERSION}")
    
    # Configure the resource file
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/pchat.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/pchat.rc
        @ONLY
    )
    
    # Add resource file to executable
    target_sources(pchat PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/pchat.rc)
endif()

# Plugin GUI (optional)
if(ENABLE_PLUGIN)
    target_sources(pchat PRIVATE plugingui.c)
endif()

# ISO codes support (optional)
pkg_check_modules(ISO_CODES iso-codes)
if(ISO_CODES_FOUND)
    target_sources(pchat PRIVATE sexy-iso-codes.c)
    # Get ISO codes prefix from pkg-config
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=prefix iso-codes
        OUTPUT_VARIABLE ISO_CODES_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    target_compile_definitions(pchat PRIVATE 
        HAVE_ISO_CODES=1
        ISO_CODES_PREFIX="${ISO_CODES_PREFIX}"
    )
endif()

# Generate resources
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources.c
    COMMAND ${GLIB_COMPILE_RESOURCES}
        --target=${CMAKE_CURRENT_BINARY_DIR}/resources.c
        --sourcedir=${CMAKE_SOURCE_DIR}/data
        --generate-source
        ${CMAKE_SOURCE_DIR}/data/pchat.gresource.xml
    DEPENDS ${CMAKE_SOURCE_DIR}/data/pchat.gresource.xml
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(pchat PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/resources.c)

# Include directories
target_include_directories(pchat PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_BINARY_DIR}/src/common
    ${GTK3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(pchat PRIVATE
    HAVE_CONFIG_H
    LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
)

# Link libraries
target_link_directories(pchat PRIVATE
    ${GTK3_LIBRARY_DIRS}
    ${GLIB_LIBRARY_DIRS}
)

target_link_libraries(pchat PRIVATE
    pchatcommon
    ${GTK3_LIBRARIES}
    ${GLIB_LIBRARIES}
)

# macOS frameworks for native notifications
if(APPLE)
    target_link_libraries(pchat PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()

# Export symbols for plugins on Linux/Unix
if(UNIX AND NOT APPLE)
    target_link_options(pchat PRIVATE "-Wl,--export-dynamic")
endif()

# Windows-specific WMI libraries (needed by common library sysinfo backend)
if(WIN32)
    target_link_libraries(pchat PRIVATE
        wbemuuid
        ole32
        oleaut32
    )
endif()

if(LIBNOTIFY_FOUND)
    target_include_directories(pchat PRIVATE ${LIBNOTIFY_INCLUDE_DIRS})
    target_link_directories(pchat PRIVATE ${LIBNOTIFY_LIBRARY_DIRS})
    target_link_libraries(pchat PRIVATE ${LIBNOTIFY_LIBRARIES})
endif()

if(LIBCANBERRA_FOUND)
    target_include_directories(pchat PRIVATE ${LIBCANBERRA_INCLUDE_DIRS})
    target_link_directories(pchat PRIVATE ${LIBCANBERRA_LIBRARY_DIRS})
    target_link_libraries(pchat PRIVATE ${LIBCANBERRA_LIBRARIES})
endif()

# macOS integration
if(GTK_MAC_INTEGRATION_FOUND)
    target_include_directories(pchat PRIVATE ${GTK_MAC_INTEGRATION_INCLUDE_DIRS})
    target_link_directories(pchat PRIVATE ${GTK_MAC_INTEGRATION_LIBRARY_DIRS})
    target_link_libraries(pchat PRIVATE ${GTK_MAC_INTEGRATION_LIBRARIES})
endif()

# Windows resource file
if(WIN32)
    # TODO: Add Windows resource compilation if needed
endif()

# Installation
install(TARGETS pchat DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

# macOS Bundle targets
if(APPLE)
    # Bundle with ad-hoc signing (for local testing)
    add_custom_target(bundle
        COMMAND ${CMAKE_SOURCE_DIR}/osx/create-bundle.sh ${PROJECT_VERSION} ${CMAKE_BINARY_DIR}
        DEPENDS pchat
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Creating macOS application bundle (ad-hoc signed)..."
    )
    
    # Bundle with proper code signing (requires CODESIGN_IDENTITY to be set)
    add_custom_target(bundle-signed
        COMMAND ${CMAKE_SOURCE_DIR}/osx/create-bundle.sh 
                ${PROJECT_VERSION} 
                ${CMAKE_BINARY_DIR} 
                "$ENV{CODESIGN_IDENTITY}"
        DEPENDS pchat
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Creating macOS application bundle (signed)..."
        VERBATIM
    )
    
    # Create DMG (requires bundle to exist)
    add_custom_target(dmg
        COMMAND ${CMAKE_SOURCE_DIR}/osx/create-dmg.sh ${PROJECT_VERSION} ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Creating distributable DMG..."
    )
    
    message(STATUS "")
    message(STATUS "macOS bundle targets:")
    message(STATUS "  make bundle        - Create bundle with ad-hoc signing (local testing)")
    message(STATUS "  make bundle-signed - Create bundle with proper code signing")
    message(STATUS "                       Set CODESIGN_IDENTITY env var first:")
    message(STATUS "                       export CODESIGN_IDENTITY=\"Developer ID Application: Your Name\"")
    message(STATUS "  make dmg           - Create DMG from existing bundle")
    message(STATUS "")
endif()
