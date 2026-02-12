/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Channel List Dialog - replicates fe-gtk3/chanlist.c functionality
 */

#include "ChannelListDialog.h"
#include "fe-wx.h"
#include "DarkMode.h"

#include <wx/clipbrd.h>
#include <wx/file.h>
#include <glib.h>
#include <cstring>
#include <regex>

#include "../common/pchat.h"
#include "../common/pchatc.h"
extern "C" {
#include "../common/cfgfiles.h"
#include "../common/outbound.h"
#include "../common/util.h"
#include "../common/fe.h"
#include "../common/server.h"
#include "../common/text.h"
}

wxBEGIN_EVENT_TABLE(ChannelListDialog, wxDialog)
    EVT_BUTTON(ID_CHANLIST_REFRESH, ChannelListDialog::OnRefresh)
    EVT_BUTTON(ID_CHANLIST_SEARCH, ChannelListDialog::OnSearch)
    EVT_BUTTON(ID_CHANLIST_JOIN, ChannelListDialog::OnJoin)
    EVT_BUTTON(ID_CHANLIST_SAVE, ChannelListDialog::OnSaveList)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, ChannelListDialog::OnItemActivated)
    EVT_LIST_ITEM_RIGHT_CLICK(wxID_ANY, ChannelListDialog::OnRightClick)
    EVT_MENU(ID_CHANLIST_JOIN_MENU, ChannelListDialog::OnJoinMenu)
    EVT_MENU(ID_CHANLIST_COPY_CHAN, ChannelListDialog::OnCopyChannel)
    EVT_MENU(ID_CHANLIST_COPY_TOPIC, ChannelListDialog::OnCopyTopic)
    EVT_TIMER(ID_CHANLIST_TIMER, ChannelListDialog::OnTimer)
    EVT_CLOSE(ChannelListDialog::OnCloseWindow)
wxEND_EVENT_TABLE()

