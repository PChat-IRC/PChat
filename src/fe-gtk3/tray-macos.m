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

#import <Cocoa/Cocoa.h>
#include "tray-macos.h"
#include <gdk/gdk.h>
#include <gtkosxapplication.h>

/* External reference to the GTK OSX Application */
extern GtkosxApplication *osx_app;

/* Forward declaration of our delegate class */
@interface PChatTrayDelegate : NSObject
{
	TrayClickCallback activateCallback;
	void *activateUserdata;
	TrayMenuCallback menuCallback;
	void *menuUserdata;
}
- (void)onStatusItemClicked:(id)sender;
- (void)onRightClick:(id)sender;
- (void)setActivateCallback:(TrayClickCallback)callback userdata:(void*)userdata;
- (void)setMenuCallback:(TrayMenuCallback)callback userdata:(void*)userdata;
@end

struct _TrayBackend
{
	NSStatusItem *statusItem;
	NSImage *currentImage;
	PChatTrayDelegate *delegate;
	GdkPixbuf *currentIcon;
	gchar *tooltip;
	gboolean visible;
	
	/* Callbacks */
	TrayClickCallback activateCallback;
	void *activateUserdata;
	TrayMenuCallback menuCallback;
	void *menuUserdata;
	TrayClickCallback embeddedCallback;
	void *embeddedUserdata;
};

/* Implementation of the delegate class */
@implementation PChatTrayDelegate

- (id)init
{
	self = [super init];
	if (self)
	{
		activateCallback = NULL;
		activateUserdata = NULL;
		menuCallback = NULL;
		menuUserdata = NULL;
	}
	return self;
}

- (void)setActivateCallback:(TrayClickCallback)callback userdata:(void*)userdata
{
	activateCallback = callback;
	activateUserdata = userdata;
}

- (void)setMenuCallback:(TrayMenuCallback)callback userdata:(void*)userdata
{
	menuCallback = callback;
	menuUserdata = userdata;
}

- (void)onStatusItemClicked:(id)sender
{
	NSEvent *event = [NSApp currentEvent];
	
	/* Check if this is a right-click or control-click */
	if (event.type == NSEventTypeRightMouseUp || 
	    (event.type == NSEventTypeLeftMouseUp && (event.modifierFlags & NSEventModifierFlagControl)))
	{
		[self onRightClick:sender];
	}
	else
	{
		/* Regular left click - activate callback */
		if (activateCallback)
			activateCallback(activateUserdata);
	}
}

- (void)onRightClick:(id)sender
{
	/* Right click - show context menu */
	if (menuCallback)
	{
		/* Call the menu callback which will create and show the GTK menu
		 * widget=NULL, button=3 (right-click), time=GDK_CURRENT_TIME */
		menuCallback(NULL, 3, GDK_CURRENT_TIME, menuUserdata);
	}
}

@end

/* Convert GdkPixbuf to NSImage */
static NSImage*
pixbuf_to_nsimage(GdkPixbuf *pixbuf)
{
	if (!pixbuf)
		return nil;
	
	int width = gdk_pixbuf_get_width(pixbuf);
	int height = gdk_pixbuf_get_height(pixbuf);
	int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	int channels = gdk_pixbuf_get_n_channels(pixbuf);
	guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
	
	/* Create bitmap representation */
	NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
		initWithBitmapDataPlanes:NULL
		pixelsWide:width
		pixelsHigh:height
		bitsPerSample:8
		samplesPerPixel:4
		hasAlpha:YES
		isPlanar:NO
		colorSpaceName:NSDeviceRGBColorSpace
		bytesPerRow:width * 4
		bitsPerPixel:32];
	
	if (!bitmap)
		return nil;
	
	/* Copy pixel data from GdkPixbuf to NSBitmapImageRep */
	guchar *dest = [bitmap bitmapData];
	for (int y = 0; y < height; y++)
	{
		guchar *src = pixels + y * rowstride;
		for (int x = 0; x < width; x++)
		{
			guchar r = src[0];
			guchar g = src[1];
			guchar b = src[2];
			guchar a = (channels == 4) ? src[3] : 0xFF;
			
			/* RGBA format for NSImage */
			*dest++ = r;
			*dest++ = g;
			*dest++ = b;
			*dest++ = a;
			
			src += channels;
		}
	}
	
	/* Create NSImage from bitmap */
	NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
	[image addRepresentation:bitmap];
	
	/* Scale to appropriate size for menu bar (typically 18-22 points) */
	[image setSize:NSMakeSize(20, 20)];
	
	[bitmap release];
	return image;
}

