/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Dark Mode Support — cross-platform theme management.
 *
 * Provides three modes: Light, Dark, System (auto-detect).
 * Centralizes all UI colors so that hardcoded values are replaced
 * by theme-aware accessors.  Colors are split into two groups:
 *
 *   1. IRC palette colors (0-41) — managed by palette.cpp; this module
 *      supplies dark-mode default presets that palette.cpp can apply.
 *
 *   2. UI chrome colors — tree text, userlist nick colors, tab indicator
 *      colors, timestamp color, raw-log colors, etc.  These are not
 *      part of the IRC palette and are managed entirely here.
 */

#ifndef PCHAT_WX_DARKMODE_H
#define PCHAT_WX_DARKMODE_H

#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/window.h>

class wxApp;

/* Theme mode persisted in config */
enum ThemeMode {
    THEME_LIGHT  = 0,
    THEME_DARK   = 1,
    THEME_SYSTEM = 2,   /* follow OS appearance */
};

/* ---- Query / control ---- */

/* Initialize dark mode subsystem.  Call once, before palette_init. */
void wx_darkmode_init(void);

/* Get / set the user-chosen theme mode (Light / Dark / System). */
ThemeMode wx_darkmode_get_mode(void);
void      wx_darkmode_set_mode(ThemeMode mode);

/* Resolve the effective appearance: true = dark, false = light.
   When mode == THEME_SYSTEM this queries the OS. */
bool wx_darkmode_is_dark(void);

/* Persist the current mode to pchat config dir ("theme.conf"). */
void wx_darkmode_save(void);

/* Load the persisted mode from "theme.conf". */
void wx_darkmode_load(void);

/* ---- UI chrome colors ---- */

/* Default tree-item text for the current session (no activity). */
wxColour wx_darkmode_tree_default_fg(void);

/* Tab activity indicator colors. */
wxColour wx_darkmode_tab_new_data(void);   /* new data - blue      */
wxColour wx_darkmode_tab_new_msg(void);    /* new message - red    */
wxColour wx_darkmode_tab_hilight(void);    /* highlight - green    */

/* Userlist nick status colors. */
wxColour wx_darkmode_nick_op(void);        /* operator   - red     */
wxColour wx_darkmode_nick_halfop(void);    /* half-op    - purple  */
wxColour wx_darkmode_nick_voice(void);     /* voice      - green   */
wxColour wx_darkmode_nick_normal(void);    /* normal user          */

/* Timestamp text color. */
wxColour wx_darkmode_timestamp(void);

/* Spell-check indicator (squiggly underline). */
wxColour wx_darkmode_spell_indicator(void);

/* Raw-log text colors. */
wxColour wx_darkmode_rawlog_outbound(void);
wxColour wx_darkmode_rawlog_inbound(void);
wxColour wx_darkmode_rawlog_bg(void);

/* Friends-list online nick color. */
wxColour wx_darkmode_friend_online(void);

/* Keyboard-shortcuts accelerator entry background. */
wxColour wx_darkmode_accel_entry_bg(void);

/* Generic panel/widget background (for panels that need manual override). */
wxColour wx_darkmode_panel_bg(void);
wxColour wx_darkmode_panel_fg(void);

/* Input/entry widgets (text boxes, list interiors). */
wxColour wx_darkmode_input_bg(void);

/* ---- Helpers ---- */

/* Recursively apply dark/light background and foreground to a window
   and all its children.  Useful after theme switch. */
void wx_darkmode_apply_to_window(wxWindow *win);

/* Enable wxWidgets 3.3 built-in dark mode support for native controls,
   menus, title bars, scrollbars, etc.  Call before creating windows.
   Also updates when theme changes (e.g. from Preferences). */
void wx_darkmode_enable_wx(wxApp *app);

#endif /* PCHAT_WX_DARKMODE_H */
