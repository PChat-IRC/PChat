/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * KeyboardShortcutsDialog - Key bindings editor
 * Replicates the GTK3 fkeys.c key_dialog functionality.
 *
 * File format (keybindings.conf):
 *   ACCEL=<accel_string>
 *   <action_name>
 *   D1:<data1> or D1!
 *   D2:<data2> or D2!
 *   <blank line>
 */

#include "KeyboardShortcutsDialog.h"

extern "C" {
#include "../common/pchat.h"
#include "../common/cfgfiles.h"
}

#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Action names matching GTK3 key_actions[] */
const char *KeyboardShortcutsDialog::s_action_names[NUM_ACTIONS] = {
    "Run Command",
    "Change Page",
    "Insert in Buffer",
    "Scroll Page",
    "Set Buffer",
    "Last Command",
    "Next Command",
    "Complete nick/command",
    "Change Selected Nick",
    "Check For Replace",
    "Move front tab left",
    "Move front tab right",
    "Move tab family left",
    "Move tab family right",
    "Push input line into history",
};

const char *KeyboardShortcutsDialog::s_action_help[NUM_ACTIONS] = {
    "Runs Data 1 as if typed into the entry box. Use \\n to delimit multiple commands.",
    "Switches notebook pages. Data 1 = page number or 'auto'. Data 2 = 'Relative' for relative switching.",
    "Inserts Data 1 at cursor position in the input entry.",
    "Scrolls text widget. Data 1 = Top, Bottom, Up, Down, +1 or -1.",
    "Sets entry contents to Data 1.",
    "Sets entry to the previous command (shell up-arrow).",
    "Sets entry to the next command (shell down-arrow).",
    "Tab-completes a nick or command. Data 1 set = double-tab selects last nick.",
    "Scrolls through nick list. Data 1 set = scroll up, else down.",
    "Checks last word against replace list and replaces if matched.",
    "Moves the front tab left by one.",
    "Moves the front tab right by one.",
    "Moves the current tab family to the left.",
    "Moves the current tab family to the right.",
    "Pushes input line into history without sending to server.",
};

wxBEGIN_EVENT_TABLE(KeyboardShortcutsDialog, wxDialog)
    EVT_BUTTON(ID_KB_ADD, KeyboardShortcutsDialog::OnAdd)
    EVT_BUTTON(ID_KB_DELETE, KeyboardShortcutsDialog::OnDelete)
    EVT_BUTTON(ID_KB_SAVE, KeyboardShortcutsDialog::OnSave)
    EVT_BUTTON(wxID_CANCEL, KeyboardShortcutsDialog::OnCancel)
    EVT_LIST_ITEM_SELECTED(ID_KB_LIST, KeyboardShortcutsDialog::OnItemSelected)
wxEND_EVENT_TABLE()

