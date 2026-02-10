/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * DCC Dialog - File transfers and DCC Chat list
 */

#ifndef PCHAT_DCCDIALOG_H
#define PCHAT_DCCDIALOG_H

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/collpane.h>

struct DCC;

/* DCC Chat List Dialog - matches HexChat "DCC Chat List" */
class DccChatDialog : public wxDialog
{
public:
    DccChatDialog(wxWindow *parent);
    ~DccChatDialog();

    void RefreshList();

private:
    void PopulateList();
    void OnCloseWindow(wxCloseEvent &event);
    void OnAbort(wxCommandEvent &event);
    void OnAccept(wxCommandEvent &event);

    wxListCtrl *m_list;

    wxDECLARE_EVENT_TABLE();
};

/* Uploads and Downloads Dialog - matches HexChat file transfer window */
class DccTransferDialog : public wxDialog
{
public:
    DccTransferDialog(wxWindow *parent);
    ~DccTransferDialog();

    void RefreshList();

private:
    void PopulateList();
    struct DCC *GetSelectedDCC();
    void UpdateDetailLabels();
    void UpdateButtonSensitivity();
    void OnCloseWindow(wxCloseEvent &event);
    void OnAbort(wxCommandEvent &event);
    void OnAccept(wxCommandEvent &event);
    void OnResume(wxCommandEvent &event);
    void OnClear(wxCommandEvent &event);
    void OnOpenFolder(wxCommandEvent &event);
    void OnFilterChanged(wxCommandEvent &event);
    void OnSelectionChanged(wxListEvent &event);
    void OnCollapsibleChanged(wxCollapsiblePaneEvent &event);

    wxListCtrl *m_list;
    wxCollapsiblePane *m_details_pane;
    wxStaticText *m_file_label;
    wxStaticText *m_addr_label;
    wxRadioButton *m_uploads_radio;
    wxRadioButton *m_downloads_radio;
    wxRadioButton *m_both_radio;
    wxButton *m_btn_abort;
    wxButton *m_btn_accept;
    wxButton *m_btn_resume;
    wxButton *m_btn_clear;
    wxButton *m_btn_open;
    int m_filter = 3; /* VIEW_BOTH */

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_DCC_CHAT_ABORT = wxID_HIGHEST + 400,
    ID_DCC_CHAT_ACCEPT,
    ID_DCC_XFER_ABORT,
    ID_DCC_XFER_ACCEPT,
    ID_DCC_XFER_RESUME,
    ID_DCC_XFER_CLEAR,
    ID_DCC_XFER_OPEN,
    ID_DCC_XFER_UPLOADS,
    ID_DCC_XFER_DOWNLOADS,
    ID_DCC_XFER_BOTH,
    ID_DCC_XFER_DETAILS,
};

#endif /* PCHAT_DCCDIALOG_H */
