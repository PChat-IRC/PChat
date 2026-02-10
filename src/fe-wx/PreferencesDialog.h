/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Preferences Dialog - replicates HexChat preferences
 * Categories: Interface, Chatting, Network
 */

#ifndef PCHAT_PREFERENCESDIALOG_H
#define PCHAT_PREFERENCESDIALOG_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/spinctrl.h>
#include <wx/slider.h>
#include <wx/listctrl.h>
#include <map>

class PreferencesDialog : public wxDialog
{
public:
    PreferencesDialog(wxWindow *parent);
    ~PreferencesDialog();

private:
    void CreateLayout();
    void LoadSettings();
    void SaveSettings();

    /* Create each category page */
    wxPanel *CreateAppearancePage(wxWindow *parent);
    wxPanel *CreateInputBoxPage(wxWindow *parent);
    wxPanel *CreateUserListPage(wxWindow *parent);
    wxPanel *CreateChannelSwitcherPage(wxWindow *parent);
    wxPanel *CreateColorsPage(wxWindow *parent);
    wxPanel *CreateChattingGeneralPage(wxWindow *parent);
    wxPanel *CreateAlertsPage(wxWindow *parent);
    wxPanel *CreateSoundsPage(wxWindow *parent);
    wxPanel *CreateLoggingPage(wxWindow *parent);
    wxPanel *CreateAdvancedPage(wxWindow *parent);
    wxPanel *CreateNetworkSetupPage(wxWindow *parent);
    wxPanel *CreateFileTransfersPage(wxWindow *parent);
    wxPanel *CreateIdentdPage(wxWindow *parent);

    void OnOK(wxCommandEvent &event);
    void OnCancel(wxCommandEvent &event);
    void OnCategoryChanged(wxTreeEvent &event);
    void OnFontBrowse(wxCommandEvent &event);
    void OnBgBrowse(wxCommandEvent &event);
    void OnSoundBrowse(wxCommandEvent &event);
    void OnSoundPlay(wxCommandEvent &event);

    /* Left tree for categories */
    wxTreeCtrl *m_category_tree;

    /* Right panel that changes based on selection */
    wxPanel *m_settings_panel;
    wxBoxSizer *m_settings_sizer;

    /* Map tree items to panels */
    std::map<wxTreeItemId, wxPanel *> m_pages;
    wxPanel *m_current_page = nullptr;

    /* ===== Appearance page controls ===== */
    wxChoice *m_language;
    wxTextCtrl *m_font_entry;
    wxCheckBox *m_colored_nicks;
    wxCheckBox *m_indent_nicks;
    wxCheckBox *m_show_marker;
    wxTextCtrl *m_bg_image;
    wxSlider *m_opacity_slider;
    wxCheckBox *m_timestamps;
    wxTextCtrl *m_timestamp_format;
    wxCheckBox *m_show_chan_modes;
    wxCheckBox *m_show_num_users;
    wxCheckBox *m_show_nickname;

    /* ===== Input Box page controls ===== */
    wxCheckBox *m_input_style;
    wxCheckBox *m_input_nick;
    wxCheckBox *m_input_spell;
    wxCheckBox *m_input_attr;
    wxCheckBox *m_input_icon;
    wxTextCtrl *m_spell_langs;
    wxTextCtrl *m_completion_suffix;
    wxChoice *m_completion_sort;
    wxSpinCtrl *m_completion_amount;
    wxCheckBox *m_input_perc_ascii;
    wxCheckBox *m_input_perc_color;

    /* ===== User List page controls ===== */
    wxCheckBox *m_ulist_hide;
    wxCheckBox *m_ulist_style;
    wxCheckBox *m_ulist_show_hosts;
    wxCheckBox *m_ulist_color;
    wxCheckBox *m_ulist_count;
    wxCheckBox *m_ulist_icons;
    wxCheckBox *m_ulist_buttons;
    wxChoice *m_ulist_sort;
    wxChoice *m_ulist_pos;
    wxCheckBox *m_away_track;
    wxSpinCtrl *m_away_size_max;
    wxTextCtrl *m_ulist_doubleclick;
    wxChoice *m_lagometer;
    wxChoice *m_throttlemeter;

    /* ===== Channel Switcher page controls ===== */
    wxChoice *m_tab_layout;
    wxChoice *m_tab_chans;
    wxChoice *m_tab_dialogs;
    wxChoice *m_tab_utils;
    wxCheckBox *m_tab_sort;
    wxCheckBox *m_tab_icons;
    wxCheckBox *m_tab_server;
    wxCheckBox *m_tab_autoopen_dialog;
    wxCheckBox *m_tab_dots;
    wxCheckBox *m_tab_scrollchans;
    wxCheckBox *m_tab_middleclose;
    wxCheckBox *m_tab_small;
    wxChoice *m_tab_newtofront;
    wxChoice *m_notice_pos;
    wxChoice *m_tab_pos;
    wxSpinCtrl *m_tab_trunc;

