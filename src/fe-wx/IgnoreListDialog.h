/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Ignore List Dialog - with toggle checkboxes and inline mask editing
 */

#ifndef PCHAT_IGNORELISTDIALOG_H
#define PCHAT_IGNORELISTDIALOG_H

#include <wx/wx.h>
#include <wx/listctrl.h>

class IgnoreListDialog : public wxDialog
{
public:
    IgnoreListDialog(wxWindow *parent);
    ~IgnoreListDialog();

    void RefreshList();
    void UpdateStats();

private:
    void PopulateList();
    void SaveIgnoreList();
    void OnCloseWindow(wxCloseEvent &event);
    void OnAdd(wxCommandEvent &event);
    void OnDelete(wxCommandEvent &event);
    void OnClear(wxCommandEvent &event);
    void OnItemClicked(wxListEvent &event);
    void OnItemActivated(wxListEvent &event);

    wxListCtrl *m_list;

    /* Ignore Stats */
    wxStaticText *m_stat_channel;
    wxStaticText *m_stat_private;
    wxStaticText *m_stat_notice;
    wxStaticText *m_stat_ctcp;
    wxStaticText *m_stat_invite;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_IGN_ADD = wxID_HIGHEST + 700,
    ID_IGN_DELETE,
    ID_IGN_CLEAR,
};

#endif /* PCHAT_IGNORELISTDIALOG_H */
