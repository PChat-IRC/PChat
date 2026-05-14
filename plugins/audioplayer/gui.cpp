/* PChat Audio Player Plugin - GUI Interface (wxWidgets + wxAUI)
 * Copyright (C) 2025
 *
 * The audioplayer plugin is loaded into pchat.exe, which runs its own
 * (GTK) main loop. To avoid clashing with that loop and to keep the
 * plugin self-contained, the wxWidgets GUI runs on its own dedicated
 * thread. All cross-thread access goes through wxApp::CallAfter, which
 * is documented as thread-safe.
 */

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#  include <wx/wx.h>
#endif

#include <wx/app.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibar.h>
#include <wx/aui/framemanager.h>
#include <wx/listctrl.h>
#include <wx/slider.h>
#include <wx/timer.h>
#include <wx/init.h>
#include <wx/filedlg.h>
#include <wx/artprov.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/frame.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <string>

extern "C" {
#include "gui.h"
#include "audioplayer.h"
}

/* The pchat-plugin.h header (pulled in via gui.h) defines convenience
 * macros like `pchat_printf` that expand to `((ph)->pchat_printf)`,
 * relying on a local `ph` symbol. We invoke the function pointers
 * through our own handle (`g_ph`), so undefine the macros here. */
#undef pchat_print
#undef pchat_printf
#undef pchat_command
#undef pchat_commandf
#undef pchat_hook_command

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */

class AudioPlayerFrame;
class AudioPlayerApp;

namespace {
    /* Shared state owned by the GUI thread. */
    std::thread             g_gui_thread;
    std::mutex              g_state_mtx;
    std::condition_variable g_state_cv;
    std::atomic<bool>       g_gui_running{false};
    AudioPlayerFrame*       g_frame = nullptr;       /* GUI thread only */
    pchat_plugin*           g_ph     = nullptr;
    AudioPlayer*            g_player = nullptr;
}

/* ------------------------------------------------------------------ */
/*  AUI toolbar IDs                                                   */
/* ------------------------------------------------------------------ */

enum {
    ID_PREV = wxID_HIGHEST + 1,
    ID_PLAY,
    ID_PAUSE,
    ID_STOP,
    ID_NEXT,
    ID_ADD_FILES,
    ID_LOAD_PLAYLIST,
    ID_CLEAR_PLAYLIST,
    ID_VOLUME_SLIDER,
    ID_PLAYLIST_LIST,
    ID_REFRESH_TIMER
};

/* ------------------------------------------------------------------ */
/*  Frame                                                             */
/* ------------------------------------------------------------------ */

class AudioPlayerFrame : public wxFrame {
public:
    AudioPlayerFrame();
    ~AudioPlayerFrame() override;

    /* May be called from any thread (uses CallAfter internally). */
    void RefreshFromAnyThread();

private:
    void BuildToolBar();
    void BuildPlaylistPanel();
    void BuildStatusBar();

    void RebuildPlaylist();
    void RefreshNowPlaying();

    /* Event handlers */
    void OnPrev(wxCommandEvent&);
    void OnPlay(wxCommandEvent&);
    void OnPause(wxCommandEvent&);
    void OnStop(wxCommandEvent&);
    void OnNext(wxCommandEvent&);
    void OnAddFiles(wxCommandEvent&);
    void OnLoadPlaylist(wxCommandEvent&);
    void OnClearPlaylist(wxCommandEvent&);
    void OnVolume(wxCommandEvent&);
    void OnRowActivated(wxListEvent&);
    void OnTimer(wxTimerEvent&);
    void OnClose(wxCloseEvent&);

    wxAuiManager   m_aui;
    wxAuiToolBar*  m_tb_transport = nullptr;
    wxAuiToolBar*  m_tb_playlist  = nullptr;
    wxSlider*      m_volume       = nullptr;
    wxStaticText*  m_now_playing  = nullptr;
    wxListCtrl*    m_playlist     = nullptr;
    wxTimer        m_timer;