    /* ===== Colors page controls ===== */
    wxButton *m_color_buttons[42];  /* palette color buttons 0-41 */
    bool m_colors_changed;          /* track if any color was edited */
    wxCheckBox *m_strip_msg;
    wxCheckBox *m_strip_replay;
    wxCheckBox *m_strip_topic;

    /* ===== Chatting > General page controls ===== */
    wxTextCtrl *m_quit_reason;
    wxTextCtrl *m_part_reason;
    wxTextCtrl *m_away_reason;
    wxCheckBox *m_away_show_once;
    wxCheckBox *m_away_auto_unmark;
    wxCheckBox *m_irc_raw_modes;
    wxCheckBox *m_notify_whois;
    wxCheckBox *m_irc_conf_mode;
    wxCheckBox *m_irc_hide_nickchange;

    /* ===== Alerts page controls ===== */
    wxCheckBox *m_balloon_chans;
    wxCheckBox *m_balloon_priv;
    wxCheckBox *m_balloon_hilight;
    wxCheckBox *m_tray_chans;
    wxCheckBox *m_tray_priv;
    wxCheckBox *m_tray_hilight;
    wxCheckBox *m_flash_chans;
    wxCheckBox *m_flash_priv;
    wxCheckBox *m_flash_hilight;
    wxCheckBox *m_beep_chans;
    wxCheckBox *m_beep_priv;
    wxCheckBox *m_beep_hilight;
    wxCheckBox *m_away_omit_alerts;
    wxCheckBox *m_focus_omitalerts;
    wxCheckBox *m_gui_tray;
    wxCheckBox *m_tray_minimize;
    wxCheckBox *m_tray_close;
    wxCheckBox *m_tray_away;
    wxCheckBox *m_tray_quiet;
    wxTextCtrl *m_extra_hilight;
    wxTextCtrl *m_no_hilight;
    wxTextCtrl *m_nick_hilight;

    /* ===== Sounds page controls ===== */
    wxListCtrl *m_sound_list;
    wxTextCtrl *m_sound_file;

    /* ===== Logging page controls ===== */
    wxCheckBox *m_logging;
    wxCheckBox *m_log_timestamps;
    wxCheckBox *m_text_replay;
    wxSpinCtrl *m_text_max_lines;
    wxTextCtrl *m_logmask;
    wxTextCtrl *m_log_timestamp_fmt;
    wxCheckBox *m_url_logging;
    wxCheckBox *m_url_grabber;
    wxSpinCtrl *m_url_grabber_limit;

    /* ===== Advanced page controls ===== */
    wxCheckBox *m_autocopy_text;
    wxCheckBox *m_autocopy_stamp;
    wxCheckBox *m_autocopy_color;
    wxTextCtrl *m_real_name;
    wxTextCtrl *m_font_alternative;
    wxCheckBox *m_gui_compact;
    wxCheckBox *m_cap_server_time;
    wxCheckBox *m_auto_reconnect;
    wxSpinCtrl *m_reconnect_delay;
    wxSpinCtrl *m_join_delay;
    wxChoice *m_ban_type;

    /* ===== Network Setup page controls ===== */
    wxTextCtrl *m_bind_host;
    wxCheckBox *m_dcc_ip_from_server;
    wxTextCtrl *m_dcc_ip;
    wxSpinCtrl *m_dcc_port_first;
    wxSpinCtrl *m_dcc_port_last;
    wxChoice *m_proxy_type;
    wxTextCtrl *m_proxy_host;
    wxSpinCtrl *m_proxy_port;
    wxChoice *m_proxy_use;
    wxCheckBox *m_proxy_auth;
    wxTextCtrl *m_proxy_user;
    wxTextCtrl *m_proxy_pass;

    /* ===== File Transfers page controls ===== */
    wxChoice *m_dcc_auto_recv;
    wxTextCtrl *m_dcc_dir;
    wxTextCtrl *m_dcc_completed_dir;
    wxCheckBox *m_dcc_save_nick;
    wxCheckBox *m_autoopen_send;
    wxCheckBox *m_autoopen_recv;
    wxCheckBox *m_autoopen_chat;
    wxSpinCtrl *m_dcc_max_send;
    wxSpinCtrl *m_dcc_max_get;
    wxSpinCtrl *m_dcc_global_max_send;
    wxSpinCtrl *m_dcc_global_max_get;
    wxSpinCtrl *m_dcc_blocksize;

    /* ===== Identd page controls ===== */
    wxCheckBox *m_identd_server;
    wxSpinCtrl *m_identd_port;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_PREF_FONT_BROWSE = wxID_HIGHEST + 200,
    ID_PREF_BG_BROWSE,
    ID_PREF_CATEGORY_TREE,
    ID_PREF_SOUND_BROWSE,
    ID_PREF_SOUND_PLAY,
};

#endif /* PCHAT_PREFERENCESDIALOG_H */
