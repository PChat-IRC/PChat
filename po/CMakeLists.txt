# Translations
file(GLOB PO_FILES *.po)

# Find xgettext for POT file generation
find_program(XGETTEXT_EXECUTABLE xgettext)

# Create POT file generation target if xgettext is available
if(XGETTEXT_EXECUTABLE)
    # Read source files from POTFILES.in
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/POTFILES.in POTFILES_LIST)
    
    # Convert relative paths to absolute paths
    set(POTFILES_ABSOLUTE "")
    foreach(POTFILE ${POTFILES_LIST})
        # Skip comments and empty lines
        if(NOT POTFILE MATCHES "^#" AND NOT POTFILE STREQUAL "")
            # Adjust paths from old structure to new structure
            string(REPLACE "src/fe-gtk/" "src/fe-gtk3/" POTFILE_ADJUSTED "${POTFILE}")
            string(REPLACE "src/common/xchat.c" "src/common/pchat.c" POTFILE_ADJUSTED "${POTFILE_ADJUSTED}")
            
            if(EXISTS "${CMAKE_SOURCE_DIR}/${POTFILE_ADJUSTED}")
                list(APPEND POTFILES_ABSOLUTE "${CMAKE_SOURCE_DIR}/${POTFILE_ADJUSTED}")
            endif()
        endif()
    endforeach()
    
    # Add custom target to update POT file
    add_custom_target(update-pot
        COMMAND ${XGETTEXT_EXECUTABLE}
            --keyword=_ --keyword=N_
            --from-code=UTF-8
            --package-name=pchat
            --package-version=${PROJECT_VERSION}
            --msgid-bugs-address=https://github.com/PChat-IRC/PChat/issues
            --copyright-holder="PChat Developers"
            --output=${CMAKE_CURRENT_SOURCE_DIR}/pchat.pot
            ${POTFILES_ABSOLUTE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Extracting translatable strings to pchat.pot"
        VERBATIM
    )
    
    message(STATUS "xgettext found: POT file can be updated with 'make update-pot'")
else()
    message(STATUS "xgettext not found: POT file generation unavailable")
endif()

foreach(PO_FILE ${PO_FILES})
    get_filename_component(LANG ${PO_FILE} NAME_WE)
    set(MO_FILE ${CMAKE_CURRENT_BINARY_DIR}/${LANG}.mo)
    
    add_custom_command(
        OUTPUT ${MO_FILE}
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${MO_FILE} ${PO_FILE}
        DEPENDS ${PO_FILE}
    )
    
    install(FILES ${MO_FILE}
        DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/${LANG}/LC_MESSAGES
        RENAME pchat.mo
        COMPONENT Translations
    )
    
    list(APPEND MO_FILES ${MO_FILE})
endforeach()

add_custom_target(translations ALL DEPENDS ${MO_FILES})
