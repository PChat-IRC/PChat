/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * DCC Chat List and DCC Transfers dialogs
 */

#include "DccDialog.h"
#include "fe-wx.h"
#include "DarkMode.h"

#include <glib.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/dcc.h"
#include "../common/network.h"
}

/* ----------------------------- DccChatDialog ----------------------------- */

wxBEGIN_EVENT_TABLE(DccChatDialog, wxDialog)
    EVT_BUTTON(ID_DCC_CHAT_ABORT, DccChatDialog::OnAbort)
    EVT_BUTTON(ID_DCC_CHAT_ACCEPT, DccChatDialog::OnAccept)
    EVT_CLOSE(DccChatDialog::OnCloseWindow)
wxEND_EVENT_TABLE()

DccChatDialog::DccChatDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, wxString(DISPLAY_NAME ": DCC Chat List"),
               wxDefaultPosition, wxSize(650, 350),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* DCC chat list control — matches GTK3 columns */
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT);
    m_list->InsertColumn(0, wxT("Status"),     wxLIST_FORMAT_LEFT,  80);
    m_list->InsertColumn(1, wxT("Nick"),       wxLIST_FORMAT_LEFT, 200);
    m_list->InsertColumn(2, wxT("Recv"),       wxLIST_FORMAT_RIGHT, 70);
    m_list->InsertColumn(3, wxT("Sent"),       wxLIST_FORMAT_RIGHT, 70);
    m_list->InsertColumn(4, wxT("Start Time"), wxLIST_FORMAT_LEFT, 160);

    mainSizer->Add(m_list, 1, wxEXPAND | wxALL, 3);

    /* Buttons — evenly spread like GTK3 */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, ID_DCC_CHAT_ABORT, wxT("Abort")),
                  0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, ID_DCC_CHAT_ACCEPT, wxT("Accept")), 0);
    btnSizer->AddStretchSpacer();
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
    Centre();

    PopulateList();

    wx_darkmode_apply_to_window(this);
}

DccChatDialog::~DccChatDialog()
{
}

void DccChatDialog::RefreshList()
{
    PopulateList();
}

void DccChatDialog::PopulateList()
{
    m_list->DeleteAllItems();

    GSList *list = dcc_list;
    while (list) {
        struct DCC *dcc = (struct DCC *)list->data;
        if (dcc->type == TYPE_CHATRECV || dcc->type == TYPE_CHATSEND) {
            long idx = m_list->GetItemCount();
            m_list->InsertItem(idx, wxEmptyString);

            wxString status;
            switch (dcc->dccstat) {
                case STAT_QUEUED:   status = wxT("Queued"); break;
                case STAT_ACTIVE:   status = wxT("Active"); break;
                case STAT_FAILED:   status = wxT("Failed"); break;
                case STAT_DONE:     status = wxT("Done"); break;
                case STAT_CONNECTING: status = wxT("Connecting"); break;
                case STAT_ABORTED:  status = wxT("Aborted"); break;
                default:            status = wxT("Unknown"); break;
            }
            m_list->SetItem(idx, 0, status);
            m_list->SetItem(idx, 1, wxString::FromUTF8(dcc->nick));
            m_list->SetItem(idx, 2, wxString::Format(wxT("%u"), dcc->pos));
            m_list->SetItem(idx, 3, wxString::Format(wxT("%u"), dcc->size));
            m_list->SetItem(idx, 4, wxString::Format(wxT("%ld"),
                                                       (long)dcc->starttime));
        }
        list = list->next;
    }
}

void DccChatDialog::OnAbort(wxCommandEvent &event)
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    /* Find corresponding DCC struct */
    int idx = 0;
    GSList *list = dcc_list;
    while (list) {
        struct DCC *dcc = (struct DCC *)list->data;
        if (dcc->type == TYPE_CHATRECV || dcc->type == TYPE_CHATSEND) {
            if (idx == sel) {
                dcc_abort(dcc->serv->front_session, dcc);
                break;
            }
            idx++;
        }
        list = list->next;
    }
    PopulateList();
}

