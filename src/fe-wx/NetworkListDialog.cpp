/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Network List Dialog implementation
 * Replicates the HexChat "Network List" dialog exactly:
 *   - User Information section (nick, second choice, third choice, username)
 *   - Networks list box
 *   - Add/Remove/Edit/Sort/Favor buttons
 *   - Skip/Favorites checkboxes
 *   - Close/Connect buttons
 */

#include "NetworkListDialog.h"
#include "fe-wx.h"

#include <wx/statline.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/servlist.h"
#include "../common/cfgfiles.h"
}

/* ===== NetworkListDialog ===== */

wxBEGIN_EVENT_TABLE(NetworkListDialog, wxDialog)
    EVT_BUTTON(ID_NET_ADD, NetworkListDialog::OnAdd)
    EVT_BUTTON(ID_NET_REMOVE, NetworkListDialog::OnRemove)
    EVT_BUTTON(ID_NET_EDIT, NetworkListDialog::OnEdit)
    EVT_BUTTON(ID_NET_SORT, NetworkListDialog::OnSort)
    EVT_BUTTON(ID_NET_FAVOR, NetworkListDialog::OnFavor)
    EVT_BUTTON(ID_NET_CONNECT, NetworkListDialog::OnConnect)
    EVT_BUTTON(wxID_CLOSE, NetworkListDialog::OnClose)
    EVT_LISTBOX(ID_NET_LIST, NetworkListDialog::OnNetworkSelected)
wxEND_EVENT_TABLE()

NetworkListDialog::NetworkListDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, wxT("Network List - PChat"),
               wxDefaultPosition, wxSize(400, 500),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    CreateLayout();
    RefreshNetworkList();

    /* Load saved user info */
    m_nick_entry->SetValue(wxString::FromUTF8(prefs.pchat_irc_nick1));
    m_second_entry->SetValue(wxString::FromUTF8(prefs.pchat_irc_nick2));
    m_third_entry->SetValue(wxString::FromUTF8(prefs.pchat_irc_nick3));
    m_username_entry->SetValue(wxString::FromUTF8(prefs.pchat_irc_user_name));

    m_skip_on_startup->SetValue(prefs.pchat_gui_slist_skip);
    m_show_favorites->SetValue(prefs.pchat_gui_slist_fav);

    Centre();
}

NetworkListDialog::~NetworkListDialog()
{
}

void NetworkListDialog::CreateLayout()
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* === User Information === */
    wxStaticBoxSizer *userBox = new wxStaticBoxSizer(wxVERTICAL, this,
                                                      wxT("User Information"));
    wxWindow *userParent = userBox->GetStaticBox();

    wxFlexGridSizer *userGrid = new wxFlexGridSizer(4, 2, 4, 8);
    userGrid->AddGrowableCol(1, 1);

    userGrid->Add(new wxStaticText(userParent, wxID_ANY, wxT("Nick name:")),
                  0, wxALIGN_CENTER_VERTICAL);
    m_nick_entry = new wxTextCtrl(userParent, wxID_ANY);
    userGrid->Add(m_nick_entry, 1, wxEXPAND);

    userGrid->Add(new wxStaticText(userParent, wxID_ANY, wxT("Second choice:")),
                  0, wxALIGN_CENTER_VERTICAL);
    m_second_entry = new wxTextCtrl(userParent, wxID_ANY);
    userGrid->Add(m_second_entry, 1, wxEXPAND);

    userGrid->Add(new wxStaticText(userParent, wxID_ANY, wxT("Third choice:")),
                  0, wxALIGN_CENTER_VERTICAL);
    m_third_entry = new wxTextCtrl(userParent, wxID_ANY);
    userGrid->Add(m_third_entry, 1, wxEXPAND);

    userGrid->Add(new wxStaticText(userParent, wxID_ANY, wxT("User name:")),
                  0, wxALIGN_CENTER_VERTICAL);
    m_username_entry = new wxTextCtrl(userParent, wxID_ANY);
    userGrid->Add(m_username_entry, 1, wxEXPAND);

    userBox->Add(userGrid, 0, wxEXPAND | wxALL, 4);
    mainSizer->Add(userBox, 0, wxEXPAND | wxALL, 8);

    /* === Networks === */
    wxStaticBoxSizer *netBox = new wxStaticBoxSizer(wxHORIZONTAL, this,
                                                     wxT("Networks"));
    wxWindow *netParent = netBox->GetStaticBox();

    /* Network list */
    m_network_list = new wxListBox(netParent, ID_NET_LIST,
                                    wxDefaultPosition, wxSize(200, 200));
    netBox->Add(m_network_list, 1, wxEXPAND | wxALL, 4);

    /* Buttons column */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxVERTICAL);
    m_add_btn = new wxButton(netParent, ID_NET_ADD, wxT("Add"));
    m_remove_btn = new wxButton(netParent, ID_NET_REMOVE, wxT("Remove"));
    m_edit_btn = new wxButton(netParent, ID_NET_EDIT, wxT("Edit..."));
    m_sort_btn = new wxButton(netParent, ID_NET_SORT, wxT("Sort"));
    m_favor_btn = new wxButton(netParent, ID_NET_FAVOR, wxT("Favor"));

    btnSizer->Add(m_add_btn, 0, wxEXPAND | wxBOTTOM, 4);
    btnSizer->Add(m_remove_btn, 0, wxEXPAND | wxBOTTOM, 4);
    btnSizer->Add(m_edit_btn, 0, wxEXPAND | wxBOTTOM, 4);
    btnSizer->Add(m_sort_btn, 0, wxEXPAND | wxBOTTOM, 4);
    btnSizer->Add(m_favor_btn, 0, wxEXPAND);

    netBox->Add(btnSizer, 0, wxALL, 4);
    mainSizer->Add(netBox, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

    /* === Options === */
    wxBoxSizer *optSizer = new wxBoxSizer(wxHORIZONTAL);
    m_skip_on_startup = new wxCheckBox(this, ID_NET_SKIP_STARTUP,
                                        wxT("Skip network list on startup"));
    m_show_favorites = new wxCheckBox(this, ID_NET_SHOW_FAV,
                                       wxT("Show favorites only"));
    optSizer->Add(m_skip_on_startup, 0, wxRIGHT, 16);
    optSizer->Add(m_show_favorites, 0);
    mainSizer->Add(optSizer, 0, wxALL, 8);

    /* === Close/Connect buttons === */
    wxBoxSizer *bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    m_close_btn = new wxButton(this, wxID_CLOSE, wxT("Close"));
    m_connect_btn = new wxButton(this, ID_NET_CONNECT, wxT("Connect"));
    m_connect_btn->SetDefault();

    bottomSizer->AddStretchSpacer();
    bottomSizer->Add(m_close_btn, 0, wxRIGHT, 8);
    bottomSizer->Add(m_connect_btn, 0);
    mainSizer->Add(bottomSizer, 0, wxEXPAND | wxALL, 8);

    SetSizer(mainSizer);
}

