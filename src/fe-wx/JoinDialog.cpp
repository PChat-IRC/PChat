/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Join Dialog - shown after connecting when no autojoin channels exist
 * Replicates fe-gtk3/joind.c functionality
 */

#include "JoinDialog.h"
#include "fe-wx.h"
#include "MainWindow.h"
#include "DarkMode.h"

#include <wx/artprov.h>
#include <glib.h>
#include <cstring>

#include "../common/pchat.h"
#include "../common/pchatc.h"
extern "C" {
#include "../common/server.h"
#include "../common/servlist.h"
#include "../common/fe.h"
}

extern MainWindow *g_main_window;

wxBEGIN_EVENT_TABLE(JoinDialog, wxDialog)
    EVT_BUTTON(ID_JOIND_OK, JoinDialog::OnOk)
    EVT_RADIOBUTTON(ID_JOIND_RADIO_JOIN, JoinDialog::OnRadioChanged)
wxEND_EVENT_TABLE()

JoinDialog::JoinDialog(wxWindow *parent, struct server *serv)
    : wxDialog(parent, wxID_ANY,
               wxString(wxT(DISPLAY_NAME ": Connection Complete")),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE),
      m_serv(serv)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* Header with network icon */
    wxBoxSizer *headerSizer = new wxBoxSizer(wxHORIZONTAL);

    wxStaticBitmap *icon = new wxStaticBitmap(this, wxID_ANY,
                                               wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_MESSAGE_BOX));
    headerSizer->Add(icon, 0, wxALL | wxALIGN_TOP, 12);

    wxBoxSizer *textSizer = new wxBoxSizer(wxVERTICAL);

    /* "Connection to <network> complete." */
    wxString netName = wxString::FromUTF8(server_get_network(serv, TRUE));
    wxString connMsg = wxString::Format(wxT("Connection to %s complete."), netName);
    wxStaticText *boldLabel = new wxStaticText(this, wxID_ANY, connMsg);
    wxFont boldFont = boldLabel->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    boldLabel->SetFont(boldFont);
    textSizer->Add(boldLabel, 0, wxBOTTOM, 4);

    textSizer->Add(new wxStaticText(this, wxID_ANY,
        wxT("In the Server-List window, no channel (chat room) has been entered\n"
            "to be automatically joined for this network.")),
        0, wxBOTTOM, 8);

    textSizer->Add(new wxStaticText(this, wxID_ANY,
        wxT("What would you like to do next?")),
        0, wxBOTTOM, 8);

    /* Radio buttons */
    m_radio_nothing = new wxRadioButton(this, ID_JOIND_RADIO_NOTHING,
        wxT("Nothing, I'll join a channel later."),
        wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    textSizer->Add(m_radio_nothing, 0, wxBOTTOM, 4);

    /* Join channel radio + entry */
    wxBoxSizer *joinSizer = new wxBoxSizer(wxHORIZONTAL);
    m_radio_join = new wxRadioButton(this, ID_JOIND_RADIO_JOIN,
        wxT("Join this channel:"));
    joinSizer->Add(m_radio_join, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    m_channel_entry = new wxTextCtrl(this, wxID_ANY, wxT("#"),
                                      wxDefaultPosition, wxSize(200, -1));
    m_channel_entry->Bind(wxEVT_SET_FOCUS, &JoinDialog::OnEntryFocus, this);
    joinSizer->Add(m_channel_entry, 1, wxALIGN_CENTER_VERTICAL);
    textSizer->Add(joinSizer, 0, wxEXPAND | wxBOTTOM, 2);

    wxStaticText *joinHint = new wxStaticText(this, wxID_ANY,
        wxT("     If you know the name of the channel you want to join, enter it here."));
    wxFont smallFont = joinHint->GetFont();
    smallFont.SetPointSize(smallFont.GetPointSize() - 1);
    joinHint->SetFont(smallFont);
    textSizer->Add(joinHint, 0, wxBOTTOM, 4);

    m_radio_chanlist = new wxRadioButton(this, ID_JOIND_RADIO_CHANLIST,
        wxT("Open the Channel-List window."));
    textSizer->Add(m_radio_chanlist, 0, wxBOTTOM, 2);

    wxStaticText *chanlistHint = new wxStaticText(this, wxID_ANY,
        wxT("     Retrieving the Channel-List may take a minute or two."));
    chanlistHint->SetFont(smallFont);
    textSizer->Add(chanlistHint, 0, wxBOTTOM, 8);

    headerSizer->Add(textSizer, 1, wxALL, 6);
    mainSizer->Add(headerSizer, 1, wxEXPAND);

    /* "Always show this dialog" checkbox */
    m_always_show = new wxCheckBox(this, wxID_ANY,
        wxT("Always show this dialog after connecting."));
    m_always_show->SetValue(prefs.pchat_gui_join_dialog != 0);
    mainSizer->Add(m_always_show, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    /* OK button */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, ID_JOIND_OK, wxT("OK")), 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 8);

    SetSizerAndFit(mainSizer);
    CentreOnParent();

    /* Default channel for specific networks */
    if (serv->network) {
        const char *netname = ((ircnet *)serv->network)->name;
        if (netname && g_ascii_strcasecmp(netname, "freenode") == 0) {
            m_channel_entry->SetValue(wxT("#valhallalinux"));
            m_radio_join->SetValue(true);
        }
    }

    wx_darkmode_apply_to_window(this);
}

JoinDialog::~JoinDialog()
{
    if (is_server(m_serv))
        m_serv->gui->joind_window = nullptr;
}

void JoinDialog::OnOk(wxCommandEvent &event)
{
    if (!is_server(m_serv)) {
        EndModal(wxID_OK);
        return;
    }

    /* "Nothing" radio */
    if (m_radio_nothing->GetValue()) {
        /* Do nothing */
    }
    /* "Join channel" radio */
    else if (m_radio_join->GetValue()) {
        wxString chan = m_channel_entry->GetValue().Trim();
        if (chan.length() < 1) {
            wxMessageBox(wxT("Channel name too short, try again."),
                         wxT("Error"), wxOK | wxICON_ERROR, this);
            return;
        }
        char *text = g_strdup(chan.utf8_str().data());
        m_serv->p_join(m_serv, text, "");
        g_free(text);
    }
    /* "Channel list" radio */
    else if (m_radio_chanlist->GetValue()) {
        if (g_main_window) {
            g_main_window->ShowChannelList(m_serv, nullptr, TRUE);
        }
    }

    /* Update pref */
    prefs.pchat_gui_join_dialog = m_always_show->GetValue() ? 1 : 0;

    if (is_server(m_serv))
        m_serv->gui->joind_window = nullptr;
    EndModal(wxID_OK);
}

void JoinDialog::OnRadioChanged(wxCommandEvent &event)
{
    if (m_radio_join->GetValue()) {
        m_channel_entry->SetFocus();
        m_channel_entry->SetInsertionPointEnd();
    }
}

void JoinDialog::OnEntryFocus(wxFocusEvent &event)
{
    m_radio_join->SetValue(true);
    event.Skip();
}