KeyboardShortcutsDialog::KeyboardShortcutsDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Keyboard Shortcuts",
               wxDefaultPosition, wxSize(650, 500),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    auto *main_sizer = new wxBoxSizer(wxVERTICAL);

    /* --- List control (4 columns: Accel, Action, Data1, Data2) --- */
    m_list_ctrl = new wxListCtrl(this, ID_KB_LIST, wxDefaultPosition, wxDefaultSize,
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list_ctrl->AppendColumn("Key", wxLIST_FORMAT_LEFT, 150);
    m_list_ctrl->AppendColumn("Action", wxLIST_FORMAT_LEFT, 180);
    m_list_ctrl->AppendColumn("Data 1", wxLIST_FORMAT_LEFT, 130);
    m_list_ctrl->AppendColumn("Data 2", wxLIST_FORMAT_LEFT, 130);
    main_sizer->Add(m_list_ctrl, 1, wxEXPAND | wxALL, 5);

    /* --- Edit controls --- */
    auto *edit_sizer = new wxFlexGridSizer(4, 2, 4, 8);
    edit_sizer->AddGrowableCol(1, 1);

    /* Accelerator key entry */
    edit_sizer->Add(new wxStaticText(this, wxID_ANY, "Key:"),
                    0, wxALIGN_CENTER_VERTICAL);
    m_accel_entry = new wxTextCtrl(this, wxID_ANY, "",
                                   wxDefaultPosition, wxDefaultSize,
                                   wxTE_READONLY);
    m_accel_entry->SetBackgroundColour(wxColour(240, 240, 255));
    m_accel_entry->SetHint("Press a key combination...");
    m_accel_entry->Bind(wxEVT_KEY_DOWN, &KeyboardShortcutsDialog::OnAccelKeyDown, this);
    edit_sizer->Add(m_accel_entry, 1, wxEXPAND);

    /* Action combo */
    edit_sizer->Add(new wxStaticText(this, wxID_ANY, "Action:"),
                    0, wxALIGN_CENTER_VERTICAL);
    wxArrayString actions;
    for (int i = 0; i < NUM_ACTIONS; i++)
        actions.Add(s_action_names[i]);
    m_action_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition,
                                    wxDefaultSize, actions);
    m_action_choice->SetSelection(0);
    edit_sizer->Add(m_action_choice, 1, wxEXPAND);

    /* Data 1 */
    edit_sizer->Add(new wxStaticText(this, wxID_ANY, "Data 1:"),
                    0, wxALIGN_CENTER_VERTICAL);
    m_data1_entry = new wxTextCtrl(this, wxID_ANY);
    edit_sizer->Add(m_data1_entry, 1, wxEXPAND);

    /* Data 2 */
    edit_sizer->Add(new wxStaticText(this, wxID_ANY, "Data 2:"),
                    0, wxALIGN_CENTER_VERTICAL);
    m_data2_entry = new wxTextCtrl(this, wxID_ANY);
    edit_sizer->Add(m_data2_entry, 1, wxEXPAND);

    main_sizer->Add(edit_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

    /* --- Help text area --- */
    m_help_text = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                 wxSize(-1, 60), wxTE_MULTILINE | wxTE_READONLY);
    m_help_text->SetValue("Select a row to see help information on its action.");
    main_sizer->Add(m_help_text, 0, wxEXPAND | wxALL, 5);

    /* --- Buttons --- */
    auto *btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->Add(new wxButton(this, ID_KB_ADD, "Add"), 0, wxRIGHT, 4);
    btn_sizer->Add(new wxButton(this, ID_KB_DELETE, "Delete"), 0, wxRIGHT, 4);
    btn_sizer->AddStretchSpacer();
    btn_sizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxRIGHT, 4);
    btn_sizer->Add(new wxButton(this, ID_KB_SAVE, "Save"), 0);
    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(main_sizer);

    LoadKeyBindings();
    PopulateList();
    Centre();
}

KeyboardShortcutsDialog::~KeyboardShortcutsDialog()
{
    for (auto &kb : m_bindings) {
        g_free(kb.accel);
        g_free(kb.data1);
        g_free(kb.data2);
    }
}

/* Convert a wx key event to a GTK-style accelerator string */
static wxString KeyEventToAccelString(wxKeyEvent &evt)
{
    wxString accel;
    int mods = evt.GetModifiers();
    int key = evt.GetKeyCode();

    if (mods & wxMOD_CONTROL) accel += "<Control>";
    if (mods & wxMOD_ALT)     accel += "<Alt>";
    if (mods & wxMOD_SHIFT)   accel += "<Shift>";

    /* Map wxWidgets key codes to GTK-style key names */
    switch (key) {
    case WXK_F1:  accel += "F1"; break;
    case WXK_F2:  accel += "F2"; break;
    case WXK_F3:  accel += "F3"; break;
    case WXK_F4:  accel += "F4"; break;
    case WXK_F5:  accel += "F5"; break;
    case WXK_F6:  accel += "F6"; break;
    case WXK_F7:  accel += "F7"; break;
    case WXK_F8:  accel += "F8"; break;
    case WXK_F9:  accel += "F9"; break;
    case WXK_F10: accel += "F10"; break;
    case WXK_F11: accel += "F11"; break;
    case WXK_F12: accel += "F12"; break;
    case WXK_TAB: accel += "Tab"; break;
    case WXK_RETURN: accel += "Return"; break;
    case WXK_NUMPAD_ENTER: accel += "KP_Enter"; break;
    case WXK_ESCAPE: accel += "Escape"; break;
    case WXK_SPACE: accel += "space"; break;
    case WXK_BACK: accel += "BackSpace"; break;
    case WXK_DELETE: accel += "Delete"; break;
    case WXK_INSERT: accel += "Insert"; break;
    case WXK_HOME: accel += "Home"; break;
    case WXK_END: accel += "End"; break;
    case WXK_PAGEUP: accel += "Prior"; break;
    case WXK_PAGEDOWN: accel += "Next"; break;
    case WXK_UP: accel += "Up"; break;
    case WXK_DOWN: accel += "Down"; break;
    case WXK_LEFT: accel += "Left"; break;
    case WXK_RIGHT: accel += "Right"; break;
    default:
        if (key >= 'A' && key <= 'Z') {
            /* wxWidgets always gives uppercase for alpha keys */
            accel += (char)('a' + (key - 'A'));
        } else if (key >= '0' && key <= '9') {
            accel += (char)key;
        } else if (key == '`') {
            accel += "grave";
        } else if (key == '-') {
            accel += "minus";
        } else if (key == '=') {
            accel += "equal";
        } else if (key == '[') {
            accel += "bracketleft";
        } else if (key == ']') {
            accel += "bracketright";
        } else if (key == '\\') {
            accel += "backslash";
        } else if (key == ';') {
            accel += "semicolon";
        } else if (key == '\'') {
            accel += "apostrophe";
        } else if (key == ',') {
            accel += "comma";
        } else if (key == '.') {
            accel += "period";
        } else if (key == '/') {
            accel += "slash";
        } else {
            /* Unknown key, use hex code */
            accel += wxString::Format("0x%x", key);
        }
        break;
    }

    return accel;
}