void DccChatDialog::OnAccept(wxCommandEvent &event)
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    int idx = 0;
    GSList *list = dcc_list;
    while (list) {
        struct DCC *dcc = (struct DCC *)list->data;
        if (dcc->type == TYPE_CHATRECV || dcc->type == TYPE_CHATSEND) {
            if (idx == sel) {
                dcc_get(dcc);
                break;
            }
            idx++;
        }
        list = list->next;
    }
    PopulateList();
}

void DccChatDialog::OnCloseWindow(wxCloseEvent &event)
{
    Hide();
}

/* ----------------------------- DccTransferDialog ----------------------------- */

/* View filter bitmask — matches GTK3 */
enum { VIEW_DOWNLOAD = 1, VIEW_UPLOAD = 2, VIEW_BOTH = 3 };

wxBEGIN_EVENT_TABLE(DccTransferDialog, wxDialog)
    EVT_BUTTON(ID_DCC_XFER_ABORT, DccTransferDialog::OnAbort)
    EVT_BUTTON(ID_DCC_XFER_ACCEPT, DccTransferDialog::OnAccept)
    EVT_BUTTON(ID_DCC_XFER_RESUME, DccTransferDialog::OnResume)
    EVT_BUTTON(ID_DCC_XFER_CLEAR, DccTransferDialog::OnClear)
    EVT_BUTTON(ID_DCC_XFER_OPEN, DccTransferDialog::OnOpenFolder)
    EVT_RADIOBUTTON(ID_DCC_XFER_UPLOADS, DccTransferDialog::OnFilterChanged)
    EVT_RADIOBUTTON(ID_DCC_XFER_DOWNLOADS, DccTransferDialog::OnFilterChanged)
    EVT_RADIOBUTTON(ID_DCC_XFER_BOTH, DccTransferDialog::OnFilterChanged)
    EVT_LIST_ITEM_SELECTED(wxID_ANY, DccTransferDialog::OnSelectionChanged)
    EVT_LIST_ITEM_DESELECTED(wxID_ANY, DccTransferDialog::OnSelectionChanged)
    EVT_COLLAPSIBLEPANE_CHANGED(ID_DCC_XFER_DETAILS, DccTransferDialog::OnCollapsibleChanged)
    EVT_CLOSE(DccTransferDialog::OnCloseWindow)
wxEND_EVENT_TABLE()

