/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Main IRC Window - replicates HexChat GTK2 layout:
 *   - Server/channel tree (left panel)
 *   - Topic bar (top)
 *   - Chat text area (center)
 *   - User list (right panel)
 *   - Input box with nick label (bottom)
 *   - Menu bar, lag meter, mode buttons, etc.
 */

#ifndef PCHAT_MAINWINDOW_H
#define PCHAT_MAINWINDOW_H

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/notebook.h>
#include <wx/gauge.h>
#include <wx/statusbr.h>
#include <wx/taskbar.h>
#include <wx/aui/aui.h>
#include <wx/stc/stc.h>
#include <wx/fdrepdlg.h>
#include <wx/tglbtn.h>
#include <wx/tokenzr.h>
#include <map>
#include <vector>
#include <string>
#include <functional>

#include <glib.h>

#include "SpellCheck.h"

extern "C" {
#include "../common/pchat.h"
#include "../common/userlist.h"
#include "../common/fe.h"
}

/* Number of channel mode flag widgets (c n t i m l k b) — matches GTK3 fe-gtk.h */
#ifndef NUM_FLAG_WIDS
#define NUM_FLAG_WIDS 8
#endif

class MainWindow;
class NetworkListDialog;
class ChannelListDialog;
class BanListDialog;
class DccChatDialog;
class DccTransferDialog;
class FriendsListDialog;
class IgnoreListDialog;
class UrlGrabberDialog;
class JoinDialog;

/* A single line of IRC text with its timestamp (raw, pre-formatting) */
struct ChatLine {
    wxString text;
    time_t stamp;
};

/* Holds per-session UI state */
struct SessionTab {
    struct session *sess;
    wxTreeItemId tree_id;
    wxString channel_name;
    wxString network_name;
    int tab_color;  /* 0=none, 1=data, 2=msg, 3=hilight */

    /* Per-session text buffer — stores raw IRC text lines so they
       survive tab switches.  Replayed into wxRichTextCtrl on switch. */
    std::vector<ChatLine> text_lines;
    static const size_t MAX_LINES = 5000;  /* scrollback limit */
};

/* ===== System Tray Icon ===== */
class PchatTrayIcon : public wxTaskBarIcon
{
public:
    PchatTrayIcon(MainWindow *owner);
    void SetTrayIcon(int icon_type);
    void SetTrayIconFromFile(const wxString &filename);
    void SetTrayTooltip(const wxString &tip);
    void FlashTray(int timeout_ms);

protected:
    wxMenu *CreatePopupMenu() override;

private:
    void OnLeftDblClick(wxTaskBarIconEvent &event);
    MainWindow *m_owner;
    int m_current_icon;
};

class MainWindow : public wxFrame
{
public:
    MainWindow();
    ~MainWindow();

    /* Session management - called from fe_* functions */
    void AddSession(struct session *sess, int focus);
    void RemoveSession(struct session *sess);

    /* Text display */
    void PrintText(struct session *sess, const wxString &text, time_t stamp);
    void ClearText(struct session *sess, int lines);
    void ApplyPaletteColors(); /* re-apply palette to chat + re-render */

    /* Topic */
    void SetTopic(struct session *sess, const wxString &topic);

    /* Tab/channel control */
    void SetChannel(struct session *sess);
    void SetTabColor(struct session *sess, int color);
    void ClearChannel(struct session *sess);
    void UpdateTitle(struct session *sess);

    /* Userlist */
    void UserlistInsert(struct session *sess, struct User *user,
                        int row, bool sel);
    void UserlistRemove(struct session *sess, struct User *user);
    void UserlistRehash(struct session *sess, struct User *user);
    void UserlistNumbers(struct session *sess);
    void UserlistClear(struct session *sess);
    void UserlistSetSelected(struct session *sess);
    void UserlistSelect(struct session *sess, const char *word[],
                        int do_clear, int scroll_to);

    /* Nick display */
    void SetNick(struct server *serv, const wxString &nick);

    /* Progress bar */
    void SetProgressBar(struct session *sess, bool active);

    /* Lag meter */
    void SetLag(struct server *serv, long lag);
    void SetThrottle(struct server *serv);

    /* Away status */
    void SetAway(struct server *serv);

