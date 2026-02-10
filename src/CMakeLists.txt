# Source directories
add_subdirectory(common)

if(ENABLE_GTKFE)
    add_subdirectory(fe-gtk3)
endif()

if(ENABLE_WXFE)
    add_subdirectory(fe-wx)
endif()

if(ENABLE_TEXTFE)
    add_subdirectory(fe-text)
endif()
