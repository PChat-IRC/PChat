/* PChat Audio Player Plugin - Core Implementation
 * Copyright (C) 2025
 *
 * Audio decoders:
 *   .mp3            -> mpg123
 *   .flac           -> libFLAC
 *   .ogg / .oga     -> libvorbis (vorbisfile)
 *   .m4a / .mp4     -> alac-decoder (David Hammerton's reverse-engineered
 *                      ALAC decoder, with its bundled QuickTime/MP4 demuxer)
 *
 * Audio output:
 *   FAudio source voice configured per-track at the file's native sample
 *   rate / channel count, with interleaved 16-bit signed PCM samples.
 */

#include "audioplayer.h"
#include "playlist_parser.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <strings.h>

#include <mpg123.h>

#include <FLAC/stream_decoder.h>
#include <FLAC/metadata.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <alac_decoder/decomp.h>
#include <alac_decoder/demux.h>
#include <alac_decoder/stream.h>

#include <FAudio.h>

/* The alac-decoder library uses this global to know whether the host CPU
 * is big-endian. It's normally defined in main.c, which the vcpkg port
 * does not build into the static library, so we define it here. */
int host_bigendian = 0;

static void alac_decoder_detect_endianness(void) {
    uint32_t v = 0x000000aa;
    unsigned char *p = (unsigned char *)&v;
    host_bigendian = (p[0] == 0xaa) ? 0 : 1;
}

/* mpg123_init() must be called once per process before any other
 * mpg123 call. Guard with a flag so repeated audioplayer_create()
 * calls don't reinitialize. */
static int g_mpg123_initialized = 0;

/* ---------------------------------------------------------------------- *
 *  Per-format decoder abstraction
 * ---------------------------------------------------------------------- */

typedef struct AudioDecoder AudioDecoder;

/* decode_next:
 *   Decodes the next chunk of samples. On success, *out_pcm points to
 *   an internal buffer of interleaved 16-bit signed PCM samples and
 *   *out_samples_per_channel is set to the number of frames decoded.
 *   The buffer is valid until the next call to decode_next or close.
 *   Returns 1 on data, 0 on EOF, -1 on error. */
typedef int  (*decode_next_fn)(AudioDecoder *d, const int16_t **out_pcm,
                               int *out_samples_per_channel);
typedef void (*decoder_close_fn)(AudioDecoder *d);

struct AudioDecoder {
    unsigned int sample_rate;
    unsigned int channels;
    decode_next_fn   decode_next;
    decoder_close_fn close;
    void *state;
};

/* ---------------------------------------------------------------------- *
 *  mpg123 (MP3)
 * ---------------------------------------------------------------------- */

typedef struct {
    mpg123_handle *mh;
    unsigned char *buf;
    size_t buf_size;
} Mp3State;

static int mp3_decode_next(AudioDecoder *d, const int16_t **out_pcm, int *out_frames) {
    Mp3State *s = (Mp3State *)d->state;
    size_t done = 0;
    int r = mpg123_read(s->mh, s->buf, s->buf_size, &done);
    if (done > 0) {
        *out_pcm = (const int16_t *)s->buf;
        *out_frames = (int)(done / (sizeof(int16_t) * d->channels));
        return 1;
    }
    if (r == MPG123_DONE) return 0;
    if (r == MPG123_OK || r == MPG123_NEW_FORMAT) return 1; /* try again */
    return -1;
}

static void mp3_close(AudioDecoder *d) {
    Mp3State *s = (Mp3State *)d->state;
    if (s) {
        if (s->mh) {
            mpg123_close(s->mh);
            mpg123_delete(s->mh);
        }
        free(s->buf);
        free(s);
    }
    free(d);
}

static AudioDecoder *open_mp3(const char *path) {
    if (!g_mpg123_initialized) {
        if (mpg123_init() != MPG123_OK) return NULL;
        g_mpg123_initialized = 1;
    }

    int err = MPG123_OK;
    mpg123_handle *mh = mpg123_new(NULL, &err);
    if (!mh) return NULL;

    if (mpg123_open(mh, path) != MPG123_OK) {
        mpg123_delete(mh);
        return NULL;
    }

    long rate = 0;
    int channels = 0, encoding = 0;
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
        mpg123_close(mh);
        mpg123_delete(mh);
        return NULL;
    }
    /* Force signed 16-bit output at the file's native rate/channels. */
    mpg123_format_none(mh);
    mpg123_format(mh, rate, channels, MPG123_ENC_SIGNED_16);

    Mp3State *s = calloc(1, sizeof(*s));
    AudioDecoder *d = calloc(1, sizeof(*d));
    if (!s || !d) {
        free(s); free(d);
        mpg123_close(mh); mpg123_delete(mh);
        return NULL;
    }
    s->mh = mh;
    s->buf_size = (size_t)mpg123_outblock(mh);
    if (s->buf_size < 4096) s->buf_size = 4096;
    s->buf = malloc(s->buf_size);
    if (!s->buf) {
        free(s); free(d);
        mpg123_close(mh); mpg123_delete(mh);
        return NULL;
    }

    d->sample_rate = (unsigned int)rate;
    d->channels    = (unsigned int)channels;
    d->state       = s;
    d->decode_next = mp3_decode_next;
    d->close       = mp3_close;
    return d;
}

/* ---------------------------------------------------------------------- *
 *  libFLAC
 * ---------------------------------------------------------------------- */

