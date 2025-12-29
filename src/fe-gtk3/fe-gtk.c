/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
 *
 * PChat
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fe-gtk.h"

#ifdef _WIN32
#include <gdk/gdkwin32.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../common/pchat.h"
#include "../common/fe.h"
#include "../common/util.h"
#include "../common/text.h"
#include "../common/cfgfiles.h"
#include "../common/pchatc.h"
#include "textview-chat.h"
#include "../common/plugin.h"
#include "../common/server.h"
#include "../common/url.h"
#include "../common/debug-log.h"
#include "gtkutil.h"
#include "maingui.h"
#include "pixmaps.h"
#include "chanlist.h"
#include "joind.h"
#include "update-checker.h"
#include "textview-chat.h"
#include "palette.h"

/* Custom log handler to suppress harmless GTK scale factor warnings */
static void
gtk_log_handler_suppress_scale_factor (const gchar *log_domain,
                                       GLogLevelFlags log_level,
                                       const gchar *message,
                                       gpointer user_data)
{
	/* Suppress specific scale factor warnings that don't affect functionality */
	if (message && strstr (message, "gtk_widget_get_scale_factor: assertion 'GTK_IS_WIDGET (widget)' failed"))
		return; /* Don't print this warning */
	
	/* For all other GTK critical messages, use default handler */
	g_log_default_handler (log_domain, log_level, message, user_data);
}
#include "menu.h"
#include "notifygui.h"
#include "textgui.h"
#include "fkeys.h"
#include "plugin-tray.h"
#include "plugin-notification.h"
#include "urlgrab.h"
#include "setup.h"

#ifdef USE_LIBCANBERRA
#include <canberra.h>
#endif

GdkPixbuf *channelwin_pix;

#ifdef USE_LIBCANBERRA
static ca_context *ca_con;
#endif

#ifdef HAVE_GTK_MAC
GtkosxApplication *osx_app;
#endif

/* === command-line parameter parsing : requires glib 2.6 === */

static char *arg_cfgdir = NULL;
static gint arg_show_autoload = 0;
static gint arg_show_config = 0;
static gint arg_show_version = 0;
static gint arg_minimize = 0;

static const GOptionEntry gopt_entries[] =
{
 {"no-auto",	'a', 0, G_OPTION_ARG_NONE,	&arg_dont_autoconnect, N_("Don't auto connect to servers"), NULL},
 {"cfgdir",	'd', 0, G_OPTION_ARG_STRING,	&arg_cfgdir, N_("Use a different config directory"), "PATH"},
 {"no-plugins",	'n', 0, G_OPTION_ARG_NONE,	&arg_skip_plugins, N_("Don't auto load any plugins"), NULL},
 {"plugindir",	'p', 0, G_OPTION_ARG_NONE,	&arg_show_autoload, N_("Show plugin/script auto-load directory"), NULL},
 {"configdir",	'u', 0, G_OPTION_ARG_NONE,	&arg_show_config, N_("Show user config directory"), NULL},
 {"url",	 0,  G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &arg_url, N_("Open an irc://server:port/channel?key URL"), "URL"},
 {"command",	'c', 0, G_OPTION_ARG_STRING,	&arg_command, N_("Execute command:"), "COMMAND"},
#ifdef USE_DBUS
 {"existing",	'e', 0, G_OPTION_ARG_NONE,	&arg_existing, N_("Open URL or execute command in an existing PChat"), NULL},
#endif
 {"minimize",	 0,  0, G_OPTION_ARG_INT,	&arg_minimize, N_("Begin minimized. Level 0=Normal 1=Iconified 2=Tray"), N_("level")},
 {"version",	'v', 0, G_OPTION_ARG_NONE,	&arg_show_version, N_("Show version information"), NULL},
 {G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &arg_urls, N_("Open an irc://server:port/channel?key URL"), "URL"},
 {NULL}
};

#ifdef _WIN32
static void
create_msg_dialog (gchar *title, gchar *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", message);
	gtk_window_set_title (GTK_WINDOW (dialog), title);

/* On Win32 we automatically have the icon. If we try to load it explicitly, it will look ugly for some reason. */
#ifndef _WIN32
	pixmaps_init ();
	gtk_window_set_icon (GTK_WINDOW (dialog), pix_xchat);
#endif

	g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_widget_show_all (dialog);
}
#endif

