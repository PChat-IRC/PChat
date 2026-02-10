# wxWidgets frontend executable

# Find wxWidgets
# On MSYS2/MinGW, CMake's FindwxWidgets fails to parse wx-config output.
# Use wx-config directly to get compile and link flags.
# wx-config is a shell script, so we must invoke it through sh on Windows.
set(WX_USE_CONFIG FALSE)

if(MINGW OR MSYS OR CYGWIN)
    find_program(WX_CONFIG_EXECUTABLE
        NAMES wx-config-3.3 wx-config
    )
    find_program(SH_EXECUTABLE
        NAMES sh bash
    )
    if(WX_CONFIG_EXECUTABLE AND SH_EXECUTABLE)
        set(WX_USE_CONFIG TRUE)
        message(STATUS "Using wx-config: ${WX_CONFIG_EXECUTABLE}")
        message(STATUS "Using shell: ${SH_EXECUTABLE}")

        execute_process(
            COMMAND ${SH_EXECUTABLE} -c "${WX_CONFIG_EXECUTABLE} --version"
            OUTPUT_VARIABLE WX_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE WX_VERSION_RES
        )
        message(STATUS "wxWidgets version: ${WX_VERSION} (result: ${WX_VERSION_RES})")

        execute_process(
            COMMAND ${SH_EXECUTABLE} -c "${WX_CONFIG_EXECUTABLE} --cxxflags"
            OUTPUT_VARIABLE WX_CXX_FLAGS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE WX_CXXFLAGS_RES
        )

        execute_process(
            COMMAND ${SH_EXECUTABLE} -c "${WX_CONFIG_EXECUTABLE} --libs core,base,richtext,adv,stc"
            OUTPUT_VARIABLE WX_LINK_FLAGS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE WX_LIBS_RES
        )

        message(STATUS "wxWidgets cxxflags: [${WX_CXX_FLAGS}] (result: ${WX_CXXFLAGS_RES})")
        message(STATUS "wxWidgets libs: [${WX_LINK_FLAGS}] (result: ${WX_LIBS_RES})")

        if(WX_CXX_FLAGS STREQUAL "" OR WX_LINK_FLAGS STREQUAL "")
            message(WARNING "wx-config returned empty output, falling back to manual paths")
            # Derive from wx-config location: e.g. /clang64/bin/wx-config -> /clang64
            get_filename_component(WX_PREFIX "${WX_CONFIG_EXECUTABLE}" DIRECTORY)
            get_filename_component(WX_PREFIX "${WX_PREFIX}" DIRECTORY)
            set(WX_CXX_FLAGS "-I${WX_PREFIX}/lib/wx/include/msw-unicode-3.3 -I${WX_PREFIX}/include/wx-3.3 -DWXUSINGDLL -D__WXMSW__")
            set(WX_LINK_FLAGS "-L${WX_PREFIX}/lib -lwx_mswu_core-3.3 -lwx_baseu-3.3 -lwx_mswu_richtext-3.3 -lwx_mswu_adv-3.3 -lwx_mswu_stc-3.3")
            message(STATUS "Using manual wxWidgets cxxflags: ${WX_CXX_FLAGS}")
            message(STATUS "Using manual wxWidgets libs: ${WX_LINK_FLAGS}")
        endif()
    endif()
endif()

if(NOT WX_USE_CONFIG)
    find_package(wxWidgets REQUIRED COMPONENTS core base richtext adv stc)
    include(${wxWidgets_USE_FILE})
endif()

# Source files
set(WX_SOURCES
    fe-wx.cpp
    PchatApp.cpp
    MainWindow.cpp
    NetworkListDialog.cpp
    PreferencesDialog.cpp
    RawLogDialog.cpp
    DccDialog.cpp
    UrlGrabberDialog.cpp
    CharChartDialog.cpp
    FriendsListDialog.cpp
    IgnoreListDialog.cpp
    PluginDialog.cpp
    EditListDialog.cpp
    KeyboardShortcutsDialog.cpp
    TextEventsDialog.cpp
    SpellCheck.cpp
    BanListDialog.cpp
    ChannelListDialog.cpp
    JoinDialog.cpp
    palette.cpp
)