typedef struct {
    FLAC__StreamDecoder *dec;
    int16_t *pcm;          /* interleaved 16-bit output buffer */
    int pcm_capacity;      /* in samples (per channel) */
    int pcm_frames;        /* frames available after last process step */
    unsigned bps;
    int got_info;
    int eof;
    int error;
} FlacState;

static FLAC__StreamDecoderWriteStatus flac_write_cb(
    const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[], void *client_data)
{
    (void)decoder;
    AudioDecoder *d = (AudioDecoder *)client_data;
    FlacState *s = (FlacState *)d->state;

    int frames   = (int)frame->header.blocksize;
    int channels = (int)frame->header.channels;
    unsigned bps = frame->header.bits_per_sample;

    if (frames > s->pcm_capacity) {
        int16_t *nbuf = realloc(s->pcm, (size_t)frames * channels * sizeof(int16_t));
        if (!nbuf) {
            s->error = 1;
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
        s->pcm = nbuf;
        s->pcm_capacity = frames;
    }

    /* Convert planar Nbit samples to interleaved 16-bit. */
    if (bps == 16) {
        for (int i = 0; i < frames; i++) {
            for (int c = 0; c < channels; c++) {
                s->pcm[i * channels + c] = (int16_t)buffer[c][i];
            }
        }
    } else if (bps == 24) {
        for (int i = 0; i < frames; i++) {
            for (int c = 0; c < channels; c++) {
                s->pcm[i * channels + c] = (int16_t)(buffer[c][i] >> 8);
            }
        }
    } else if (bps == 8) {
        for (int i = 0; i < frames; i++) {
            for (int c = 0; c < channels; c++) {
                s->pcm[i * channels + c] = (int16_t)(buffer[c][i] << 8);
            }
        }
    } else {
        /* Generic fallback: scale to 16-bit. */
        int shift = (int)bps - 16;
        for (int i = 0; i < frames; i++) {
            for (int c = 0; c < channels; c++) {
                FLAC__int32 v = buffer[c][i];
                if (shift > 0) v >>= shift;
                else if (shift < 0) v <<= -shift;
                if (v > 32767) v = 32767;
                else if (v < -32768) v = -32768;
                s->pcm[i * channels + c] = (int16_t)v;
            }
        }
    }

    s->pcm_frames = frames;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flac_metadata_cb(const FLAC__StreamDecoder *decoder,
                             const FLAC__StreamMetadata *metadata,
                             void *client_data)
{
    (void)decoder;
    AudioDecoder *d = (AudioDecoder *)client_data;
    FlacState *s = (FlacState *)d->state;

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        d->sample_rate = metadata->data.stream_info.sample_rate;
        d->channels    = metadata->data.stream_info.channels;
        s->bps         = metadata->data.stream_info.bits_per_sample;
        s->got_info    = 1;
    }
}

static void flac_error_cb(const FLAC__StreamDecoder *decoder,
                          FLAC__StreamDecoderErrorStatus status,
                          void *client_data)
{
    (void)decoder; (void)status;
    AudioDecoder *d = (AudioDecoder *)client_data;
    FlacState *s = (FlacState *)d->state;
    s->error = 1;
}

static int flac_decode_next(AudioDecoder *d, const int16_t **out_pcm, int *out_frames) {
    FlacState *s = (FlacState *)d->state;
    if (s->eof) return 0;

    s->pcm_frames = 0;
    if (!FLAC__stream_decoder_process_single(s->dec)) {
        return -1;
    }
    FLAC__StreamDecoderState st = FLAC__stream_decoder_get_state(s->dec);
    if (st == FLAC__STREAM_DECODER_END_OF_STREAM) {
        s->eof = 1;
        if (s->pcm_frames == 0) return 0;
    }
    if (s->error) return -1;

    if (s->pcm_frames > 0) {
        *out_pcm = s->pcm;
        *out_frames = s->pcm_frames;
        return 1;
    }
    /* Metadata block or similar: signal "try again". */
    *out_pcm = s->pcm;
    *out_frames = 0;
    return 1;
}

static void flac_close(AudioDecoder *d) {
    FlacState *s = (FlacState *)d->state;
    if (s) {
        if (s->dec) {
            FLAC__stream_decoder_finish(s->dec);
            FLAC__stream_decoder_delete(s->dec);
        }
        free(s->pcm);
        free(s);
    }
    free(d);
}

static AudioDecoder *open_flac(const char *path) {
    AudioDecoder *d = calloc(1, sizeof(*d));
    FlacState   *s = calloc(1, sizeof(*s));
    if (!d || !s) { free(d); free(s); return NULL; }
    d->state = s;
    d->decode_next = flac_decode_next;
    d->close = flac_close;

    s->dec = FLAC__stream_decoder_new();
    if (!s->dec) { flac_close(d); return NULL; }

    FLAC__StreamDecoderInitStatus is =
        FLAC__stream_decoder_init_file(s->dec, path,
                                       flac_write_cb,
                                       flac_metadata_cb,
                                       flac_error_cb,
                                       d);
    if (is != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        flac_close(d);
        return NULL;
    }

    /* Pump until STREAMINFO is delivered so we know rate/channels. */
    if (!FLAC__stream_decoder_process_until_end_of_metadata(s->dec) || !s->got_info) {
        flac_close(d);
        return NULL;
    }
    return d;
}

/* ---------------------------------------------------------------------- *
 *  libvorbis (Ogg Vorbis)
 * ---------------------------------------------------------------------- */

typedef struct {
    OggVorbis_File vf;
    int16_t *buf;
    size_t buf_bytes;
} VorbisState;

static int vorbis_decode_next(AudioDecoder *d, const int16_t **out_pcm, int *out_frames) {
    VorbisState *s = (VorbisState *)d->state;
    int section = 0;
    long n = ov_read(&s->vf, (char *)s->buf, (int)s->buf_bytes,
                     0 /* little-endian */, 2 /* 16-bit */, 1 /* signed */,
                     &section);
    if (n == 0) return 0;
    if (n < 0)  return -1;
    *out_pcm = s->buf;
    *out_frames = (int)(n / (long)(sizeof(int16_t) * d->channels));
    return 1;
}

static void vorbis_close(AudioDecoder *d) {
    VorbisState *s = (VorbisState *)d->state;
    if (s) {
        ov_clear(&s->vf);
        free(s->buf);
        free(s);
    }
    free(d);
}

static AudioDecoder *open_vorbis(const char *path) {
    AudioDecoder *d = calloc(1, sizeof(*d));
    VorbisState *s = calloc(1, sizeof(*s));
    if (!d || !s) { free(d); free(s); return NULL; }

    if (ov_fopen(path, &s->vf) != 0) {
        free(d); free(s);
        return NULL;
    }
    vorbis_info *vi = ov_info(&s->vf, -1);
    if (!vi) {
        ov_clear(&s->vf);
        free(d); free(s);
        return NULL;
    }

    s->buf_bytes = 16384;
    s->buf = malloc(s->buf_bytes);
    if (!s->buf) {
        ov_clear(&s->vf);
        free(d); free(s);
        return NULL;
    }

    d->sample_rate = (unsigned int)vi->rate;
    d->channels    = (unsigned int)vi->channels;
    d->state       = s;
    d->decode_next = vorbis_decode_next;
    d->close       = vorbis_close;
    return d;
}

/* ---------------------------------------------------------------------- *
 *  alac-decoder (ALAC inside MP4/M4A)
 * ---------------------------------------------------------------------- */

typedef struct {
    FILE *fp;
    stream_t *stream;
    demux_res_t demux;
    alac_file *alac;
    uint32_t sample_index;     /* next sample (= ALAC frame) to read */
    void *in_buf;
    size_t in_buf_size;
    void *out_buf;
    size_t out_buf_size;
    int bytes_per_frame;       /* sample_size/8 * channels */
    uint16_t sample_size;
} AlacState;

static int alac_decode_next(AudioDecoder *d, const int16_t **out_pcm, int *out_frames) {
    AlacState *s = (AlacState *)d->state;
    if (s->sample_index >= s->demux.num_sample_byte_sizes) return 0;

    uint32_t byte_size = s->demux.sample_byte_size[s->sample_index];
    if (byte_size > s->in_buf_size) return -1;

    stream_read(s->stream, byte_size, s->in_buf);

    int out_bytes = (int)s->out_buf_size;
    decode_frame(s->alac, s->in_buf, s->out_buf, &out_bytes);

    s->sample_index++;

    if (out_bytes <= 0) {
        *out_pcm = NULL;
        *out_frames = 0;
        return 1; /* keep going */
    }

    /* alac-decoder emits sample_size-bit native-endian samples. */
    if (s->sample_size == 16) {
        *out_pcm = (const int16_t *)s->out_buf;
        *out_frames = out_bytes / s->bytes_per_frame;
    } else {
        /* Down-shift wider samples (24/32-bit ALAC) to 16-bit in place. */
        int channels = (int)d->channels;
        int total_samples = out_bytes / ((int)s->sample_size / 8);
        int16_t *dst = (int16_t *)s->out_buf;
        if (s->sample_size == 24) {
            /* Assume 24-bit packed as int32 in alac output. The reference
             * decoder writes machine-native ints; treat as int32. */
            int32_t *src = (int32_t *)s->out_buf;
            for (int i = 0; i < total_samples; i++) {
                int32_t v = src[i] >> 8;
                if (v > 32767) v = 32767;
                else if (v < -32768) v = -32768;
                dst[i] = (int16_t)v;
            }
        } else {
            /* Unknown bit depth: zero-fill rather than emit noise. */
            memset(dst, 0, total_samples * sizeof(int16_t));
        }
        *out_pcm = dst;
        *out_frames = total_samples / channels;
    }
    return 1;
}

static void alac_close(AudioDecoder *d) {
    AlacState *s = (AlacState *)d->state;
    if (s) {
        if (s->stream) stream_destroy(s->stream);
        if (s->fp)     fclose(s->fp);
        /* The alac-decoder library doesn't expose a "destroy_alac"; the
         * struct is allocated via plain malloc inside create_alac.
         * Looking at upstream source it's safe to free it here. */
        if (s->alac)   free(s->alac);
        free(s->demux.time_to_sample);
        free(s->demux.sample_byte_size);
        free(s->demux.codecdata);
        free(s->in_buf);
        free(s->out_buf);
        free(s);
    }
    free(d);
}

static AudioDecoder *open_alac(const char *path) {
    AudioDecoder *d = calloc(1, sizeof(*d));
    AlacState   *s = calloc(1, sizeof(*s));
    if (!d || !s) { free(d); free(s); return NULL; }
    d->state = s;
    d->decode_next = alac_decode_next;
    d->close = alac_close;

    s->fp = fopen(path, "rb");
    if (!s->fp) { alac_close(d); return NULL; }

    s->stream = stream_create_file(s->fp, 1 /* big-endian for MP4 atoms */);
    if (!s->stream) { alac_close(d); return NULL; }

    if (!qtmovie_read(s->stream, &s->demux) || !s->demux.format_read) {
        alac_close(d);
        return NULL;
    }
    if (s->demux.format != MAKEFOURCC('a','l','a','c')) {
        alac_close(d);
        return NULL;
    }

    s->alac = create_alac(s->demux.sample_size, s->demux.num_channels);
    if (!s->alac) { alac_close(d); return NULL; }
    alac_set_info(s->alac, s->demux.codecdata);

    s->sample_size     = s->demux.sample_size;
    s->bytes_per_frame = (s->demux.sample_size / 8) * s->demux.num_channels;

    /* Buffer sizes mirror those used by the reference main.c. */
    s->in_buf_size  = 1024u * 80u;
    s->out_buf_size = 1024u * 24u;
    s->in_buf  = malloc(s->in_buf_size);
    s->out_buf = malloc(s->out_buf_size);
    if (!s->in_buf || !s->out_buf) { alac_close(d); return NULL; }

    d->sample_rate = s->demux.sample_rate;
    d->channels    = s->demux.num_channels;
    return d;
}

/* ---------------------------------------------------------------------- *
 *  Decoder factory: dispatch by file extension
 * ---------------------------------------------------------------------- */

static AudioDecoder *open_decoder(const char *path) {
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;

    if (strcasecmp(dot, ".mp3") == 0)
        return open_mp3(path);
    if (strcasecmp(dot, ".flac") == 0)
        return open_flac(path);
    if (strcasecmp(dot, ".ogg") == 0 || strcasecmp(dot, ".oga") == 0)
        return open_vorbis(path);
    if (strcasecmp(dot, ".m4a") == 0 || strcasecmp(dot, ".mp4") == 0 ||
        strcasecmp(dot, ".alac") == 0)
        return open_alac(path);

    return NULL;
}

/* ---------------------------------------------------------------------- *
 *  Metadata extraction (best-effort, per format)
 * ---------------------------------------------------------------------- */

static char *xstrdup_trim(const char *s) {
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' ||
                     s[n-1] == '\r' || s[n-1] == '\n')) n--;
    char *r = malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

