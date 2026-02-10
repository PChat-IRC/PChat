# Install icon files to ${CMAKE_INSTALL_PREFIX}/share/pchat/icons
file(GLOB PCHAT_ICON_PNG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.png")
install(FILES ${PCHAT_ICON_PNG_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pchat/icons
    COMPONENT Runtime
)