    /* Input box */
    char *GetInputBoxContents(struct session *sess);
    int GetInputBoxCursor(struct session *sess);
    void SetInputBoxContents(struct session *sess, const wxString &text);
    void SetInputBoxCursor(struct session *sess, int delta, int pos);
    void InsertInputText(const wxString &text);

    /* Server events */
    void OnServerEvent(struct server *serv, int type, int arg);

    /* Mode buttons (t n s i p m l k) */
    void UpdateModeButtons(struct session *sess, char mode, char sign);
    void UpdateChannelKey(struct session *sess);
    void UpdateChannelLimit(struct session *sess);

    /* Non-channel mode */
    void SetNonChannel(struct session *sess, int state);

    /* Lastlog search */
    void DoLastlog(struct session *sess, struct session *lastlog_sess,
                   const char *sstr, int flags);

    /* GUI info for scripting API */
    int GetGuiInfo(struct session *sess, int info_type);
    void *GetGuiInfoPtr(struct session *sess, int info_type);

    /* Dialogs */
    void ShowNetworkList();
    void ShowChannelList(struct server *serv, const char *filter, int do_refresh);

    /* Channel list */
    void AddChanListRow(struct server *serv, const char *chan,
                        const char *users, const char *topic);
    void ChanListEnd(struct server *serv);

    /* Ban list */
    bool AddBanListEntry(struct session *sess, const char *mask,
                         const char *who, const char *when, int rplcode);
    bool BanListEnd(struct session *sess, int rplcode);

    /* System tray */
    PchatTrayIcon *GetTrayIcon() { return m_tray_icon; }

    /* Accessor */
    struct session *GetCurrentSession() { return m_current_session; }

    /* Dialog accessors (for fe_* callbacks) */
    DccChatDialog *GetDccChatDialog() { return m_dcc_chat_dlg; }
    DccTransferDialog *GetDccTransferDialog() { return m_dcc_xfer_dlg; }
    FriendsListDialog *GetFriendsListDialog() { return m_friends_dlg; }
    IgnoreListDialog *GetIgnoreListDialog() { return m_ignore_dlg; }
    UrlGrabberDialog *GetUrlGrabberDialog() { return m_url_dlg; }

    /* Public menu handlers (called from fe_dcc_open_*_win) */
    void OnDccChat(wxCommandEvent &event);
    void OnDccRecv(wxCommandEvent &event);

    /* Button panel updates (called from fe_buttons_update / fe_dlgbuttons_update) */
    void UpdateUserlistButtons();
    void UpdateDialogButtons();

private:
    /* Create the UI layout matching HexChat */
    void CreateMenuBar();
    void CreateMainLayout();
    void CreateModeBar(wxPanel *parent, wxBoxSizer *sizer);
    void CreateStatusBar();

    /* Select a session as current */
    void SwitchToSession(struct session *sess);
    SessionTab *FindSessionTab(struct session *sess);

    /* Tab completion */
    void DoTabCompletion(bool shift_held);
    void TabCompClean();

    /* Menu event handlers - File */
    void OnServerList(wxCommandEvent &event);
    void OnNewServerTab(wxCommandEvent &event);
    void OnNewChannelTab(wxCommandEvent &event);
    void OnNewServerWindow(wxCommandEvent &event);
    void OnNewChannelWindow(wxCommandEvent &event);
    void OnLoadPlugin(wxCommandEvent &event);
    void OnCloseTab(wxCommandEvent &event);
    void OnQuit(wxCommandEvent &event);

    /* Menu event handlers - View */
    void OnToggleMenuBar(wxCommandEvent &event);
    void OnToggleTopicBar(wxCommandEvent &event);
    void OnToggleUserList(wxCommandEvent &event);
    void OnToggleULButtons(wxCommandEvent &event);
    void OnToggleModeButtons(wxCommandEvent &event);
    void OnLayoutTabs(wxCommandEvent &event);
    void OnLayoutTree(wxCommandEvent &event);
    void OnMetresOff(wxCommandEvent &event);
    void OnMetresGraph(wxCommandEvent &event);
    void OnMetresText(wxCommandEvent &event);
    void OnMetresBoth(wxCommandEvent &event);
    void OnFullscreen(wxCommandEvent &event);