/* Look up a Vorbis comment of the form "KEY=VALUE", case-insensitive on key. */
static char *vc_get(char **comments, int count, const char *key) {
    size_t klen = strlen(key);
    for (int i = 0; i < count; i++) {
        const char *c = comments[i];
        if (!c) continue;
        if (strncasecmp(c, key, klen) == 0 && c[klen] == '=') {
            return xstrdup_trim(c + klen + 1);
        }
    }
    return NULL;
}

static void fill_metadata_mp3(PlaylistItem *item, const char *path) {
    if (!g_mpg123_initialized) {
        if (mpg123_init() != MPG123_OK) return;
        g_mpg123_initialized = 1;
    }
    int err = MPG123_OK;
    mpg123_handle *mh = mpg123_new(NULL, &err);
    if (!mh) return;
    if (mpg123_open(mh, path) != MPG123_OK) {
        mpg123_delete(mh);
        return;
    }
    /* Force scan so length/ID3 are populated. */
    mpg123_scan(mh);

    mpg123_id3v1 *v1 = NULL;
    mpg123_id3v2 *v2 = NULL;
    if (mpg123_id3(mh, &v1, &v2) == MPG123_OK) {
        if (v2) {
            if (v2->title  && v2->title->p)  item->title  = xstrdup_trim(v2->title->p);
            if (v2->artist && v2->artist->p) item->artist = xstrdup_trim(v2->artist->p);
            if (v2->album  && v2->album->p)  item->album  = xstrdup_trim(v2->album->p);
            if (v2->genre  && v2->genre->p)  item->genre  = xstrdup_trim(v2->genre->p);
            if (v2->year   && v2->year->p)   item->year   = xstrdup_trim(v2->year->p);
        }
        if (v1) {
            if (!item->title  && v1->title[0])  item->title  = xstrdup_trim(v1->title);
            if (!item->artist && v1->artist[0]) item->artist = xstrdup_trim(v1->artist);
            if (!item->album  && v1->album[0])  item->album  = xstrdup_trim(v1->album);
            if (!item->year   && v1->year[0])   item->year   = xstrdup_trim(v1->year);
        }
    }

    /* Duration: length in samples / sample rate. */
    off_t length = mpg123_length(mh);
    long rate = 0; int ch = 0, enc = 0;
    if (length > 0 && mpg123_getformat(mh, &rate, &ch, &enc) == MPG123_OK && rate > 0) {
        item->duration = (int)((double)length / (double)rate);
    }

    mpg123_close(mh);
    mpg123_delete(mh);
}

