/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Plugins and Scripts Dialog
 */

#include "PluginDialog.h"
#include "fe-wx.h"

#include <glib.h>
#include <wx/filename.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/outbound.h"
#include "../common/cfgfiles.h"
}

#define PLUGIN_C
#ifndef PCHAT_CONTEXT_DEFINED
#define PCHAT_CONTEXT_DEFINED
typedef struct session pchat_context;
#endif

extern "C" {
#include "../common/pchat-plugin.h"
#include "../common/plugin.h"
extern GSList *plugin_list;
}

/* Singleton instance pointer */
PluginDialog *PluginDialog::s_instance = nullptr;

wxBEGIN_EVENT_TABLE(PluginDialog, wxDialog)
    EVT_BUTTON(ID_PLUGIN_LOAD, PluginDialog::OnLoad)
    EVT_BUTTON(ID_PLUGIN_UNLOAD, PluginDialog::OnUnload)
    EVT_BUTTON(ID_PLUGIN_RELOAD, PluginDialog::OnReload)
    EVT_CLOSE(PluginDialog::OnCloseWindow)
wxEND_EVENT_TABLE()

/* Helper: return just the filename part of a path */
static const char *file_part(const char *path)
{
    const char *p = strrchr(path, '/');
#ifdef _WIN32
    const char *q = strrchr(path, '\\');
    if (q && (!p || q > p)) p = q;
#endif
    return p ? p + 1 : path;
}

/* Helper: check if a filename is a native plugin (.dll / .so / .dylib) */
static bool is_native_plugin(const char *filename)
{
    if (!filename) return false;
    int len = (int)strlen(filename);
#ifdef _WIN32
    return (len > 4 && g_ascii_strcasecmp(filename + len - 4, ".dll") == 0);
#elif defined(__APPLE__)
    return (len > 6 && g_ascii_strcasecmp(filename + len - 6, ".dylib") == 0) ||
           (len > 3 && g_ascii_strcasecmp(filename + len - 3, ".so") == 0);
#else
    return (len > 3 && g_ascii_strcasecmp(filename + len - 3, ".so") == 0);
#endif
}

PluginDialog::PluginDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY,
               wxString(DISPLAY_NAME ": Plugins and Scripts"),
               wxDefaultPosition, wxSize(600, 380),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX)
{
    s_instance = this;

    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* Plugin list */
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, wxT("Name"), wxLIST_FORMAT_LEFT, 140);
    m_list->InsertColumn(1, wxT("Version"), wxLIST_FORMAT_LEFT, 70);
    m_list->InsertColumn(2, wxT("File"), wxLIST_FORMAT_LEFT, 160);
    m_list->InsertColumn(3, wxT("Description"), wxLIST_FORMAT_LEFT, 200);

    mainSizer->Add(m_list, 1, wxEXPAND | wxALL, 4);

    /* Buttons — evenly spread */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, ID_PLUGIN_LOAD, wxT("Load...")), 0);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, ID_PLUGIN_UNLOAD, wxT("Unload")), 0);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, ID_PLUGIN_RELOAD, wxT("Reload")), 0);
    btnSizer->AddStretchSpacer();

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 4);

    SetSizer(mainSizer);
    Centre();

    PopulateList();
}

PluginDialog::~PluginDialog()
{
    s_instance = nullptr;
}

void PluginDialog::RefreshList()
{
    PopulateList();
}

void PluginDialog::PopulateList()
{
    m_list->DeleteAllItems();

    GSList *list = plugin_list;
    int idx = 0;
    while (list) {
        pchat_plugin *pl = (pchat_plugin *)list->data;
        /* Only show plugins that have a version string (skip internal ones) */
        if (pl->version[0] != 0) {
            m_list->InsertItem(idx, wxString::FromUTF8(pl->name));
            m_list->SetItem(idx, 1, wxString::FromUTF8(pl->version));
            m_list->SetItem(idx, 2, wxString::FromUTF8(file_part(pl->filename)));
            m_list->SetItem(idx, 3, wxString::FromUTF8(pl->desc));
            idx++;
        }
        list = list->next;
    }
}

void PluginDialog::OnLoad(wxCommandEvent &event)
{
    wxString wildcard;
#ifdef _WIN32
    wildcard = wxT("Plugins (*.dll)|*.dll|Scripts (*.py;*.lua;*.pl)|*.py;*.lua;*.pl|All files (*.*)|*.*");
#else
    wildcard = wxT("Plugins (*.so)|*.so|Scripts (*.py;*.lua;*.pl)|*.py;*.lua;*.pl|All files (*.*)|*.*");
#endif

    /* Default to the addons directory */
    wxString defaultDir = wxString::FromUTF8(get_xdir());
    defaultDir += wxFileName::GetPathSeparator();
    defaultDir += wxT("addons");

    wxFileDialog dlg(this, wxT("Load Plugin or Script"),
                      defaultDir, wxEmptyString, wildcard,
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        wxString path = dlg.GetPath();
        if (current_sess) {
            char *buf;
            wxCharBuffer utf8 = path.utf8_str();
            if (strchr(utf8.data(), ' '))
                buf = g_strdup_printf("LOAD \"%s\"", utf8.data());
            else
                buf = g_strdup_printf("LOAD %s", utf8.data());
            handle_command(current_sess, buf, FALSE);
            g_free(buf);
        }
        /* List refreshes automatically via fe_pluginlist_update callback */
    }
}

void PluginDialog::OnUnload(wxCommandEvent &event)
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    wxString name = m_list->GetItemText(sel, 0);  /* Name column */
    wxString file = m_list->GetItemText(sel, 2);  /* File column */

    wxCharBuffer fileUtf8 = file.utf8_str();
    wxCharBuffer nameUtf8 = name.utf8_str();

    if (is_native_plugin(fileUtf8.data())) {
        /* Native plugin: use plugin_kill directly */
        if (plugin_kill((char *)nameUtf8.data(), FALSE) == 2) {
            wxMessageBox(_("That plugin is refusing to unload."),
                         wxT("PChat"), wxICON_ERROR, this);
        }
    } else {
        /* Script: let the interpreter plugin handle it */
        char *buf;
        if (strchr(fileUtf8.data(), ' '))
            buf = g_strdup_printf("UNLOAD \"%s\"", fileUtf8.data());
        else
            buf = g_strdup_printf("UNLOAD %s", fileUtf8.data());
        if (current_sess)
            handle_command(current_sess, buf, FALSE);
        g_free(buf);
    }
    /* List refreshes automatically via fe_pluginlist_update callback */
}

void PluginDialog::OnReload(wxCommandEvent &event)
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    wxString file = m_list->GetItemText(sel, 2);  /* File column */
    wxCharBuffer fileUtf8 = file.utf8_str();

    char *buf;
    if (strchr(fileUtf8.data(), ' '))
        buf = g_strdup_printf("RELOAD \"%s\"", fileUtf8.data());
    else
        buf = g_strdup_printf("RELOAD %s", fileUtf8.data());
    if (current_sess)
        handle_command(current_sess, buf, FALSE);
    g_free(buf);
    /* List refreshes automatically via fe_pluginlist_update callback */
}

void PluginDialog::OnCloseWindow(wxCloseEvent &event)
{
    Destroy();
}
