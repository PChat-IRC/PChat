/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Raw Log Dialog - displays raw IRC protocol traffic
 */

#include "RawLogDialog.h"
#include "DarkMode.h"
#include "fe-wx.h"

wxBEGIN_EVENT_TABLE(RawLogDialog, wxDialog)
    EVT_BUTTON(ID_RAWLOG_CLEAR, RawLogDialog::OnClearLog)
    EVT_BUTTON(ID_RAWLOG_SAVE, RawLogDialog::OnSaveAs)
    EVT_CLOSE(RawLogDialog::OnClose)
wxEND_EVENT_TABLE()

RawLogDialog::RawLogDialog(wxWindow *parent, struct server *serv)
    : wxDialog(parent, wxID_ANY,
               wxString::Format(wxT("Raw Log (%s) - PChat"),
                   serv && serv->servername[0] ?
                       wxString::FromUTF8(serv->servername) :
                       wxString(wxT("unknown"))),
               wxDefaultPosition, wxSize(650, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX),
      m_serv(serv)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* Text display */
    m_text = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                             wxDefaultPosition, wxDefaultSize,
                             wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 |
                             wxHSCROLL);
    wxFont monoFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL,
                     wxFONTWEIGHT_NORMAL);
    m_text->SetFont(monoFont);
    m_text->SetBackgroundColour(wx_darkmode_rawlog_bg());
    mainSizer->Add(m_text, 1, wxEXPAND | wxALL, 4);

    /* Buttons */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(this, ID_RAWLOG_CLEAR, wxT("Clear Raw Log")),
                  0, wxRIGHT, 8);
    btnSizer->Add(new wxButton(this, ID_RAWLOG_SAVE, wxT("Save As...")),
                  0, wxRIGHT, 8);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, wxID_CLOSE, wxT("Close")), 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 4);

    SetSizer(mainSizer);
    Centre();

    wx_darkmode_apply_to_window(this);
}

RawLogDialog::~RawLogDialog()
{
    if (m_serv && m_serv->gui)
        m_serv->gui->rawlog_window = nullptr;
}

void RawLogDialog::AppendText(const wxString &text, bool outbound)
{
    m_text->SetDefaultStyle(wxTextAttr(
        outbound ? wx_darkmode_rawlog_outbound() : wx_darkmode_rawlog_inbound()));

    wxString prefix = outbound ? wxT("<< ") : wxT(">> ");
    m_text->AppendText(prefix + text + wxT("\n"));

    /* Scroll to bottom */
    m_text->ShowPosition(m_text->GetLastPosition());
}

void RawLogDialog::OnClearLog(wxCommandEvent &event)
{
    m_text->Clear();
}

void RawLogDialog::OnSaveAs(wxCommandEvent &event)
{
    wxFileDialog dlg(this, wxT("Save Raw Log"),
                      wxEmptyString, wxT("rawlog.txt"),
                      wxT("Text files (*.txt)|*.txt|All files (*.*)|*.*"),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        m_text->SaveFile(dlg.GetPath());
    }
}

void RawLogDialog::OnClose(wxCloseEvent &event)
{
    Hide();
}
