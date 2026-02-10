/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Network List Dialog - replicates HexChat's server list
 * Shows User Information fields combined with Networks list
 */

#ifndef PCHAT_NETWORKLISTDIALOG_H
#define PCHAT_NETWORKLISTDIALOG_H

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/combobox.h>

#include <glib.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/servlist.h"
}

class NetworkListDialog : public wxDialog
{
public:
    NetworkListDialog(wxWindow *parent);
    ~NetworkListDialog();

    void RefreshNetworkList();

private:
    void CreateLayout();

    /* Event handlers */
    void OnAdd(wxCommandEvent &event);
    void OnRemove(wxCommandEvent &event);
    void OnEdit(wxCommandEvent &event);
    void OnSort(wxCommandEvent &event);
    void OnFavor(wxCommandEvent &event);
    void OnConnect(wxCommandEvent &event);
    void OnClose(wxCommandEvent &event);
    void OnNetworkSelected(wxCommandEvent &event);

    /* User Information fields */
    wxTextCtrl *m_nick_entry;
    wxTextCtrl *m_second_entry;
    wxTextCtrl *m_third_entry;
    wxTextCtrl *m_username_entry;

    /* Networks list */
    wxListBox *m_network_list;

    /* Options */
    wxCheckBox *m_skip_on_startup;
    wxCheckBox *m_show_favorites;

    /* Buttons */
    wxButton *m_add_btn;
    wxButton *m_remove_btn;
    wxButton *m_edit_btn;
    wxButton *m_sort_btn;
    wxButton *m_favor_btn;
    wxButton *m_close_btn;
    wxButton *m_connect_btn;

    /* Currently selected network */
    ircnet *m_selected_net = nullptr;

    wxDECLARE_EVENT_TABLE();
};

/* Network Edit Dialog - matches HexChat layout exactly */
class NetworkEditDialog : public wxDialog
{
public:
    NetworkEditDialog(wxWindow *parent, ircnet *net);
    ~NetworkEditDialog();

private:
    void CreateLayout();
    void PopulateFields();
    void SaveFields();
    void UpdateIdentitySensitivity();

    void OnClose(wxCommandEvent &event);
    void OnCloseWindow(wxCloseEvent &event);
    void OnAddButton(wxCommandEvent &event);
    void OnRemoveButton(wxCommandEvent &event);
    void OnEditButton(wxCommandEvent &event);
    void OnUseGlobalToggle(wxCommandEvent &event);
    void OnLoginMethodChanged(wxCommandEvent &event);

    ircnet *m_net;

    /* Notebook with 3 tabs: Servers, Autojoin channels, Connect commands */
    wxNotebook *m_notebook;

    /* Server list (Servers tab) */
    wxListBox *m_server_list;

    /* Channel list (Autojoin channels tab) */
    wxListBox *m_channel_list;

    /* Command list (Connect commands tab) */
    wxListBox *m_command_list;

    /* Shared buttons for all tabs */
    wxButton *m_add_btn;
    wxButton *m_remove_btn;
    wxButton *m_edit_btn;

    /* Checkboxes */
    wxCheckBox *m_connect_selected;     /* Connect to selected server only */
    wxCheckBox *m_auto_connect;         /* Connect to this network automatically */
    wxCheckBox *m_bypass_proxy;         /* Bypass proxy server */
    wxCheckBox *m_use_ssl;              /* Use SSL for all servers */
    wxCheckBox *m_accept_invalid_cert;  /* Accept invalid SSL certificates */
    wxCheckBox *m_use_global_info;      /* Use global user information */

    /* Per-network identity */
    wxStaticText *m_nick_label;
    wxTextCtrl *m_nick_entry;
    wxStaticText *m_nick2_label;
    wxTextCtrl *m_nick2_entry;
    wxStaticText *m_realname_label;
    wxTextCtrl *m_realname_entry;
    wxStaticText *m_username_label;
    wxTextCtrl *m_username_entry;

    /* Login */
    wxChoice *m_login_method;
    wxTextCtrl *m_password_entry;

    /* Encoding */
    wxComboBox *m_encoding;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_NET_ADD = wxID_HIGHEST + 100,
    ID_NET_REMOVE,
    ID_NET_EDIT,
    ID_NET_SORT,
    ID_NET_FAVOR,
    ID_NET_CONNECT,
    ID_NET_LIST,
    ID_NET_SKIP_STARTUP,
    ID_NET_SHOW_FAV,
    /* Edit dialog */
    ID_EDIT_ADD,
    ID_EDIT_REMOVE,
    ID_EDIT_EDIT,
    ID_EDIT_USE_GLOBAL,
    ID_EDIT_LOGIN_METHOD,
};

#endif /* PCHAT_NETWORKLISTDIALOG_H */
