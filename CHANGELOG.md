# Changelog

All notable changes to PChat will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2025-12-24

### Major Changes
- **Complete GTK3 Migration**: Fully migrated from legacy GTK2 to modern GTK3 APIs
- **New Text Widget**: Replaced custom xtext widget with GTKTextView for better maintainability and future GTK4 compatibility
- **Modern Build System**: Added comprehensive CMake build system alongside existing Autotools
- **Code Modernization**: Merged latest HexChat common code and modernized codebase

### Added
- **Audio Player Plugin**: Full-featured audio player with FFmpeg/FAudio support
  - Supports FLAC, MP3, OGG, WAV, and other formats
  - Playlist management (M3U, M3U8, PLS formats)
  - GTK3 GUI with playback controls and volume slider
  - Commands: `/PLAY`, `/PAUSE`, `/STOP`, `/NEXT`, `/PREV`, `/PLAYLIST`, `/NP`
- **Notification Plugin**: Cross-platform desktop notification support
  - WinRT notifications on Windows with fallback to Shell_NotifyIcon
  - Native notification support for Linux/macOS
  - Debug logging infrastructure for core and plugins
- **CMake Build System**: Complete CMake support with platform detection
  - Windows NSIS installer generation
  - macOS bundle and DMG creation
  - Linux packaging support
  - Plugin build configuration
- **IRC Text Formatting Shortcuts**: Keyboard shortcuts for IRC formatting
  - Ctrl+B (Bold), Ctrl+I (Italic), Ctrl+U (Underline)
  - Ctrl+K (Color), Ctrl+O (Reset formatting)
- **macOS Native Integration**: gtk-mac-integration support for native menu bar
- **Libera.Chat**: Added Libera.Chat to default server list

### Changed
- **Configuration Location**: Changed from `~/.xchat2/` to `~/.config/pchat/` (XDG-compliant)
- **Configuration File**: Renamed from `xchat.conf` to `PChat.conf`
- **Classic Icon Restored**: Returned to original PChat icon/logo
- **Memory Management**: Standardized to use GLib memory functions (`g_malloc`, `g_free`, etc.)
- **String Operations**: Replaced unsafe functions with bounds-checked GLib equivalents
  - `strcpy()` → `g_strlcpy()`
  - `strcat()` → `g_strlcat()`
  - `sprintf()` → `g_snprintf()`
- **Userlist Sorting**: Fixed and improved IRC rank-based sorting with multiple sort modes
- **Smart Autoscroll**: Textview only autoscrolls when user is at bottom (within 10 pixels)
- **Font Settings**: Fixed font persistence and immediate application of changes

### Fixed
- **GTK3 Deprecations**: Eliminated all GTK3 deprecation warnings
  - Replaced `gtk_table` with `GtkGrid`
  - Updated `GdkColor` to `GdkRGBA`
  - Modernized dialog APIs
  - Fixed widget show/hide calls
  - Updated box packing functions
- **Color Palette**: Fixed color palette corruption and CSS provider lifecycle management
- **Build System**: Various build fixes for Windows, macOS, and Linux
  - OpenSSL detection from Homebrew/MacPorts
  - ISO_CODES_LOCALDIR configuration
  - MEMRCHR detection
  - SSL pkg-config integration
- **Memory Leaks**: Fixed multiple memory leaks from uncaptured `g_build_filename()` results
- **Userlist Display**: Fixed users not sorting by IRC rank (ops, halfops, voice)
- **Pane Sizing**: Fixed channel/query window pane sizing issues
- **URL Clickability**: Restored clickable URLs with hover cursor feedback
- **IRC Control Codes**: Fixed malformed IRC control code filtering
- **Background Colors**: Fixed background color support in text display
- **Dialog Constraints**: Fixed various dialog sizing and constraint issues
- **Menubar Warnings**: Eliminated GTK critical warnings from menubar operations
- **Deprecated Warnings**: Fixed warnings in checksum and fishlim plugins
- **DBUS Issues**: Fixed DBUS integration problems

### Removed
- **XText Widget**: Removed legacy xtext rendering code
- **Perl Plugin**: Removed Perl plugin support
- **Old Build Files**: Cleaned up Autotools-only build artifacts
- **GTK2 Support**: Removed all GTK2 compatibility code

### Security
- Buffer overflow prevention through bounds-checked string operations
- Proper error handling with GLib's abort-on-failure semantics
- Shell escape sequence handling for file paths

### Platform Support
- **Windows**: NSIS installer, WinRT notifications, proper plugin extensions
- **macOS**: Self-contained .app bundles, DMG distribution, native menu bar integration
- **Linux**: XDG-compliant configuration, modern GTK3 support

## [1.5.4] - 2015-2021

### Early Development (2015-2017)
- Initial fork from HexChat
- Experimental GTK3 port with Cairo renderer
- Tab completion improvements
- DND dragging with Cairo support
- Platform-specific support code
- New icons by Alexandros
- Travis CI / Tea-CI integration

### Maintenance Period (2021)
- MSYS2/OpenSSH build testing

### Historical Base
- X-Chat ("xchat") Copyright (c) 1998-2010 By Peter Zelezny
- HexChat ("hexchat") Copyright (c) 2009-2014 By Berke Viktor
- PChat ("pchat") Copyright (c) 2025 by Zach Bacon

---

## Links
- [Repository](https://github.com/PChat-IRC/PChat)
- [Issue Tracker](https://github.com/PChat-IRC/PChat/issues)
- [IRCHelp.org](http://irchelp.org)

## License
This program is released under the GPL v2 with the additional exemption
that compiling, linking, and/or using OpenSSL is allowed.
