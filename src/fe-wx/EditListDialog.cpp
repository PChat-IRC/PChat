/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * EditListDialog - Generic 2-column editable list dialog
 * Replicates the GTK3 editlist.c functionality.
 *
 * File format (NAME/CMD pairs):
 *   NAME <name>
 *   CMD <command>
 *   <blank line>
 */

#include "EditListDialog.h"

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/cfgfiles.h"
#include "../common/fe.h"
}

#include <fcntl.h>  /* O_TRUNC etc */

#ifndef O_BINARY
#define O_BINARY 0
#endif

wxBEGIN_EVENT_TABLE(EditListDialog, wxDialog)
    EVT_BUTTON(ID_ADD_BTN, EditListDialog::OnAdd)
    EVT_BUTTON(ID_DELETE_BTN, EditListDialog::OnDelete)
    EVT_BUTTON(ID_SAVE_BTN, EditListDialog::OnSave)
    EVT_BUTTON(wxID_CANCEL, EditListDialog::OnCancel)
    EVT_BUTTON(ID_SORT_ASC_BTN, EditListDialog::OnSortAsc)
    EVT_BUTTON(ID_SORT_DESC_BTN, EditListDialog::OnSortDesc)
    EVT_LIST_ITEM_SELECTED(wxID_ANY, EditListDialog::OnItemSelected)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, EditListDialog::OnItemActivated)
wxEND_EVENT_TABLE()

EditListDialog::EditListDialog(wxWindow *parent,
                               const wxString &title1,
                               const wxString &title2,
                               GSList **list,
                               const wxString &title,
                               const char *conf_file,
                               const wxString &helptext,
                               bool show_edit_fields,
                               bool show_sort_buttons)
    : wxDialog(parent, wxID_ANY, title,
               wxDefaultPosition, wxSize(500, 380),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_list(list), m_conf_file(conf_file),
      m_title1(title1), m_title2(title2),
      m_show_edit_fields(show_edit_fields)
{
    auto *main_sizer = new wxBoxSizer(wxVERTICAL);

    /* --- List control --- */
    m_list_ctrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list_ctrl->AppendColumn(title1, wxLIST_FORMAT_LEFT, 180);
    m_list_ctrl->AppendColumn(title2, wxLIST_FORMAT_LEFT, 280);
    if (!helptext.IsEmpty())
        m_list_ctrl->SetToolTip(helptext);
    main_sizer->Add(m_list_ctrl, 1, wxEXPAND | wxALL, 5);

    /* --- Edit fields (optional) --- */
    if (show_edit_fields) {
        auto *edit_sizer = new wxFlexGridSizer(2, 2, 4, 8);
        edit_sizer->AddGrowableCol(1, 1);

        edit_sizer->Add(new wxStaticText(this, wxID_ANY, title1 + ":"),
                        0, wxALIGN_CENTER_VERTICAL);
        m_name_entry = new wxTextCtrl(this, wxID_ANY);
        edit_sizer->Add(m_name_entry, 1, wxEXPAND);

        edit_sizer->Add(new wxStaticText(this, wxID_ANY, title2 + ":"),
                        0, wxALIGN_CENTER_VERTICAL);
        m_cmd_entry = new wxTextCtrl(this, wxID_ANY);
        edit_sizer->Add(m_cmd_entry, 1, wxEXPAND);

        main_sizer->Add(edit_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
    } else {
        m_name_entry = nullptr;
        m_cmd_entry = nullptr;
    }

    /* --- Buttons --- */
    auto *btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    if (show_sort_buttons) {
        btn_sizer->Add(new wxButton(this, ID_ADD_BTN, "Add"), 0, wxRIGHT, 4);
        btn_sizer->Add(new wxButton(this, ID_DELETE_BTN, "Delete"), 0, wxRIGHT, 4);
        btn_sizer->Add(new wxButton(this, ID_SORT_ASC_BTN, "Sort \xe2\x96\xb2"), 0, wxRIGHT, 4);
        btn_sizer->Add(new wxButton(this, ID_SORT_DESC_BTN, "Sort \xe2\x96\xbc"), 0, wxRIGHT, 4);
        btn_sizer->AddStretchSpacer();
        btn_sizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxRIGHT, 4);
        btn_sizer->Add(new wxButton(this, ID_SAVE_BTN, "Save"), 0);
    } else {
        btn_sizer->AddStretchSpacer();
        btn_sizer->Add(new wxButton(this, ID_ADD_BTN, "Add"), 0);
        btn_sizer->AddStretchSpacer();
        btn_sizer->Add(new wxButton(this, ID_DELETE_BTN, "Delete"), 0);
        btn_sizer->AddStretchSpacer();
        btn_sizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0);
        btn_sizer->AddStretchSpacer();
        btn_sizer->Add(new wxButton(this, ID_SAVE_BTN, "Save"), 0);
        btn_sizer->AddStretchSpacer();
    }
    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(main_sizer);

    PopulateList();
    Centre();
}

