/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * TextEventsDialog - Text events editor
 * Replicates the GTK3 textgui.c pevent_dialog functionality.
 *
 * File format (pevents.conf):
 *   event_name=<name>
 *   event_text=<format_string>
 *   <blank line>
 */

#include "TextEventsDialog.h"
#include "DarkMode.h"

/* glib.h must be included outside extern "C" first because gwin32.h
   contains C++ templates on Windows. The include guard in glib.h then
   prevents re-inclusion when pchat.h tries to pull it in. */
#include <glib.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/text.h"
#include "../common/cfgfiles.h"
#include "../common/fe.h"
}

#include <wx/filedlg.h>

extern "C" {
extern struct text_event te[];
extern char *pntevts_text[];
extern char *pntevts[];
}

wxBEGIN_EVENT_TABLE(TextEventsDialog, wxDialog)
    EVT_LIST_ITEM_SELECTED(ID_TE_EVENT_LIST, TextEventsDialog::OnEventSelected)
    EVT_TEXT_ENTER(ID_TE_FORMAT_ENTRY, TextEventsDialog::OnFormatEnter)
    EVT_BUTTON(ID_TE_TEST_ALL, TextEventsDialog::OnTestAll)
    EVT_BUTTON(ID_TE_SAVE, TextEventsDialog::OnSave)
    EVT_BUTTON(ID_TE_SAVE_AS, TextEventsDialog::OnSaveAs)
    EVT_BUTTON(ID_TE_LOAD_FROM, TextEventsDialog::OnLoadFrom)
    EVT_BUTTON(wxID_OK, TextEventsDialog::OnOK)
wxEND_EVENT_TABLE()

TextEventsDialog::TextEventsDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Edit Events",
               wxDefaultPosition, wxSize(700, 550),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    auto *main_sizer = new wxBoxSizer(wxVERTICAL);

    /* --- Vertical splitter: event list (top) | edit area (bottom) --- */
    auto *splitter = new wxSplitterWindow(this, wxID_ANY,
                                           wxDefaultPosition, wxDefaultSize,
                                           wxSP_3D | wxSP_LIVE_UPDATE);

    /* Top panel: event list */
    auto *top_panel = new wxPanel(splitter);
    auto *top_sizer = new wxBoxSizer(wxVERTICAL);

    m_event_list = new wxListCtrl(top_panel, ID_TE_EVENT_LIST,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxLC_REPORT | wxLC_SINGLE_SEL);
    m_event_list->AppendColumn("Event", wxLIST_FORMAT_LEFT, 200);
    m_event_list->AppendColumn("Text", wxLIST_FORMAT_LEFT, 450);
    top_sizer->Add(m_event_list, 1, wxEXPAND | wxALL, 2);
    top_panel->SetSizer(top_sizer);

    /* Bottom panel: format entry + preview + param help */
    auto *bottom_panel = new wxPanel(splitter);
    auto *bottom_sizer = new wxBoxSizer(wxVERTICAL);

    /* Format entry */
    auto *entry_sizer = new wxBoxSizer(wxHORIZONTAL);
    entry_sizer->Add(new wxStaticText(bottom_panel, wxID_ANY, "Format:"),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    m_format_entry = new wxTextCtrl(bottom_panel, ID_TE_FORMAT_ENTRY, "",
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_PROCESS_ENTER);
    entry_sizer->Add(m_format_entry, 1, wxEXPAND);
    bottom_sizer->Add(entry_sizer, 0, wxEXPAND | wxALL, 4);

    /* Preview area and param help side by side */
    auto *preview_hbox = new wxBoxSizer(wxHORIZONTAL);

    m_preview = new wxRichTextCtrl(bottom_panel, wxID_ANY, "",
                                    wxDefaultPosition, wxSize(-1, 80),
                                    wxRE_MULTILINE | wxRE_READONLY);
    preview_hbox->Add(m_preview, 1, wxEXPAND | wxRIGHT, 4);

    /* Parameter help list */
    m_param_list = new wxListCtrl(bottom_panel, wxID_ANY,
                                   wxDefaultPosition, wxSize(200, -1),
                                   wxLC_REPORT | wxLC_NO_HEADER);
    m_param_list->AppendColumn("$", wxLIST_FORMAT_LEFT, 30);
    m_param_list->AppendColumn("Description", wxLIST_FORMAT_LEFT, 160);
    preview_hbox->Add(m_param_list, 0, wxEXPAND);

    bottom_sizer->Add(preview_hbox, 1, wxEXPAND | wxLEFT | wxRIGHT, 4);
    bottom_panel->SetSizer(bottom_sizer);

    splitter->SplitHorizontally(top_panel, bottom_panel, 280);
    splitter->SetMinimumPaneSize(100);
    main_sizer->Add(splitter, 1, wxEXPAND | wxALL, 2);

    /* --- Buttons --- */
    auto *btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->Add(new wxButton(this, ID_TE_TEST_ALL, "Test All"), 0, wxRIGHT, 4);
    btn_sizer->AddStretchSpacer();
    btn_sizer->Add(new wxButton(this, ID_TE_LOAD_FROM, "Load From..."), 0, wxRIGHT, 4);
    btn_sizer->Add(new wxButton(this, ID_TE_SAVE_AS, "Save As..."), 0, wxRIGHT, 4);
    btn_sizer->Add(new wxButton(this, ID_TE_SAVE, "Save"), 0, wxRIGHT, 4);
    btn_sizer->Add(new wxButton(this, wxID_OK, "OK"), 0);
    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(main_sizer);

    PopulateEvents();
    Centre();

    wx_darkmode_apply_to_window(this);
}