static void fill_metadata_flac(PlaylistItem *item, const char *path) {
    /* Duration via STREAMINFO. */
    FLAC__StreamMetadata sm;
    if (FLAC__metadata_get_streaminfo(path, &sm)) {
        if (sm.data.stream_info.sample_rate > 0) {
            item->duration = (int)(sm.data.stream_info.total_samples /
                                   sm.data.stream_info.sample_rate);
        }
    }

    /* Vorbis comments via the simple metadata API. */
    FLAC__StreamMetadata *vc = NULL;
    if (FLAC__metadata_get_tags(path, &vc) && vc) {
        char **comments = malloc(sizeof(char *) * vc->data.vorbis_comment.num_comments);
        if (comments) {
            for (FLAC__uint32 i = 0; i < vc->data.vorbis_comment.num_comments; i++) {
                comments[i] = (char *)vc->data.vorbis_comment.comments[i].entry;
            }
            int n = (int)vc->data.vorbis_comment.num_comments;
            if (!item->title)  item->title  = vc_get(comments, n, "TITLE");
            if (!item->artist) item->artist = vc_get(comments, n, "ARTIST");
            if (!item->album)  item->album  = vc_get(comments, n, "ALBUM");
            if (!item->genre)  item->genre  = vc_get(comments, n, "GENRE");
            if (!item->year)   item->year   = vc_get(comments, n, "DATE");
            free(comments);
        }
        FLAC__metadata_object_delete(vc);
    }
}

