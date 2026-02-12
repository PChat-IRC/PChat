/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Dark Mode Support — implementation.
 */

#include "DarkMode.h"

#include <wx/settings.h>
#include <wx/colour.h>
#include <wx/gdicmn.h>
#include <wx/window.h>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <glib.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/cfgfiles.h"
}

/* ---- State ---- */

static ThemeMode s_theme_mode = THEME_SYSTEM;
static bool s_initialized = false;

/* ---- OS dark-mode detection ---- */

#ifdef _WIN32
static bool detect_windows_dark_mode()
{
    /* Query the Windows 10/11 registry for dark mode preference.
       This is the most reliable method — wxSystemSettings::GetAppearance()
       may not pick up the user's setting in all build configurations
       (e.g. MSYS2/MinGW/Clang64 builds). */
    HKEY hKey = nullptr;
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = REG_DWORD;

    LSTATUS res = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey);

    if (res == ERROR_SUCCESS) {
        res = RegQueryValueExW(hKey, L"AppsUseLightTheme",
                               nullptr, &type, (LPBYTE)&value, &size);
        RegCloseKey(hKey);

        if (res == ERROR_SUCCESS && type == REG_DWORD) {
            /* AppsUseLightTheme: 0 = dark, 1 = light */
            return value == 0;
        }
    }

    /* Fallback: try wxWidgets detection */
#if wxCHECK_VERSION(3, 1, 3)
    return wxSystemSettings::GetAppearance().IsDark();
#else
    return false;
#endif
}
#endif /* _WIN32 */

static bool detect_os_dark_mode()
{
#ifdef _WIN32
    return detect_windows_dark_mode();
#elif wxCHECK_VERSION(3, 1, 3)
    return wxSystemSettings::GetAppearance().IsDark();
#else
    /* Heuristic: if the system window background luminance is low,
       assume dark mode. */
    wxColour bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    int lum = (bg.Red() * 299 + bg.Green() * 587 + bg.Blue() * 114) / 1000;
    return lum < 128;
#endif
}

/* ---- Public API ---- */

void wx_darkmode_init(void)
{
    if (s_initialized)
        return;
    s_initialized = true;
    s_theme_mode = THEME_SYSTEM;
}

ThemeMode wx_darkmode_get_mode(void)
{
    return s_theme_mode;
}

void wx_darkmode_set_mode(ThemeMode mode)
{
    s_theme_mode = mode;
}

bool wx_darkmode_is_dark(void)
{
    switch (s_theme_mode) {
    case THEME_LIGHT:  return false;
    case THEME_DARK:   return true;
    case THEME_SYSTEM:
    default:           return detect_os_dark_mode();
    }
}

/* ---- Persistence ---- */

