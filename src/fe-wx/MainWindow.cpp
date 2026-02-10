/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Main IRC Window implementation
 * Replicates the HexChat GTK2 interface layout with full feature set
 */

#include "MainWindow.h"
#include "PchatApp.h"
#include "NetworkListDialog.h"
#include "PreferencesDialog.h"
#include "PluginDialog.h"
#include "RawLogDialog.h"
#include "UrlGrabberDialog.h"
#include "CharChartDialog.h"
#include "FriendsListDialog.h"
#include "IgnoreListDialog.h"
#include "DccDialog.h"
#include "EditListDialog.h"
#include "KeyboardShortcutsDialog.h"
#include "TextEventsDialog.h"
#include "BanListDialog.h"
#include "ChannelListDialog.h"
#include "JoinDialog.h"
#include "palette.h"
#include "fe-wx.h"
#include <wx/aboutdlg.h>
#include <wx/tglbtn.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/fe.h"
#include "../common/cfgfiles.h"
#include "../common/servlist.h"
#include "../common/outbound.h"
#include "../common/server.h"
#include "../common/text.h"
#include "../common/util.h"
#include "../common/userlist.h"
#include "../common/url.h"
#include "../common/tree.h"
#include "../common/plugin.h"
}

#include <wx/artprov.h>
#include <wx/imaglist.h>
#include <wx/stdpaths.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/clipbrd.h>
#include <wx/fdrepdlg.h>
#include <wx/notifmsg.h>
#include <wx/datetime.h>
#include <wx/filedlg.h>
#include <wx/file.h>
#include <algorithm>

/* Static data */
char MainWindow::s_input_buffer[2048] = {0};

/* Event table */
wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    /* File menu */
    EVT_MENU(ID_SERVER_LIST, MainWindow::OnServerList)
    EVT_MENU(ID_NEW_SERVER_TAB, MainWindow::OnNewServerTab)
    EVT_MENU(ID_NEW_CHANNEL_TAB, MainWindow::OnNewChannelTab)
    EVT_MENU(ID_NEW_SERVER_WINDOW, MainWindow::OnNewServerWindow)
    EVT_MENU(ID_NEW_CHANNEL_WINDOW, MainWindow::OnNewChannelWindow)
    EVT_MENU(ID_LOAD_PLUGIN, MainWindow::OnLoadPlugin)
    EVT_MENU(ID_CLOSE_TAB, MainWindow::OnCloseTab)
    EVT_MENU(wxID_EXIT, MainWindow::OnQuit)
    /* View menu */
    EVT_MENU(ID_TOGGLE_MENUBAR, MainWindow::OnToggleMenuBar)
    EVT_MENU(ID_TOGGLE_TOPICBAR, MainWindow::OnToggleTopicBar)
    EVT_MENU(ID_TOGGLE_USERLIST, MainWindow::OnToggleUserList)
    EVT_MENU(ID_TOGGLE_ULBUTTONS, MainWindow::OnToggleULButtons)
    EVT_MENU(ID_TOGGLE_MODEBUTTONS, MainWindow::OnToggleModeButtons)
    EVT_MENU(ID_LAYOUT_TABS, MainWindow::OnLayoutTabs)
    EVT_MENU(ID_LAYOUT_TREE, MainWindow::OnLayoutTree)
    EVT_MENU(ID_METRES_OFF, MainWindow::OnMetresOff)
    EVT_MENU(ID_METRES_GRAPH, MainWindow::OnMetresGraph)
    EVT_MENU(ID_METRES_TEXT, MainWindow::OnMetresText)
    EVT_MENU(ID_METRES_BOTH, MainWindow::OnMetresBoth)
    EVT_MENU(ID_FULLSCREEN, MainWindow::OnFullscreen)
    /* Server menu */
    EVT_MENU(ID_DISCONNECT, MainWindow::OnDisconnect)
    EVT_MENU(ID_RECONNECT, MainWindow::OnReconnect)
    EVT_MENU(ID_JOIN_CHANNEL, MainWindow::OnJoinChannel)
    EVT_MENU(ID_CHANNEL_LIST, MainWindow::OnChannelList)
    EVT_MENU(ID_AWAY, MainWindow::OnAway)
    /* Settings menu */
    EVT_MENU(ID_PREFERENCES, MainWindow::OnPreferences)
    EVT_MENU(ID_AUTO_REPLACE, MainWindow::OnAutoReplace)
    EVT_MENU(ID_CTCP_REPLIES, MainWindow::OnCtcpReplies)
    EVT_MENU(ID_DIALOG_BUTTONS, MainWindow::OnDialogButtons)
    EVT_MENU(ID_KEYBOARD_SHORTCUTS, MainWindow::OnKeyboardShortcuts)
    EVT_MENU(ID_TEXT_EVENTS, MainWindow::OnTextEvents)
    EVT_MENU(ID_URL_HANDLERS, MainWindow::OnUrlHandlers)
    EVT_MENU(ID_USER_COMMANDS, MainWindow::OnUserCommands)
    EVT_MENU(ID_USERLIST_BUTTONS, MainWindow::OnUserlistButtons)
    EVT_MENU(ID_USERLIST_POPUP_CONF, MainWindow::OnUserlistPopupConf)
    /* Window menu */
    EVT_MENU(ID_BAN_LIST, MainWindow::OnBanList)
    EVT_MENU(ID_CHAR_CHART, MainWindow::OnCharChart)
    EVT_MENU(ID_DCC_CHAT, MainWindow::OnDccChat)
    EVT_MENU(ID_DCC_RECV, MainWindow::OnDccRecv)
    EVT_MENU(ID_FRIENDS_LIST, MainWindow::OnFriendsList)
    EVT_MENU(ID_IGNORE_LIST, MainWindow::OnIgnoreList)
    EVT_MENU(ID_PLUGINS, MainWindow::OnPlugins)
    EVT_MENU(ID_RAW_LOG, MainWindow::OnRawLog)
    EVT_MENU(ID_URL_GRABBER, MainWindow::OnUrlGrabber)
    EVT_MENU(ID_RESET_MARKER, MainWindow::OnResetMarker)
    EVT_MENU(ID_COPY_SELECTION, MainWindow::OnCopySelection)
    EVT_MENU(ID_CLEAR_TEXT, MainWindow::OnClearText)
    EVT_MENU(ID_SAVE_TEXT, MainWindow::OnSaveText)
    EVT_MENU(ID_SEARCH_TEXT, MainWindow::OnSearchText)
    EVT_MENU(ID_SEARCH_NEXT, MainWindow::OnSearchNext)
    EVT_MENU(ID_SEARCH_PREV, MainWindow::OnSearchPrev)
    /* Help menu */
    EVT_MENU(ID_HELP_DOCS, MainWindow::OnHelpDocs)
    EVT_MENU(wxID_ABOUT, MainWindow::OnAbout)
    /* Widget events */
    EVT_TREE_SEL_CHANGED(wxID_ANY, MainWindow::OnTreeSelChanged)
    EVT_CLOSE(MainWindow::OnClose)
    EVT_FIND(wxID_ANY, MainWindow::OnFindDialogEvent)
    EVT_FIND_NEXT(wxID_ANY, MainWindow::OnFindDialogEvent)
    EVT_FIND_CLOSE(wxID_ANY, MainWindow::OnFindDialogEvent)
wxEND_EVENT_TABLE()

/* ===== System Tray Icon ===== */

PchatTrayIcon::PchatTrayIcon(MainWindow *owner)
    : m_owner(owner), m_current_icon(0)
{
    Bind(wxEVT_TASKBAR_LEFT_DCLICK, &PchatTrayIcon::OnLeftDblClick, this);
    SetTrayIcon(0);
}

void PchatTrayIcon::SetTrayIcon(int icon_type)
{
    m_current_icon = icon_type;
    wxIcon icon = wxArtProvider::GetIcon(wxART_INFORMATION, wxART_OTHER, wxSize(16, 16));
    wxString tooltip = wxT("PChat");
    SetIcon(icon, tooltip);
}

void PchatTrayIcon::SetTrayIconFromFile(const wxString &filename)
{
    if (filename.IsEmpty() || !wxFileExists(filename))
        return;
    wxIcon icon;
    icon.LoadFile(filename, wxBITMAP_TYPE_ANY);
    if (icon.IsOk()) {
        SetIcon(icon, wxT("PChat"));
    }
}

void PchatTrayIcon::SetTrayTooltip(const wxString &tip)
{
    wxIcon icon = wxArtProvider::GetIcon(wxART_INFORMATION, wxART_OTHER, wxSize(16, 16));
    SetIcon(icon, tip);
}

void PchatTrayIcon::FlashTray(int timeout_ms)
{
    /* Flash effect - just change tooltip temporarily */
    SetTrayTooltip(wxT("PChat - New Activity"));
}

wxMenu *PchatTrayIcon::CreatePopupMenu()
{
    wxMenu *menu = new wxMenu();
    menu->Append(wxID_ANY, wxT("&Show PChat"));
    menu->AppendSeparator();
    menu->Append(wxID_EXIT, wxT("&Quit"));
    return menu;
}

void PchatTrayIcon::OnLeftDblClick(wxTaskBarIconEvent &event)
{
    if (m_owner) {
        m_owner->Show();
        m_owner->Raise();
        if (m_owner->IsIconized())
            m_owner->Iconize(false);
    }
}

/* ===== MainWindow ===== */

MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, wxT("PChat"),
              wxDefaultPosition, wxSize(1000, 600)),
      m_tab_comp_active(false), m_tab_comp_prefix_len(0)
{
    /* Set minimum size */
    SetMinSize(wxSize(640, 480));

    /* Load saved window position if available */
    if (prefs.pchat_gui_win_width > 0 && prefs.pchat_gui_win_height > 0) {
        SetSize(prefs.pchat_gui_win_left, prefs.pchat_gui_win_top,
                prefs.pchat_gui_win_width, prefs.pchat_gui_win_height);
    }

    CreateMenuBar();
    CreateMainLayout();
    CreateStatusBar();

    /* Sync menu check items to match saved prefs */
    SyncMenuCheckItems();

    /* Center if no saved position */
    if (prefs.pchat_gui_win_width <= 0)
        Centre();

    /* Create system tray icon */
    m_tray_icon = new PchatTrayIcon(this);

    /* Initialize spell checker */
    if (SpellChecker::Instance().Init(wxT("en_US"))) {
        wxLogStatus(wxT("Spell check: en_US dictionary loaded"));
    } else {
        wxLogStatus(wxT("Spell check: no dictionary found"));
    }

    /* Spell check debounce timer — fires 200ms after last keystroke */
    m_spell_timer.Bind(wxEVT_TIMER, &MainWindow::OnSpellTimer, this);
}

MainWindow::~MainWindow()
{
    delete m_tray_icon;
    delete m_find_dlg;
}

/* ===== UI Creation ===== */

