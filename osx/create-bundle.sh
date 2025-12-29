#!/bin/bash
# Create macOS application bundle for PChat with all dependencies

set -e

# Configuration
APP_NAME="PChat"
BUNDLE_NAME="${APP_NAME}.app"
EXECUTABLE_NAME="pchat"
VERSION="${1:-1.5.4}"
BUILD_DIR="${2:-build}"
SIGN_IDENTITY="${3:--}"  # Default to ad-hoc signing if not specified
SOURCE_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "Creating ${APP_NAME} application bundle..."
echo "Version: ${VERSION}"
echo "Build directory: ${BUILD_DIR}"
if [ "$SIGN_IDENTITY" != "-" ]; then
    echo "Code signing identity: ${SIGN_IDENTITY}"
else
    echo "Code signing: ad-hoc (for local testing only)"
fi

# Create bundle structure
BUNDLE_DIR="${BUILD_DIR}/${BUNDLE_NAME}"
CONTENTS_DIR="${BUNDLE_DIR}/Contents"
MACOS_DIR="${CONTENTS_DIR}/MacOS"
RESOURCES_DIR="${CONTENTS_DIR}/Resources"
FRAMEWORKS_DIR="${CONTENTS_DIR}/Frameworks"
LIB_DIR="${CONTENTS_DIR}/lib"

echo "Creating bundle directories..."
rm -rf "${BUNDLE_DIR}"
mkdir -p "${MACOS_DIR}"
mkdir -p "${RESOURCES_DIR}"
mkdir -p "${FRAMEWORKS_DIR}"
mkdir -p "${LIB_DIR}"

# Copy executable
echo "Copying executable..."
cp "${BUILD_DIR}/src/fe-gtk3/${EXECUTABLE_NAME}" "${MACOS_DIR}/${APP_NAME}"
chmod +x "${MACOS_DIR}/${APP_NAME}"

# Generate Info.plist
echo "Creating Info.plist..."
sed "s/@VERSION@/${VERSION}/g" "${SOURCE_DIR}/osx/Info.plist.in" > "${CONTENTS_DIR}/Info.plist"

# Copy icon if it exists
if [ -f "${SOURCE_DIR}/osx/pchat.icns" ]; then
    echo "Copying icon..."
    cp "${SOURCE_DIR}/osx/pchat.icns" "${RESOURCES_DIR}/PChat.icns"
fi