static void fill_metadata_vorbis(PlaylistItem *item, const char *path) {
    OggVorbis_File vf;
    if (ov_fopen(path, &vf) != 0) return;

    vorbis_comment *vc = ov_comment(&vf, -1);
    if (vc && vc->user_comments && vc->comment_lengths) {
        int n = vc->comments;
        if (!item->title)  item->title  = vc_get(vc->user_comments, n, "TITLE");
        if (!item->artist) item->artist = vc_get(vc->user_comments, n, "ARTIST");
        if (!item->album)  item->album  = vc_get(vc->user_comments, n, "ALBUM");
        if (!item->genre)  item->genre  = vc_get(vc->user_comments, n, "GENRE");
        if (!item->year)   item->year   = vc_get(vc->user_comments, n, "DATE");
    }

    double secs = ov_time_total(&vf, -1);
    if (secs > 0) item->duration = (int)secs;

    ov_clear(&vf);
}

/* ---------------------------------------------------------------------- *
 *  FAudio output (recreated per playback session with native format)
 * ---------------------------------------------------------------------- */

typedef struct {
    FAudio *audio;
    FAudioMasteringVoice *mastering_voice;
    FAudioSourceVoice *source_voice;
} FAudioContext;

/* Internal structures */
typedef struct AudioPlayerInternal AudioPlayerInternal; /* not used */

/* Helper function prototypes */
static void* playback_thread_func(void *arg);
static int init_faudio_context(FAudioContext *ctx,
                               unsigned int sample_rate,
                               unsigned int channels);
static void cleanup_faudio_context(FAudioContext *ctx);
static PlaylistItem* create_playlist_item(const char *filepath);
static void free_playlist_item(PlaylistItem *item);

/* Create a new audio player instance */
AudioPlayer* audioplayer_create(void) {
    AudioPlayer *player = malloc(sizeof(AudioPlayer));
    if (!player) {
        return NULL;
    }

    memset(player, 0, sizeof(AudioPlayer));

    player->state = STATE_STOPPED;
    player->loop_playlist = false;
    player->shuffle = false;
    player->volume = 1.0f;  /* Default: 100% volume */

    pthread_mutex_init(&player->lock, NULL);

    alac_decoder_detect_endianness();

    return player;
}

/* Destroy audio player instance */
void audioplayer_destroy(AudioPlayer *player) {
    if (!player) {
        return;
    }

    audioplayer_stop(player);
    audioplayer_clear_playlist(player);

    pthread_mutex_destroy(&player->lock);
    free(player);
}

/* Internal: Stop playback (assumes lock is held) */
static void audioplayer_stop_locked(AudioPlayer *player) {
    if (player->state != STATE_STOPPED) {
        player->state = STATE_STOPPED;
        pthread_mutex_unlock(&player->lock);

        /* Wait for playback thread to finish */
        pthread_join(player->playback_thread, NULL);

        pthread_mutex_lock(&player->lock);

        if (player->current_track && !player->playlist_head) {
            /* Free standalone track */
            free_playlist_item(player->current_track);
            player->current_track = NULL;
        }
    }
}

/* Play a file immediately */
int audioplayer_play(AudioPlayer *player, const char *filepath) {
    if (!player || !filepath) {
        return -1;
    }

    pthread_mutex_lock(&player->lock);

    /* Stop current playback if any */
    if (player->state != STATE_STOPPED) {
        audioplayer_stop_locked(player);
    }

    /* Create playlist item */
    PlaylistItem *item = create_playlist_item(filepath);
    if (!item) {
        pthread_mutex_unlock(&player->lock);
        return -1;
    }

    player->current_track = item;
    player->state = STATE_PLAYING;

    /* Start playback thread */
    if (pthread_create(&player->playback_thread, NULL, playback_thread_func, player) != 0) {
        free_playlist_item(item);
        player->current_track = NULL;
        player->state = STATE_STOPPED;
        pthread_mutex_unlock(&player->lock);
        return -1;
    }

    pthread_mutex_unlock(&player->lock);
    return 0;
}

/* Play a specific playlist item */
int audioplayer_play_playlist_item(AudioPlayer *player, PlaylistItem *item) {
    if (!player || !item) {
        return -1;
    }

    pthread_mutex_lock(&player->lock);

    /* Stop current playback if any */
    if (player->state != STATE_STOPPED) {
        audioplayer_stop_locked(player);
    }

    /* Set current track to the playlist item */
    player->current_track = item;
    player->state = STATE_PLAYING;

    /* Start playback thread */
    if (pthread_create(&player->playback_thread, NULL, playback_thread_func, player) != 0) {
        player->current_track = NULL;
        player->state = STATE_STOPPED;
        pthread_mutex_unlock(&player->lock);
        return -1;
    }

    pthread_mutex_unlock(&player->lock);
    return 0;
}