void MainWindow::CreateMenuBar()
{
    wxMenuBar *menuBar = new wxMenuBar();

    /* === File Menu (matches GTK3 _File) === */
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(ID_SERVER_LIST, wxT("Network Li&st...\tCtrl+S"),
                     wxT("Open network/server list"));
    fileMenu->AppendSeparator();

    /* New submenu */
    wxMenu *newMenu = new wxMenu();
    newMenu->Append(ID_NEW_SERVER_TAB, wxT("Server &Tab...\tCtrl+T"),
                    wxT("Open a new server tab"));
    newMenu->Append(ID_NEW_CHANNEL_TAB, wxT("&Channel Tab..."),
                    wxT("Open a new channel tab"));
    newMenu->Append(ID_NEW_SERVER_WINDOW, wxT("Server &Window...\tCtrl+N"),
                    wxT("Open a new server window"));
    newMenu->Append(ID_NEW_CHANNEL_WINDOW, wxT("C&hannel Window..."),
                    wxT("Open a new channel window"));
    fileMenu->AppendSubMenu(newMenu, wxT("&New"));

    fileMenu->AppendSeparator();
#ifdef USE_PLUGIN
    fileMenu->Append(ID_LOAD_PLUGIN, wxT("&Load Plugin or Script..."),
                     wxT("Load a plugin or script file"));
    fileMenu->AppendSeparator();
#endif
    fileMenu->Append(ID_CLOSE_TAB, wxT("&Close\tCtrl+W"),
                     wxT("Close the current tab"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, wxT("&Quit\tCtrl+Q"),
                     wxT("Exit PChat"));
    menuBar->Append(fileMenu, wxT("&File"));

    /* === View Menu (matches GTK3 _View) === */
    wxMenu *viewMenu = new wxMenu();
    viewMenu->AppendCheckItem(ID_TOGGLE_MENUBAR, wxT("&Menu Bar\tCtrl+F9"),
                              wxT("Toggle menu bar visibility"));
    viewMenu->Check(ID_TOGGLE_MENUBAR, !prefs.pchat_gui_hide_menu);
    viewMenu->AppendCheckItem(ID_TOGGLE_TOPICBAR, wxT("&Topic Bar"),
                              wxT("Toggle topic bar visibility"));
    viewMenu->Check(ID_TOGGLE_TOPICBAR, prefs.pchat_gui_topicbar);
    viewMenu->AppendCheckItem(ID_TOGGLE_USERLIST, wxT("&User List\tCtrl+F7"),
                              wxT("Toggle user list visibility"));
    viewMenu->Check(ID_TOGGLE_USERLIST, !prefs.pchat_gui_ulist_hide);
    viewMenu->AppendCheckItem(ID_TOGGLE_ULBUTTONS, wxT("U&serlist Buttons"),
                              wxT("Toggle userlist buttons visibility"));
    viewMenu->Check(ID_TOGGLE_ULBUTTONS, prefs.pchat_gui_ulist_buttons);
    viewMenu->AppendCheckItem(ID_TOGGLE_MODEBUTTONS, wxT("M&ode Buttons"),
                              wxT("Toggle channel mode buttons visibility"));
    viewMenu->Check(ID_TOGGLE_MODEBUTTONS, prefs.pchat_gui_mode_buttons);
    viewMenu->AppendSeparator();

    /* Channel Switcher submenu (radio: Tabs / Tree) */
    wxMenu *switcherMenu = new wxMenu();
    switcherMenu->AppendRadioItem(ID_LAYOUT_TABS, wxT("&Tabs"),
                                   wxT("Use tab-style channel switcher"));
    switcherMenu->AppendRadioItem(ID_LAYOUT_TREE, wxT("T&ree"),
                                   wxT("Use tree-style channel switcher"));
    if (prefs.pchat_gui_tab_layout == 2)
        switcherMenu->Check(ID_LAYOUT_TREE, true);
    else
        switcherMenu->Check(ID_LAYOUT_TABS, true);
    viewMenu->AppendSubMenu(switcherMenu, wxT("&Channel Switcher"));

    /* Network Meters submenu (radio: Off / Graph / Text / Both) */
    wxMenu *metresMenu = new wxMenu();
    metresMenu->AppendRadioItem(ID_METRES_OFF, wxT("&Off"),
                                 wxT("Disable network meters"));
    metresMenu->AppendRadioItem(ID_METRES_GRAPH, wxT("&Graph"),
                                 wxT("Show graph meters"));
    metresMenu->AppendRadioItem(ID_METRES_TEXT, wxT("&Text"),
                                 wxT("Show text meters"));
    metresMenu->AppendRadioItem(ID_METRES_BOTH, wxT("&Both"),
                                 wxT("Show both graph and text meters"));
    switch (prefs.pchat_gui_lagometer) {
    case 1: metresMenu->Check(ID_METRES_GRAPH, true); break;
    case 2: metresMenu->Check(ID_METRES_TEXT, true); break;
    case 3: metresMenu->Check(ID_METRES_BOTH, true); break;
    default: metresMenu->Check(ID_METRES_OFF, true); break;
    }
    viewMenu->AppendSubMenu(metresMenu, wxT("&Network Meters"));

    viewMenu->AppendSeparator();
    viewMenu->AppendCheckItem(ID_FULLSCREEN, wxT("Toggle &Fullscreen\tF11"),
                              wxT("Toggle fullscreen mode"));
    menuBar->Append(viewMenu, wxT("&View"));

    /* === Server Menu (matches GTK3 _Server) === */
    wxMenu *serverMenu = new wxMenu();
    serverMenu->Append(ID_DISCONNECT, wxT("&Disconnect"),
                       wxT("Disconnect from server"));
    serverMenu->Append(ID_RECONNECT, wxT("&Reconnect"),
                       wxT("Reconnect to server"));
    serverMenu->Append(ID_JOIN_CHANNEL, wxT("&Join a Channel..."),
                       wxT("Join an IRC channel"));
    serverMenu->Append(ID_CHANNEL_LIST, wxT("&List of Channels..."),
                       wxT("Browse channel list"));
    serverMenu->AppendSeparator();
    serverMenu->AppendCheckItem(ID_AWAY, wxT("Marked &Away\tAlt+A"),
                                wxT("Toggle away status"));
    menuBar->Append(serverMenu, wxT("&Server"));

    /* === Settings Menu (matches GTK3 S_ettings) === */
    wxMenu *settingsMenu = new wxMenu();
    settingsMenu->Append(ID_PREFERENCES, wxT("&Preferences"),
                         wxT("Edit preferences"));
    settingsMenu->AppendSeparator();
    settingsMenu->Append(ID_AUTO_REPLACE, wxT("Auto Replace..."),
                         wxT("Edit auto-replace list"));
    settingsMenu->Append(ID_CTCP_REPLIES, wxT("CTCP Replies..."),
                         wxT("Edit CTCP reply list"));
    settingsMenu->Append(ID_DIALOG_BUTTONS, wxT("Dialog Buttons..."),
                         wxT("Edit dialog buttons"));
    settingsMenu->Append(ID_KEYBOARD_SHORTCUTS, wxT("Keyboard Shortcuts..."),
                         wxT("Edit key bindings"));
    settingsMenu->Append(ID_TEXT_EVENTS, wxT("Text Events..."),
                         wxT("Edit text events"));
    settingsMenu->Append(ID_URL_HANDLERS, wxT("URL Handlers..."),
                         wxT("Edit URL handlers"));
    settingsMenu->Append(ID_USER_COMMANDS, wxT("User Commands..."),
                         wxT("Edit user commands"));
    settingsMenu->Append(ID_USERLIST_BUTTONS, wxT("Userlist Buttons..."),
                         wxT("Edit userlist buttons"));
    settingsMenu->Append(ID_USERLIST_POPUP_CONF, wxT("Userlist Popup..."),
                         wxT("Edit userlist popup menu"));
    menuBar->Append(settingsMenu, wxT("S&ettings"));

    /* === Window Menu (matches GTK3 _Window) === */
    wxMenu *windowMenu = new wxMenu();
    windowMenu->Append(ID_BAN_LIST, wxT("&Ban List..."),
                       wxT("Show channel ban list"));
    windowMenu->Append(ID_CHAR_CHART, wxT("Character Chart..."),
                       wxT("Show character chart"));
    windowMenu->Append(ID_DCC_CHAT, wxT("Direct Chat..."),
                       wxT("Show DCC chats"));
    windowMenu->Append(ID_DCC_RECV, wxT("File &Transfers..."),
                       wxT("Show file transfers"));
    windowMenu->Append(ID_FRIENDS_LIST, wxT("Friends List..."),
                       wxT("Show friends/notify list"));
    windowMenu->Append(ID_IGNORE_LIST, wxT("Ignore List..."),
                       wxT("Show ignore list"));
    windowMenu->Append(ID_PLUGINS, wxT("&Plugins and Scripts..."),
                       wxT("Manage plugins and scripts"));
    windowMenu->Append(ID_RAW_LOG, wxT("&Raw Log..."),
                       wxT("Show raw IRC traffic"));
    windowMenu->Append(ID_URL_GRABBER, wxT("URL Grabber..."),
                       wxT("Show captured URLs"));
    windowMenu->AppendSeparator();
    windowMenu->Append(ID_RESET_MARKER, wxT("Reset Marker Line\tCtrl+M"),
                       wxT("Reset the marker line position"));
    windowMenu->Append(ID_COPY_SELECTION, wxT("&Copy Selection\tCtrl+Shift+C"),
                       wxT("Copy selected text"));
    windowMenu->Append(ID_CLEAR_TEXT, wxT("C&lear Text"),
                       wxT("Clear the chat text"));
    windowMenu->Append(ID_SAVE_TEXT, wxT("Save Text..."),
                       wxT("Save chat text to file"));

    /* Search submenu */
    wxMenu *searchMenu = new wxMenu();
    searchMenu->Append(ID_SEARCH_TEXT, wxT("Search &Text...\tCtrl+F"),
                       wxT("Search in chat text"));
    searchMenu->Append(ID_SEARCH_NEXT, wxT("Search &Next\tCtrl+G"),
                       wxT("Find next match"));
    searchMenu->Append(ID_SEARCH_PREV, wxT("Search &Previous\tCtrl+Shift+G"),
                       wxT("Find previous match"));
    windowMenu->AppendSubMenu(searchMenu, wxT("Search"));

    menuBar->Append(windowMenu, wxT("&Window"));

    /* === Help Menu (matches GTK3 _Help) === */
    wxMenu *helpMenu = new wxMenu();
    helpMenu->Append(ID_HELP_DOCS, wxT("&Contents\tF1"),
                     wxT("Open PChat documentation"));
    helpMenu->AppendSeparator();
    helpMenu->Append(wxID_ABOUT, wxT("&About PChat"),
                     wxT("About PChat"));
    menuBar->Append(helpMenu, wxT("&Help"));

    SetMenuBar(menuBar);

    /* Hide menu bar if pref says so (accelerators still work) */
    if (prefs.pchat_gui_hide_menu) {
#ifdef __WXMSW__
        GetMenuBar()->Show(false);
#endif
    }
}

void MainWindow::CreateModeBar(wxPanel *parent, wxBoxSizer *parentSizer)
{
    /* Topic bar panel — contains topic entry + mode buttons inline */
    m_topic_panel = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *topicSizer = new wxBoxSizer(wxHORIZONTAL);

    /* Topic entry (takes remaining space) */
    m_topic_entry = new wxTextCtrl(m_topic_panel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_PROCESS_ENTER);
    m_topic_entry->SetToolTip(wxT("Channel topic - press Enter to change"));
    topicSizer->Add(m_topic_entry, 1, wxEXPAND | wxRIGHT, 2);

    /* Mode buttons panel (right of topic, same row) */
    m_mode_panel = new wxPanel(m_topic_panel, wxID_ANY);
    wxBoxSizer *modeSizer = new wxBoxSizer(wxHORIZONTAL);

    /* HexChat order: c n t i m b k [key_entry] l [limit_entry]
       Array indices match NUM_FLAG_WIDS: 0=c 1=n 2=t 3=i 4=m 5=l 6=k 7=b
       But visual order: c(0) n(1) t(2) i(3) m(4) b(7) k(6) [key] l(5) [limit] */
    struct ModeButtonDef {
        int index;       /* flag_wid index */
        const char *label;
        int id;
        const char *tooltip;
    };
    static const ModeButtonDef defs[] = {
        { 0, "c", ID_MODE_C, "Filter Colors (+c)" },
        { 1, "n", ID_MODE_N, "No Outside Messages (+n)" },
        { 2, "t", ID_MODE_T, "Topic Protection (+t)" },
        { 3, "i", ID_MODE_I, "Invite Only (+i)" },
        { 4, "m", ID_MODE_M, "Moderated (+m)" },
        { 7, "b", ID_MODE_B, "Ban List (+b)" },
    };

    for (int i = 0; i < 6; i++) {
        int idx = defs[i].index;
        m_mode_buttons[idx] = new wxToggleButton(
            m_mode_panel, defs[i].id,
            wxString::FromAscii(defs[i].label),
            wxDefaultPosition, wxSize(24, 22));
        m_mode_buttons[idx]->SetToolTip(wxString::FromAscii(defs[i].tooltip));
        m_mode_buttons[idx]->Bind(wxEVT_TOGGLEBUTTON,
                                   &MainWindow::OnModeButtonToggle, this);
        modeSizer->Add(m_mode_buttons[idx], 0, wxALIGN_CENTER_VERTICAL, 0);
    }

    /* k button + key entry */
    m_mode_buttons[6] = new wxToggleButton(
        m_mode_panel, ID_MODE_K, wxT("k"),
        wxDefaultPosition, wxSize(24, 22));
    m_mode_buttons[6]->SetToolTip(wxT("Channel Keyword (+k)"));
    m_mode_buttons[6]->Bind(wxEVT_TOGGLEBUTTON,
                             &MainWindow::OnModeButtonToggle, this);
    modeSizer->Add(m_mode_buttons[6], 0, wxALIGN_CENTER_VERTICAL, 0);

    m_key_entry = new wxTextCtrl(m_mode_panel, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxSize(100, -1),
                                  wxTE_PROCESS_ENTER);
    m_key_entry->SetMaxLength(23);
    m_key_entry->Bind(wxEVT_TEXT_ENTER, &MainWindow::OnKeyKeyEntry, this);
    modeSizer->Add(m_key_entry, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 1);

    /* l button + limit entry */
    m_mode_buttons[5] = new wxToggleButton(
        m_mode_panel, ID_MODE_L, wxT("l"),
        wxDefaultPosition, wxSize(24, 22));
    m_mode_buttons[5]->SetToolTip(wxT("User Limit (+l)"));
    m_mode_buttons[5]->Bind(wxEVT_TOGGLEBUTTON,
                             &MainWindow::OnModeButtonToggle, this);
    modeSizer->Add(m_mode_buttons[5], 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 1);

    m_limit_entry = new wxTextCtrl(m_mode_panel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(40, -1),
                                    wxTE_PROCESS_ENTER);
    m_limit_entry->SetMaxLength(10);
    m_limit_entry->Bind(wxEVT_TEXT_ENTER, &MainWindow::OnLimitKeyEntry, this);
    modeSizer->Add(m_limit_entry, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 1);

    m_mode_panel->SetSizer(modeSizer);

    /* Mode panel hidden by default — shown only when in a channel with pref enabled */
    m_mode_panel->Show(false);

    topicSizer->Add(m_mode_panel, 0, wxALIGN_CENTER_VERTICAL);
    m_topic_panel->SetSizer(topicSizer);
    parentSizer->Add(m_topic_panel, 0, wxEXPAND | wxALL, 2);
}

void MainWindow::CreateMainLayout()
{
    /* Main panel */
    m_main_panel = new wxPanel(this, wxID_ANY);

    /* === Left splitter: channel tree | right panel === */
    m_hsplitter_left = new wxSplitterWindow(m_main_panel, wxID_ANY,
                                             wxDefaultPosition, wxDefaultSize,
                                             wxSP_BORDER | wxSP_LIVE_UPDATE);
    m_hsplitter_left->SetMinimumPaneSize(80);

    /* Channel tree (left panel) - matches HexChat tree view */
    m_channel_tree = new wxTreeCtrl(m_hsplitter_left, wxID_ANY,
                                     wxDefaultPosition, wxSize(140, -1),
                                     wxTR_HAS_BUTTONS | wxTR_SINGLE |
                                     wxTR_HIDE_ROOT | wxTR_NO_LINES);
    m_tree_root = m_channel_tree->AddRoot(wxT("Networks"));

    /* === Right panel: (chat panel | userlist) === */
    /* This panel sits in the right pane of the left splitter,
       matching HexChat where the tree spans full height and
       the chat area (with topic/input) sits between tree and userlist */
    m_right_panel = new wxPanel(m_hsplitter_left, wxID_ANY);
    m_right_sizer = new wxBoxSizer(wxVERTICAL);

    /* Right splitter: chat panel | user list */
    m_hsplitter_right = new wxSplitterWindow(m_right_panel, wxID_ANY,
                                              wxDefaultPosition, wxDefaultSize,
                                              wxSP_BORDER | wxSP_LIVE_UPDATE);
    m_hsplitter_right->SetMinimumPaneSize(100);
    m_right_sizer->Add(m_hsplitter_right, 1, wxEXPAND);

    m_right_panel->SetSizer(m_right_sizer);

    /* === Chat panel: left pane of right splitter === */
    /* Contains topic bar, chat text, input box, dialog buttons */
    m_chat_panel = new wxPanel(m_hsplitter_right, wxID_ANY);
    m_chat_sizer = new wxBoxSizer(wxVERTICAL);

    /* Topic bar + mode buttons (inline, same row) */
    CreateModeBar(m_chat_panel, m_chat_sizer);

    /* Topic bar hidden by default — shown only when entering a channel */
    m_topic_panel->Show(false);

    /* --- Parse chat font for input box and chat text --- */
    wxString fontName = wxString::FromUTF8(prefs.pchat_text_font_main);
    if (fontName.IsEmpty()) {
        fontName = wxT("Consolas");
    }
    wxString faceName = fontName.BeforeLast(' ');
    wxString sizeStr = fontName.AfterLast(' ');
    int fontSize = 10;
    if (!sizeStr.IsEmpty()) {
        long sz;
        if (sizeStr.ToLong(&sz)) fontSize = (int)sz;
    }
    if (faceName.IsEmpty()) faceName = fontName;

    wxFont chatFont(fontSize, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL,
                    wxFONTWEIGHT_NORMAL, false, faceName);

    /* === Bottom: nick button + input box (inside chat panel) === */
    m_input_panel = new wxPanel(m_chat_panel, wxID_ANY);
    wxBoxSizer *inputSizer = new wxBoxSizer(wxHORIZONTAL);

    /* Nick button — click to change nickname */
    m_nick_button = new wxButton(m_input_panel, wxID_ANY, wxT("(nick)"),
                                  wxDefaultPosition, wxDefaultSize,
                                  wxBU_EXACTFIT | wxBORDER_NONE);
    wxFont nickFont = m_nick_button->GetFont();
    nickFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_nick_button->SetFont(nickFont);
    m_nick_button->Bind(wxEVT_BUTTON, &MainWindow::OnNickButtonClick, this);
    inputSizer->Add(m_nick_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    /* Input text box — use wxStyledTextCtrl for spell-check indicators */
    m_input_box = new wxStyledTextCtrl(m_input_panel, wxID_ANY,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxBORDER_THEME);

    /* Configure STC for single-line input behavior */
    m_input_box->SetMarginWidth(0, 0); /* hide line number margin */
    m_input_box->SetMarginWidth(1, 0); /* hide marker margin */
    m_input_box->SetMarginWidth(2, 0); /* hide fold margin */
    m_input_box->SetUseVerticalScrollBar(false);
    m_input_box->SetUseHorizontalScrollBar(false);
    m_input_box->SetWrapMode(wxSTC_WRAP_NONE);
    m_input_box->SetScrollWidth(1);
    m_input_box->SetScrollWidthTracking(true);
    m_input_box->UsePopUp(0); /* disable STC default context menu */
    m_input_box->SetEOLMode(wxSTC_EOL_LF);
    m_input_box->SetCaretLineVisible(false);

    /* Set font via Scintilla styles */
    m_input_box->StyleSetFont(wxSTC_STYLE_DEFAULT, chatFont);
    m_input_box->StyleClearAll();

    /* Constrain height to a single line */
    int inputLineH = m_input_box->TextHeight(0);
    if (inputLineH < 16) inputLineH = 16;
    m_input_box->SetMinSize(wxSize(-1, inputLineH + 6));
    m_input_box->SetMaxSize(wxSize(-1, inputLineH + 6));

    /* Spell check squiggly underline indicator (indicator 0) */
    m_input_box->IndicatorSetStyle(0, wxSTC_INDIC_SQUIGGLE);
    m_input_box->IndicatorSetForeground(0, wxColour(255, 0, 0));

    /* Word chars for STC word boundary detection (used by spell check) */
    m_input_box->SetWordChars(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'");

    /* Only send modification events for text changes, not style/indicator */
    m_input_box->SetModEventMask(wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT);

    inputSizer->Add(m_input_box, 1, wxEXPAND);

    m_input_panel->SetSizer(inputSizer);

    /* Connect key handler for input history */
    m_input_box->Bind(wxEVT_KEY_DOWN, &MainWindow::OnInputKeyDown, this);
    m_input_box->Bind(wxEVT_CONTEXT_MENU, &MainWindow::OnInputContextMenu, this);
    m_input_box->Bind(wxEVT_STC_MODIFIED, &MainWindow::OnInputModified, this);
    m_topic_entry->Bind(wxEVT_TEXT_ENTER, &MainWindow::OnTopicEnter, this);

    /* === Dialog buttons panel (below input, inside chat panel) === */
    m_dlg_buttons_panel = new wxPanel(m_chat_panel, wxID_ANY);
    m_dlg_buttons_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_dlg_buttons_panel->SetSizer(m_dlg_buttons_sizer);
    CreateDialogButtons();
    /* Dialog buttons shown if the list is non-empty */
    m_dlg_buttons_panel->Show(dlgbutton_list != nullptr);

    /* Chat text display (inside chat panel) */
    m_chat_text = new wxRichTextCtrl(m_chat_panel, wxID_ANY,
                                      wxEmptyString, wxDefaultPosition,
                                      wxDefaultSize,
                                      wxRE_MULTILINE | wxRE_READONLY |
                                      wxVSCROLL);
    m_chat_text->SetEditable(false);
    m_chat_text->SetFont(chatFont);
    m_chat_text->SetBackgroundColour(wx_palette_get(COL_BG));
    m_chat_text->SetForegroundColour(wx_palette_get(COL_FG));

    /* Bind right-click on chat area */
    m_chat_text->Bind(wxEVT_RIGHT_DOWN, &MainWindow::OnChatRightClick, this);

    /* Add widgets to chat panel sizer: topic, chat, input, dlg buttons */
    m_chat_sizer->Add(m_chat_text, 1, wxEXPAND);
    m_chat_sizer->Add(m_input_panel, 0, wxEXPAND | wxALL, 2);
    m_chat_sizer->Add(m_dlg_buttons_panel, 0, wxEXPAND | wxLEFT | wxRIGHT, 2);

    m_chat_panel->SetSizer(m_chat_sizer);

    /* User list panel (right side) */
    m_userlist_panel = new wxPanel(m_hsplitter_right, wxID_ANY);
    wxBoxSizer *userSizer = new wxBoxSizer(wxVERTICAL);

    /* User count label at top of user list - matches HexChat "1 ops, 5 total" */
    m_usercount_label = new wxStaticText(m_userlist_panel, wxID_ANY, wxEmptyString,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxALIGN_RIGHT);
    userSizer->Add(m_usercount_label, 0, wxEXPAND | wxALL, 2);

    /* User list (multi-select to match HexChat) */
    m_user_list = new wxListCtrl(m_userlist_panel, wxID_ANY,
                                  wxDefaultPosition, wxSize(130, -1),
                                  wxLC_REPORT |
                                  wxLC_NO_HEADER);
    m_user_list->InsertColumn(0, wxT("Nick"), wxLIST_FORMAT_LEFT, 120);
    LoadUserlistIcons();
    m_user_list->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK,
                      &MainWindow::OnUserlistRightClick, this);
    userSizer->Add(m_user_list, 1, wxEXPAND);

    /* Userlist buttons panel (below user list, 2-column grid like HexChat) */
    m_ul_buttons_panel = new wxPanel(m_userlist_panel, wxID_ANY);
    auto *ulGrid = new wxFlexGridSizer(0, 2, 2, 2);  /* 2 cols, 2px gaps */
    ulGrid->AddGrowableCol(0, 1);
    ulGrid->AddGrowableCol(1, 1);
    m_ul_buttons_sizer = ulGrid;
    m_ul_buttons_panel->SetSizer(m_ul_buttons_sizer);
    userSizer->Add(m_ul_buttons_panel, 0, wxEXPAND);
    /* Don't call CreateUserlistButtons() here — button_list is not loaded yet.
       Buttons will be populated lazily when first needed. */
    m_ul_buttons_panel->Show(prefs.pchat_gui_ulist_buttons);

    m_userlist_panel->SetSizer(userSizer);

    /* Split: tree | right panel */
    int leftSize = prefs.pchat_gui_pane_left_size > 0 ?
                   prefs.pchat_gui_pane_left_size : 140;

    /* If layout is tabs mode, show only right panel (no tree) */
    if (prefs.pchat_gui_tab_layout == 0) {
        m_hsplitter_left->Initialize(m_right_panel);
        m_channel_tree->Hide();
        m_tree_visible = false;
    } else {
        m_hsplitter_left->SplitVertically(m_channel_tree, m_right_panel,
                                           leftSize);
        m_tree_visible = true;
    }

    /* Split: chat | userlist — start with userlist hidden since initial
     * session is SESS_SERVER; SwitchToSession will show it for channels */
    m_hsplitter_right->Initialize(m_chat_panel);

    /* === Main vertical sizer === */
    m_main_sizer = new wxBoxSizer(wxVERTICAL);
    m_main_sizer->Add(m_hsplitter_left, 1, wxEXPAND);

    /* === Bottom tab bar (shown in tabs mode, hidden in tree mode) === */
    m_tab_bar = new wxNotebook(m_main_panel, wxID_ANY,
                                wxDefaultPosition, wxDefaultSize,
                                wxNB_BOTTOM | wxNB_MULTILINE);
    m_main_sizer->Add(m_tab_bar, 0, wxEXPAND);

    /* Bind tab selection event */
    m_tab_bar->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED,
                    &MainWindow::OnTabBarPageChanged, this);

    /* Load tree/tab icons (channel, server, dialog, util) */
    LoadTreeIcons();

    /* Show tab bar only in tabs mode (layout == 0) */
    if (prefs.pchat_gui_tab_layout == 0) {
        m_tab_bar->Show(true);
        m_tab_bar_visible = true;
    } else {
        m_tab_bar->Show(false);
        m_tab_bar_visible = false;
    }

    m_main_panel->SetSizer(m_main_sizer);
}

void MainWindow::CreateStatusBar()
{
    wxStatusBar *sb = wxFrame::CreateStatusBar(3);

    int widths[] = {-1, 160, 100};
    sb->SetStatusWidths(3, widths);

    sb->SetStatusText(wxT("Ready"), 0);

    /* Reposition meter widgets when the status bar resizes */
    sb->Bind(wxEVT_SIZE, [this](wxSizeEvent &evt) {
        evt.Skip();
        CallAfter([this]() { PositionMeterWidgets(); });
    });

    /* Embed lag/throttle meter widgets directly into the status bar */
    CreateMeters();
}

/* ===== Session Management ===== */

void MainWindow::AddSession(struct session *sess, int focus)
{
    SessionTab tab;
    tab.sess = sess;
    tab.tab_color = 0;

    /* Determine network name */
    wxString networkName;
    if (sess->server && sess->server->network) {
        ircnet *net = (ircnet *)sess->server->network;
        networkName = wxString::FromUTF8(net->name);
    } else if (sess->server && sess->server->servername[0]) {
        networkName = wxString::FromUTF8(sess->server->servername);
    } else {
        networkName = wxT("(server)");
    }
    tab.network_name = networkName;

    /* Determine channel name */
    wxString channelName;
    if (sess->channel[0]) {
        channelName = wxString::FromUTF8(sess->channel);
    } else if (sess->type == SESS_SERVER) {
        channelName = networkName;
    } else {
        channelName = wxT("(channel)");
    }
    tab.channel_name = channelName;

    /* Find or create network node in tree */
    wxTreeItemId networkNode;
    wxTreeItemIdValue cookie;
    wxTreeItemId child = m_channel_tree->GetFirstChild(m_tree_root, cookie);
    while (child.IsOk()) {
        if (m_channel_tree->GetItemText(child) == networkName) {
            networkNode = child;
            break;
        }
        child = m_channel_tree->GetNextChild(m_tree_root, cookie);
    }

    if (!networkNode.IsOk()) {
        networkNode = m_channel_tree->AppendItem(m_tree_root, networkName,
                                                  0, 0); /* server icon */
    }

    /* Add channel under network, or use network node for server tab */
    int treeIcon = GetSessionIconIndex(sess);
    if (sess->type == SESS_SERVER) {
        tab.tree_id = networkNode;
        if (treeIcon >= 0)
            m_channel_tree->SetItemImage(networkNode, treeIcon);
    } else {
        tab.tree_id = m_channel_tree->AppendItem(networkNode, channelName,
                                                  treeIcon, treeIcon);
    }

    m_channel_tree->Expand(networkNode);

    m_sessions.push_back(tab);
    m_session_tree_map[sess] = tab.tree_id;

    /* Add a page to the bottom tab bar */
    if (m_tab_bar) {
        wxPanel *dummy = new wxPanel(m_tab_bar, wxID_ANY, wxDefaultPosition, wxSize(0, 0));
        dummy->Show(false);
        int tabIcon = GetSessionIconIndex(sess);
        m_tab_bar->AddPage(dummy, channelName, false, tabIcon);
        m_session_tab_map[sess] = (int)m_tab_bar->GetPageCount() - 1;
    }

    if (focus || !m_current_session) {
        SwitchToSession(sess);
        m_channel_tree->SelectItem(tab.tree_id);
    }

    sess->gui->is_tab = 1;
}

void MainWindow::RemoveSession(struct session *sess)
{
    auto it = m_session_tree_map.find(sess);
    if (it != m_session_tree_map.end()) {
        if (it->second.IsOk())
            m_channel_tree->Delete(it->second);
        m_session_tree_map.erase(it);
    }

    /* Remove the tab bar page and rebuild index map */
    auto tabIt = m_session_tab_map.find(sess);
    if (tabIt != m_session_tab_map.end() && m_tab_bar) {
        int idx = tabIt->second;
        m_tab_switching = true;
        if (idx >= 0 && idx < (int)m_tab_bar->GetPageCount())
            m_tab_bar->DeletePage(idx);
        m_tab_switching = false;
        m_session_tab_map.erase(tabIt);
        /* Rebuild indices for remaining sessions */
        RebuildTabBarIndices();
    }

    for (auto sit = m_sessions.begin(); sit != m_sessions.end(); ++sit) {
        if (sit->sess == sess) {
            m_sessions.erase(sit);
            break;
        }
    }

    if (m_current_session == sess) {
        m_current_session = nullptr;
        if (!m_sessions.empty()) {
            SwitchToSession(m_sessions.back().sess);
        }
    }
}

/* Helper callback to repopulate userlist when switching sessions */
static int userlist_repopulate_cb(const void *key, void *data)
{
    struct User *user = (struct User *)key;
    MainWindow *win = (MainWindow *)data;
    session *sess = win->GetCurrentSession();
    if (sess)
        win->UserlistInsert(sess, user, -1, user->selected);
    return TRUE; /* continue traversal */
}

void MainWindow::SwitchToSession(struct session *sess)
{
    if (m_current_session == sess) return;

    /* Reset tab completion state when switching sessions */
    TabCompClean();

    m_current_session = sess;
    current_sess = sess;
    current_tab = sess;

    /* Update topic */
    if (sess->topic) {
        m_topic_entry->SetValue(wxString::FromUTF8(sess->topic));
    } else {
        m_topic_entry->SetValue(wxEmptyString);
    }

    /* Update nick label */
    if (sess->server && sess->server->nick[0]) {
        m_nick_button->SetLabel(wxString::FromUTF8(sess->server->nick));
    }

    /* Update title */
    UpdateTitle(sess);

    /* Update userlist */
    m_user_list->Freeze();
    m_user_list->DeleteAllItems();
    if (sess->type == SESS_CHANNEL && sess->usertree) {
        tree_foreach(sess->usertree,
                     (tree_traverse_func *)userlist_repopulate_cb, this);
    }
    m_user_list->Thaw();

    /* Update user count */
    UserlistNumbers(sess);

    /* Clear and repopulate chat from the per-session text buffer.
       Limit to the last 500 lines for fast tab switching — the full
       scrollback is still stored in text_lines for /lastlog etc. */
    m_chat_text->Freeze();
    m_chat_text->Clear();
    SessionTab *tab = FindSessionTab(sess);
    if (tab && !tab->text_lines.empty()) {
        size_t start = 0;
        if (tab->text_lines.size() > 500)
            start = tab->text_lines.size() - 500;
        for (size_t j = start; j < tab->text_lines.size(); j++) {
            AppendIrcText(m_chat_text, tab->text_lines[j].text,
                          tab->text_lines[j].stamp);
        }
    }
    m_chat_text->Thaw();
    m_chat_text->ShowPosition(m_chat_text->GetLastPosition());

    /* Update mode buttons */
    for (int i = 0; i < NUM_FLAG_WIDS; i++) {
        m_mode_buttons[i]->SetValue(false);
    }
    m_limit_entry->SetValue(wxEmptyString);
    m_key_entry->SetValue(wxEmptyString);

    /* Show/hide mode panel based on session type */
    m_mode_panel->Show(sess->type == SESS_CHANNEL &&
                       prefs.pchat_gui_mode_buttons);
    m_topic_panel->Layout();

    /* Show/hide topic bar based on session type — hidden on server views */
    m_topic_panel->Show(sess->type == SESS_CHANNEL &&
                        prefs.pchat_gui_topicbar);
    m_chat_panel->Layout();

    /* Show/hide userlist based on session type */
    if (sess->type == SESS_CHANNEL && !prefs.pchat_gui_ulist_hide) {
        if (!m_hsplitter_right->IsSplit()) {
            int rightSize = prefs.pchat_gui_pane_right_size > 0 ?
                            prefs.pchat_gui_pane_right_size : 130;
            m_hsplitter_right->SplitVertically(m_chat_panel, m_userlist_panel,
                                                -rightSize);
        }
    } else if (sess->type != SESS_CHANNEL) {
        if (m_hsplitter_right->IsSplit()) {
            m_hsplitter_right->Unsplit(m_userlist_panel);
        }
    }

    /* Update channel key & limit from restore_gui if available */
    if (sess->res) {
        if (sess->res->key_text)
            m_key_entry->SetValue(wxString::FromUTF8(sess->res->key_text));
        if (sess->res->limit_text)
            m_limit_entry->SetValue(wxString::FromUTF8(sess->res->limit_text));
        /* Restore mode button states */
        for (int i = 0; i < NUM_FLAG_WIDS; i++) {
            m_mode_buttons[i]->SetValue(sess->res->flag_wid_state[i] != 0);
        }
    }

    /* Reset tab color for current tab */
    SetTabColor(sess, FE_COLOR_NONE);

    /* Select in tree */
    auto it = m_session_tree_map.find(sess);
    if (it != m_session_tree_map.end() && it->second.IsOk()) {
        m_tab_switching = true;
        m_channel_tree->SelectItem(it->second);
        m_tab_switching = false;
    }

    /* Select in tab bar */
    UpdateTabBarSelection(sess);

    /* Focus input box */
    m_input_box->SetFocus();

    /* Lazily populate userlist buttons if not yet created */
    if (m_ul_buttons_sizer &&
        m_ul_buttons_sizer->GetItemCount() == 0 &&
        button_list != nullptr) {
        CreateUserlistButtons();
        m_ul_buttons_panel->Show(prefs.pchat_gui_ulist_buttons);
        m_userlist_panel->Layout();
    }
}

SessionTab *MainWindow::FindSessionTab(struct session *sess)
{
    for (auto &tab : m_sessions) {
        if (tab.sess == sess)
            return &tab;
    }
    return nullptr;
}

/* ===== Tab Completion ===== */

void MainWindow::TabCompClean()
{
    m_tab_comp_active = false;
    m_tab_comp_prefix.clear();
    m_tab_comp_prefix_len = 0;
}

void MainWindow::DoTabCompletion(bool shift_held)
{
    if (!m_current_session) return;

    wxString text = m_input_box->GetText();
    if (text.IsEmpty()) return;

    long cursor_pos = m_input_box->GetCurrentPos();
    std::string utf8text(text.utf8_str().data());

    /* Find the start of the word at cursor position */
    int word_start = (int)cursor_pos;
    while (word_start > 0 && utf8text[word_start - 1] != ' ')
        word_start--;

    /* Detect if we're re-completing (cursor is after a previous completion) */
    bool is_recomp = false;
    int skip_suffix = 0;

    /* Check if previous char is space (after completion suffix) */
    if (cursor_pos > 0 && utf8text[cursor_pos - 1] == ' ') {
        skip_suffix = 1;
        int check_pos = (int)cursor_pos - 2;
        if (check_pos >= 0) {
            char suffix_char = prefs.pchat_completion_suffix[0];
            if (suffix_char && utf8text[check_pos] == suffix_char) {
                skip_suffix = 2;
            }
        }
        if (m_tab_comp_active) {
            is_recomp = true;
            /* Recalculate word_start before suffix */
            word_start = (int)cursor_pos - skip_suffix;
            while (word_start > 0 && utf8text[word_start - 1] != ' ')
                word_start--;
        }
    }

    /* Detect command completion (/command) */
    bool is_cmd = false;
    int prefix_start = word_start;
    if (word_start == 0 && !utf8text.empty() &&
        utf8text[0] == prefs.pchat_input_command_char[0]) {
        is_cmd = true;
        prefix_start = 1; /* skip the '/' */
    }

    /* Extract the partial word */
    int word_end = is_recomp ? ((int)cursor_pos - skip_suffix) : (int)cursor_pos;
    if (word_end <= prefix_start) {
        m_tab_comp_active = false;
        return;
    }

    std::string partial;
    if (is_recomp && m_tab_comp_active) {
        partial = m_tab_comp_prefix;
    } else {
        partial = utf8text.substr(prefix_start, word_end - prefix_start);
        m_tab_comp_prefix = partial;
        m_tab_comp_prefix_len = (int)partial.length();
        m_tab_comp_active = true;
    }

    if (partial.empty()) return;

    /* Build candidate list */
    std::vector<std::string> candidates;
    bool is_nick = !is_cmd && partial[0] != '#' && partial[0] != '&';

    if (is_nick) {
        /* Nick completion from userlist */
        if (m_current_session->type == SESS_DIALOG) {
            /* In a dialog, complete the other person's name */
            if (rfc_ncasecmp((char *)m_current_session->channel,
                             (char *)partial.c_str(), (int)partial.length()) == 0) {
                candidates.push_back(m_current_session->channel);
            }
        } else {
            GList *users = userlist_double_list(m_current_session);
            for (GList *l = users; l; l = l->next) {
                struct User *user = (struct User *)l->data;
                if (rfc_ncasecmp(user->nick, (char *)partial.c_str(),
                                 (int)partial.length()) == 0) {
                    candidates.push_back(user->nick);
                }
            }
            g_list_free(users);

            /* Sort by last-talk time if configured */
            if (prefs.pchat_completion_sort == 1 && m_current_session->usertree) {
                std::sort(candidates.begin(), candidates.end(),
                    [this](const std::string &a, const std::string &b) {
                        struct User *ua = userlist_find(m_current_session,
                                                        a.c_str());
                        struct User *ub = userlist_find(m_current_session,
                                                        b.c_str());
                        if (!ua || !ub) return false;
                        if (ua->me) return false;
                        if (ub->me) return true;
                        return ua->lasttalk > ub->lasttalk;
                    });
            }
        }
    } else if (is_cmd) {
        /* Command completion */
        for (int i = 0; xc_cmds[i].name != nullptr; i++) {
            if (rfc_ncasecmp(xc_cmds[i].name, (char *)partial.c_str(),
                             (int)partial.length()) == 0) {
                candidates.push_back(xc_cmds[i].name);
            }
        }
        /* Plugin commands */
        GList *plugin_cmds = plugin_command_list(nullptr);
        for (GList *l = plugin_cmds; l; l = l->next) {
            char *cmd_name = (char *)l->data;
            if (cmd_name && rfc_ncasecmp(cmd_name, (char *)partial.c_str(),
                                         (int)partial.length()) == 0) {
                candidates.push_back(cmd_name);
            }
        }
        g_list_free(plugin_cmds);
    } else {
        /* Channel completion */
        for (GSList *slist = sess_list; slist; slist = slist->next) {
            session *s = (session *)slist->data;
            if (s->channel[0] && (s->channel[0] == '#' || s->channel[0] == '&')) {
                if (rfc_ncasecmp(s->channel, (char *)partial.c_str(),
                                 (int)partial.length()) == 0) {
                    candidates.push_back(s->channel);
                }
            }
        }
    }

    if (candidates.empty()) {
        m_tab_comp_active = false;
        return;
    }

    /* If cycling (re-completing), find current match and go to next/prev */
    std::string result;
    if (is_recomp && candidates.size() > 1) {
        std::string current_word = utf8text.substr(prefix_start,
                                                    word_end - prefix_start);
        auto it = std::find(candidates.begin(), candidates.end(), current_word);
        if (it != candidates.end()) {
            if (!shift_held) {
                ++it;
                if (it == candidates.end()) it = candidates.begin();
            } else {
                if (it == candidates.begin()) it = candidates.end();
                --it;
            }
            result = *it;
        } else {
            result = candidates[0];
        }
    } else if (candidates.size() == 1 ||
               (prefs.pchat_completion_amount > 0 &&
                (int)candidates.size() <= prefs.pchat_completion_amount)) {
        result = candidates[0];
    } else {
        /* Multiple matches: find longest common prefix (bash-style) */
        result = candidates[0];
        for (size_t ci = 1; ci < candidates.size(); ci++) {
            size_t match_len = 0;
            while (match_len < result.length() &&
                   match_len < candidates[ci].length() &&
                   g_ascii_tolower(result[match_len]) ==
                   g_ascii_tolower(candidates[ci][match_len])) {
                match_len++;
            }
            result = result.substr(0, match_len);
        }

        /* Print all candidates to chat */
        std::string candidate_line;
        for (const auto &c : candidates) {
            if (!candidate_line.empty()) candidate_line += " ";
            candidate_line += c;
        }
        PrintText(m_current_session, wxString::FromUTF8(candidate_line.c_str()), 0);

        if (result.length() <= partial.length()) {
            /* No common prefix longer than what user typed — don't modify input */
            return;
        }
    }

    /* Build the new input text */
    std::string newtext;
    if (word_start > 0) {
        newtext = utf8text.substr(0, word_start);
    }
    if (is_cmd) {
        newtext += prefs.pchat_input_command_char[0];
    }
    newtext += result;

    /* Add suffix for nick at start of line */
    if (is_nick && word_start == 0 && prefs.pchat_completion_suffix[0]) {
        newtext += prefs.pchat_completion_suffix[0];
    }
    newtext += " ";

    int new_cursor = (int)newtext.length();

    /* Append any text after cursor */
    if ((int)cursor_pos < (int)utf8text.length()) {
        int post_start = (int)cursor_pos;
        if (is_recomp) post_start = (int)cursor_pos;
        newtext += utf8text.substr(post_start);
    }

    m_input_box->SetText(wxString::FromUTF8(newtext.c_str()));
    m_input_box->GotoPos(new_cursor);
}

/* ===== Text Display ===== */

void MainWindow::PrintText(struct session *sess, const wxString &text,
                            time_t stamp)
{
    /* Ensure the text ends with \n.
       Normal messages from format_event always end with \n, but
       scrollback_load strips the trailing \n (g_data_input_stream_read_line).
       We normalize here so every stored line consistently ends with \n. */
    wxString normalized = text;
    if (normalized.IsEmpty() || normalized.Last() != '\n')
        normalized += '\n';

    /* Store in the per-session buffer so it survives tab switches */
    SessionTab *tab = FindSessionTab(sess);
    if (tab) {
        tab->text_lines.push_back({normalized, stamp});
        /* Enforce scrollback limit */
        if (tab->text_lines.size() > SessionTab::MAX_LINES) {
            tab->text_lines.erase(
                tab->text_lines.begin(),
                tab->text_lines.begin() +
                    (tab->text_lines.size() - SessionTab::MAX_LINES));
        }
    }

    /* Render to the chat widget only if this is the current session */
    if (sess == m_current_session) {
        AppendIrcText(m_chat_text, normalized, stamp);
        m_chat_text->ShowPosition(m_chat_text->GetLastPosition());
    }
}

void MainWindow::AppendIrcText(wxRichTextCtrl *ctrl, const wxString &text,
                                time_t stamp)
{
    /* Parse IRC formatting codes and render them.
     *
     * The backend sends text with embedded control bytes:
     *   \003CC[,CC] - mIRC color (fg, optional bg)
     *   \002       - bold toggle
     *   \006       - blink (ignored)
     *   \007       - beep  (ignored)
     *   \010       - hidden toggle (text between pairs is invisible)
     *   \017       - reset all formatting
     *   \026       - reverse (swap fg/bg)
     *   \035       - italic toggle
     *   \036       - strikethrough toggle
     *   \037       - underline toggle
     *   \n         - line break (separates messages)
     *   \t         - indent marker (nick/timestamp | body)
     *
     * Any other control char < 0x20 (except printable tab/newline)
     * is silently skipped, matching the GTK3 renderer.
     */
    ctrl->SetInsertionPointEnd();
    ctrl->Freeze();

    wxFont baseFont = ctrl->GetFont();

    /* Lazy font cache — up to 16 variants (bold × italic × underline ×
       strikethrough).  Avoids costly wxFont copies on every style flush.
       Most IRC lines use only the plain font (index 0). */
    wxFont fontCache[16];
    bool  fontCached[16] = {};
    fontCache[0] = baseFont;
    fontCached[0] = true;

    auto getFont = [&](int idx) -> const wxFont& {
        if (!fontCached[idx]) {
            fontCache[idx] = baseFont;
            if (idx & 1) fontCache[idx].SetWeight(wxFONTWEIGHT_BOLD);
            if (idx & 2) fontCache[idx].SetStyle(wxFONTSTYLE_ITALIC);
            if (idx & 4) fontCache[idx].SetUnderlined(true);
            if (idx & 8) fontCache[idx].SetStrikethrough(true);
            fontCached[idx] = true;
        }
        return fontCache[idx];
    };

    wxColour fgColor = wx_palette_get(COL_FG);
    wxColour bgColor = wx_palette_get(COL_BG);
    bool bold = false;
    bool underline = false;
    bool italic = false;
    bool strikethrough = false;
    bool hidden = false;
    bool line_start = true;  /* true at start & after each \n */

    wxString buffer;
    size_t i = 0;

    auto flushBuffer = [&]() {
        if (buffer.IsEmpty()) return;
        if (hidden) {
            buffer.Clear();
            return;  /* hidden text — discard */
        }
        wxRichTextAttr style;
        int fontIdx = (bold ? 1 : 0) | (italic ? 2 : 0) |
                      (underline ? 4 : 0) | (strikethrough ? 8 : 0);
        style.SetFont(getFont(fontIdx));
        style.SetTextColour(fgColor);
        if (bgColor != wx_palette_get(COL_BG)) {
            style.SetBackgroundColour(bgColor);
        }
        ctrl->BeginStyle(style);
        ctrl->WriteText(buffer);
        ctrl->EndStyle();
        buffer.Clear();
    };

    /* Emit a timestamp at the start of each line if enabled */
    auto emitTimestamp = [&]() {
        if (!prefs.pchat_stamp_text) return;
        time_t t = stamp ? stamp : time(nullptr);
        char *stamp_str = nullptr;
        int stamp_len = get_stamp_str(prefs.pchat_stamp_text_format, t, &stamp_str);
        if (stamp_str && stamp_len > 0) {
            wxRichTextAttr tsStyle;
            wxFont tsFont = baseFont;
            tsStyle.SetFont(tsFont);
            tsStyle.SetTextColour(wxColour(128, 128, 128));
            ctrl->BeginStyle(tsStyle);
            ctrl->WriteText(wxString::FromUTF8(stamp_str));
            ctrl->EndStyle();
            g_free(stamp_str);
        }
    };

    while (i < text.length()) {
        wxChar ch = text[i];

        /* Emit timestamp at the beginning of each logical line */
        if (line_start && ch != '\n') {
            emitTimestamp();
            line_start = false;
        }

        switch ((int)ch) {
        case '\n': /* newline — end of message line */
            flushBuffer();
            ctrl->Newline();
            /* Reset formatting for next line */
            bold = false;
            underline = false;
            italic = false;
            strikethrough = false;
            hidden = false;
            fgColor = wx_palette_get(COL_FG);
            bgColor = wx_palette_get(COL_BG);
            line_start = true;
            i++;
            break;

        case '\t': /* indent marker — rendered as a space separator */
            flushBuffer();
            buffer += wxT(" ");
            i++;
            break;

        case 3: /* mIRC color \003 */
        {
            flushBuffer();
            i++;
            /* Parse foreground color (up to 2 digits) */
            wxString fgNum;
            while (i < text.length() && text[i] >= '0' && text[i] <= '9' &&
                   fgNum.length() < 2) {
                fgNum += text[i++];
            }
            if (!fgNum.IsEmpty()) {
                long fg;
                fgNum.ToLong(&fg);
                fgColor = GetIrcColor((int)(fg % 16));
            } else {
                /* Bare \003 resets colors */
                fgColor = wx_palette_get(COL_FG);
                bgColor = wx_palette_get(COL_BG);
            }
            /* Parse optional background ,CC */
            if (i < text.length() && text[i] == ',') {
                i++;
                wxString bgNum;
                while (i < text.length() && text[i] >= '0' &&
                       text[i] <= '9' && bgNum.length() < 2) {
                    bgNum += text[i++];
                }
                if (!bgNum.IsEmpty()) {
                    long bg;
                    bgNum.ToLong(&bg);
                    bgColor = GetIrcColor((int)(bg % 16));
                }
            }
            break;
        }

        case 2: /* bold \002 */
            flushBuffer();
            bold = !bold;
            i++;
            break;

        case 6:  /* blink \006 — ignored */
        case 7:  /* beep  \007 — ignored */
            i++;
            break;

        case 8: /* hidden \010 — toggle invisible text */
            flushBuffer();
            hidden = !hidden;
            i++;
            break;

        case 15: /* reset \017 */
            flushBuffer();
            bold = false;
            underline = false;
            italic = false;
            strikethrough = false;
            hidden = false;
            fgColor = wx_palette_get(COL_FG);
            bgColor = wx_palette_get(COL_BG);
            i++;
            break;

        case 22: /* reverse \026 */
            flushBuffer();
            std::swap(fgColor, bgColor);
            i++;
            break;

        case 29: /* italic \035 */
            flushBuffer();
            italic = !italic;
            i++;
            break;

        case 30: /* strikethrough \036 */
            flushBuffer();
            strikethrough = !strikethrough;
            i++;
            break;

        case 31: /* underline \037 */
            flushBuffer();
            underline = !underline;
            i++;
            break;

        default:
            if (ch < 32) {
                /* Skip any other control characters */
                i++;
            } else {
                buffer += ch;
                i++;
            }
            break;
        }
    }

    flushBuffer();
    ctrl->Thaw();
}

wxColour MainWindow::GetIrcColor(int index)
{
    if (index < 0 || index >= 32)
        return wx_palette_get(COL_FG);
    return wx_palette_get(index);
}

void MainWindow::ClearText(struct session *sess, int lines)
{
    /* Clear per-session buffer */
    SessionTab *tab = FindSessionTab(sess);
    if (tab) {
        tab->text_lines.clear();
    }
    if (sess == m_current_session) {
        m_chat_text->Clear();
    }
}

void MainWindow::ApplyPaletteColors()
{
    /* Update chat widget background and foreground */
    m_chat_text->SetBackgroundColour(wx_palette_get(COL_BG));
    m_chat_text->SetForegroundColour(wx_palette_get(COL_FG));

    /* Re-render current session content with new colors */
    if (m_current_session) {
        SessionTab *tab = FindSessionTab(m_current_session);
        if (tab && !tab->text_lines.empty()) {
            m_chat_text->Freeze();
            m_chat_text->Clear();
            size_t start = 0;
            if (tab->text_lines.size() > 500)
                start = tab->text_lines.size() - 500;
            for (size_t j = start; j < tab->text_lines.size(); j++) {
                AppendIrcText(m_chat_text, tab->text_lines[j].text,
                              tab->text_lines[j].stamp);
            }
            m_chat_text->Thaw();
            m_chat_text->ShowPosition(m_chat_text->GetLastPosition());
        }
    }

    m_chat_text->Refresh();
}

/* ===== Topic ===== */

void MainWindow::SetTopic(struct session *sess, const wxString &topic)
{
    if (sess == m_current_session) {
        m_topic_entry->SetValue(topic);
    }
}

/* ===== Tab/Channel ===== */

void MainWindow::SetChannel(struct session *sess)
{
    SessionTab *tab = FindSessionTab(sess);
    if (!tab) return;

    wxString name;
    if (sess->channel[0]) {
        name = wxString::FromUTF8(sess->channel);
    } else if (sess->type == SESS_SERVER && sess->server) {
        if (sess->server->network) {
            ircnet *net = (ircnet *)sess->server->network;
            name = wxString::FromUTF8(net->name);
        } else if (sess->server->servername[0]) {
            name = wxString::FromUTF8(sess->server->servername);
        }
    }

    if (!name.IsEmpty()) {
        tab->channel_name = name;
        if (tab->tree_id.IsOk()) {
            m_channel_tree->SetItemText(tab->tree_id, name);
        }
        /* Update tab bar label */
        UpdateTabBarLabel(sess, name);
    }

    if (sess == m_current_session) {
        UpdateTitle(sess);
        /* Show mode panel for channels (if pref enabled) */
        m_mode_panel->Show(sess->type == SESS_CHANNEL && prefs.pchat_gui_mode_buttons);
        m_topic_panel->Layout();
    }
}

void MainWindow::SetTabColor(struct session *sess, int color)
{
    SessionTab *tab = FindSessionTab(sess);
    if (!tab) return;

    tab->tab_color = color & ~FE_COLOR_ALLFLAGS;

    if (tab->tree_id.IsOk()) {
        wxColour textColor;
        switch (tab->tab_color) {
        case FE_COLOR_NEW_DATA:
            textColor = wxColour(0, 0, 200);    /* blue for data */
            break;
        case FE_COLOR_NEW_MSG:
            textColor = wxColour(200, 0, 0);    /* red for message */
            break;
        case FE_COLOR_NEW_HILIGHT:
            textColor = wxColour(0, 160, 0);    /* green for highlight */
            break;
        default:
            textColor = *wxBLACK;
            break;
        }
        m_channel_tree->SetItemTextColour(tab->tree_id, textColor);
    }
}

void MainWindow::ClearChannel(struct session *sess)
{
    if (sess == m_current_session) {
        m_topic_entry->SetValue(wxEmptyString);
        UserlistClear(sess);
    }
}

void MainWindow::UpdateTitle(struct session *sess)
{
    if (sess != m_current_session) return;

    wxString title;
    if (sess->channel[0]) {
        title = wxString::Format(wxT("%s @ %s - PChat"),
                                  wxString::FromUTF8(sess->channel),
                                  sess->server ?
                                      wxString::FromUTF8(sess->server->servername) :
                                      wxString(wxT("(disconnected)")));
    } else if (sess->server && sess->server->servername[0]) {
        title = wxString::Format(wxT("%s - PChat"),
                                  wxString::FromUTF8(sess->server->servername));
    } else {
        title = wxT("PChat");
    }

    /* Prepend nick if enabled */
    if (prefs.pchat_gui_win_nick && sess->server && sess->server->nick[0]) {
        title = wxString::Format(wxT("%s @ %s"),
                                  wxString::FromUTF8(sess->server->nick),
                                  title);
    }

    SetTitle(title);
}

/* ===== Mode Buttons ===== */

void MainWindow::UpdateModeButtons(struct session *sess, char mode, char sign)
{
    if (sess != m_current_session) return;

    /* Map mode char to flag_wid index:
       0=c 1=n 2=t 3=i 4=m 5=l 6=k 7=b */
    int idx = -1;
    switch (mode) {
    case 'c': idx = 0; break;
    case 'n': idx = 1; break;
    case 't': idx = 2; break;
    case 'i': idx = 3; break;
    case 'm': idx = 4; break;
    case 'l': idx = 5; break;
    case 'k': idx = 6; break;
    case 'b': idx = 7; break;
    }
    if (idx >= 0 && idx < NUM_FLAG_WIDS) {
        bool on = (sign == '+');
        m_mode_buttons[idx]->SetValue(on);
        if (sess->res)
            sess->res->flag_wid_state[idx] = on ? 1 : 0;
    }
}

void MainWindow::UpdateChannelKey(struct session *sess)
{
    if (sess != m_current_session) return;
    if (sess->res && sess->res->key_text) {
        m_key_entry->SetValue(wxString::FromUTF8(sess->res->key_text));
    } else {
        m_key_entry->SetValue(wxEmptyString);
    }
}

void MainWindow::UpdateChannelLimit(struct session *sess)
{
    if (sess != m_current_session) return;
    if (sess->res && sess->res->limit_text) {
        m_limit_entry->SetValue(wxString::FromUTF8(sess->res->limit_text));
    } else {
        m_limit_entry->SetValue(wxEmptyString);
    }
}

void MainWindow::SetNonChannel(struct session *sess, int state)
{
    if (sess != m_current_session) return;

    /* Hide/show channel-specific widgets */
    m_mode_panel->Show(state == 0 && prefs.pchat_gui_mode_buttons);
    m_topic_panel->Layout();
}

/* ===== Userlist ===== */

/* Icon indices in m_userlist_imagelist:
   0 = voice (+)
   1 = halfop (%)
   2 = op (@)
   3 = owner (1 level above op)
   4 = founder (2 levels above op)
   5 = netop (3 levels above op)
*/

/* Tree/tab icon indices:
   0 = server
   1 = channel
   2 = dialog (also used for notices, snotices, etc.)
   3 = utility
*/
void MainWindow::LoadTreeIcons()
{
    wxArrayString searchPaths;
    wxString exeDir = wxPathOnly(wxStandardPaths::Get().GetExecutablePath());
    searchPaths.Add(exeDir + wxFILE_SEP_PATH + wxT("icons"));
    searchPaths.Add(exeDir + wxFILE_SEP_PATH + wxT("..") + wxFILE_SEP_PATH +
                    wxT("..") + wxFILE_SEP_PATH + wxT("data") +
                    wxFILE_SEP_PATH + wxT("icons"));
    searchPaths.Add(exeDir + wxFILE_SEP_PATH + wxT("..") + wxFILE_SEP_PATH +
                    wxT("share") + wxFILE_SEP_PATH + wxT("pchat") +
                    wxFILE_SEP_PATH + wxT("icons"));

#ifdef __WXMSW__
    wxString msysPrefix;
    if (wxGetEnv(wxT("MSYSTEM_PREFIX"), &msysPrefix)) {
        searchPaths.Add(msysPrefix + wxT("/share/pchat/icons"));
    }
#endif

    wxString iconDir;
    for (const auto &path : searchPaths) {
        wxString test = path + wxFILE_SEP_PATH + wxT("tree_channel.png");
        if (wxFileExists(test)) {
            iconDir = path;
            break;
        }
    }

    if (iconDir.IsEmpty()) {
        wxLogDebug(wxT("Tree icons not found"));
        return;
    }

    wxLogDebug(wxT("Loading tree icons from: %s"), iconDir);

    const wxString iconFiles[] = {
        wxT("tree_server.png"),   /* 0 */
        wxT("tree_channel.png"),  /* 1 */
        wxT("tree_dialog.png"),   /* 2 */
        wxT("tree_util.png"),     /* 3 */
    };

    wxImage testImg(iconDir + wxFILE_SEP_PATH + iconFiles[0], wxBITMAP_TYPE_PNG);
    int w = 16, h = 16;
    if (testImg.IsOk()) {
        w = testImg.GetWidth();
        h = testImg.GetHeight();
    }

    m_tree_imagelist = new wxImageList(w, h, false, 4);

    for (int i = 0; i < 4; i++) {
        wxString path = iconDir + wxFILE_SEP_PATH + iconFiles[i];
        wxImage img(path, wxBITMAP_TYPE_PNG);
        if (img.IsOk()) {
            if (img.GetWidth() != w || img.GetHeight() != h)
                img.Rescale(w, h, wxIMAGE_QUALITY_HIGH);
            m_tree_imagelist->Add(wxBitmap(img));
        } else {
            wxBitmap blank(w, h, 32);
            m_tree_imagelist->Add(blank);
        }
    }

    m_channel_tree->SetImageList(m_tree_imagelist);
    m_tab_bar->SetImageList(m_tree_imagelist);
}

int MainWindow::GetSessionIconIndex(struct session *sess)
{
    if (!sess || !m_tree_imagelist) return -1;
    switch (sess->type) {
    case SESS_SERVER:  return 0;
    case SESS_CHANNEL: return 1;
    default:           return 2; /* dialog, notices, snotices */
    }
}

void MainWindow::LoadUserlistIcons()
{
    /* Search for icon PNGs: build dir, source data dir, installed share dir */
    wxArrayString searchPaths;
    wxString exeDir = wxPathOnly(wxStandardPaths::Get().GetExecutablePath());
    searchPaths.Add(exeDir + wxFILE_SEP_PATH + wxT("icons"));
    searchPaths.Add(exeDir + wxFILE_SEP_PATH + wxT("..") + wxFILE_SEP_PATH +
                    wxT("..") + wxFILE_SEP_PATH + wxT("data") +
                    wxFILE_SEP_PATH + wxT("icons"));
    searchPaths.Add(exeDir + wxFILE_SEP_PATH + wxT("..") + wxFILE_SEP_PATH +
                    wxT("share") + wxFILE_SEP_PATH + wxT("pchat") +
                    wxFILE_SEP_PATH + wxT("icons"));

#ifdef __WXMSW__
    wxString msysPrefix;
    if (wxGetEnv(wxT("MSYSTEM_PREFIX"), &msysPrefix)) {
        searchPaths.Add(msysPrefix + wxT("/share/pchat/icons"));
    }
#endif

    wxString iconDir;
    for (const auto &path : searchPaths) {
        wxString test = path + wxFILE_SEP_PATH + wxT("ulist_op.png");
        if (wxFileExists(test)) {
            iconDir = path;
            break;
        }
    }

    if (iconDir.IsEmpty()) {
        wxLogDebug(wxT("Userlist icons not found"));
        return;
    }

    wxLogDebug(wxT("Loading userlist icons from: %s"), iconDir);

    /* Load the 6 icons in order: voice, halfop, op, owner, founder, netop */
    const wxString iconFiles[] = {
        wxT("ulist_voice.png"),
        wxT("ulist_halfop.png"),
        wxT("ulist_op.png"),
        wxT("ulist_owner.png"),
        wxT("ulist_founder.png"),
        wxT("ulist_netop.png"),
    };

    /* Determine icon size from the first loadable image */
    wxImage testImg(iconDir + wxFILE_SEP_PATH + iconFiles[0], wxBITMAP_TYPE_PNG);
    int w = 16, h = 16;
    if (testImg.IsOk()) {
        w = testImg.GetWidth();
        h = testImg.GetHeight();
    }

    m_userlist_imagelist = new wxImageList(w, h, false, 6);

    for (int i = 0; i < 6; i++) {
        wxString path = iconDir + wxFILE_SEP_PATH + iconFiles[i];
        wxImage img(path, wxBITMAP_TYPE_PNG);
        if (img.IsOk()) {
            if (img.GetWidth() != w || img.GetHeight() != h)
                img.Rescale(w, h, wxIMAGE_QUALITY_HIGH);
            m_userlist_imagelist->Add(wxBitmap(img));
        } else {
            /* Placeholder transparent bitmap */
            wxBitmap blank(w, h, 32);
            m_userlist_imagelist->Add(blank);
        }
    }

    m_user_list->SetImageList(m_userlist_imagelist, wxIMAGE_LIST_SMALL);
}

int MainWindow::GetUserIconIndex(struct User *user)
{
    if (!user || !m_userlist_imagelist) return -1;

    /* Direct prefix mapping — matches GTK3 get_user_icon() */
    switch (user->prefix[0]) {
    case 0:   return -1;     /* no prefix = no icon */
    case '+':  return 0;     /* voice */
    case '%':  return 1;     /* halfop */
    case '@':  return 2;     /* op */
    default:
        break;
    }

    /* For prefixes above @, determine how many levels above op.
       Need the server's nick_prefixes string. */
    if (m_current_session && m_current_session->server) {
        server *serv = m_current_session->server;
        char *pre = strchr(serv->nick_prefixes, '@');
        if (pre && pre != serv->nick_prefixes) {
            pre--;
            int level = 0;
            while (1) {
                if (pre[0] == user->prefix[0]) {
                    switch (level) {
                    case 0: return 3;  /* owner  — 1 above op */
                    case 1: return 4;  /* founder — 2 above op */
                    case 2: return 5;  /* netop  — 3 above op */
                    }
                    break;
                }
                level++;
                if (pre == serv->nick_prefixes)
                    break;
                pre--;
            }
        }
    }

    return -1;
}

void MainWindow::UserlistInsert(struct session *sess, struct User *user,
                                 int row, bool sel)
{
    if (sess != m_current_session) return;

    wxString nick = wxString::FromUTF8(user->nick);
    int iconIdx = GetUserIconIndex(user);

    long idx;
    if (row >= 0 && row < m_user_list->GetItemCount()) {
        idx = m_user_list->InsertItem(row, nick, iconIdx);
    } else {
        idx = m_user_list->InsertItem(m_user_list->GetItemCount(), nick, iconIdx);
    }

    /* Color nick by status */
    if (user->op) {
        m_user_list->SetItemTextColour(idx, wxColour(200, 0, 0));
    } else if (user->hop) {
        m_user_list->SetItemTextColour(idx, wxColour(128, 0, 128));
    } else if (user->voice) {
        m_user_list->SetItemTextColour(idx, wxColour(0, 128, 0));
    }

    /* Store user pointer for lookup */
    m_user_list->SetItemPtrData(idx, (wxUIntPtr)user);

    if (sel) {
        m_user_list->SetItemState(idx, wxLIST_STATE_SELECTED,
                                   wxLIST_STATE_SELECTED);
    }
}

void MainWindow::UserlistRemove(struct session *sess, struct User *user)
{
    if (sess != m_current_session) return;

    for (long i = 0; i < m_user_list->GetItemCount(); i++) {
        if ((struct User *)m_user_list->GetItemData(i) == user) {
            m_user_list->DeleteItem(i);
            break;
        }
    }
}

void MainWindow::UserlistRehash(struct session *sess, struct User *user)
{
    if (sess != m_current_session) return;

    for (long i = 0; i < m_user_list->GetItemCount(); i++) {
        if ((struct User *)m_user_list->GetItemData(i) == user) {
            wxString nick = wxString::FromUTF8(user->nick);
            m_user_list->SetItemText(i, nick);

            /* Update icon */
            int iconIdx = GetUserIconIndex(user);
            wxListItem li;
            li.SetId(i);
            li.SetImage(iconIdx);
            li.SetMask(wxLIST_MASK_IMAGE);
            m_user_list->SetItem(li);

            if (user->op) {
                m_user_list->SetItemTextColour(i, wxColour(200, 0, 0));
            } else if (user->hop) {
                m_user_list->SetItemTextColour(i, wxColour(128, 0, 128));
            } else if (user->voice) {
                m_user_list->SetItemTextColour(i, wxColour(0, 128, 0));
            } else {
                m_user_list->SetItemTextColour(i, *wxBLACK);
            }
            break;
        }
    }
}

void MainWindow::UserlistNumbers(struct session *sess)
{
    if (sess != m_current_session) return;

    wxString info;
    if (sess->type == SESS_CHANNEL) {
        info = wxString::Format(wxT("%d ops, %d total"),
                                 sess->ops, sess->total);
    }
    m_usercount_label->SetLabel(info);
}

void MainWindow::UserlistClear(struct session *sess)
{
    if (sess == m_current_session) {
        m_user_list->DeleteAllItems();
        m_usercount_label->SetLabel(wxEmptyString);
    }
}

void MainWindow::UserlistSetSelected(struct session *sess)
{
    if (sess != m_current_session) return;
    /* Sync the selected state from each User struct to the listctrl */
    for (long i = 0; i < m_user_list->GetItemCount(); i++) {
        struct User *user = (struct User *)m_user_list->GetItemData(i);
        if (user) {
            m_user_list->SetItemState(i,
                user->selected ? wxLIST_STATE_SELECTED : 0,
                wxLIST_STATE_SELECTED);
        }
    }
}

void MainWindow::UserlistSelect(struct session *sess, const char *word[],
                                 int do_clear, int scroll_to)
{
    if (sess != m_current_session) return;

    if (do_clear) {
        /* Deselect all */
        long item = -1;
        while ((item = m_user_list->GetNextItem(item, wxLIST_NEXT_ALL,
                                                 wxLIST_STATE_SELECTED)) != -1) {
            m_user_list->SetItemState(item, 0, wxLIST_STATE_SELECTED);
        }
    }

    /* Select matching nicks */
    for (int w = 0; word[w] && word[w][0]; w++) {
        for (long i = 0; i < m_user_list->GetItemCount(); i++) {
            struct User *user = (struct User *)m_user_list->GetItemData(i);
            if (user && g_ascii_strcasecmp(user->nick, word[w]) == 0) {
                m_user_list->SetItemState(i, wxLIST_STATE_SELECTED,
                                           wxLIST_STATE_SELECTED);
                if (scroll_to)
                    m_user_list->EnsureVisible(i);
                break;
            }
        }
    }
}

/* ===== Nick Display ===== */

void MainWindow::SetNick(struct server *serv, const wxString &nick)
{
    if (m_current_session && m_current_session->server == serv) {
        m_nick_button->SetLabel(nick);
    }
}

void MainWindow::OnNickButtonClick(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server)
        return;

    wxString currentNick = wxString::FromUTF8(m_current_session->server->nick);
    wxTextEntryDialog dlg(this, wxT("Enter new nickname:"),
                          wxT("Change Nickname"), currentNick);
    if (dlg.ShowModal() == wxID_OK) {
        wxString newNick = dlg.GetValue().Trim().Trim(false);
        if (!newNick.IsEmpty() && newNick != currentNick) {
            char cmd_buf[512];
            snprintf(cmd_buf, sizeof(cmd_buf), "nick %s",
                     (const char *)newNick.ToUTF8());
            handle_command(m_current_session, cmd_buf, FALSE);
        }
    }
}