void NetworkListDialog::RefreshNetworkList()
{
    m_network_list->Clear();

    bool favOnly = m_show_favorites ? m_show_favorites->GetValue() : false;

    for (GSList *list = network_list; list; list = list->next) {
        ircnet *net = (ircnet *)list->data;
        if (favOnly && !(net->flags & FLAG_FAVORITE))
            continue;
        m_network_list->Append(wxString::FromUTF8(net->name), net);
    }

    /* Try to select saved selection */
    if (prefs.pchat_gui_slist_select >= 0 &&
        prefs.pchat_gui_slist_select < (int)m_network_list->GetCount()) {
        m_network_list->SetSelection(prefs.pchat_gui_slist_select);
    }
}

void NetworkListDialog::OnAdd(wxCommandEvent &event)
{
    wxTextEntryDialog dlg(this, wxT("Enter network name:"),
                           wxT("Add Network"), wxT("New Network"));
    if (dlg.ShowModal() == wxID_OK) {
        wxString name = dlg.GetValue();
        if (!name.IsEmpty()) {
            servlist_net_add((char *)name.utf8_str().data(), nullptr, FALSE);
            servlist_save();
            RefreshNetworkList();
        }
    }
}

void NetworkListDialog::OnRemove(wxCommandEvent &event)
{
    int sel = m_network_list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    ircnet *net = (ircnet *)m_network_list->GetClientData(sel);
    if (!net) return;

    int result = wxMessageBox(
        wxString::Format(wxT("Remove network '%s'?"),
                          wxString::FromUTF8(net->name)),
        wxT("PChat"), wxYES_NO | wxICON_QUESTION, this);

    if (result == wxYES) {
        servlist_net_remove(net);
        servlist_save();
        RefreshNetworkList();
    }
}

void NetworkListDialog::OnEdit(wxCommandEvent &event)
{
    int sel = m_network_list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    ircnet *net = (ircnet *)m_network_list->GetClientData(sel);
    if (!net) return;

    NetworkEditDialog dlg(this, net);
    if (dlg.ShowModal() == wxID_OK) {
        servlist_save();
        RefreshNetworkList();
    }
}

void NetworkListDialog::OnSort(wxCommandEvent &event)
{
    /* Sort network list alphabetically */
    network_list = g_slist_sort(network_list, [](gconstpointer a, gconstpointer b) -> gint {
        const ircnet *na = (const ircnet *)a;
        const ircnet *nb = (const ircnet *)b;
        return g_ascii_strcasecmp(na->name, nb->name);
    });
    servlist_save();
    RefreshNetworkList();
}