DccTransferDialog::DccTransferDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, wxString(DISPLAY_NAME ": Uploads and Downloads"),
               wxDefaultPosition, wxSize(700, 400),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX),
      m_filter(VIEW_BOTH)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* ---- Transfer list ---- */
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT);
    m_list->InsertColumn(0, wxT("Status"),   wxLIST_FORMAT_LEFT,  75);
    m_list->InsertColumn(1, wxT("File"),     wxLIST_FORMAT_LEFT, 160);
    m_list->InsertColumn(2, wxT("Size"),     wxLIST_FORMAT_RIGHT, 75);
    m_list->InsertColumn(3, wxT("Position"), wxLIST_FORMAT_RIGHT, 75);
    m_list->InsertColumn(4, wxT("%"),        wxLIST_FORMAT_RIGHT, 45);
    m_list->InsertColumn(5, wxT("KB/s"),     wxLIST_FORMAT_RIGHT, 55);
    m_list->InsertColumn(6, wxT("ETA"),      wxLIST_FORMAT_LEFT,  65);
    m_list->InsertColumn(7, wxT("Nick"),     wxLIST_FORMAT_LEFT, 100);

    mainSizer->Add(m_list, 1, wxEXPAND | wxALL, 3);

    /* ---- Details expander + radio buttons row ---- */
    wxBoxSizer *filterSizer = new wxBoxSizer(wxHORIZONTAL);

    /* Collapsible "Details" pane — matches GtkExpander */
    m_details_pane = new wxCollapsiblePane(this, ID_DCC_XFER_DETAILS, wxT("Details"),
                                            wxDefaultPosition, wxDefaultSize,
                                            wxCP_DEFAULT_STYLE | wxCP_NO_TLW_RESIZE);

    /* Populate the pane with File: and Address: labels */
    wxWindow *paneWin = m_details_pane->GetPane();
    wxFlexGridSizer *detailGrid = new wxFlexGridSizer(2, 2, 2, 6);
    detailGrid->AddGrowableCol(1, 1);

    wxStaticText *fileHdr = new wxStaticText(paneWin, wxID_ANY, wxT("File:"));
    fileHdr->SetFont(fileHdr->GetFont().Bold());
    m_file_label = new wxStaticText(paneWin, wxID_ANY, wxEmptyString);

    wxStaticText *addrHdr = new wxStaticText(paneWin, wxID_ANY, wxT("Address:"));
    addrHdr->SetFont(addrHdr->GetFont().Bold());
    m_addr_label = new wxStaticText(paneWin, wxID_ANY, wxEmptyString);

    detailGrid->Add(fileHdr, 0, wxALIGN_LEFT);
    detailGrid->Add(m_file_label, 1, wxEXPAND);
    detailGrid->Add(addrHdr, 0, wxALIGN_LEFT);
    detailGrid->Add(m_addr_label, 1, wxEXPAND);
    paneWin->SetSizer(detailGrid);

    filterSizer->Add(m_details_pane, 0, wxALIGN_CENTER_VERTICAL);
    filterSizer->AddStretchSpacer();

    /* Radio buttons: Uploads, Downloads, Both */
    m_uploads_radio = new wxRadioButton(this, ID_DCC_XFER_UPLOADS,
                                         wxT("Uploads"), wxDefaultPosition,
                                         wxDefaultSize, wxRB_GROUP);
    m_downloads_radio = new wxRadioButton(this, ID_DCC_XFER_DOWNLOADS,
                                           wxT("Downloads"));
    m_both_radio = new wxRadioButton(this, ID_DCC_XFER_BOTH, wxT("Both"));
    m_both_radio->SetValue(true);

    filterSizer->Add(m_uploads_radio, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    filterSizer->Add(m_downloads_radio, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    filterSizer->Add(m_both_radio, 0, wxALIGN_CENTER_VERTICAL);

    mainSizer->Add(filterSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 3);

    /* ---- Action buttons — evenly spread ---- */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_btn_abort  = new wxButton(this, ID_DCC_XFER_ABORT,  wxT("Abort"));
    m_btn_accept = new wxButton(this, ID_DCC_XFER_ACCEPT, wxT("Accept"));
    m_btn_resume = new wxButton(this, ID_DCC_XFER_RESUME, wxT("Resume"));
    m_btn_clear  = new wxButton(this, ID_DCC_XFER_CLEAR,  wxT("Clear"));
    m_btn_open   = new wxButton(this, ID_DCC_XFER_OPEN,   wxT("Open Folder..."));

    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_abort, 0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_accept, 0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_resume, 0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_clear, 0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_btn_open, 0);
    btnSizer->AddStretchSpacer();

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
    SetMinSize(wxSize(550, 300));
    Centre();

    /* Start with buttons disabled until selection */
    m_btn_abort->Enable(false);
    m_btn_accept->Enable(false);
    m_btn_resume->Enable(false);

    PopulateList();

    wx_darkmode_apply_to_window(this);
}

DccTransferDialog::~DccTransferDialog()
{
}

void DccTransferDialog::RefreshList()
{
    PopulateList();
}

