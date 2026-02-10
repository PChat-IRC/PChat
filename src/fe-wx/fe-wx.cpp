/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Core frontend implementation - implements fe_* callbacks from fe.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

#include <wx/wx.h>
#include <wx/filename.h>

#include <glib.h>
#include <glib/gi18n.h>

/* Backend headers */
extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/fe.h"
#include "../common/cfgfiles.h"
#include "../common/servlist.h"
#include "../common/outbound.h"
#include "../common/server.h"
#include "../common/text.h"
#include "../common/util.h"
#include "../common/dcc.h"
#include "../common/notify.h"
#include "../common/ignore.h"
#include "../common/url.h"
}

#include "fe-wx.h"
#include "MainWindow.h"
#include "PchatApp.h"
#include "NetworkListDialog.h"
#include "RawLogDialog.h"
#include "DccDialog.h"
#include "UrlGrabberDialog.h"
#include "FriendsListDialog.h"
#include "IgnoreListDialog.h"
#include "ChannelListDialog.h"
#include "JoinDialog.h"
#include "PluginDialog.h"

/* The global wx app instance accessor */
extern PchatApp *g_app;
extern MainWindow *g_main_window;

/* ===== Frontend API Implementation ===== */

/* Saved argc/argv for wxEntry */
static int s_argc = 0;
static char **s_argv = nullptr;

