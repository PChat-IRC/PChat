/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * TextEventsDialog - Text events editor
 * Replicates the GTK3 textgui.c pevent_dialog_show() functionality.
 */

#ifndef PCHAT_TEXTEVENTSDIALOG_H
#define PCHAT_TEXTEVENTSDIALOG_H

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>
#include <wx/richtext/richtextctrl.h>

class TextEventsDialog : public wxDialog
{
public:
    TextEventsDialog(wxWindow *parent);
    ~TextEventsDialog();

private:
    void PopulateEvents();
    void FillParamHelp(int event_index);
    void DoPreview(const wxString &text, int event_index);

    void OnEventSelected(wxListEvent &event);
    void OnFormatEnter(wxCommandEvent &event);
    void OnTestAll(wxCommandEvent &event);
    void OnSave(wxCommandEvent &event);
    void OnSaveAs(wxCommandEvent &event);
    void OnLoadFrom(wxCommandEvent &event);
    void OnOK(wxCommandEvent &event);

    wxListCtrl *m_event_list;
    wxTextCtrl *m_format_entry;
    wxRichTextCtrl *m_preview;
    wxListCtrl *m_param_list;

    int m_selected_event = -1;

    enum {
        ID_TE_EVENT_LIST = wxID_HIGHEST + 700,
        ID_TE_FORMAT_ENTRY,
        ID_TE_TEST_ALL,
        ID_TE_SAVE,
        ID_TE_SAVE_AS,
        ID_TE_LOAD_FROM,
    };

    wxDECLARE_EVENT_TABLE();
};

#endif /* PCHAT_TEXTEVENTSDIALOG_H */
