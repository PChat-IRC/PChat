/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Ignore List Dialog - with click-to-toggle flags and inline mask editing
 */

#include "IgnoreListDialog.h"
#include "DarkMode.h"

#include <glib.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/ignore.h"
#include "../common/cfgfiles.h"
}

wxBEGIN_EVENT_TABLE(IgnoreListDialog, wxDialog)
    EVT_BUTTON(ID_IGN_ADD, IgnoreListDialog::OnAdd)
    EVT_BUTTON(ID_IGN_DELETE, IgnoreListDialog::OnDelete)
    EVT_BUTTON(ID_IGN_CLEAR, IgnoreListDialog::OnClear)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, IgnoreListDialog::OnItemActivated)
    EVT_CLOSE(IgnoreListDialog::OnCloseWindow)
wxEND_EVENT_TABLE()

IgnoreListDialog::IgnoreListDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, wxT("Ignore List - PChat"),
               wxDefaultPosition, wxSize(680, 480),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* Hint label */
    mainSizer->Add(new wxStaticText(this, wxID_ANY,
        wxT("Click on checkmark columns to toggle. Double-click mask to edit.")),
        0, wxLEFT | wxTOP, 4);

    /* Ignore list */
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, wxT("Mask"), wxLIST_FORMAT_LEFT, 180);
    m_list->InsertColumn(1, wxT("Channel"), wxLIST_FORMAT_CENTER, 60);
    m_list->InsertColumn(2, wxT("Private"), wxLIST_FORMAT_CENTER, 55);
    m_list->InsertColumn(3, wxT("Notice"), wxLIST_FORMAT_CENTER, 55);
    m_list->InsertColumn(4, wxT("CTCP"), wxLIST_FORMAT_CENTER, 50);
    m_list->InsertColumn(5, wxT("DCC"), wxLIST_FORMAT_CENTER, 45);
    m_list->InsertColumn(6, wxT("Invite"), wxLIST_FORMAT_CENTER, 50);
    m_list->InsertColumn(7, wxT("Unignore"), wxLIST_FORMAT_CENTER, 65);

    /* Bind left-click for column toggling */
    m_list->Bind(wxEVT_LIST_COL_CLICK, [](wxListEvent &e){});
    m_list->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &evt) {
        int flags;
        long item = m_list->HitTest(evt.GetPosition(), flags);
        if (item < 0) { evt.Skip(); return; }

        /* Figure out which column was clicked by x position */
        int x = evt.GetPosition().x;
        /* Account for horizontal scroll */
        int scrollx = m_list->GetScrollPos(wxHORIZONTAL);

        int col = -1;
        int accum = 0;
        for (int c = 0; c < m_list->GetColumnCount(); c++) {
            int w = m_list->GetColumnWidth(c);
            if (x < accum + w) {
                col = c;
                break;
            }
            accum += w;
        }

        /* Columns 1-7 are toggle flags */
        if (col >= 1 && col <= 7) {
            /* Find the ignore entry at this index */
            int idx = 0;
            GSList *list = ignore_list;
            struct ignore *ig = nullptr;
            while (list) {
                if (idx == item) {
                    ig = (struct ignore *)list->data;
                    break;
                }
                idx++;
                list = list->next;
            }
            if (!ig) { evt.Skip(); return; }

            /* Map column to flag */
            static const int col_flags[] = {
                0, IG_CHAN, IG_PRIV, IG_NOTI, IG_CTCP,
                IG_DCC, IG_INVI, IG_UNIG
            };
            int flag = col_flags[col];

            /* Toggle the flag */
            ig->type ^= flag;

            /* Update display */
            m_list->SetItem(item, col,
                (ig->type & flag) ? wxT("\u2713") : wxT(""));

            SaveIgnoreList();
            UpdateStats();
        }

        evt.Skip();
    });

    mainSizer->Add(m_list, 1, wxEXPAND | wxALL, 4);

    /* Ignore Stats */
    wxStaticBoxSizer *statsBox = new wxStaticBoxSizer(wxHORIZONTAL, this,
                                                       wxT("Ignore Stats"));

    m_stat_channel = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY, wxT("Channel: 0"));
    m_stat_private = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY, wxT("Private: 0"));
    m_stat_notice = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY, wxT("Notice: 0"));
    m_stat_ctcp = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY, wxT("CTCP: 0"));
    m_stat_invite = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY, wxT("Invite: 0"));

    statsBox->Add(m_stat_channel, 1, wxALL, 4);
    statsBox->Add(m_stat_private, 1, wxALL, 4);
    statsBox->Add(m_stat_notice, 1, wxALL, 4);
    statsBox->Add(m_stat_ctcp, 1, wxALL, 4);
    statsBox->Add(m_stat_invite, 1, wxALL, 4);

    mainSizer->Add(statsBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);

    /* Buttons */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(this, ID_IGN_ADD, wxT("Add...")),
                  0, wxRIGHT, 4);
    btnSizer->Add(new wxButton(this, ID_IGN_DELETE, wxT("Delete")),
                  0, wxRIGHT, 4);
    btnSizer->Add(new wxButton(this, ID_IGN_CLEAR, wxT("Clear")),
                  0, wxRIGHT, 4);

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 4);

    SetSizer(mainSizer);
    Centre();

    PopulateList();
    UpdateStats();

    wx_darkmode_apply_to_window(this);
}

IgnoreListDialog::~IgnoreListDialog()
{
}

