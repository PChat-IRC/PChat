set(EXTRA_PATCHES "")
if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
    list(APPEND EXTRA_PATCHES fix_clang-cl_build.patch)
endif()

vcpkg_from_gitlab(
    OUT_SOURCE_PATH SOURCE_PATH
    GITLAB_URL https://gitlab.freedesktop.org
    REPO cairo/cairo
    REF "${VERSION}"
    SHA512 663e6edf2718e8205e30ba309ac609ced9e88e6e1ec857fc48b345dfce82b044d58ec6b4a2d2b281fba30a659a368625ea7501f8b43fe26c137a7ebffdbaac91
    PATCHES
        msvc-convenience.diff
        ${EXTRA_PATCHES}
)

if("fontconfig" IN_LIST FEATURES)
    list(APPEND OPTIONS -Dfontconfig=enabled)
else()
    list(APPEND OPTIONS -Dfontconfig=disabled)
endif()

if("freetype" IN_LIST FEATURES)
    list(APPEND OPTIONS -Dfreetype=enabled)
else()
    list(APPEND OPTIONS -Dfreetype=disabled)
endif()

if ("x11" IN_LIST FEATURES)
    message(WARNING "You will need to install Xorg dependencies to use feature x11:\nsudo apt install libx11-dev libxft-dev libxext-dev\n")
    list(APPEND OPTIONS -Dxlib=enabled)
else()
    list(APPEND OPTIONS -Dxlib=disabled)
endif()
list(APPEND OPTIONS -Dxcb=disabled)
list(APPEND OPTIONS -Dxlib-xcb=disabled)

if("gobject" IN_LIST FEATURES)
    list(APPEND OPTIONS -Dglib=enabled)
else()
    list(APPEND OPTIONS -Dglib=disabled)
endif()

if("lzo" IN_LIST FEATURES)
    list(APPEND OPTIONS -Dlzo=enabled)
else()
    list(APPEND OPTIONS -Dlzo=disabled)
endif()

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${OPTIONS}
        -Dtests=disabled
        -Dzlib=enabled
        -Dpng=enabled
        -Dspectre=auto
        -Dgtk2-utils=disabled
        -Dsymbol-lookup=disabled
)
vcpkg_install_meson()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/cairo/cairo.h" "defined(CAIRO_WIN32_STATIC_BUILD)" "1")
endif()

vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static" OR NOT VCPKG_TARGET_IS_WINDOWS)
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()

# --- PChat overlay patch ---------------------------------------------------
# On MinGW + static, libcairo.a contains C++ object code from the DirectWrite
# font backend (cairo-dwrite-font.cpp). Upstream cairo.pc only lists C runtime
# libs, so any consumer that links with `gcc` (e.g. meson's `links:` checks
# in pango, or PChat's own gcc-driven link) fails with undefined references
# to operator new[]/delete[], __gxx_personality_seh0,
# __cxa_throw_bad_array_new_length and the C++ typeinfo vtables.
#
# Append `-lstdc++` to the *public* `Libs:` line of every installed cairo
# `.pc` file. We can't use `Libs.private:` because meson's dependency() /
# `links:` checks resolve dependencies via `pkg-config --libs` (non-static
# mode) and therefore skip private libs entirely.
if(VCPKG_TARGET_IS_MINGW AND VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    set(_pc_files
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-ft.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-fc.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-png.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-pdf.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-ps.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-svg.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-win32.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-win32-font.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-script.pc"
        "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/cairo-script-interpreter.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-ft.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-fc.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-png.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-pdf.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-ps.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-svg.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-win32.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-win32-font.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-script.pc"
        "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/cairo-script-interpreter.pc"
    )
    foreach(_pc IN LISTS _pc_files)
        if(EXISTS "${_pc}")
            file(READ "${_pc}" _contents)
            if(NOT _contents MATCHES "Libs:[^\r\n]*-lstdc\\+\\+")
                if(_contents MATCHES "(\r?\n)Libs:[^\r\n]*")
                    string(REGEX REPLACE
                        "(\r?\n)(Libs:[^\r\n]*)"
                        "\\1\\2 -lstdc++"
                        _contents "${_contents}")
                else()
                    string(APPEND _contents "\nLibs: -lstdc++\n")
                endif()
                file(WRITE "${_pc}" "${_contents}")
                message(STATUS "PChat overlay: appended -lstdc++ to Libs in ${_pc}")
            endif()
        endif()
    endforeach()
endif()
# --- end PChat overlay patch -----------------------------------------------

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING" "${SOURCE_PATH}/COPYING-LGPL-2.1" "${SOURCE_PATH}/COPYING-MPL-1.1")
