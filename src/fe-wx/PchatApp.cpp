/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * wxWidgets Application class implementation
 */

#include "PchatApp.h"
#include "MainWindow.h"
#include "palette.h"
#include "DarkMode.h"
#include "fe-wx.h"

#include <wx/image.h>
#include <wx/imagpng.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/cfgfiles.h"
#include "../common/servlist.h"
#include "../common/text.h"
#include "../common/plugin.h"
#include "../common/fe.h"
}

/* Global pointers */
PchatApp *g_app = nullptr;
MainWindow *g_main_window = nullptr;

/* Use NO_MAIN so pchat.c's main() drives the startup sequence */
wxIMPLEMENT_APP_NO_MAIN(PchatApp);

/* Timer for polling GLib events */
class GLibPollTimer : public wxTimer
{
public:
    GLibPollTimer(PchatApp *app) : m_app(app) {}
    void Notify() override { m_app->PollGLibEvents(); }
private:
    PchatApp *m_app;
};

/* Simple timer class for socket watches using GLib integration */

bool PchatApp::OnInit()
{
    g_app = this;

    /* Register only the PNG image handler — the only format we use.
       wxInitAllImageHandlers() loads every codec and is slower. */
    wxImage::AddHandler(new wxPNGHandler);

    /* Initialize dark mode subsystem and load saved preference.
       Must be done before palette_init so the palette picks the
       correct default colors (light or dark). */
    wx_darkmode_init();
    wx_darkmode_load();

    /* Use wxWidgets 3.3 built-in dark mode support for native controls,
       menus, title bars, scrollbars, etc.  This must be called before
       any windows are created. */
    wx_darkmode_enable_wx(this);

    /* Initialize and load the color palette from colors.conf */
    wx_palette_init();
    wx_palette_load();

    /* Set app name for config paths */
    SetAppName(wxT("pchat"));

    /* Create main window — start hidden; it will be shown
       when the user connects from the server list dialog */
    g_main_window = new MainWindow();
    g_main_window->Show(false);
    SetTopWindow(g_main_window);

    /* Start GLib event polling timer.
       50ms (20 Hz) balances responsiveness with CPU usage — 10ms was
       unnecessarily aggressive and caused constant context switching. */
    m_glib_poll_timer = new GLibPollTimer(this);
    m_glib_poll_timer->Start(50);

    return true;
}

int PchatApp::OnExit()
{
    /* Stop GLib polling */
    if (m_glib_poll_timer) {
        m_glib_poll_timer->Stop();
        delete m_glib_poll_timer;
        m_glib_poll_timer = nullptr;
    }

    /* GLib timers and IO watches are cleaned up by the backend
       (pchat_exit → server_cleanup etc.) before we get here. */

    g_main_window = nullptr;
    g_app = nullptr;

    return 0;
}

int PchatApp::AddTimer(int interval_ms, GSourceFunc callback, void *userdata)
{
    /* Use GLib timers — identical to the GTK3 frontend.
       GLib safely handles self-removal from within callbacks
       (g_source_remove from inside the callback is fine).
       PollGLibEvents() dispatches these via g_main_context_iteration. */
    return (int)g_timeout_add(interval_ms, callback, userdata);
}

void PchatApp::RemoveTimer(int tag)
{
    g_source_remove((guint)tag);
}

int PchatApp::AddSocketWatch(int sok, int flags, void *func, void *data)
{
    /* Register the fd/socket with GLib's main context.
       The GLib poll timer (PollGLibEvents) dispatches these.
       Must match the GTK3 frontend logic in fe-gtk.c exactly. */

    GIOChannel *channel;
#ifdef _WIN32
    if (flags & FIA_FD)
        channel = g_io_channel_win32_new_fd(sok);       /* pipe / plain fd */
    else
        channel = g_io_channel_win32_new_socket(sok);   /* Winsock socket */
#else
    channel = g_io_channel_unix_new(sok);
#endif

    GIOCondition cond = (GIOCondition)0;
    if (flags & FIA_READ)
        cond = (GIOCondition)(cond | G_IO_IN | G_IO_HUP | G_IO_ERR);
    if (flags & FIA_WRITE)
        cond = (GIOCondition)(cond | G_IO_OUT | G_IO_ERR);
    if (flags & FIA_EX)
        cond = (GIOCondition)(cond | G_IO_PRI);

    typedef gboolean (*GIOFunc_t)(GIOChannel *, GIOCondition, gpointer);
    auto callback = (GIOFunc_t)func;

    guint source_id = g_io_add_watch(channel, cond, callback, data);
    g_io_channel_unref(channel);

    return (int)source_id;
}

void PchatApp::RemoveSocketWatch(int tag)
{
    g_source_remove((guint)tag);
}

void PchatApp::PollGLibEvents()
{
    /* Guard against re-entrancy: GLib dispatch can call backend callbacks
       that update wxWidgets UI, which on Windows can re-enter the message
       loop and trigger this timer again while the context is mid-iteration. */
    if (m_polling)
        return;
    m_polling = true;

    /* Process pending GLib main context events.
       This integrates GLib's event loop (socket I/O, timers, etc.)
       with the wxWidgets event loop.

       We must explicitly acquire the context before polling.  When
       plugins initialise GTK3 (via gtk_init_check), GTK registers
       GDK sources on the default context.  On Windows the GDK Win32
       backend can leave internal context state that makes bare calls
       to g_main_context_pending() crash inside g_main_context_prepare()
       unless the calling thread owns the context. */
    GMainContext *ctx = g_main_context_default();
    if (ctx && g_main_context_acquire(ctx)) {
        int budget = 50;
        while (budget-- > 0 && g_main_context_pending(ctx)) {
            g_main_context_iteration(ctx, FALSE);
        }
        g_main_context_release(ctx);
    }

    m_polling = false;
}
