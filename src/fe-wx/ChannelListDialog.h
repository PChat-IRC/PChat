/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Channel List Dialog - replicates fe-gtk3/chanlist.c functionality
 */

#ifndef PCHAT_CHANNELLISTDIALOG_H
#define PCHAT_CHANNELLISTDIALOG_H

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <vector>
#include <string>

#include "../common/pchat.h"

struct ChanListRow {
    wxString channel;
    int users;
    wxString topic;
};

class ChannelListDialog : public wxDialog
{
public:
    ChannelListDialog(wxWindow *parent, struct server *serv, bool do_refresh);
    ~ChannelListDialog();

    /* Called from fe_add_chan_list / fe_chan_list_end */
    void AddRow(const char *chan, const char *users, const char *topic);
    void ListEnd();

    struct server *GetServer() { return m_serv; }

private:
    void DoRefresh();
    void ApplyFilter();
    void UpdateCaption();
    void UpdateButtons();
    bool MatchesFilter(const ChanListRow &row);

    void OnRefresh(wxCommandEvent &event);
    void OnSearch(wxCommandEvent &event);
    void OnJoin(wxCommandEvent &event);
    void OnSaveList(wxCommandEvent &event);
    void OnMinUsersChanged(wxSpinEvent &event);
    void OnMaxUsersChanged(wxSpinEvent &event);
    void OnItemActivated(wxListEvent &event);
    void OnRightClick(wxListEvent &event);
    void OnJoinMenu(wxCommandEvent &event);
    void OnCopyChannel(wxCommandEvent &event);
    void OnCopyTopic(wxCommandEvent &event);
    void OnCloseWindow(wxCloseEvent &event);
    void OnTimer(wxTimerEvent &event);

    struct server *m_serv;

    wxListCtrl *m_list;
    wxTextCtrl *m_find_entry;
    wxComboBox *m_search_type;
    wxCheckBox *m_match_channel;
    wxCheckBox *m_match_topic;
    wxSpinCtrl *m_min_spin;
    wxSpinCtrl *m_max_spin;
    wxStaticText *m_caption;
    wxButton *m_btn_refresh;
    wxButton *m_btn_join;
    wxButton *m_btn_save;
    wxButton *m_btn_search;

    /* All received rows */
    std::vector<ChanListRow> m_all_rows;

    /* Counters */
    int m_users_found;
    int m_users_shown;
    int m_channels_found;
    int m_channels_shown;

    /* Pending rows for batch insert */
    wxTimer m_flush_timer;

    /* Context menu item */
    long m_ctx_item;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_CHANLIST_REFRESH = wxID_HIGHEST + 850,
    ID_CHANLIST_SEARCH,
    ID_CHANLIST_JOIN,
    ID_CHANLIST_SAVE,
    ID_CHANLIST_MIN_SPIN,
    ID_CHANLIST_MAX_SPIN,
    ID_CHANLIST_TIMER,
    ID_CHANLIST_JOIN_MENU,
    ID_CHANLIST_COPY_CHAN,
    ID_CHANLIST_COPY_TOPIC,
};

#endif /* PCHAT_CHANNELLISTDIALOG_H */