/* Pause playback */
int audioplayer_pause(AudioPlayer *player) {
    if (!player) {
        return -1;
    }

    pthread_mutex_lock(&player->lock);

    if (player->state == STATE_PLAYING) {
        player->state = STATE_PAUSED;
        if (player->faudio_device) {
            FAudioContext *fa_ctx = (FAudioContext*)player->faudio_device;
            if (fa_ctx->source_voice) {
                FAudioSourceVoice_Stop(fa_ctx->source_voice, 0, FAUDIO_COMMIT_NOW);
            }
        }
    }

    pthread_mutex_unlock(&player->lock);
    return 0;
}

/* Resume playback */
int audioplayer_resume(AudioPlayer *player) {
    if (!player) {
        return -1;
    }

    pthread_mutex_lock(&player->lock);

    if (player->state == STATE_PAUSED) {
        player->state = STATE_PLAYING;
        if (player->faudio_device) {
            FAudioContext *fa_ctx = (FAudioContext*)player->faudio_device;
            if (fa_ctx->source_voice) {
                FAudioSourceVoice_Start(fa_ctx->source_voice, 0, FAUDIO_COMMIT_NOW);
            }
        }
    }

    pthread_mutex_unlock(&player->lock);
    return 0;
}

/* Stop playback */
int audioplayer_stop(AudioPlayer *player) {
    if (!player) {
        return -1;
    }

    pthread_mutex_lock(&player->lock);
    audioplayer_stop_locked(player);
    pthread_mutex_unlock(&player->lock);
    return 0;
}

/* Play next track in playlist */
int audioplayer_next(AudioPlayer *player) {
    if (!player || !player->current_track) {
        return -1;
    }

    pthread_mutex_lock(&player->lock);

    PlaylistItem *next = player->current_track->next;
    if (!next && player->loop_playlist) {
        next = player->playlist_head;
    }

    if (next) {
        bool was_playing = (player->state == STATE_PLAYING);

        if (player->state != STATE_STOPPED) {
            audioplayer_stop_locked(player);
        }

        player->current_track = next;

        if (was_playing) {
            player->state = STATE_PLAYING;
            if (pthread_create(&player->playback_thread, NULL, playback_thread_func, player) != 0) {
                player->state = STATE_STOPPED;
                pthread_mutex_unlock(&player->lock);
                return -1;
            }
        }

        pthread_mutex_unlock(&player->lock);
        return 0;
    }

    pthread_mutex_unlock(&player->lock);
    return -1;
}

/* Play previous track in playlist */
int audioplayer_prev(AudioPlayer *player) {
    if (!player || !player->current_track) {
        return -1;
    }

    pthread_mutex_lock(&player->lock);

    PlaylistItem *prev = player->current_track->prev;
    if (!prev && player->loop_playlist) {
        prev = player->playlist_tail;
    }

    if (prev) {
        bool was_playing = (player->state == STATE_PLAYING);

        if (player->state != STATE_STOPPED) {
            audioplayer_stop_locked(player);
        }

        player->current_track = prev;

        if (was_playing) {
            player->state = STATE_PLAYING;
            if (pthread_create(&player->playback_thread, NULL, playback_thread_func, player) != 0) {
                player->state = STATE_STOPPED;
                pthread_mutex_unlock(&player->lock);
                return -1;
            }
        }

        pthread_mutex_unlock(&player->lock);
        return 0;
    }

    pthread_mutex_unlock(&player->lock);
    return -1;
}

/* Add file to playlist */
int audioplayer_add_to_playlist(AudioPlayer *player, const char *filepath) {
    if (!player || !filepath) {
        return -1;
    }

    PlaylistItem *item = create_playlist_item(filepath);
    if (!item) {
        return -1;
    }

    pthread_mutex_lock(&player->lock);

    if (!player->playlist_head) {
        player->playlist_head = item;
        player->playlist_tail = item;
    } else {
        player->playlist_tail->next = item;
        item->prev = player->playlist_tail;
        player->playlist_tail = item;
    }

    pthread_mutex_unlock(&player->lock);
    return 0;
}

/* Clear entire playlist */
int audioplayer_clear_playlist(AudioPlayer *player) {
    if (!player) {
        return -1;
    }

    pthread_mutex_lock(&player->lock);

    PlaylistItem *item = player->playlist_head;
    while (item) {
        PlaylistItem *next = item->next;
        free_playlist_item(item);
        item = next;
    }

    player->playlist_head = NULL;
    player->playlist_tail = NULL;
    player->current_track = NULL;

    pthread_mutex_unlock(&player->lock);
    return 0;
}

/* Load playlist from file (.m3u, .m3u8, .pls) */
int audioplayer_load_playlist_file(AudioPlayer *player, const char *playlist_file) {
    if (!player || !playlist_file) {
        return -1;
    }

    PlaylistParseResult *result = parse_playlist_file(playlist_file);
    if (!result) {
        return -1;
    }

    audioplayer_clear_playlist(player);

    int added = 0;
    for (int i = 0; i < result->count; i++) {
        if (audioplayer_add_to_playlist(player, result->files[i]) == 0) {
            added++;
        }
    }

    free_playlist_result(result);

    return added >= 0 ? added : -1;
}

/* Get playlist */
PlaylistItem* audioplayer_get_playlist(AudioPlayer *player) {
    if (!player) return NULL;
    return player->playlist_head;
}

/* Get playlist count */
int audioplayer_get_playlist_count(AudioPlayer *player) {
    if (!player) return 0;

    pthread_mutex_lock(&player->lock);

    int count = 0;
    PlaylistItem *item = player->playlist_head;
    while (item) {
        count++;
        item = item->next;
    }

    pthread_mutex_unlock(&player->lock);
    return count;
}

