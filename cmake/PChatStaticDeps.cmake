# PChatStaticDeps.cmake
#
# Helpers shared between the main executable and the plugin DLLs to make a
# fully-static (vcpkg *-static triplet) MinGW build self-contained.
#
# Plugins re-run pkg_check_modules() in their own subdirectories, which
# overwrites the shared *_LDFLAGS variables that the root CMakeLists.txt
# painstakingly replaced with their _STATIC_ counterparts. Each plugin DLL
# then ends up with import-table entries against libgtk-3-0.dll /
# libglib-2.0-0.dll / libmpg123-0.dll / ... -- DLLs that don't exist in a
# fully static install. Windows reports the resulting LoadLibrary failure
# as "The specified module could not be found", referring to the missing
# transitive dependency rather than the plugin itself.
#
# Each plugin should call pchat_use_static_pkg_vars() with the set of
# pkg-config prefixes it relies on, and pchat_apply_static_compile_defs()
# on its target, immediately after its pkg_check_modules() calls.

# Strip MinGW runtime libs that vcpkg's pkg-config files inject into
# _STATIC_LDFLAGS. They resolve to the *.dll.a import libs and silently
# pull libgcc_s_seh-1.dll / libstdc++-6.dll / libwinpthread-1.dll into
# the link, defeating -static-libgcc / -static-libstdc++.
function(pchat_strip_runtime_libs out_var in_list)
    set(_filtered "")
    foreach(_item IN LISTS in_list)
        if(NOT _item MATCHES "^-l(gcc_s|stdc\\+\\+|pthread)$" AND
           NOT _item STREQUAL "gcc_s" AND
           NOT _item STREQUAL "stdc++" AND
           NOT _item STREQUAL "pthread")
            list(APPEND _filtered "${_item}")
        endif()
    endforeach()
    set(${out_var} "${_filtered}" PARENT_SCOPE)
endfunction()

