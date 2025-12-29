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

#ifndef PCHAT_TRAY_MACOS_H
#define PCHAT_TRAY_MACOS_H

#include "tray-backend.h"

/* macOS NSStatusBar implementation */
TrayBackend* tray_macos_new(GdkPixbuf *icon, const char *tooltip);
void tray_macos_set_icon(TrayBackend *backend, GdkPixbuf *icon);
void tray_macos_set_tooltip(TrayBackend *backend, const char *tooltip);
void tray_macos_set_visible(TrayBackend *backend, gboolean visible);
gboolean tray_macos_is_embedded(TrayBackend *backend);
void tray_macos_set_activate_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata);
void tray_macos_set_menu_callback(TrayBackend *backend, TrayMenuCallback callback, void *userdata);
void tray_macos_set_embedded_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata);
void tray_macos_destroy(TrayBackend *backend);

#endif /* PCHAT_TRAY_MACOS_H */
