/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Raw Log Dialog - shows raw IRC traffic
 */

#ifndef PCHAT_RAWLOGDIALOG_H
#define PCHAT_RAWLOGDIALOG_H

#include <wx/wx.h>

#include <glib.h>

extern "C" {
#include "../common/pchat.h"
}

class RawLogDialog : public wxDialog
{
public:
    RawLogDialog(wxWindow *parent, struct server *serv);
    ~RawLogDialog();

    void AppendText(const wxString &text, bool outbound);

private:
    void OnClose(wxCloseEvent &event);
    void OnClearLog(wxCommandEvent &event);
    void OnSaveAs(wxCommandEvent &event);

    wxTextCtrl *m_text;
    struct server *m_serv;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_RAWLOG_CLEAR = wxID_HIGHEST + 300,
    ID_RAWLOG_SAVE,
};

#endif /* PCHAT_RAWLOGDIALOG_H */