/* ===== Progress Bar ===== */

void MainWindow::SetProgressBar(struct session *sess, bool active)
{
    if (active) {
        GetStatusBar()->SetStatusText(wxT("Connecting..."), 0);
    } else {
        GetStatusBar()->SetStatusText(wxT("Connected"), 0);
    }
}

/* ===== Lag Meter ===== */

void MainWindow::SetLag(struct server *serv, long lag)
{
    if (!m_current_session || m_current_session->server != serv) return;

    double lag_seconds = lag / 1000.0;
    wxString lagText = wxString::Format(wxT("Lag: %.1fs"), lag_seconds);

    /* Update gauge widget if present (range 0-100, max 2s mapped to 100) */
    if (m_lag_meter) {
        int val = (int)(lag_seconds * 50.0);  /* 2s = 100 */
        if (val > 100) val = 100;
        if (val < 0) val = 0;
        m_lag_meter->SetValue(val);
    }
    if (m_lag_label) {
        m_lag_label->SetLabel(lagText);
    }

    /* Fall back to plain text if no widgets */
    if (!m_lag_meter && !m_lag_label) {
        GetStatusBar()->SetStatusText(lagText, 1);
    }
}

void MainWindow::SetThrottle(struct server *serv)
{
    if (!m_current_session || m_current_session->server != serv) return;

    wxString queueText = wxString::Format(wxT("Q: %d"),
                                           serv->sendq_len);

    /* Update throttle gauge/label if present */
    if (m_throttle_meter) {
        int val = serv->sendq_len;
        if (val > 100) val = 100;
        m_throttle_meter->SetValue(val);
    }
    if (m_throttle_label) {
        m_throttle_label->SetLabel(queueText);
    }

    /* Fall back to plain text if no widgets */
    if (!m_throttle_meter && !m_throttle_label) {
        GetStatusBar()->SetStatusText(queueText, 2);
    }
}

