/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * KeyboardShortcutsDialog - Key bindings editor
 * Replicates the GTK3 fkeys.c key_dialog_show() functionality.
 */

#ifndef PCHAT_KEYBOARDSHORTCUTSDIALOG_H
#define PCHAT_KEYBOARDSHORTCUTSDIALOG_H

#include <wx/wx.h>
#include <wx/listctrl.h>

#include <glib.h>

/* Forward-declare struct key_binding since it is defined in
   fe-gtk3/fkeys.c (static). We replicate the minimal structure here. */
struct wx_key_binding
{
    char *accel;       /* Accelerator string, e.g. "<Control>a" */
    int action;        /* Index into action names */
    char *data1;
    char *data2;
};

class KeyboardShortcutsDialog : public wxDialog
{
public:
    KeyboardShortcutsDialog(wxWindow *parent);
    ~KeyboardShortcutsDialog();

    /* Key action names (matching GTK3 key_actions[]) */
    static const int NUM_ACTIONS = 15;
    static const char *s_action_names[NUM_ACTIONS];
    static const char *s_action_help[NUM_ACTIONS];

private:
    void LoadKeyBindings();
    void SaveKeyBindings();
    void PopulateList();
    void UpdateHelp(int action_index);

    void OnAdd(wxCommandEvent &event);
    void OnDelete(wxCommandEvent &event);
    void OnSave(wxCommandEvent &event);
    void OnCancel(wxCommandEvent &event);
    void OnItemSelected(wxListEvent &event);
    void OnAccelKeyDown(wxKeyEvent &event);

    wxListCtrl *m_list_ctrl;
    wxTextCtrl *m_accel_entry;
    wxChoice *m_action_choice;
    wxTextCtrl *m_data1_entry;
    wxTextCtrl *m_data2_entry;
    wxTextCtrl *m_help_text;

    /* In-memory copy of key bindings */
    std::vector<wx_key_binding> m_bindings;

    enum {
        ID_KB_ADD = wxID_HIGHEST + 600,
        ID_KB_DELETE,
        ID_KB_SAVE,
        ID_KB_LIST,
    };

    wxDECLARE_EVENT_TABLE();
};

#endif /* PCHAT_KEYBOARDSHORTCUTSDIALOG_H */
