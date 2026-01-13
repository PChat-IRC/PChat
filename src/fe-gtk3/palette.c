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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "fe-gtk.h"
#include "palette.h"

#include "../common/pchat.h"
#include "../common/util.h"
#include "../common/cfgfiles.h"
#include "../common/typedef.h"

/* Linear RGB values from GTK2 - will be converted to sRGB at runtime */
static const guint16 default_colors[][3] = {
	/* colors for xtext */
	{0xd3d3, 0xd7d7, 0xcfcf}, /* 0 white */
	{0x2e2e, 0x3434, 0x3636}, /* 1 black */
	{0x3434, 0x6565, 0xa4a4}, /* 2 blue */
	{0x4e4e, 0x9a9a, 0x0606}, /* 3 green */
	{0xcccc, 0x0000, 0x0000}, /* 4 red */
	{0x8f8f, 0x3939, 0x0202}, /* 5 light red */
	{0x5c5c, 0x3535, 0x6666}, /* 6 purple */
	{0xcece, 0x5c5c, 0x0000}, /* 7 orange */
	{0x9999, 0x7a7a, 0x0000}, /* 8 yellow (darker for readability) */
	{0x7373, 0xd2d2, 0x1616}, /* 9 green */
	{0x1111, 0xa8a8, 0x7979}, /* 10 aqua */
	{0x5858, 0xa1a1, 0x9d9d}, /* 11 light aqua */
	{0x5757, 0x7979, 0x9e9e}, /* 12 blue */
	{0xa0d0, 0x42d4, 0x6562}, /* 13 light purple */
	{0x5555, 0x5757, 0x5353}, /* 14 grey */
	{0x8888, 0x8a8a, 0x8585}, /* 15 light grey */

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

	{0xd3d3, 0xd7d7, 0xcfcf}, /* 32 marktext Fore (white) */
	{0x2020, 0x4a4a, 0x8787}, /* 33 marktext Back (blue) */
	{0x2512, 0x29e8, 0x2b85}, /* 34 foreground (black) */
	{0xfae0, 0xfae0, 0xf8c4}, /* 35 background (white) */
	{0x8f8f, 0x3939, 0x0202}, /* 36 marker line (red) */

	/* colors for GUI */
	{0x8f8f, 0x3939, 0x0202}, /* 37 tab New Data (dark red) */
	{0x3434, 0x6565, 0xa4a4}, /* 38 tab Nick Mentioned (blue) */
	{0xcccc, 0x0000, 0x0000}, /* 39 tab New Message (red) */
	{0x8888, 0x8a8a, 0x8585}, /* 40 away user (grey) */
	{0xa4a4, 0x0000, 0x0000}, /* 41 spell checker color (red) */
};

GdkRGBA colors[MAX_COL + 1];


void
palette_alloc (GtkWidget * widget)
{
	static int done_init = FALSE;
	int i;

	if (!done_init)
	{
		done_init = TRUE;
		/* Initialize colors from default values - darken for GTK3 brightness */
		for (i = 0; i <= MAX_COL; i++)
		{
			/* Skip background (35) to keep it light */
			if (i == 35)
			{
				colors[i].red = default_colors[i][0] / 65535.0;
				colors[i].green = default_colors[i][1] / 65535.0;
				colors[i].blue = default_colors[i][2] / 65535.0;
			}
			else
			{
				colors[i].red = (default_colors[i][0] / 65535.0) * 0.7;
				colors[i].green = (default_colors[i][1] / 65535.0) * 0.7;
				colors[i].blue = (default_colors[i][2] / 65535.0) * 0.7;
			}
			colors[i].alpha = 1.0;
		}
	}
}

/* maps XChat 2.0.x colors to current */
static const int remap[] =
{
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
	33,	/* 16:marktextback */
	32,	/* 17:marktextfore */
	34,	/* 18: fg */
	35,	/* 19: bg */
	37,	/* 20: newdata */
	38,	/* 21: blue */
	39,	/* 22: newmsg */
	40		/* 23: away */
};

void
palette_load (void)
{
	int i, j, l, fh, res;
	char prefname[256];
	struct stat st;
	char *cfg;
	guint16 red, green, blue;
	int upgrade = FALSE;

	fh = pchat_open_file ("colors.conf", O_RDONLY, 0, 0);
	if (fh == -1)
	{
		fh = pchat_open_file ("palette.conf", O_RDONLY, 0, 0);
		upgrade = TRUE;
	}

	if (fh != -1)
	{
		fstat (fh, &st);
		cfg = g_malloc (st.st_size + 1);
		if (cfg)
		{
			cfg[0] = '\0';
			l = read (fh, cfg, st.st_size);
			if (l >= 0)
				cfg[l] = '\0';

			if (!upgrade)
			{
				/* mIRC colors 0-31 are here */
				for (i = 0; i < 32; i++)
				{
					snprintf (prefname, sizeof prefname, "color_%d", i);
					cfg_get_color (cfg, prefname, &red, &green, &blue);
					colors[i].red = red / 65535.0;
					colors[i].green = green / 65535.0;
					colors[i].blue = blue / 65535.0;
					colors[i].alpha = 1.0;
				}

				/* our special colors are mapped at 256+ */
				for (i = 256, j = 32; j < MAX_COL+1; i++, j++)
				{
					snprintf (prefname, sizeof prefname, "color_%d", i);
					cfg_get_color (cfg, prefname, &red, &green, &blue);
					colors[j].red = red / 65535.0;
					colors[j].green = green / 65535.0;
					colors[j].blue = blue / 65535.0;
					colors[j].alpha = 1.0;
				}

			} else
			{
				/* loading 2.0.x palette.conf */
				for (i = 0; i < MAX_COL+1; i++)
				{
					snprintf (prefname, sizeof prefname, "color_%d_red", i);
					red = cfg_get_int (cfg, prefname);

					snprintf (prefname, sizeof prefname, "color_%d_grn", i);
					green = cfg_get_int (cfg, prefname);

					snprintf (prefname, sizeof prefname, "color_%d_blu", i);
					blue = cfg_get_int_with_result (cfg, prefname, &res);

					if (res)
					{
						colors[remap[i]].red = red / 65535.0;
						colors[remap[i]].green = green / 65535.0;
						colors[remap[i]].blue = blue / 65535.0;
						colors[remap[i]].alpha = 1.0;
					}
				}

				/* copy 0-15 to 16-31 */
				for (i = 0; i < 16; i++)
				{
					colors[i+16] = colors[i];
				}
			}
			g_free (cfg);
		}
		close (fh);
	}
}

void
palette_save (void)
{
	int i, j, fh;
	char prefname[256];

	fh = pchat_open_file ("colors.conf", O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
	if (fh != -1)
	{
		/* mIRC colors 0-31 are here */
		for (i = 0; i < 32; i++)
		{
			snprintf (prefname, sizeof prefname, "color_%d", i);
			cfg_put_color (fh, (int)(colors[i].red * 65535), (int)(colors[i].green * 65535), (int)(colors[i].blue * 65535), prefname);
		}

		/* our special colors are mapped at 256+ */
		for (i = 256, j = 32; j < MAX_COL+1; i++, j++)
		{
			snprintf (prefname, sizeof prefname, "color_%d", i);
			cfg_put_color (fh, (int)(colors[j].red * 65535), (int)(colors[j].green * 65535), (int)(colors[j].blue * 65535), prefname);
		}

		close (fh);
	}
}