int
fe_args (int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;
	char *buffer;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	context = g_option_context_new (NULL);
#ifdef _WIN32
	g_option_context_set_help_enabled (context, FALSE);	/* disable stdout help as stdout is unavailable for subsystem:windows */
#endif
	g_option_context_add_main_entries (context, gopt_entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_option_context_parse (context, &argc, &argv, &error);

#ifdef _WIN32
	if (error)											/* workaround for argv not being available when using subsystem:windows */
	{
		if (error->message)								/* the error message contains argv so search for patterns in that */
		{
			if (strstr (error->message, "--help-all") != NULL)
			{
				buffer = g_strdup_printf ("%s", g_option_context_get_help (context, FALSE, NULL));
				gtk_init (&argc, &argv);
				create_msg_dialog ("Long Help", buffer);
				g_free (buffer);
				return 0;
			}
			else if (strstr (error->message, "--help") != NULL || strstr (error->message, "-?") != NULL)
			{
				buffer = g_strdup_printf ("%s", g_option_context_get_help (context, TRUE, NULL));
				gtk_init (&argc, &argv);
				create_msg_dialog ("Help", buffer);
				g_free (buffer);
				return 0;
			}
			else
			{
				buffer = g_strdup_printf ("%s\n", error->message);
				gtk_init (&argc, &argv);
				create_msg_dialog ("Error", buffer);
				g_free (buffer);
				return 1;
			}
		}
	}
#else
	if (error)
	{
		if (error->message)
			printf ("%s\n", error->message);
		return 1;
	}
#endif

	g_option_context_free (context);

	if (arg_show_version)
	{
		buffer = g_strdup_printf ("%s %s", PACKAGE_NAME, PACKAGE_VERSION);
#ifdef _WIN32
		gtk_init (&argc, &argv);
		create_msg_dialog ("Version Information", buffer);
#else
		puts (buffer);
#endif
		g_free (buffer);

		return 0;
	}

	if (arg_show_autoload)
	{
		buffer = g_strdup_printf ("%s%caddons%c", get_xdir(), G_DIR_SEPARATOR, G_DIR_SEPARATOR);
#ifdef _WIN32
		gtk_init (&argc, &argv);
		create_msg_dialog ("Plugin/Script Auto-load Directory", buffer);
#else
		puts (buffer);
#endif
		g_free (buffer);

		return 0;
	}

	if (arg_show_config)
	{
		buffer = g_strdup_printf ("%s%c", get_xdir(), G_DIR_SEPARATOR);
#ifdef _WIN32
		gtk_init (&argc, &argv);
		create_msg_dialog ("User Config Directory", buffer);
#else
		puts (buffer);
#endif
		g_free (buffer);

		return 0;
	}

#ifdef _WIN32
	/* this is mainly for irc:// URL handling. When windows calls us from */
	/* I.E, it doesn't give an option of "Start in" directory, like short */
	/* cuts can. So we have to set the current dir manually, to the path  */
	/* of the exe. */
	{
		char *tmp = g_strdup (argv[0]);
		char *sl;

		sl = strrchr (tmp, G_DIR_SEPARATOR);
		if (sl)
		{
			*sl = 0;
			chdir (tmp);
		}
		free (tmp);
	}
#endif

	char *log_path = g_build_filename (get_xdir (), "pchat-debug.log", NULL);
	debug_log_init(log_path);
	g_free(log_path);
	DEBUG_LOG("INIT", "Starting PChat initialization");

#ifdef _WIN32
	/* Set up GTK environment for standalone execution */
	{
		char *exe_path = g_win32_get_package_installation_directory_of_module(NULL);
		if (exe_path)
		{
			char *share_path = g_build_filename(exe_path, "share", NULL);
			char *lib_path = g_build_filename(exe_path, "lib", NULL);
			
			/* Set environment variables for GTK to find its resources */
			g_setenv("GTK_DATA_PREFIX", exe_path, TRUE);
			g_setenv("XDG_DATA_DIRS", share_path, TRUE);
			g_setenv("GTK_EXE_PREFIX", exe_path, TRUE);
			g_setenv("GTK_PATH", lib_path, TRUE);
			
			/* Force software rendering to avoid graphics driver issues */
			g_setenv("GDK_RENDERING", "image", TRUE);
			g_setenv("GSK_RENDERER", "cairo", TRUE);
			
			/* Enable verbose debugging */
			g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
			g_setenv("GTK_DEBUG", "all", TRUE);
			g_setenv("GDK_DEBUG", "all", TRUE);
			g_setenv("G_DEBUG", "fatal-warnings", TRUE);
			
			DEBUG_LOG("INIT", "Set GTK paths - exe: %s", exe_path);
			DEBUG_LOG("INIT", "Forced software rendering (GDK_RENDERING=image, GSK_RENDERER=cairo)");
			DEBUG_LOG("INIT", "Enabled GTK debugging");
			
			g_free(share_path);
			g_free(lib_path);
			g_free(exe_path);
		}
	}
#endif

	gtk_init (&argc, &argv);
	DEBUG_LOG("INIT", "GTK initialized successfully");

	/* Apply CSS to reduce tab button padding/height */
	{
		GtkCssProvider *css_provider = gtk_css_provider_new();
		const gchar *css_data = 
			"#xchat-tab { "
			"  padding: 2px 6px; "
			"  min-height: 0; "
			"}";
		gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);
		gtk_style_context_add_provider_for_screen(
			gdk_screen_get_default(),
			GTK_STYLE_PROVIDER(css_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref(css_provider);
	}

	/* Suppress GTK scale factor warnings that don't affect functionality */
	g_log_set_handler ("Gtk", G_LOG_LEVEL_CRITICAL, 
		(GLogFunc) gtk_log_handler_suppress_scale_factor, NULL);

#ifdef HAVE_GTK_MAC
	osx_app = g_object_new(GTKOSX_TYPE_APPLICATION, NULL);
#endif

	return -1;
}

static GtkCssProvider *input_css_provider = NULL;

void
create_input_style_css (void)
{
	char css[512];
	static gboolean done_css = FALSE;

	if (prefs.pchat_gui_input_style && !done_css)
	{
		done_css = TRUE;
		
		if (!input_css_provider)
		{
			input_css_provider = gtk_css_provider_new();
			gtk_style_context_add_provider_for_screen(
				gdk_screen_get_default(),
				GTK_STYLE_PROVIDER(input_css_provider),
				GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		}

		snprintf(css, sizeof(css),
			".xchat-inputbox { caret-color: rgb(%d,%d,%d); }",
			(int)(colors[COL_FG].red * 255),
			(int)(colors[COL_FG].green * 255),
			(int)(colors[COL_FG].blue * 255));

		gtk_css_provider_load_from_data(input_css_provider, css, -1, NULL);
	}
}

PangoFontDescription *
create_input_style_font (void)
{
	char buf[256];
	PangoFontDescription *font_desc;

	font_desc = pango_font_description_from_string (prefs.pchat_text_font);

	/* fall back */
	if (pango_font_description_get_size (font_desc) == 0)
	{
		snprintf (buf, sizeof (buf), _("Failed to open font:\n\n%s"), prefs.pchat_text_font);
		fe_message (buf, FE_MSG_ERROR);
		pango_font_description_free (font_desc);
		font_desc = pango_font_description_from_string ("sans 11");
	}

	return font_desc;
}

void
fe_init (void)
{
	DEBUG_LOG("INIT", "fe_init: Starting frontend initialization");
	palette_load ();
	key_init ();
	pixmaps_init ();

#ifdef HAVE_GTK_MAC
	gtkosx_application_set_dock_icon_pixbuf (osx_app, pix_xchat);
#endif
	channelwin_pix = gdk_pixbuf_new_from_file (prefs.pchat_text_background, NULL);
	input_font_desc = create_input_style_font ();
	create_input_style_css ();
	
	/* Initialize update checker for Windows and macOS */
	update_checker_init ();
	
	DEBUG_LOG("INIT", "fe_init: Frontend initialization complete");
}

void
fe_main (void)
{
	DEBUG_LOG("INIT", "fe_main: Entering GTK main loop");
#ifdef HAVE_GTK_MAC
	gtkosx_application_ready(osx_app);
#endif

	gtk_main ();
	DEBUG_LOG("INIT", "fe_main: GTK main loop exited");

	/* sleep for 2 seconds so any QUIT messages are not lost. The  */
	/* GUI is closed at this point, so the user doesn't even know! */
	if (prefs.wait_on_exit)
		sleep (2);
}

void
fe_cleanup (void)
{
	/* it's saved when pressing OK in setup.c */
	/*palette_save ();*/
	
	/* Cleanup update checker */
	update_checker_cleanup ();
}

void
fe_exit (void)
{
	gtk_main_quit ();
}

int
fe_timeout_add (int interval, void *callback, void *userdata)
{
	return g_timeout_add (interval, (GSourceFunc) callback, userdata);
}

int
fe_timeout_add_seconds (int interval, void *callback, void *userdata)
{
	return g_timeout_add_seconds (interval, (GSourceFunc) callback, userdata);
}

void
fe_timeout_remove (int tag)
{
	g_source_remove (tag);
}

#ifdef _WIN32

static void
log_handler (const gchar   *log_domain,
		       GLogLevelFlags log_level,
		       const gchar   *message,
		       gpointer	      unused_data)
{
	session *sess;

	/* if (getenv ("XCHAT_WARNING_IGNORE")) this gets ignored sometimes, so simply just disable all warnings */
		return;

	sess = find_dialog (serv_list->data, "(warnings)");
	if (!sess)
		sess = new_ircwindow (serv_list->data, "(warnings)", SESS_DIALOG, 0);

	PrintTextf (sess, "%s\t%s\n", log_domain, message);
	if (getenv ("XCHAT_WARNING_ABORT"))
		abort ();
}

#endif

/* install tray and notification plugins */

static int
fe_idle (gpointer data)
{
	session *sess = sess_list->data;

	DEBUG_LOG("INIT", "fe_idle: Loading tray plugin");
	plugin_add (sess, NULL, NULL, tray_plugin_init, tray_plugin_deinit, NULL, FALSE);
	DEBUG_LOG("INIT", "fe_idle: Loading notification plugin");
	plugin_add (sess, NULL, NULL, notification_plugin_init, notification_plugin_deinit, NULL, FALSE);
	DEBUG_LOG("INIT", "fe_idle: Plugins loaded successfully");

	if (arg_minimize == 1)
		gtk_window_iconify (GTK_WINDOW (sess->gui->window));
	else if (arg_minimize == 2)
		tray_toggle_visibility (FALSE);

	return 0;
}

void
fe_new_window (session *sess, int focus)
{
	int tab = FALSE;
	DEBUG_LOG("WINDOW", "fe_new_window: type=%d, focus=%d", sess->type, focus);

	if (sess->type == SESS_DIALOG)
	{
		if (prefs.pchat_gui_tab_dialogs)
			tab = TRUE;
	} else
	{
		if (prefs.pchat_gui_tab_chans)
			tab = TRUE;
	}

	DEBUG_LOG("WINDOW", "fe_new_window: Calling mg_changui_new, tab=%d", tab);
	mg_changui_new (sess, NULL, tab, focus);
	DEBUG_LOG("WINDOW", "fe_new_window: mg_changui_new completed");

#ifdef _WIN32
	DEBUG_LOG("WINDOW", "fe_new_window: Setting up log handlers");
	g_log_set_handler ("GLib", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	g_log_set_handler ("GLib-GObject", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	g_log_set_handler ("Gdk", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	g_log_set_handler ("Gtk", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, (GLogFunc)log_handler, 0);
	DEBUG_LOG("WINDOW", "fe_new_window: Log handlers set");
#endif

	if (!sess_list->next)
	{
		DEBUG_LOG("WINDOW", "fe_new_window: Adding fe_idle callback");
		g_idle_add (fe_idle, NULL);
		DEBUG_LOG("WINDOW", "fe_new_window: fe_idle callback added");
	}

	DEBUG_LOG("WINDOW", "fe_new_window: Completed");
}

void
fe_new_server (struct server *serv)
{
	serv->gui = malloc (sizeof (struct server_gui));
	memset (serv->gui, 0, sizeof (struct server_gui));
}

void
fe_message (char *msg, int flags)
{
	GtkWidget *dialog;
	int type = GTK_MESSAGE_WARNING;

	if (flags & FE_MSG_ERROR)
		type = GTK_MESSAGE_ERROR;
	if (flags & FE_MSG_INFO)
		type = GTK_MESSAGE_INFO;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent_window), 0, type,
												GTK_BUTTONS_OK, "%s", msg);
	if (flags & FE_MSG_MARKUP)
		gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), msg);
	g_signal_connect (G_OBJECT (dialog), "response",
							G_CALLBACK (gtk_widget_destroy), 0);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gtk_widget_show_all (dialog);

	/* Note: gtk_dialog_run deprecated - now always non-blocking */
	if (flags & FE_MSG_WAIT) {
		/* Previously blocked here, but modern GTK prefers async dialogs */
	}
}

void
fe_idle_add (void *func, void *data)
{
	g_idle_add (func, data);
}

void
fe_input_remove (int tag)
{
	g_source_remove (tag);
}

int
fe_input_add (int sok, int flags, void *func, void *data)
{
	int tag, type = 0;
	GIOChannel *channel;

#ifdef _WIN32
	if (flags & FIA_FD)
		channel = g_io_channel_win32_new_fd (sok);
	else
		channel = g_io_channel_win32_new_socket (sok);
#else
	channel = g_io_channel_unix_new (sok);
#endif

	if (flags & FIA_READ)
		type |= G_IO_IN | G_IO_HUP | G_IO_ERR;
	if (flags & FIA_WRITE)
		type |= G_IO_OUT | G_IO_ERR;
	if (flags & FIA_EX)
		type |= G_IO_PRI;

	tag = g_io_add_watch (channel, type, (GIOFunc) func, data);
	g_io_channel_unref (channel);

	return tag;
}

void
fe_set_topic (session *sess, char *topic, char *stripped_topic)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		if (prefs.pchat_text_stripcolor_topic)
		{
			gtk_entry_set_text (GTK_ENTRY (sess->gui->topic_entry), stripped_topic);
		}
		else
		{
			gtk_entry_set_text (GTK_ENTRY (sess->gui->topic_entry), topic);
		}
		mg_set_topic_tip (sess);
	}
	else
	{
		if (sess->res->topic_text)
		{
			free (sess->res->topic_text);
		}

		if (prefs.pchat_text_stripcolor_topic)
		{
			sess->res->topic_text = g_strdup (stripped_topic);
		}
		else
		{
			sess->res->topic_text = g_strdup (topic);
		}
	}
}