void NetworkListDialog::OnFavor(wxCommandEvent &event)
{
    int sel = m_network_list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    ircnet *net = (ircnet *)m_network_list->GetClientData(sel);
    if (!net) return;

    net->flags ^= FLAG_FAVORITE;
    servlist_save();
}

void NetworkListDialog::OnConnect(wxCommandEvent &event)
{
    /* Save user info */
    strncpy(prefs.pchat_irc_nick1,
            m_nick_entry->GetValue().utf8_str().data(), NICKLEN - 1);
    strncpy(prefs.pchat_irc_nick2,
            m_second_entry->GetValue().utf8_str().data(), NICKLEN - 1);
    strncpy(prefs.pchat_irc_nick3,
            m_third_entry->GetValue().utf8_str().data(), NICKLEN - 1);
    strncpy(prefs.pchat_irc_user_name,
            m_username_entry->GetValue().utf8_str().data(), 126);

    prefs.pchat_gui_slist_skip = m_skip_on_startup->GetValue() ? 1 : 0;
    prefs.pchat_gui_slist_fav = m_show_favorites->GetValue() ? 1 : 0;

    save_config();

    int sel = m_network_list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    ircnet *net = (ircnet *)m_network_list->GetClientData(sel);
    if (!net) return;

    prefs.pchat_gui_slist_select = sel;

    /* Determine which session to reuse, following the GTK frontend logic:
     * 1. If current_sess is no longer valid, use NULL (create new).
     * 2. Search existing sessions for one already on this network:
     *    - If found and not connected, reuse it.
     *    - If found and connected, use NULL (create new tab).
     * 3. If no match, but current_sess is empty and not connected, reuse it.
     * Passing NULL to servlist_connect() causes it to create a new session. */
    session *chosen = current_sess;
    session *target = nullptr;

    if (!is_session(chosen))
        chosen = nullptr;

    {
        GSList *list;
        session *sess;

        for (list = sess_list; list; list = list->next)
        {
            sess = (session *)list->data;
            if (sess->server->network == net)
            {
                target = sess;
                if (sess->server->connected)
                    target = nullptr;  /* already connected to this net, open new */
                break;
            }
        }

        /* Use the chosen session if it's empty and not connected */
        if (!target && chosen &&
            !chosen->server->connected &&
            chosen->server->server_session->channel[0] == 0)
        {
            target = chosen;
        }
    }

    /* Connect to selected network */
    servlist_connect(target, net, TRUE);

    /* Show the main window now that connection is underway */
    wxWindow *parent = GetParent();
    if (parent && !parent->IsShown()) {
        parent->Show(true);
        parent->Raise();
    }

    Hide();
}

void NetworkListDialog::OnClose(wxCommandEvent &event)
{
    /* Save user info */
    strncpy(prefs.pchat_irc_nick1,
            m_nick_entry->GetValue().utf8_str().data(), NICKLEN - 1);
    strncpy(prefs.pchat_irc_nick2,
            m_second_entry->GetValue().utf8_str().data(), NICKLEN - 1);
    strncpy(prefs.pchat_irc_nick3,
            m_third_entry->GetValue().utf8_str().data(), NICKLEN - 1);
    strncpy(prefs.pchat_irc_user_name,
            m_username_entry->GetValue().utf8_str().data(), 126);

    save_config();

    /* If the main window was never shown (no connection made), quit */
    wxWindow *parent = GetParent();
    if (parent && !parent->IsShown()) {
        Hide();
        parent->Close(true);
        return;
    }

    Hide();
}

void NetworkListDialog::OnNetworkSelected(wxCommandEvent &event)
{
    int sel = m_network_list->GetSelection();
    if (sel != wxNOT_FOUND) {
        m_selected_net = (ircnet *)m_network_list->GetClientData(sel);
    }
}

/* ===== NetworkEditDialog ===== */

/* Character set options — matches HexChat */
static const char *charset_options[] = {
    IRC_DEFAULT_CHARSET,
    "IRC (Latin/Unicode Hybrid)",
    "ISO-8859-15 (Western Europe)",
    "ISO-8859-2 (Central Europe)",
    "ISO-8859-7 (Greek)",
    "ISO-8859-8 (Hebrew)",
    "ISO-8859-9 (Turkish)",
    "ISO-2022-JP (Japanese)",
    "SJIS (Japanese)",
    "CP949 (Korean)",
    "KOI8-R (Cyrillic)",
    "CP1251 (Cyrillic)",
    "CP1256 (Arabic)",
    "CP1257 (Baltic)",
    "GB18030 (Chinese)",
    "TIS-620 (Thai)",
    nullptr
};

