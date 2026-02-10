/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Color palette — mirrors fe-gtk3/palette.c
 */

#include "palette.h"

#include <wx/colour.h>
#include <wx/gdicmn.h>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <glib.h>

extern "C" {
#include "../common/pchat.h"
#include "../common/cfgfiles.h"
}

/* ---- Default palette (guint16 0–65535, matching fe-gtk3/palette.c) ---- */
static const unsigned short default_colors[][3] = {
    /* mIRC colors 0-15 */
    {0xd3d3, 0xd7d7, 0xcfcf}, /*  0 white */
    {0x2e2e, 0x3434, 0x3636}, /*  1 black */
    {0x3434, 0x6565, 0xa4a4}, /*  2 blue */
    {0x4e4e, 0x9a9a, 0x0606}, /*  3 green */
    {0xcccc, 0x0000, 0x0000}, /*  4 red */
    {0x8f8f, 0x3939, 0x0202}, /*  5 light red */
    {0x5c5c, 0x3535, 0x6666}, /*  6 purple */
    {0xcece, 0x5c5c, 0x0000}, /*  7 orange */
    {0x9999, 0x7a7a, 0x0000}, /*  8 yellow */
    {0x7373, 0xd2d2, 0x1616}, /*  9 green */
    {0x1111, 0xa8a8, 0x7979}, /* 10 aqua */
    {0x5858, 0xa1a1, 0x9d9d}, /* 11 light aqua */
    {0x5757, 0x7979, 0x9e9e}, /* 12 blue */
    {0xa0d0, 0x42d4, 0x6562}, /* 13 light purple */
    {0x5555, 0x5757, 0x5353}, /* 14 grey */
    {0x8888, 0x8a8a, 0x8585}, /* 15 light grey */

    /* Local/extended colors 16-31 */
    {0xd3d3, 0xd7d7, 0xcfcf}, /* 16 white */
    {0x2e2e, 0x3434, 0x3636}, /* 17 black */
    {0x3434, 0x6565, 0xa4a4}, /* 18 blue */
    {0x4e4e, 0x9a9a, 0x0606}, /* 19 green */
    {0xcccc, 0x0000, 0x0000}, /* 20 red */
    {0x8f8f, 0x3939, 0x0202}, /* 21 light red */
    {0x5c5c, 0x3535, 0x6666}, /* 22 purple */
    {0xcece, 0x5c5c, 0x0000}, /* 23 orange */
    {0xc4c4, 0xa0a0, 0x0000}, /* 24 yellow */
    {0x7373, 0xd2d2, 0x1616}, /* 25 green */
    {0x1111, 0xa8a8, 0x7979}, /* 26 aqua */
    {0x5858, 0xa1a1, 0x9d9d}, /* 27 light aqua */
    {0x5757, 0x7979, 0x9e9e}, /* 28 blue */
    {0xa0d0, 0x42d4, 0x6562}, /* 29 light purple */
    {0x5555, 0x5757, 0x5353}, /* 30 grey */
    {0x8888, 0x8a8a, 0x8585}, /* 31 light grey */

    /* Special colors 32-41 */
    {0xd3d3, 0xd7d7, 0xcfcf}, /* 32 marktext Fore (white) */
    {0x2020, 0x4a4a, 0x8787}, /* 33 marktext Back (blue) */
    {0x2512, 0x29e8, 0x2b85}, /* 34 foreground (dark) */
    {0xfae0, 0xfae0, 0xf8c4}, /* 35 background (light) */
    {0x8f8f, 0x3939, 0x0202}, /* 36 marker line (red) */
    {0x8f8f, 0x3939, 0x0202}, /* 37 tab New Data (dark red) */
    {0x3434, 0x6565, 0xa4a4}, /* 38 tab Nick Mentioned (blue) */
    {0xcccc, 0x0000, 0x0000}, /* 39 tab New Message (red) */
    {0x8888, 0x8a8a, 0x8585}, /* 40 away user (grey) */
    {0xa4a4, 0x0000, 0x0000}, /* 41 spell checker color (red) */
};

