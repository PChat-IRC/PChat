/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * EditListDialog - Generic 2-column editable list dialog
 * Used for: Auto Replace, CTCP Replies, Dialog Buttons,
 *           URL Handlers, User Commands, Userlist Buttons,
 *           Userlist Popup
 *
 * Replicates the GTK3 editlist.c functionality.
 */

#ifndef PCHAT_EDITLISTDIALOG_H
#define PCHAT_EDITLISTDIALOG_H

#include <wx/wx.h>
#include <wx/listctrl.h>

#include <glib.h>

class EditListDialog : public wxDialog
{
public:
    /**
     * @param parent     Parent window
     * @param title1     Column 1 header (e.g. "Name", "Text")
     * @param title2     Column 2 header (e.g. "Command", "Replace with")
     * @param list       Pointer to the GSList* that holds struct popup entries
     * @param title      Window title
     * @param conf_file  Config filename (e.g. "replace.conf")
     */
    EditListDialog(wxWindow *parent,
                   const wxString &title1, const wxString &title2,
                   GSList **list,
                   const wxString &title,
                   const char *conf_file,
                   const wxString &helptext = "",
                   bool show_edit_fields = true,
                   bool show_sort_buttons = true);
    ~EditListDialog();

private:
    void PopulateList();
    void SaveList();
    bool ShowItemDialog(wxString &name, wxString &cmd, const wxString &dlg_title);

    void OnAdd(wxCommandEvent &event);
    void OnDelete(wxCommandEvent &event);
    void OnSave(wxCommandEvent &event);
    void OnCancel(wxCommandEvent &event);
    void OnSortAsc(wxCommandEvent &event);
    void OnSortDesc(wxCommandEvent &event);
    void OnItemSelected(wxListEvent &event);
    void OnItemActivated(wxListEvent &event);

    wxListCtrl *m_list_ctrl;
    wxTextCtrl *m_name_entry;
    wxTextCtrl *m_cmd_entry;
    GSList **m_list;
    const char *m_conf_file;
    wxString m_title1;
    wxString m_title2;
    bool m_show_edit_fields;

    enum {
        ID_ADD_BTN = wxID_HIGHEST + 500,
        ID_DELETE_BTN,
        ID_SAVE_BTN,
        ID_SORT_ASC_BTN,
        ID_SORT_DESC_BTN,
    };

    wxDECLARE_EVENT_TABLE();
};

#endif /* PCHAT_EDITLISTDIALOG_H */