void IgnoreListDialog::PopulateList()
{
    m_list->DeleteAllItems();

    GSList *list = ignore_list;
    int idx = 0;
    while (list) {
        struct ignore *ig = (struct ignore *)list->data;
        m_list->InsertItem(idx, wxString::FromUTF8(ig->mask));

        m_list->SetItem(idx, 1,
            (ig->type & IG_CHAN) ? wxT("\u2713") : wxT(""));
        m_list->SetItem(idx, 2,
            (ig->type & IG_PRIV) ? wxT("\u2713") : wxT(""));
        m_list->SetItem(idx, 3,
            (ig->type & IG_NOTI) ? wxT("\u2713") : wxT(""));
        m_list->SetItem(idx, 4,
            (ig->type & IG_CTCP) ? wxT("\u2713") : wxT(""));
        m_list->SetItem(idx, 5,
            (ig->type & IG_DCC) ? wxT("\u2713") : wxT(""));
        m_list->SetItem(idx, 6,
            (ig->type & IG_INVI) ? wxT("\u2713") : wxT(""));
        m_list->SetItem(idx, 7,
            (ig->type & IG_UNIG) ? wxT("\u2713") : wxT(""));

        idx++;
        list = list->next;
    }
}

void IgnoreListDialog::RefreshList()
{
    PopulateList();
}

void IgnoreListDialog::SaveIgnoreList()
{
    ignore_save();
}

void IgnoreListDialog::UpdateStats()
{
    int chan = 0, priv = 0, noti = 0, ctcp = 0, invi = 0;

    GSList *list = ignore_list;
    while (list) {
        struct ignore *ig = (struct ignore *)list->data;
        if (ig->type & IG_CHAN) chan++;
        if (ig->type & IG_PRIV) priv++;
        if (ig->type & IG_NOTI) noti++;
        if (ig->type & IG_CTCP) ctcp++;
        if (ig->type & IG_INVI) invi++;
        list = list->next;
    }

    m_stat_channel->SetLabel(wxString::Format(wxT("Channel: %d"), chan));
    m_stat_private->SetLabel(wxString::Format(wxT("Private: %d"), priv));
    m_stat_notice->SetLabel(wxString::Format(wxT("Notice: %d"), noti));
    m_stat_ctcp->SetLabel(wxString::Format(wxT("CTCP: %d"), ctcp));
    m_stat_invite->SetLabel(wxString::Format(wxT("Invite: %d"), invi));
}

void IgnoreListDialog::OnAdd(wxCommandEvent &event)
{
    wxTextEntryDialog dlg(this,
                           wxT("Enter mask to ignore (e.g. nick!*@*.host):"),
                           wxT("Add Ignore Mask"));
    if (dlg.ShowModal() == wxID_OK) {
        wxString mask = dlg.GetValue().Trim();
        if (!mask.IsEmpty()) {
            /* Default: ignore all types */
            int type = IG_CHAN | IG_PRIV | IG_NOTI | IG_CTCP | IG_DCC |
                       IG_INVI;
            char *m = g_strdup(mask.utf8_str().data());
            ignore_add(m, type, TRUE);
            g_free(m);
            PopulateList();
            UpdateStats();
        }
    }
}

void IgnoreListDialog::OnDelete(wxCommandEvent &event)
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    wxString mask = m_list->GetItemText(sel);
    char *m = g_strdup(mask.utf8_str().data());
    ignore_del(m, nullptr);
    g_free(m);
    PopulateList();
    UpdateStats();
}

void IgnoreListDialog::OnClear(wxCommandEvent &event)
{
    int answer = wxMessageBox(
        wxT("Are you sure you want to clear the entire ignore list?"),
        wxT("Clear Ignore List"),
        wxYES_NO | wxICON_QUESTION, this);

    if (answer == wxYES) {
        while (ignore_list) {
            struct ignore *ig = (struct ignore *)ignore_list->data;
            ignore_del(ig->mask, nullptr);
        }
        PopulateList();
        UpdateStats();
    }
}

void IgnoreListDialog::OnItemActivated(wxListEvent &event)
{
    /* Double-click on a row: edit the mask */
    long item = event.GetIndex();
    if (item < 0) return;

    wxString oldMask = m_list->GetItemText(item);

    /* Find the ignore entry */
    int idx = 0;
    GSList *list = ignore_list;
    struct ignore *ig = nullptr;
    while (list) {
        if (idx == item) {
            ig = (struct ignore *)list->data;
            break;
        }
        idx++;
        list = list->next;
    }
    if (!ig) return;

    wxTextEntryDialog dlg(this, wxT("Edit ignore mask:"),
                           wxT("Edit Mask"), oldMask);
    if (dlg.ShowModal() == wxID_OK) {
        wxString newMask = dlg.GetValue().Trim();
        if (!newMask.IsEmpty() && newMask != oldMask) {
            int oldType = ig->type;
            /* Remove old, add new with same flags */
            char *om = g_strdup(oldMask.utf8_str().data());
            ignore_del(om, nullptr);
            g_free(om);

            char *nm = g_strdup(newMask.utf8_str().data());
            ignore_add(nm, oldType, TRUE);
            g_free(nm);

            PopulateList();
            UpdateStats();
        }
    }
}

void IgnoreListDialog::OnItemClicked(wxListEvent &event)
{
    /* Not used directly - click toggling handled via wxEVT_LEFT_UP bind */
    event.Skip();
}

void IgnoreListDialog::OnCloseWindow(wxCloseEvent &event)
{
    Hide();
}
