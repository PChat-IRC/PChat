/* PChat
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

#include "tray-linux.h"
#include <libayatana-appindicator/app-indicator.h>
#include <string.h>
#include <unistd.h>

struct _TrayBackend
{
	AppIndicator *indicator;
	GtkWidget *menu;
	GdkPixbuf *current_icon;
	gchar *icon_name;
	gchar *temp_icon_path;
	
	/* Callbacks */
	TrayClickCallback activate_callback;
	void *activate_userdata;
	TrayMenuCallback menu_callback;
	void *menu_userdata;
	TrayClickCallback embedded_callback;
	void *embedded_userdata;
};

/* Convert GdkPixbuf to temporary icon file for AppIndicator */
static gchar*
save_pixbuf_to_temp(GdkPixbuf *pixbuf, const char *name)
{
	gchar *filename;
	GError *error = NULL;
	
	filename = g_strdup_printf("/tmp/pchat-tray-%s-%d.png", name, getpid());
	
	if (!gdk_pixbuf_save(pixbuf, filename, "png", &error, NULL))
	{
		g_warning("Failed to save tray icon: %s", error ? error->message : "unknown error");
		if (error)
			g_error_free(error);
		g_free(filename);
		return NULL;
	}
	
	return filename;
}

/* Activation handler - called when clicking restore/hide menu item */
static void
on_activate_clicked(GtkMenuItem *item, gpointer userdata)
{
	TrayBackend *backend = (TrayBackend*)userdata;
	
	if (backend && backend->activate_callback)
	{
		backend->activate_callback(backend->activate_userdata);
	}
}

TrayBackend* tray_linux_new(GdkPixbuf *icon, const char *tooltip)
{
	TrayBackend *backend;
	
	backend = g_new0(TrayBackend, 1);
	
	/* Create AppIndicator */
	backend->indicator = app_indicator_new(
		"pchat-tray",
		"pchat",
		APP_INDICATOR_CATEGORY_COMMUNICATIONS
	);
	
	/* Set initial icon */
	if (icon)
	{
		backend->current_icon = g_object_ref(icon);
		backend->icon_name = g_strdup("pchat-normal");
		backend->temp_icon_path = save_pixbuf_to_temp(icon, backend->icon_name);
		
		if (backend->temp_icon_path)
		{
			/* Extract directory and filename for AppIndicator */
			gchar *dirname = g_path_get_dirname(backend->temp_icon_path);
			gchar *basename = g_path_get_basename(backend->temp_icon_path);
			gchar *name_without_ext = g_strndup(basename, strlen(basename) - 4); /* Remove .png */
			
			app_indicator_set_icon_theme_path(backend->indicator, dirname);
			app_indicator_set_icon_full(backend->indicator, name_without_ext, tooltip);
			
			g_free(dirname);
			g_free(basename);
			g_free(name_without_ext);
		}
	}
	
	/* Create empty menu (AppIndicator requires a menu, will be populated later) */
	backend->menu = gtk_menu_new();
	app_indicator_set_menu(backend->indicator, GTK_MENU(backend->menu));
	
	/* Set status to active (visible) */
	app_indicator_set_status(backend->indicator, APP_INDICATOR_STATUS_ACTIVE);
	
	return backend;
}

void tray_linux_set_icon(TrayBackend *backend, GdkPixbuf *icon)
{
	if (!backend || !icon)
		return;
	
	/* Clean up old icon */
	if (backend->current_icon)
		g_object_unref(backend->current_icon);
	if (backend->temp_icon_path)
	{
		unlink(backend->temp_icon_path);
		g_free(backend->temp_icon_path);
	}
	if (backend->icon_name)
		g_free(backend->icon_name);
	
	/* Save new icon */
	backend->current_icon = g_object_ref(icon);
	backend->icon_name = g_strdup_printf("pchat-%ld", g_get_real_time());
	backend->temp_icon_path = save_pixbuf_to_temp(icon, backend->icon_name);
	
	if (backend->temp_icon_path)
	{
		gchar *dirname = g_path_get_dirname(backend->temp_icon_path);
		gchar *basename = g_path_get_basename(backend->temp_icon_path);
		gchar *name_without_ext = g_strndup(basename, strlen(basename) - 4);
		
		app_indicator_set_icon_theme_path(backend->indicator, dirname);
		app_indicator_set_icon_full(backend->indicator, name_without_ext, "PChat");
		
		g_free(dirname);
		g_free(basename);
		g_free(name_without_ext);
	}
}

void tray_linux_set_tooltip(TrayBackend *backend, const char *tooltip)
{
	if (!backend || !tooltip)
		return;
	
	/* AppIndicator doesn't support tooltips directly, but we can set the title */
	app_indicator_set_title(backend->indicator, tooltip);
}

void tray_linux_set_visible(TrayBackend *backend, gboolean visible)
{
	if (!backend)
		return;
	
	app_indicator_set_status(backend->indicator,
		visible ? APP_INDICATOR_STATUS_ACTIVE : APP_INDICATOR_STATUS_PASSIVE);
}

gboolean tray_linux_is_embedded(TrayBackend *backend)
{
	if (!backend)
		return FALSE;
	
	/* AppIndicator is always "embedded" when active */
	return app_indicator_get_status(backend->indicator) == APP_INDICATOR_STATUS_ACTIVE;
}

void tray_linux_set_activate_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata)
{
	if (!backend)
		return;
	
	backend->activate_callback = callback;
	backend->activate_userdata = userdata;
	
	/* Note: AppIndicator doesn't have a direct "activate" signal like GtkStatusIcon
	 * We'll handle this through the menu instead */
}

void tray_linux_set_menu_callback(TrayBackend *backend, TrayMenuCallback callback, void *userdata)
{
	if (!backend)
		return;
	
	backend->menu_callback = callback;
	backend->menu_userdata = userdata;
	
	/* Build initial menu */
	if (callback && backend->menu)
	{
		callback(GTK_WIDGET(backend->menu), 3, 0, userdata);
	}
}

void tray_linux_set_embedded_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata)
{
	if (!backend)
		return;
	
	backend->embedded_callback = callback;
	backend->embedded_userdata = userdata;
	
	/* AppIndicator doesn't have an embedded notification like GtkStatusIcon */
}

void tray_linux_rebuild_menu(TrayBackend *backend)
{
	if (!backend || !backend->menu || !backend->menu_callback)
		return;
	
	/* Clear existing menu items */
	GList *children = gtk_container_get_children(GTK_CONTAINER(backend->menu));
	for (GList *iter = children; iter != NULL; iter = iter->next)
		gtk_widget_destroy(GTK_WIDGET(iter->data));
	g_list_free(children);
	
	/* Rebuild menu */
	backend->menu_callback(GTK_WIDGET(backend->menu), 3, 0, backend->menu_userdata);
}

void tray_linux_destroy(TrayBackend *backend)
{
	if (!backend)
		return;
	
	/* Clean up temporary icon file */
	if (backend->temp_icon_path)
	{
		unlink(backend->temp_icon_path);
		g_free(backend->temp_icon_path);
	}
	
	if (backend->icon_name)
		g_free(backend->icon_name);
	
	if (backend->current_icon)
		g_object_unref(backend->current_icon);
	
	if (backend->menu)
		gtk_widget_destroy(backend->menu);
	
	if (backend->indicator)
		g_object_unref(backend->indicator);
	
	g_free(backend);
}
