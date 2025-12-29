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

/* GTK-based update checker following Sparkle framework specifications
 * Parses Sparkle appcast XML format and checks for updates
 * Compatible with Windows and macOS builds
 * Automatically disabled in portable mode
 */

#include "config.h"

#ifdef HAVE_UPDATE_CHECKER

#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "fe-gtk.h"
#include "gtkutil.h"
#include "../common/pchat.h"
#include "../common/util.h"
#include "update-checker.h"

#define DEFAULT_APPCAST_URL "https://pchat.github.io/appcast.xml"
#define UPDATE_CHECK_TIMEOUT 30 /* seconds */

typedef struct {
    char *version;
    char *title;
    char *description;
    char *download_url;
    char *release_notes_url;
    gboolean critical;
} UpdateInfo;

static struct {
    char *appcast_url;
    gboolean automatic_checks;
    gboolean check_in_progress;
    SoupSession *session;
} update_state = {
    .appcast_url = NULL,
    .automatic_checks = FALSE,
    .check_in_progress = FALSE,
    .session = NULL
};

/* Forward declarations */
static void show_update_dialog(UpdateInfo *info, GtkWindow *parent, gboolean show_no_update);
static void free_update_info(UpdateInfo *info);
static gboolean parse_appcast(const char *xml_data, UpdateInfo **info);
static int compare_versions(const char *v1, const char *v2);

/* Initialize the update checker */
void
update_checker_init(void)
{
    if (!update_state.session) {
        update_state.session = soup_session_new();
        g_object_set(update_state.session,
                    "timeout", UPDATE_CHECK_TIMEOUT,
                    "user-agent", "PChat/" PACKAGE_VERSION,
                    NULL);
    }
    
    if (!update_state.appcast_url) {
        update_state.appcast_url = g_strdup(DEFAULT_APPCAST_URL);
    }
}

/* Cleanup resources */
void
update_checker_cleanup(void)
{
    if (update_state.session) {
        g_object_unref(update_state.session);
        update_state.session = NULL;
    }
    
    g_free(update_state.appcast_url);
    update_state.appcast_url = NULL;
}

/* Set the appcast URL */
void
update_checker_set_appcast_url(const char *url)
{
    g_free(update_state.appcast_url);
    update_state.appcast_url = g_strdup(url);
}

/* Enable/disable automatic checks */
void
update_checker_set_automatic_checks(gboolean enabled)
{
    update_state.automatic_checks = enabled;
}

/* Free update info structure */
static void
free_update_info(UpdateInfo *info)
{
    if (!info)
        return;
    
    g_free(info->version);
    g_free(info->title);
    g_free(info->description);
    g_free(info->download_url);
    g_free(info->release_notes_url);
    g_free(info);
}