wxBEGIN_EVENT_TABLE(NetworkEditDialog, wxDialog)
    EVT_BUTTON(wxID_CLOSE, NetworkEditDialog::OnClose)
    EVT_BUTTON(ID_EDIT_ADD, NetworkEditDialog::OnAddButton)
    EVT_BUTTON(ID_EDIT_REMOVE, NetworkEditDialog::OnRemoveButton)
    EVT_BUTTON(ID_EDIT_EDIT, NetworkEditDialog::OnEditButton)
    EVT_CHECKBOX(ID_EDIT_USE_GLOBAL, NetworkEditDialog::OnUseGlobalToggle)
    EVT_CHOICE(ID_EDIT_LOGIN_METHOD, NetworkEditDialog::OnLoginMethodChanged)
wxEND_EVENT_TABLE()

NetworkEditDialog::NetworkEditDialog(wxWindow *parent, ircnet *net)
    : wxDialog(parent, wxID_ANY,
               wxString::Format(wxT("Edit %s - PChat"),
                                 wxString::FromUTF8(net->name)),
               wxDefaultPosition, wxSize(420, 580),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_net(net)
{
    CreateLayout();
    PopulateFields();
    Centre();
}

NetworkEditDialog::~NetworkEditDialog()
{
    SaveFields();
}

void NetworkEditDialog::CreateLayout()
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* === Top section: Notebook (tabs at bottom) + Add/Remove/Edit buttons === */
    wxBoxSizer *topSizer = new wxBoxSizer(wxHORIZONTAL);

    m_notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, wxNB_BOTTOM);

    /* Tab 1: Servers */
    wxPanel *serverPage = new wxPanel(m_notebook);
    wxBoxSizer *serverSizer = new wxBoxSizer(wxVERTICAL);
    m_server_list = new wxListBox(serverPage, wxID_ANY);
    serverSizer->Add(m_server_list, 1, wxEXPAND);
    serverPage->SetSizer(serverSizer);
    m_notebook->AddPage(serverPage, wxT("Servers"));

    /* Tab 2: Autojoin channels */
    wxPanel *channelPage = new wxPanel(m_notebook);
    wxBoxSizer *channelSizer = new wxBoxSizer(wxVERTICAL);
    m_channel_list = new wxListBox(channelPage, wxID_ANY);
    channelSizer->Add(m_channel_list, 1, wxEXPAND);
    channelPage->SetSizer(channelSizer);
    m_notebook->AddPage(channelPage, wxT("Autojoin channels"));

    /* Tab 3: Connect commands */
    wxPanel *commandPage = new wxPanel(m_notebook);
    wxBoxSizer *commandSizer = new wxBoxSizer(wxVERTICAL);
    m_command_list = new wxListBox(commandPage, wxID_ANY);
    commandSizer->Add(m_command_list, 1, wxEXPAND);
    commandPage->SetSizer(commandSizer);
    m_notebook->AddPage(commandPage, wxT("Connect commands"));

    m_notebook->SetMinSize(wxSize(-1, 120));
    topSizer->Add(m_notebook, 1, wxEXPAND | wxRIGHT, 4);

    /* Shared buttons (right of notebook) */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxVERTICAL);
    m_add_btn = new wxButton(this, ID_EDIT_ADD, wxT("Add"));
    m_remove_btn = new wxButton(this, ID_EDIT_REMOVE, wxT("Remove"));
    m_edit_btn = new wxButton(this, ID_EDIT_EDIT, wxT("Edit"));
    /* Ensure all buttons have consistent minimum width */
    wxSize btnMin(80, -1);
    m_add_btn->SetMinSize(btnMin);
    m_remove_btn->SetMinSize(btnMin);
    m_edit_btn->SetMinSize(btnMin);
    btnSizer->Add(m_add_btn, 0, wxEXPAND | wxBOTTOM, 2);
    btnSizer->Add(m_remove_btn, 0, wxEXPAND | wxBOTTOM, 2);
    btnSizer->Add(m_edit_btn, 0, wxEXPAND);
    topSizer->Add(btnSizer, 0, wxALIGN_TOP);

    mainSizer->Add(topSizer, 1, wxEXPAND | wxALL, 8);

    /* === Checkboxes (no group box, flat list) === */
    m_connect_selected = new wxCheckBox(this, wxID_ANY,
        wxT("Connect to selected server only"));
    m_connect_selected->SetToolTip(
        wxT("Don't cycle through all the servers when the connection fails."));
    m_auto_connect = new wxCheckBox(this, wxID_ANY,
        wxT("Connect to this network automatically"));
    m_bypass_proxy = new wxCheckBox(this, wxID_ANY,
        wxT("Bypass proxy server"));
    m_use_ssl = new wxCheckBox(this, wxID_ANY,
        wxT("Use SSL for all the servers on this network"));
    m_accept_invalid_cert = new wxCheckBox(this, wxID_ANY,
        wxT("Accept invalid SSL certificates"));
    m_use_global_info = new wxCheckBox(this, ID_EDIT_USE_GLOBAL,
        wxT("Use global user information"));

    mainSizer->Add(m_connect_selected, 0, wxLEFT | wxRIGHT | wxTOP, 8);
    mainSizer->Add(m_auto_connect, 0, wxLEFT | wxRIGHT | wxTOP, 8);
    mainSizer->Add(m_bypass_proxy, 0, wxLEFT | wxRIGHT | wxTOP, 8);
    mainSizer->Add(m_use_ssl, 0, wxLEFT | wxRIGHT | wxTOP, 8);
    mainSizer->Add(m_accept_invalid_cert, 0, wxLEFT | wxRIGHT | wxTOP, 8);
    mainSizer->Add(m_use_global_info, 0, wxLEFT | wxRIGHT | wxTOP, 8);

    /* === Identity fields (2-column grid) === */
    wxFlexGridSizer *idGrid = new wxFlexGridSizer(4, 2, 4, 8);
    idGrid->AddGrowableCol(1, 1);

    m_nick_label = new wxStaticText(this, wxID_ANY, wxT("Nick name:"));
    idGrid->Add(m_nick_label, 0, wxALIGN_CENTER_VERTICAL);
    m_nick_entry = new wxTextCtrl(this, wxID_ANY);
    idGrid->Add(m_nick_entry, 1, wxEXPAND);

    m_nick2_label = new wxStaticText(this, wxID_ANY, wxT("Second choice:"));
    idGrid->Add(m_nick2_label, 0, wxALIGN_CENTER_VERTICAL);
    m_nick2_entry = new wxTextCtrl(this, wxID_ANY);
    idGrid->Add(m_nick2_entry, 1, wxEXPAND);

    m_realname_label = new wxStaticText(this, wxID_ANY, wxT("Real name:"));
    idGrid->Add(m_realname_label, 0, wxALIGN_CENTER_VERTICAL);
    m_realname_entry = new wxTextCtrl(this, wxID_ANY);
    idGrid->Add(m_realname_entry, 1, wxEXPAND);

    m_username_label = new wxStaticText(this, wxID_ANY, wxT("User name:"));
    idGrid->Add(m_username_label, 0, wxALIGN_CENTER_VERTICAL);
    m_username_entry = new wxTextCtrl(this, wxID_ANY);
    idGrid->Add(m_username_entry, 1, wxEXPAND);

    mainSizer->Add(idGrid, 0, wxEXPAND | wxALL, 8);

    /* === Login method + Password === */
    wxFlexGridSizer *loginGrid = new wxFlexGridSizer(2, 2, 4, 8);
    loginGrid->AddGrowableCol(1, 1);

    loginGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Login method:")),
                   0, wxALIGN_CENTER_VERTICAL);
    m_login_method = new wxChoice(this, ID_EDIT_LOGIN_METHOD);
    m_login_method->Append(wxT("Default"));
    m_login_method->Append(wxT("NickServ (/MSG NickServ)"));
    m_login_method->Append(wxT("NickServ (/NICKSERV)"));
    m_login_method->Append(wxT("Challenge Auth (irc.quakenet.org)"));
    m_login_method->Append(wxT("Challenge Auth (HMAC-SHA-256)"));
    m_login_method->Append(wxT("Challenge Auth (HMAC-SHA-256) 2"));
    m_login_method->Append(wxT("SASL PLAIN (username + password)"));
    m_login_method->Append(wxT("Server password (/PASS password)"));
    m_login_method->Append(wxT("SASL EXTERNAL (cert)"));
    m_login_method->Append(wxT("Custom commands..."));
    m_login_method->Append(wxT("SASL SCRAM-SHA-1"));
    m_login_method->Append(wxT("SASL SCRAM-SHA-256"));
    m_login_method->Append(wxT("SASL SCRAM-SHA-512"));
    loginGrid->Add(m_login_method, 1, wxEXPAND);

    loginGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Password:")),
                   0, wxALIGN_CENTER_VERTICAL);
    m_password_entry = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                       wxDefaultPosition, wxDefaultSize,
                                       wxTE_PASSWORD);
    m_password_entry->SetToolTip(
        wxT("Password used for login. If in doubt, leave blank."));
    loginGrid->Add(m_password_entry, 1, wxEXPAND);

    mainSizer->Add(loginGrid, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);

    /* === Character set === */
    wxFlexGridSizer *charsetGrid = new wxFlexGridSizer(1, 2, 4, 8);
    charsetGrid->AddGrowableCol(1, 1);

    charsetGrid->Add(new wxStaticText(this, wxID_ANY, wxT("Character set:")),
                     0, wxALIGN_CENTER_VERTICAL);
    m_encoding = new wxComboBox(this, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxDefaultSize,
                                 0, nullptr, 0);
    /* Populate encoding list */
    for (int i = 0; charset_options[i] != nullptr; i++) {
        m_encoding->Append(wxString::FromUTF8(charset_options[i]));
    }
    charsetGrid->Add(m_encoding, 1, wxEXPAND);

    mainSizer->Add(charsetGrid, 0, wxEXPAND | wxALL, 8);

    /* === Separator + Close button === */
    mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 8);

    wxBoxSizer *bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    bottomSizer->AddStretchSpacer();
    bottomSizer->Add(new wxButton(this, wxID_CLOSE, wxT("Close")), 0);
    mainSizer->Add(bottomSizer, 0, wxEXPAND | wxALL, 8);

    SetSizer(mainSizer);

    Bind(wxEVT_CLOSE_WINDOW, &NetworkEditDialog::OnCloseWindow, this);
}