void wx_darkmode_save(void)
{
    int fh = pchat_open_file("theme.conf",
                              O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
    if (fh == -1)
        return;

    char buf[64];
    snprintf(buf, sizeof(buf), "theme_mode = %d\n", (int)s_theme_mode);
#ifdef _WIN32
    _write(fh, buf, (unsigned)strlen(buf));
    _close(fh);
#else
    (void)write(fh, buf, strlen(buf));
    close(fh);
#endif
}

void wx_darkmode_load(void)
{
    wx_darkmode_init();

    int fh = pchat_open_file("theme.conf", O_RDONLY, 0, 0);
    if (fh == -1)
        return;

    char buf[128];
    memset(buf, 0, sizeof(buf));
#ifdef _WIN32
    int n = _read(fh, buf, sizeof(buf) - 1);
    _close(fh);
#else
    int n = read(fh, buf, sizeof(buf) - 1);
    close(fh);
#endif
    if (n <= 0)
        return;
    buf[n] = '\0';

    int val = -1;
    if (sscanf(buf, "theme_mode = %d", &val) == 1) {
        if (val >= 0 && val <= 2)
            s_theme_mode = (ThemeMode)val;
    }
}

/* ================================================================
 * UI Chrome Colors — light and dark variants
 * ================================================================ */

/* ---- Tree default text ---- */
wxColour wx_darkmode_tree_default_fg(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(210, 210, 210)   /* light grey on dark bg */
        : wxColour(0, 0, 0);       /* black on light bg */
}

/* ---- Tab indicator colors ---- */
wxColour wx_darkmode_tab_new_data(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(100, 149, 237)   /* cornflower blue */
        : wxColour(0, 0, 200);
}

wxColour wx_darkmode_tab_new_msg(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(255, 100, 100)   /* soft red */
        : wxColour(200, 0, 0);
}

wxColour wx_darkmode_tab_hilight(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(100, 220, 100)   /* soft green */
        : wxColour(0, 160, 0);
}

/* ---- Userlist nick colors ---- */
wxColour wx_darkmode_nick_op(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(255, 110, 110)   /* bright red */
        : wxColour(200, 0, 0);
}

wxColour wx_darkmode_nick_halfop(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(200, 130, 220)   /* light purple */
        : wxColour(128, 0, 128);
}

wxColour wx_darkmode_nick_voice(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(100, 200, 100)   /* bright green */
        : wxColour(0, 128, 0);
}

wxColour wx_darkmode_nick_normal(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(210, 210, 210)
        : wxColour(0, 0, 0);
}

/* ---- Timestamp ---- */
wxColour wx_darkmode_timestamp(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(140, 140, 140)   /* grey, visible on dark */
        : wxColour(128, 128, 128);
}

/* ---- Spell check ---- */
wxColour wx_darkmode_spell_indicator(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(255, 80, 80)     /* softer red on dark */
        : wxColour(255, 0, 0);
}

/* ---- Raw log ---- */
wxColour wx_darkmode_rawlog_outbound(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(100, 149, 237)   /* cornflower blue */
        : wxColour(0, 0, 180);
}

wxColour wx_darkmode_rawlog_inbound(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(255, 120, 120)   /* soft red */
        : wxColour(180, 0, 0);
}

wxColour wx_darkmode_rawlog_bg(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(30, 30, 30)
        : wxColour(255, 255, 255);
}

/* ---- Friends list ---- */
wxColour wx_darkmode_friend_online(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(100, 220, 100)
        : wxColour(0, 128, 0);
}

/* ---- Keyboard shortcuts accel entry bg ---- */
wxColour wx_darkmode_accel_entry_bg(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(50, 50, 65)
        : wxColour(240, 240, 255);
}

/* ---- Generic panel background / foreground ---- */
wxColour wx_darkmode_panel_bg(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(40, 40, 40)
        : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
}

wxColour wx_darkmode_panel_fg(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(220, 220, 220)
        : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
}

/* Input / entry background — slightly lighter than panel for contrast. */
wxColour wx_darkmode_input_bg(void)
{
    return wx_darkmode_is_dark()
        ? wxColour(50, 50, 50)
        : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
}

/* ---- Recursive window theming helper ---- */

/*
 * wx_darkmode_apply_to_window  — recursively theme an entire dialog.
 *
 * The helper detects special widget types (wxListCtrl, wxListBox,
 * wxTextCtrl, wxGrid, wxStyledTextCtrl, wxRichTextCtrl, wxDataViewCtrl)
 * and gives them the "input" background so they stand out from the
 * surrounding panel chrome.  Everything else gets panel bg/fg.
 */

#include <wx/listctrl.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/grid.h>
#include <wx/stc/stc.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/dataview.h>
#include <wx/notebook.h>
#include <wx/tglbtn.h>
#include <wx/button.h>

void wx_darkmode_apply_to_window(wxWindow *win)
{
    if (!win)
        return;

    bool dark = wx_darkmode_is_dark();
    wxColour panelBg = wx_darkmode_panel_bg();
    wxColour panelFg = wx_darkmode_panel_fg();
    wxColour inputBg = wx_darkmode_input_bg();

    if (dark) {
        /* Determine if this is an "input-style" widget that should
           get the lighter input background */
        bool is_input = false;
        if (dynamic_cast<wxListCtrl *>(win)  ||
            dynamic_cast<wxListBox *>(win)   ||
            dynamic_cast<wxTextCtrl *>(win)  ||
            dynamic_cast<wxDataViewCtrl *>(win)) {
            is_input = true;
        }

        /* wxGrid needs cell-level styling */
        wxGrid *grid = dynamic_cast<wxGrid *>(win);
        if (grid) {
            grid->SetDefaultCellBackgroundColour(inputBg);
            grid->SetDefaultCellTextColour(panelFg);
            grid->SetLabelBackgroundColour(panelBg);
            grid->SetLabelTextColour(panelFg);
            grid->SetGridLineColour(wxColour(70, 70, 70));
            grid->SetBackgroundColour(panelBg);
            grid->SetForegroundColour(panelFg);
        } else if (auto *stc = dynamic_cast<wxStyledTextCtrl *>(win)) {
            stc->StyleSetBackground(wxSTC_STYLE_DEFAULT, inputBg);
            stc->StyleSetForeground(wxSTC_STYLE_DEFAULT, panelFg);
            stc->StyleClearAll();
            stc->SetCaretForeground(panelFg);
        } else if (auto *rtc = dynamic_cast<wxRichTextCtrl *>(win)) {
            rtc->SetBackgroundColour(inputBg);
            rtc->SetForegroundColour(panelFg);
        } else {
            win->SetBackgroundColour(is_input ? inputBg : panelBg);
            win->SetForegroundColour(panelFg);
        }
    } else {
        /* Light mode: reset to system defaults */
        wxGrid *grid = dynamic_cast<wxGrid *>(win);
        if (grid) {
            grid->SetDefaultCellBackgroundColour(
                wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
            grid->SetDefaultCellTextColour(
                wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            grid->SetLabelBackgroundColour(wxNullColour);
            grid->SetLabelTextColour(wxNullColour);
            grid->SetGridLineColour(wxNullColour);
        } else if (auto *stc = dynamic_cast<wxStyledTextCtrl *>(win)) {
            wxColour sysBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
            wxColour sysFg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
            stc->StyleSetBackground(wxSTC_STYLE_DEFAULT, sysBg);
            stc->StyleSetForeground(wxSTC_STYLE_DEFAULT, sysFg);
            stc->StyleClearAll();
            stc->SetCaretForeground(sysFg);
        }
        win->SetBackgroundColour(wxNullColour);
        win->SetForegroundColour(wxNullColour);
    }

    /* Recurse into children */
    for (wxWindowList::iterator it = win->GetChildren().begin();
         it != win->GetChildren().end(); ++it) {
        wx_darkmode_apply_to_window(*it);
    }

    win->Refresh();
}

/* ---- wxWidgets 3.3 built-in dark mode support ---- */

#include <wx/app.h>

void wx_darkmode_enable_wx(wxApp *app)
{
    if (!app)
        return;

    bool dark = wx_darkmode_is_dark();

#ifdef __WXMSW__
    /* wxWidgets 3.3 MSWEnableDarkMode() handles ALL native Windows dark mode:
       - Title bars (DWM immersive dark mode)
       - Menu bars and context menus (undocumented uxtheme APIs)
       - Native controls (scrollbars, checkboxes, radio buttons, etc.)
       - Status bars, toolbars, list controls, tree controls
       It calls SetPreferredAppMode, AllowDarkModeForWindow,
       FlushMenuThemes, DwmSetWindowAttribute, SetWindowTheme
       internally — all the things we were doing manually. */
    switch (s_theme_mode) {
    case THEME_DARK:
        app->MSWEnableDarkMode(wxApp::DarkMode_Always);
        break;
    case THEME_LIGHT:
        /* Don't enable dark mode at all for explicit light mode */
        break;
    case THEME_SYSTEM:
    default:
        app->MSWEnableDarkMode(wxApp::DarkMode_Auto);
        break;
    }
#else
    /* On GTK/macOS, use the cross-platform SetAppearance API (3.3+)
       or fall back to GTK environment hints for older versions. */
#if wxCHECK_VERSION(3, 3, 0)
    if (dark)
        app->SetAppearance(wxApp::Appearance::Dark);
    else
        app->SetAppearance(wxApp::Appearance::System);
#else
    /* wxWidgets 3.2 does not have SetAppearance.
       On GTK we hint via the GTK_THEME environment variable.
       On macOS there is no simple equivalent in 3.2,
       so we leave it to the OS. */
#ifdef __WXGTK__
    if (dark)
        g_setenv("GTK_THEME", "Adwaita:dark", TRUE);
    else
        g_unsetenv("GTK_THEME");
#endif /* __WXGTK__ */
    (void)app; /* suppress unused-parameter warning when no action taken */
#endif /* wxCHECK_VERSION(3, 3, 0) */
#endif
}