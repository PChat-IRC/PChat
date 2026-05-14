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
        tray-backend.c
        tray-windows.c
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
        tray-backend.c
        $<IF:$<BOOL:${APPLE}>,tray-macos.m,tray-linux.c>
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

# Add update checker source if enabled
if(HAVE_UPDATE_CHECKER)
    target_sources(pchat PRIVATE update-checker.c)
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
        OUTPUT_VARIABLE ISO_CODES_BUILD_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # On Windows installs we ship the iso-codes data files inside PChat's own
    # share/ directory (see install(DIRECTORY ...) below), so point the
    # compile-time ISO_CODES_PREFIX macro at the PChat install prefix. On
    # other platforms iso-codes is a system package, so use its real prefix.
    if(WIN32)
        set(ISO_CODES_RUNTIME_PREFIX "${CMAKE_INSTALL_PREFIX}")
    else()
        set(ISO_CODES_RUNTIME_PREFIX "${ISO_CODES_BUILD_PREFIX}")
    endif()
    target_compile_definitions(pchat PRIVATE
        HAVE_ISO_CODES=1
        ISO_CODES_PREFIX="${ISO_CODES_RUNTIME_PREFIX}"
    )

    # Bundle the iso-codes data alongside PChat. sexy-iso-codes.c reads
    # share/xml/iso-codes/iso_{639,3166}.xml, and gettext loads
    # share/locale/<lang>/LC_MESSAGES/iso_{639,3166}.mo via bindtextdomain().
    if(WIN32 AND EXISTS "${ISO_CODES_BUILD_PREFIX}/share/xml/iso-codes")
        install(DIRECTORY "${ISO_CODES_BUILD_PREFIX}/share/xml/iso-codes"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/xml"
                COMPONENT Runtime
                FILES_MATCHING PATTERN "iso_639.xml" PATTERN "iso_3166.xml")
        # Translations: only iso_639*.mo and iso_3166*.mo, scattered across
        # locale directories. Install the whole share/locale tree filtered to
        # those domains.
        install(DIRECTORY "${ISO_CODES_BUILD_PREFIX}/share/locale/"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/locale"
                COMPONENT Runtime
                FILES_MATCHING
                    PATTERN "iso_639.mo"
                    PATTERN "iso_639_3.mo"
                    PATTERN "iso_3166.mo"
                    PATTERN "iso_3166-2.mo")
    endif()
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

# libayatana-appindicator3 for Linux tray support
if(UNIX AND NOT APPLE AND NOT WIN32)
    pkg_check_modules(APPINDICATOR ayatana-appindicator3-0.1)
    if(NOT APPINDICATOR_FOUND)
        # Try alternative package name on some distros
        pkg_check_modules(APPINDICATOR ayatana-indicator3-0.4)
    endif()
    if(APPINDICATOR_FOUND)
        message(STATUS "Found libayatana-appindicator3: ${APPINDICATOR_LIBRARIES}")
    else()
        message(WARNING "libayatana-appindicator3 not found - tray icon may not work on modern Linux desktops")
    endif()
endif()

# Include directories
target_include_directories(pchat PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_BINARY_DIR}/src/common
    ${GTK3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${APPINDICATOR_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(pchat PRIVATE
    HAVE_CONFIG_H
    LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
    # GTK4 migration prep: forbid use of GTK/GDK APIs deprecated in 3.x and
    # require direct field access via accessor functions.
    GDK_DISABLE_DEPRECATED
    GTK_DISABLE_DEPRECATED
    GSEAL_ENABLE
)

# Link libraries
target_link_directories(pchat PRIVATE
    ${GTK3_LIBRARY_DIRS}
    ${GLIB_LIBRARY_DIRS}
    ${LIBSOUP_LIBRARY_DIRS}
    ${LIBXML2_LIBRARY_DIRS}
)

target_link_libraries(pchat PRIVATE
    pchatcommon
    ${GTK3_LDFLAGS}
    ${GLIB_LDFLAGS}
    ${APPINDICATOR_LDFLAGS}
    ${LIBSOUP_LDFLAGS}
    ${LIBXML2_LDFLAGS}
)

# When statically linking GLib/GTK on Windows, expose those symbols from
# pchat.exe so plugin DLLs can resolve them at runtime against the main exe
# instead of bundling their own static copy (two parallel GType registries
# would otherwise crash the process the first time a plugin creates a GTK
# widget). Generates libpchat.dll.a as the import library plugins link to.
pchat_configure_exe_for_plugins(pchat)

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
        opengl32
    )
endif()

if(LIBNOTIFY_FOUND)
    target_include_directories(pchat PRIVATE ${LIBNOTIFY_INCLUDE_DIRS})
    target_link_directories(pchat PRIVATE ${LIBNOTIFY_LIBRARY_DIRS})
    target_link_libraries(pchat PRIVATE ${LIBNOTIFY_LDFLAGS})
endif()

if(LIBCANBERRA_FOUND)
    target_include_directories(pchat PRIVATE ${LIBCANBERRA_INCLUDE_DIRS})
    target_link_directories(pchat PRIVATE ${LIBCANBERRA_LIBRARY_DIRS})
    target_link_libraries(pchat PRIVATE ${LIBCANBERRA_LDFLAGS})
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