/* Compare semantic versions (e.g., "2.16.0" vs "2.15.9")
 * Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
static int
compare_versions(const char *v1, const char *v2)
{
    int v1_major = 0, v1_minor = 0, v1_patch = 0;
    int v2_major = 0, v2_minor = 0, v2_patch = 0;
    
    sscanf(v1, "%d.%d.%d", &v1_major, &v1_minor, &v1_patch);
    sscanf(v2, "%d.%d.%d", &v2_major, &v2_minor, &v2_patch);
    
    if (v1_major != v2_major)
        return v1_major > v2_major ? 1 : -1;
    if (v1_minor != v2_minor)
        return v1_minor > v2_minor ? 1 : -1;
    if (v1_patch != v2_patch)
        return v1_patch > v2_patch ? 1 : -1;
    
    return 0;
}

/* Get current platform identifier for appcast filtering */
static const char*
get_platform_identifier(void)
{
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

/* Parse Sparkle appcast XML format
 * Sparkle format example:
 * <rss version="2.0" xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
 *   <channel>
 *     <item>
 *       <title>Version 2.16.0</title>
 *       <sparkle:version>2.16.0</sparkle:version>
 *       <sparkle:releaseNotesLink>https://example.com/notes.html</sparkle:releaseNotesLink>
 *       <description><![CDATA[Release notes here]]></description>
 *       <enclosure url="https://example.com/PChat-2.16.0.dmg" 
 *                  sparkle:version="2.16.0"
 *                  sparkle:os="macos"
 *                  type="application/octet-stream"/>
 *       <enclosure url="https://example.com/PChat-2.16.0.exe" 
 *                  sparkle:version="2.16.0"
 *                  sparkle:os="windows"
 *                  type="application/octet-stream"/>
 *     </item>
 *   </channel>
 * </rss>
 */
static gboolean
parse_appcast(const char *xml_data, UpdateInfo **info_out)
{
    xmlDocPtr doc;
    xmlNodePtr root, channel, item;
    xmlNsPtr ns;
    UpdateInfo *info = NULL;
    gboolean found_item = FALSE;
    
    *info_out = NULL;
    
    /* Parse XML */
    doc = xmlReadMemory(xml_data, strlen(xml_data), "appcast.xml", NULL, 0);
    if (!doc) {
        return FALSE;
    }
    
    root = xmlDocGetRootElement(doc);
    if (!root || xmlStrcmp(root->name, BAD_CAST "rss")) {
        xmlFreeDoc(doc);
        return FALSE;
    }
    
    /* Find channel */
    for (channel = root->children; channel; channel = channel->next) {
        if (channel->type == XML_ELEMENT_NODE && 
            !xmlStrcmp(channel->name, BAD_CAST "channel")) {
            break;
        }
    }
    
    if (!channel) {
        xmlFreeDoc(doc);
        return FALSE;
    }
    Get current platform for filtering enclosures */
    const char *current_platform = get_platform_identifier();
    
    /* Parse item children */
    for (xmlNodePtr node = item->children; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE)
            continue;
        
        if (!xmlStrcmp(node->name, BAD_CAST "title")) {
            xmlChar *content = xmlNodeGetContent(node);
            info->title = g_strdup((char *)content);
            xmlFree(content);
        }
        else if (!xmlStrcmp(node->name, BAD_CAST "description")) {
            xmlChar *content = xmlNodeGetContent(node);
            info->description = g_strdup((char *)content);
            xmlFree(content);
        }
        else if (!xmlStrcmp(node->name, BAD_CAST "enclosure")) {
            /* Check if this enclosure is for our platform */
            xmlChar *os_attr = xmlGetProp(node, BAD_CAST "sparkle:os");
            gboolean platform_matches = FALSE;
            
            if (os_attr) {
                /* Platform specified - check if it matches */
                platform_matches = (g_ascii_strcasecmp((char *)os_attr, current_platform) == 0);
                xmlFree(os_attr);
            } else {
                /* No platform specified - use as fallback if we don't have a URL yet */
                platform_matches = (info->download_url == NULL);
            }
            
            if (platform_matches) {
                xmlChar *url = xmlGetProp(node, BAD_CAST "url");
                if (url) {
                    g_free(info->download_url);  /* Replace if we had a fallback */
                    info->download_url = g_strdup((char *)url);
                    xmlFree(url);
                }
                
                /* Check for sparkle:version attribute */
                xmlChar *version = xmlGetProp(node, BAD_CAST "sparkle:version");
                if (!version) {
                    version = xmlGetProp(node, BAD_CAST "version");
                }
                if (version && !info->version) {
                    info->version = g_strdup((char *)version);
                    xmlFree(version);
                }lNodeGetContent(node);
            info->description = g_strdup((char *)content);
            xmlFree(content);
        }
        else if (!xmlStrcmp(node->name, BAD_CAST "enclosure")) {
            xmlChar *url = xmlGetProp(node, BAD_CAST "url");
            if (url) {
                info->download_url = g_strdup((char *)url);
                xmlFree(url);
            }
            
            /* Check for sparkle:version attribute */
            xmlChar *version = xmlGetProp(node, BAD_CAST "sparkle:version");
            if (!version) {
                version = xmlGetProp(node, BAD_CAST "version");
            }
            if (version && !info->version) {
                info->version = g_strdup((char *)version);
                xmlFree(version);
            }
        }
        /* Check for sparkle namespace elements */
        else if (node->ns && node->ns->href && 
                 strstr((char *)node->ns->href, "sparkle")) {
            if (!xmlStrcmp(node->name, BAD_CAST "version")) {
                xmlChar *content = xmlNodeGetContent(node);
                if (!info->version) {
                    info->version = g_strdup((char *)content);
                }
                xmlFree(content);
            }
            else if (!xmlStrcmp(node->name, BAD_CAST "releaseNotesLink")) {
                xmlChar *content = xmlNodeGetContent(node);
                info->release_notes_url = g_strdup((char *)content);
                xmlFree(content);
            }
        }
    }
    
    xmlFreeDoc(doc);
    
    /* Validate we have minimum required info */
    if (!info->version || !info->download_url) {
        free_update_info(info);
        return FALSE;
    }
    
    *info_out = info;
    return TRUE;
}

/* Show update available dialog */
static void
show_update_dialog(UpdateInfo *info, GtkWindow *parent, gboolean show_no_update)
{
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *vbox;
    GtkWidget *label;
    char *markup;
    gint result;
    
    if (!info && !show_no_update) {
        return;
    }
    
    if (!info) {
        /* No update available */
        dialog = gtk_message_dialog_new(parent,
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_INFO,
                                       GTK_BUTTONS_OK,
                                       "You're up to date!");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                 "PChat %s is currently the newest version available.",
                                                 PACKAGE_VERSION);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    /* Check if this is actually a newer version */
    if (compare_versions(info->version, PACKAGE_VERSION) <= 0) {
        if (show_no_update) {
            dialog = gtk_message_dialog_new(parent,
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_INFO,
                                           GTK_BUTTONS_OK,
                                           "You're up to date!");
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                     "PChat %s is currently the newest version available.",
                                                     PACKAGE_VERSION);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
        return;
    }
    
    /* Create update available dialog */
    dialog = gtk_dialog_new_with_buttons("Update Available",
                                        parent,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "_Skip This Version", GTK_RESPONSE_REJECT,
                                        "_Remind Me Later", GTK_RESPONSE_CANCEL,
                                        "_Download Update", GTK_RESPONSE_ACCEPT,
                                        NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 450, -1);
    
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(content_area), vbox);
    
    /* Title */
    markup = g_markup_printf_escaped("<span size='large' weight='bold'>A new version of PChat is available!</span>\n\n"
                                    "PChat %s is now available (you have %s).",
                                    info->version, PACKAGE_VERSION);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    g_free(markup);
    
    /* Description/Release notes */
    if (info->description && strlen(info->description) > 0) {
        GtkWidget *scrolled;
        GtkWidget *textview;
        GtkTextBuffer *buffer;
        
        scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                           GTK_SHADOW_IN);
        gtk_widget_set_size_request(scrolled, -1, 150);
        
        textview = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
        gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 6);
        gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textview), 6);
        
        buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
        gtk_text_buffer_set_text(buffer, info->description, -1);
        
        gtk_container_add(GTK_CONTAINER(scrolled), textview);
        gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
    }
    
    gtk_widget_show_all(vbox);
    
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (result == GTK_RESPONSE_ACCEPT) {
        /* Open download URL in browser */
        fe_open_url(info->download_url);
    }
    
    gtk_widget_destroy(dialog);
}

