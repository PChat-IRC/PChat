/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Plugin and Scripts Dialog
 */

#ifndef PCHAT_PLUGINDIALOG_H
#define PCHAT_PLUGINDIALOG_H

#include <wx/wx.h>
#include <wx/listctrl.h>

class PluginDialog : public wxDialog
{
public:
    PluginDialog(wxWindow *parent);
    ~PluginDialog();

    void RefreshList();

    /* Singleton-style access — only one dialog at a time */
    static PluginDialog *GetInstance() { return s_instance; }

private:
    void PopulateList();
    void OnCloseWindow(wxCloseEvent &event);
    void OnLoad(wxCommandEvent &event);
    void OnUnload(wxCommandEvent &event);
    void OnReload(wxCommandEvent &event);

    wxListCtrl *m_list;

    static PluginDialog *s_instance;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_PLUGIN_LOAD = wxID_HIGHEST + 800,
    ID_PLUGIN_UNLOAD,
    ID_PLUGIN_RELOAD,
};

#endif /* PCHAT_PLUGINDIALOG_H */