void NetworkEditDialog::PopulateFields()
{
    /* Servers */
    for (GSList *list = m_net->servlist; list; list = list->next) {
        ircserver *serv = (ircserver *)list->data;
        m_server_list->Append(wxString::FromUTF8(serv->hostname));
    }

    /* Autojoin channels */
    for (GSList *list = m_net->favchanlist; list; list = list->next) {
        favchannel *fav = (favchannel *)list->data;
        wxString entry = wxString::FromUTF8(fav->name);
        if (fav->key && fav->key[0]) {
            entry += wxT(" (") + wxString::FromUTF8(fav->key) + wxT(")");
        }
        m_channel_list->Append(entry);
    }

    /* Connect commands */
    for (GSList *list = m_net->commandlist; list; list = list->next) {
        commandentry *cmd = (commandentry *)list->data;
        m_command_list->Append(wxString::FromUTF8(cmd->command));
    }

    /* Select the previously-selected server */
    if (m_net->selected >= 0 && m_net->selected < (int)m_server_list->GetCount())
        m_server_list->SetSelection(m_net->selected);
    else if (m_server_list->GetCount() > 0)
        m_server_list->SetSelection(0);

    /* Checkboxes — note: "Connect to selected server only" is the INVERSE of FLAG_CYCLE */
    m_connect_selected->SetValue(!(m_net->flags & FLAG_CYCLE));
    m_auto_connect->SetValue((m_net->flags & FLAG_AUTO_CONNECT) != 0);
    m_bypass_proxy->SetValue(!(m_net->flags & FLAG_USE_PROXY));
    m_use_ssl->SetValue((m_net->flags & FLAG_USE_SSL) != 0);
    m_accept_invalid_cert->SetValue((m_net->flags & FLAG_ALLOW_INVALID) != 0);
    m_use_global_info->SetValue((m_net->flags & FLAG_USE_GLOBAL) != 0);

    /* Identity */
    if (m_net->nick) m_nick_entry->SetValue(wxString::FromUTF8(m_net->nick));
    if (m_net->nick2) m_nick2_entry->SetValue(wxString::FromUTF8(m_net->nick2));
    if (m_net->real) m_realname_entry->SetValue(wxString::FromUTF8(m_net->real));
    if (m_net->user) m_username_entry->SetValue(wxString::FromUTF8(m_net->user));

    /* Login method — map internal login type to combo index */
    int loginIdx = 0;
    switch (m_net->logintype) {
        case LOGIN_DEFAULT:       loginIdx = 0; break;
        case LOGIN_MSG_NICKSERV:  loginIdx = 1; break;
        case LOGIN_NICKSERV:      loginIdx = 2; break;
        case LOGIN_CHALLENGEAUTH: loginIdx = 3; break;
        case LOGIN_SASL:          loginIdx = 6; break;
        case LOGIN_PASS:          loginIdx = 7; break;
        case LOGIN_SASLEXTERNAL:  loginIdx = 8; break;
        case LOGIN_CUSTOM:        loginIdx = 9; break;
        case LOGIN_SASL_SCRAM_SHA_1:   loginIdx = 10; break;
        case LOGIN_SASL_SCRAM_SHA_256: loginIdx = 11; break;
        case LOGIN_SASL_SCRAM_SHA_512: loginIdx = 12; break;
        default:                  loginIdx = 0; break;
    }
    if (loginIdx < (int)m_login_method->GetCount())
        m_login_method->SetSelection(loginIdx);

    /* Password */
    if (m_net->pass) {
        m_password_entry->SetValue(wxString::FromUTF8(m_net->pass));
    }
    /* Disable password for SASL EXTERNAL */
    if (m_net->logintype == LOGIN_SASLEXTERNAL)
        m_password_entry->Enable(false);

    /* Character set */
    if (m_net->encoding) {
        m_encoding->SetValue(wxString::FromUTF8(m_net->encoding));
    } else {
        m_encoding->SetValue(wxT("System default"));
    }

    /* Grey out identity fields if using global info */
    UpdateIdentitySensitivity();
}

