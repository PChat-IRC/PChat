/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PCHAT_FE_WX_H
#define PCHAT_FE_WX_H

#include "config.h"

#define DISPLAY_NAME "PChat"

#include <glib.h>
#include <glib/gi18n.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for backend types */
struct session;
struct server;
struct User;
struct DCC;

/* Number of channel mode flag widgets */
#define NUM_FLAG_WIDS 8

/* Per-server GUI state */
struct server_gui
{
    void *rawlog_window;      /* RawLogDialog pointer */
    void *chanlist_window;    /* ChannelListDialog pointer */
    void *joind_window;       /* JoinDialog pointer */
};

/* Persistent state across tab switches */
typedef struct restore_gui
{
    void *banlist;              /* BanListDialog pointer */
    void *tab;                  /* channel pointer */
    void *user_model;           /* saved user list data */
    void *buffer;               /* saved chat buffer data */

    char *input_text;
    char *topic_text;
    char *key_text;
    char *limit_text;

    float old_ul_value;
    float lag_value;
    char *lag_text;
    char *lag_tip;
    float queue_value;
    char *queue_text;
    char *queue_tip;

    short flag_wid_state[NUM_FLAG_WIDS];
    unsigned int c_graph:1;
} restore_gui;

/* Per-session GUI state */
typedef struct session_gui
{
    void *main_window;          /* MainWindow pointer */
    void *textview;             /* chat text control pointer */
    void *user_tree;            /* userlist control pointer */
    void *topic_entry;          /* topic entry pointer */
    void *input_box;            /* input text control pointer */
    void *nick_label;           /* nick label pointer */
    void *chanview;             /* channel tree/tab pointer */
    void *tab_page;             /* notebook page for this session */

    int pane_left_size;
    int pane_right_size;

    guint16 is_tab;
    guint16 ul_hidden;
} session_gui;

#ifdef __cplusplus
}
#endif

#endif /* PCHAT_FE_WX_H */