/* Format bytes into B / KB / MB / GB */
static wxString FormatSize(guint64 bytes)
{
    if (bytes < 1024)
        return wxString::Format(wxT("%llu"), (unsigned long long)bytes);
    if (bytes < 1024 * 1024)
        return wxString::Format(wxT("%.1f KB"), bytes / 1024.0);
    if (bytes < (guint64)1024 * 1024 * 1024)
        return wxString::Format(wxT("%.1f MB"), bytes / (1024.0 * 1024.0));
    return wxString::Format(wxT("%.2f GB"), bytes / (1024.0 * 1024.0 * 1024.0));
}

void DccTransferDialog::PopulateList()
{
    m_list->DeleteAllItems();

    GSList *list = dcc_list;
    while (list) {
        struct DCC *dcc = (struct DCC *)list->data;
        bool is_send = (dcc->type == TYPE_SEND);
        bool is_recv = (dcc->type == TYPE_RECV);

        if (!is_send && !is_recv) {
            list = list->next;
            continue;
        }

        /* Apply filter bitmask */
        if (is_send && !(m_filter & VIEW_UPLOAD))   { list = list->next; continue; }
        if (is_recv && !(m_filter & VIEW_DOWNLOAD))  { list = list->next; continue; }

        long idx = m_list->GetItemCount();
        m_list->InsertItem(idx, wxEmptyString);

        wxString status;
        switch (dcc->dccstat) {
            case STAT_QUEUED:     status = wxT("Queued"); break;
            case STAT_ACTIVE:     status = wxT("Active"); break;
            case STAT_FAILED:     status = wxT("Failed"); break;
            case STAT_DONE:       status = wxT("Done"); break;
            case STAT_CONNECTING: status = wxT("Connecting"); break;
            case STAT_ABORTED:    status = wxT("Aborted"); break;
            default:              status = wxT("Unknown"); break;
        }
        m_list->SetItem(idx, 0, status);
        m_list->SetItem(idx, 1, wxString::FromUTF8(dcc->file));
        m_list->SetItem(idx, 2, FormatSize(dcc->size));
        m_list->SetItem(idx, 3, FormatSize(dcc->pos));

        /* Percentage */
        if (dcc->size > 0) {
            int pct = (int)((dcc->pos * 100) / dcc->size);
            m_list->SetItem(idx, 4, wxString::Format(wxT("%d%%"), pct));
        } else {
            m_list->SetItem(idx, 4, wxT("0%"));
        }

        /* KB/s */
        if (dcc->cps > 0)
            m_list->SetItem(idx, 5, wxString::Format(wxT("%.1f"), dcc->cps / 1024.0));
        else
            m_list->SetItem(idx, 5, wxT("-"));

        /* ETA in HH:MM:SS */
        if (dcc->cps > 0 && dcc->size > dcc->pos) {
            int eta = (int)((dcc->size - dcc->pos) / dcc->cps);
            int h = eta / 3600;
            int m = (eta % 3600) / 60;
            int s = eta % 60;
            if (h > 0)
                m_list->SetItem(idx, 6, wxString::Format(wxT("%d:%02d:%02d"), h, m, s));
            else
                m_list->SetItem(idx, 6, wxString::Format(wxT("%02d:%02d"), m, s));
        } else {
            m_list->SetItem(idx, 6, wxT("-"));
        }

        m_list->SetItem(idx, 7, wxString::FromUTF8(dcc->nick));

        list = list->next;
    }

    UpdateButtonSensitivity();
}

struct DCC *DccTransferDialog::GetSelectedDCC()
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return nullptr;

    int idx = 0;
    GSList *list = dcc_list;
    while (list) {
        struct DCC *dcc = (struct DCC *)list->data;
        bool is_send = (dcc->type == TYPE_SEND);
        bool is_recv = (dcc->type == TYPE_RECV);
        if (!is_send && !is_recv)                     { list = list->next; continue; }
        if (is_send && !(m_filter & VIEW_UPLOAD))     { list = list->next; continue; }
        if (is_recv && !(m_filter & VIEW_DOWNLOAD))   { list = list->next; continue; }

        if (idx == sel) return dcc;
        idx++;
        list = list->next;
    }
    return nullptr;
}

