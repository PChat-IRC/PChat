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

#ifndef PCHAT_TRAY_BACKEND_H
#define PCHAT_TRAY_BACKEND_H

#include <gtk/gtk.h>

/* Opaque handle for platform-specific tray icon */
typedef struct _TrayBackend TrayBackend;

/* Callback function types */
typedef void (*TrayClickCallback)(void *userdata);
typedef void (*TrayMenuCallback)(GtkWidget *widget, guint button, guint time, void *userdata);

/* Initialize the tray backend with an icon */
TrayBackend* tray_backend_new(GdkPixbuf *icon, const char *tooltip);

/* Set the tray icon image */
void tray_backend_set_icon(TrayBackend *backend, GdkPixbuf *icon);

/* Set the tray icon tooltip */
void tray_backend_set_tooltip(TrayBackend *backend, const char *tooltip);

/* Show or hide the tray icon */
void tray_backend_set_visible(TrayBackend *backend, gboolean visible);

/* Check if tray icon is embedded/visible */
gboolean tray_backend_is_embedded(TrayBackend *backend);

/* Set callback for left-click/activate */
void tray_backend_set_activate_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata);

/* Set callback for right-click/popup menu */
void tray_backend_set_menu_callback(TrayBackend *backend, TrayMenuCallback callback, void *userdata);

/* Set callback for embedded state changes */
void tray_backend_set_embedded_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata);

/* Destroy the tray icon and free resources */
void tray_backend_destroy(TrayBackend *backend);

/* Rebuild the menu (for AppIndicator) */
void tray_backend_rebuild_menu(TrayBackend *backend);

/* Get backend type name for debugging */
const char* tray_backend_get_type(void);

#endif /* PCHAT_TRAY_BACKEND_H */