void KeyboardShortcutsDialog::OnAccelKeyDown(wxKeyEvent &evt)
{
    int key = evt.GetKeyCode();

    /* Ignore modifier-only presses */
    if (key == WXK_SHIFT || key == WXK_CONTROL || key == WXK_ALT ||
        key == WXK_RAW_CONTROL || key == WXK_WINDOWS_LEFT ||
        key == WXK_WINDOWS_RIGHT) {
        evt.Skip();
        return;
    }

    wxString accel = KeyEventToAccelString(evt);
    m_accel_entry->SetValue(accel);
    /* Don't skip - we consumed the key */
}

void KeyboardShortcutsDialog::LoadKeyBindings()
{
    /* Clear existing */
    for (auto &kb : m_bindings) {
        g_free(kb.accel);
        g_free(kb.data1);
        g_free(kb.data2);
    }
    m_bindings.clear();

    /* Read keybindings.conf */
    int fd = pchat_open_file("keybindings.conf", O_RDONLY, 0, 0);
    if (fd < 0)
        return;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return;
    }

    char *buf = (char *)g_malloc(st.st_size + 1);
    read(fd, buf, st.st_size);
    buf[st.st_size] = '\0';
    close(fd);

    /* Parse the file: state machine matching GTK3 key_load_kbs */
    enum { ST_ACCEL, ST_ACTION, ST_D1, ST_D2 };
    int state = ST_ACCEL;
    wx_key_binding kb = {};

    char *line = strtok(buf, "\n");
    while (line) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\0') {
            line = strtok(nullptr, "\n");
            continue;
        }

        /* Strip trailing whitespace */
        int len = strlen(line);
        while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t' ||
                           line[len - 1] == '\r'))
            line[--len] = '\0';

        switch (state) {
        case ST_ACCEL:
            if (strncmp(line, "ACCEL=", 6) == 0) {
                kb.accel = g_strdup(line + 6);
                state = ST_ACTION;
            } else {
                /* Legacy format: modifier line (C, A, S, CS, None) */
                /* Try to handle old format */
                kb.accel = nullptr;
                /* Skip legacy entries for now */
                state = ST_ACCEL;
            }
            break;

        case ST_ACTION: {
            kb.action = -1;
            for (int i = 0; i < NUM_ACTIONS; i++) {
                if (strcmp(line, s_action_names[i]) == 0) {
                    kb.action = i;
                    break;
                }
            }
            if (kb.action == -1) {
                /* Unknown action, skip */
                g_free(kb.accel);
                kb.accel = nullptr;
                state = ST_ACCEL;
            } else {
                state = ST_D1;
            }
            break;
        }

        case ST_D1:
            if (strncmp(line, "D1:", 3) == 0)
                kb.data1 = g_strdup(line + 3);
            else
                kb.data1 = g_strdup("");
            state = ST_D2;
            break;

        case ST_D2:
            if (strncmp(line, "D2:", 3) == 0)
                kb.data2 = g_strdup(line + 3);
            else
                kb.data2 = g_strdup("");

            /* Complete entry - add to list */
            m_bindings.push_back(kb);
            memset(&kb, 0, sizeof(kb));
            state = ST_ACCEL;
            break;
        }

        line = strtok(nullptr, "\n");
    }

    g_free(buf);
}