if(WIN32)
    add_executable(pchat-wx WIN32 ${WX_SOURCES} pchat-wx.rc)
else()
    add_executable(pchat-wx ${WX_SOURCES})
endif()

# Enable C++ for wxWidgets frontend
set_target_properties(pchat-wx PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    LINKER_LANGUAGE CXX
)

# Include directories
target_include_directories(pchat-wx PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_BINARY_DIR}/src/common
    ${CMAKE_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
)

# Hunspell spell checking
pkg_check_modules(HUNSPELL hunspell)
if(HUNSPELL_FOUND)
    target_include_directories(pchat-wx PRIVATE ${HUNSPELL_INCLUDE_DIRS})
    target_link_directories(pchat-wx PRIVATE ${HUNSPELL_LIBRARY_DIRS})
    target_link_libraries(pchat-wx PRIVATE ${HUNSPELL_LIBRARIES})
    target_compile_definitions(pchat-wx PRIVATE HAVE_HUNSPELL)
    message(STATUS "Hunspell found: ${HUNSPELL_LIBRARIES}")
else()
    message(WARNING "Hunspell not found - spell checking will be disabled")
endif()

# Compiler definitions
target_compile_definitions(pchat-wx PRIVATE
    HAVE_CONFIG_H
    LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
    UNICODE
    _UNICODE
)

# wxWidgets flags
if(WX_USE_CONFIG)
    # Append wx-config flags directly
    separate_arguments(WX_CXX_FLAGS_LIST UNIX_COMMAND "${WX_CXX_FLAGS}")
    target_compile_options(pchat-wx PRIVATE ${WX_CXX_FLAGS_LIST})

    separate_arguments(WX_LINK_FLAGS_LIST UNIX_COMMAND "${WX_LINK_FLAGS}")
    target_link_libraries(pchat-wx PRIVATE ${WX_LINK_FLAGS_LIST})
else()
    target_include_directories(pchat-wx PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_link_libraries(pchat-wx PRIVATE ${wxWidgets_LIBRARIES})
endif()

# Link directories
target_link_directories(pchat-wx PRIVATE
    ${GLIB_LIBRARY_DIRS}
)

# Link libraries
target_link_libraries(pchat-wx PRIVATE
    pchatcommon
    ${GLIB_LIBRARIES}
)

# OpenSSL (if used by common lib)
if(OPENSSL_FOUND)
    target_include_directories(pchat-wx PRIVATE ${OPENSSL_INCLUDE_DIRS})
    target_link_directories(pchat-wx PRIVATE ${OPENSSL_LIBRARY_DIRS})
    target_link_libraries(pchat-wx PRIVATE ${OPENSSL_LIBRARIES})
endif()

# Export symbols for plugins on Linux/Unix
if(UNIX AND NOT APPLE)
    target_link_options(pchat-wx PRIVATE "-Wl,--export-dynamic")
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(pchat-wx PRIVATE
        ws2_32
        wbemuuid
        ole32
        oleaut32
        winmm
    )
endif()

# macOS frameworks
if(APPLE)
    target_link_libraries(pchat-wx PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()

# Installation
install(TARGETS pchat-wx DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

# Copy icon files next to the executable for runtime loading
set(PCHAT_ICON_DIR "${CMAKE_SOURCE_DIR}/data/icons")
set(PCHAT_ICON_OUTPUT_DIR "$<TARGET_FILE_DIR:pchat-wx>/icons")
file(GLOB PCHAT_ICON_FILES "${PCHAT_ICON_DIR}/*.png")
add_custom_command(TARGET pchat-wx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PCHAT_ICON_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PCHAT_ICON_FILES} "${PCHAT_ICON_OUTPUT_DIR}"
    COMMENT "Copying PChat icons to build output"
)
