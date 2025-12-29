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

#ifndef PCHAT_UPDATE_CHECKER_H
#define PCHAT_UPDATE_CHECKER_H

#include "config.h"
#include <glib.h>
#include <gtk/gtk.h>

/* Sparkle-compatible update checker for PChat
 * Enabled by default on Windows and macOS builds
 * Automatically disabled in portable mode
 */

#ifdef HAVE_UPDATE_CHECKER

/* Initialize the update checker system */
void update_checker_init(void);

/* Cleanup resources */
void update_checker_cleanup(void);

/* Check for updates with UI feedback (shows dialog if update available) */
void update_checker_check_with_ui(GtkWindow *parent);

/* Check for updates silently (only shows dialog if update available) */
void update_checker_check_silently(void);

/* Set the appcast URL (Sparkle format) */
void update_checker_set_appcast_url(const char *url);

/* Enable/disable automatic update checks on startup */
void update_checker_set_automatic_checks(gboolean enabled);

#else
/* Stub implementations when update checker is disabled */
static inline void update_checker_init(void) {}
static inline void update_checker_cleanup(void) {}
static inline void update_checker_check_with_ui(GtkWindow *parent) {}
static inline void update_checker_check_silently(void) {}
static inline void update_checker_set_appcast_url(const char *url) {}
static inline void update_checker_set_automatic_checks(gboolean enabled) {}
#endif

#endif /* PCHAT_UPDATE_CHECKER_H */