void MainWindow::SetAway(struct server *serv)
{
    if (!m_current_session || m_current_session->server != serv) return;

    if (serv->is_away) {
        GetStatusBar()->SetStatusText(wxT("Away"), 0);
    } else {
        GetStatusBar()->SetStatusText(wxT("Connected"), 0);
    }

    /* Sync the Away check menu item */
    wxMenuBar *mb = GetMenuBar();
    if (mb) {
        wxMenuItem *item = mb->FindItem(ID_AWAY);
        if (item) item->Check(serv->is_away);
    }
}

/* ===== Meter Management ===== */

void MainWindow::CreateMeters()
{
    wxStatusBar *sb = GetStatusBar();
    if (!sb) return;

    m_lag_meter = nullptr;
    m_lag_label = nullptr;
    m_throttle_meter = nullptr;
    m_throttle_label = nullptr;

    int lag_pref = prefs.pchat_gui_lagometer;      /* bitmask: bit0=graph, bit1=text */
    int thr_pref = prefs.pchat_gui_throttlemeter;   /* bitmask: bit0=graph, bit1=text */

    /* Lag widgets in status bar field 1 */
    if (lag_pref & 1) {
        m_lag_meter = new wxGauge(sb, wxID_ANY, 100,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxGA_HORIZONTAL | wxGA_SMOOTH);
        m_lag_meter->SetValue(0);
    }
    if (lag_pref & 2) {
        m_lag_label = new wxStaticText(sb, wxID_ANY, wxT("Lag: 0.0s"));
    }

    /* Throttle widgets in status bar field 2 */
    if (thr_pref & 1) {
        m_throttle_meter = new wxGauge(sb, wxID_ANY, 100,
                                       wxDefaultPosition, wxDefaultSize,
                                       wxGA_HORIZONTAL | wxGA_SMOOTH);
        m_throttle_meter->SetValue(0);
    }
    if (thr_pref & 2) {
        m_throttle_label = new wxStaticText(sb, wxID_ANY, wxT("Q: 0"));
    }

    /* Position the widgets inside the status bar fields */
    PositionMeterWidgets();

    /* If no gauges/labels, fall back to text-only status fields */
    if (!m_lag_meter && !m_lag_label) {
        sb->SetStatusText(wxT("Lag: 0.0s"), 1);
    } else {
        sb->SetStatusText(wxEmptyString, 1);
    }
    if (!m_throttle_meter && !m_throttle_label) {
        sb->SetStatusText(wxT("Q: 0"), 2);
    } else {
        sb->SetStatusText(wxEmptyString, 2);
    }
}