void
fe_set_hilight (struct session *sess)
{
	if (sess->gui->is_tab)
		fe_set_tab_color (sess, 3);	/* set tab to blue */

	if (prefs.pchat_input_flash_hilight && (!prefs.pchat_away_omit_alerts || !sess->server->is_away))
		fe_flash_window (sess); /* taskbar flash */
}

static void
fe_update_mode_entry (session *sess, GtkWidget *entry, char **text, char *new_text)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		if (sess->gui->flag_wid[0])	/* channel mode buttons enabled? */
			gtk_entry_set_text (GTK_ENTRY (entry), new_text);
	} else
	{
		if (sess->gui->is_tab)
		{
			if (*text)
				free (*text);
			*text = g_strdup (new_text);
		}
	}
}

void
fe_update_channel_key (struct session *sess)
{
	fe_update_mode_entry (sess, sess->gui->key_entry,
								 &sess->res->key_text, sess->channelkey);
	fe_set_title (sess);
}

void
fe_update_channel_limit (struct session *sess)
{
	char tmp[16];

	sprintf (tmp, "%d", sess->limit);
	fe_update_mode_entry (sess, sess->gui->limit_entry,
								 &sess->res->limit_text, tmp);
	fe_set_title (sess);
}

int
fe_is_chanwindow (struct server *serv)
{
	if (!serv->gui->chanlist_window)
		return 0;
	return 1;
}