    /* Menu event handlers - Server */
    void OnDisconnect(wxCommandEvent &event);
    void OnReconnect(wxCommandEvent &event);
    void OnJoinChannel(wxCommandEvent &event);
    void OnChannelList(wxCommandEvent &event);
    void OnAway(wxCommandEvent &event);

    /* Menu event handlers - Settings */
    void OnPreferences(wxCommandEvent &event);
    void OnAutoReplace(wxCommandEvent &event);
    void OnCtcpReplies(wxCommandEvent &event);
    void OnDialogButtons(wxCommandEvent &event);
    void OnKeyboardShortcuts(wxCommandEvent &event);
    void OnTextEvents(wxCommandEvent &event);
    void OnUrlHandlers(wxCommandEvent &event);
    void OnUserCommands(wxCommandEvent &event);
    void OnUserlistButtons(wxCommandEvent &event);
    void OnUserlistPopupConf(wxCommandEvent &event);

    /* Menu event handlers - Window */
    void OnBanList(wxCommandEvent &event);
    void OnCharChart(wxCommandEvent &event);
    void OnFriendsList(wxCommandEvent &event);
    void OnIgnoreList(wxCommandEvent &event);
    void OnPlugins(wxCommandEvent &event);
    void OnRawLog(wxCommandEvent &event);
    void OnUrlGrabber(wxCommandEvent &event);
    void OnResetMarker(wxCommandEvent &event);
    void OnCopySelection(wxCommandEvent &event);
    void OnClearText(wxCommandEvent &event);
    void OnSaveText(wxCommandEvent &event);
    void OnSearchText(wxCommandEvent &event);
    void OnSearchNext(wxCommandEvent &event);
    void OnSearchPrev(wxCommandEvent &event);

    /* Menu event handlers - Help */
    void OnHelpDocs(wxCommandEvent &event);
    void OnAbout(wxCommandEvent &event);

    /* Widget event handlers */
    void OnTreeSelChanged(wxTreeEvent &event);
    void OnInputEnter(wxCommandEvent &event);
    void OnInputKeyDown(wxKeyEvent &event);
    void OnClose(wxCloseEvent &event);
    void OnTopicEnter(wxCommandEvent &event);
    void OnUserlistRightClick(wxListEvent &event);
    void OnChatRightClick(wxMouseEvent &event);
    void OnModeButtonToggle(wxCommandEvent &event);
    void OnLimitKeyEntry(wxCommandEvent &event);
    void OnKeyKeyEntry(wxCommandEvent &event);
    void OnNickButtonClick(wxCommandEvent &event);

    /* Spell check */
    void OnInputContextMenu(wxContextMenuEvent &event);
    void OnInputModified(wxStyledTextEvent &event);
    void OnSpellSuggestion(wxCommandEvent &event);
    void OnSpellAddWord(wxCommandEvent &event);
    void SpellCheckInput();
    void OnSpellTimer(wxTimerEvent &event);
    wxTimer m_spell_timer;              /* debounce timer for spell check */
    wxString m_spell_word;          /* word being spell-checked in context menu */
    int m_spell_word_start = 0;     /* byte start position of misspelled word */
    int m_spell_word_end = 0;       /* byte end position of misspelled word */
    bool m_updating_input = false;  /* prevent recursion in input modification */

    /* Find dialog */
    void OnFindDialogEvent(wxFindDialogEvent &event);

    /* Append formatted IRC text to chat view */
    void AppendIrcText(wxRichTextCtrl *ctrl, const wxString &text,
                       time_t stamp = 0);

    /* Apply IRC color/format codes */
    wxColour GetIrcColor(int index);

    /* Build userlist right-click popup */
    wxMenu *BuildUserlistPopup();
    void OnUserlistPopupCmd(wxCommandEvent &event);

    /* Build chat area right-click popup */
    wxMenu *BuildChatPopup();

    /* Meter management */
    void CreateMeters();
    void UpdateMeters();
    void PositionMeterWidgets();

    /* Userlist buttons management */
    void CreateUserlistButtons();
    void ExecuteUserlistButtonCmd(const char *cmd);

    /* Tree/tab icon helpers */
    void LoadTreeIcons();
    int GetSessionIconIndex(struct session *sess);

    /* Userlist icon helpers */
    void LoadUserlistIcons();
    int GetUserIconIndex(struct User *user);

    /* Dialog buttons management */
    void CreateDialogButtons();