/* Callback for soup message completion */
static void
update_check_callback(GObject *source, GAsyncResult *res, gpointer user_data)
{
    GtkWindow *parent = user_data;
    gboolean show_ui = GPOINTER_TO_INT(g_object_get_data(source, "show-ui"));
    GInputStream *stream;
    GError *error = NULL;
    GBytes *bytes;
    UpdateInfo *info = NULL;
    
    update_state.check_in_progress = FALSE;
    
    stream = soup_session_send_finish(SOUP_SESSION(source), res, &error);
    
    if (!stream) {
        if (show_ui && error) {
            GtkWidget *dialog = gtk_message_dialog_new(parent,
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      GTK_MESSAGE_ERROR,
                                                      GTK_BUTTONS_OK,
                                                      "Update check failed");
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                     "%s", error->message);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
        g_clear_error(&error);
        return;
    }
    
    /* Read response */
    bytes = g_input_stream_read_bytes(stream, 65536, NULL, &error);
    g_object_unref(stream);
    
    if (!bytes) {
        if (show_ui && error) {
            GtkWidget *dialog = gtk_message_dialog_new(parent,
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      GTK_MESSAGE_ERROR,
                                                      GTK_BUTTONS_OK,
                                                      "Update check failed");
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                     "Failed to read response: %s", error->message);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
        g_clear_error(&error);
        return;
    }
    
    /* Parse appcast */
    const char *data = g_bytes_get_data(bytes, NULL);
    if (parse_appcast(data, &info)) {
        show_update_dialog(info, parent, show_ui);
        free_update_info(info);
    } else if (show_ui) {
        GtkWidget *dialog = gtk_message_dialog_new(parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_ERROR,
                                                  GTK_BUTTONS_OK,
                                                  "Update check failed");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                 "Failed to parse appcast XML.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    
    g_bytes_unref(bytes);
}

/* Perform update check */
static void
do_update_check(GtkWindow *parent, gboolean show_ui)
{
    SoupMessage *msg;
    
    if (update_state.check_in_progress) {
        return;
    }
    
    if (!update_state.appcast_url) {
        if (show_ui) {
            GtkWidget *dialog = gtk_message_dialog_new(parent,
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      GTK_MESSAGE_ERROR,
                                                      GTK_BUTTONS_OK,
                                                      "Update check failed");
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                     "No appcast URL configured.");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
        return;
    }
    
    update_state.check_in_progress = TRUE;
    
    msg = soup_message_new("GET", update_state.appcast_url);
    g_object_set_data(G_OBJECT(update_state.session), "show-ui", GINT_TO_POINTER(show_ui));
    
    soup_session_send_async(update_state.session, msg, G_PRIORITY_DEFAULT, NULL,
                           update_check_callback, parent);
    
    g_object_unref(msg);
}

/* Check for updates with UI */
void
update_checker_check_with_ui(GtkWindow *parent)
{
    do_update_check(parent, TRUE);
}

/* Check for updates silently */
void
update_checker_check_silently(void)
{
    do_update_check(NULL, FALSE);
}

#endif /* HAVE_UPDATE_CHECKER */