void
fe_notify_update (char *name)
{
	if (!name)
		notify_gui_update ();
}

void
fe_text_clear (struct session *sess, int lines)
{
	PchatTextViewChat *chat = PCHAT_TEXTVIEW_CHAT (sess->gui->textview);
	pchat_textview_chat_clear (chat, lines);
}

void
fe_close_window (struct session *sess)
{
	if (sess->gui->is_tab)
		mg_tab_close (sess);
	else
		gtk_widget_destroy (sess->gui->window);
}

void
fe_progressbar_start (session *sess)
{
	if (!sess->gui->is_tab || current_tab == sess)
	/* if it's the focused tab, create it for real! */
		mg_progressbar_create (sess->gui);
	else
	/* otherwise just remember to create on when it gets focused */
		sess->res->c_graph = TRUE;
}

void
fe_progressbar_end (server *serv)
{
	GSList *list = sess_list;
	session *sess;

	while (list)				  /* check all windows that use this server and  *
									   * remove the connecting graph, if it has one. */
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (sess->gui->bar)
				mg_progressbar_destroy (sess->gui);
			sess->res->c_graph = FALSE;
		}
		list = list->next;
	}
}

void
fe_print_text (struct session *sess, char *text, time_t stamp,
			   gboolean no_activity)
{
	PrintTextRaw (sess->res->buffer, (unsigned char *)text, prefs.pchat_text_indent, stamp);

	if (!no_activity && sess != current_tab && sess->gui->is_tab)
	{
		lastact_update (sess);
		fe_set_tab_color (sess, 1);
	}
}