void MainWindow::PositionMeterWidgets()
{
    wxStatusBar *sb = GetStatusBar();
    if (!sb) return;

    /* Position lag widgets inside field 1 */
    wxRect lagRect;
    sb->GetFieldRect(1, lagRect);

    int lagX = lagRect.x + 2;
    if (m_lag_meter) {
        int gaugeH = lagRect.height - 6;
        if (gaugeH < 6) gaugeH = 6;
        int gaugeW = (m_lag_label) ? lagRect.width / 2 - 4 : lagRect.width - 4;
        m_lag_meter->SetSize(lagX, lagRect.y + 3, gaugeW, gaugeH);
        lagX += gaugeW + 4;
    }
    if (m_lag_label) {
        int labelW = lagRect.width - (lagX - lagRect.x) - 2;
        m_lag_label->SetSize(lagX, lagRect.y + 2, labelW, lagRect.height - 4);
    }

    /* Position throttle widgets inside field 2 */
    wxRect thrRect;
    sb->GetFieldRect(2, thrRect);

    int thrX = thrRect.x + 2;
    if (m_throttle_meter) {
        int gaugeH = thrRect.height - 6;
        if (gaugeH < 6) gaugeH = 6;
        int gaugeW = (m_throttle_label) ? thrRect.width / 2 - 4 : thrRect.width - 4;
        m_throttle_meter->SetSize(thrX, thrRect.y + 3, gaugeW, gaugeH);
        thrX += gaugeW + 4;
    }
    if (m_throttle_label) {
        int labelW = thrRect.width - (thrX - thrRect.x) - 2;
        m_throttle_label->SetSize(thrX, thrRect.y + 2, labelW, thrRect.height - 4);
    }
}

void MainWindow::UpdateMeters()
{
    wxStatusBar *sb = GetStatusBar();
    if (!sb) return;

    /* Destroy existing meter widgets */
    if (m_lag_meter) { m_lag_meter->Destroy(); m_lag_meter = nullptr; }
    if (m_lag_label) { m_lag_label->Destroy(); m_lag_label = nullptr; }
    if (m_throttle_meter) { m_throttle_meter->Destroy(); m_throttle_meter = nullptr; }
    if (m_throttle_label) { m_throttle_label->Destroy(); m_throttle_label = nullptr; }

    /* Recreate based on current prefs */
    CreateMeters();
}

/* ===== Userlist Buttons ===== */

/* Execute a userlist button command with %s/%a nick substitution (matches HexChat) */
void MainWindow::ExecuteUserlistButtonCmd(const char *cmd)
{
    if (!m_current_session) return;
    session *sess = m_current_session;

    bool using_allnicks = (strstr(cmd, "%a") != nullptr);

    /* Collect selected nicks from the userlist */
    std::vector<std::string> nicks;
    long item = -1;
    while ((item = m_user_list->GetNextItem(item, wxLIST_NEXT_ALL,
                                             wxLIST_STATE_SELECTED)) != -1) {
        struct User *user = (struct User *)m_user_list->GetItemData(item);
        if (user && user->nick[0])
            nicks.push_back(user->nick);
    }

    /* If dialog session, use the channel name as the nick */
    if (sess->type == SESS_DIALOG && nicks.empty()) {
        if (sess->channel[0])
            nicks.push_back(sess->channel);
    }

    /* Build allnicks string */
    std::string allnicks;
    for (size_t i = 0; i < nicks.size(); i++) {
        if (i > 0) allnicks += ' ';
        allnicks += nicks[i];
    }

    /* Determine host, account, network for auto_insert */
    const char *nick = nicks.empty() ? "" : nicks[0].c_str();
    const char *host = "Host unknown";
    const char *account = "Account unknown";
    struct User *user = nicks.empty() ? nullptr : userlist_find(sess, (char *)nick);
    if (user) {
        if (user->hostname)
            host = strchr(user->hostname, '@') + 1;
        if (user->account)
            account = user->account;
    }

    if (nicks.empty()) {
        /* No selection — execute once with empty nick */
        int len = strlen(cmd) + 512;
        char *buf = (char *)g_malloc(len);
        auto_insert(buf, len, (unsigned char *)cmd, nullptr, nullptr,
                    (char *)"", sess->channel, (char *)"",
                    server_get_network(sess->server, TRUE),
                    (char *)host, sess->server->nick,
                    (char *)nick, (char *)account);
        if (*buf == '!')
            pchat_exec(buf + 1);
        else
            handle_command(sess, buf, TRUE);
        g_free(buf);
        return;
    }

    if (using_allnicks) {
        /* Execute once with all nicks */
        int len = strlen(cmd) + allnicks.size() + strlen(nick) + 512;
        char *buf = (char *)g_malloc(len);
        auto_insert(buf, len, (unsigned char *)cmd, nullptr, nullptr,
                    (char *)allnicks.c_str(), sess->channel, (char *)"",
                    server_get_network(sess->server, TRUE),
                    (char *)host, sess->server->nick,
                    (char *)nick, (char *)account);
        if (*buf == '!')
            pchat_exec(buf + 1);
        else
            handle_command(sess, buf, TRUE);
        g_free(buf);
    } else {
        /* Execute once per selected nick */
        for (size_t i = 0; i < nicks.size(); i++) {
            const char *n = nicks[i].c_str();
            struct User *u = userlist_find(sess, (char *)n);
            const char *h = "Host unknown";
            const char *a = "Account unknown";
            if (u) {
                if (u->hostname) h = strchr(u->hostname, '@') + 1;
                if (u->account) a = u->account;
            }
            int len = strlen(cmd) + strlen(n) + 512;
            char *buf = (char *)g_malloc(len);
            auto_insert(buf, len, (unsigned char *)cmd, nullptr, nullptr,
                        (char *)"", sess->channel, (char *)"",
                        server_get_network(sess->server, TRUE),
                        (char *)h, sess->server->nick,
                        (char *)n, (char *)a);
            if (*buf == '!')
                pchat_exec(buf + 1);
            else
                handle_command(sess, buf, TRUE);
            g_free(buf);
        }
    }
}

void MainWindow::CreateUserlistButtons()
{
    if (!m_ul_buttons_panel || !m_ul_buttons_sizer) return;

    /* Iterate button_list GSList of struct popup — 2-column grid layout */
    for (GSList *sl = button_list; sl; sl = sl->next) {
        auto *pop = static_cast<struct popup *>(sl->data);
        if (!pop->cmd || !pop->cmd[0]) continue;

        wxString label = wxString::FromUTF8(pop->name);
        std::string cmdStr(pop->cmd);

        wxButton *btn = new wxButton(m_ul_buttons_panel, wxID_ANY, label,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxBU_EXACTFIT);
        btn->SetToolTip(wxString::FromUTF8(pop->cmd));

        /* Bind click — proper nick substitution like HexChat */
        btn->Bind(wxEVT_BUTTON, [this, cmdStr](wxCommandEvent &) {
            ExecuteUserlistButtonCmd(cmdStr.c_str());
        });

        m_ul_buttons_sizer->Add(btn, 0, wxEXPAND);
    }
}

void MainWindow::UpdateUserlistButtons()
{
    if (!m_ul_buttons_panel) return;

    /* Clear old sizer and contents, create fresh grid sizer */
    if (m_ul_buttons_sizer) {
        m_ul_buttons_sizer->Clear(true);
        m_ul_buttons_panel->SetSizer(nullptr, true);
    }
    m_ul_buttons_sizer = new wxFlexGridSizer(0, 2, 2, 2);
    static_cast<wxFlexGridSizer*>(m_ul_buttons_sizer)->AddGrowableCol(0, 1);
    static_cast<wxFlexGridSizer*>(m_ul_buttons_sizer)->AddGrowableCol(1, 1);
    m_ul_buttons_panel->SetSizer(m_ul_buttons_sizer);
    CreateUserlistButtons();

    m_ul_buttons_panel->Show(prefs.pchat_gui_ulist_buttons);
    m_ul_buttons_panel->Layout();
    m_userlist_panel->Layout();
}

/* ===== Dialog Buttons ===== */

void MainWindow::CreateDialogButtons()
{
    if (!m_dlg_buttons_panel || !m_dlg_buttons_sizer) return;

    /* Iterate dlgbutton_list GSList of struct popup */
    for (GSList *sl = dlgbutton_list; sl; sl = sl->next) {
        auto *pop = static_cast<struct popup *>(sl->data);
        if (!pop->name || !pop->name[0]) continue;

        wxString label = wxString::FromUTF8(pop->name);
        wxString cmd = wxString::FromUTF8(pop->cmd);

        wxButton *btn = new wxButton(m_dlg_buttons_panel, wxID_ANY, label,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxBU_EXACTFIT);
        btn->SetToolTip(cmd);

        /* Bind click to execute the command */
        btn->Bind(wxEVT_BUTTON, [this, cmd](wxCommandEvent &) {
            if (!m_current_session) return;
            wxCharBuffer utf8 = cmd.ToUTF8();
            handle_command(m_current_session, (char *)utf8.data(), FALSE);
        });

        m_dlg_buttons_sizer->Add(btn, 0, wxALL, 1);
    }
}

void MainWindow::UpdateDialogButtons()
{
    if (!m_dlg_buttons_panel || !m_dlg_buttons_sizer) return;

    /* Clear and recreate */
    m_dlg_buttons_sizer->Clear(true);
    CreateDialogButtons();

    m_dlg_buttons_panel->Show(dlgbutton_list != nullptr);
    m_dlg_buttons_panel->Layout();
    if (m_main_panel) m_main_panel->Layout();
}

/* ===== Menu State Sync ===== */

void MainWindow::SyncMenuCheckItems()
{
    wxMenuBar *mb = GetMenuBar();
    if (!mb) return;

    /* Topic bar */
    wxMenuItem *item = mb->FindItem(ID_TOGGLE_TOPICBAR);
    if (item) item->Check(prefs.pchat_gui_topicbar);

    /* User list */
    item = mb->FindItem(ID_TOGGLE_USERLIST);
    if (item) item->Check(!prefs.pchat_gui_ulist_hide);

    /* UL buttons */
    item = mb->FindItem(ID_TOGGLE_ULBUTTONS);
    if (item) item->Check(prefs.pchat_gui_ulist_buttons);

    /* Mode buttons */
    item = mb->FindItem(ID_TOGGLE_MODEBUTTONS);
    if (item) item->Check(prefs.pchat_gui_mode_buttons);

    /* Channel switcher layout */
    item = mb->FindItem(ID_LAYOUT_TABS);
    if (item) item->Check(prefs.pchat_gui_tab_layout == 0);
    item = mb->FindItem(ID_LAYOUT_TREE);
    if (item) item->Check(prefs.pchat_gui_tab_layout == 2);

    /* Network metres */
    int metre_mode = 0;
    if (prefs.pchat_gui_lagometer == 1) metre_mode = 1;
    else if (prefs.pchat_gui_lagometer == 2) metre_mode = 2;
    else if (prefs.pchat_gui_lagometer == 3) metre_mode = 3;

    item = mb->FindItem(ID_METRES_OFF);
    if (item) item->Check(metre_mode == 0);
    item = mb->FindItem(ID_METRES_GRAPH);
    if (item) item->Check(metre_mode == 1);
    item = mb->FindItem(ID_METRES_TEXT);
    if (item) item->Check(metre_mode == 2);
    item = mb->FindItem(ID_METRES_BOTH);
    if (item) item->Check(metre_mode == 3);
}

/* ===== Plugin Menu Items ===== */