    /* Cached playlist snapshot so we only rebuild when something changes. */
    std::vector<PlaylistItem*> m_cached_items;
    PlaylistItem*              m_cached_current = nullptr;
    PlayerState                m_cached_state   = STATE_STOPPED;

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(AudioPlayerFrame, wxFrame)
    EVT_TOOL(ID_PREV,            AudioPlayerFrame::OnPrev)
    EVT_TOOL(ID_PLAY,            AudioPlayerFrame::OnPlay)
    EVT_TOOL(ID_PAUSE,           AudioPlayerFrame::OnPause)
    EVT_TOOL(ID_STOP,            AudioPlayerFrame::OnStop)
    EVT_TOOL(ID_NEXT,            AudioPlayerFrame::OnNext)
    EVT_TOOL(ID_ADD_FILES,       AudioPlayerFrame::OnAddFiles)
    EVT_TOOL(ID_LOAD_PLAYLIST,   AudioPlayerFrame::OnLoadPlaylist)
    EVT_TOOL(ID_CLEAR_PLAYLIST,  AudioPlayerFrame::OnClearPlaylist)
    EVT_SLIDER(ID_VOLUME_SLIDER, AudioPlayerFrame::OnVolume)
    EVT_LIST_ITEM_ACTIVATED(ID_PLAYLIST_LIST, AudioPlayerFrame::OnRowActivated)
    EVT_TIMER(ID_REFRESH_TIMER,  AudioPlayerFrame::OnTimer)
    EVT_CLOSE(AudioPlayerFrame::OnClose)
wxEND_EVENT_TABLE()

AudioPlayerFrame::AudioPlayerFrame()
    : wxFrame(nullptr, wxID_ANY, wxT("Audio Player"),
              wxDefaultPosition, wxSize(640, 480),
              wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN)
    , m_timer(this, ID_REFRESH_TIMER)
{
    m_aui.SetManagedWindow(this);

    BuildToolBar();
    BuildPlaylistPanel();
    BuildStatusBar();

    m_aui.Update();

    /* Initial state. */
    if (g_player) {
        m_volume->SetValue(static_cast<int>(audioplayer_get_volume(g_player) * 100.0f));
    }
    RebuildPlaylist();
    RefreshNowPlaying();

    m_timer.Start(500);
}

AudioPlayerFrame::~AudioPlayerFrame() {
    m_timer.Stop();
    m_aui.UnInit();
}

void AudioPlayerFrame::BuildToolBar() {
    /* Transport (left) */
    m_tb_transport = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxAUI_TB_DEFAULT_STYLE);
    m_tb_transport->AddTool(ID_PREV,  wxT("Previous"),
        wxArtProvider::GetBitmap(wxART_GO_BACK, wxART_TOOLBAR), wxT("Previous"));
    m_tb_transport->AddTool(ID_PLAY,  wxT("Play"),
        wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR), wxT("Play / Resume"));
    m_tb_transport->AddTool(ID_PAUSE, wxT("Pause"),
        wxArtProvider::GetBitmap(wxART_MINUS, wxART_TOOLBAR), wxT("Pause"));
    m_tb_transport->AddTool(ID_STOP,  wxT("Stop"),
        wxArtProvider::GetBitmap(wxART_CROSS_MARK, wxART_TOOLBAR), wxT("Stop"));
    m_tb_transport->AddTool(ID_NEXT,  wxT("Next"),
        wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR), wxT("Next"));
    m_tb_transport->AddSeparator();
    m_volume = new wxSlider(m_tb_transport, ID_VOLUME_SLIDER, 100, 0, 100,
                            wxDefaultPosition, wxSize(140, -1));
    m_tb_transport->AddControl(m_volume, wxT("Volume"));
    m_tb_transport->Realize();

    /* Playlist controls (right) */
    m_tb_playlist = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     wxAUI_TB_DEFAULT_STYLE);
    m_tb_playlist->AddTool(ID_ADD_FILES, wxT("Add"),
        wxArtProvider::GetBitmap(wxART_PLUS, wxART_TOOLBAR), wxT("Add audio files"));
    m_tb_playlist->AddTool(ID_LOAD_PLAYLIST, wxT("Load"),
        wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR), wxT("Load playlist file"));
    m_tb_playlist->AddTool(ID_CLEAR_PLAYLIST, wxT("Clear"),
        wxArtProvider::GetBitmap(wxART_DELETE, wxART_TOOLBAR), wxT("Clear playlist"));
    m_tb_playlist->Realize();

    m_aui.AddPane(m_tb_transport, wxAuiPaneInfo().Name(wxT("transport"))
                  .Caption(wxT("Playback")).ToolbarPane().Top().LeftDockable(false)
                  .RightDockable(false));
    m_aui.AddPane(m_tb_playlist, wxAuiPaneInfo().Name(wxT("playlist_tb"))
                  .Caption(wxT("Playlist")).ToolbarPane().Top().LeftDockable(false)
                  .RightDockable(false).Position(1));
}

