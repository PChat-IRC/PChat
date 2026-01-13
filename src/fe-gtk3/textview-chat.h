/* PChat - IRC Client using GtkTextView
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PCHAT_TEXTVIEW_CHAT_H
#define PCHAT_TEXTVIEW_CHAT_H

#include <gtk/gtk.h>
#include <time.h>

G_BEGIN_DECLS

/* Search flags for lastlog functionality */
typedef enum {
	PCHAT_SEARCH_CASE_MATCH = 1,
	PCHAT_SEARCH_BACKWARD = 2,
	PCHAT_SEARCH_HIGHLIGHT = 4,
	PCHAT_SEARCH_FOLLOW = 8,
	PCHAT_SEARCH_REGEXP = 16,
	PCHAT_SEARCH_VISIBLE_ONLY = 32
} PchatSearchFlags;

#define PCHAT_TYPE_TEXTVIEW_CHAT            (pchat_textview_chat_get_type())
#define PCHAT_TEXTVIEW_CHAT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PCHAT_TYPE_TEXTVIEW_CHAT, PchatTextViewChat))
#define PCHAT_TEXTVIEW_CHAT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PCHAT_TYPE_TEXTVIEW_CHAT, PchatTextViewChatClass))
#define PCHAT_IS_TEXTVIEW_CHAT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PCHAT_TYPE_TEXTVIEW_CHAT))
#define PCHAT_IS_TEXTVIEW_CHAT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PCHAT_TYPE_TEXTVIEW_CHAT))

typedef struct _PchatTextViewChat       PchatTextViewChat;
typedef struct _PchatTextViewChatClass  PchatTextViewChatClass;
typedef struct _PchatTextViewChatPrivate PchatTextViewChatPrivate;
typedef struct _PchatChatBuffer         PchatChatBuffer;

/* Chat buffer - similar to xtext_buffer */
struct _PchatChatBuffer
{
	GtkTextBuffer *buffer;
	GtkTextMark *end_mark;
	GtkTextMark *marker_mark;  /* Marker line position */
	gint line_count;
	gint indent;                /* Current auto-indent width */
	gboolean marker_seen;
	gboolean show_marker;
	gpointer user_data;         /* For application use */
	
	/* Search state for lastlog */
	GRegex *search_re;          /* Compiled regex */
	gchar *search_text;         /* Original search string */
	gchar *search_nee;          /* Casefolded search string */
	gint search_lnee;           /* Length of search_nee */
};

struct _PchatTextViewChat
{
	GtkTextView parent;
	PchatTextViewChatPrivate *priv;
};

struct _PchatTextViewChatClass
{
	GtkTextViewClass parent_class;
	
	/* Signals */
	void (*word_clicked) (PchatTextViewChat *chat, const gchar *word, GdkEventButton *event);
};

GType pchat_textview_chat_get_type (void) G_GNUC_CONST;

/* Constructor */
GtkWidget *pchat_textview_chat_new (void);

/* Buffer management */
PchatChatBuffer *pchat_chat_buffer_new (PchatTextViewChat *chat);
void pchat_chat_buffer_free (PchatChatBuffer *buf);
void pchat_chat_buffer_show (PchatTextViewChat *chat, PchatChatBuffer *buf);
PchatChatBuffer *pchat_textview_chat_get_buffer (PchatTextViewChat *chat);

/* Core text operations */

/* Buffer-specific operations */
void pchat_chat_buffer_append (PchatChatBuffer *buf, PchatTextViewChat *chat,
                                const gchar *text, gsize len);
void pchat_chat_buffer_append_indent (PchatChatBuffer *buf, PchatTextViewChat *chat,
                                       const gchar *left_text, gsize left_len,
                                       const gchar *right_text, gsize right_len,
                                       time_t stamp);
void pchat_chat_buffer_clear (PchatChatBuffer *buf, gint lines);

/* Marker line support */
void pchat_chat_buffer_set_marker (PchatChatBuffer *buf, PchatTextViewChat *chat);
void pchat_chat_buffer_reset_marker (PchatChatBuffer *buf);
gboolean pchat_chat_buffer_marker_seen (PchatChatBuffer *buf);
void pchat_textview_chat_append (PchatTextViewChat *chat, const gchar *text, gsize len);
void pchat_textview_chat_append_with_stamp (PchatTextViewChat *chat, PchatChatBuffer *buf,
                                             const gchar *text, gsize len, time_t stamp);
void pchat_textview_chat_append_indent (PchatTextViewChat *chat, PchatChatBuffer *buf,
                                         const gchar *left_text, gsize left_len,
                                         const gchar *right_text, gsize right_len,
                                         time_t stamp);
void pchat_textview_chat_clear (PchatTextViewChat *chat, gint lines);

/* Configuration */
void pchat_textview_chat_set_palette (PchatTextViewChat *chat, GdkRGBA palette[], gint palette_size);
void pchat_textview_chat_set_font (PchatTextViewChat *chat, const gchar *font_name);
void pchat_textview_chat_set_max_lines (PchatTextViewChat *chat, gint max_lines);
void pchat_textview_chat_set_max_auto_indent (PchatTextViewChat *chat, gint max_auto_indent);
void pchat_textview_chat_set_show_timestamps (PchatTextViewChat *chat, gboolean show);
void pchat_textview_chat_set_indent (PchatTextViewChat *chat, gboolean indent);
void pchat_textview_chat_set_auto_indent (PchatTextViewChat *chat, gboolean auto_indent);
void pchat_textview_chat_set_show_separator (PchatTextViewChat *chat, gboolean show);
void pchat_textview_chat_set_thin_separator (PchatTextViewChat *chat, gboolean thin);
void pchat_textview_chat_set_wordwrap (PchatTextViewChat *chat, gboolean wordwrap);

/* URL checking callback */
typedef gint (*PchatUrlCheckFunc) (GtkWidget *widget, gchar *url);
void pchat_textview_chat_set_urlcheck_function (PchatTextViewChat *chat, PchatUrlCheckFunc func);

/* Search */
gboolean pchat_textview_chat_search (PchatTextViewChat *chat, const gchar *text, 
                                      PchatSearchFlags flags, GError **error);
void pchat_textview_chat_clear_search_highlight (PchatTextViewChat *chat);

/* Save/export */
void pchat_textview_chat_save (PchatTextViewChat *chat, int fd);
void pchat_chat_buffer_save (PchatChatBuffer *buf, int fd);

/* Utility */
gboolean pchat_chat_buffer_is_empty (PchatChatBuffer *buf);
void pchat_textview_chat_foreach (PchatTextViewChat *chat,
                                   void (*func)(PchatTextViewChat *chat, const gchar *line, gpointer data),
                                   gpointer data);

/* Lastlog support - set search criteria and copy matching lines */
void pchat_chat_buffer_set_search_regex (PchatChatBuffer *buf, const gchar *pattern,
                                          gboolean case_sensitive, GError **error);
void pchat_chat_buffer_set_search_text (PchatChatBuffer *buf, const gchar *text,
                                         gboolean case_sensitive);
void pchat_chat_buffer_clear_search (PchatChatBuffer *buf);
gint pchat_chat_buffer_lastlog (PchatChatBuffer *output, PchatChatBuffer *search_area,
                                 PchatTextViewChat *chat);

G_END_DECLS

#endif /* PCHAT_TEXTVIEW_CHAT_H */
