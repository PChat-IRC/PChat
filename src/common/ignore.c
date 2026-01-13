/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "pchat.h"
#include "ignore.h"
#include "cfgfiles.h"
#include "fe.h"
#include "text.h"
#include "util.h"
#include "pchatc.h"
#include "typedef.h"


int ignored_ctcp = 0;			  /* keep a count of all we ignore */
int ignored_priv = 0;
int ignored_chan = 0;
int ignored_noti = 0;
int ignored_invi = 0;
static int ignored_total = 0;

/* ignore_exists ():
 * returns: struct ig, if this mask is in the ignore list already
 *          NULL, otherwise
 */
struct ignore *
ignore_exists (char *mask)
{
	struct ignore *ig = NULL;
	GSList *list;

	list = ignore_list;
	while (list)
	{
		ig = (struct ignore *) list->data;
		if (!rfc_casecmp (ig->mask, mask))
			return ig;
		list = list->next;
	}
	return NULL;

}

/* ignore_add(...)

 * returns:
 *            0 fail
 *            1 success
 *            2 success (old ignore has been changed)
 */

int
ignore_add (char *mask, int type, gboolean overwrite)
{
	struct ignore *ig = NULL;
	int change_only = FALSE;

	/* first check if it's already ignored */
	ig = ignore_exists (mask);
	if (ig)
		change_only = TRUE;

	if (!change_only)
		ig = g_new (struct ignore, 1);

	ig->mask = g_strdup (mask);

	if (!overwrite && change_only)
		ig->type |= type;
	else
		ig->type = type;

	if (!change_only)
		ignore_list = g_slist_prepend (ignore_list, ig);
	fe_ignore_update (1);

	if (change_only)
		return 2;

	return 1;
}

void
ignore_showlist (session *sess)
{
	struct ignore *ig;
	GSList *list = ignore_list;
	char tbuf[256];
	char *pos;
	int i = 0;

	EMIT_SIGNAL (XP_TE_IGNOREHEADER, sess, 0, 0, 0, 0, 0);

	while (list)
	{
		ig = list->data;
		i++;

		/* Use tracked position to avoid strcat rescanning the buffer each time */
		pos = tbuf + g_snprintf (tbuf, sizeof (tbuf), " %-25s ", ig->mask);

		/* Each column is 5 chars ("YES  " or "NO   ") */
		#define APPEND_YESNO(flag) do { \
			if (ig->type & (flag)) { \
				memcpy (pos, _("YES  "), 5); \
			} else { \
				memcpy (pos, _("NO   "), 5); \
			} \
			pos += 5; \
		} while (0)

		APPEND_YESNO(IG_PRIV);
		APPEND_YESNO(IG_NOTI);
		APPEND_YESNO(IG_CHAN);
		APPEND_YESNO(IG_CTCP);
		APPEND_YESNO(IG_DCC);
		APPEND_YESNO(IG_INVI);
		APPEND_YESNO(IG_UNIG);

		#undef APPEND_YESNO

		*pos++ = '\n';
		*pos = '\0';

		PrintText (sess, tbuf);
		/*EMIT_SIGNAL (XP_TE_IGNORELIST, sess, ig->mask, 0, 0, 0, 0); */
		/* use this later, when TE's support 7 args */
		list = list->next;
	}

	if (!i)
		EMIT_SIGNAL (XP_TE_IGNOREEMPTY, sess, 0, 0, 0, 0, 0);

	EMIT_SIGNAL (XP_TE_IGNOREFOOTER, sess, 0, 0, 0, 0, 0);
}

/* ignore_del()

 * one of the args must be NULL, use mask OR *ig, not both
 *
 */

int
ignore_del (char *mask, struct ignore *ig)
{
	if (!ig)
	{
		GSList *list = ignore_list;

		while (list)
		{
			ig = (struct ignore *) list->data;
			if (!rfc_casecmp (ig->mask, mask))
				break;
			list = list->next;
			ig = 0;
		}
	}
	if (ig)
	{
		ignore_list = g_slist_remove (ignore_list, ig);
		g_free (ig->mask);
		g_free (ig);
		fe_ignore_update (1);
		return TRUE;
	}
	return FALSE;
}

/* check if a msg should be ignored by browsing our ignore list */
/* Optimized: single-pass check instead of two iterations */

int
ignore_check (char *host, int type)
{
	struct ignore *ig;
	GSList *list = ignore_list;
	struct ignore *matched_ignore = NULL;

	/* Single pass: find matching ignore, but UNIGNORE takes precedence */
	while (list)
	{
		ig = (struct ignore *) list->data;

		if (ig->type & type)
		{
			if (match (ig->mask, host))
			{
				/* UNIGNORE entries take precedence - return immediately */
				if (ig->type & IG_UNIG)
					return FALSE;

				/* Remember the first matching ignore entry */
				if (!matched_ignore)
					matched_ignore = ig;
			}
		}
		list = list->next;
	}

	/* If we found a matching ignore (and no UNIGNORE matched), count it */
	if (matched_ignore)
	{
		ignored_total++;
		if (type & IG_PRIV)
			ignored_priv++;
		if (type & IG_NOTI)
			ignored_noti++;
		if (type & IG_CHAN)
			ignored_chan++;
		if (type & IG_CTCP)
			ignored_ctcp++;
		if (type & IG_INVI)
			ignored_invi++;
		fe_ignore_update (2);
		return TRUE;
	}

	return FALSE;
}