void MainWindow::AddPluginMenuItem(menu_entry *me)
{
    if (!me || !me->label) return;

    wxMenuBar *mb = GetMenuBar();
    if (!mb) return;

    /* Determine which menu to add to.
       If me->is_main, add to the main menu bar.
       The path field determines the submenu location. */
    wxString label = wxString::FromUTF8(me->label);

    /* Strip pango markup if present */
    if (me->markup) {
        label.Replace(wxT("<b>"), wxEmptyString);
        label.Replace(wxT("</b>"), wxEmptyString);
        label.Replace(wxT("<i>"), wxEmptyString);
        label.Replace(wxT("</i>"), wxEmptyString);
    }

    /* Allocate a wx menu ID */
    int wxid = m_next_plugin_menu_id++;

    /* Find or create the target menu.
       For simplicity, plugin menu items go into a "Plugins" submenu
       under the Window menu if they're main menu items, or we handle
       the path to find the right menu. */
    wxMenu *targetMenu = nullptr;

    if (me->is_main && me->path) {
        /* Try to find the menu by path. The root_offset skips the "$MENUBAR/" prefix. */
        wxString path = wxString::FromUTF8(me->path + me->root_offset);

        /* Walk the path components to find or create submenus */
        wxMenu *curMenu = nullptr;
        wxString remaining = path;

        /* First component is a top-level menu name */
        wxString topLevel = remaining.BeforeFirst('/');
        remaining = remaining.AfterFirst('/');

        /* Search menu bar for matching top-level menu */
        for (size_t i = 0; i < mb->GetMenuCount(); i++) {
            if (mb->GetMenuLabel(i).IsSameAs(topLevel, false)) {
                curMenu = mb->GetMenu(i);
                break;
            }
        }

        /* If not found, try known menu names */
        if (!curMenu) {
            if (topLevel.IsSameAs(wxT("Settings"), false) ||
                topLevel.IsSameAs(wxT("Preferences"), false)) {
                int settingsIdx = mb->FindMenu(wxT("Settings"));
                if (settingsIdx != wxNOT_FOUND) curMenu = mb->GetMenu(settingsIdx);
            } else if (topLevel.IsSameAs(wxT("Window"), false)) {
                int windowIdx = mb->FindMenu(wxT("Window"));
                if (windowIdx != wxNOT_FOUND) curMenu = mb->GetMenu(windowIdx);
            }
        }

        /* Walk remaining path components to find/create submenus */
        while (curMenu && !remaining.IsEmpty()) {
            wxString part = remaining.BeforeFirst('/');
            remaining = remaining.AfterFirst('/');

            /* Find existing submenu with this label */
            wxMenu *subMenu = nullptr;
            size_t itemCount = curMenu->GetMenuItemCount();
            for (size_t i = 0; i < itemCount; i++) {
                wxMenuItem *mi = curMenu->FindItemByPosition(i);
                if (mi && mi->GetSubMenu() &&
                    mi->GetItemLabelText().IsSameAs(part, false)) {
                    subMenu = mi->GetSubMenu();
                    break;
                }
            }

            if (!subMenu && !part.IsEmpty()) {
                /* Create submenu */
                subMenu = new wxMenu();
                curMenu->AppendSubMenu(subMenu, part);
            }
            curMenu = subMenu;
        }

        targetMenu = curMenu;
    }

    /* Fallback: put in Window menu */
    if (!targetMenu) {
        int windowIdx = mb->FindMenu(wxT("Window"));
        if (windowIdx != wxNOT_FOUND)
            targetMenu = mb->GetMenu(windowIdx);
    }

    if (!targetMenu) return;

    /* Add the item */
    if (!me->cmd && me->label) {
        /* Submenu — label only, no command means it's a submenu header */
        wxMenu *subMenu = new wxMenu();
        targetMenu->AppendSubMenu(subMenu, label);
    } else if (!me->label) {
        /* Separator */
        targetMenu->AppendSeparator();
    } else if (me->ucmd) {
        /* Toggle item */
        wxMenuItem *item = targetMenu->AppendCheckItem(wxid, label);
        item->Check(me->state != 0);
        item->Enable(me->enable != 0);
        Bind(wxEVT_MENU, &MainWindow::OnPluginMenuCmd, this, wxid);
    } else {
        /* Regular item */
        wxMenuItem *item = targetMenu->Append(wxid, label);
        item->Enable(me->enable != 0);
        Bind(wxEVT_MENU, &MainWindow::OnPluginMenuCmd, this, wxid);
    }

    /* Track the association */
    m_plugin_menu_items.push_back({me, wxid});
}

void MainWindow::RemovePluginMenuItem(menu_entry *me)
{
    if (!me) return;

    wxMenuBar *mb = GetMenuBar();
    if (!mb) return;

    for (auto it = m_plugin_menu_items.begin(); it != m_plugin_menu_items.end(); ++it) {
        if (it->me == me) {
            wxMenuItem *item = mb->FindItem(it->wxid);
            if (item) {
                wxMenu *parentMenu = item->GetMenu();
                if (parentMenu)
                    parentMenu->Destroy(item);
            }
            Unbind(wxEVT_MENU, &MainWindow::OnPluginMenuCmd, this, it->wxid);
            m_plugin_menu_items.erase(it);
            break;
        }
    }
}

void MainWindow::UpdatePluginMenuItem(menu_entry *me)
{
    if (!me) return;

    wxMenuBar *mb = GetMenuBar();
    if (!mb) return;

    for (auto &pmi : m_plugin_menu_items) {
        if (pmi.me == me) {
            wxMenuItem *item = mb->FindItem(pmi.wxid);
            if (item) {
                item->Enable(me->enable != 0);
                if (item->IsCheckable())
                    item->Check(me->state != 0);
            }
            break;
        }
    }
}

void MainWindow::OnPluginMenuCmd(wxCommandEvent &event)
{
    int wxid = event.GetId();
    for (auto &pmi : m_plugin_menu_items) {
        if (pmi.wxid == wxid && pmi.me) {
            menu_entry *me = pmi.me;
            const char *cmd = nullptr;

            /* For toggle items, use ucmd when unchecked */
            if (me->ucmd && event.IsChecked() == false)
                cmd = me->ucmd;
            else
                cmd = me->cmd;

            if (cmd && m_current_session) {
                handle_command(m_current_session, (char *)cmd, FALSE);
            }
            return;
        }
    }
}

/* ===== Lastlog ===== */

void MainWindow::DoLastlog(struct session *sess, struct session *lastlog_sess,
                            const char *sstr, int flags)
{
    /* The lastlog feature searches the scrollback buffer.
       For now, display a message. Full text buffer search would require
       wx text buffer introspection. */
    if (!lastlog_sess || !sstr) return;

    wxString search = wxString::FromUTF8(sstr);
    wxString chatContent = m_chat_text->GetValue();

    wxString results;
    wxStringTokenizer tokenizer(chatContent, wxT("\n"));
    while (tokenizer.HasMoreTokens()) {
        wxString line = tokenizer.GetNextToken();
        if (line.Lower().Contains(search.Lower())) {
            results += line + wxT("\n");
        }
    }

    if (!results.IsEmpty()) {
        PrintText(lastlog_sess, results, 0);
    } else {
        PrintText(lastlog_sess,
                  wxString::Format(wxT("No matches for \"%s\""), search), 0);
    }
}

/* ===== GUI Info for scripting API ===== */

int MainWindow::GetGuiInfo(struct session *sess, int info_type)
{
    switch (info_type) {
    case 0: /* window status: 1=focused, 2=normal, 3=hidden */
        if (!IsShown()) return 3;
        if (IsActive()) return 1;
        return 2;
    default:
        return 0;
    }
}

void *MainWindow::GetGuiInfoPtr(struct session *sess, int info_type)
{
    switch (info_type) {
    case 0: /* GtkWindow ptr - not applicable for wx */
        return (void *)this;
    default:
        return nullptr;
    }
}

/* ===== Input Box ===== */

char *MainWindow::GetInputBoxContents(struct session *sess)
{
    wxString text = m_input_box->GetText();
    strncpy(s_input_buffer, text.utf8_str().data(), sizeof(s_input_buffer) - 1);
    s_input_buffer[sizeof(s_input_buffer) - 1] = '\0';
    return s_input_buffer;
}

int MainWindow::GetInputBoxCursor(struct session *sess)
{
    return (int)m_input_box->GetCurrentPos();
}

void MainWindow::SetInputBoxContents(struct session *sess,
                                      const wxString &text)
{
    if (sess == m_current_session) {
        m_input_box->SetText(text);
    }
}

void MainWindow::SetInputBoxCursor(struct session *sess, int delta, int pos)
{
    if (sess != m_current_session) return;

    if (delta) {
        long current = m_input_box->GetCurrentPos();
        m_input_box->GotoPos(current + pos);
    } else {
        m_input_box->GotoPos(pos);
    }
}

void MainWindow::InsertInputText(const wxString &text)
{
    m_input_box->ReplaceSelection(text);
    m_input_box->SetFocus();
}

/* ===== Server Events ===== */

void MainWindow::OnServerEvent(struct server *serv, int type, int arg)
{
    switch (type) {
    case FE_SE_CONNECT:
        GetStatusBar()->SetStatusText(wxT("Connecting..."), 0);
        break;
    case FE_SE_LOGGEDIN:
        GetStatusBar()->SetStatusText(wxT("Connected"), 0);
        if (serv->front_session) {
            SetNick(serv, wxString::FromUTF8(serv->nick));
        }
        /* If no auto-join channels, open join dialog */
        if (arg == 0 && prefs.pchat_gui_join_dialog) {
            if (!serv->gui->joind_window) {
                JoinDialog *jd = new JoinDialog(this, serv);
                serv->gui->joind_window = jd;
                jd->ShowModal();
                jd->Destroy();
                if (is_server(serv))
                    serv->gui->joind_window = nullptr;
            }
        }
        break;
    case FE_SE_DISCONNECT:
        GetStatusBar()->SetStatusText(wxT("Disconnected"), 0);
        /* Close join dialog if open */
        if (serv->gui && serv->gui->joind_window) {
            JoinDialog *jd = (JoinDialog *)serv->gui->joind_window;
            jd->EndModal(wxID_CANCEL);
            serv->gui->joind_window = nullptr;
        }
        break;
    case FE_SE_RECONDELAY:
        GetStatusBar()->SetStatusText(
            wxString::Format(wxT("Reconnecting in %d seconds..."), arg), 0);
        break;
    case FE_SE_CONNECTING:
        GetStatusBar()->SetStatusText(wxT("Connecting..."), 0);
        break;
    }
}

/* ===== Channel List ===== */

void MainWindow::ShowChannelList(struct server *serv, const char *filter,
                                  int do_refresh)
{
    if (!serv || !serv->gui) return;

    if (serv->gui->chanlist_window) {
        ChannelListDialog *dlg = (ChannelListDialog *)serv->gui->chanlist_window;
        dlg->Raise();
        dlg->SetFocus();
        return;
    }

    ChannelListDialog *dlg = new ChannelListDialog(this, serv, do_refresh != 0);
    serv->gui->chanlist_window = dlg;
    dlg->Show();
}

void MainWindow::AddChanListRow(struct server *serv, const char *chan,
                                 const char *users, const char *topic)
{
    if (!serv || !serv->gui || !serv->gui->chanlist_window) return;
    ChannelListDialog *dlg = (ChannelListDialog *)serv->gui->chanlist_window;
    dlg->AddRow(chan, users, topic);
}

void MainWindow::ChanListEnd(struct server *serv)
{
    if (!serv || !serv->gui || !serv->gui->chanlist_window) return;
    ChannelListDialog *dlg = (ChannelListDialog *)serv->gui->chanlist_window;
    dlg->ListEnd();
}

/* ===== Ban List ===== */

bool MainWindow::AddBanListEntry(struct session *sess, const char *mask,
                                  const char *who, const char *when,
                                  int rplcode)
{
    if (!sess || !sess->res || !sess->res->banlist) return false;
    BanListDialog *dlg = (BanListDialog *)sess->res->banlist;
    return dlg->AddEntry(mask, who, when, rplcode);
}

bool MainWindow::BanListEnd(struct session *sess, int rplcode)
{
    if (!sess || !sess->res || !sess->res->banlist) return false;
    BanListDialog *dlg = (BanListDialog *)sess->res->banlist;
    return dlg->ListEnd(rplcode);
}

/* ===== Dialogs ===== */

void MainWindow::ShowNetworkList()
{
    if (!m_network_list_dlg) {
        m_network_list_dlg = new NetworkListDialog(this);
    }
    m_network_list_dlg->Show();
    m_network_list_dlg->Raise();
}

/* ===== Right-Click Context Menus ===== */

wxMenu *MainWindow::BuildUserlistPopup()
{
    wxMenu *menu = new wxMenu();

    /* Get the selected nick for the submenu title */
    long sel = m_user_list->GetNextItem(-1, wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED);
    if (sel >= 0) {
        struct User *user = (struct User *)m_user_list->GetItemData(sel);
        if (user) {
            wxString nick = wxString::FromUTF8(user->nick);
            wxMenu *nickSub = new wxMenu();
            /* Nick info items populated on show */
            if (user->hostname) {
                nickSub->Append(wxID_ANY, wxString::FromUTF8(user->hostname))
                    ->Enable(false);
            }
            if (user->realname && user->realname[0]) {
                nickSub->Append(wxID_ANY, wxString::FromUTF8(user->realname))
                    ->Enable(false);
            }
            if (user->servername && user->servername[0]) {
                nickSub->Append(wxID_ANY, wxString::FromUTF8(user->servername))
                    ->Enable(false);
            }
            menu->AppendSubMenu(nickSub, nick);
            menu->AppendSeparator();
        }
    }

    /* Top-level items matching GTK3 popup_list */
    menu->Append(ID_USERLIST_POPUP_BASE + 0, wxT("Open Dialog Window"));
    menu->Append(ID_USERLIST_POPUP_BASE + 1, wxT("Send a File..."));
    menu->Append(ID_USERLIST_POPUP_BASE + 2, wxT("User Info (WhoIs)"));
    menu->Append(ID_USERLIST_POPUP_BASE + 3, wxT("Add to Friends List..."));
    menu->Append(ID_USERLIST_POPUP_BASE + 4, wxT("Ignore"));

    /* Operator Actions submenu */
    wxMenu *opMenu = new wxMenu();
    opMenu->Append(ID_USERLIST_POPUP_BASE + 5, wxT("Give Ops"));
    opMenu->Append(ID_USERLIST_POPUP_BASE + 6, wxT("Take Ops"));
    opMenu->AppendSeparator();
    opMenu->Append(ID_USERLIST_POPUP_BASE + 7, wxT("Give Voice"));
    opMenu->Append(ID_USERLIST_POPUP_BASE + 8, wxT("Take Voice"));
    opMenu->AppendSeparator();

    /* Kick/Ban sub-submenu */
    wxMenu *kickBanMenu = new wxMenu();
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 9,  wxT("Kick"));
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 10, wxT("Ban"));
    kickBanMenu->AppendSeparator();
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 11, wxT("Ban *!*@*.host"));
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 12, wxT("Ban *!*@domain"));
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 13, wxT("Ban *!*user@*.host"));
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 14, wxT("Ban *!*user@domain"));
    kickBanMenu->AppendSeparator();
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 15, wxT("KickBan *!*@*.host"));
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 16, wxT("KickBan *!*@domain"));
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 17, wxT("KickBan *!*user@*.host"));
    kickBanMenu->Append(ID_USERLIST_POPUP_BASE + 18, wxT("KickBan *!*user@domain"));

    opMenu->AppendSubMenu(kickBanMenu, wxT("Kick/Ban"));
    menu->AppendSubMenu(opMenu, wxT("Operator Actions"));

    return menu;
}

void MainWindow::OnUserlistPopupCmd(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server) return;

    /* Get selected user */
    long sel = m_user_list->GetNextItem(-1, wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED);
    if (sel < 0) return;

    struct User *user = (struct User *)m_user_list->GetItemData(sel);
    if (!user) return;

    int cmd_id = event.GetId() - ID_USERLIST_POPUP_BASE;
    char cmd_buf[512];

    switch (cmd_id) {
    case 0: /* Open Dialog Window */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "QUERY %s", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 1: /* Send a File */
        fe_dcc_send_filereq(m_current_session, user->nick, 0, 0);
        break;
    case 2: /* User Info (WhoIs) */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "WHOIS %s %s",
                   user->nick, user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 3: /* Add to Friends List */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "NOTIFY -n ASK %s", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 4: /* Ignore */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "IGNORE %s!*@* ALL", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 5: /* Give Ops */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "OP %%a");
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 6: /* Take Ops */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "DEOP %%a");
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 7: /* Give Voice */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "VOICE %%a");
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 8: /* Take Voice */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "DEVOICE %%a");
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 9: /* Kick */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "KICK %s", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 10: /* Ban */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "BAN %s", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 11: /* Ban *!*@*.host */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "BAN %s 0", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 12: /* Ban *!*@domain */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "BAN %s 1", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 13: /* Ban *!*user@*.host */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "BAN %s 2", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 14: /* Ban *!*user@domain */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "BAN %s 3", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 15: /* KickBan *!*@*.host */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "KICKBAN %s 0", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 16: /* KickBan *!*@domain */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "KICKBAN %s 1", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 17: /* KickBan *!*user@*.host */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "KICKBAN %s 2", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    case 18: /* KickBan *!*user@domain */
        g_snprintf(cmd_buf, sizeof(cmd_buf), "KICKBAN %s 3", user->nick);
        handle_command(m_current_session, cmd_buf, FALSE);
        break;
    }
}

wxMenu *MainWindow::BuildChatPopup()
{
    wxMenu *menu = new wxMenu();
    menu->Append(wxID_COPY, wxT("&Copy"));
    menu->Append(wxID_SELECTALL, wxT("Select &All"));
    menu->AppendSeparator();
    menu->Append(ID_CLEAR_TEXT, wxT("C&lear Text"));
    menu->AppendSeparator();
    menu->Append(ID_SEARCH_TEXT, wxT("&Search Text...\tCtrl+F"));
    return menu;
}

/* ===== Menu Event Handlers - File ===== */

void MainWindow::OnServerList(wxCommandEvent &event)
{
    ShowNetworkList();
}

void MainWindow::OnNewServerTab(wxCommandEvent &event)
{
    int old = prefs.pchat_gui_tab_chans;
    prefs.pchat_gui_tab_chans = 1;
    new_ircwindow(nullptr, nullptr, SESS_SERVER, 1);
    prefs.pchat_gui_tab_chans = old;
}

void MainWindow::OnNewChannelTab(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server) return;
    int old = prefs.pchat_gui_tab_chans;
    prefs.pchat_gui_tab_chans = 1;
    new_ircwindow(m_current_session->server, nullptr, SESS_CHANNEL, 1);
    prefs.pchat_gui_tab_chans = old;
}

void MainWindow::OnNewServerWindow(wxCommandEvent &event)
{
    int old = prefs.pchat_gui_tab_chans;
    prefs.pchat_gui_tab_chans = 0;
    new_ircwindow(nullptr, nullptr, SESS_SERVER, 1);
    prefs.pchat_gui_tab_chans = old;
}

void MainWindow::OnNewChannelWindow(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server) return;
    int old = prefs.pchat_gui_tab_chans;
    prefs.pchat_gui_tab_chans = 0;
    new_ircwindow(m_current_session->server, nullptr, SESS_CHANNEL, 1);
    prefs.pchat_gui_tab_chans = old;
}