TextEventsDialog::~TextEventsDialog()
{
}

void TextEventsDialog::PopulateEvents()
{
    m_event_list->DeleteAllItems();

    for (int i = 0; i < NUM_XP; i++) {
        long idx = m_event_list->InsertItem(i, wxString::FromUTF8(te[i].name));
        m_event_list->SetItem(idx, 1,
            wxString::FromUTF8(pntevts_text[i] ? pntevts_text[i] : ""));
    }
}

void TextEventsDialog::FillParamHelp(int event_index)
{
    m_param_list->DeleteAllItems();

    if (event_index < 0 || event_index >= NUM_XP)
        return;

    int num_args = te[event_index].num_args & 0x7f;
    for (int i = 0; i < num_args; i++) {
        const char *help = te[event_index].help[i];
        if (!help) break;

        /* Skip leading \001 if present */
        if (help[0] == '\001')
            help++;

        long idx = m_param_list->InsertItem(i,
                                             wxString::Format("$%d", i + 1));
        m_param_list->SetItem(idx, 1, wxString::FromUTF8(help));
    }
}

void TextEventsDialog::DoPreview(const wxString &text, int event_index)
{
    /* Simple preview: just show the format string with variables replaced
       by placeholder text */
    wxString preview = text;

    /* Replace $1, $2, etc. with sample values */
    if (event_index >= 0 && event_index < NUM_XP) {
        int num_args = te[event_index].num_args & 0x7f;
        for (int i = num_args; i >= 1; i--) {
            wxString var = wxString::Format("$%d", i);
            wxString sample;
            if (te[event_index].help && te[event_index].help[i - 1]) {
                const char *h = te[event_index].help[i - 1];
                if (h[0] == '\001') h++;
                sample = wxString::Format("<%s>", h);
            } else {
                sample = wxString::Format("<arg%d>", i);
            }
            preview.Replace(var, sample);
        }
    }

    /* Strip IRC color codes for now (just show plain text) */
    wxString clean;
    bool in_color = false;
    int color_digits = 0;
    for (size_t i = 0; i < preview.length(); i++) {
        wxUniChar c = preview[i];
        if (c == '%' && i + 1 < preview.length()) {
            wxUniChar next = preview[i + 1];
            if (next == 'C' || next == 'B' || next == 'U' || next == 'O' ||
                next == 'R' || next == 'H' || next == 'I') {
                i++; /* skip the format char */
                if (next == 'C') {
                    in_color = true;
                    color_digits = 0;
                }
                continue;
            }
        }
        if (in_color && ((c >= '0' && c <= '9') || c == ',')) {
            color_digits++;
            if (color_digits > 5) in_color = false;
            continue;
        }
        in_color = false;

        if (c == '\t') {
            clean += "    ";
        } else {
            clean += c;
        }
    }

    m_preview->Clear();
    m_preview->WriteText(clean);
}

