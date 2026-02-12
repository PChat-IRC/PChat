/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Friends / Notify List Dialog
 */

#include "FriendsListDialog.h"
#include "DarkMode.h"
#include "fe-wx.h"

#include <glib.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/notify.h"
#include "../common/servlist.h"
#include "../common/outbound.h"
}

wxBEGIN_EVENT_TABLE(FriendsListDialog, wxDialog)
    EVT_BUTTON(ID_FRIEND_ADD, FriendsListDialog::OnAdd)
    EVT_BUTTON(ID_FRIEND_REMOVE, FriendsListDialog::OnRemove)
    EVT_BUTTON(ID_FRIEND_DIALOG, FriendsListDialog::OnOpenDialog)
    EVT_LIST_ITEM_SELECTED(wxID_ANY, FriendsListDialog::OnSelectionChanged)
    EVT_LIST_ITEM_DESELECTED(wxID_ANY, FriendsListDialog::OnSelectionChanged)
    EVT_CLOSE(FriendsListDialog::OnCloseWindow)
wxEND_EVENT_TABLE()

FriendsListDialog::FriendsListDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, wxString(DISPLAY_NAME ": Friends List"),
               wxDefaultPosition, wxSize(540, 360),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* Friends list control */
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, wxT("Name"),      wxLIST_FORMAT_LEFT, 200);
    m_list->InsertColumn(1, wxT("Status"),    wxLIST_FORMAT_LEFT,  70);
    m_list->InsertColumn(2, wxT("Network"),   wxLIST_FORMAT_LEFT,  90);
    m_list->InsertColumn(3, wxT("Last Seen"), wxLIST_FORMAT_LEFT, 120);

    mainSizer->Add(m_list, 1, wxEXPAND | wxALL, 3);

    /* Buttons — evenly spread, no Close */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton *btnAdd = new wxButton(this, ID_FRIEND_ADD, wxT("Add..."));
    m_btn_remove = new wxButton(this, ID_FRIEND_REMOVE, wxT("Remove"));
    m_btn_dialog = new wxButton(this, ID_FRIEND_DIALOG, wxT("Open Dialog"));

    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnAdd, 0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_remove, 0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_dialog, 0);
    btnSizer->AddStretchSpacer();

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
    SetMinSize(wxSize(400, 250));
    Centre();

    /* Start with Remove/Open Dialog disabled */
    m_btn_remove->Enable(false);
    m_btn_dialog->Enable(false);

    PopulateList();

    wx_darkmode_apply_to_window(this);
}

FriendsListDialog::~FriendsListDialog()
{
}

void FriendsListDialog::RefreshList()
{
    PopulateList();
}

void FriendsListDialog::PopulateList()
{
    m_list->DeleteAllItems();

    GSList *list = notify_list;
    int idx = 0;
    while (list) {
        struct notify *n = (struct notify *)list->data;
        m_list->InsertItem(idx, wxString::FromUTF8(n->name));

        /* Check if online */
        bool online = FALSE;
        GSList *nsl = n->server_list;
        wxString network_str;
        wxString lastseen_str;
        while (nsl) {
            struct notify_per_server *nps =
                (struct notify_per_server *)nsl->data;
            if (nps->ison) online = TRUE;
            if (nps->server && nps->server->network) {
                if (!network_str.IsEmpty()) network_str += wxT(", ");
                network_str += wxString::FromUTF8(
                    ((ircnet *)nps->server->network)->name);
            }
            if (nps->lastseen != 0) {
                char buf[64];
                struct tm *tm_info = localtime(&nps->lastseen);
                if (tm_info) {
                    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm_info);
                    lastseen_str = wxString::FromUTF8(buf);
                }
            }
            nsl = nsl->next;
        }

        m_list->SetItem(idx, 1, online ? wxT("Online") : wxT("Offline"));
        m_list->SetItem(idx, 2, network_str);
        m_list->SetItem(idx, 3, lastseen_str);

        /* Color online entries */
        if (online) {
            m_list->SetItemTextColour(idx, wx_darkmode_friend_online());
        }

        idx++;
        list = list->next;
    }
}

void FriendsListDialog::OnAdd(wxCommandEvent &event)
{
    wxTextEntryDialog dlg(this, wxT("Enter nick name to add:"),
                           wxT("Add Friend"));
    if (dlg.ShowModal() == wxID_OK) {
        wxString nick = dlg.GetValue().Trim();
        if (!nick.IsEmpty()) {
            char *name = g_strdup(nick.utf8_str().data());
            notify_adduser(name, nullptr);
            g_free(name);
            PopulateList();
        }
    }
}

void FriendsListDialog::OnRemove(wxCommandEvent &event)
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    wxString nick = m_list->GetItemText(sel);
    char *name = g_strdup(nick.utf8_str().data());
    notify_deluser(name);
    g_free(name);
    PopulateList();
}

void FriendsListDialog::OnOpenDialog(wxCommandEvent &event)
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    wxString nick = m_list->GetItemText(sel);
    /* Open a query/dialog with the selected friend */
    if (current_sess) {
        wxString cmd = wxT("/query ") + nick;
        char *buf = g_strdup(cmd.utf8_str().data());
        handle_command(current_sess, buf, FALSE);
        g_free(buf);
    }
}

void FriendsListDialog::UpdateButtonSensitivity()
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    bool has_sel = (sel != -1);

    m_btn_remove->Enable(has_sel);

    /* Open Dialog only enabled if selected friend is online */
    bool can_open = false;
    if (has_sel) {
        wxString status = m_list->GetItemText(sel, 1);
        if (status == wxT("Online"))
            can_open = true;
    }
    m_btn_dialog->Enable(can_open);
}

void FriendsListDialog::OnSelectionChanged(wxListEvent &)
{
    UpdateButtonSensitivity();
}

void FriendsListDialog::OnCloseWindow(wxCloseEvent &event)
{
    notify_save();
    Hide();
}
