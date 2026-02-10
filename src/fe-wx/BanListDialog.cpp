/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Ban List Dialog - displays bans, exempts, invites, quiets
 * Replicates fe-gtk3/banlist.c functionality
 */

#include "BanListDialog.h"
#include "fe-wx.h"

#include <wx/clipbrd.h>
#include <glib.h>
#include <ctime>
#include <cstring>
#include <vector>

#include "../common/pchat.h"
#include "../common/pchatc.h"
extern "C" {
#include "../common/modes.h"
#include "../common/outbound.h"
#include "../common/fe.h"
}

/* Static mode info table — mirrors GTK3 banlist.c modes[] */
const BanModeInfo BanListDialog::s_modes[MODE_CT] = {
    { "Bans",    "Ban",    'b', RPL_BANLIST,    RPL_ENDOFBANLIST,    1 << MODE_BAN    },
    { "Exempts", "Exempt", 'e', RPL_EXCEPTLIST, RPL_ENDOFEXCEPTLIST, 1 << MODE_EXEMPT },
    { "Invites", "Invite", 'I', RPL_INVITELIST, RPL_ENDOFINVITELIST, 1 << MODE_INVITE },
    { "Quiets",  "Quiet",  'q', RPL_QUIETLIST,  RPL_ENDOFQUIETLIST,  1 << MODE_QUIET  },
};

wxBEGIN_EVENT_TABLE(BanListDialog, wxDialog)
    EVT_BUTTON(ID_BANLIST_REFRESH, BanListDialog::OnRefresh)
    EVT_BUTTON(ID_BANLIST_REMOVE, BanListDialog::OnRemove)
    EVT_BUTTON(ID_BANLIST_CROP, BanListDialog::OnCrop)
    EVT_BUTTON(ID_BANLIST_CLEAR, BanListDialog::OnClear)
    EVT_DATAVIEW_SELECTION_CHANGED(wxID_ANY, BanListDialog::OnSelectionChanged)
    EVT_DATAVIEW_ITEM_CONTEXT_MENU(wxID_ANY, BanListDialog::OnRightClick)
    EVT_MENU(ID_BANLIST_COPY_MASK, BanListDialog::OnCopyMask)
    EVT_MENU(ID_BANLIST_COPY_ENTRY, BanListDialog::OnCopyEntry)
    EVT_CLOSE(BanListDialog::OnCloseWindow)
wxEND_EVENT_TABLE()