void NetworkEditDialog::SaveFields()
{
    /* Save flags — preserve FLAG_FAVORITE which is toggled separately */
    guint32 saved_favorite = m_net->flags & FLAG_FAVORITE;
    m_net->flags = saved_favorite;
    if (!m_connect_selected->GetValue()) m_net->flags |= FLAG_CYCLE;
    if (m_auto_connect->GetValue()) m_net->flags |= FLAG_AUTO_CONNECT;
    if (!m_bypass_proxy->GetValue()) m_net->flags |= FLAG_USE_PROXY;
    if (m_use_ssl->GetValue()) m_net->flags |= FLAG_USE_SSL;
    if (m_accept_invalid_cert->GetValue()) m_net->flags |= FLAG_ALLOW_INVALID;
    if (m_use_global_info->GetValue()) m_net->flags |= FLAG_USE_GLOBAL;

    /* Save selected server index */
    int selSrv = m_server_list->GetSelection();
    m_net->selected = (selSrv != wxNOT_FOUND) ? selSrv : 0;

    /* Save identity */
    g_free(m_net->nick);
    wxString nickVal = m_nick_entry->GetValue();
    m_net->nick = nickVal.IsEmpty() ? nullptr : g_strdup(nickVal.utf8_str().data());

    g_free(m_net->nick2);
    wxString nick2Val = m_nick2_entry->GetValue();
    m_net->nick2 = nick2Val.IsEmpty() ? nullptr : g_strdup(nick2Val.utf8_str().data());

    g_free(m_net->real);
    wxString realVal = m_realname_entry->GetValue();
    m_net->real = realVal.IsEmpty() ? nullptr : g_strdup(realVal.utf8_str().data());

    g_free(m_net->user);
    wxString userVal = m_username_entry->GetValue();
    m_net->user = userVal.IsEmpty() ? nullptr : g_strdup(userVal.utf8_str().data());

    /* Save login method — map combo index to internal login type */
    static const int login_map[] = {
        LOGIN_DEFAULT, LOGIN_MSG_NICKSERV, LOGIN_NICKSERV,
        LOGIN_CHALLENGEAUTH, LOGIN_CHALLENGEAUTH, LOGIN_CHALLENGEAUTH,
        LOGIN_SASL, LOGIN_PASS, LOGIN_SASLEXTERNAL, LOGIN_CUSTOM,
        LOGIN_SASL_SCRAM_SHA_1, LOGIN_SASL_SCRAM_SHA_256, LOGIN_SASL_SCRAM_SHA_512
    };
    int sel = m_login_method->GetSelection();
    if (sel >= 0 && sel < (int)(sizeof(login_map)/sizeof(login_map[0])))
        m_net->logintype = login_map[sel];

    /* Save password */
    g_free(m_net->pass);
    wxString passVal = m_password_entry->GetValue();
    m_net->pass = passVal.IsEmpty() ? nullptr : g_strdup(passVal.utf8_str().data());

    /* Save encoding */
    g_free(m_net->encoding);
    wxString encVal = m_encoding->GetValue();
    if (encVal.IsEmpty() || encVal == wxT("System default"))
        m_net->encoding = nullptr;
    else
        m_net->encoding = g_strdup(encVal.utf8_str().data());

    servlist_save();
}