EditListDialog::~EditListDialog()
{
}

void EditListDialog::PopulateList()
{
    m_list_ctrl->DeleteAllItems();

    if (!m_list || !*m_list)
        return;

    int row = 0;
    for (GSList *l = *m_list; l; l = l->next) {
        auto *pop = static_cast<struct popup *>(l->data);
        long idx = m_list_ctrl->InsertItem(row, wxString::FromUTF8(pop->name));
        m_list_ctrl->SetItem(idx, 1, wxString::FromUTF8(pop->cmd));
        row++;
    }
}

void EditListDialog::SaveList()
{
    if (!m_list || !m_conf_file)
        return;

    int fd = pchat_open_file(m_conf_file,
                             O_TRUNC | O_WRONLY | O_CREAT | O_BINARY,
                             0600, XOF_DOMODE);
    if (fd == -1) {
        wxMessageBox("Failed to save configuration file.",
                     "PChat", wxOK | wxICON_ERROR, this);
        return;
    }

    int count = m_list_ctrl->GetItemCount();
    for (int i = 0; i < count; i++) {
        wxString name = m_list_ctrl->GetItemText(i, 0);
        wxString cmd = m_list_ctrl->GetItemText(i, 1);

        auto name_utf8 = name.ToUTF8();
        auto cmd_utf8 = cmd.ToUTF8();

        /* Write NAME line */
        write(fd, "NAME ", 5);
        write(fd, name_utf8.data(), name_utf8.length());
        write(fd, "\n", 1);

        /* Write CMD line */
        write(fd, "CMD ", 4);
        write(fd, cmd_utf8.data(), cmd_utf8.length());
        write(fd, "\n\n", 2);
    }

    close(fd);

    /* Reload the in-memory list from the saved file */
    list_free(m_list);
    list_loadconf(const_cast<char *>(m_conf_file), m_list, nullptr);

    /* Update UI elements that depend on these lists */
    for (GSList *sl = sess_list; sl; sl = sl->next) {
        auto *sess = static_cast<struct session *>(sl->data);
        if (*m_list == button_list)
            fe_buttons_update(sess);
        if (*m_list == dlgbutton_list)
            fe_dlgbuttons_update(sess);
    }
}

void EditListDialog::OnAdd(wxCommandEvent &event)
{
    wxString name, cmd;

    if (!m_show_edit_fields) {
        if (!ShowItemDialog(name, cmd, "Add"))
            return;
    } else {
        name = m_name_entry->GetValue().Trim();
        cmd = m_cmd_entry->GetValue().Trim();
        if (name.IsEmpty())
            return;
    }

    long idx = m_list_ctrl->InsertItem(m_list_ctrl->GetItemCount(), name);
    m_list_ctrl->SetItem(idx, 1, cmd);

    if (m_show_edit_fields) {
        /* Clear entries for next input */
        m_name_entry->Clear();
        m_cmd_entry->Clear();
        m_name_entry->SetFocus();
    }

    /* Scroll to and select the new item */
    m_list_ctrl->EnsureVisible(idx);
    m_list_ctrl->SetItemState(idx, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
}

void EditListDialog::OnDelete(wxCommandEvent &event)
{
    long sel = m_list_ctrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1)
        return;

    m_list_ctrl->DeleteItem(sel);

    /* Select the next (or last) item */
    int count = m_list_ctrl->GetItemCount();
    if (count > 0) {
        if (sel >= count)
            sel = count - 1;
        m_list_ctrl->SetItemState(sel, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }
}

void EditListDialog::OnSave(wxCommandEvent &event)
{
    SaveList();
    EndModal(wxID_OK);
}

void EditListDialog::OnCancel(wxCommandEvent &event)
{
    EndModal(wxID_CANCEL);
}

void EditListDialog::OnSortAsc(wxCommandEvent &)
{
    /* Collect all items, sort ascending by name, re-populate */
    struct Entry { wxString name; wxString cmd; };
    std::vector<Entry> entries;
    int count = m_list_ctrl->GetItemCount();
    entries.reserve(count);
    for (int i = 0; i < count; i++) {
        entries.push_back({m_list_ctrl->GetItemText(i, 0),
                           m_list_ctrl->GetItemText(i, 1)});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) {
                  return a.name.CmpNoCase(b.name) < 0;
              });
    m_list_ctrl->DeleteAllItems();
    for (int i = 0; i < (int)entries.size(); i++) {
        long idx = m_list_ctrl->InsertItem(i, entries[i].name);
        m_list_ctrl->SetItem(idx, 1, entries[i].cmd);
    }
}

