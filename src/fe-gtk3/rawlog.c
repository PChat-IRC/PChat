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
#include <fcntl.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "fe-gtk.h"

#include <gdk/gdkkeysyms.h>

#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/cfgfiles.h"
#include "../common/server.h"
#include "gtkutil.h"
#include "palette.h"
#include "maingui.h"
#include "rawlog.h"
#include "textview-chat.h"
#include "fkeys.h"

static void
close_rawlog (GtkWidget *wid, server *serv)
{
	if (is_server (serv))
	{
		if (serv->gui->rawlog_buffer)
		{
			pchat_chat_buffer_free (serv->gui->rawlog_buffer);
			serv->gui->rawlog_buffer = NULL;
		}
		serv->gui->rawlog_window = 0;
	}
}

static void
rawlog_save (server *serv, char *file)
{
	int fh = -1;

	if (file)
	{
		if (serv->gui->rawlog_window)
			fh = pchat_open_file (file, O_TRUNC | O_WRONLY | O_CREAT,
										 0600, XOF_DOMODE | XOF_FULLPATH);
		if (fh != -1)
		{
			GtkTextBuffer *buffer;
			GtkTextIter start, end;
			gchar *text;
			
			buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (serv->gui->rawlog_textlist));
			gtk_text_buffer_get_bounds (buffer, &start, &end);
			text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
			
			write (fh, text, strlen (text));
			g_free (text);
			close (fh);
		}
	}
}

static int
rawlog_clearbutton (GtkWidget * wid, server *serv)
{
	pchat_textview_chat_clear (PCHAT_TEXTVIEW_CHAT (serv->gui->rawlog_textlist), 0);
	return FALSE;
}

static int
rawlog_savebutton (GtkWidget * wid, server *serv)
{
	gtkutil_file_req (_("Save As..."), rawlog_save, serv, NULL, NULL, FRF_WRITE);
	return FALSE;
}

static gboolean
rawlog_key_cb (GtkWidget * wid, GdkEventKey * key, gpointer userdata)
{
	/* Copy rawlog selection to clipboard when Ctrl+Shift+C is pressed,
	 * but make sure not to copy twice, i.e. when auto-copy is enabled.
	 */
	if (!prefs.pchat_text_autocopy_text &&
		(key->keyval == GDK_KEY_c || key->keyval == GDK_KEY_C) &&
		key->state & STATE_SHIFT &&
		key->state & STATE_CTRL)
	{
		GtkTextBuffer *buffer;
		GtkClipboard *clipboard;
		
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (userdata));
		clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
		gtk_text_buffer_copy_clipboard (buffer, clipboard);
	}
	return FALSE;
}

void
open_rawlog (struct server *serv)
{
	GtkWidget *bbox, *scrolledwindow, *vbox;
	char tbuf[256];

	if (serv->gui->rawlog_window)
	{
		mg_bring_tofront (serv->gui->rawlog_window);
		return;
	}

	snprintf (tbuf, sizeof tbuf, _(DISPLAY_NAME": Raw Log (%s)"), serv->servername);
	serv->gui->rawlog_window =
		mg_create_generic_tab ("RawLog", tbuf, FALSE, TRUE, close_rawlog, serv,
							 750, 500, &vbox, serv);
	gtkutil_destroy_on_esc (serv->gui->rawlog_window);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);

	serv->gui->rawlog_textlist = pchat_textview_chat_new ();
	gtk_container_add (GTK_CONTAINER (scrolledwindow), serv->gui->rawlog_textlist);
	pchat_textview_chat_set_font (PCHAT_TEXTVIEW_CHAT (serv->gui->rawlog_textlist), prefs.pchat_text_font);
	pchat_textview_chat_set_palette (PCHAT_TEXTVIEW_CHAT (serv->gui->rawlog_textlist), colors, 37);
	pchat_textview_chat_set_show_timestamps (PCHAT_TEXTVIEW_CHAT (serv->gui->rawlog_textlist), FALSE);
	
	/* Create and set buffer */
	serv->gui->rawlog_buffer = pchat_chat_buffer_new (PCHAT_TEXTVIEW_CHAT (serv->gui->rawlog_textlist));
	pchat_chat_buffer_show (PCHAT_TEXTVIEW_CHAT (serv->gui->rawlog_textlist), serv->gui->rawlog_buffer);

	bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_set_homogeneous (GTK_BOX (bbox), TRUE);
	gtk_box_pack_end (GTK_BOX (vbox), bbox, 0, 0, 4);

	gtkutil_button (bbox, "_Clear", NULL, rawlog_clearbutton,
						 serv, _("Clear Raw Log"));

	gtkutil_button (bbox, "_Save", NULL, rawlog_savebutton,
						 serv, _("Save As..."));

	/* Copy selection to clipboard when Ctrl+Shift+C is pressed AND text auto-copy is disabled */
	g_signal_connect (G_OBJECT (serv->gui->rawlog_window), "key_press_event", G_CALLBACK (rawlog_key_cb), serv->gui->rawlog_textlist);

	gtk_widget_show_all (serv->gui->rawlog_window);
}

void
fe_add_rawlog (server *serv, char *text, int len, int outbound)
{
	char **split_text;
	char *new_text;
	int i;

	if (!serv->gui->rawlog_window)
		return;

	split_text = g_strsplit (text, "\r\n", 0);

	for (i = 0; i < g_strv_length (split_text); i++)
	{
		if (split_text[i][0] == 0)
			break;

		if (outbound)
			new_text = g_strconcat ("\00304<<\017 ", split_text[i], "\n", NULL);
		else
			new_text = g_strconcat ("\00303>>\017 ", split_text[i], "\n", NULL);

		pchat_textview_chat_append (PCHAT_TEXTVIEW_CHAT (serv->gui->rawlog_textlist), new_text, strlen (new_text));

		g_free (new_text);
	}

	g_strfreev (split_text);
}