void
fe_beep (session *sess)
{
#ifdef _WIN32
	if (!PlaySound ("Notification.IM", NULL, SND_ALIAS|SND_ASYNC))
	{
		/* This is really just a fallback attempt, may or may not work on new Windows releases, especially on x64.
		 * You should set up the "Instant Message Notification" system sound instead, supported on Vista and up.
		 */
		Beep (1000, 50);
	}
#else
#ifdef USE_LIBCANBERRA
	if (ca_con == NULL)
	{
		ca_context_create (&ca_con);
		ca_context_change_props (ca_con,
										CA_PROP_APPLICATION_ID, "xchat",
										CA_PROP_APPLICATION_NAME, DISPLAY_NAME,
										CA_PROP_APPLICATION_ICON_NAME, "xchat", NULL);
	}

	if (ca_context_play (ca_con, 0, CA_PROP_EVENT_ID, "message-new-instant", NULL) != 0)
#endif
	gdk_display_beep (gdk_display_get_default ());
#endif
}

void
fe_lastlog (session *sess, session *lastlog_sess, char *sstr, int flags)
{
	GError *err = NULL;
	PchatChatBuffer *buf, *lbuf;
	PchatTextViewChat *chat, *lastlog_chat;
	
	chat = PCHAT_TEXTVIEW_CHAT (sess->gui->textview);
	lastlog_chat = PCHAT_TEXTVIEW_CHAT (lastlog_sess->gui->textview);
	
	buf = pchat_textview_chat_get_buffer (chat);
	lbuf = pchat_textview_chat_get_buffer (lastlog_chat);
	
	if (!buf || pchat_chat_buffer_is_empty (buf))
	{
		PrintText (lastlog_sess, _("Search buffer is empty.\n"));
		return;
	}
	
	/* Set up search criteria */
	if (flags & 16) /* regexp flag */
	{
		pchat_chat_buffer_set_search_regex (lbuf, sstr, (flags & 1) != 0, &err); /* case_match flag */
		if (err)
		{
			PrintText (lastlog_sess, _(err->message));
			g_error_free (err);
			return;
		}
	}
	else
	{
		pchat_chat_buffer_set_search_text (lbuf, sstr, (flags & 1) != 0); /* case_match flag */
	}
	
	/* Perform the lastlog search */
	pchat_chat_buffer_lastlog (lbuf, buf, lastlog_chat);
}