static char *
ignore_read_next_entry (char *my_cfg, struct ignore *ignore)
{
	char tbuf[1024];

	/* Casting to char * done below just to satisfy compiler */

	if (my_cfg)
	{
		my_cfg = cfg_get_str (my_cfg, "mask", tbuf, sizeof (tbuf));
		if (!my_cfg)
			return NULL;
		ignore->mask = g_strdup (tbuf);
	}
	if (my_cfg)
	{
		my_cfg = cfg_get_str (my_cfg, "type", tbuf, sizeof (tbuf));
		ignore->type = atoi (tbuf);
	}
	return my_cfg;
}

void
ignore_load ()
{
	struct ignore *ignore;
	struct stat st;
	char *cfg, *my_cfg;
	int fh;

	fh = pchat_open_file ("ignore.conf", O_RDONLY, 0, 0);
	if (fh != -1)
	{
		fstat (fh, &st);
		if (st.st_size)
		{
			cfg = g_malloc0 (st.st_size + 1);
			read (fh, cfg, st.st_size);
			my_cfg = cfg;
			while (my_cfg)
			{
				ignore = g_new0 (struct ignore, 1);
				if ((my_cfg = ignore_read_next_entry (my_cfg, ignore)))
					ignore_list = g_slist_prepend (ignore_list, ignore);
				else
					g_free (ignore);
			}
			g_free (cfg);
		}
		close (fh);
	}
}

void
ignore_save ()
{
	char buf[1024];
	int fh;
	GSList *temp = ignore_list;
	struct ignore *ig;

	fh = pchat_open_file ("ignore.conf", O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
	if (fh != -1)
	{
		while (temp)
		{
			ig = (struct ignore *) temp->data;
			if (!(ig->type & IG_NOSAVE))
			{
				g_snprintf (buf, sizeof (buf), "mask = %s\ntype = %u\n\n",
							 ig->mask, ig->type);
				write (fh, buf, strlen (buf));
			}
			temp = temp->next;
		}
		close (fh);
	}

}

static gboolean
flood_autodialog_timeout (gpointer data)
{
	prefs.pchat_gui_autoopen_dialog = 1;
	return FALSE;
}

int
flood_check (char *nick, char *ip, server *serv, session *sess, int what)	/*0=ctcp  1=priv */
{
	time_t current_time;
	current_time = time (NULL);

	if (what == 0)
	{
		if (serv->ctcp_last_time == 0)	/*first ctcp in this server */
		{
			serv->ctcp_last_time = time (NULL);
			serv->ctcp_counter++;
		} else
		{
			if (difftime (current_time, serv->ctcp_last_time) < prefs.pchat_flood_ctcp_time)	/*if we got the ctcp in the seconds limit */
			{
				serv->ctcp_counter++;
				if (serv->ctcp_counter == prefs.pchat_flood_ctcp_num)	/*if we reached the maximun numbers of ctcp in the seconds limits */
				{
					char *mask, *message, *real_ip;

					serv->ctcp_last_time = current_time;	/*we got the flood, restore all the vars for next one */
					serv->ctcp_counter = 0;

					real_ip = strchr (ip, '@');
					if (real_ip != NULL)
						mask = g_strdup_printf ("*!*%s", real_ip);
					else
						mask = g_strdup_printf ("%s!*", nick);

					message = g_strdup_printf (_("You are being CTCP flooded from %s, ignoring %s\n"), nick, mask);

					PrintText (sess, message);
					ignore_add (mask, IG_CTCP, FALSE);

					g_free (message);
					g_free (mask);
					return 0;
				}
			}
		}
	} else
	{
		if (serv->msg_last_time == 0)
		{
			serv->msg_last_time = time (NULL);
			serv->ctcp_counter++;
		} else
		{
			if (difftime (current_time, serv->msg_last_time) <
				 prefs.pchat_flood_msg_time)
			{
				char buf[512];
				serv->msg_counter++;
				if (serv->msg_counter == prefs.pchat_flood_msg_num)	/*if we reached the maximun numbers of ctcp in the seconds limits */
				{
					g_snprintf (buf, sizeof (buf),
					 _("You are being MSG flooded from %s, setting gui_autoopen_dialog OFF.\n"),
								 ip);
					PrintText (sess, buf);
					serv->msg_last_time = current_time;	/*we got the flood, restore all the vars for next one */
					serv->msg_counter = 0;

					if (prefs.pchat_gui_autoopen_dialog)
					{
						prefs.pchat_gui_autoopen_dialog = 0;
						/* turn it back on in 30 secs */
						fe_timeout_add_seconds (30, flood_autodialog_timeout, NULL);
					}
					return 0;
				}
			}
		}
	}
	return 1;
}

