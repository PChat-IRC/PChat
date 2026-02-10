/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * URL Grabber Dialog
 */

#include "UrlGrabberDialog.h"

#include <glib.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/url.h"
#include "../common/tree.h"
}

wxBEGIN_EVENT_TABLE(UrlGrabberDialog, wxDialog)
    EVT_BUTTON(ID_URL_CLEAR, UrlGrabberDialog::OnClear)
    EVT_BUTTON(ID_URL_COPY, UrlGrabberDialog::OnCopy)
    EVT_BUTTON(ID_URL_SAVE, UrlGrabberDialog::OnSaveAs)
    EVT_LISTBOX_DCLICK(ID_URL_LIST, UrlGrabberDialog::OnDoubleClick)
    EVT_CLOSE(UrlGrabberDialog::OnCloseWindow)
wxEND_EVENT_TABLE()

UrlGrabberDialog::UrlGrabberDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, wxT("URL Grabber - PChat"),
               wxDefaultPosition, wxSize(550, 380),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* URL list */
    m_url_list = new wxListBox(this, ID_URL_LIST,
                                wxDefaultPosition, wxDefaultSize,
                                0, nullptr, wxLB_SINGLE);
    mainSizer->Add(m_url_list, 1, wxEXPAND | wxALL, 4);

    /* Buttons */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(this, ID_URL_CLEAR, wxT("Clear")),
                  0, wxRIGHT, 8);
    btnSizer->Add(new wxButton(this, ID_URL_COPY, wxT("Copy")),
                  0, wxRIGHT, 8);
    btnSizer->Add(new wxButton(this, ID_URL_SAVE, wxT("Save As...")),
                  0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, wxID_CLOSE, wxT("Close")), 0);

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 4);

    SetSizer(mainSizer);
    Centre();

    PopulateList();
}

UrlGrabberDialog::~UrlGrabberDialog()
{
}

void UrlGrabberDialog::RefreshList()
{
    PopulateList();
}

void UrlGrabberDialog::PopulateList()
{
    m_url_list->Clear();

    /* Walk the URL tree from the backend */
    tree_foreach((tree *)url_tree, (tree_traverse_func *)UrlTreeCallback, this);
}

int UrlGrabberDialog::UrlTreeCallback(const void *key, void *userdata)
{
    UrlGrabberDialog *dlg = static_cast<UrlGrabberDialog *>(userdata);
    dlg->m_url_list->Append(wxString::FromUTF8((const char *)key));
    return 0; /* continue */
}

void UrlGrabberDialog::OnClear(wxCommandEvent &event)
{
    url_clear();
    m_url_list->Clear();
}

void UrlGrabberDialog::OnCopy(wxCommandEvent &event)
{
    int sel = m_url_list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(
            new wxTextDataObject(m_url_list->GetString(sel)));
        wxTheClipboard->Close();
    }
}

void UrlGrabberDialog::OnSaveAs(wxCommandEvent &event)
{
    wxFileDialog dlg(this, wxT("Save URL list"),
                      wxEmptyString, wxT("urls.txt"),
                      wxT("Text files (*.txt)|*.txt|All files (*.*)|*.*"),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        wxFile file(dlg.GetPath(), wxFile::write);
        if (file.IsOpened()) {
            for (unsigned int i = 0; i < m_url_list->GetCount(); i++) {
                file.Write(m_url_list->GetString(i) + wxT("\n"));
            }
        }
    }
}

void UrlGrabberDialog::OnDoubleClick(wxCommandEvent &event)
{
    int sel = m_url_list->GetSelection();
    if (sel == wxNOT_FOUND) return;

    wxString url = m_url_list->GetString(sel);
    wxLaunchDefaultBrowser(url);
}

void UrlGrabberDialog::OnCloseWindow(wxCloseEvent &event)
{
    Hide();
}
