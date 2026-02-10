/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Join Dialog - shown after connecting when no autojoin channels exist
 * Replicates fe-gtk3/joind.c functionality
 */

#ifndef PCHAT_JOINDIALOG_H
#define PCHAT_JOINDIALOG_H

#include <wx/wx.h>

#include "../common/pchat.h"

class JoinDialog : public wxDialog
{
public:
    JoinDialog(wxWindow *parent, struct server *serv);
    ~JoinDialog();

private:
    void OnOk(wxCommandEvent &event);
    void OnRadioChanged(wxCommandEvent &event);
    void OnEntryFocus(wxFocusEvent &event);

    struct server *m_serv;

    wxRadioButton *m_radio_nothing;
    wxRadioButton *m_radio_join;
    wxRadioButton *m_radio_chanlist;
    wxTextCtrl *m_channel_entry;
    wxCheckBox *m_always_show;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_JOIND_RADIO_NOTHING = wxID_HIGHEST + 900,
    ID_JOIND_RADIO_JOIN,
    ID_JOIND_RADIO_CHANLIST,
    ID_JOIND_OK,
};

#endif /* PCHAT_JOINDIALOG_H */