void AudioPlayerFrame::BuildPlaylistPanel() {
    wxPanel* center = new wxPanel(this);
    wxBoxSizer* sz = new wxBoxSizer(wxVERTICAL);

    m_now_playing = new wxStaticText(center, wxID_ANY, wxT("Not playing"));
    wxFont f = m_now_playing->GetFont();
    f.MakeBold();
    m_now_playing->SetFont(f);
    sz->Add(m_now_playing, 0, wxEXPAND | wxALL, 6);

    m_playlist = new wxListCtrl(center, ID_PLAYLIST_LIST,
                                wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
    m_playlist->AppendColumn(wxT(""),     wxLIST_FORMAT_LEFT, 24);
    m_playlist->AppendColumn(wxT("Title"), wxLIST_FORMAT_LEFT, 560);
    sz->Add(m_playlist, 1, wxEXPAND | wxALL, 4);

    center->SetSizer(sz);

    m_aui.AddPane(center, wxAuiPaneInfo().Name(wxT("center")).CenterPane());
}

void AudioPlayerFrame::BuildStatusBar() {
    CreateStatusBar(1);
    SetStatusText(wxT("Ready"));
}

/* -------------------------- helpers ------------------------------- */

void AudioPlayerFrame::RebuildPlaylist() {
    if (!g_player) return;

    std::vector<PlaylistItem*> items;
    for (PlaylistItem* it = audioplayer_get_playlist(g_player); it; it = it->next) {
        items.push_back(it);
    }
    PlaylistItem* current = audioplayer_get_current_track(g_player);

    /* Cheap change detection. */
    if (items == m_cached_items && current == m_cached_current) {
        return;
    }
    m_cached_items   = items;
    m_cached_current = current;

    m_playlist->Freeze();
    m_playlist->DeleteAllItems();

    long row = 0;
    for (PlaylistItem* it : items) {
        long idx = m_playlist->InsertItem(row, (it == current) ? wxT("\u25B6") : wxT(""));
        wxString title;
        if (it->artist && it->title) {
            title.Printf(wxT("%s - %s"), wxString::FromUTF8(it->artist),
                                         wxString::FromUTF8(it->title));
        } else if (it->title) {
            title = wxString::FromUTF8(it->title);
        } else {
            title = wxString::FromUTF8(it->filepath ? it->filepath : "");
        }
        m_playlist->SetItem(idx, 1, title);
        m_playlist->SetItemPtrData(idx, reinterpret_cast<wxUIntPtr>(it));
        if (it == current) {
            m_playlist->SetItemState(idx, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        }
        ++row;
    }
    m_playlist->Thaw();
}

void AudioPlayerFrame::RefreshNowPlaying() {
    if (!g_player) return;

    PlayerState   state   = audioplayer_get_state(g_player);
    PlaylistItem* current = audioplayer_get_current_track(g_player);
    m_cached_state = state;

    if (current && state != STATE_STOPPED) {
        const wxChar* st = wxT("Stopped");
        switch (state) {
            case STATE_PLAYING: st = wxT("Playing"); break;
            case STATE_PAUSED:  st = wxT("Paused");  break;
            default: break;
        }
        wxString text;
        if (current->artist && current->title) {
            if (current->album) {
                text.Printf(wxT("%s: %s - %s [%s]"), st,
                            wxString::FromUTF8(current->artist),
                            wxString::FromUTF8(current->title),
                            wxString::FromUTF8(current->album));
            } else {
                text.Printf(wxT("%s: %s - %s"), st,
                            wxString::FromUTF8(current->artist),
                            wxString::FromUTF8(current->title));
            }
        } else {
            text.Printf(wxT("%s: %s"), st,
                        wxString::FromUTF8(current->title ? current->title : ""));
        }
        m_now_playing->SetLabel(text);
    } else {
        m_now_playing->SetLabel(wxT("Not playing"));
    }
}

void AudioPlayerFrame::RefreshFromAnyThread() {
    CallAfter([this]{ RefreshNowPlaying(); RebuildPlaylist(); });
}

/* -------------------------- event handlers ------------------------ */

void AudioPlayerFrame::OnPrev(wxCommandEvent&)  { audioplayer_prev(g_player); RebuildPlaylist(); RefreshNowPlaying(); }
void AudioPlayerFrame::OnPause(wxCommandEvent&) { audioplayer_pause(g_player); RefreshNowPlaying(); }
void AudioPlayerFrame::OnStop(wxCommandEvent&)  { audioplayer_stop(g_player); RefreshNowPlaying(); }
void AudioPlayerFrame::OnNext(wxCommandEvent&)  { audioplayer_next(g_player); RebuildPlaylist(); RefreshNowPlaying(); }

void AudioPlayerFrame::OnPlay(wxCommandEvent&) {
    if (!g_player) return;
    if (audioplayer_get_state(g_player) == STATE_PAUSED) {
        audioplayer_resume(g_player);
    } else {
        PlaylistItem* current = audioplayer_get_current_track(g_player);
        if (current) {
            audioplayer_play_playlist_item(g_player, current);
        } else {
            PlaylistItem* first = audioplayer_get_playlist(g_player);
            if (first) audioplayer_play_playlist_item(g_player, first);
        }
    }
    RebuildPlaylist();
    RefreshNowPlaying();
}

void AudioPlayerFrame::OnVolume(wxCommandEvent&) {
    if (g_player && m_volume) {
        audioplayer_set_volume(g_player, m_volume->GetValue() / 100.0f);
    }
}

void AudioPlayerFrame::OnAddFiles(wxCommandEvent&) {
    wxFileDialog dlg(this, wxT("Add Audio Files"), wxEmptyString, wxEmptyString,
        wxT("Audio files (*.mp3;*.flac;*.ogg;*.oga;*.m4a)|*.mp3;*.flac;*.ogg;*.oga;*.m4a|All files (*.*)|*.*"),
        wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    wxArrayString paths;
    dlg.GetPaths(paths);
    for (const wxString& p : paths) {
        const wxScopedCharBuffer u8 = p.ToUTF8();
        audioplayer_add_to_playlist(g_player, u8.data());
    }
    RebuildPlaylist();
}

void AudioPlayerFrame::OnLoadPlaylist(wxCommandEvent&) {
    wxFileDialog dlg(this, wxT("Load Playlist"), wxEmptyString, wxEmptyString,
        wxT("Playlist files (*.m3u;*.m3u8;*.pls)|*.m3u;*.m3u8;*.pls|All files (*.*)|*.*"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    const wxScopedCharBuffer u8 = dlg.GetPath().ToUTF8();
    int count = audioplayer_load_playlist_file(g_player, u8.data());
    if (g_ph) {
        if (count > 0) {
            g_ph->pchat_printf(g_ph, "[AudioPlayer] Loaded %d tracks from playlist\n", count);
        } else if (count == 0) {
            g_ph->pchat_printf(g_ph, "[AudioPlayer] No valid tracks found in playlist\n");
        } else {
            g_ph->pchat_printf(g_ph, "[AudioPlayer] Failed to load playlist\n");
        }
    }
    RebuildPlaylist();
}

void AudioPlayerFrame::OnClearPlaylist(wxCommandEvent&) {
    audioplayer_clear_playlist(g_player);
    RebuildPlaylist();
}

void AudioPlayerFrame::OnRowActivated(wxListEvent& e) {
    long row = e.GetIndex();
    if (row < 0) return;
    PlaylistItem* it = reinterpret_cast<PlaylistItem*>(m_playlist->GetItemData(row));
    if (it && it->filepath) {
        audioplayer_play_playlist_item(g_player, it);
        RebuildPlaylist();
        RefreshNowPlaying();
    }
}

void AudioPlayerFrame::OnTimer(wxTimerEvent&) {
    RefreshNowPlaying();
    RebuildPlaylist();
}

void AudioPlayerFrame::OnClose(wxCloseEvent& e) {
    /* Hide instead of destroy: a subsequent /APLAYER will re-show the
     * same window, mirroring the original GTK behaviour. */
    if (e.CanVeto()) {
        e.Veto();
        Hide();
    } else {
        Destroy();
    }
}

/* ------------------------------------------------------------------ */
/*  wxApp running on the GUI thread                                   */
/* ------------------------------------------------------------------ */

class AudioPlayerApp : public wxApp {
public:
    bool OnInit() override {
        g_frame = new AudioPlayerFrame();
        g_frame->Show(true);
        SetTopWindow(g_frame);
        {
            std::lock_guard<std::mutex> lk(g_state_mtx);
            g_gui_running.store(true, std::memory_order_release);
        }
        g_state_cv.notify_all();
        return true;
    }

    int OnExit() override {
        g_frame = nullptr;
        g_gui_running.store(false, std::memory_order_release);
        return wxApp::OnExit();
    }
};

/* We don't use wxIMPLEMENT_APP because we drive wxEntry manually
 * from a worker thread; we still need the IMPLEMENT_APP_NO_MAIN
 * machinery so the app object is created via wxApp::SetInitializerFunction. */
wxIMPLEMENT_APP_NO_MAIN(AudioPlayerApp);

/* ------------------------------------------------------------------ */
/*  GUI thread entrypoint                                             */
/* ------------------------------------------------------------------ */

static void gui_thread_main() {
    /* wxEntry expects argv. Provide a single-element argv[0]. */
    int    argc   = 1;
    char   arg0[] = "audioplayer";
    char*  argv[] = { arg0, nullptr };
    wxEntry(argc, argv);
}

/* ------------------------------------------------------------------ */
/*  C-callable entry points                                           */
/* ------------------------------------------------------------------ */

extern "C" void audioplayer_gui_init(pchat_plugin* ph, AudioPlayer* player) {
    g_ph     = ph;
    g_player = player;

    if (g_gui_running.load(std::memory_order_acquire) && g_frame) {
        /* Already up: re-show and raise. */
        AudioPlayerFrame* f = g_frame;
        f->CallAfter([f]{ f->Show(true); f->Raise(); });
        return;
    }

    if (g_gui_thread.joinable()) {
        /* Stale (thread exited). Reap before respawning. */
        g_gui_thread.join();
    }

    g_gui_thread = std::thread(gui_thread_main);

    /* Wait for OnInit to publish g_frame so subsequent calls work. */
    std::unique_lock<std::mutex> lk(g_state_mtx);
    g_state_cv.wait_for(lk, std::chrono::seconds(5),
        []{ return g_gui_running.load(std::memory_order_acquire); });
}

extern "C" void audioplayer_gui_update(void) {
    if (!g_gui_running.load(std::memory_order_acquire) || !g_frame) return;
    g_frame->RefreshFromAnyThread();
}

extern "C" void audioplayer_gui_cleanup(void) {
    if (g_gui_running.load(std::memory_order_acquire) && g_frame) {
        AudioPlayerFrame* f = g_frame;
        f->CallAfter([f]{ f->Destroy(); if (wxTheApp) wxTheApp->ExitMainLoop(); });
    }
    if (g_gui_thread.joinable()) {
        g_gui_thread.join();
    }
    g_frame = nullptr;
    g_gui_running.store(false, std::memory_order_release);
}