void
fe_set_lag (server *serv, long lag)
{
	GSList *list = sess_list;
	session *sess;
	gdouble per;
	char lagtext[64];
	char lagtip[128];
	unsigned long nowtim;

	if (lag == -1)
	{
		if (!serv->lag_sent)
			return;
		nowtim = make_ping_time ();
		lag = nowtim - serv->lag_sent;
	}

	/* if there is no pong for >30s report the lag as +30s */
	if (lag > 30000 && serv->lag_sent)
		lag=30000;

	per = ((double)lag) / 1000.0;
	if (per > 1.0)
		per = 1.0;

	snprintf (lagtext, sizeof (lagtext) - 1, "%s%ld.%lds",
			  serv->lag_sent ? "+" : "", lag / 1000, (lag/100) % 10);
	snprintf (lagtip, sizeof (lagtip) - 1, "Lag: %s%ld.%ld seconds",
				 serv->lag_sent ? "+" : "", lag / 1000, (lag/100) % 10);

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (sess->res->lag_tip)
				free (sess->res->lag_tip);
			sess->res->lag_tip = g_strdup (lagtip);

			if (!sess->gui->is_tab || current_tab == sess)
			{
				if (sess->gui->lagometer)
				{
					gtk_progress_bar_set_fraction ((GtkProgressBar *) sess->gui->lagometer, per);
					gtk_widget_set_tooltip_text (gtk_widget_get_parent (sess->gui->lagometer), lagtip);
				}
				if (sess->gui->laginfo)
					gtk_label_set_text ((GtkLabel *) sess->gui->laginfo, lagtext);
			} else
			{
				sess->res->lag_value = per;
				if (sess->res->lag_text)
					free (sess->res->lag_text);
				sess->res->lag_text = g_strdup (lagtext);
			}
		}
		list = list->next;
	}
}

void
fe_set_throttle (server *serv)
{
	GSList *list = sess_list;
	struct session *sess;
	float per;
	char tbuf[96];
	char tip[160];

	per = (float) serv->sendq_len / 1024.0;
	if (per > 1.0)
		per = 1.0;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			snprintf (tbuf, sizeof (tbuf) - 1, _("%d bytes"), serv->sendq_len);
			snprintf (tip, sizeof (tip) - 1, _("Network send queue: %d bytes"), serv->sendq_len);

			if (sess->res->queue_tip)
				free (sess->res->queue_tip);
			sess->res->queue_tip = g_strdup (tip);

			if (!sess->gui->is_tab || current_tab == sess)
			{
				if (sess->gui->throttlemeter)
				{
					gtk_progress_bar_set_fraction ((GtkProgressBar *) sess->gui->throttlemeter, per);
					gtk_widget_set_tooltip_text (gtk_widget_get_parent (sess->gui->throttlemeter), tip);
				}
				if (sess->gui->throttleinfo)
					gtk_label_set_text ((GtkLabel *) sess->gui->throttleinfo, tbuf);
			} else
			{
				sess->res->queue_value = per;
				if (sess->res->queue_text)
					free (sess->res->queue_text);
				sess->res->queue_text = g_strdup (tbuf);
			}
		}
		list = list->next;
	}
}