void EditListDialog::OnSortDesc(wxCommandEvent &)
{
    struct Entry { wxString name; wxString cmd; };
    std::vector<Entry> entries;
    int count = m_list_ctrl->GetItemCount();
    entries.reserve(count);
    for (int i = 0; i < count; i++) {
        entries.push_back({m_list_ctrl->GetItemText(i, 0),
                           m_list_ctrl->GetItemText(i, 1)});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) {
                  return a.name.CmpNoCase(b.name) > 0;
              });
    m_list_ctrl->DeleteAllItems();
    for (int i = 0; i < (int)entries.size(); i++) {
        long idx = m_list_ctrl->InsertItem(i, entries[i].name);
        m_list_ctrl->SetItem(idx, 1, entries[i].cmd);
    }
}

void EditListDialog::OnItemSelected(wxListEvent &event)
{
    if (!m_show_edit_fields)
        return;

    long sel = event.GetIndex();
    if (sel == -1)
        return;

    m_name_entry->SetValue(m_list_ctrl->GetItemText(sel, 0));
    m_cmd_entry->SetValue(m_list_ctrl->GetItemText(sel, 1));
}

void EditListDialog::OnItemActivated(wxListEvent &event)
{
    long sel = event.GetIndex();
    if (sel == -1)
        return;

    if (!m_show_edit_fields) {
        /* Double-click: edit via popup dialog */
        wxString name = m_list_ctrl->GetItemText(sel, 0);
        wxString cmd = m_list_ctrl->GetItemText(sel, 1);
        if (ShowItemDialog(name, cmd, "Edit")) {
            m_list_ctrl->SetItem(sel, 0, name);
            m_list_ctrl->SetItem(sel, 1, cmd);
        }
        return;
    }

    /* Double-click: populate edit fields for modification */
    m_name_entry->SetValue(m_list_ctrl->GetItemText(sel, 0));
    m_cmd_entry->SetValue(m_list_ctrl->GetItemText(sel, 1));

    /* Delete old entry so user can re-add modified version */
    m_list_ctrl->DeleteItem(sel);
    m_name_entry->SetFocus();
}

bool EditListDialog::ShowItemDialog(wxString &name, wxString &cmd,
                                    const wxString &dlg_title)
{
    wxDialog dlg(this, wxID_ANY, dlg_title,
                 wxDefaultPosition, wxSize(400, 160),
                 wxDEFAULT_DIALOG_STYLE);

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(2, 2, 4, 8);
    grid->AddGrowableCol(1, 1);

    grid->Add(new wxStaticText(&dlg, wxID_ANY, m_title1 + ":"),
              0, wxALIGN_CENTER_VERTICAL);
    auto *name_entry = new wxTextCtrl(&dlg, wxID_ANY, name);
    grid->Add(name_entry, 1, wxEXPAND);

    grid->Add(new wxStaticText(&dlg, wxID_ANY, m_title2 + ":"),
              0, wxALIGN_CENTER_VERTICAL);
    auto *cmd_entry = new wxTextCtrl(&dlg, wxID_ANY, cmd);
    grid->Add(cmd_entry, 1, wxEXPAND);

    sizer->Add(grid, 0, wxEXPAND | wxALL, 10);
    sizer->Add(dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL),
               0, wxEXPAND | wxALL, 5);
    dlg.SetSizer(sizer);

    if (dlg.ShowModal() != wxID_OK)
        return false;

    name = name_entry->GetValue().Trim();
    cmd = cmd_entry->GetValue().Trim();
    return !name.IsEmpty();
}
