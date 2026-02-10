/* PChat Audio Player Plugin - GUI Interface
 * Copyright (C) 2025
 */

#include <gtk/gtk.h>
#include <string.h>
#include "gui.h"
#include "audioplayer.h"

/* Undefine xchat macros to allow direct struct member access */
#undef pchat_printf
#undef pchat_print

/* GUI state */
static GtkWidget *main_window = NULL;
static GtkWidget *playlist_view = NULL;
static GtkWidget *play_button = NULL;
static GtkWidget *pause_button = NULL;
static GtkWidget *stop_button = NULL;
static GtkWidget *prev_button = NULL;
static GtkWidget *next_button = NULL;
static GtkWidget *now_playing_label = NULL;
static GtkWidget *volume_scale = NULL;
static GtkListStore *playlist_store = NULL;
static gboolean gtk_initialized = FALSE;

static pchat_plugin *ph_gui = NULL;
static AudioPlayer *player_gui = NULL;
static guint update_timer = 0;
static gboolean playlist_needs_update = TRUE;
static PlaylistItem *last_current_track = NULL;

enum {
    COL_TITLE,
    COL_FILEPATH,
    COL_POINTER,
    N_COLUMNS
};

/* Forward declarations */
static void update_playlist_view(void);
static void update_now_playing(void);
static gboolean update_timer_callback(gpointer data);

/* Button callbacks */
static void on_play_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    if (audioplayer_get_state(player_gui) == STATE_PAUSED) {
        audioplayer_resume(player_gui);
    } else {
        /* Play first track in playlist or current track */
        PlaylistItem *current = audioplayer_get_current_track(player_gui);
        if (current) {
            /* Replay current track */
            audioplayer_play_playlist_item(player_gui, current);
        } else {
            /* Play first track in playlist */
            PlaylistItem *first = audioplayer_get_playlist(player_gui);
            if (first) {
                audioplayer_play_playlist_item(player_gui, first);
            }
        }
        playlist_needs_update = TRUE;
    }
}

static void on_pause_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    audioplayer_pause(player_gui);
}

static void on_stop_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    audioplayer_stop(player_gui);
}

static void on_prev_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    if (audioplayer_prev(player_gui) == 0) {
        playlist_needs_update = TRUE;
    }
}

static void on_next_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    if (audioplayer_next(player_gui) == 0) {
        playlist_needs_update = TRUE;
    }
}

static void on_volume_changed(GtkRange *range, gpointer user_data) {
    (void)user_data;
    double value = gtk_range_get_value(range);
    audioplayer_set_volume(player_gui, (float)value / 100.0f);
}

static void on_add_file_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Add Audio File",
        GTK_WINDOW(main_window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    
    /* Add audio file filter */
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Audio Files");
    gtk_file_filter_add_pattern(filter, "*.mp3");
    gtk_file_filter_add_pattern(filter, "*.flac");
    gtk_file_filter_add_pattern(filter, "*.ogg");
    gtk_file_filter_add_pattern(filter, "*.wav");
    gtk_file_filter_add_pattern(filter, "*.m4a");
    gtk_file_filter_add_pattern(filter, "*.opus");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "All Files");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GSList *filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
        GSList *iter = filenames;
        
        while (iter) {
            char *filename = (char *)iter->data;
            audioplayer_add_to_playlist(player_gui, filename);
            g_free(filename);
            iter = iter->next;
        }
        
        g_slist_free(filenames);
        playlist_needs_update = TRUE;
    }
    
    gtk_widget_destroy(dialog);
}

static void on_load_playlist_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Load Playlist",
        GTK_WINDOW(main_window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    
    /* Add playlist file filter */
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Playlist Files");
    gtk_file_filter_add_pattern(filter, "*.m3u");
    gtk_file_filter_add_pattern(filter, "*.m3u8");
    gtk_file_filter_add_pattern(filter, "*.pls");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        int count = audioplayer_load_playlist_file(player_gui, filename);
        
        if (count > 0) {
            ph_gui->pchat_printf(ph_gui, "[AudioPlayer] Loaded %d tracks from playlist\n", count);
        } else if (count == 0) {
            ph_gui->pchat_printf(ph_gui, "[AudioPlayer] No valid tracks found in playlist\n");
        } else {
            ph_gui->pchat_printf(ph_gui, "[AudioPlayer] Failed to load playlist\n");
        }
        
        g_free(filename);
        playlist_needs_update = TRUE;
    }
    
    gtk_widget_destroy(dialog);
}

static void on_clear_playlist_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    audioplayer_clear_playlist(player_gui);
    playlist_needs_update = TRUE;
}