void
fe_ctrl_gui (session *sess, fe_gui_action action, int arg)
{
	switch (action)
	{
	case FE_GUI_HIDE:
		gtk_widget_hide (sess->gui->window); break;
	case FE_GUI_SHOW:
		gtk_widget_show (sess->gui->window);
		gtk_window_present (GTK_WINDOW (sess->gui->window));
		break;
	case FE_GUI_FOCUS:
		mg_bring_tofront_sess (sess); break;
	case FE_GUI_FLASH:
		fe_flash_window (sess); break;
	case FE_GUI_COLOR:
		fe_set_tab_color (sess, arg); break;
	case FE_GUI_ICONIFY:
		gtk_window_iconify (GTK_WINDOW (sess->gui->window)); break;
	case FE_GUI_MENU:
		menu_bar_toggle ();	/* toggle menubar on/off */
		break;
	case FE_GUI_ATTACH:
		mg_detach (sess, arg);	/* arg: 0=toggle 1=detach 2=attach */
		break;
	case FE_GUI_APPLY:
		setup_apply_real (TRUE, TRUE, TRUE);
	}
}

static void
dcc_saveas_cb (struct DCC *dcc, char *file)
{
	if (is_dcc (dcc))
	{
		if (dcc->dccstat == STAT_QUEUED)
		{
			if (file)
				dcc_get_with_destfile (dcc, file);
			else if (dcc->resume_sent == 0)
				dcc_abort (dcc->serv->front_session, dcc);
		}
	}
}

void
fe_confirm (const char *message, void (*yesproc)(void *), void (*noproc)(void *), void *ud)
{
	/* warning, assuming fe_confirm is used by DCC only! */
	struct DCC *dcc = ud;
	char *filepath;

	if (dcc->file)
	{
		filepath = g_build_filename (prefs.pchat_dcc_dir, dcc->file, NULL);
		gtkutil_file_req (message, dcc_saveas_cb, ud, filepath, NULL,
								FRF_WRITE|FRF_NOASKOVERWRITE|FRF_FILTERISINITIAL);
		g_free (filepath);
	}
}

int
fe_gui_info (session *sess, int info_type)
{
	switch (info_type)
	{
	case 0:	/* window status */
		if (!gtk_widget_get_visible (GTK_WIDGET (sess->gui->window)))
		{
			return 2;	/* hidden (iconified or systray) */
		}

		if (gtk_window_is_active (GTK_WINDOW (sess->gui->window)))
		{
			return 1;	/* active/focused */
		}

		return 0;		/* normal (no keyboard focus or behind a window) */
	}

	return -1;
}

void *
fe_gui_info_ptr (session *sess, int info_type)
{
	switch (info_type)
	{
	case 0:	/* native window pointer (for plugins) */
#ifdef _WIN32
		return gdk_win32_window_get_impl_hwnd (gtk_widget_get_window (sess->gui->window));
#else
		return sess->gui->window;
#endif
		break;

	case 1:	/* GtkWindow * (for plugins) */
		return sess->gui->window;
	}
	return NULL;
}

char *
fe_get_inputbox_contents (session *sess)
{
	/* not the current tab */
	if (sess->res->input_text)
		return sess->res->input_text;

	/* current focused tab */
	return SPELL_ENTRY_GET_TEXT (sess->gui->input_box);
}

int
fe_get_inputbox_cursor (session *sess)
{
	/* not the current tab (we don't remember the cursor pos) */
	if (sess->res->input_text)
		return 0;

	/* current focused tab */
	return SPELL_ENTRY_GET_POS (sess->gui->input_box);
}

void
fe_set_inputbox_cursor (session *sess, int delta, int pos)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		if (delta)
			pos += SPELL_ENTRY_GET_POS (sess->gui->input_box);
		SPELL_ENTRY_SET_POS (sess->gui->input_box, pos);
	} else
	{
		/* we don't support changing non-front tabs yet */
	}
}