/* Structure for deferred status item creation */
typedef struct {
	TrayBackend *backend;
	GdkPixbuf *icon;
	char *tooltip;
	int attempts;
} DeferredTrayData;

static gboolean
create_status_item_deferred(gpointer user_data)
{
	DeferredTrayData *data = (DeferredTrayData *)user_data;
	FILE *logfile = fopen("/tmp/pchat-tray.log", "a");
	
	@autoreleasepool {
		/* Get NSApplication - GTK manages it, so just use sharedApplication */
		NSApplication *app = [NSApplication sharedApplication];
		
		if (logfile) {
			fprintf(logfile, "create_status_item_deferred: NSApp: %p\n", app);
			fprintf(logfile, "create_status_item_deferred: attempt %d, isRunning = %d\n",
				data->attempts, [app isRunning]);
			fflush(logfile);
		}
		
		/* Wait until NSApp is actually running */
		if (![app isRunning] && data->attempts < 10) {
			data->attempts++;
			if (logfile) {
				fprintf(logfile, "create_status_item_deferred: NSApp not running yet, will retry\n");
				fclose(logfile);
			}
			return G_SOURCE_CONTINUE; /* Try again */
		}
		
		if (logfile) {
			fprintf(logfile, "create_status_item_deferred: NSApp is running, creating status item\n");
			fprintf(logfile, "create_status_item_deferred: NSApp presentation options: %lu\n",
				(unsigned long)[app presentationOptions]);
			fprintf(logfile, "create_status_item_deferred: Status bar thickness: %f\n",
				[[NSStatusBar systemStatusBar] thickness]);
			fflush(logfile);
		}
		
		NSStatusBar *statusBar = [NSStatusBar systemStatusBar];
		data->backend->statusItem = [[statusBar statusItemWithLength:22.0] retain];
		
		/* Set autosave name - this helps macOS remember the position */
		[data->backend->statusItem setAutosaveName:@"org.pchat.statusitem"];
		
		if (logfile) {
			fprintf(logfile, "create_status_item_deferred: Status item created: %p\n", 
				data->backend->statusItem);
			fflush(logfile);
		}
		
		if (!data->backend->statusItem) {
			if (logfile) {
				fprintf(logfile, "create_status_item_deferred: ERROR - Failed to create status item!\n");
				fclose(logfile);
			}
			g_free(data);
			return G_SOURCE_REMOVE;
		}
		
		/* Create and set up delegate */
		data->backend->delegate = [[PChatTrayDelegate alloc] init];
		
		/* Set up button */
		NSStatusBarButton *button = [data->backend->statusItem button];
		if (logfile) {
			fprintf(logfile, "create_status_item_deferred: Button = %p\n", button);
			fflush(logfile);
		}
		
		if (button) {
			[button setTarget:data->backend->delegate];
			[button setAction:@selector(onStatusItemClicked:)];
			[button sendActionOn:NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp];
			
			/* Set a title as fallback in case image doesn't work */
			[button setTitle:@"P"];
			
			/* Set icon */
			if (data->icon) {
				data->backend->currentIcon = g_object_ref(data->icon);
				data->backend->currentImage = pixbuf_to_nsimage(data->icon);
				if (data->backend->currentImage) {
					[button setImage:data->backend->currentImage];
					[data->backend->currentImage setTemplate:YES];
					/* Clear title if image worked */
					[button setTitle:@""];
					if (logfile) {
						fprintf(logfile, "create_status_item_deferred: Icon set successfully\n");
						fflush(logfile);
					}
				} else {
					if (logfile) {
						fprintf(logfile, "create_status_item_deferred: Icon conversion failed, using title\n");
						fflush(logfile);
					}
				}
			}
			
			/* Set tooltip */
			if (data->tooltip) {
				data->backend->tooltip = g_strdup(data->tooltip);
				[button setToolTip:[NSString stringWithUTF8String:data->tooltip]];
			}
		}
		
		data->backend->visible = TRUE;
		[data->backend->statusItem setVisible:YES];
		
		if (logfile) {
			fprintf(logfile, "create_status_item_deferred: Tray icon created successfully, visible = %d\n",
				[data->backend->statusItem isVisible]);
			fclose(logfile);
		}
		
		/* Clean up */
		if (data->icon)
			g_object_unref(data->icon);
		g_free(data->tooltip);
		g_free(data);
	}
	
	return G_SOURCE_REMOVE;
}