BanListDialog::BanListDialog(wxWindow *parent, struct session *sess)
    : wxDialog(parent, wxID_ANY,
              wxString::Format(wxT(DISPLAY_NAME ": Ban List (%s)"),
                               wxString::FromUTF8(sess->server->servername)),
              wxDefaultPosition, wxSize(620, 400),
              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX),
      m_sess(sess),
      m_capable(0), m_readable(0), m_writeable(0),
      m_checked(0), m_pending(0),
      m_line_ct(0), m_select_ct(0), m_ctx_row(-1)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* ---- Data-view list with columns: Type, Mask, From, Date ---- */
    m_list = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                     wxDefaultSize,
                                     wxDV_MULTIPLE | wxDV_ROW_LINES);

    m_list->AppendTextColumn(wxT("Type"), wxDATAVIEW_CELL_INERT, 70,
                              wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(wxT("Mask"), wxDATAVIEW_CELL_INERT, 200,
                              wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(wxT("From"), wxDATAVIEW_CELL_INERT, 160,
                              wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(wxT("Date"), wxDATAVIEW_CELL_INERT, 160,
                              wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);

    mainSizer->Add(m_list, 1, wxEXPAND | wxALL, 3);

    /* ---- Initialize mode capabilities and default to bans ---- */
    InitModeCapabilities();
    m_checked = 1 << MODE_BAN;

    /* ---- Buttons: Remove, Crop, Clear, Refresh (evenly spread) ---- */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_btn_remove  = new wxButton(this, ID_BANLIST_REMOVE,  wxT("Remove"));
    m_btn_crop    = new wxButton(this, ID_BANLIST_CROP,    wxT("Crop"));
    m_btn_clear   = new wxButton(this, ID_BANLIST_CLEAR,   wxT("Clear"));
    m_btn_refresh = new wxButton(this, ID_BANLIST_REFRESH, wxT("Refresh"));

    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_remove, 0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_crop, 0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_clear, 0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_refresh, 0);
    btnSizer->AddStretchSpacer();

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
    SetMinSize(wxSize(500, 250));
    Centre();

    /* Initial refresh */
    DoRefresh();
}

BanListDialog::~BanListDialog()
{
    if (m_sess && m_sess->res)
        m_sess->res->banlist = nullptr;
}

void BanListDialog::InitModeCapabilities()
{
    struct server *serv = m_sess->server;

    /* Bans are always capable */
    m_capable  |= (1 << MODE_BAN);
    m_readable |= (1 << MODE_BAN);
    m_writeable|= (1 << MODE_BAN);

    /* Check exempt support */
    if (serv->have_except) {
        m_capable  |= (1 << MODE_EXEMPT);
        m_writeable|= (1 << MODE_EXEMPT);
    } else if (serv->chanmodes) {
        for (char *cm = serv->chanmodes; *cm && *cm != ','; cm++) {
            if (*cm == 'e') {
                m_capable  |= (1 << MODE_EXEMPT);
                m_writeable|= (1 << MODE_EXEMPT);
                break;
            }
        }
    }

    /* Check invite support */
    if (serv->have_invite) {
        m_capable  |= (1 << MODE_INVITE);
        m_writeable|= (1 << MODE_INVITE);
    } else if (serv->chanmodes) {
        for (char *cm = serv->chanmodes; *cm && *cm != ','; cm++) {
            if (*cm == 'I') {
                m_capable  |= (1 << MODE_INVITE);
                m_writeable|= (1 << MODE_INVITE);
                break;
            }
        }
    }

    /* Check quiet support */
    if (serv->chanmodes) {
        for (char *cm = serv->chanmodes; *cm && *cm != ','; cm++) {
            if (*cm == 'q') {
                m_capable   |= (1 << MODE_QUIET);
                m_readable  |= (1 << MODE_QUIET);
                m_writeable |= (1 << MODE_QUIET);
                break;
            }
        }
    }
}

int BanListDialog::GetSelectedCount()
{
    wxDataViewItemArray sel;
    return m_list->GetSelections(sel);
}

void BanListDialog::UpdateSensitivity()
{
    bool is_op = false;
    if (m_sess->me && (m_sess->me->op || m_sess->me->hop))
        is_op = true;

    /* Checkbox sensitivity — not used, kept for future reference */
    /* Button sensitivity — matches GTK3 banlist_sensitize() */
    if (!is_op || m_line_ct == 0) {
        m_btn_clear->Enable(false);
        m_btn_crop->Enable(false);
        m_btn_remove->Enable(false);
    } else {
        if (m_select_ct == 0) {
            m_btn_clear->Enable(true);
            m_btn_crop->Enable(false);
            m_btn_remove->Enable(false);
        } else {
            m_btn_clear->Enable(false);
            m_btn_crop->Enable(m_line_ct != m_select_ct);
            m_btn_remove->Enable(true);
        }
    }

    m_btn_refresh->Enable(!m_pending && m_checked);
}

void BanListDialog::DoRefresh()
{
    if (!m_sess->server->connected) {
        wxMessageBox(wxT("Not connected."), wxT("Error"),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    /* Update title with channel and server */
    SetTitle(wxString::Format(wxT(DISPLAY_NAME ": Ban List (%s, %s)"),
             wxString::FromUTF8(m_sess->channel),
             wxString::FromUTF8(m_sess->server->servername)));

    m_list->DeleteAllItems();
    m_line_ct = 0;
    m_select_ct = 0;
    m_pending = m_checked;

    UpdateSensitivity();

    if (m_pending) {
        char tbuf[256];
        for (int i = 0; i < MODE_CT; i++) {
            if (m_pending & (1 << i)) {
                g_snprintf(tbuf, sizeof tbuf, "quote mode %s +%c",
                           m_sess->channel, s_modes[i].letter);
                handle_command(m_sess, tbuf, FALSE);
            }
        }
    }
}

bool BanListDialog::AddEntry(const char *mask, const char *who,
                              const char *when, int rplcode)
{
    int mode_idx = -1;
    for (int i = 0; i < MODE_CT; i++) {
        if (s_modes[i].code == rplcode) {
            mode_idx = i;
            break;
        }
    }
    if (mode_idx < 0) return false;
    if (!(m_pending & (1 << mode_idx))) return false;

    wxVector<wxVariant> row;
    row.push_back(wxVariant(wxString::FromUTF8(s_modes[mode_idx].type)));
    row.push_back(wxVariant(wxString::FromUTF8(mask)));
    row.push_back(wxVariant(wxString::FromUTF8(who)));
    row.push_back(wxVariant(wxString::FromUTF8(when)));
    m_list->AppendItem(row);

    m_line_ct++;
    return true;
}

bool BanListDialog::ListEnd(int rplcode)
{
    int mode_idx = -1;
    for (int i = 0; i < MODE_CT; i++) {
        if (s_modes[i].endcode == rplcode) {
            mode_idx = i;
            break;
        }
    }
    if (mode_idx < 0) return false;
    if (!(m_pending & s_modes[mode_idx].bit)) return false;

    m_pending &= ~s_modes[mode_idx].bit;
    if (!m_pending) {
        m_btn_refresh->Enable(true);
        UpdateSensitivity();
    }
    return true;
}

void BanListDialog::OnRefresh(wxCommandEvent &)
{
    DoRefresh();
}

void BanListDialog::OnRemove(wxCommandEvent &event)
{
    char tbuf[2048];
    int total_removed = 0;

    wxDataViewItemArray sel;
    m_list->GetSelections(sel);

    for (int mode_num = 0; mode_num < MODE_CT; mode_num++) {
        std::vector<wxString> masks;

        for (const auto &item : sel) {
            int row = m_list->ItemToRow(item);
            if (row < 0) continue;
            wxString type = m_list->GetTextValue(row, COL_TYPE);
            if (type == wxString::FromUTF8(s_modes[mode_num].type))
                masks.push_back(m_list->GetTextValue(row, COL_MASK));
        }

        if (!masks.empty()) {
            char **mask_arr = g_new(char *, masks.size());
            for (size_t i = 0; i < masks.size(); i++)
                mask_arr[i] = g_strdup(masks[i].utf8_str().data());

            send_channel_modes(m_sess, tbuf, mask_arr, 0,
                               (int)masks.size(), '-',
                               s_modes[mode_num].letter, 0);

            for (size_t i = 0; i < masks.size(); i++)
                g_free(mask_arr[i]);
            g_free(mask_arr);

            total_removed += (int)masks.size();
        }
    }

    if (total_removed < 1) {
        wxMessageBox(wxT("You must select some bans."), wxT("Error"),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    DoRefresh();
}

void BanListDialog::OnCrop(wxCommandEvent &event)
{
    wxDataViewItemArray sel;
    m_list->GetSelections(sel);

    if (sel.empty()) {
        wxMessageBox(wxT("You must select some bans."), wxT("Error"),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    /* Build set of selected rows */
    std::vector<bool> was_selected(m_list->GetItemCount(), false);
    for (const auto &item : sel) {
        int row = m_list->ItemToRow(item);
        if (row >= 0)
            was_selected[row] = true;
    }

    /* Invert selection: select all except those originally selected */
    m_list->UnselectAll();
    for (int r = 0; r < (int)m_list->GetItemCount(); r++) {
        if (!was_selected[r])
            m_list->SelectRow(r);
    }

    /* Remove the inverted selection */
    OnRemove(event);
}

void BanListDialog::OnClear(wxCommandEvent &event)
{
    int answer = wxMessageBox(
        wxString::Format(wxT("Are you sure you want to remove all listed items in %s?"),
                         wxString::FromUTF8(m_sess->channel)),
        wxT("Clear Bans"), wxOK | wxCANCEL | wxICON_QUESTION, this);

    if (answer == wxOK) {
        /* Select all and remove */
        for (int r = 0; r < (int)m_list->GetItemCount(); r++)
            m_list->SelectRow(r);
        OnRemove(event);
    }
}


void BanListDialog::OnSelectionChanged(wxDataViewEvent &)
{
    m_select_ct = GetSelectedCount();
    UpdateSensitivity();
}

void BanListDialog::OnRightClick(wxDataViewEvent &event)
{
    wxDataViewItem item = event.GetItem();
    if (!item.IsOk()) return;
    m_ctx_row = m_list->ItemToRow(item);
    if (m_ctx_row < 0) return;

    wxMenu menu;
    menu.Append(ID_BANLIST_COPY_MASK, wxT("Copy mask"));
    menu.Append(ID_BANLIST_COPY_ENTRY, wxT("Copy entry"));
    PopupMenu(&menu);
}

void BanListDialog::OnCopyMask(wxCommandEvent &)
{
    if (m_ctx_row < 0 || m_ctx_row >= (int)m_list->GetItemCount()) return;

    wxString mask = m_list->GetTextValue(m_ctx_row, COL_MASK);
    if (!mask.IsEmpty() && wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(mask));
        wxTheClipboard->Close();
    }
}

void BanListDialog::OnCopyEntry(wxCommandEvent &)
{
    if (m_ctx_row < 0 || m_ctx_row >= (int)m_list->GetItemCount()) return;

    wxString mask = m_list->GetTextValue(m_ctx_row, COL_MASK);
    wxString from = m_list->GetTextValue(m_ctx_row, COL_FROM);
    wxString date = m_list->GetTextValue(m_ctx_row, COL_DATE);
    wxString entry = wxString::Format(wxT("%s on %s by %s"), mask, date, from);

    if (!entry.IsEmpty() && wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(entry));
        wxTheClipboard->Close();
    }
}

void BanListDialog::OnCloseWindow(wxCloseEvent &)
{
    if (m_sess && m_sess->res)
        m_sess->res->banlist = nullptr;
    Destroy();
}