ChannelListDialog::ChannelListDialog(wxWindow *parent, struct server *serv,
                                       bool do_refresh)
    : wxDialog(parent, wxID_ANY,
              wxString::Format(wxT(DISPLAY_NAME ": Channel List (%s)"),
                               wxString::FromUTF8(server_get_network(serv, TRUE))),
              wxDefaultPosition, wxSize(700, 520),
              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX),
      m_serv(serv),
      m_users_found(0), m_users_shown(0),
      m_channels_found(0), m_channels_shown(0),
      m_flush_timer(this, ID_CHANLIST_TIMER),
      m_ctx_item(-1)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* Caption label */
    m_caption = new wxStaticText(this, wxID_ANY, wxEmptyString);
    mainSizer->Add(m_caption, 0, wxEXPAND | wxALL, 4);

    /* Channel list control */
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, wxT("Channel"), wxLIST_FORMAT_LEFT, 150);
    m_list->InsertColumn(1, wxT("Users"), wxLIST_FORMAT_RIGHT, 60);
    m_list->InsertColumn(2, wxT("Topic"), wxLIST_FORMAT_LEFT, 440);

    mainSizer->Add(m_list, 1, wxEXPAND | wxALL, 4);

    /* Grid layout for filter options */
    wxFlexGridSizer *grid = new wxFlexGridSizer(4, 4, 4, 12);
    grid->AddGrowableCol(1);

    /* Row 0: Find */
    grid->Add(new wxStaticText(this, wxID_ANY, wxT("Find:")),
              0, wxALIGN_CENTER_VERTICAL);
    m_find_entry = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxTE_PROCESS_ENTER);
    m_find_entry->Bind(wxEVT_TEXT_ENTER, &ChannelListDialog::OnSearch, this);
    grid->Add(m_find_entry, 1, wxEXPAND);
    grid->Add(new wxStaticText(this, wxID_ANY, wxEmptyString), 0); /* spacer */
    m_btn_join = new wxButton(this, ID_CHANLIST_JOIN, wxT("Join Channel"));
    grid->Add(m_btn_join, 0, wxEXPAND);

    /* Row 1: Search type */
    grid->Add(new wxStaticText(this, wxID_ANY, wxT("Search type:")),
              0, wxALIGN_CENTER_VERTICAL);
    m_search_type = new wxComboBox(this, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    0, nullptr, wxCB_READONLY);
    m_search_type->Append(wxT("Simple Search"));
    m_search_type->Append(wxT("Pattern Match (Wildcards)"));
    m_search_type->Append(wxT("Regular Expression"));
    m_search_type->SetSelection(0);
    grid->Add(m_search_type, 1, wxEXPAND);
    grid->Add(new wxStaticText(this, wxID_ANY, wxEmptyString), 0);
    m_btn_save = new wxButton(this, ID_CHANLIST_SAVE, wxT("Save List..."));
    grid->Add(m_btn_save, 0, wxEXPAND);

    /* Row 2: Look in */
    grid->Add(new wxStaticText(this, wxID_ANY, wxT("Look in:")),
              0, wxALIGN_CENTER_VERTICAL);
    {
        wxBoxSizer *lookSizer = new wxBoxSizer(wxHORIZONTAL);
        m_match_channel = new wxCheckBox(this, wxID_ANY, wxT("Channel name"));
        m_match_channel->SetValue(true);
        m_match_topic = new wxCheckBox(this, wxID_ANY, wxT("Topic"));
        m_match_topic->SetValue(true);
        lookSizer->Add(m_match_channel, 0, wxRIGHT, 12);
        lookSizer->Add(m_match_topic, 0);
        grid->Add(lookSizer, 1, wxEXPAND);
    }
    grid->Add(new wxStaticText(this, wxID_ANY, wxEmptyString), 0);
    m_btn_refresh = new wxButton(this, ID_CHANLIST_REFRESH, wxT("Download List"));
    grid->Add(m_btn_refresh, 0, wxEXPAND);

    /* Row 3: Show only */
    grid->Add(new wxStaticText(this, wxID_ANY, wxT("Show only:")),
              0, wxALIGN_CENTER_VERTICAL);
    {
        wxBoxSizer *filterSizer = new wxBoxSizer(wxHORIZONTAL);
        filterSizer->Add(new wxStaticText(this, wxID_ANY, wxT("channels with")),
                         0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

        int minUsers = prefs.pchat_gui_chanlist_minusers;
        if (minUsers < 1 || minUsers > 999999) minUsers = 5;
        m_min_spin = new wxSpinCtrl(this, ID_CHANLIST_MIN_SPIN, wxEmptyString,
                                     wxDefaultPosition, wxSize(80, -1),
                                     wxSP_ARROW_KEYS, 1, 999999, minUsers);
        filterSizer->Add(m_min_spin, 0, wxRIGHT, 4);

        filterSizer->Add(new wxStaticText(this, wxID_ANY, wxT("to")),
                         0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

        int maxUsers = prefs.pchat_gui_chanlist_maxusers;
        if (maxUsers < 1 || maxUsers > 999999) maxUsers = 9999;
        m_max_spin = new wxSpinCtrl(this, ID_CHANLIST_MAX_SPIN, wxEmptyString,
                                     wxDefaultPosition, wxSize(80, -1),
                                     wxSP_ARROW_KEYS, 1, 999999, maxUsers);
        filterSizer->Add(m_max_spin, 0, wxRIGHT, 4);

        filterSizer->Add(new wxStaticText(this, wxID_ANY, wxT("users.")),
                         0, wxALIGN_CENTER_VERTICAL);
        grid->Add(filterSizer, 1, wxEXPAND);
    }
    grid->Add(new wxStaticText(this, wxID_ANY, wxEmptyString), 0);
    m_btn_search = new wxButton(this, ID_CHANLIST_SEARCH, wxT("Search"));
    grid->Add(m_btn_search, 0, wxEXPAND);

    mainSizer->Add(grid, 0, wxEXPAND | wxALL, 4);

    SetSizer(mainSizer);
    Centre();

    UpdateCaption();
    UpdateButtons();

    m_flush_timer.Start(250);

    if (do_refresh)
        DoRefresh();

    wx_darkmode_apply_to_window(this);
}

ChannelListDialog::~ChannelListDialog()
{
    m_flush_timer.Stop();
    if (is_server(m_serv))
        m_serv->gui->chanlist_window = nullptr;
}

void ChannelListDialog::UpdateCaption()
{
    m_caption->SetLabel(
        wxString::Format(wxT("Displaying %d/%d users on %d/%d channels."),
                         m_users_shown, m_users_found,
                         m_channels_shown, m_channels_found));
}

void ChannelListDialog::UpdateButtons()
{
    bool has_items = (m_channels_shown > 0);
    m_btn_join->Enable(has_items);
    m_btn_save->Enable(has_items);
}

bool ChannelListDialog::MatchesFilter(const ChanListRow &row)
{
    int minUsers = m_min_spin->GetValue();
    int maxUsers = m_max_spin->GetValue();

    if (row.users < minUsers) return false;
    if (maxUsers > 0 && row.users > maxUsers) return false;

    wxString pattern = m_find_entry->GetValue();
    if (pattern.IsEmpty()) return true;

    bool want_channel = m_match_channel->GetValue();
    bool want_topic = m_match_topic->GetValue();

    /* If both or neither checked, search both */
    if (want_channel == want_topic) {
        want_channel = true;
        want_topic = true;
    }

    int search_type = m_search_type->GetSelection();

    auto matches = [&](const wxString &str) -> bool {
        switch (search_type) {
        case 1: /* Wildcard */
            return str.Matches(wxT("*") + pattern + wxT("*"));
        case 2: /* Regex */
            try {
                std::regex re(pattern.utf8_str().data(),
                              std::regex::icase | std::regex::extended);
                return std::regex_search(
                    std::string(str.utf8_str().data()), re);
            } catch (...) {
                return false;
            }
        default: /* Simple search (case-insensitive) */
            return str.Lower().Contains(pattern.Lower());
        }
    };

    if (want_channel && matches(row.channel)) return true;
    if (want_topic && matches(row.topic)) return true;
    return false;
}

void ChannelListDialog::DoRefresh()
{
    if (!m_serv->connected) {
        wxMessageBox(wxT("Not connected."), wxT("Error"),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    m_list->DeleteAllItems();
    m_all_rows.clear();
    m_users_found = 0;
    m_users_shown = 0;
    m_channels_found = 0;
    m_channels_shown = 0;
    UpdateCaption();

    m_btn_refresh->Enable(false);

    int minUsers = m_min_spin->GetValue();

    /* Save prefs */
    prefs.pchat_gui_chanlist_minusers = minUsers;
    prefs.pchat_gui_chanlist_maxusers = m_max_spin->GetValue();
    save_config();

    /* Request list from server */
    if (m_serv->use_listargs) {
        m_serv->p_list_channels(m_serv, "", minUsers);
    } else {
        m_serv->p_list_channels(m_serv, "", 1);
    }
}

void ChannelListDialog::AddRow(const char *chan, const char *users,
                                const char *topic)
{
    ChanListRow row;
    row.channel = wxString::FromUTF8(chan);
    row.users = atoi(users);

    /* Strip colors from topic */
    char *stripped = strip_color(topic, -1, STRIP_ALL);
    row.topic = wxString::FromUTF8(stripped);
    g_free(stripped);

    m_all_rows.push_back(row);

    m_users_found += row.users;
    m_channels_found++;

    /* Check if it passes the filter */
    if (MatchesFilter(row)) {
        long idx = m_list->GetItemCount();
        m_list->InsertItem(idx, row.channel);
        m_list->SetItem(idx, 1, wxString::Format(wxT("%d"), row.users));
        m_list->SetItem(idx, 2, row.topic);

        m_users_shown += row.users;
        m_channels_shown++;
    }
}

void ChannelListDialog::ListEnd()
{
    m_btn_refresh->Enable(true);
    UpdateCaption();
    UpdateButtons();
}

void ChannelListDialog::ApplyFilter()
{
    m_list->DeleteAllItems();
    m_users_shown = 0;
    m_channels_shown = 0;

    for (const auto &row : m_all_rows) {
        if (MatchesFilter(row)) {
            long idx = m_list->GetItemCount();
            m_list->InsertItem(idx, row.channel);
            m_list->SetItem(idx, 1, wxString::Format(wxT("%d"), row.users));
            m_list->SetItem(idx, 2, row.topic);

            m_users_shown += row.users;
            m_channels_shown++;
        }
    }

    UpdateCaption();
    UpdateButtons();
}

void ChannelListDialog::OnRefresh(wxCommandEvent &event)
{
    DoRefresh();
}

void ChannelListDialog::OnSearch(wxCommandEvent &event)
{
    if (m_all_rows.empty()) {
        DoRefresh();
    } else {
        ApplyFilter();
    }
}

void ChannelListDialog::OnJoin(wxCommandEvent &event)
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    wxString chan = m_list->GetItemText(sel, 0);
    if (!chan.IsEmpty() && chan != wxT("*") && m_serv->connected) {
        wxString cmd = wxT("join ") + chan;
        char *buf = g_strdup(cmd.utf8_str().data());
        handle_command(m_serv->server_session, buf, FALSE);
        g_free(buf);
    }
}

void ChannelListDialog::OnSaveList(wxCommandEvent &event)
{
    if (m_list->GetItemCount() == 0) return;

    wxFileDialog dlg(this, wxT("Save Channel List"),
                     wxEmptyString, wxT("chanlist.txt"),
                     wxT("Text files (*.txt)|*.txt|All files (*.*)|*.*"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    wxFile file(dlg.GetPath(), wxFile::write);
    if (!file.IsOpened()) return;

    time_t t = time(nullptr);
    wxString header = wxString::Format(wxT("PChat Channel List: %s - %s\n"),
                                       wxString::FromUTF8(m_serv->servername),
                                       wxString::FromUTF8(ctime(&t)));
    file.Write(header);

    for (long i = 0; i < m_list->GetItemCount(); i++) {
        wxString chan = m_list->GetItemText(i, 0);
        wxString users = m_list->GetItemText(i, 1);
        wxString topic = m_list->GetItemText(i, 2);
        wxString line = wxString::Format(wxT("%-16s %-5s %s\n"),
                                         chan, users, topic);
        file.Write(line);
    }
}

void ChannelListDialog::OnMinUsersChanged(wxSpinEvent &event)
{
    prefs.pchat_gui_chanlist_minusers = m_min_spin->GetValue();
    save_config();
}

void ChannelListDialog::OnMaxUsersChanged(wxSpinEvent &event)
{
    prefs.pchat_gui_chanlist_maxusers = m_max_spin->GetValue();
    save_config();
}

void ChannelListDialog::OnItemActivated(wxListEvent &event)
{
    wxCommandEvent dummy;
    OnJoin(dummy);
}

void ChannelListDialog::OnRightClick(wxListEvent &event)
{
    m_ctx_item = event.GetIndex();
    if (m_ctx_item < 0) return;

    wxMenu menu;
    menu.Append(ID_CHANLIST_JOIN_MENU, wxT("Join Channel"));
    menu.Append(ID_CHANLIST_COPY_CHAN, wxT("Copy Channel Name"));
    menu.Append(ID_CHANLIST_COPY_TOPIC, wxT("Copy Topic Text"));
    PopupMenu(&menu);
}

void ChannelListDialog::OnJoinMenu(wxCommandEvent &event)
{
    if (m_ctx_item < 0) return;
    m_list->SetItemState(m_ctx_item, wxLIST_STATE_SELECTED,
                         wxLIST_STATE_SELECTED);
    OnJoin(event);
}

void ChannelListDialog::OnCopyChannel(wxCommandEvent &event)
{
    if (m_ctx_item < 0 || m_ctx_item >= m_list->GetItemCount()) return;
    wxString chan = m_list->GetItemText(m_ctx_item, 0);
    if (!chan.IsEmpty() && wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(chan));
        wxTheClipboard->Close();
    }
}

void ChannelListDialog::OnCopyTopic(wxCommandEvent &event)
{
    if (m_ctx_item < 0 || m_ctx_item >= m_list->GetItemCount()) return;
    wxString topic = m_list->GetItemText(m_ctx_item, 2);
    if (!topic.IsEmpty() && wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(topic));
        wxTheClipboard->Close();
    }
}

void ChannelListDialog::OnTimer(wxTimerEvent &event)
{
    /* Periodic caption update */
    UpdateCaption();
}

void ChannelListDialog::OnCloseWindow(wxCloseEvent &event)
{
    m_flush_timer.Stop();
    if (is_server(m_serv))
        m_serv->gui->chanlist_window = nullptr;
    Destroy();
}