extern "C" {

int fe_args(int argc, char *argv[])
{
    /* Save argc/argv for wxEntryStart later.
       The backend's main() calls us before fe_init()/fe_main().
       We initialize wxWidgets here so that wxApp::OnInit() runs. */
    s_argc = argc;
    s_argv = argv;

    /* Initialize wxWidgets subsystem.
       wxEntryStart creates the wxApp instance (PchatApp) via
       wxIMPLEMENT_APP_NO_MAIN and calls OnInit(). */
    wxEntryStart(s_argc, s_argv);
    if (!wxTheApp || !wxTheApp->CallOnInit()) {
        /* wxWidgets init failed */
        return 1;
    }

    return -1; /* -1 means continue to fe_init */
}

void fe_init(void)
{
    /* The main window was already created in PchatApp::OnInit().
       Any additional frontend init after config is loaded goes here. */
}

void fe_main(void)
{
    /* Run the wxWidgets event loop — this blocks until the app exits. */
    if (wxTheApp) {
        wxTheApp->OnRun();
        wxTheApp->OnExit();
        wxEntryCleanup();
    }
}

void fe_cleanup(void)
{
    /* Cleanup handled by wxApp::OnExit */
}

void fe_exit(void)
{
    /* Break out of the wxWidgets event loop.
       pchat_exit() calls this at the very end after all cleanup is done.
       Do NOT call Close() here -- that re-enters OnClose and causes a hang. */
    if (g_main_window) {
        g_main_window->Destroy();
        g_main_window = nullptr;
    }
    if (wxTheApp) {
        wxTheApp->ExitMainLoop();
    }
}

int fe_timeout_add(int interval, void *callback, void *userdata)
{
    if (g_app) {
        return g_app->AddTimer(interval, (GSourceFunc)callback, userdata);
    }
    return 0;
}

int fe_timeout_add_seconds(int interval, void *callback, void *userdata)
{
    return fe_timeout_add(interval * 1000, callback, userdata);
}

void fe_timeout_remove(int tag)
{
    if (g_app) {
        g_app->RemoveTimer(tag);
    }
}

void fe_new_window(struct session *sess, int focus)
{
    if (!g_main_window)
        return;

    sess->gui = (session_gui *)g_new0(session_gui, 1);
    sess->res = (restore_gui *)g_new0(restore_gui, 1);
    sess->gui->main_window = g_main_window;

    g_main_window->AddSession(sess, focus);
}

void fe_new_server(struct server *serv)
{
    serv->gui = (server_gui *)g_new0(server_gui, 1);
}

void fe_add_rawlog(struct server *serv, char *text, int len, int outbound)
{
    if (g_main_window) {
        if (serv->gui && serv->gui->rawlog_window) {
            wxString wxText(text, wxConvUTF8, len);
            RawLogDialog *dlg = (RawLogDialog *)serv->gui->rawlog_window;
            dlg->AppendText(wxText, outbound != 0);
        }
    }
}

void fe_message(char *msg, int flags)
{
    if (!g_main_window) return;

    wxString wxMsg = wxString::FromUTF8(msg);
    long style = wxOK;
    wxString title = wxT("PChat");

    if (flags & FE_MSG_ERROR) {
        style |= wxICON_ERROR;
        title = wxT("PChat - Error");
    } else if (flags & FE_MSG_WARN) {
        style |= wxICON_WARNING;
        title = wxT("PChat - Warning");
    } else {
        style |= wxICON_INFORMATION;
    }

    wxMessageBox(wxMsg, title, style, g_main_window);
}

int fe_input_add(int sok, int flags, void *func, void *data)
{
    if (g_app) {
        return g_app->AddSocketWatch(sok, flags, func, data);
    }
    return 0;
}

void fe_input_remove(int tag)
{
    if (g_app) {
        g_app->RemoveSocketWatch(tag);
    }
}

void fe_idle_add(void *func, void *data)
{
    /* Queue the callback for the next idle iteration of the wx event
       loop, matching GLib's g_idle_add() semantics.  The previous
       implementation called the callback synchronously which could
       cause re-entrancy issues. */
    if (wxTheApp) {
        auto callback = (int(*)(void *))func;
        auto ud = data;
        wxTheApp->CallAfter([callback, ud]() {
            callback(ud);
        });
    }
}

void fe_set_topic(struct session *sess, char *topic, char *stripped_topic)
{
    if (!g_main_window) return;
    wxString wxTopic = stripped_topic ? wxString::FromUTF8(stripped_topic) : wxString();
    g_main_window->SetTopic(sess, wxTopic);
}

void fe_set_tab_color(struct session *sess, tabcolor col)
{
    if (!g_main_window) return;
    int c = (int)col;
    g_main_window->SetTabColor(sess, c);
}

void fe_flash_window(struct session *sess)
{
    if (g_main_window) {
        g_main_window->RequestUserAttention();
    }
}

void fe_update_mode_buttons(struct session *sess, char mode, char sign)
{
    if (!g_main_window) return;
    g_main_window->UpdateModeButtons(sess, mode, sign);
}

void fe_update_channel_key(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->UpdateChannelKey(sess);
}

void fe_update_channel_limit(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->UpdateChannelLimit(sess);
}

int fe_is_chanwindow(struct server *serv)
{
    return (serv->gui && serv->gui->chanlist_window) ? 1 : 0;
}

void fe_add_chan_list(struct server *serv, char *chan, char *users, char *topic)
{
    if (!g_main_window) return;
    g_main_window->AddChanListRow(serv, chan, users, topic);
}

void fe_chan_list_end(struct server *serv)
{
    if (!g_main_window) return;
    g_main_window->ChanListEnd(serv);
}

gboolean fe_add_ban_list(struct session *sess, char *mask, char *who,
                          char *when, int rplcode)
{
    if (!g_main_window) return FALSE;
    return g_main_window->AddBanListEntry(sess, mask, who, when, rplcode) ? TRUE : FALSE;
}

gboolean fe_ban_list_end(struct session *sess, int rplcode)
{
    if (!g_main_window) return FALSE;
    return g_main_window->BanListEnd(sess, rplcode) ? TRUE : FALSE;
}

void fe_notify_update(char *name)
{
    if (!g_main_window) return;
    FriendsListDialog *dlg = g_main_window->GetFriendsListDialog();
    if (dlg && dlg->IsShown()) dlg->RefreshList();
}

void fe_notify_ask(char *name, char *networks)
{
    if (!g_main_window) return;

    /* Dialog to prompt the user for a nick + networks to add to the
       notify (friends) list.  Matches the GTK3 two-entry dialog. */
    wxDialog dlg(g_main_window, wxID_ANY, wxT("Add to Friends List"),
                 wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE);

    wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);

    /* Nick entry */
    wxStaticText *nickLabel = new wxStaticText(&dlg, wxID_ANY,
                                                wxT("Enter nickname to add:"));
    vbox->Add(nickLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
    wxTextCtrl *nickEntry = new wxTextCtrl(&dlg, wxID_ANY,
                                           name ? wxString::FromUTF8(name) : wxString());
    vbox->Add(nickEntry, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

    /* Network entry */
    wxStaticText *netLabel = new wxStaticText(&dlg, wxID_ANY,
                                              wxT("Notify on these networks:"));
    vbox->Add(netLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
    wxTextCtrl *netEntry = new wxTextCtrl(&dlg, wxID_ANY,
                                          (networks && networks[0])
                                              ? wxString::FromUTF8(networks)
                                              : wxT("ALL"));
    vbox->Add(netEntry, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

    wxStaticText *hint = new wxStaticText(&dlg, wxID_ANY,
        wxT("Comma separated list of networks is accepted."));
    wxFont hintFont = hint->GetFont();
    hintFont.SetPointSize(hintFont.GetPointSize() - 1);
    hintFont.SetStyle(wxFONTSTYLE_ITALIC);
    hint->SetFont(hintFont);
    vbox->Add(hint, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    /* OK / Cancel buttons */
    wxSizer *btnSizer = dlg.CreateButtonSizer(wxOK | wxCANCEL);
    vbox->Add(btnSizer, 0, wxALIGN_RIGHT | wxALL, 10);

    dlg.SetSizerAndFit(vbox);
    dlg.CentreOnParent();

    if (dlg.ShowModal() == wxID_OK) {
        wxString nick = nickEntry->GetValue().Trim();
        wxString net = netEntry->GetValue().Trim();
        if (!nick.IsEmpty()) {
            notify_adduser((char *)nick.utf8_str().data(),
                           net.IsEmpty() ? nullptr
                                         : (char *)net.utf8_str().data());
        }
    }
}

void fe_text_clear(struct session *sess, int lines)
{
    if (!g_main_window) return;
    g_main_window->ClearText(sess, lines);
}

void fe_close_window(struct session *sess)
{
    if (!g_main_window) return;

    /* Remove from the frontend's session list and tree.
       This must be synchronous — the backend's free_sessions() loop
       depends on immediate removal from sess_list.
       GTK3 does this synchronously too (mg_ircdestroy → session_free). */
    g_main_window->RemoveSession(sess);

    /* Tell the backend to remove from sess_list & free the session.
       session_free() will call fe_session_callback() which frees
       our gui/res structs — same pattern as GTK3's mg_ircdestroy. */
    session_free(sess);
}

void fe_progressbar_start(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->SetProgressBar(sess, true);
}

void fe_progressbar_end(struct server *serv)
{
    if (!g_main_window) return;
    if (serv->front_session)
        g_main_window->SetProgressBar(serv->front_session, false);
}

void fe_print_text(struct session *sess, char *text, time_t stamp,
                    gboolean no_activity)
{
    if (!g_main_window) return;
    wxString wxText = wxString::FromUTF8(text);
    g_main_window->PrintText(sess, wxText, stamp);
}

void fe_userlist_insert(struct session *sess, struct User *newuser,
                         int row, gboolean sel)
{
    if (!g_main_window) return;
    g_main_window->UserlistInsert(sess, newuser, row, sel);
}

int fe_userlist_remove(struct session *sess, struct User *user)
{
    if (!g_main_window) return 0;
    g_main_window->UserlistRemove(sess, user);
    return 1;
}

void fe_userlist_rehash(struct session *sess, struct User *user)
{
    if (!g_main_window) return;
    g_main_window->UserlistRehash(sess, user);
}

void fe_userlist_update(struct session *sess, struct User *user)
{
    fe_userlist_rehash(sess, user);
}

void fe_userlist_numbers(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->UserlistNumbers(sess);
}

void fe_userlist_clear(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->UserlistClear(sess);
}

void fe_userlist_set_selected(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->UserlistSetSelected(sess);
}

void fe_uselect(session *sess, char *word[], int do_clear, int scroll_to)
{
    if (!g_main_window) return;
    g_main_window->UserlistSelect(sess, (const char **)word, do_clear, scroll_to);
}

void fe_dcc_add(struct DCC *dcc)
{
    if (!g_main_window) return;
    /* Refresh the open DCC dialogs, if any */
    DccChatDialog *chatDlg = g_main_window->GetDccChatDialog();
    if (chatDlg && chatDlg->IsShown()) chatDlg->RefreshList();
    DccTransferDialog *xferDlg = g_main_window->GetDccTransferDialog();
    if (xferDlg && xferDlg->IsShown()) xferDlg->RefreshList();
}

void fe_dcc_update(struct DCC *dcc)
{
    if (!g_main_window) return;
    DccChatDialog *chatDlg = g_main_window->GetDccChatDialog();
    if (chatDlg && chatDlg->IsShown()) chatDlg->RefreshList();
    DccTransferDialog *xferDlg = g_main_window->GetDccTransferDialog();
    if (xferDlg && xferDlg->IsShown()) xferDlg->RefreshList();
}

void fe_dcc_remove(struct DCC *dcc)
{
    if (!g_main_window) return;
    DccChatDialog *chatDlg = g_main_window->GetDccChatDialog();
    if (chatDlg && chatDlg->IsShown()) chatDlg->RefreshList();
    DccTransferDialog *xferDlg = g_main_window->GetDccTransferDialog();
    if (xferDlg && xferDlg->IsShown()) xferDlg->RefreshList();
}

int fe_dcc_open_recv_win(int passive)
{
    if (g_main_window) {
        DccTransferDialog *dlg = g_main_window->GetDccTransferDialog();
        if (!dlg || !dlg->IsShown()) {
            wxCommandEvent dummy;
            g_main_window->OnDccRecv(dummy);
        } else {
            dlg->Raise();
        }
    }
    return TRUE;
}

int fe_dcc_open_send_win(int passive)
{
    if (g_main_window) {
        DccTransferDialog *dlg = g_main_window->GetDccTransferDialog();
        if (!dlg || !dlg->IsShown()) {
            wxCommandEvent dummy;
            g_main_window->OnDccRecv(dummy);
        } else {
            dlg->Raise();
        }
    }
    return TRUE;
}

int fe_dcc_open_chat_win(int passive)
{
    if (g_main_window) {
        DccChatDialog *dlg = g_main_window->GetDccChatDialog();
        if (!dlg || !dlg->IsShown()) {
            wxCommandEvent dummy;
            g_main_window->OnDccChat(dummy);
        } else {
            dlg->Raise();
        }
    }
    return TRUE;
}

void fe_clear_channel(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->ClearChannel(sess);
}

void fe_session_callback(struct session *sess)
{
    /* Called FROM session_free() — free frontend-owned structs.
       Mirrors GTK3 maingui.c: frees gui, res, and their sub-fields.
       Do NOT call session_free() here — that's our caller! */

    if (sess->res) {
        g_free(sess->res->input_text);
        g_free(sess->res->topic_text);
        g_free(sess->res->key_text);
        g_free(sess->res->limit_text);
        g_free(sess->res->lag_text);
        g_free(sess->res->lag_tip);
        g_free(sess->res->queue_text);
        g_free(sess->res->queue_tip);
        g_free(sess->res);
        sess->res = nullptr;
    }
    if (sess->gui) {
        g_free(sess->gui);
        sess->gui = nullptr;
    }
}

void fe_server_callback(struct server *serv)
{
    /* Called FROM server_free() — free frontend-owned server GUI struct.
       Mirrors GTK3 maingui.c: closes chanlist/rawlog/join windows, frees gui.
       Do NOT call server_free() here — that's our caller! */

    if (serv->gui) {
        if (serv->gui->joind_window) {
            JoinDialog *jd = (JoinDialog *)serv->gui->joind_window;
            jd->Destroy();
            serv->gui->joind_window = nullptr;
        }
        if (serv->gui->chanlist_window) {
            ChannelListDialog *cl = (ChannelListDialog *)serv->gui->chanlist_window;
            cl->Destroy();
            serv->gui->chanlist_window = nullptr;
        }
        if (serv->gui->rawlog_window) {
            RawLogDialog *rl = (RawLogDialog *)serv->gui->rawlog_window;
            rl->Destroy();
            serv->gui->rawlog_window = nullptr;
        }
        g_free(serv->gui);
        serv->gui = nullptr;
    }
}

void fe_url_add(const char *text)
{
    if (!g_main_window) return;
    /* If URL grabber dialog is open, refresh it */
    UrlGrabberDialog *dlg = g_main_window->GetUrlGrabberDialog();
    if (dlg && dlg->IsShown()) dlg->RefreshList();
}

void fe_pluginlist_update(void)
{
    PluginDialog *dlg = PluginDialog::GetInstance();
    if (dlg) {
        dlg->RefreshList();
    }
}

void fe_buttons_update(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->UpdateUserlistButtons();
}

void fe_dlgbuttons_update(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->UpdateDialogButtons();
}

void fe_dcc_send_filereq(struct session *sess, char *nick, int maxcps,
                          int passive)
{
    if (!g_main_window) return;
    wxFileDialog dlg(g_main_window, wxT("Select file to send"),
                     wxEmptyString, wxEmptyString, wxT("*.*"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
    if (dlg.ShowModal() == wxID_OK) {
        wxArrayString paths;
        dlg.GetPaths(paths);
        for (const auto &path : paths) {
            dcc_send(sess, nick,
                     (char *)path.utf8_str().data(), maxcps, passive);
        }
    }
}

void fe_set_channel(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->SetChannel(sess);
}

void fe_set_title(struct session *sess)
{
    if (!g_main_window) return;
    g_main_window->UpdateTitle(sess);
}

void fe_set_nonchannel(struct session *sess, int state)
{
    if (!g_main_window) return;
    g_main_window->SetNonChannel(sess, state);
}

void fe_set_nick(struct server *serv, char *newnick)
{
    if (!g_main_window) return;
    wxString nick = wxString::FromUTF8(newnick);
    g_main_window->SetNick(serv, nick);
}

void fe_ignore_update(int level)
{
    if (!g_main_window) return;
    IgnoreListDialog *dlg = g_main_window->GetIgnoreListDialog();
    if (dlg && dlg->IsShown()) {
        dlg->RefreshList();
        dlg->UpdateStats();
    }
}

void fe_beep(session *sess)
{
    wxBell();
}

void fe_lastlog(session *sess, session *lastlog_sess, char *sstr, int flags)
{
    if (!g_main_window) return;
    g_main_window->DoLastlog(sess, lastlog_sess, sstr, flags);
}

void fe_set_lag(server *serv, long lag)
{
    if (!g_main_window) return;
    g_main_window->SetLag(serv, lag);
}

void fe_set_throttle(server *serv)
{
    if (!g_main_window) return;
    g_main_window->SetThrottle(serv);
}

void fe_set_away(server *serv)
{
    if (!g_main_window) return;
    g_main_window->SetAway(serv);
}

void fe_serverlist_open(session *sess)
{
    if (!g_main_window) return;
    g_main_window->ShowNetworkList();
}

void fe_get_bool(char *title, char *prompt, void *callback, void *userdata)
{
    if (!g_main_window) return;
    wxString t = wxString::FromUTF8(title);
    wxString p = wxString::FromUTF8(prompt);
    auto cb = (void(*)(int, void *))callback;
    int result = wxMessageBox(p, t, wxYES_NO | wxICON_QUESTION, g_main_window);
    cb(result == wxYES ? 1 : 0, userdata);
}

void fe_get_str(char *prompt, char *def, void *callback, void *ud)
{
    if (!g_main_window) return;
    wxString p = wxString::FromUTF8(prompt);
    wxString d = def ? wxString::FromUTF8(def) : wxString();
    auto cb = (void(*)(int, char *, void *))callback;
    wxTextEntryDialog dlg(g_main_window, p, wxT("PChat"), d);
    if (dlg.ShowModal() == wxID_OK) {
        wxString val = dlg.GetValue();
        cb(0, (char *)val.utf8_str().data(), ud);
    }
}

void fe_get_int(char *prompt, int def, void *callback, void *ud)
{
    if (!g_main_window) return;
    wxString p = wxString::FromUTF8(prompt);
    auto cb = (void(*)(int, int, void *))callback;
    wxString defStr = wxString::Format(wxT("%d"), def);
    wxTextEntryDialog dlg(g_main_window, p, wxT("PChat"), defStr);
    if (dlg.ShowModal() == wxID_OK) {
        long val;
        dlg.GetValue().ToLong(&val);
        cb(0, (int)val, ud);
    }
}

void fe_get_file(const char *title, char *initial,
                  void (*callback)(void *userdata, char *file),
                  void *userdata, int flags)
{
    if (!g_main_window) return;
    wxString t = wxString::FromUTF8(title);
    wxString init = initial ? wxString::FromUTF8(initial) : wxString();

    if (flags & FRF_CHOOSEFOLDER) {
        wxDirDialog dlg(g_main_window, t, init, wxDD_DEFAULT_STYLE);
        if (dlg.ShowModal() == wxID_OK) {
            wxString path = dlg.GetPath();
            callback(userdata, (char *)path.utf8_str().data());
        }
    } else {
        long style;
        if (flags & FRF_WRITE)
            style = wxFD_SAVE;
        else if (flags & FRF_MULTIPLE)
            style = wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE;
        else
            style = wxFD_OPEN | wxFD_FILE_MUST_EXIST;

        wxFileDialog dlg(g_main_window, t, wxEmptyString, init,
                         wxT("*.*"), style);
        if (dlg.ShowModal() == wxID_OK) {
            if (flags & FRF_MULTIPLE) {
                wxArrayString paths;
                dlg.GetPaths(paths);
                for (const auto &p : paths) {
                    callback(userdata, (char *)p.utf8_str().data());
                }
            } else {
                wxString path = dlg.GetPath();
                callback(userdata, (char *)path.utf8_str().data());
            }
        }
    }
}

void fe_ctrl_gui(session *sess, fe_gui_action action, int arg)
{
    if (!g_main_window) return;
    switch (action) {
    case FE_GUI_HIDE:
        g_main_window->Hide();
        break;
    case FE_GUI_SHOW:
        g_main_window->Show();
        g_main_window->Raise();
        break;
    case FE_GUI_FOCUS:
        g_main_window->Raise();
        g_main_window->SetFocus();
        break;
    case FE_GUI_FLASH:
        g_main_window->RequestUserAttention();
        break;
    case FE_GUI_COLOR:
        /* /gui color N — set tab color for the given session */
        fe_set_tab_color(sess, (tabcolor)arg);
        break;
    case FE_GUI_ICONIFY:
        g_main_window->Iconize(true);
        break;
    case FE_GUI_MENU:
        /* Toggle the menu bar on/off */
        {
            prefs.pchat_gui_hide_menu = !prefs.pchat_gui_hide_menu;
            wxMenuBar *mb = g_main_window->GetMenuBar();
            if (mb) mb->Show(!prefs.pchat_gui_hide_menu);
        }
        break;
    case FE_GUI_ATTACH:
        /* Tab attach/detach — not supported in single-window wx layout */
        break;
    case FE_GUI_APPLY:
        /* Apply settings — save config and refresh UI */
        save_config();
        break;
    default:
        break;
    }
}

int fe_gui_info(session *sess, int info_type)
{
    if (!g_main_window) return 0;
    return g_main_window->GetGuiInfo(sess, info_type);
}

void *fe_gui_info_ptr(session *sess, int info_type)
{
    if (!g_main_window) return nullptr;
    return g_main_window->GetGuiInfoPtr(sess, info_type);
}

void fe_confirm(const char *message, void (*yesproc)(void *),
                void (*noproc)(void *), void *ud)
{
    if (!g_main_window) return;
    wxString msg = wxString::FromUTF8(message);
    int result = wxMessageBox(msg, wxT("PChat - Confirm"),
                               wxYES_NO | wxICON_QUESTION, g_main_window);
    if (result == wxYES) {
        yesproc(ud);
    } else {
        noproc(ud);
    }
}

char *fe_get_inputbox_contents(struct session *sess)
{
    /* This needs to be synchronous, so return empty if called from
       non-GUI thread */
    static char empty[] = "";
    if (!g_main_window) return empty;
    return g_main_window->GetInputBoxContents(sess);
}

int fe_get_inputbox_cursor(struct session *sess)
{
    if (!g_main_window) return 0;
    return g_main_window->GetInputBoxCursor(sess);
}

void fe_set_inputbox_contents(struct session *sess, char *text)
{
    if (!g_main_window) return;
    wxString t = wxString::FromUTF8(text);
    g_main_window->SetInputBoxContents(sess, t);
}

void fe_set_inputbox_cursor(struct session *sess, int delta, int pos)
{
    if (!g_main_window) return;
    g_main_window->SetInputBoxCursor(sess, delta, pos);
}

void fe_open_url(const char *url)
{
    wxString wxUrl = wxString::FromUTF8(url);
    wxLaunchDefaultBrowser(wxUrl);
}

void fe_menu_del(menu_entry *entry)
{
    if (!g_main_window || !entry) return;
    g_main_window->RemovePluginMenuItem(entry);
}

char *fe_menu_add(menu_entry *entry)
{
    if (!g_main_window || !entry) return nullptr;
    g_main_window->AddPluginMenuItem(entry);

    if (!entry->markup || !entry->label)
        return nullptr;

    /* Return label with markup stripped.
       Simple approach: remove angle-bracket tags. */
    wxString label = wxString::FromUTF8(entry->label);
    wxString stripped;
    bool inTag = false;
    for (size_t i = 0; i < label.length(); i++) {
        if (label[i] == '<') { inTag = true; continue; }
        if (label[i] == '>') { inTag = false; continue; }
        if (!inTag) stripped += label[i];
    }
    return g_strdup(stripped.utf8_str().data());
}

void fe_menu_update(menu_entry *entry)
{
    if (!g_main_window || !entry) return;
    g_main_window->UpdatePluginMenuItem(entry);
}

void fe_server_event(server *serv, int type, int arg)
{
    if (!g_main_window) return;
    g_main_window->OnServerEvent(serv, type, arg);
}

void fe_tray_set_flash(const char *filename1, const char *filename2,
                        int timeout)
{
    if (!g_main_window || !g_main_window->GetTrayIcon()) return;
    /* Use the first custom icon file if provided */
    if (filename1 && filename1[0]) {
        g_main_window->GetTrayIcon()->SetTrayIconFromFile(
            wxString::FromUTF8(filename1));
    }
    g_main_window->GetTrayIcon()->FlashTray(timeout > 0 ? timeout : 500);
}

void fe_tray_set_file(const char *filename)
{
    if (!g_main_window || !g_main_window->GetTrayIcon()) return;
    if (filename && filename[0]) {
        g_main_window->GetTrayIcon()->SetTrayIconFromFile(
            wxString::FromUTF8(filename));
    }
}

void fe_tray_set_icon(feicon icon)
{
    if (!g_main_window || !g_main_window->GetTrayIcon()) return;
    g_main_window->GetTrayIcon()->SetTrayIcon((int)icon);
}

void fe_tray_set_tooltip(const char *text)
{
    if (!g_main_window || !g_main_window->GetTrayIcon()) return;
    wxString tip = wxString::FromUTF8(text);
    g_main_window->GetTrayIcon()->SetTrayTooltip(tip);
}

void fe_open_chan_list(server *serv, char *filter, int do_refresh)
{
    if (!g_main_window) return;
    g_main_window->ShowChannelList(serv, filter, do_refresh);
}

const char *fe_get_default_font(void)
{
#ifdef _WIN32
    return "Consolas 10";
#else
    return "Monospace 10";
#endif
}

} /* extern "C" */
