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

#include "tray-backend.h"

/* Forward declarations for backend implementations */
#if defined(__linux__) && !defined(_WIN32)
#include "tray-linux.h"
#define BACKEND_NAME "AppIndicator"
#define backend_new tray_linux_new
#define backend_set_icon tray_linux_set_icon
#define backend_set_tooltip tray_linux_set_tooltip
#define backend_set_visible tray_linux_set_visible
#define backend_is_embedded tray_linux_is_embedded
#define backend_set_activate_callback tray_linux_set_activate_callback
#define backend_set_menu_callback tray_linux_set_menu_callback
#define backend_set_embedded_callback tray_linux_set_embedded_callback
#define backend_destroy tray_linux_destroy

#elif defined(_WIN32)
#include "tray-windows.h"
#define BACKEND_NAME "Shell_NotifyIcon"
#define backend_new tray_windows_new
#define backend_set_icon tray_windows_set_icon
#define backend_set_tooltip tray_windows_set_tooltip
#define backend_set_visible tray_windows_set_visible
#define backend_is_embedded tray_windows_is_embedded
#define backend_set_activate_callback tray_windows_set_activate_callback
#define backend_set_menu_callback tray_windows_set_menu_callback
#define backend_set_embedded_callback tray_windows_set_embedded_callback
#define backend_destroy tray_windows_destroy

#elif defined(__APPLE__)
/* macOS implementation - to be added later */
#define BACKEND_NAME "macOS"
/* Placeholder */

#else
/* Fallback to GtkStatusIcon */
#define BACKEND_NAME "GtkStatusIcon"
/* Placeholder */

#endif

/* Wrapper functions that dispatch to platform backend */

TrayBackend* tray_backend_new(GdkPixbuf *icon, const char *tooltip)
{
#ifdef backend_new
	return backend_new(icon, tooltip);
#else
	return NULL;
#endif
}

void tray_backend_set_icon(TrayBackend *backend, GdkPixbuf *icon)
{
#ifdef backend_set_icon
	if (backend)
		backend_set_icon(backend, icon);
#endif
}

void tray_backend_set_tooltip(TrayBackend *backend, const char *tooltip)
{
#ifdef backend_set_tooltip
	if (backend)
		backend_set_tooltip(backend, tooltip);
#endif
}

void tray_backend_set_visible(TrayBackend *backend, gboolean visible)
{
#ifdef backend_set_visible
	if (backend)
		backend_set_visible(backend, visible);
#endif
}

gboolean tray_backend_is_embedded(TrayBackend *backend)
{
#ifdef backend_is_embedded
	if (backend)
		return backend_is_embedded(backend);
#endif
	return FALSE;
}

void tray_backend_set_activate_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata)
{
#ifdef backend_set_activate_callback
	if (backend)
		backend_set_activate_callback(backend, callback, userdata);
#endif
}

void tray_backend_set_menu_callback(TrayBackend *backend, TrayMenuCallback callback, void *userdata)
{
#ifdef backend_set_menu_callback
	if (backend)
		backend_set_menu_callback(backend, callback, userdata);
#endif
}

void tray_backend_set_embedded_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata)
{
#ifdef backend_set_embedded_callback
	if (backend)
		backend_set_embedded_callback(backend, callback, userdata);
#endif
}

void tray_backend_destroy(TrayBackend *backend)
{
#ifdef backend_destroy
	if (backend)
		backend_destroy(backend);
#endif
}

void tray_backend_rebuild_menu(TrayBackend *backend)
{
#if defined(__linux__) && !defined(_WIN32)
	extern void tray_linux_rebuild_menu(TrayBackend *backend);
	tray_linux_rebuild_menu(backend);
#elif defined(_WIN32)
	/* Windows doesn't use persistent menus like AppIndicator,
	 * menu is created on-demand when right-clicking */
#endif
}

const char* tray_backend_get_type(void)
{
	return BACKEND_NAME;
}