void NetworkEditDialog::UpdateIdentitySensitivity()
{
    bool useGlobal = m_use_global_info->GetValue();
    m_nick_label->Enable(!useGlobal);
    m_nick_entry->Enable(!useGlobal);
    m_nick2_label->Enable(!useGlobal);
    m_nick2_entry->Enable(!useGlobal);
    m_realname_label->Enable(!useGlobal);
    m_realname_entry->Enable(!useGlobal);
    m_username_label->Enable(!useGlobal);
    m_username_entry->Enable(!useGlobal);
}

void NetworkEditDialog::OnUseGlobalToggle(wxCommandEvent &event)
{
    UpdateIdentitySensitivity();
}

void NetworkEditDialog::OnLoginMethodChanged(wxCommandEvent &event)
{
    /* Disable password for SASL EXTERNAL */
    int sel = m_login_method->GetSelection();
    /* Index 8 = SASL EXTERNAL */
    m_password_entry->Enable(sel != 8);
}

void NetworkEditDialog::OnClose(wxCommandEvent &event)
{
    SaveFields();
    EndModal(wxID_OK);
}

void NetworkEditDialog::OnCloseWindow(wxCloseEvent &event)
{
    SaveFields();
    EndModal(wxID_OK);
}

void NetworkEditDialog::OnAddButton(wxCommandEvent &event)
{
    int page = m_notebook->GetSelection();

    if (page == 0) {
        /* Add server */
        wxTextEntryDialog dlg(this, wxT("Enter server hostname:"),
                               wxT("Add Server"),
                               wxT("newserver/6667"));
        if (dlg.ShowModal() == wxID_OK) {
            wxString host = dlg.GetValue();
            if (!host.IsEmpty()) {
                servlist_server_add(m_net, (char *)host.utf8_str().data());
                m_server_list->Append(host);
            }
        }
    } else if (page == 1) {
        /* Add channel */
        wxTextEntryDialog dlg(this, wxT("Enter channel name:"),
                               wxT("Add Channel"), wxT("#"));
        if (dlg.ShowModal() == wxID_OK) {
            wxString chan = dlg.GetValue();
            if (!chan.IsEmpty()) {
                servlist_favchan_add(m_net, (char *)chan.utf8_str().data());
                m_channel_list->Append(chan);
            }
        }
    } else if (page == 2) {
        /* Add command */
        wxTextEntryDialog dlg(this, wxT("Enter command:"),
                               wxT("Add Command"));
        if (dlg.ShowModal() == wxID_OK) {
            wxString cmd = dlg.GetValue();
            if (!cmd.IsEmpty()) {
                servlist_command_add(m_net, (char *)cmd.utf8_str().data());
                m_command_list->Append(cmd);
            }
        }
    }
}

