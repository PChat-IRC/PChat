/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * URL Grabber Dialog
 */

#ifndef PCHAT_URLGRABBERDIALOG_H
#define PCHAT_URLGRABBERDIALOG_H

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/file.h>
#include <wx/clipbrd.h>

class UrlGrabberDialog : public wxDialog
{
public:
    UrlGrabberDialog(wxWindow *parent);
    ~UrlGrabberDialog();

    void RefreshList();

private:
    void PopulateList();
    static int UrlTreeCallback(const void *key, void *userdata);
    void OnCloseWindow(wxCloseEvent &event);
    void OnClear(wxCommandEvent &event);
    void OnCopy(wxCommandEvent &event);
    void OnSaveAs(wxCommandEvent &event);
    void OnDoubleClick(wxCommandEvent &event);

    wxListBox *m_url_list;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_URL_CLEAR = wxID_HIGHEST + 500,
    ID_URL_COPY,
    ID_URL_SAVE,
    ID_URL_LIST,
};

#endif /* PCHAT_URLGRABBERDIALOG_H */