static void on_playlist_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                                     GtkTreeViewColumn *column, gpointer user_data) {
    (void)tree_view;
    (void)column;
    (void)user_data;
    
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(playlist_store), &iter, path)) {
        PlaylistItem *item = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(playlist_store), &iter, COL_POINTER, &item, -1);
        
        if (item && item->filepath) {
            /* Use the play_playlist_item function to properly set current_track */
            audioplayer_play_playlist_item(player_gui, item);
            playlist_needs_update = TRUE;
        }
    }
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    (void)user_data;
    
    if (update_timer) {
        g_source_remove(update_timer);
        update_timer = 0;
    }
    
    main_window = NULL;
}

static void update_playlist_view(void) {
    if (!playlist_store || !playlist_view) return;
    
    /* Save scroll position */
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(gtk_widget_get_parent(playlist_view)));
    gdouble scroll_pos = gtk_adjustment_get_value(vadj);
    
    gtk_list_store_clear(playlist_store);
    
    PlaylistItem *item = audioplayer_get_playlist(player_gui);
    PlaylistItem *current = audioplayer_get_current_track(player_gui);
    
    GtkTreeIter current_iter;
    gboolean has_current = FALSE;
    
    while (item) {
        GtkTreeIter iter;
        gtk_list_store_append(playlist_store, &iter);
        
        if (item == current) {
            current_iter = iter;
            has_current = TRUE;
        }
        
        /* Format: [▶] Artist - Title */
        gchar *display_title;
        if (item->artist && item->title) {
            display_title = g_strdup_printf("%s%s - %s",
                (item == current) ? "▶ " : "",
                item->artist,
                item->title);
        } else {
            display_title = g_strdup_printf("%s%s",
                (item == current) ? "▶ " : "",
                item->title);
        }
        
        gtk_list_store_set(playlist_store, &iter,
            COL_TITLE, display_title,
            COL_FILEPATH, item->filepath,
            COL_POINTER, item,
            -1);
        
        g_free(display_title);
        item = item->next;
    }
    
    /* Restore scroll position */
    gtk_adjustment_set_value(vadj, scroll_pos);
    
    /* Optionally highlight current track */
    if (has_current) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
        gtk_tree_selection_select_iter(selection, &current_iter);
    }
}

static void update_now_playing(void) {
    if (!now_playing_label) return;
    
    PlaylistItem *current = audioplayer_get_current_track(player_gui);
    PlayerState state = audioplayer_get_state(player_gui);
    
    if (current && state != STATE_STOPPED) {
        const char *state_str = "";
        switch (state) {
            case STATE_PLAYING: state_str = "Playing"; break;
            case STATE_PAUSED: state_str = "Paused"; break;
            default: state_str = "Stopped"; break;
        }
        
        gchar *text;
        if (current->artist && current->title) {
            /* Show "Artist - Title [Album]" if metadata available */
            if (current->album) {
                text = g_strdup_printf("<b>%s:</b> %s - %s <i>[%s]</i>",
                    state_str, current->artist, current->title, current->album);
            } else {
                text = g_strdup_printf("<b>%s:</b> %s - %s",
                    state_str, current->artist, current->title);
            }
        } else {
            text = g_strdup_printf("<b>%s:</b> %s", state_str, current->title);
        }
        
        gtk_label_set_markup(GTK_LABEL(now_playing_label), text);
        g_free(text);
    } else {
        gtk_label_set_text(GTK_LABEL(now_playing_label), "Not playing");
    }
}

static gboolean update_timer_callback(gpointer data) {
    (void)data;
    
    /* Always update now playing label */
    update_now_playing();
    
    /* Only update playlist view if current track changed or playlist was modified */
    PlaylistItem *current = audioplayer_get_current_track(player_gui);
    if (playlist_needs_update || current != last_current_track) {
        update_playlist_view();
        last_current_track = current;
        playlist_needs_update = FALSE;
    }
    
    return TRUE;  /* Continue timer */
}