# For each pkg-config prefix in ARGN, copy the _STATIC_* variables over the
# default ones so target_link_libraries(${PKG}_LDFLAGS) pulls in the full
# static dependency chain. Stripping is applied so the MinGW runtime stays
# static. No-op when PCHAT_STATIC_LINK is false.
function(pchat_use_static_pkg_vars)
    if(NOT PCHAT_STATIC_LINK)
        return()
    endif()
    foreach(_pkg IN LISTS ARGN)
        if(${_pkg}_STATIC_LDFLAGS)
            pchat_strip_runtime_libs(_ldflags   "${${_pkg}_STATIC_LDFLAGS}")
            pchat_strip_runtime_libs(_libraries "${${_pkg}_STATIC_LIBRARIES}")
            set(${_pkg}_LDFLAGS      "${_ldflags}"                       PARENT_SCOPE)
            set(${_pkg}_LIBRARIES    "${_libraries}"                     PARENT_SCOPE)
            set(${_pkg}_LIBRARY_DIRS "${${_pkg}_STATIC_LIBRARY_DIRS}"    PARENT_SCOPE)
            set(${_pkg}_INCLUDE_DIRS "${${_pkg}_STATIC_INCLUDE_DIRS}"    PARENT_SCOPE)
            set(${_pkg}_CFLAGS       "${${_pkg}_STATIC_CFLAGS}"          PARENT_SCOPE)
            set(${_pkg}_CFLAGS_OTHER "${${_pkg}_STATIC_CFLAGS_OTHER}"    PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

# Apply the GLib/GTK static-build compile defs to a target. Without these
# the headers annotate every public symbol as __declspec(dllimport) and
# generate undefined references to __imp_g_*/__imp_gtk_*/__imp_fribidi_*
# against the static archives. No-op outside of static Windows builds.
function(pchat_apply_static_compile_defs target)
    if(NOT (PCHAT_STATIC_LINK AND WIN32))
        return()
    endif()
    target_compile_definitions(${target} PRIVATE
        GLIB_STATIC_COMPILATION
        GOBJECT_STATIC_COMPILATION
        GIO_STATIC_COMPILATION
        GMODULE_STATIC_COMPILATION
        FRIBIDI_LIB_STATIC
        PCRE2_STATIC
    )
endfunction()

# Configure the main pchat executable so plugin DLLs can resolve glib/gtk/etc.
# symbols against IT at runtime, instead of bundling their own static copies.
#
# When pchat.exe links GLib/GTK statically AND a plugin DLL also links them
# statically, the process ends up with two independent GType registries,
# two GObject metadata caches, two GLib loggers, etc. The first GTK widget
# created by the plugin then trips a critical inside g_log and the loader
# aborts the process (see SIGTRAP in g_log_structured_array originating
# from the plugin DLL).
#
# Setting ENABLE_EXPORTS on the exe target makes CMake/MinGW pass
# `-Wl,--export-all-symbols -Wl,--out-implib=libpchat.dll.a` so the
# statically-linked glib/gtk symbols become re-exports of pchat.exe.
# Plugins then link the generated import library and Windows resolves
# their glib/gtk imports back into pchat.exe at LoadLibrary time --
# a single shared runtime, no duplicate type system.
function(pchat_configure_exe_for_plugins target)
    if(NOT (PCHAT_STATIC_LINK AND WIN32))
        return()
    endif()

    # ENABLE_EXPORTS makes CMake/MinGW pass `-Wl,--export-all-symbols
    # -Wl,--out-implib=<libpchat.dll.a>` so the executable produces a usable
    # import library and re-exports its symbols. By itself this is NOT
    # enough: ld only links objects from a static archive that satisfy
    # outstanding undefined references. Plugin DLLs reference many
    # glib symbols (g_ptr_array_*, g_key_file_*, g_base64_*, ...) that
    # pchat itself does not use, so those objects never enter pchat.exe
    # and therefore never appear in libpchat.dll.a. The plugin links
    # then fail with hundreds of "undefined reference to g_*".
    #
    # The fix is to force-link the relevant glib static archives in
    # their entirety via `-Wl,--whole-archive`, so every object becomes
    # part of pchat.exe and gets re-exported.
    #
    # NOTE: We deliberately scope this to the GLib stack only. The
    # audioplayer plugin used to need GTK widgets re-exported through
    # pchat.exe but it now ships its own wxWidgets GUI, and no other
    # plugin (checksum, fishlim, lua, sysinfo, winamp, python) calls
    # into GTK/Pango/Cairo/Atk. Pulling those into pchat.exe under
    # --whole-archive used to drag windres-compiled VERSIONINFO
    # resource objects (e.g. libgtk-3.a's gtk.rc) into pchat.exe's
    # .rsrc section and collide with our own pchat.rc VERSIONINFO,
    # producing `ld: .rsrc merge failure: duplicate leaf`.
    set_target_properties(${target} PROPERTIES ENABLE_EXPORTS TRUE)

    # Libraries whose full contents must be pulled into pchat.exe so
    # plugins can satisfy their imports against libpchat.dll.a. The
    # set is intentionally limited to the GLib stack to avoid pulling
    # in any windres-compiled resource objects (e.g. libintl.a's
    # libintl.res.o, libgtk-3.a's gtk.rc resource) which would collide
    # with our own pchat.rc VERSIONINFO during link
    # (`ld: .rsrc merge failure: duplicate leaf`). Plugins use the
    # `_()` macro that delegates to `pchat_gettext` on the plugin
    # handle, so they do not need libintl symbols re-exported.
    set(_runtime_libs
        gio-2.0 gobject-2.0 gmodule-2.0 gthread-2.0 glib-2.0
    )

    # Build a single linker argument string. Use the comma-separated
    # form of -Wl so every entry is grouped between --whole-archive and
    # --no-whole-archive even after CMake/ninja word-splits the option.
    set(_wa_arg "-Wl,--whole-archive")
    foreach(_lib IN LISTS _runtime_libs)
        string(APPEND _wa_arg ",-l${_lib}")
    endforeach()
    string(APPEND _wa_arg ",--no-whole-archive")

    # Place the whole-archive group BEFORE the rest of the link line so
    # ld sees the forced symbols first; later occurrences of the same
    # -l<lib> in pkg-config flags add nothing (objects already consumed)
    # and don't trigger multiple-definition errors.
    target_link_options(${target} BEFORE PRIVATE "${_wa_arg}")

    # ENABLE_EXPORTS on an .exe target tells CMake/MinGW to emit
    # `-Wl,--out-implib=libpchat.dll.a`, but on this compiler/CMake
    # combo it does NOT automatically pass `--export-all-symbols`.
    # Without that flag MinGW ld writes a near-empty import library
    # (only auto-detected dllexport-marked symbols), so plugin DLLs
    # then fail to resolve g_free, g_ptr_array_*, etc. against it.
    # Force the flag so every global symbol -- including the glib
    # objects pulled in via --whole-archive above -- ends up in
    # libpchat.dll.a.
    target_link_options(${target} PRIVATE "-Wl,--export-all-symbols")
endfunction()

# Link a plugin against the runtime libraries provided by pchat.exe.
#
# On Windows static builds: links the `pchat` exe target so glib/gtk/gio/
# gmodule symbols are resolved against pchat.exe's import library at link
# time (and against pchat.exe in memory at load time). The plugin DLL
# therefore does NOT bundle a duplicate copy of GLib/GTK. The pkg names
# in ARGN are ignored in this mode.
#
# On all other configurations (shared builds, non-Windows): falls through
# to passing each pkg's _LDFLAGS to target_link_libraries as usual.
#
# Plugin-private dependencies (mpg123, FAudio, openssl, lua, ...) should
# still be linked directly with target_link_libraries -- only the
# glib/gtk/gio/gmodule layer is shared with the main exe.
function(pchat_link_plugin_runtime target)
    if(PCHAT_STATIC_LINK AND WIN32)
        target_link_libraries(${target} PRIVATE pchat)
    else()
        foreach(_pkg IN LISTS ARGN)
            if(${_pkg}_LDFLAGS)
                target_link_libraries(${target} PRIVATE ${${_pkg}_LDFLAGS})
            endif()
        endforeach()
    endif()
endfunction()