    /* Update menu check items to reflect current state */
    void SyncMenuCheckItems();

    /* Plugin menu items */
public:
    void AddPluginMenuItem(menu_entry *me);
    void RemovePluginMenuItem(menu_entry *me);
    void UpdatePluginMenuItem(menu_entry *me);

private:
    void OnPluginMenuCmd(wxCommandEvent &event);
    struct PluginMenuItem {
        menu_entry *me;
        int wxid;
    };
    std::vector<PluginMenuItem> m_plugin_menu_items;

    /* --- UI Widgets --- */

    /* Left pane: server/channel tree */
    wxTreeCtrl *m_channel_tree;
    wxTreeItemId m_tree_root;

    /* Bottom tab bar (shown in tabs mode, hidden in tree mode) */
    wxNotebook *m_tab_bar = nullptr;
    bool m_tab_switching = false;  /* guard against recursive tab/tree selection */
    std::map<struct session *, int> m_session_tab_map; /* sess → notebook page index */

    /* Center: chat display */
    wxRichTextCtrl *m_chat_text;

    /* Top: topic bar + mode buttons */
    wxTextCtrl *m_topic_entry;
    wxPanel *m_topic_panel;

    /* Mode buttons (c n t i m b k l) — inline with topic bar */
    wxToggleButton *m_mode_buttons[NUM_FLAG_WIDS]; /* c n t i m b k l */
    wxTextCtrl *m_key_entry;
    wxTextCtrl *m_limit_entry;
    wxPanel *m_mode_panel;  /* contains mode buttons, sits right of topic entry */

    /* Right pane: user list */
    wxListCtrl *m_user_list;
    wxImageList *m_userlist_imagelist = nullptr;
    wxImageList *m_tree_imagelist = nullptr;
    wxStaticText *m_usercount_label;
    wxPanel *m_userlist_panel;

    /* Userlist buttons panel (below user list, 2-column grid) */
    wxPanel *m_ul_buttons_panel = nullptr;
    wxSizer *m_ul_buttons_sizer = nullptr;

    /* Dialog buttons panel (below input box) */
    wxPanel *m_dlg_buttons_panel = nullptr;
    wxBoxSizer *m_dlg_buttons_sizer = nullptr;

    /* Bottom: input area */
    wxStyledTextCtrl *m_input_box;
    wxButton *m_nick_button = nullptr;  /* click to change nick */


    /* Main panel (holds entire layout) */
    wxPanel *m_main_panel = nullptr;
    wxBoxSizer *m_main_sizer = nullptr;

    /* Splitters */
    wxSplitterWindow *m_hsplitter_left;   /* tree | rest */
    wxSplitterWindow *m_hsplitter_right;  /* chat | userlist */

    /* Right panel: topic + mode + chat/userlist (right of tree) */
    wxPanel *m_right_panel = nullptr;
    wxBoxSizer *m_right_sizer = nullptr;

    /* Chat panel: left pane of m_hsplitter_right (topic+chat+input+dlgbuttons) */
    wxPanel *m_chat_panel = nullptr;
    wxBoxSizer *m_chat_sizer = nullptr;

    /* Input panel (nick dot + nick button + input box) */
    wxPanel *m_input_panel = nullptr;

    /* Status bar with embedded lag/throttle meters */
    wxGauge *m_lag_meter = nullptr;
    wxStaticText *m_lag_label = nullptr;
    wxGauge *m_throttle_meter = nullptr;
    wxStaticText *m_throttle_label = nullptr;

    /* Session tracking */
    std::vector<SessionTab> m_sessions;
    struct session *m_current_session = nullptr;
    std::map<struct session *, wxTreeItemId> m_session_tree_map;

    /* Dialogs */
    NetworkListDialog *m_network_list_dlg = nullptr;
    DccChatDialog *m_dcc_chat_dlg = nullptr;
    DccTransferDialog *m_dcc_xfer_dlg = nullptr;
    FriendsListDialog *m_friends_dlg = nullptr;
    IgnoreListDialog *m_ignore_dlg = nullptr;
    UrlGrabberDialog *m_url_dlg = nullptr;
    wxFindReplaceDialog *m_find_dlg = nullptr;
    wxFindReplaceData m_find_data;