void audioplayer_gui_init(pchat_plugin *ph, AudioPlayer *player) {
    ph_gui = ph;
    player_gui = player;
    
    /* Ensure GTK3 is initialized (may not be when running under wx frontend) */
    if (!gtk_initialized) {
        if (!gtk_init_check(NULL, NULL)) {
            ph->pchat_print(ph, "[AudioPlayer] Cannot open GUI: GTK3 initialization failed.\n");
            return;
        }
        gtk_initialized = TRUE;
    }
    
    if (main_window) {
        /* Window already exists, just show it */
        gtk_window_present(GTK_WINDOW(main_window));
        return;
    }
    
    /* Create main window */
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "Audio Player");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 600, 450);
    g_signal_connect(main_window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    /* Create headerbar */
    GtkWidget *headerbar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerbar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(headerbar), "Audio Player");
    gtk_window_set_titlebar(GTK_WINDOW(main_window), headerbar);
    
    /* Playback control buttons in headerbar */
    prev_button = gtk_button_new_from_icon_name("media-skip-backward-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(prev_button, "Previous");
    play_button = gtk_button_new_from_icon_name("media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(play_button, "Play");
    pause_button = gtk_button_new_from_icon_name("media-playback-pause-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(pause_button, "Pause");
    stop_button = gtk_button_new_from_icon_name("media-playback-stop-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(stop_button, "Stop");
    next_button = gtk_button_new_from_icon_name("media-skip-forward-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(next_button, "Next");
    
    /* Add buttons to headerbar - playback controls on the left */
    gtk_header_bar_pack_start(GTK_HEADER_BAR(headerbar), prev_button);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(headerbar), play_button);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(headerbar), pause_button);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(headerbar), stop_button);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(headerbar), next_button);
    
    g_signal_connect(prev_button, "clicked", G_CALLBACK(on_prev_clicked), NULL);
    g_signal_connect(play_button, "clicked", G_CALLBACK(on_play_clicked), NULL);
    g_signal_connect(pause_button, "clicked", G_CALLBACK(on_pause_clicked), NULL);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), NULL);
    g_signal_connect(next_button, "clicked", G_CALLBACK(on_next_clicked), NULL);
    
    /* Volume popover button on the right */
    GtkWidget *volume_button = gtk_menu_button_new();
    GtkWidget *volume_icon = gtk_image_new_from_icon_name("audio-volume-high-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(volume_button), volume_icon);
    gtk_widget_set_tooltip_text(volume_button, "Volume");
    
    /* Create popover with volume slider */
    GtkWidget *volume_popover = gtk_popover_new(volume_button);
    GtkWidget *volume_popover_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(volume_popover_box, 12);
    gtk_widget_set_margin_end(volume_popover_box, 12);
    gtk_widget_set_margin_top(volume_popover_box, 12);
    gtk_widget_set_margin_bottom(volume_popover_box, 12);
    gtk_container_add(GTK_CONTAINER(volume_popover), volume_popover_box);
    
    /* Volume slider in popover */
    volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0.0, 100.0, 1.0);
    gtk_scale_set_draw_value(GTK_SCALE(volume_scale), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(volume_scale), GTK_POS_BOTTOM);
    gtk_range_set_inverted(GTK_RANGE(volume_scale), TRUE);  /* Higher values at top */
    gtk_range_set_value(GTK_RANGE(volume_scale), audioplayer_get_volume(player_gui) * 100.0);
    gtk_widget_set_size_request(volume_scale, -1, 120);
    gtk_box_pack_start(GTK_BOX(volume_popover_box), volume_scale, TRUE, TRUE, 0);
    
    g_signal_connect(volume_scale, "value-changed", G_CALLBACK(on_volume_changed), NULL);
    
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(volume_button), volume_popover);
    gtk_widget_show_all(volume_popover_box);
    
    /* Playlist control buttons on the right */
    GtkWidget *add_file_btn = gtk_button_new_from_icon_name("list-add-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(add_file_btn, "Add Files");
    GtkWidget *load_playlist_btn = gtk_button_new_from_icon_name("document-open-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(load_playlist_btn, "Load Playlist");
    GtkWidget *clear_btn = gtk_button_new_from_icon_name("edit-clear-all-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(clear_btn, "Clear Playlist");
    
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerbar), clear_btn);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerbar), load_playlist_btn);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerbar), add_file_btn);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerbar), volume_button);
    
    g_signal_connect(add_file_btn, "clicked", G_CALLBACK(on_add_file_clicked), NULL);
    g_signal_connect(load_playlist_btn, "clicked", G_CALLBACK(on_load_playlist_clicked), NULL);
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_playlist_clicked), NULL);
    
    /* Create main vertical box */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);
    
    /* Now playing label with padding */
    now_playing_label = gtk_label_new("Not playing");
    gtk_label_set_xalign(GTK_LABEL(now_playing_label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(now_playing_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_start(now_playing_label, 10);
    gtk_widget_set_margin_end(now_playing_label, 10);
    gtk_widget_set_margin_top(now_playing_label, 5);
    gtk_widget_set_margin_bottom(now_playing_label, 5);
    gtk_box_pack_start(GTK_BOX(vbox), now_playing_label, FALSE, FALSE, 0);
    
    /* Playlist view - fills remaining space */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
    
    playlist_store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    playlist_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(playlist_store));
    gtk_container_add(GTK_CONTAINER(scrolled), playlist_view);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Playlist", renderer, "text", COL_TITLE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(playlist_view), column);
    
    g_signal_connect(playlist_view, "row-activated",
        G_CALLBACK(on_playlist_row_activated), NULL);
    
    /* Initial update */
    update_playlist_view();
    update_now_playing();
    
    /* Start update timer (every 500ms) */
    update_timer = g_timeout_add(500, update_timer_callback, NULL);
    
    /* Show window */
    gtk_widget_show_all(main_window);
}

void audioplayer_gui_update(void) {
    if (main_window) {
        update_now_playing();
        update_playlist_view();
    }
}

void audioplayer_gui_cleanup(void) {
    if (update_timer) {
        g_source_remove(update_timer);
        update_timer = 0;
    }
    
    if (main_window) {
        gtk_widget_destroy(main_window);
        main_window = NULL;
    }
}
