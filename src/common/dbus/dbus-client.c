/* dbus-client.c - HexChat command-line options for D-Bus
 * Copyright (C) 2006 Claessens Xavier
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
 *
 * Claessens Xavier
 * xclaesse@gmail.com
 */

#include "config.h"

#include "dbus-client.h"
#include <stdlib.h>
#include <gio/gio.h>
#include "pchat.h"
#include "pchatc.h"

#define DBUS_REMOTE_PATH "/org/pchat/Remote"
#define DBUS_REMOTE_INTERFACE "org.pchat.plugin"
#define DBUS_SERVICE "org.pchat.service"

#define DBUS_SERVICE_DBUS "org.freedesktop.DBus"
#define DBUS_PATH_DBUS "/org/freedesktop/DBus"
#define DBUS_INTERFACE_DBUS "org.freedesktop.DBus"

static void
write_error (char *message, GError **error)
{
	if (error == NULL || *error == NULL) {
		return;
	}
	g_printerr ("%s: %s\n", message, (*error)->message);
	g_clear_error (error);
}

static inline GVariant *
new_param_variant (const char *arg)
{
	GVariant * const args[1] = {
		g_variant_new_string (arg)
	};
	return g_variant_new_tuple (args, 1);
}

void
pchat_remote (void)
/* Note: This code now uses GDBus (GLib's D-Bus implementation) instead of
 * the older libdbus API, so the historical dbus_g_connection_unref crash
 * issue no longer applies. */
{
	GDBusConnection *connection;
	GDBusProxy *dbus = NULL;
	GVariant *ret;
	GDBusProxy *remote_object = NULL;
	gboolean pchat_running;
	GError *error = NULL;
	char *command = NULL;
	guint i;

	/* if there is nothing to do, return now. */
	if (!arg_existing || !(arg_url || arg_urls || arg_command)) {
		return;
	}

	arg_dont_autoconnect = TRUE;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (!connection)
	{
		write_error (_("Couldn't connect to session bus"), &error);
		return;
	}

	/* Checks if HexChat is already running */
	dbus = g_dbus_proxy_new_sync (connection,
								  G_DBUS_PROXY_FLAGS_NONE,
								  NULL,
								  DBUS_SERVICE_DBUS,
								  DBUS_PATH_DBUS,
								  DBUS_INTERFACE_DBUS,
								  NULL,
								  &error);

	ret = g_dbus_proxy_call_sync (dbus, "NameHasOwner",
								 new_param_variant (DBUS_SERVICE),
								 G_DBUS_CALL_FLAGS_NONE,
								 -1,
								 NULL,
								 &error);
	if (!ret)
	{
		write_error (_("Failed to complete NameHasOwner"), &error);
		pchat_running = FALSE;
	}
	else
	{
		GVariant *child = g_variant_get_child_value (ret, 0);
		pchat_running = g_variant_get_boolean (child);
		g_variant_unref (ret);
		g_variant_unref (child);
	}
	g_object_unref (dbus);

	if (!pchat_running) {
		g_object_unref (connection);
		return;
	}

	remote_object = g_dbus_proxy_new_sync (connection,
								  G_DBUS_PROXY_FLAGS_NONE,
								  NULL,
								  DBUS_SERVICE,
								  DBUS_REMOTE_PATH,
								  DBUS_REMOTE_INTERFACE,
								  NULL,
								  &error);

	if (!remote_object)
	{
		write_error("Failed to connect to PChat", &error);
		g_object_unref (connection);
		exit (0);
	}

	if (arg_url) {
		command = g_strdup_printf ("url %s", arg_url);
	} else if (arg_command) {
		command = g_strdup (arg_command);
	}

	if (command)
	{
		g_dbus_proxy_call_sync (remote_object, "Command",
								new_param_variant (command),
								G_DBUS_CALL_FLAGS_NONE,
								-1,
								NULL,
								&error);

		if (error)
			write_error (_("Failed to complete Command"), &error);
		g_free (command);
	}

	if (arg_urls)
	{
		for (i = 0; i < g_strv_length(arg_urls); i++)
		{
			command = g_strdup_printf ("url %s", arg_urls[i]);

			g_dbus_proxy_call_sync (remote_object, "Command",
									new_param_variant (command),
									G_DBUS_CALL_FLAGS_NONE,
									-1,
									NULL,
									&error);
			if (error)
				write_error (_("Failed to complete Command"), &error);
			g_free (command);
		}
		g_strfreev (arg_urls);
	}

	g_object_unref (remote_object);
	g_object_unref (connection);
	exit (0);
}
