/* PChat Audio Player Plugin
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PCHAT_AUDIOPLAYER_H
#define PCHAT_AUDIOPLAYER_H

#include <pthread.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct AudioPlayer AudioPlayer;
typedef struct PlaylistItem PlaylistItem;

/* Playlist item structure */
struct PlaylistItem {
    char *filepath;
    char *title;        /* Display title (from tags or filename) */
    char *artist;       /* Artist from metadata */
    char *album;        /* Album from metadata */
    char *genre;        /* Genre from metadata */
    char *year;         /* Year from metadata */
    int duration;
    PlaylistItem *next;
    PlaylistItem *prev;
};

/* Player states */
typedef enum {
    STATE_STOPPED,
    STATE_PLAYING,
    STATE_PAUSED
} PlayerState;

/* Audio player structure */
struct AudioPlayer {
    PlaylistItem *playlist_head;
    PlaylistItem *playlist_tail;
    PlaylistItem *current_track;
    
    PlayerState state;
    bool loop_playlist;
    bool shuffle;
    float volume;          /* Volume (0.0 to 1.0) */
    
    pthread_t playback_thread;
    pthread_mutex_t lock;

    void *decoder_ctx;     /* Per-format AudioDecoder pointer */
    void *faudio_device;   /* FAudio device */
};

/* Function declarations */
AudioPlayer* audioplayer_create(void);
void audioplayer_destroy(AudioPlayer *player);

/* Playback controls */
int audioplayer_play(AudioPlayer *player, const char *filepath);
int audioplayer_play_playlist_item(AudioPlayer *player, PlaylistItem *item);
int audioplayer_pause(AudioPlayer *player);
int audioplayer_resume(AudioPlayer *player);
int audioplayer_stop(AudioPlayer *player);
int audioplayer_next(AudioPlayer *player);
int audioplayer_prev(AudioPlayer *player);

/* Playlist management */
int audioplayer_add_to_playlist(AudioPlayer *player, const char *filepath);
int audioplayer_remove_from_playlist(AudioPlayer *player, int index);
int audioplayer_clear_playlist(AudioPlayer *player);
int audioplayer_load_playlist_file(AudioPlayer *player, const char *playlist_file);
PlaylistItem* audioplayer_get_playlist(AudioPlayer *player);
int audioplayer_get_playlist_count(AudioPlayer *player);

/* Status queries */
PlayerState audioplayer_get_state(AudioPlayer *player);
PlaylistItem* audioplayer_get_current_track(AudioPlayer *player);
int audioplayer_get_position(AudioPlayer *player);

/* Settings */
void audioplayer_set_loop(AudioPlayer *player, bool loop);
void audioplayer_set_shuffle(AudioPlayer *player, bool shuffle);
void audioplayer_set_volume(AudioPlayer *player, float volume);
float audioplayer_get_volume(AudioPlayer *player);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PCHAT_AUDIOPLAYER_H */
