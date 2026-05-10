# Data files installation
add_subdirectory(icons)
add_subdirectory(man)
add_subdirectory(misc)
add_subdirectory(pkgconfig)

# Install gresource XML
install(FILES pchat.gresource.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/pchat COMPONENT Runtime)

# Install OpenSSL cert.pem on Windows for standalone SSL verification (OpenSSL backend only)
if(WIN32 AND PCHAT_SSL_BACKEND STREQUAL "openssl")
    # Find cert.pem in MSYS2/MinGW OpenSSL directory
    find_file(OPENSSL_CERT_FILE
        NAMES cert.pem ca-bundle.crt
        PATHS
            "$ENV{MSYSTEM_PREFIX}/etc/ssl"
            "$ENV{MSYSTEM_PREFIX}/ssl"
            "/clang64/etc/ssl"
            "/mingw64/etc/ssl"
            "/ucrt64/etc/ssl"
        NO_DEFAULT_PATH
    )
    
    if(OPENSSL_CERT_FILE)
        message(STATUS "Found OpenSSL cert.pem: ${OPENSSL_CERT_FILE}")
        if(ENABLE_PORTABLE)
            # Portable mode: install cert.pem to root directory
            install(FILES ${OPENSSL_CERT_FILE} DESTINATION . COMPONENT Runtime)
        else()
            # Normal mode: install to share directory
            install(FILES ${OPENSSL_CERT_FILE} DESTINATION ${CMAKE_INSTALL_DATADIR}/pchat COMPONENT Runtime)
        endif()
    else()
        message(WARNING "OpenSSL cert.pem not found - SSL certificate verification may fail")
    endif()
endif()

# Create portable-mode marker file for portable builds
if(ENABLE_PORTABLE AND WIN32)
    install(CODE "file(WRITE \"\${CMAKE_INSTALL_PREFIX}/portable-mode\" \"\")" COMPONENT Runtime)
endif()