/* Get player state */
PlayerState audioplayer_get_state(AudioPlayer *player) {
    if (!player) return STATE_STOPPED;
    return player->state;
}

/* Get current track */
PlaylistItem* audioplayer_get_current_track(AudioPlayer *player) {
    if (!player) return NULL;
    return player->current_track;
}

/* Set loop mode */
void audioplayer_set_loop(AudioPlayer *player, bool loop) {
    if (player) {
        pthread_mutex_lock(&player->lock);
        player->loop_playlist = loop;
        pthread_mutex_unlock(&player->lock);
    }
}

/* Set shuffle mode */
void audioplayer_set_shuffle(AudioPlayer *player, bool shuffle) {
    if (player) {
        pthread_mutex_lock(&player->lock);
        player->shuffle = shuffle;
        pthread_mutex_unlock(&player->lock);
    }
}

/* ---------------------------------------------------------------------- *
 *  Helper Functions
 * ---------------------------------------------------------------------- */

static PlaylistItem* create_playlist_item(const char *filepath) {
    PlaylistItem *item = malloc(sizeof(PlaylistItem));
    if (!item) return NULL;

    memset(item, 0, sizeof(PlaylistItem));
    item->filepath = strdup(filepath);

    /* Best-effort metadata extraction per format. */
    const char *dot = strrchr(filepath, '.');
    if (dot) {
        if (strcasecmp(dot, ".mp3") == 0)
            fill_metadata_mp3(item, filepath);
        else if (strcasecmp(dot, ".flac") == 0)
            fill_metadata_flac(item, filepath);
        else if (strcasecmp(dot, ".ogg") == 0 || strcasecmp(dot, ".oga") == 0)
            fill_metadata_vorbis(item, filepath);
        /* ALAC metadata is not extracted: alac-decoder only exposes
         * codec data, not container-level tags. */
    }

    /* Fallback: derive title from filename. */
    if (!item->title) {
        const char *fn = strrchr(filepath, '/');
        if (!fn) fn = strrchr(filepath, '\\');
        fn = fn ? fn + 1 : filepath;
        item->title = strdup(fn);
    }

    return item;
}

static void free_playlist_item(PlaylistItem *item) {
    if (!item) return;
    free(item->filepath);
    free(item->title);
    free(item->artist);
    free(item->album);
    free(item->genre);
    free(item->year);
    free(item);
}

/* Callback to free buffer after FAudio is done with it */
static void FAUDIOCALL buffer_end_callback(FAudioVoiceCallback *callback, void *pBufferContext) {
    (void)callback;
    if (pBufferContext) {
        free(pBufferContext);
    }
}

static FAudioVoiceCallback voice_callbacks = {
    .OnBufferEnd = buffer_end_callback,
    .OnBufferStart = NULL,
    .OnLoopEnd = NULL,
    .OnStreamEnd = NULL,
    .OnVoiceError = NULL,
    .OnVoiceProcessingPassEnd = NULL,
    .OnVoiceProcessingPassStart = NULL
};

static int init_faudio_context(FAudioContext *ctx,
                               unsigned int sample_rate,
                               unsigned int channels) {
    memset(ctx, 0, sizeof(FAudioContext));

    if (FAudioCreate(&ctx->audio, 0, FAUDIO_DEFAULT_PROCESSOR) != 0) {
        return -1;
    }

    if (FAudio_CreateMasteringVoice(ctx->audio, &ctx->mastering_voice,
        FAUDIO_DEFAULT_CHANNELS, FAUDIO_DEFAULT_SAMPLERATE, 0, 0, NULL) != 0) {
        FAudio_Release(ctx->audio);
        return -1;
    }

    FAudioWaveFormatEx waveformat = {
        .wFormatTag = FAUDIO_FORMAT_PCM,
        .nChannels = (uint16_t)channels,
        .nSamplesPerSec = sample_rate,
        .wBitsPerSample = 16,
        .nBlockAlign = (uint16_t)(channels * 2),
        .nAvgBytesPerSec = sample_rate * channels * 2,
        .cbSize = 0
    };

    if (FAudio_CreateSourceVoice(ctx->audio, &ctx->source_voice, &waveformat,
        0, FAUDIO_DEFAULT_FREQ_RATIO, &voice_callbacks, NULL, NULL) != 0) {
        FAudioVoice_DestroyVoice(ctx->mastering_voice);
        FAudio_Release(ctx->audio);
        return -1;
    }

    return 0;
}

static void cleanup_faudio_context(FAudioContext *ctx) {
    if (ctx->source_voice) {
        FAudioSourceVoice_Stop(ctx->source_voice, 0, FAUDIO_COMMIT_NOW);
        FAudioVoice_DestroyVoice(ctx->source_voice);
    }
    if (ctx->mastering_voice) {
        FAudioVoice_DestroyVoice(ctx->mastering_voice);
    }
    if (ctx->audio) {
        FAudio_Release(ctx->audio);
    }
}