void KeyboardShortcutsDialog::SaveKeyBindings()
{
    int fd = pchat_open_file("keybindings.conf",
                             O_CREAT | O_TRUNC | O_WRONLY | O_BINARY,
                             0600, XOF_DOMODE);
    if (fd < 0) {
        wxMessageBox("Failed to save key bindings.", "PChat",
                     wxOK | wxICON_ERROR, this);
        return;
    }

    const char *header = "# PChat key bindings config file\n\n";
    write(fd, header, strlen(header));

    int count = m_list_ctrl->GetItemCount();
    for (int i = 0; i < count; i++) {
        wxString accel = m_list_ctrl->GetItemText(i, 0);
        wxString action = m_list_ctrl->GetItemText(i, 1);
        wxString d1 = m_list_ctrl->GetItemText(i, 2);
        wxString d2 = m_list_ctrl->GetItemText(i, 3);

        auto accel_u = accel.ToUTF8();
        auto action_u = action.ToUTF8();
        auto d1_u = d1.ToUTF8();
        auto d2_u = d2.ToUTF8();

        /* ACCEL=<accel>\n<action>\n */
        char buf[512];
        snprintf(buf, sizeof(buf), "ACCEL=%s\n%s\n",
                 accel_u.data(), action_u.data());
        write(fd, buf, strlen(buf));

        /* D1: or D1! */
        if (d1.IsEmpty()) {
            write(fd, "D1!\n", 4);
        } else {
            snprintf(buf, sizeof(buf), "D1:%s\n", d1_u.data());
            write(fd, buf, strlen(buf));
        }

        /* D2: or D2! */
        if (d2.IsEmpty()) {
            write(fd, "D2!\n", 4);
        } else {
            snprintf(buf, sizeof(buf), "D2:%s\n", d2_u.data());
            write(fd, buf, strlen(buf));
        }

        write(fd, "\n", 1);
    }

    close(fd);
}

void KeyboardShortcutsDialog::PopulateList()
{
    m_list_ctrl->DeleteAllItems();

    for (int i = 0; i < (int)m_bindings.size(); i++) {
        const auto &kb = m_bindings[i];
        long idx = m_list_ctrl->InsertItem(i, wxString::FromUTF8(kb.accel ? kb.accel : ""));

        if (kb.action >= 0 && kb.action < NUM_ACTIONS)
            m_list_ctrl->SetItem(idx, 1, s_action_names[kb.action]);
        else
            m_list_ctrl->SetItem(idx, 1, "Unknown");

        m_list_ctrl->SetItem(idx, 2, wxString::FromUTF8(kb.data1 ? kb.data1 : ""));
        m_list_ctrl->SetItem(idx, 3, wxString::FromUTF8(kb.data2 ? kb.data2 : ""));
    }
}

void KeyboardShortcutsDialog::UpdateHelp(int action_index)
{
    if (action_index >= 0 && action_index < NUM_ACTIONS)
        m_help_text->SetValue(s_action_help[action_index]);
    else
        m_help_text->SetValue("Select a row to see help information on its action.");
}

void KeyboardShortcutsDialog::OnItemSelected(wxListEvent &event)
{
    long sel = event.GetIndex();
    if (sel == -1)
        return;

    m_accel_entry->SetValue(m_list_ctrl->GetItemText(sel, 0));
    wxString action_name = m_list_ctrl->GetItemText(sel, 1);
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (action_name == s_action_names[i]) {
            m_action_choice->SetSelection(i);
            UpdateHelp(i);
            break;
        }
    }
    m_data1_entry->SetValue(m_list_ctrl->GetItemText(sel, 2));
    m_data2_entry->SetValue(m_list_ctrl->GetItemText(sel, 3));
}

void KeyboardShortcutsDialog::OnAdd(wxCommandEvent &event)
{
    wxString accel = m_accel_entry->GetValue().Trim();
    if (accel.IsEmpty()) {
        wxMessageBox("Press a key combination first.", "PChat",
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    int action = m_action_choice->GetSelection();
    if (action < 0 || action >= NUM_ACTIONS)
        return;

    wxString d1 = m_data1_entry->GetValue();
    wxString d2 = m_data2_entry->GetValue();

    long idx = m_list_ctrl->InsertItem(m_list_ctrl->GetItemCount(), accel);
    m_list_ctrl->SetItem(idx, 1, s_action_names[action]);
    m_list_ctrl->SetItem(idx, 2, d1);
    m_list_ctrl->SetItem(idx, 3, d2);

    /* Clear entries */
    m_accel_entry->Clear();
    m_data1_entry->Clear();
    m_data2_entry->Clear();

    m_list_ctrl->EnsureVisible(idx);
}

void KeyboardShortcutsDialog::OnDelete(wxCommandEvent &event)
{
    long sel = m_list_ctrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1)
        return;

    m_list_ctrl->DeleteItem(sel);

    int count = m_list_ctrl->GetItemCount();
    if (count > 0) {
        if (sel >= count)
            sel = count - 1;
        m_list_ctrl->SetItemState(sel, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }
}

void KeyboardShortcutsDialog::OnSave(wxCommandEvent &event)
{
    SaveKeyBindings();
    EndModal(wxID_OK);
}

void KeyboardShortcutsDialog::OnCancel(wxCommandEvent &event)
{
    EndModal(wxID_CANCEL);
}