    /* Tab completion state */
    std::string m_tab_comp_prefix;   /* original partial word */
    int m_tab_comp_prefix_len;
    bool m_tab_comp_active;

    /* System tray */
    PchatTrayIcon *m_tray_icon = nullptr;

    /* View-toggle state */
    bool m_userlist_visible = true;
    bool m_tree_visible = true;
    bool m_topic_visible = true;
    bool m_tab_bar_visible = false;

    /* Tab bar helpers */
    void SyncTabBarFromSessions();
    void OnTabBarPageChanged(wxBookCtrlEvent &event);
    int FindTabBarIndex(struct session *sess);
    void UpdateTabBarSelection(struct session *sess);
    void UpdateTabBarLabel(struct session *sess, const wxString &label);
    void UpdateTabBarColor(struct session *sess, int color);
    void RebuildTabBarIndices();

    /* Next ID for dynamically added plugin menu items */
    int m_next_plugin_menu_id = 29000;

    /* Input history buffer */
    static char s_input_buffer[2048];

    wxDECLARE_EVENT_TABLE();
};

/* Menu IDs - matching GTK3 menu structure */
enum {
    /* File menu */
    ID_SERVER_LIST = wxID_HIGHEST + 1,
    ID_NEW_SERVER_TAB,
    ID_NEW_CHANNEL_TAB,
    ID_NEW_SERVER_WINDOW,
    ID_NEW_CHANNEL_WINDOW,
    ID_LOAD_PLUGIN,
    ID_CLOSE_TAB,
    /* View menu */
    ID_TOGGLE_MENUBAR,
    ID_TOGGLE_TOPICBAR,
    ID_TOGGLE_USERLIST,
    ID_TOGGLE_ULBUTTONS,
    ID_TOGGLE_MODEBUTTONS,
    ID_LAYOUT_TABS,
    ID_LAYOUT_TREE,
    ID_METRES_OFF,
    ID_METRES_GRAPH,
    ID_METRES_TEXT,
    ID_METRES_BOTH,
    ID_FULLSCREEN,
    /* Server menu */
    ID_DISCONNECT,
    ID_RECONNECT,
    ID_JOIN_CHANNEL,
    ID_CHANNEL_LIST,
    ID_AWAY,
    /* Settings menu */
    ID_PREFERENCES,
    ID_AUTO_REPLACE,
    ID_CTCP_REPLIES,
    ID_DIALOG_BUTTONS,
    ID_KEYBOARD_SHORTCUTS,
    ID_TEXT_EVENTS,
    ID_URL_HANDLERS,
    ID_USER_COMMANDS,
    ID_USERLIST_BUTTONS,
    ID_USERLIST_POPUP_CONF,
    /* Window menu */
    ID_BAN_LIST,
    ID_CHAR_CHART,
    ID_DCC_CHAT,
    ID_DCC_RECV,
    ID_FRIENDS_LIST,
    ID_IGNORE_LIST,
    ID_PLUGINS,
    ID_RAW_LOG,
    ID_URL_GRABBER,
    ID_RESET_MARKER,
    ID_COPY_SELECTION,
    ID_CLEAR_TEXT,
    ID_SAVE_TEXT,
    ID_SEARCH_TEXT,
    ID_SEARCH_NEXT,
    ID_SEARCH_PREV,
    /* Help menu */
    ID_HELP_DOCS,
    /* Mode button IDs (matching NUM_FLAG_WIDS order) */
    ID_MODE_C,  /* 0: filter colors */
    ID_MODE_N,  /* 1: no outside messages */
    ID_MODE_T,  /* 2: topic protection */
    ID_MODE_I,  /* 3: invite only */
    ID_MODE_M,  /* 4: moderated */
    ID_MODE_L,  /* 5: user limit */
    ID_MODE_K,  /* 6: keyword */
    ID_MODE_B,  /* 7: ban list */
    /* Userlist popup command base */
    ID_USERLIST_POPUP_BASE = wxID_HIGHEST + 200,
    /* Spell check suggestion IDs */
    ID_SPELL_SUGGEST_BASE = wxID_HIGHEST + 300,
    ID_SPELL_SUGGEST_END = ID_SPELL_SUGGEST_BASE + 15,
    ID_SPELL_ADD_WORD,
};

#endif /* PCHAT_MAINWINDOW_H */