void TextEventsDialog::OnEventSelected(wxListEvent &event)
{
    long sel = event.GetIndex();
    if (sel < 0 || sel >= NUM_XP)
        return;

    m_selected_event = sel;

    /* Fill format entry */
    m_format_entry->SetValue(
        wxString::FromUTF8(pntevts_text[sel] ? pntevts_text[sel] : ""));

    /* Fill parameter help */
    FillParamHelp(sel);

    /* Preview */
    DoPreview(m_format_entry->GetValue(), sel);
}

void TextEventsDialog::OnFormatEnter(wxCommandEvent &event)
{
    if (m_selected_event < 0 || m_selected_event >= NUM_XP)
        return;

    wxString text = m_format_entry->GetValue();
    auto text_utf8 = text.ToUTF8();

    /* Validate: build the event string to check for errors */
    char *out = nullptr;
    int max_arg = 0;
    if (pevt_build_string(text_utf8.data(), &out, &max_arg) != 0) {
        wxMessageBox("Error parsing the format string.",
                     "PChat", wxOK | wxICON_ERROR, this);
        return;
    }

    int allowed = te[m_selected_event].num_args & 0x7f;
    if (max_arg > allowed) {
        g_free(out);
        wxMessageBox(wxString::Format(
            "This event only accepts %d argument(s). $%d is invalid.",
            allowed, max_arg),
            "PChat", wxOK | wxICON_WARNING, this);
        return;
    }

    /* Update in-memory data */
    if (pntevts_text[m_selected_event])
        g_free(pntevts_text[m_selected_event]);
    if (pntevts[m_selected_event])
        g_free(pntevts[m_selected_event]);

    int len = strlen(text_utf8.data());
    pntevts_text[m_selected_event] = (char *)g_malloc(len + 1);
    memcpy(pntevts_text[m_selected_event], text_utf8.data(), len + 1);
    pntevts[m_selected_event] = out;

    /* Update the list view */
    m_event_list->SetItem(m_selected_event, 1, text);

    /* Preview */
    DoPreview(text, m_selected_event);

    /* Mark that events need saving */
    prefs.save_pevents = 1;
}

void TextEventsDialog::OnTestAll(wxCommandEvent &event)
{
    m_preview->Clear();

    for (int n = 0; n < NUM_XP; n++) {
        const char *text = pntevts_text[n];
        if (!text || text[0] == '\0')
            continue;

        wxString line = wxString::Format("[%s] %s",
                                          te[n].name, text);
        m_preview->WriteText(line + "\n");
    }
}

void TextEventsDialog::OnSave(wxCommandEvent &event)
{
    pevent_save(nullptr);
}

void TextEventsDialog::OnSaveAs(wxCommandEvent &event)
{
    wxFileDialog dlg(this, "Save Text Events", "", "pevents.conf",
                     "Config files (*.conf)|*.conf|All files (*.*)|*.*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        auto path = dlg.GetPath().ToUTF8();
        pevent_save(const_cast<char *>(path.data()));
    }
}

void TextEventsDialog::OnLoadFrom(wxCommandEvent &event)
{
    wxFileDialog dlg(this, "Load Text Events", "", "",
                     "Config files (*.conf)|*.conf|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        auto path = dlg.GetPath().ToUTF8();
        pevent_load(const_cast<char *>(path.data()));
        pevent_make_pntevts();
        PopulateEvents();
        prefs.save_pevents = 1;

        m_selected_event = -1;
        m_format_entry->Clear();
        m_param_list->DeleteAllItems();
        m_preview->Clear();
    }
}

void TextEventsDialog::OnOK(wxCommandEvent &event)
{
    /* Save on close like GTK3 does */
    pevent_save(nullptr);
    EndModal(wxID_OK);
}