TrayBackend* tray_macos_new(GdkPixbuf *icon, const char *tooltip)
{
	TrayBackend *backend;
	FILE *logfile = fopen("/tmp/pchat-tray.log", "a");
	
	g_print("tray_macos_new: Creating macOS tray icon\n");
	fprintf(stderr, "tray_macos_new: Creating macOS tray icon\n");
	if (logfile) {
		fprintf(logfile, "tray_macos_new: Creating macOS tray icon (deferred approach)\n");
		fflush(logfile);
	}
	
	backend = g_new0(TrayBackend, 1);
	backend->visible = FALSE;
	
	/* Defer the actual status item creation until NSApp is running */
	DeferredTrayData *data = g_new0(DeferredTrayData, 1);
	data->backend = backend;
	data->icon = icon ? g_object_ref(icon) : NULL;
	data->tooltip = tooltip ? g_strdup(tooltip) : NULL;
	data->attempts = 0;
	
	/* Check every 100ms until NSApp is running */
	g_timeout_add(100, create_status_item_deferred, data);
	
	if (logfile) {
		fprintf(logfile, "tray_macos_new: Deferred creation scheduled\n");
		fclose(logfile);
	}
	
	return backend;
}

void tray_macos_set_icon(TrayBackend *backend, GdkPixbuf *icon)
{
	if (!backend || !icon)
		return;
	
	@autoreleasepool {
		/* Clean up old icon */
		if (backend->currentIcon)
			g_object_unref(backend->currentIcon);
		if (backend->currentImage)
			[backend->currentImage release];
		
		/* Set new icon */
		backend->currentIcon = g_object_ref(icon);
		backend->currentImage = pixbuf_to_nsimage(icon);
		
		if (backend->currentImage)
		{
			NSStatusBarButton *button = [backend->statusItem button];
			if (button)
			{
				[button setImage:backend->currentImage];
				[backend->currentImage setTemplate:YES];
			}
		}
	}
}

void tray_macos_set_tooltip(TrayBackend *backend, const char *tooltip)
{
	if (!backend || !tooltip)
		return;
	
	@autoreleasepool {
		if (backend->tooltip)
			g_free(backend->tooltip);
		
		backend->tooltip = g_strdup(tooltip);
		
		NSStatusBarButton *button = [backend->statusItem button];
		if (button)
			[button setToolTip:[NSString stringWithUTF8String:tooltip]];
	}
}

void tray_macos_set_visible(TrayBackend *backend, gboolean visible)
{
	if (!backend)
		return;
	
	@autoreleasepool {
		backend->visible = visible;
		[backend->statusItem setVisible:visible ? YES : NO];
	}
}

gboolean tray_macos_is_embedded(TrayBackend *backend)
{
	if (!backend)
		return FALSE;
	
	/* Status items are always "embedded" on macOS when visible */
	return backend->visible;
}

void tray_macos_set_activate_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata)
{
	if (!backend)
		return;
	
	backend->activateCallback = callback;
	backend->activateUserdata = userdata;
	
	@autoreleasepool {
		[backend->delegate setActivateCallback:callback userdata:userdata];
	}
}

void tray_macos_set_menu_callback(TrayBackend *backend, TrayMenuCallback callback, void *userdata)
{
	if (!backend)
		return;
	
	backend->menuCallback = callback;
	backend->menuUserdata = userdata;
	
	@autoreleasepool {
		[backend->delegate setMenuCallback:callback userdata:userdata];
	}
}

void tray_macos_set_embedded_callback(TrayBackend *backend, TrayClickCallback callback, void *userdata)
{
	if (!backend)
		return;
	
	backend->embeddedCallback = callback;
	backend->embeddedUserdata = userdata;
	
	/* Status items don't have an "embedded" notification on macOS,
	 * but we can call it immediately since they're always embedded when created */
	if (callback)
		callback(userdata);
}

void tray_macos_destroy(TrayBackend *backend)
{
	if (!backend)
		return;
	
	@autoreleasepool {
		/* Remove status item */
		if (backend->statusItem)
		{
			[[NSStatusBar systemStatusBar] removeStatusItem:backend->statusItem];
			[backend->statusItem release];
		}
		
		/* Clean up delegate */
		if (backend->delegate)
			[backend->delegate release];
		
		/* Clean up image */
		if (backend->currentImage)
			[backend->currentImage release];
		
		/* Clean up pixbuf */
		if (backend->currentIcon)
			g_object_unref(backend->currentIcon);
		
		/* Clean up tooltip */
		if (backend->tooltip)
			g_free(backend->tooltip);
		
		g_free(backend);
	}
}
