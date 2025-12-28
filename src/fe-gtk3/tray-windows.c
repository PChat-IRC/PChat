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

#include "tray-windows.h"
#include <windows.h>
#include <shellapi.h>
#include <gdk/gdkwin32.h>

#define WM_TRAYICON (WM_APP + 1)
#define TRAY_ID 1

struct _TrayBackend
{
	NOTIFYICONDATAW nid;
	HWND hwnd;
	HICON hicon;
	GdkPixbuf *current_icon;
	gboolean visible;
	
	/* Callbacks */
	TrayClickCallback activate_callback;
	void *activate_userdata;
	TrayMenuCallback menu_callback;
	void *menu_userdata;
	TrayClickCallback embedded_callback;
	void *embedded_userdata;
};

/* Convert GdkPixbuf to Windows HICON */
static HICON
pixbuf_to_hicon(GdkPixbuf *pixbuf)
{
	HICON hicon = NULL;
	ICONINFO icon_info;
	BITMAP bm;
	int width, height;
	HDC hdc;
	HBITMAP hbm_color, hbm_mask;
	guchar *pixels;
	int stride, channels;
	
	if (!pixbuf)
		return NULL;
	
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	pixels = gdk_pixbuf_get_pixels(pixbuf);
	stride = gdk_pixbuf_get_rowstride(pixbuf);
	channels = gdk_pixbuf_get_n_channels(pixbuf);
	
	/* Create device contexts */
	hdc = GetDC(NULL);
	
	/* Create color bitmap */
	BITMAPV5HEADER bi;
	ZeroMemory(&bi, sizeof(BITMAPV5HEADER));
	bi.bV5Size = sizeof(BITMAPV5HEADER);
	bi.bV5Width = width;
	bi.bV5Height = -height; /* top-down */
	bi.bV5Planes = 1;
	bi.bV5BitCount = 32;
	bi.bV5Compression = BI_BITFIELDS;
	bi.bV5RedMask = 0x00FF0000;
	bi.bV5GreenMask = 0x0000FF00;
	bi.bV5BlueMask = 0x000000FF;
	bi.bV5AlphaMask = 0xFF000000;
	
	void *bits;
	hbm_color = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
	
	if (hbm_color && bits)
	{
		/* Copy pixel data with alpha channel */
		guchar *dest = (guchar*)bits;
		for (int y = 0; y < height; y++)
		{
			guchar *src = pixels + y * stride;
			for (int x = 0; x < width; x++)
			{
				guchar r = src[0];
				guchar g = src[1];
				guchar b = src[2];
				guchar a = (channels == 4) ? src[3] : 0xFF;
				
				/* Pre-multiply alpha for Windows */
				if (a < 0xFF)
				{
					r = (r * a) / 255;
					g = (g * a) / 255;
					b = (b * a) / 255;
				}
				
				/* BGRA format for Windows */
				*dest++ = b;
				*dest++ = g;
				*dest++ = r;
				*dest++ = a;
				
				src += channels;
			}
		}
		
		/* Create mask bitmap (required for ICONINFO) */
		hbm_mask = CreateBitmap(width, height, 1, 1, NULL);
		
		/* Create icon */
		icon_info.fIcon = TRUE;
		icon_info.xHotspot = 0;
		icon_info.yHotspot = 0;
		icon_info.hbmMask = hbm_mask;
		icon_info.hbmColor = hbm_color;
		
		hicon = CreateIconIndirect(&icon_info);
		
		DeleteObject(hbm_mask);
		DeleteObject(hbm_color);
	}
	
	ReleaseDC(NULL, hdc);
	return hicon;
}