void MainWindow::OnLoadPlugin(wxCommandEvent &event)
{
#ifdef USE_PLUGIN
    wxFileDialog dlg(this, wxT("Load Plugin or Script"),
                     wxEmptyString, wxEmptyString,
                     wxT("All files (*.*)|*.*"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        wxString path = dlg.GetPath();
        char *utf8 = g_strdup(path.utf8_str().data());
        char cmd_buf[1024];
        g_snprintf(cmd_buf, sizeof(cmd_buf), "LOAD \"%s\"", utf8);
        handle_command(m_current_session, cmd_buf, FALSE);
        g_free(utf8);
    }
#endif
}

void MainWindow::OnCloseTab(wxCommandEvent &event)
{
    if (!m_current_session) return;

    /* Close current session/tab */
    char cmd[] = "CLOSE";
    handle_command(m_current_session, cmd, FALSE);
}

void MainWindow::OnQuit(wxCommandEvent &event)
{
    Close(false);
}

/* ===== Menu Event Handlers - View ===== */

void MainWindow::OnToggleMenuBar(wxCommandEvent &event)
{
    prefs.pchat_gui_hide_menu = !prefs.pchat_gui_hide_menu;
    wxMenuBar *mb = GetMenuBar();
    if (mb) {
        if (prefs.pchat_gui_hide_menu) {
            mb->Show(false);
            wxMessageBox(
                wxT("The menu bar is now hidden. You can restore it\n"
                    "with Ctrl+F9 or by right-clicking this window."),
                wxT("PChat"), wxOK | wxICON_INFORMATION, this);
        } else {
            mb->Show(true);
        }
    }
}

void MainWindow::OnToggleTopicBar(wxCommandEvent &event)
{
    prefs.pchat_gui_topicbar = !prefs.pchat_gui_topicbar;
    m_topic_visible = prefs.pchat_gui_topicbar;
    /* Only show topic bar if pref is enabled AND we're in a channel */
    bool showTopic = m_topic_visible &&
                     m_current_session &&
                     m_current_session->type == SESS_CHANNEL;
    m_topic_panel->Show(showTopic);
    m_chat_panel->Layout();
}

void MainWindow::OnToggleUserList(wxCommandEvent &event)
{
    prefs.pchat_gui_ulist_hide = !prefs.pchat_gui_ulist_hide;
    m_userlist_visible = !prefs.pchat_gui_ulist_hide;
    if (m_userlist_visible) {
        m_hsplitter_right->SplitVertically(m_chat_panel, m_userlist_panel, -130);
    } else {
        m_hsplitter_right->Unsplit(m_userlist_panel);
    }
}

void MainWindow::OnToggleULButtons(wxCommandEvent &event)
{
    prefs.pchat_gui_ulist_buttons = !prefs.pchat_gui_ulist_buttons;
    /* Toggle visibility of userlist buttons panel */
    if (m_ul_buttons_panel) {
        /* Lazily populate buttons if not yet created */
        if (prefs.pchat_gui_ulist_buttons &&
            m_ul_buttons_sizer &&
            m_ul_buttons_sizer->GetItemCount() == 0 &&
            button_list != nullptr) {
            CreateUserlistButtons();
        }
        m_ul_buttons_panel->Show(prefs.pchat_gui_ulist_buttons);
        m_userlist_panel->Layout();
    }
}

void MainWindow::OnToggleModeButtons(wxCommandEvent &event)
{
    prefs.pchat_gui_mode_buttons = !prefs.pchat_gui_mode_buttons;
    m_mode_panel->Show(prefs.pchat_gui_mode_buttons &&
                       m_current_session &&
                       m_current_session->type == SESS_CHANNEL);
    m_topic_panel->Layout();
}

void MainWindow::OnLayoutTabs(wxCommandEvent &event)
{
    prefs.pchat_gui_tab_layout = 0;
    /* Hide tree panel - switch to tab-like mode */
    if (m_tree_visible && m_hsplitter_left->IsSplit()) {
        m_hsplitter_left->Unsplit(m_channel_tree);
        m_channel_tree->Hide();
        m_tree_visible = false;
    }
    /* Show tab bar */
    if (m_tab_bar && !m_tab_bar_visible) {
        m_tab_bar->Show(true);
        m_tab_bar_visible = true;
        m_main_panel->Layout();
    }
}

void MainWindow::OnLayoutTree(wxCommandEvent &event)
{
    prefs.pchat_gui_tab_layout = 2;
    /* Hide tab bar */
    if (m_tab_bar && m_tab_bar_visible) {
        m_tab_bar->Show(false);
        m_tab_bar_visible = false;
    }
    /* Show tree panel - switch to tree mode */
    if (!m_tree_visible) {
        m_channel_tree->Show();
        int leftSize = prefs.pchat_gui_pane_left_size > 0 ?
                       prefs.pchat_gui_pane_left_size : 140;
        m_hsplitter_left->SplitVertically(m_channel_tree, m_right_panel,
                                           leftSize);
        m_tree_visible = true;
    }
    m_main_panel->Layout();
}

void MainWindow::OnMetresOff(wxCommandEvent &event)
{
    prefs.pchat_gui_lagometer = 0;
    prefs.pchat_gui_throttlemeter = 0;
    UpdateMeters();
}

void MainWindow::OnMetresGraph(wxCommandEvent &event)
{
    prefs.pchat_gui_lagometer = 1;
    prefs.pchat_gui_throttlemeter = 1;
    UpdateMeters();
}

void MainWindow::OnMetresText(wxCommandEvent &event)
{
    prefs.pchat_gui_lagometer = 2;
    prefs.pchat_gui_throttlemeter = 2;
    UpdateMeters();
}

void MainWindow::OnMetresBoth(wxCommandEvent &event)
{
    prefs.pchat_gui_lagometer = 3;
    prefs.pchat_gui_throttlemeter = 3;
    UpdateMeters();
}

void MainWindow::OnFullscreen(wxCommandEvent &event)
{
    prefs.pchat_gui_win_fullscreen = !IsFullScreen();
    ShowFullScreen(prefs.pchat_gui_win_fullscreen);
}

/* ===== Menu Event Handlers - Server ===== */

void MainWindow::OnDisconnect(wxCommandEvent &event)
{
    if (m_current_session) {
        char cmd[] = "DISCON";
        handle_command(m_current_session, cmd, FALSE);
    }
}

void MainWindow::OnReconnect(wxCommandEvent &event)
{
    if (!m_current_session) return;

    if (m_current_session->server &&
        m_current_session->server->hostname[0]) {
        char cmd[] = "RECONNECT";
        handle_command(m_current_session, cmd, FALSE);
    } else {
        /* No hostname — open server list */
        ShowNetworkList();
    }
}

void MainWindow::OnJoinChannel(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server) return;

    wxTextEntryDialog dlg(this, wxT("Enter channel name:"),
                           wxT("Join Channel"), wxT("#"));
    if (dlg.ShowModal() == wxID_OK) {
        wxString chan = dlg.GetValue();
        if (!chan.IsEmpty()) {
            char cmd_buf[512];
            g_snprintf(cmd_buf, sizeof(cmd_buf), "JOIN %s",
                       (const char *)chan.utf8_str());
            handle_command(m_current_session, cmd_buf, FALSE);
        }
    }
}

void MainWindow::OnChannelList(wxCommandEvent &event)
{
    if (m_current_session && m_current_session->server) {
        char cmd[] = "LIST";
        handle_command(m_current_session, cmd, FALSE);
    }
}

void MainWindow::OnAway(wxCommandEvent &event)
{
    if (!m_current_session) return;

    if (m_current_session->server &&
        m_current_session->server->is_away) {
        char cmd[] = "BACK";
        handle_command(m_current_session, cmd, FALSE);
    } else {
        char cmd[] = "AWAY";
        handle_command(m_current_session, cmd, FALSE);
    }
}

/* ===== Menu Event Handlers - Settings ===== */

void MainWindow::OnPreferences(wxCommandEvent &event)
{
    PreferencesDialog dlg(this);
    dlg.ShowModal();
}

void MainWindow::OnAutoReplace(wxCommandEvent &event)
{
    EditListDialog dlg(this, "Text", "Replace with",
                       &replace_list, "Auto Replace",
                       "replace.conf");
    dlg.ShowModal();
}

void MainWindow::OnCtcpReplies(wxCommandEvent &event)
{
    EditListDialog dlg(this, "CTCP", "Reply",
                       &ctcp_list, "CTCP Replies",
                       "ctcpreply.conf");
    dlg.ShowModal();
}

void MainWindow::OnDialogButtons(wxCommandEvent &event)
{
    EditListDialog dlg(this, "Name", "Command",
                       &dlgbutton_list, "Dialog Buttons",
                       "dlgbuttons.conf");
    dlg.ShowModal();
}

void MainWindow::OnKeyboardShortcuts(wxCommandEvent &event)
{
    KeyboardShortcutsDialog dlg(this);
    dlg.ShowModal();
}

void MainWindow::OnTextEvents(wxCommandEvent &event)
{
    TextEventsDialog dlg(this);
    dlg.ShowModal();
}

void MainWindow::OnUrlHandlers(wxCommandEvent &event)
{
    EditListDialog dlg(this, "Name", "Command",
                       &urlhandler_list,
                       wxString(DISPLAY_NAME ": URL Handlers"),
                       "urlhandlers.conf",
                       "URL Handlers - Special codes:\n\n"
                       "  %s  =  the URL string\n\n"
                       "Putting a ! in front of the command\n"
                       "indicates it should be sent to a\n"
                       "shell instead of " DISPLAY_NAME,
                       false, false);
    dlg.ShowModal();
}

void MainWindow::OnUserCommands(wxCommandEvent &event)
{
    EditListDialog dlg(this, "Name", "Command",
                       &command_list,
                       wxString(DISPLAY_NAME ": User Defined Commands"),
                       "commands.conf",
                       "", false, false);
    dlg.ShowModal();
}

void MainWindow::OnUserlistButtons(wxCommandEvent &event)
{
    EditListDialog dlg(this, "Name", "Command",
                       &button_list,
                       wxString(DISPLAY_NAME ": Userlist buttons"),
                       "buttons.conf",
                       "", false, false);
    dlg.ShowModal();
}

void MainWindow::OnUserlistPopupConf(wxCommandEvent &event)
{
    EditListDialog dlg(this, "Name", "Command",
                       &popup_list,
                       wxString(DISPLAY_NAME ": Userlist Popup menu"),
                       "popup.conf",
                       "Userlist Buttons - Special codes:\n\n"
                       "%a  =  all selected nicks\n"
                       "%c  =  current channel\n"
                       "%e  =  current network name\n"
                       "%h  =  selected nick's hostname\n"
                       "%m  =  machine info\n"
                       "%n  =  your nick\n"
                       "%s  =  selected nick\n"
                       "%t  =  time/date",
                       false, false);
    dlg.ShowModal();
}

/* ===== Menu Event Handlers - Window ===== */

void MainWindow::OnBanList(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server ||
        m_current_session->type != SESS_CHANNEL ||
        m_current_session->channel[0] == 0) {
        wxMessageBox(wxT("You can only open the Ban List window while in a channel tab."),
                     wxT("Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    if (m_current_session->res && m_current_session->res->banlist) {
        BanListDialog *dlg = (BanListDialog *)m_current_session->res->banlist;
        dlg->Raise();
        dlg->SetFocus();
        return;
    }

    BanListDialog *dlg = new BanListDialog(this, m_current_session);
    if (m_current_session->res)
        m_current_session->res->banlist = dlg;
    dlg->Show();
}

void MainWindow::OnCharChart(wxCommandEvent &event)
{
    CharChartDialog *dlg = new CharChartDialog(this);
    dlg->Show();
}

void MainWindow::OnDccChat(wxCommandEvent &event)
{
    if (m_dcc_chat_dlg && m_dcc_chat_dlg->IsShown()) {
        m_dcc_chat_dlg->Raise();
        return;
    }
    m_dcc_chat_dlg = new DccChatDialog(this);
    m_dcc_chat_dlg->Show();
}

void MainWindow::OnDccRecv(wxCommandEvent &event)
{
    if (m_dcc_xfer_dlg && m_dcc_xfer_dlg->IsShown()) {
        m_dcc_xfer_dlg->Raise();
        return;
    }
    m_dcc_xfer_dlg = new DccTransferDialog(this);
    m_dcc_xfer_dlg->Show();
}

void MainWindow::OnFriendsList(wxCommandEvent &event)
{
    if (m_friends_dlg && m_friends_dlg->IsShown()) {
        m_friends_dlg->Raise();
        return;
    }
    m_friends_dlg = new FriendsListDialog(this);
    m_friends_dlg->Show();
}

void MainWindow::OnIgnoreList(wxCommandEvent &event)
{
    if (m_ignore_dlg && m_ignore_dlg->IsShown()) {
        m_ignore_dlg->Raise();
        return;
    }
    m_ignore_dlg = new IgnoreListDialog(this);
    m_ignore_dlg->Show();
}

void MainWindow::OnPlugins(wxCommandEvent &event)
{
    PluginDialog *dlg = PluginDialog::GetInstance();
    if (dlg) {
        dlg->Raise();
        dlg->SetFocus();
    } else {
        dlg = new PluginDialog(this);
        dlg->Show();
    }
}

void MainWindow::OnRawLog(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server) return;

    struct server *serv = m_current_session->server;
    if (!serv->gui->rawlog_window) {
        RawLogDialog *dlg = new RawLogDialog(this, serv);
        serv->gui->rawlog_window = dlg;
        dlg->Show();
    } else {
        RawLogDialog *dlg = (RawLogDialog *)serv->gui->rawlog_window;
        dlg->Raise();
    }
}

void MainWindow::OnUrlGrabber(wxCommandEvent &event)
{
    if (m_url_dlg && m_url_dlg->IsShown()) {
        m_url_dlg->Raise();
        return;
    }
    m_url_dlg = new UrlGrabberDialog(this);
    m_url_dlg->Show();
}

void MainWindow::OnResetMarker(wxCommandEvent &event)
{
    /* Reset the marker line position in the chat buffer.
       We set the marker to the current end of the text, so the next
       time text is appended, everything after this point is "new". */
    if (m_current_session && m_chat_text) {
        long end_pos = m_chat_text->GetLastPosition();
        /* Store the marker position in the session's textview state.
           For now, just scroll to the end to visually "reset" the marker. */
        m_chat_text->ShowPosition(end_pos);
    }
}

void MainWindow::OnCopySelection(wxCommandEvent &event)
{
    wxString sel = m_chat_text->GetStringSelection();
    if (!sel.IsEmpty()) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(sel));
            wxTheClipboard->Close();
        }
    }
}

void MainWindow::OnClearText(wxCommandEvent &event)
{
    if (m_current_session)
        ClearText(m_current_session, 0);
}

void MainWindow::OnSaveText(wxCommandEvent &event)
{
    wxFileDialog dlg(this, wxT("Save Chat Text"),
                     wxEmptyString, wxT("chat.txt"),
                     wxT("Text files (*.txt)|*.txt|All files (*.*)|*.*"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        wxString path = dlg.GetPath();
        wxString content = m_chat_text->GetValue();
        wxFile file(path, wxFile::write);
        if (file.IsOpened()) {
            file.Write(content);
            file.Close();
        }
    }
}

void MainWindow::OnSearchText(wxCommandEvent &event)
{
    if (m_find_dlg) {
        m_find_dlg->Raise();
        return;
    }
    m_find_dlg = new wxFindReplaceDialog(this, &m_find_data,
                                          wxT("Search Text"));
    m_find_dlg->Show();
}

void MainWindow::OnSearchNext(wxCommandEvent &event)
{
    wxString search = m_find_data.GetFindString();
    if (search.IsEmpty()) {
        OnSearchText(event);
        return;
    }

    /* Search forward from current position */
    wxString content = m_chat_text->GetValue();
    long pos = m_chat_text->GetInsertionPoint();
    long found = content.Mid(pos + 1).Find(search);
    if (found != wxNOT_FOUND) {
        long abs_pos = pos + 1 + found;
        m_chat_text->SetInsertionPoint(abs_pos);
        m_chat_text->ShowPosition(abs_pos);
        m_chat_text->SetSelection(abs_pos, abs_pos + search.Len());
    } else {
        /* Wrap around */
        found = content.Find(search);
        if (found != wxNOT_FOUND) {
            m_chat_text->SetInsertionPoint(found);
            m_chat_text->ShowPosition(found);
            m_chat_text->SetSelection(found, found + search.Len());
        }
    }
}

void MainWindow::OnSearchPrev(wxCommandEvent &event)
{
    wxString search = m_find_data.GetFindString();
    if (search.IsEmpty()) {
        OnSearchText(event);
        return;
    }

    /* Search backward from current position */
    wxString content = m_chat_text->GetValue();
    long pos = m_chat_text->GetInsertionPoint();
    if (pos > 0) {
        wxString before = content.Left(pos);
        long found = before.rfind(search.c_str());
        if (found != (long)wxString::npos) {
            m_chat_text->SetInsertionPoint(found);
            m_chat_text->ShowPosition(found);
            m_chat_text->SetSelection(found, found + search.Len());
            return;
        }
    }
    /* Wrap around to end */
    long found = content.rfind(search.c_str());
    if (found != (long)wxString::npos) {
        m_chat_text->SetInsertionPoint(found);
        m_chat_text->ShowPosition(found);
        m_chat_text->SetSelection(found, found + search.Len());
    }
}

/* ===== Menu Event Handlers - Help ===== */

void MainWindow::OnHelpDocs(wxCommandEvent &event)
{
    wxLaunchDefaultBrowser(wxT("https://thatzachbacon.com/docs"));
}

void MainWindow::OnAbout(wxCommandEvent &event)
{
    wxAboutDialogInfo info;
    info.SetName(wxT("PChat"));
    info.SetVersion(wxT(VERSION));
    info.SetDescription(wxT("An IRC client based on HexChat"));
    info.SetCopyright(wxT("Copyright (C) 2025-2026 Zach Bacon"));
    info.SetLicence(
        wxT("This program is free software; you can redistribute it and/or modify\n"
            "it under the terms of the GNU General Public License as published by\n"
            "the Free Software Foundation; version 2.\n\n"
            "This program is distributed in the hope that it will be useful,\n"
            "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
            "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
            "GNU General Public License for more details.\n\n"
            "You should have received a copy of the GNU General Public License\n"
            "along with this program. If not, see <http://www.gnu.org/licenses/>"));
    info.SetWebSite(wxT("https://thatzachbacon.com"));
    info.AddDeveloper(wxT("Zach Bacon"));

    /* Try to load the PChat logo */
    wxArrayString searchPaths;
    wxString exeDir = wxPathOnly(wxStandardPaths::Get().GetExecutablePath());
    searchPaths.Add(exeDir + wxFILE_SEP_PATH + wxT("icons"));
    searchPaths.Add(exeDir + wxFILE_SEP_PATH + wxT("..") + wxFILE_SEP_PATH +
                    wxT("..") + wxFILE_SEP_PATH + wxT("data") +
                    wxFILE_SEP_PATH + wxT("icons"));
    searchPaths.Add(exeDir + wxFILE_SEP_PATH + wxT("..") + wxFILE_SEP_PATH +
                    wxT("share") + wxFILE_SEP_PATH + wxT("pchat") +
                    wxFILE_SEP_PATH + wxT("icons"));
#ifdef __WXMSW__
    wxString msysPrefix;
    if (wxGetEnv(wxT("MSYSTEM_PREFIX"), &msysPrefix))
        searchPaths.Add(msysPrefix + wxT("/share/pchat/icons"));
#endif
    for (const auto &dir : searchPaths) {
        wxString logoPath = dir + wxFILE_SEP_PATH + wxT("pchat.png");
        if (wxFileExists(logoPath)) {
            wxIcon logo;
            logo.LoadFile(logoPath, wxBITMAP_TYPE_PNG);
            if (logo.IsOk())
                info.SetIcon(logo);
            break;
        }
    }

    wxAboutBox(info, this);
}

void MainWindow::OnTreeSelChanged(wxTreeEvent &event)
{
    if (m_tab_switching) return;

    wxTreeItemId item = event.GetItem();
    if (!item.IsOk()) return;

    /* Find corresponding session */
    for (auto &pair : m_session_tree_map) {
        if (pair.second == item) {
            SwitchToSession(pair.first);
            return;
        }
    }
}

void MainWindow::OnInputEnter(wxCommandEvent &event)
{
    if (!m_current_session) return;

    wxString text = m_input_box->GetText();
    if (text.IsEmpty()) return;

    /* Reset tab completion */
    TabCompClean();

    /* Add to history */
    char *utf8 = g_strdup(text.utf8_str().data());
    history_add(&m_current_session->history, utf8);

    /* Process command or send message */
    handle_multiline(m_current_session, utf8, TRUE, FALSE);

    g_free(utf8);
    m_input_box->ClearAll();
}

void MainWindow::OnInputKeyDown(wxKeyEvent &event)
{
    if (!m_current_session) {
        event.Skip();
        return;
    }

    int key = event.GetKeyCode();
    bool ctrl = event.ControlDown();

    /* Enter — send message (don't let STC insert a newline) */
    if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER) {
        wxCommandEvent dummy;
        OnInputEnter(dummy);
        return;
    }

    if (key == WXK_UP && !ctrl) {
        /* History up */
        const char *prev = history_up(&m_current_session->history,
                                       (char *)m_input_box->GetText().utf8_str().data());
        if (prev) {
            m_input_box->SetText(wxString::FromUTF8(prev));
            m_input_box->GotoPos(m_input_box->GetLength());
        }
        return;
    }

    if (key == WXK_DOWN && !ctrl) {
        /* History down */
        const char *next = history_down(&m_current_session->history);
        if (next) {
            m_input_box->SetText(wxString::FromUTF8(next));
            m_input_box->GotoPos(m_input_box->GetLength());
        } else {
            m_input_box->ClearAll();
        }
        return;
    }

    if (key == WXK_TAB) {
        /* Tab completion */
        DoTabCompletion(event.ShiftDown());
        return; /* Don't skip - consume the tab */
    }

    /* Ctrl+B = Bold formatting code */
    if (ctrl && key == 'B') {
        m_input_box->ReplaceSelection(wxString((wxChar)2, 1));
        return;
    }

    /* Ctrl+I = Italic formatting code */
    if (ctrl && key == 'I') {
        m_input_box->ReplaceSelection(wxString((wxChar)29, 1));
        return;
    }

    /* Ctrl+U = Underline formatting code */
    if (ctrl && key == 'U') {
        m_input_box->ReplaceSelection(wxString((wxChar)31, 1));
        return;
    }

    /* Ctrl+K = Color code */
    if (ctrl && key == 'K') {
        m_input_box->ReplaceSelection(wxString((wxChar)3, 1));
        return;
    }

    /* Ctrl+O = Reset formatting */
    if (ctrl && key == 'O') {
        m_input_box->ReplaceSelection(wxString((wxChar)15, 1));
        return;
    }

    /* Page Up/Down scroll chat */
    if (key == WXK_PAGEUP) {
        m_chat_text->PageUp();
        return;
    }
    if (key == WXK_PAGEDOWN) {
        m_chat_text->PageDown();
        return;
    }

    /* Ctrl+PgUp / Ctrl+PgDn - switch tabs */
    if (ctrl && key == WXK_PAGEUP) {
        /* Switch to previous session */
        if (m_current_session && m_sessions.size() > 1) {
            for (size_t i = 0; i < m_sessions.size(); i++) {
                if (m_sessions[i].sess == m_current_session) {
                    size_t prev = (i == 0) ? m_sessions.size() - 1 : i - 1;
                    SwitchToSession(m_sessions[prev].sess);
                    if (m_sessions[prev].tree_id.IsOk())
                        m_channel_tree->SelectItem(m_sessions[prev].tree_id);
                    break;
                }
            }
        }
        return;
    }

    if (ctrl && key == WXK_PAGEDOWN) {
        /* Switch to next session */
        if (m_current_session && m_sessions.size() > 1) {
            for (size_t i = 0; i < m_sessions.size(); i++) {
                if (m_sessions[i].sess == m_current_session) {
                    size_t next = (i + 1) % m_sessions.size();
                    SwitchToSession(m_sessions[next].sess);
                    if (m_sessions[next].tree_id.IsOk())
                        m_channel_tree->SelectItem(m_sessions[next].tree_id);
                    break;
                }
            }
        }
        return;
    }

    /* Alt+1-9 - direct tab access */
    if (event.AltDown() && key >= '1' && key <= '9') {
        int idx = key - '1';
        if (idx >= 0 && idx < (int)m_sessions.size()) {
            SwitchToSession(m_sessions[idx].sess);
            if (m_sessions[idx].tree_id.IsOk())
                m_channel_tree->SelectItem(m_sessions[idx].tree_id);
        }
        return;
    }

    /* Any other key resets tab completion */
    if (key != WXK_SHIFT && key != WXK_CONTROL && key != WXK_ALT) {
        TabCompClean();
    }

    event.Skip();
}

