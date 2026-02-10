/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * wxWidgets Application class
 *
 * Timer and I/O watches are handled through GLib's main context,
 * matching the approach used by the GTK3 frontend.  A single
 * wxTimer (GLibPollTimer) polls GLib every few ms to dispatch
 * all pending GLib sources (timers, socket I/O, etc.).
 */

#ifndef PCHAT_APP_H
#define PCHAT_APP_H

#include <wx/wx.h>
#include <wx/timer.h>
#include <glib.h>

class MainWindow;

class PchatApp : public wxApp
{
public:
    bool OnInit() override;
    int OnExit() override;

    /* Timer management — delegates to GLib (safe for self-removal) */
    int AddTimer(int interval_ms, GSourceFunc callback, void *userdata);
    void RemoveTimer(int tag);

    /* Socket/FD watch management — delegates to GLib */
    int AddSocketWatch(int sok, int flags, void *func, void *data);
    void RemoveSocketWatch(int tag);

    /* Pump GLib events from the wx event loop */
    void PollGLibEvents();

private:
    /* wxTimer that polls GLib's default main context */
    wxTimer *m_glib_poll_timer = nullptr;

    /* Re-entrancy guard for PollGLibEvents */
    bool m_polling = false;
};

wxDECLARE_APP(PchatApp);

/* Global pointers for C code access */
extern PchatApp *g_app;
extern MainWindow *g_main_window;

#endif /* PCHAT_APP_H */