void NetworkEditDialog::OnRemoveButton(wxCommandEvent &event)
{
    int page = m_notebook->GetSelection();

    if (page == 0) {
        int sel = m_server_list->GetSelection();
        if (sel == wxNOT_FOUND) return;
        ircserver *serv = servlist_server_find(m_net,
            (char *)m_server_list->GetString(sel).utf8_str().data(), nullptr);
        if (serv) {
            servlist_server_remove(m_net, serv);
            m_server_list->Delete(sel);
        }
    } else if (page == 1) {
        int sel = m_channel_list->GetSelection();
        if (sel == wxNOT_FOUND) return;
        /* Extract channel name (strip key suffix if present) */
        wxString entry = m_channel_list->GetString(sel);
        wxString chanName = entry.BeforeFirst(' ');
        if (chanName.IsEmpty()) chanName = entry;
        favchannel *fav = servlist_favchan_find(m_net,
            (char *)chanName.utf8_str().data(), nullptr);
        if (fav) {
            servlist_favchan_remove(m_net, fav);
            m_channel_list->Delete(sel);
        }
    } else if (page == 2) {
        int sel = m_command_list->GetSelection();
        if (sel == wxNOT_FOUND) return;
        commandentry *cmd = servlist_command_find(m_net,
            (char *)m_command_list->GetString(sel).utf8_str().data(), nullptr);
        if (cmd) {
            servlist_command_remove(m_net, cmd);
            m_command_list->Delete(sel);
        }
    }
}

void NetworkEditDialog::OnEditButton(wxCommandEvent &event)
{
    int page = m_notebook->GetSelection();
    wxListBox *list = nullptr;

    if (page == 0) list = m_server_list;
    else if (page == 1) list = m_channel_list;
    else if (page == 2) list = m_command_list;

    if (!list) return;
    int sel = list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    wxString oldVal = list->GetString(sel);
    wxTextEntryDialog dlg(this, wxT("Edit:"), wxT("Edit"), oldVal);
    if (dlg.ShowModal() == wxID_OK) {
        wxString newVal = dlg.GetValue();
        if (!newVal.IsEmpty() && newVal != oldVal) {
            if (page == 0) {
                /* Edit server — remove old, add new */
                ircserver *serv = servlist_server_find(m_net,
                    (char *)oldVal.utf8_str().data(), nullptr);
                if (serv) {
                    g_free(serv->hostname);
                    serv->hostname = g_strdup(newVal.utf8_str().data());
                }
            } else if (page == 1) {
                wxString chanName = oldVal.BeforeFirst(' ');
                if (chanName.IsEmpty()) chanName = oldVal;
                favchannel *fav = servlist_favchan_find(m_net,
                    (char *)chanName.utf8_str().data(), nullptr);
                if (fav) {
                    g_free(fav->name);
                    fav->name = g_strdup(newVal.utf8_str().data());
                }
            } else if (page == 2) {
                commandentry *cmd = servlist_command_find(m_net,
                    (char *)oldVal.utf8_str().data(), nullptr);
                if (cmd) {
                    g_free(cmd->command);
                    cmd->command = g_strdup(newVal.utf8_str().data());
                }
            }
            list->SetString(sel, newVal);
        }
    }
}