void MainWindow::OnClose(wxCloseEvent &event)
{
    /* Save window geometry */
    if (!IsMaximized() && !IsIconized()) {
        wxRect r = GetRect();
        prefs.pchat_gui_win_left = r.GetLeft();
        prefs.pchat_gui_win_top = r.GetTop();
        prefs.pchat_gui_win_width = r.GetWidth();
        prefs.pchat_gui_win_height = r.GetHeight();
    }

    prefs.pchat_gui_pane_left_size = m_hsplitter_left->GetSashPosition();
    if (m_hsplitter_right->IsSplit()) {
        prefs.pchat_gui_pane_right_size =
            m_hsplitter_right->GetSize().GetWidth() -
            m_hsplitter_right->GetSashPosition();
    }

    /* Ask to quit if connected */
    if (prefs.pchat_gui_quit_dialog && !pchat_is_quitting) {
        bool connected = false;
        for (GSList *list = serv_list; list; list = list->next) {
            server *s = (server *)list->data;
            if (s->connected) {
                connected = true;
                break;
            }
        }
        if (connected) {
            int result = wxMessageBox(
                wxT("You are still connected to IRC.\n"
                    "Are you sure you want to quit?"),
                wxT("PChat - Quit"),
                wxYES_NO | wxICON_QUESTION, this);
            if (result != wxYES) {
                event.Veto();
                return;
            }
        }
    }

    /* Remove tray icon before quitting */
    if (m_tray_icon) {
        m_tray_icon->RemoveIcon();
    }

    /* Initiate clean shutdown via the backend.
       pchat_exit() saves config, frees sessions, then calls fe_exit()
       which destroys this window and exits the event loop. */
    if (!pchat_is_quitting) {
        pchat_is_quitting = TRUE;
        pchat_exit();  /* This will call fe_exit() → Destroy() + ExitMainLoop() */
    } else {
        /* Already shutting down (re-entered from fe_exit), just destroy */
        Destroy();
    }
}

void MainWindow::OnTopicEnter(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server) return;

    wxString topic = m_topic_entry->GetValue();
    char *utf8 = g_strdup(topic.utf8_str().data());

    m_current_session->server->p_topic(m_current_session->server,
                                        m_current_session->channel, utf8);
    g_free(utf8);
}

void MainWindow::OnUserlistRightClick(wxListEvent &event)
{
    wxMenu *menu = BuildUserlistPopup();
    /* Bind all popup commands */
    for (int i = 0; i <= 18; i++) {
        menu->Bind(wxEVT_MENU, &MainWindow::OnUserlistPopupCmd, this,
                   ID_USERLIST_POPUP_BASE + i);
    }
    PopupMenu(menu);
    delete menu;
}

void MainWindow::OnChatRightClick(wxMouseEvent &event)
{
    wxMenu *menu = BuildChatPopup();
    PopupMenu(menu);
    delete menu;
}

/* ===== Spell Check ===== */

void MainWindow::SpellCheckInput()
{
    SpellChecker &sc = SpellChecker::Instance();
    if (!sc.IsReady()) return;

    /* Clear all existing spell indicators */
    m_input_box->SetIndicatorCurrent(0);
    int len = m_input_box->GetLength();
    m_input_box->IndicatorClearRange(0, len);

    if (len == 0) return;

    /* Walk through every word and check spelling.
       WordStartPosition/WordEndPosition with onlyWordCharacters=true can
       look backward past non-word chars, so we must ensure forward progress
       by requiring wordEnd > pos. */
    int pos = 0;
    while (pos < len) {
        int wordStart = m_input_box->WordStartPosition(pos, true);
        int wordEnd = m_input_box->WordEndPosition(pos, true);

        if (wordEnd > wordStart && wordEnd > pos) {
            wxString word = m_input_box->GetTextRange(wordStart, wordEnd);
            if (!word.IsEmpty() && !sc.CheckWord(word)) {
                m_input_box->IndicatorFillRange(wordStart, wordEnd - wordStart);
            }
            pos = wordEnd;
        } else {
            pos++;
        }
    }
}

void MainWindow::OnInputModified(wxStyledTextEvent &event)
{
    event.Skip();
    if (m_updating_input) return;

    /* Strip newlines from pasted text */
    wxString text = m_input_box->GetText();
    if (text.Contains(wxT("\n")) || text.Contains(wxT("\r"))) {
        m_updating_input = true;
        int pos = m_input_box->GetCurrentPos();
        text.Replace(wxT("\r\n"), wxT(" "));
        text.Replace(wxT("\n"), wxT(" "));
        text.Replace(wxT("\r"), wxT(" "));
        m_input_box->SetText(text);
        if (pos <= m_input_box->GetLength())
            m_input_box->GotoPos(pos);
        m_updating_input = false;
    }

    /* Debounce spell checking — restart 200ms one-shot timer.
       This avoids running the spell checker on every single keystroke,
       which was a significant source of input lag. */
    m_spell_timer.StartOnce(200);
}

void MainWindow::OnSpellTimer(wxTimerEvent &)
{
    SpellCheckInput();
}

void MainWindow::OnInputContextMenu(wxContextMenuEvent &event)
{
    wxMenu menu;

    SpellChecker &sc = SpellChecker::Instance();

    /* Determine character position under the right-click point.
       Convert screen coords to client coords, then use STC's
       PositionFromPointClose to get the byte position. */
    int charPos = -1;
    wxPoint screenPt = event.GetPosition();
    if (screenPt != wxDefaultPosition) {
        wxPoint clientPt = m_input_box->ScreenToClient(screenPt);
        charPos = m_input_box->PositionFromPointClose(clientPt.x, clientPt.y);
    }
    if (charPos < 0) {
        charPos = m_input_box->GetCurrentPos();
    }

    /* Use STC's built-in word boundary detection */
    int start = m_input_box->WordStartPosition(charPos, true);
    int end = m_input_box->WordEndPosition(charPos, true);

    wxString word;
    if (end > start)
        word = m_input_box->GetTextRange(start, end);

    m_spell_word = word;
    m_spell_word_start = start;
    m_spell_word_end = end;

    bool wordMisspelled = false;

    if (sc.IsReady() && !word.IsEmpty() && !sc.CheckWord(word)) {
        wordMisspelled = true;

        /* Word is misspelled — show suggestions */
        auto suggestions = sc.Suggest(word);

        if (!suggestions.empty()) {
            for (size_t i = 0; i < suggestions.size() && i < 8; i++) {
                menu.Append(ID_SPELL_SUGGEST_BASE + (int)i, suggestions[i]);
            }
        } else {
            menu.Append(wxID_ANY, wxT("(no suggestions)"))->Enable(false);
        }

        menu.AppendSeparator();
        menu.Append(ID_SPELL_ADD_WORD,
                    wxString::Format(wxT("Add \"%s\" to dictionary"), word));
        menu.AppendSeparator();
    }

    /* Standard edit menu items */
    menu.Append(wxID_UNDO, wxT("&Undo"));
    menu.AppendSeparator();
    menu.Append(wxID_CUT, wxT("Cu&t"));
    menu.Append(wxID_COPY, wxT("&Copy"));
    menu.Append(wxID_PASTE, wxT("&Paste"));
    menu.Append(wxID_DELETE, wxT("&Delete"));
    menu.AppendSeparator();
    menu.Append(wxID_SELECTALL, wxT("Select &All"));

    /* Bind spell events dynamically when word is misspelled */
    if (wordMisspelled) {
        for (int i = ID_SPELL_SUGGEST_BASE; i <= ID_SPELL_SUGGEST_END; i++) {
            Bind(wxEVT_MENU, &MainWindow::OnSpellSuggestion, this, i);
        }
        Bind(wxEVT_MENU, &MainWindow::OnSpellAddWord, this, ID_SPELL_ADD_WORD);
    }

    PopupMenu(&menu);

    /* Unbind spell events after menu is dismissed */
    if (wordMisspelled) {
        for (int i = ID_SPELL_SUGGEST_BASE; i <= ID_SPELL_SUGGEST_END; i++) {
            Unbind(wxEVT_MENU, &MainWindow::OnSpellSuggestion, this, i);
        }
        Unbind(wxEVT_MENU, &MainWindow::OnSpellAddWord, this, ID_SPELL_ADD_WORD);
    }
}

void MainWindow::OnSpellSuggestion(wxCommandEvent &event)
{
    int idx = event.GetId() - ID_SPELL_SUGGEST_BASE;
    SpellChecker &sc = SpellChecker::Instance();

    if (m_spell_word.IsEmpty()) return;

    auto suggestions = sc.Suggest(m_spell_word);
    if (idx < 0 || idx >= (int)suggestions.size()) return;

    wxString replacement = suggestions[idx];

    /* Use stored word boundaries from the right-click position */
    wxString curWord = m_input_box->GetTextRange(m_spell_word_start,
                                                  m_spell_word_end);
    if (curWord == m_spell_word) {
        m_input_box->SetTargetStart(m_spell_word_start);
        m_input_box->SetTargetEnd(m_spell_word_end);
        m_input_box->ReplaceTarget(replacement);
        m_input_box->GotoPos(m_spell_word_start +
                              (int)replacement.utf8_str().length());
    }
}

void MainWindow::OnSpellAddWord(wxCommandEvent &event)
{
    if (!m_spell_word.IsEmpty()) {
        SpellChecker::Instance().AddWord(m_spell_word);
        /* Re-check to remove the squiggle from the added word */
        SpellCheckInput();
    }
}


void MainWindow::OnModeButtonToggle(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server) return;
    if (m_current_session->type != SESS_CHANNEL) return;

    int id = event.GetId();
    char mode_char = 0;
    int btn_idx = -1;

    /* Map button ID to mode char and flag_wid index */
    switch (id) {
    case ID_MODE_C: mode_char = 'c'; btn_idx = 0; break;
    case ID_MODE_N: mode_char = 'n'; btn_idx = 1; break;
    case ID_MODE_T: mode_char = 't'; btn_idx = 2; break;
    case ID_MODE_I: mode_char = 'i'; btn_idx = 3; break;
    case ID_MODE_M: mode_char = 'm'; btn_idx = 4; break;
    case ID_MODE_L: mode_char = 'l'; btn_idx = 5; break;
    case ID_MODE_K: mode_char = 'k'; btn_idx = 6; break;
    case ID_MODE_B: mode_char = 'b'; btn_idx = 7; break;
    }

    if (mode_char && btn_idx >= 0) {
        bool on = m_mode_buttons[btn_idx]->GetValue();
        char cmd_buf[128];

        if (mode_char == 'b') {
            /* Ban list — request list from server */
            g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s +b",
                       m_current_session->channel);
            handle_command(m_current_session, cmd_buf, FALSE);
            /* Untoggle immediately — b is momentary */
            m_mode_buttons[btn_idx]->SetValue(false);
        } else if (mode_char == 'l') {
            /* Limit mode needs a value */
            wxString limit = m_limit_entry->GetValue();
            if (on && !limit.IsEmpty()) {
                g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s +l %s",
                           m_current_session->channel,
                           (const char *)limit.utf8_str());
            } else {
                g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s -l",
                           m_current_session->channel);
            }
            handle_command(m_current_session, cmd_buf, FALSE);
        } else if (mode_char == 'k') {
            /* Key mode needs a value */
            wxString key = m_key_entry->GetValue();
            if (on && !key.IsEmpty()) {
                g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s +k %s",
                           m_current_session->channel,
                           (const char *)key.utf8_str());
            } else {
                g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s -k",
                           m_current_session->channel);
            }
            handle_command(m_current_session, cmd_buf, FALSE);
        } else {
            g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s %c%c",
                       m_current_session->channel, on ? '+' : '-', mode_char);
            handle_command(m_current_session, cmd_buf, FALSE);
        }
    }
}

void MainWindow::OnLimitKeyEntry(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server) return;

    wxString limit = m_limit_entry->GetValue();
    char cmd_buf[128];
    if (!limit.IsEmpty()) {
        g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s +l %s",
                   m_current_session->channel,
                   (const char *)limit.utf8_str());
    } else {
        g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s -l",
                   m_current_session->channel);
    }
    handle_command(m_current_session, cmd_buf, FALSE);
}

void MainWindow::OnKeyKeyEntry(wxCommandEvent &event)
{
    if (!m_current_session || !m_current_session->server) return;

    wxString key = m_key_entry->GetValue();
    char cmd_buf[128];
    if (!key.IsEmpty()) {
        g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s +k %s",
                   m_current_session->channel,
                   (const char *)key.utf8_str());
    } else {
        g_snprintf(cmd_buf, sizeof(cmd_buf), "MODE %s -k",
                   m_current_session->channel);
    }
    handle_command(m_current_session, cmd_buf, FALSE);
}

void MainWindow::OnFindDialogEvent(wxFindDialogEvent &event)
{
    wxEventType type = event.GetEventType();

    if (type == wxEVT_FIND_CLOSE) {
        m_find_dlg->Destroy();
        m_find_dlg = nullptr;
        return;
    }

    /* Search text in chat area */
    wxString searchStr = event.GetFindString();
    if (searchStr.IsEmpty()) return;

    int flags = event.GetFlags();
    wxString chatContent = m_chat_text->GetValue();

    wxString searchIn = chatContent;
    wxString searchFor = searchStr;
    if (!(flags & wxFR_MATCHCASE)) {
        searchIn = searchIn.Lower();
        searchFor = searchFor.Lower();
    }

    /* Find from current position */
    long startPos = m_chat_text->GetInsertionPoint();
    long pos;

    if (flags & wxFR_DOWN) {
        pos = searchIn.find(searchFor, startPos + 1);
    } else {
        /* Search backward */
        pos = searchIn.rfind(searchFor, startPos > 0 ? startPos - 1 : 0);
    }

    if (pos != wxNOT_FOUND) {
        m_chat_text->SetSelection(pos, pos + (long)searchStr.Length());
        m_chat_text->ShowPosition(pos);
    } else {
        wxMessageBox(wxString::Format(wxT("'%s' not found."), searchStr),
                     wxT("Search"), wxOK | wxICON_INFORMATION, this);
    }
}

/* ===== Tab Bar Helpers ===== */

void MainWindow::OnTabBarPageChanged(wxBookCtrlEvent &event)
{
    if (m_tab_switching) {
        event.Skip();
        return;
    }

    int sel = event.GetSelection();
    if (sel < 0) return;

    /* Find the session for this tab index */
    for (auto &pair : m_session_tab_map) {
        if (pair.second == sel) {
            SwitchToSession(pair.first);
            return;
        }
    }
}

void MainWindow::UpdateTabBarSelection(struct session *sess)
{
    if (!m_tab_bar) return;

    auto it = m_session_tab_map.find(sess);
    if (it != m_session_tab_map.end()) {
        int idx = it->second;
        if (idx >= 0 && idx < (int)m_tab_bar->GetPageCount() &&
            m_tab_bar->GetSelection() != idx) {
            m_tab_switching = true;
            m_tab_bar->SetSelection(idx);
            m_tab_switching = false;
        }
    }
}

void MainWindow::UpdateTabBarLabel(struct session *sess, const wxString &label)
{
    if (!m_tab_bar) return;

    auto it = m_session_tab_map.find(sess);
    if (it != m_session_tab_map.end()) {
        int idx = it->second;
        if (idx >= 0 && idx < (int)m_tab_bar->GetPageCount()) {
            m_tab_bar->SetPageText(idx, label);
        }
    }
}

void MainWindow::UpdateTabBarColor(struct session *sess, int color)
{
    /* wxNotebook on Windows doesn't natively support per-tab text colour.
       This is a no-op for now; colour is handled by the tree view.
       Could be extended with owner-drawn tabs in the future. */
    (void)sess;
    (void)color;
}

void MainWindow::RebuildTabBarIndices()
{
    /* Rebuild m_session_tab_map to match current notebook page order.
       Called after a page is deleted. */
    m_session_tab_map.clear();
    int pageCount = m_tab_bar ? (int)m_tab_bar->GetPageCount() : 0;
    int idx = 0;
    for (auto &st : m_sessions) {
        if (idx < pageCount) {
            m_session_tab_map[st.sess] = idx;
            idx++;
        }
    }
}