# Function to copy a library and its dependencies recursively
copy_lib_and_deps() {
    local lib_path="$1"
    local lib_name=$(basename "$lib_path")
    local target_dir="$2"
    
    # Skip if already copied
    if [ -f "${target_dir}/${lib_name}" ]; then
        return
    fi
    
    # Skip system libraries
    if [[ "$lib_path" == /System/* ]] || [[ "$lib_path" == /usr/lib/* ]] || [[ "$lib_path" == /usr/X11/* ]]; then
        return
    fi
    
    echo "  Copying: $lib_name"
    cp "$lib_path" "${target_dir}/"
    chmod +w "${target_dir}/${lib_name}"
    
    # Get dependencies
    local deps=$(otool -L "$lib_path" | grep -E "^\s+/" | awk '{print $1}')
    
    for dep in $deps; do
        # Skip the library itself
        if [[ "$dep" == "$lib_path" ]]; then
            continue
        fi
        
        # Recursively copy dependencies
        if [ -f "$dep" ]; then
            copy_lib_and_deps "$dep" "$target_dir"
        fi
    done
}

# Get all library dependencies
echo "Collecting library dependencies..."
MAIN_EXECUTABLE="${MACOS_DIR}/${APP_NAME}"

# Get direct dependencies
DIRECT_DEPS=$(otool -L "${MAIN_EXECUTABLE}" | grep -E "^\s+/" | awk '{print $1}')

for dep in $DIRECT_DEPS; do
    if [[ "$dep" != /System/* ]] && [[ "$dep" != /usr/lib/* ]] && [[ "$dep" != /usr/X11/* ]]; then
        copy_lib_and_deps "$dep" "${LIB_DIR}"
    fi
done

# Fix library paths in the main executable
echo "Fixing library paths in executable..."
for lib in "${LIB_DIR}"/*.dylib; do
    if [ -f "$lib" ]; then
        lib_name=$(basename "$lib")
        # Change the path in the executable
        install_name_tool -change "$(otool -L "${MAIN_EXECUTABLE}" | grep "$lib_name" | awk '{print $1}')" \
            "@executable_path/../lib/${lib_name}" "${MAIN_EXECUTABLE}" 2>/dev/null || true
    fi
done

# Fix library paths in all copied libraries
echo "Fixing library paths in bundled libraries..."
for lib in "${LIB_DIR}"/*.dylib; do
    if [ -f "$lib" ]; then
        lib_name=$(basename "$lib")
        
        # Fix the library's own id
        install_name_tool -id "@executable_path/../lib/${lib_name}" "$lib" 2>/dev/null || true
        
        # Fix paths to other libraries
        DEPS=$(otool -L "$lib" | grep -E "^\s+/" | awk '{print $1}')
        for dep in $DEPS; do
            dep_name=$(basename "$dep")
            if [ -f "${LIB_DIR}/${dep_name}" ]; then
                install_name_tool -change "$dep" "@executable_path/../lib/${dep_name}" "$lib" 2>/dev/null || true
            fi
        done
    fi
done

# Copy GTK modules and supporting files
echo "Copying GTK resources..."

# Find GTK prefix
GTK_PREFIX=$(pkg-config --variable=prefix gtk+-3.0)

# Copy GSettings schemas
if [ -d "${GTK_PREFIX}/share/glib-2.0/schemas" ]; then
    echo "  Copying GSettings schemas..."
    mkdir -p "${RESOURCES_DIR}/share/glib-2.0"
    cp -r "${GTK_PREFIX}/share/glib-2.0/schemas" "${RESOURCES_DIR}/share/glib-2.0/"
    # Compile schemas
    glib-compile-schemas "${RESOURCES_DIR}/share/glib-2.0/schemas" 2>/dev/null || true
fi

# Copy GTK icon theme
if [ -d "${GTK_PREFIX}/share/icons/hicolor" ]; then
    echo "  Copying icon theme..."
    mkdir -p "${RESOURCES_DIR}/share/icons"
    cp -r "${GTK_PREFIX}/share/icons/hicolor" "${RESOURCES_DIR}/share/icons/"
    # Update icon cache
    gtk-update-icon-cache "${RESOURCES_DIR}/share/icons/hicolor" 2>/dev/null || true
fi

# Copy locale files for GTK
if [ -d "${GTK_PREFIX}/share/locale" ]; then
    echo "  Copying locale files..."
    mkdir -p "${RESOURCES_DIR}/share/locale"
    # Only copy major locales to save space
    for lang in en de es fr it ja pt ru zh_CN; do
        if [ -d "${GTK_PREFIX}/share/locale/${lang}" ]; then
            cp -r "${GTK_PREFIX}/share/locale/${lang}" "${RESOURCES_DIR}/share/locale/" 2>/dev/null || true
        fi
    done
fi

# Copy GDK pixbuf loaders
GDK_PIXBUF_PREFIX=$(pkg-config --variable=prefix gdk-pixbuf-2.0)
if [ -d "${GDK_PIXBUF_PREFIX}/lib/gdk-pixbuf-2.0" ]; then
    echo "  Copying GDK pixbuf loaders..."
    mkdir -p "${LIB_DIR}/gdk-pixbuf-2.0"
    cp -r "${GDK_PIXBUF_PREFIX}/lib/gdk-pixbuf-2.0" "${LIB_DIR}/"
    
    # Update pixbuf loaders cache
    GDK_PIXBUF_MODULEDIR="${LIB_DIR}/gdk-pixbuf-2.0/2.10.0/loaders"
    if [ -d "$GDK_PIXBUF_MODULEDIR" ]; then
        # Fix library paths in pixbuf loaders
        for loader in "${GDK_PIXBUF_MODULEDIR}"/*.so; do
            if [ -f "$loader" ]; then
                LOADER_DEPS=$(otool -L "$loader" | grep -E "^\s+/" | awk '{print $1}')
                for dep in $LOADER_DEPS; do
                    dep_name=$(basename "$dep")
                    if [ -f "${LIB_DIR}/${dep_name}" ]; then
                        install_name_tool -change "$dep" "@executable_path/../lib/${dep_name}" "$loader" 2>/dev/null || true
                    fi
                done
            fi
        done
        
        # Create loaders cache
        # Use the system gdk-pixbuf-query-loaders if available, or skip if it fails
        if command -v gdk-pixbuf-query-loaders >/dev/null 2>&1; then
            echo "Generating GDK pixbuf loaders cache..."
            gdk-pixbuf-query-loaders "${GDK_PIXBUF_MODULEDIR}"/*.so > "${LIB_DIR}/gdk-pixbuf-2.0/2.10.0/loaders.cache" 2>/dev/null || {
                echo "Warning: Failed to generate loaders cache (this is expected if unsigned)"
                # Create a minimal cache file
                echo "# Generated by create-bundle.sh" > "${LIB_DIR}/gdk-pixbuf-2.0/2.10.0/loaders.cache"
            }
            # Fix paths in cache to be relative
            sed -i '' "s|${GDK_PIXBUF_MODULEDIR}|@executable_path/../lib/gdk-pixbuf-2.0/2.10.0/loaders|g" \
                "${LIB_DIR}/gdk-pixbuf-2.0/2.10.0/loaders.cache" 2>/dev/null || true
        else
            echo "Warning: gdk-pixbuf-query-loaders not found, creating empty cache"
            echo "# Empty cache - will use system defaults" > "${LIB_DIR}/gdk-pixbuf-2.0/2.10.0/loaders.cache"
        fi
    fi
fi

# Create launcher script that sets up environment
echo "Creating launcher script..."
cat > "${MACOS_DIR}/launcher.sh" << 'EOF'
#!/bin/bash
BUNDLE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
RESOURCES_DIR="${BUNDLE_DIR}/Resources"
LIB_DIR="${BUNDLE_DIR}/lib"

export DYLD_LIBRARY_PATH="${LIB_DIR}:${DYLD_LIBRARY_PATH}"
export GDK_PIXBUF_MODULE_FILE="${LIB_DIR}/gdk-pixbuf-2.0/2.10.0/loaders.cache"
export GDK_PIXBUF_MODULEDIR="${LIB_DIR}/gdk-pixbuf-2.0/2.10.0/loaders"
export XDG_DATA_DIRS="${RESOURCES_DIR}/share:${XDG_DATA_DIRS}"
export GTK_DATA_PREFIX="${RESOURCES_DIR}"
export GTK_EXE_PREFIX="${BUNDLE_DIR}"
export GTK_PATH="${BUNDLE_DIR}"

# Disable GTK overlay scrollbars on macOS (optional)
export GTK_OVERLAY_SCROLLING=0

exec "${BUNDLE_DIR}/MacOS/PChat.bin" "$@"
EOF

chmod +x "${MACOS_DIR}/launcher.sh"

# Rename the main executable and use launcher
mv "${MACOS_DIR}/${APP_NAME}" "${MACOS_DIR}/${APP_NAME}.bin"
mv "${MACOS_DIR}/launcher.sh" "${MACOS_DIR}/${APP_NAME}"

# Sign the bundle
echo "Signing bundle with identity: ${SIGN_IDENTITY}"

# Sign all libraries first
echo "  Signing libraries..."
for lib in "${LIB_DIR}"/*.dylib; do
    if [ -f "$lib" ]; then
        # Sign with timestamp and hardened runtime for distribution builds
        if [ "$SIGN_IDENTITY" != "-" ]; then
            codesign --force --sign "${SIGN_IDENTITY}" --timestamp --options runtime "$lib" 2>&1 | grep -v "replacing existing signature" || {
                echo "    Warning: Failed to sign $(basename "$lib") - continuing anyway"
            }
        else
            codesign --force --sign - "$lib" 2>&1 | grep -v "replacing existing signature" || true
        fi
    fi
done

# Sign pixbuf loaders
if [ -d "${LIB_DIR}/gdk-pixbuf-2.0" ]; then
    echo "  Signing GDK pixbuf loaders..."
    for loader in "${LIB_DIR}"/gdk-pixbuf-2.0/*/*.so; do
        if [ -f "$loader" ]; then
            if [ "$SIGN_IDENTITY" != "-" ]; then
                codesign --force --sign "${SIGN_IDENTITY}" --timestamp --options runtime "$loader" 2>&1 | grep -v "replacing existing signature" || {
                    echo "    Warning: Failed to sign $(basename "$loader") - continuing anyway"
                }
            else
                codesign --force --sign - "$loader" 2>&1 | grep -v "replacing existing signature" || true
            fi
        fi
    done
fi

# Sign the main executable
echo "  Signing main executable..."
if [ "$SIGN_IDENTITY" != "-" ]; then
    codesign --force --sign "${SIGN_IDENTITY}" --timestamp --options runtime "${MACOS_DIR}/${APP_NAME}.bin" || {
        echo "ERROR: Failed to sign main executable"
        echo "This may be due to keychain access issues. Try:"
        echo "  1. Ensure your keychain is unlocked: security unlock-keychain"
        echo "  2. Verify your certificate: security find-identity -v -p codesigning"
        echo "  3. Check certificate access: codesign --verify --deep --strict --verbose=2 \"${MACOS_DIR}/${APP_NAME}.bin\""
        exit 1
    }
else
    codesign --force --sign - "${MACOS_DIR}/${APP_NAME}.bin" || {
        echo "ERROR: Failed to sign main executable"
        exit 1
    }
fi

# Sign the whole bundle with entitlements if using a real identity
echo "  Signing application bundle..."
if [ "$SIGN_IDENTITY" != "-" ]; then
    # For distribution, you might want to add entitlements here
    codesign --force --deep --sign "${SIGN_IDENTITY}" \
        --options runtime \
        "${BUNDLE_DIR}" || {
        echo "ERROR: Failed to sign bundle"
        exit 1
    }
else
    # Ad-hoc signing for local testing
    codesign --force --deep --sign - "${BUNDLE_DIR}" || {
        echo "WARNING: Ad-hoc code signing failed"
    }
fi

echo "  ✓ Code signing complete"

echo ""
echo "Bundle created successfully: ${BUNDLE_DIR}"
echo ""
echo "To run: open ${BUNDLE_DIR}"
echo "To create DMG: hdiutil create -volname PChat -srcfolder ${BUNDLE_DIR} -ov -format UDZO PChat-${VERSION}.dmg"
echo ""

# Verify the bundle
echo "Verifying bundle..."
if [ -x "${MACOS_DIR}/${APP_NAME}" ]; then
    echo "✓ Executable is present and executable"
else
    echo "✗ Executable missing or not executable"
    exit 1
fi

if [ -f "${CONTENTS_DIR}/Info.plist" ]; then
    echo "✓ Info.plist is present"
else
    echo "✗ Info.plist missing"
    exit 1
fi

echo "✓ Bundle verification complete"