/* Window procedure to handle tray icon messages */
static LRESULT CALLBACK
tray_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TrayBackend *backend = (TrayBackend*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	
	switch (msg)
	{
	case WM_TRAYICON:
		if (LOWORD(lParam) == WM_LBUTTONUP)
		{
			/* Left click - activate callback */
			if (backend && backend->activate_callback)
				backend->activate_callback(backend->activate_userdata);
		}
		else if (LOWORD(lParam) == WM_RBUTTONUP)
		{
			/* Right click - show context menu */
			if (backend && backend->menu_callback)
			{
				/* Windows requires SetForegroundWindow BEFORE showing popup menus */
				SetForegroundWindow(hwnd);
				
				/* Call menu callback with proper parameters
				 * widget=NULL (not AppIndicator), button=3 (right click), time=GDK_CURRENT_TIME
				 * The callback will create and popup the menu */
				backend->menu_callback(NULL, 3, GDK_CURRENT_TIME, backend->menu_userdata);
				
				/* Post a dummy message to ensure menu dismisses properly on Windows */
				PostMessage(hwnd, WM_NULL, 0, 0);
			}
		}
		return 0;
		
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

TrayBackend* tray_windows_new(GdkPixbuf *icon, const char *tooltip)
{
	TrayBackend *backend;
	WNDCLASSEXW wc;
	
	backend = g_new0(TrayBackend, 1);
	backend->visible = FALSE;
	
	/* Register window class for hidden window */
	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.lpfnWndProc = tray_wndproc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = L"PChatTrayWindow";
	RegisterClassExW(&wc);
	
	/* Create hidden window */
	backend->hwnd = CreateWindowExW(0, L"PChatTrayWindow", L"PChat Tray",
		0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);
	
	if (!backend->hwnd)
	{
		g_free(backend);
		return NULL;
	}
	
	/* Associate backend with window */
	SetWindowLongPtr(backend->hwnd, GWLP_USERDATA, (LONG_PTR)backend);
	
	/* Initialize NOTIFYICONDATA */
	ZeroMemory(&backend->nid, sizeof(NOTIFYICONDATAW));
	backend->nid.cbSize = sizeof(NOTIFYICONDATAW);
	backend->nid.hWnd = backend->hwnd;
	backend->nid.uID = TRAY_ID;
	backend->nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	backend->nid.uCallbackMessage = WM_TRAYICON;
	
	/* Set initial icon */
	if (icon)
	{
		backend->current_icon = g_object_ref(icon);
		backend->hicon = pixbuf_to_hicon(icon);
		backend->nid.hIcon = backend->hicon;
	}
	
	/* Set tooltip */
	if (tooltip)
	{
		MultiByteToWideChar(CP_UTF8, 0, tooltip, -1, backend->nid.szTip,
			sizeof(backend->nid.szTip) / sizeof(WCHAR));
	}
	
	return backend;
}

void tray_windows_set_icon(TrayBackend *backend, GdkPixbuf *icon)
{
	if (!backend || !icon)
		return;
	
	/* Clean up old icon */
	if (backend->hicon)
		DestroyIcon(backend->hicon);
	if (backend->current_icon)
		g_object_unref(backend->current_icon);
	
	/* Set new icon */
	backend->current_icon = g_object_ref(icon);
	backend->hicon = pixbuf_to_hicon(icon);
	backend->nid.hIcon = backend->hicon;
	
	if (backend->visible)
		Shell_NotifyIconW(NIM_MODIFY, &backend->nid);
}

void tray_windows_set_tooltip(TrayBackend *backend, const char *tooltip)
{
	if (!backend || !tooltip)
		return;
	
	MultiByteToWideChar(CP_UTF8, 0, tooltip, -1, backend->nid.szTip,
		sizeof(backend->nid.szTip) / sizeof(WCHAR));
	
	if (backend->visible)
		Shell_NotifyIconW(NIM_MODIFY, &backend->nid);
}

void tray_windows_set_visible(TrayBackend *backend, gboolean visible)
{
	if (!backend)
		return;
	
	if (visible && !backend->visible)
	{
		Shell_NotifyIconW(NIM_ADD, &backend->nid);
		backend->visible = TRUE;
		
		if (backend->embedded_callback)
			backend->embedded_callback(backend->embedded_userdata);
	}
	else if (!visible && backend->visible)
	{
		Shell_NotifyIconW(NIM_DELETE, &backend->nid);
		backend->visible = FALSE;
	}
}

gboolean tray_windows_is_embedded(TrayBackend *backend)
{
	if (!backend)
		return FALSE;
	
	return backend->visible;
}

void tray_windows_set_activate_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata)
{
	if (!backend)
		return;
	
	backend->activate_callback = callback;
	backend->activate_userdata = userdata;
}

void tray_windows_set_menu_callback(TrayBackend *backend, TrayMenuCallback callback, void *userdata)
{
	if (!backend)
		return;
	
	backend->menu_callback = callback;
	backend->menu_userdata = userdata;
}

void tray_windows_set_embedded_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata)
{
	if (!backend)
		return;
	
	backend->embedded_callback = callback;
	backend->embedded_userdata = userdata;
}

void tray_windows_destroy(TrayBackend *backend)
{
	if (!backend)
		return;
	
	/* Remove tray icon */
	if (backend->visible)
		Shell_NotifyIconW(NIM_DELETE, &backend->nid);
	
	/* Clean up icon */
	if (backend->hicon)
		DestroyIcon(backend->hicon);
	
	if (backend->current_icon)
		g_object_unref(backend->current_icon);
	
	/* Destroy window */
	if (backend->hwnd)
		DestroyWindow(backend->hwnd);
	
	g_free(backend);
}