void DccTransferDialog::UpdateDetailLabels()
{
    struct DCC *dcc = GetSelectedDCC();
    if (!dcc) {
        m_file_label->SetLabel(wxEmptyString);
        m_addr_label->SetLabel(wxEmptyString);
        return;
    }

    /* File: show destfile for receives, file for sends */
    if (dcc->type == TYPE_RECV && dcc->destfile[0])
        m_file_label->SetLabel(wxString::FromUTF8(dcc->destfile));
    else
        m_file_label->SetLabel(wxString::FromUTF8(dcc->file));

    /* Address: ip : port */
    char ipbuf[128];
    g_snprintf(ipbuf, sizeof ipbuf, "%s : %d", net_ip(dcc->addr), dcc->port);
    m_addr_label->SetLabel(wxString::FromUTF8(ipbuf));
}

void DccTransferDialog::UpdateButtonSensitivity()
{
    struct DCC *dcc = GetSelectedDCC();

    bool has_sel = (dcc != nullptr);
    bool can_accept = has_sel && dcc->dccstat == STAT_QUEUED && dcc->type == TYPE_RECV;
    bool can_resume = can_accept; /* resume applies to queued receives */
    bool can_abort = has_sel && (dcc->dccstat == STAT_QUEUED ||
                                 dcc->dccstat == STAT_ACTIVE ||
                                 dcc->dccstat == STAT_CONNECTING);

    m_btn_abort->Enable(can_abort);
    m_btn_accept->Enable(can_accept);
    m_btn_resume->Enable(can_resume);
}

void DccTransferDialog::OnAbort(wxCommandEvent &)
{
    struct DCC *dcc = GetSelectedDCC();
    if (dcc) {
        dcc_abort(dcc->serv->front_session, dcc);
        PopulateList();
    }
}

void DccTransferDialog::OnAccept(wxCommandEvent &)
{
    struct DCC *dcc = GetSelectedDCC();
    if (dcc) {
        dcc_get(dcc);
        PopulateList();
    }
}

void DccTransferDialog::OnResume(wxCommandEvent &)
{
    struct DCC *dcc = GetSelectedDCC();
    if (dcc) {
        dcc_get(dcc);
        PopulateList();
    }
}

void DccTransferDialog::OnClear(wxCommandEvent &)
{
    GSList *list = dcc_list;
    GSList *next;
    while (list) {
        next = list->next;
        struct DCC *dcc = (struct DCC *)list->data;
        if ((dcc->type == TYPE_SEND || dcc->type == TYPE_RECV) &&
            (dcc->dccstat == STAT_DONE || dcc->dccstat == STAT_FAILED ||
             dcc->dccstat == STAT_ABORTED)) {
            dcc_abort(current_sess, dcc);
        }
        list = next;
    }
    PopulateList();
}

void DccTransferDialog::OnOpenFolder(wxCommandEvent &)
{
    wxString path = wxString::FromUTF8(prefs.pchat_dcc_dir);
    if (path.IsEmpty())
        path = wxGetHomeDir();
    wxLaunchDefaultApplication(path);
}

void DccTransferDialog::OnFilterChanged(wxCommandEvent &event)
{
    int id = event.GetId();
    if (id == ID_DCC_XFER_UPLOADS)        m_filter = VIEW_UPLOAD;
    else if (id == ID_DCC_XFER_DOWNLOADS) m_filter = VIEW_DOWNLOAD;
    else                                   m_filter = VIEW_BOTH;
    PopulateList();
}

void DccTransferDialog::OnSelectionChanged(wxListEvent &)
{
    UpdateDetailLabels();
    UpdateButtonSensitivity();
}

void DccTransferDialog::OnCollapsibleChanged(wxCollapsiblePaneEvent &)
{
    Layout();
}

void DccTransferDialog::OnCloseWindow(wxCloseEvent &)
{
    Hide();
}