void
fe_set_inputbox_contents (session *sess, char *text)
{
	if (!sess->gui->is_tab || sess == current_tab)
	{
		SPELL_ENTRY_SET_TEXT (sess->gui->input_box, text);
	} else
	{
		if (sess->res->input_text)
			free (sess->res->input_text);
		sess->res->input_text = g_strdup (text);
	}
}

static void
fe_open_url_inner (const char *url)
{
#ifdef _WIN32
	ShellExecute (0, "open", url, NULL, NULL, SW_SHOWNORMAL);
#elif defined __APPLE__
	/* on Mac you can just 'open http://foo.bar/' */
	gchar open[512];
	g_snprintf (open, sizeof(open), "%s %s", g_find_program_in_path ("open"), url, NULL);
	pchat_exec (open);
#else
	gtk_show_uri_on_window (NULL, url, GDK_CURRENT_TIME, NULL);
#endif
}

static void
fe_open_url_locale (const char *url)
{
	int url_type = url_check_word (url);
	char *uri;

	/* gvfs likes file:// */
	if (url_type == WORD_PATH)
	{
#ifndef _WIN32
		uri = g_strconcat ("file://", url, NULL);
		fe_open_url_inner (uri);
		g_free (uri);
#else
		fe_open_url_inner (url);
#endif
	}
	/* IPv6 addr. Add http:// */
	else if (url_type == WORD_HOST6)
	{
		/* IPv6 addrs in urls should be enclosed in [ ] */
		if (*url != '[')
			uri = g_strdup_printf ("http://[%s]", url);
		else
			uri = g_strdup_printf ("http://%s", url);

		fe_open_url_inner (uri);
		g_free (uri);
	}
	/* the http:// part's missing, prepend it, otherwise it won't always work */
	else if (strchr (url, ':') == NULL)
	{
		url = g_strdup_printf ("http://%s", url);
		fe_open_url_inner (url);
		g_free ((char *)url);
	}
	/* we have a sane URL, send it to the browser untouched */
	else
	{
		fe_open_url_inner (url);
	}
}

void
fe_open_url (const char *url)
{
	char *loc;

	/* UTF-8 locale handling - always enabled in modern systems */
	{
		fe_open_url_locale (url);
		return;
	}

	/* the OS expects it in "locale" encoding. This makes it work on
	   unix systems that use ISO-8859-x and Win32. */
	loc = g_locale_from_utf8 (url, -1, 0, 0, 0);
	if (loc)
	{
		fe_open_url_locale (loc);
		g_free (loc);
	}
}

void
fe_server_event (server *serv, int type, int arg)
{
	GSList *list = sess_list;
	session *sess;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv && (current_tab == sess || !sess->gui->is_tab))
		{
			session_gui *gui = sess->gui;

			switch (type)
			{
			case FE_SE_CONNECTING:	/* connecting in progress */
			case FE_SE_RECONDELAY:	/* reconnect delay begun */
				/* enable Disconnect item */
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_DISCONNECT], 1);
				break;

			case FE_SE_CONNECT:
				/* enable Disconnect and Away menu items */
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_AWAY], 1);
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_DISCONNECT], 1);
				break;

			case FE_SE_LOGGEDIN:	/* end of MOTD */
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_JOIN], 1);
				/* if number of auto-join channels is zero, open joind */
				if (arg == 0)
					joind_open (serv);
				break;

			case FE_SE_DISCONNECT:
				/* disable Disconnect and Away menu items */
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_AWAY], 0);
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_DISCONNECT], 0);
				gtk_widget_set_sensitive (gui->menu_item[MENU_ID_JOIN], 0);
				/* close the join-dialog, if one exists */
				joind_close (serv);
			}
		}
		list = list->next;
	}
}

void
fe_get_file (const char *title, char *initial,
				 void (*callback) (void *userdata, char *file), void *userdata,
				 int flags)

{
	/* OK: Call callback once per file, then once more with file=NULL. */
	/* CANCEL: Call callback once with file=NULL. */
	gtkutil_file_req (title, callback, userdata, initial, NULL, flags | FRF_FILTERISINITIAL);
}

void
fe_open_chan_list (server *serv, char *filter, int do_refresh)
{
	chanlist_opengui (serv, do_refresh);
}

const char *
fe_get_default_font (void)
{
#ifdef _WIN32
	if (gtkutil_find_font ("Consolas"))
		return "Consolas 10";
	else
#else
#ifdef __APPLE__
	if (gtkutil_find_font ("Menlo"))
		return "Menlo 13";
	else
#endif
#endif
		return NULL;
}
