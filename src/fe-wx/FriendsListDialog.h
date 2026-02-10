/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Friends List (Notify) Dialog
 */

#ifndef PCHAT_FRIENDSLISTDIALOG_H
#define PCHAT_FRIENDSLISTDIALOG_H

#include <wx/wx.h>
#include <wx/listctrl.h>

class FriendsListDialog : public wxDialog
{
public:
    FriendsListDialog(wxWindow *parent);
    ~FriendsListDialog();

    void RefreshList();

private:
    void PopulateList();
    void UpdateButtonSensitivity();
    void OnCloseWindow(wxCloseEvent &event);
    void OnAdd(wxCommandEvent &event);
    void OnRemove(wxCommandEvent &event);
    void OnOpenDialog(wxCommandEvent &event);
    void OnSelectionChanged(wxListEvent &event);

    wxListCtrl *m_list;
    wxButton *m_btn_remove;
    wxButton *m_btn_dialog;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_FRIEND_ADD = wxID_HIGHEST + 600,
    ID_FRIEND_REMOVE,
    ID_FRIEND_DIALOG,
};

#endif /* PCHAT_FRIENDSLISTDIALOG_H */
