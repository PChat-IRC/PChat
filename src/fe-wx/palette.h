/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Color palette — mirrors fe-gtk3/palette.c
 * 42 colors: 0-15 mIRC standard, 16-31 local/extended,
 * 32-41 special (mark, fg, bg, marker, tabs, away, spell).
 * Loaded/saved from colors.conf in the same format as GTK3.
 */

#ifndef PCHAT_WX_PALETTE_H
#define PCHAT_WX_PALETTE_H

#include <wx/colour.h>

/* Special color indices */
#define COL_MARK_FG  32
#define COL_MARK_BG  33
#define COL_FG       34
#define COL_BG       35
#define COL_MARKER   36
#define COL_NEW_DATA 37
#define COL_HILIGHT  38
#define COL_NEW_MSG  39
#define COL_AWAY     40
#define COL_SPELL    41
#define MAX_COL      41
#define NUM_COLORS   (MAX_COL + 1)

/* Initialize palette with defaults */
void wx_palette_init(void);

/* Load palette from colors.conf (call after wx_palette_init) */
void wx_palette_load(void);

/* Save palette to colors.conf */
void wx_palette_save(void);

/* Get a palette color as wxColour */
wxColour wx_palette_get(int index);

/* Set a palette color from wxColour */
void wx_palette_set(int index, const wxColour &color);

/* Get raw RGB (0-65535 range) for a palette entry */
void wx_palette_get_rgb16(int index, unsigned short *r, unsigned short *g,
                          unsigned short *b);

#endif /* PCHAT_WX_PALETTE_H */