/* Palette storage — 16-bit per channel, 42 entries */
static unsigned short palette_r[NUM_COLORS];
static unsigned short palette_g[NUM_COLORS];
static unsigned short palette_b[NUM_COLORS];
static bool palette_initialized = false;

void
wx_palette_init(void)
{
    if (palette_initialized)
        return;
    palette_initialized = true;

    for (int i = 0; i < NUM_COLORS; i++) {
        if (i == COL_BG) {
            /* Keep background at full brightness */
            palette_r[i] = default_colors[i][0];
            palette_g[i] = default_colors[i][1];
            palette_b[i] = default_colors[i][2];
        } else {
            /* Darken other colors by 0.7 to match GTK3 palette_alloc() */
            palette_r[i] = (unsigned short)(default_colors[i][0] * 0.7);
            palette_g[i] = (unsigned short)(default_colors[i][1] * 0.7);
            palette_b[i] = (unsigned short)(default_colors[i][2] * 0.7);
        }
    }
}

void
wx_palette_load(void)
{
    wx_palette_init();

    int fh = pchat_open_file("colors.conf", O_RDONLY, 0, 0);
    if (fh == -1)
        return;

    struct stat st;
    fstat(fh, &st);
    char *cfg = (char *)g_malloc(st.st_size + 1);
    if (!cfg) {
        close(fh);
        return;
    }
    cfg[0] = '\0';
    int l = read(fh, cfg, st.st_size);
    if (l >= 0)
        cfg[l] = '\0';
    close(fh);

    char prefname[64];
    guint16 r, g, b;

    /* mIRC colors 0-31 */
    for (int i = 0; i < 32; i++) {
        snprintf(prefname, sizeof(prefname), "color_%d", i);
        if (cfg_get_color(cfg, prefname, &r, &g, &b)) {
            palette_r[i] = r;
            palette_g[i] = g;
            palette_b[i] = b;
        }
    }

    /* Special colors: stored as color_256+ → indices 32-41 */
    for (int i = 256, j = 32; j <= MAX_COL; i++, j++) {
        snprintf(prefname, sizeof(prefname), "color_%d", i);
        if (cfg_get_color(cfg, prefname, &r, &g, &b)) {
            palette_r[j] = r;
            palette_g[j] = g;
            palette_b[j] = b;
        }
    }

    g_free(cfg);
}

void
wx_palette_save(void)
{
    int fh = pchat_open_file("colors.conf",
                              O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
    if (fh == -1)
        return;

    char prefname[64];

    /* mIRC colors 0-31 */
    for (int i = 0; i < 32; i++) {
        snprintf(prefname, sizeof(prefname), "color_%d", i);
        cfg_put_color(fh, palette_r[i], palette_g[i], palette_b[i], prefname);
    }

    /* Special colors at 256+ */
    for (int i = 256, j = 32; j <= MAX_COL; i++, j++) {
        snprintf(prefname, sizeof(prefname), "color_%d", i);
        cfg_put_color(fh, palette_r[j], palette_g[j], palette_b[j], prefname);
    }

    close(fh);
}

wxColour
wx_palette_get(int index)
{
    if (index < 0 || index >= NUM_COLORS)
        return *wxBLACK;
    /* Convert 16-bit to 8-bit for wxColour */
    return wxColour(palette_r[index] >> 8,
                    palette_g[index] >> 8,
                    palette_b[index] >> 8);
}

void
wx_palette_set(int index, const wxColour &color)
{
    if (index < 0 || index >= NUM_COLORS)
        return;
    /* Convert 8-bit to 16-bit */
    palette_r[index] = (unsigned short)(color.Red()   << 8 | color.Red());
    palette_g[index] = (unsigned short)(color.Green() << 8 | color.Green());
    palette_b[index] = (unsigned short)(color.Blue()  << 8 | color.Blue());
}

void
wx_palette_get_rgb16(int index, unsigned short *r, unsigned short *g,
                     unsigned short *b)
{
    if (index < 0 || index >= NUM_COLORS) {
        *r = *g = *b = 0;
        return;
    }
    *r = palette_r[index];
    *g = palette_g[index];
    *b = palette_b[index];
}