/* Playback thread function */
static void* playback_thread_func(void *arg) {
    AudioPlayer *player = (AudioPlayer*)arg;

    pthread_mutex_lock(&player->lock);
    const char *filepath = player->current_track ? player->current_track->filepath : NULL;
    pthread_mutex_unlock(&player->lock);

    if (!filepath) return NULL;

    AudioDecoder *decoder = open_decoder(filepath);
    if (!decoder) {
        fprintf(stderr, "[AudioPlayer] No decoder available for: %s\n", filepath);
        pthread_mutex_lock(&player->lock);
        player->state = STATE_STOPPED;
        pthread_mutex_unlock(&player->lock);
        return NULL;
    }
    fprintf(stderr, "[AudioPlayer] Decoder opened: %u Hz, %u ch\n",
            decoder->sample_rate, decoder->channels);

    FAudioContext faudio_ctx;
    if (init_faudio_context(&faudio_ctx, decoder->sample_rate, decoder->channels) < 0) {
        fprintf(stderr, "[AudioPlayer] Failed to initialize FAudio context\n");
        decoder->close(decoder);
        pthread_mutex_lock(&player->lock);
        player->state = STATE_STOPPED;
        pthread_mutex_unlock(&player->lock);
        return NULL;
    }

    pthread_mutex_lock(&player->lock);
    player->decoder_ctx = decoder;
    player->faudio_device = &faudio_ctx;
    float volume = player->volume;
    pthread_mutex_unlock(&player->lock);

    FAudioVoice_SetVolume((FAudioVoice*)faudio_ctx.source_voice, volume, FAUDIO_COMMIT_NOW);

    fprintf(stderr, "[AudioPlayer] Starting playback...\n");
    FAudioSourceVoice_Start(faudio_ctx.source_voice, 0, FAUDIO_COMMIT_NOW);

    /* Decode and submit loop. */
    int bytes_per_frame = (int)(decoder->channels * sizeof(int16_t));

    for (;;) {
        pthread_mutex_lock(&player->lock);
        PlayerState state = player->state;
        pthread_mutex_unlock(&player->lock);

        if (state == STATE_STOPPED) break;

        while (state == STATE_PAUSED) {
            usleep(10000);
            pthread_mutex_lock(&player->lock);
            state = player->state;
            pthread_mutex_unlock(&player->lock);
            if (state == STATE_STOPPED) break;
        }
        if (state == STATE_STOPPED) break;

        const int16_t *pcm = NULL;
        int frames = 0;
        int r = decoder->decode_next(decoder, &pcm, &frames);
        if (r < 0) {
            fprintf(stderr, "[AudioPlayer] Decode error\n");
            break;
        }
        if (r == 0) {
            /* EOF */
            break;
        }
        if (frames <= 0 || !pcm) {
            /* Decoder produced no audio this step (e.g. metadata); loop. */
            continue;
        }

        size_t buffer_size = (size_t)frames * bytes_per_frame;
        uint8_t *buffer_copy = malloc(buffer_size);
        if (!buffer_copy) break;
        memcpy(buffer_copy, pcm, buffer_size);

        FAudioBuffer buffer = {
            .AudioBytes = (uint32_t)buffer_size,
            .pAudioData = buffer_copy,
            .PlayBegin = 0,
            .PlayLength = 0,
            .LoopBegin = 0,
            .LoopLength = 0,
            .LoopCount = 0,
            .pContext = buffer_copy,
        };
        FAudioSourceVoice_SubmitSourceBuffer(faudio_ctx.source_voice, &buffer, NULL);

        /* Throttle to avoid unbounded queue growth. */
        FAudioVoiceState voice_state;
        do {
            FAudioSourceVoice_GetState(faudio_ctx.source_voice, &voice_state, 0);
            if (voice_state.BuffersQueued > 4) {
                usleep(5000);
                pthread_mutex_lock(&player->lock);
                int stopped = (player->state == STATE_STOPPED);
                pthread_mutex_unlock(&player->lock);
                if (stopped) break;
            }
        } while (voice_state.BuffersQueued > 4);
    }

    /* Drain remaining queued buffers. */
    FAudioVoiceState voice_state;
    do {
        FAudioSourceVoice_GetState(faudio_ctx.source_voice, &voice_state, 0);
        if (voice_state.BuffersQueued > 0) usleep(10000);

        pthread_mutex_lock(&player->lock);
        int stopped = (player->state == STATE_STOPPED);
        pthread_mutex_unlock(&player->lock);
        if (stopped) break;
    } while (voice_state.BuffersQueued > 0);

    /* Cleanup */
    decoder->close(decoder);
    cleanup_faudio_context(&faudio_ctx);

    pthread_mutex_lock(&player->lock);
    player->decoder_ctx = NULL;
    player->faudio_device = NULL;

    /* Auto-play next track if in playlist mode */
    if (player->state != STATE_STOPPED && player->current_track && player->current_track->next) {
        PlaylistItem *next = player->current_track->next;
        player->current_track = next;
        pthread_mutex_unlock(&player->lock);
        audioplayer_play(player, next->filepath);
    } else {
        player->state = STATE_STOPPED;
        pthread_mutex_unlock(&player->lock);
    }

    return NULL;
}

/* Set volume (0.0 to 1.0) */
void audioplayer_set_volume(AudioPlayer *player, float volume) {
    if (!player) return;

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    pthread_mutex_lock(&player->lock);
    player->volume = volume;

    if (player->faudio_device) {
        FAudioContext *fa_ctx = (FAudioContext*)player->faudio_device;
        if (fa_ctx->source_voice) {
            FAudioVoice_SetVolume((FAudioVoice*)fa_ctx->source_voice, volume, FAUDIO_COMMIT_NOW);
        }
    }

    pthread_mutex_unlock(&player->lock);
}

/* Get current volume */
float audioplayer_get_volume(AudioPlayer *player) {
    if (!player) return 1.0f;
    pthread_mutex_lock(&player->lock);
    float volume = player->volume;
    pthread_mutex_unlock(&player->lock);
    return volume;
}

/* Get current playback position (seconds). Not currently tracked. */
int audioplayer_get_position(AudioPlayer *player) {
    (void)player;
    return 0;
}
