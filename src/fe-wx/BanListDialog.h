/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Ban List Dialog - displays bans, exempts, invites, quiets
 * Replicates fe-gtk3/banlist.c functionality
 */

#ifndef PCHAT_BANLISTDIALOG_H
#define PCHAT_BANLISTDIALOG_H

#include <wx/wx.h>
#include <wx/dataview.h>

#include "../common/pchat.h"

/* RPL codes */
#ifndef RPL_BANLIST
#define RPL_BANLIST 367
#define RPL_ENDOFBANLIST 368
#define RPL_INVITELIST 346
#define RPL_ENDOFINVITELIST 347
#define RPL_EXCEPTLIST 348
#define RPL_ENDOFEXCEPTLIST 349
#define RPL_QUIETLIST 728
#define RPL_ENDOFQUIETLIST 729
#endif

/* Mode indices */
enum BanlistMode {
    MODE_BAN = 0,
    MODE_EXEMPT,
    MODE_INVITE,
    MODE_QUIET,
    MODE_CT
};

/* Info about each mode type */
struct BanModeInfo {
    const char *name;     /* Checkbox label, e.g. "Bans" */
    const char *type;     /* Column value, e.g. "Ban" */
    char letter;          /* Mode letter, e.g. 'b' */
    int code;             /* RPL code */
    int endcode;          /* RPL end code */
    int bit;              /* 1 << mode index */
};

/* Column indices for the data-view model */
enum {
    COL_TYPE = 0,
    COL_MASK,
    COL_FROM,
    COL_DATE,
    COL_COUNT
};

class BanListDialog : public wxDialog
{
public:
    BanListDialog(wxWindow *parent, struct session *sess);
    ~BanListDialog();

    /* Called from fe_add_ban_list / fe_ban_list_end */
    bool AddEntry(const char *mask, const char *who,
                  const char *when, int rplcode);
    bool ListEnd(int rplcode);

    struct session *GetSession() { return m_sess; }

private:
    void InitModeCapabilities();
    void DoRefresh();
    void UpdateSensitivity();
    int  GetSelectedCount();

    void OnRefresh(wxCommandEvent &event);
    void OnRemove(wxCommandEvent &event);
    void OnCrop(wxCommandEvent &event);
    void OnClear(wxCommandEvent &event);
    void OnSelectionChanged(wxDataViewEvent &event);
    void OnRightClick(wxDataViewEvent &event);
    void OnCopyMask(wxCommandEvent &event);
    void OnCopyEntry(wxCommandEvent &event);
    void OnCloseWindow(wxCloseEvent &event);

    struct session *m_sess;
    wxDataViewListCtrl *m_list;

    wxButton *m_btn_remove;
    wxButton *m_btn_crop;
    wxButton *m_btn_clear;
    wxButton *m_btn_refresh;

    int m_capable;    /* bitmask of supported modes */
    int m_readable;   /* modes readable without op */
    int m_writeable;  /* modes writable as op */
    int m_checked;    /* currently checked modes */
    int m_pending;    /* modes we're waiting for replies */
    int m_line_ct;    /* number of lines in list */
    int m_select_ct;  /* number of selected lines */

    /* Right-click context */
    int m_ctx_row;

    static const BanModeInfo s_modes[MODE_CT];

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_BANLIST_REFRESH = wxID_HIGHEST + 800,
    ID_BANLIST_REMOVE,
    ID_BANLIST_CROP,
    ID_BANLIST_CLEAR,
    ID_BANLIST_CHECK_BASE, /* through CHECK_BASE + MODE_CT - 1 */
    ID_BANLIST_CHECK_END = ID_BANLIST_CHECK_BASE + MODE_CT,
    ID_BANLIST_COPY_MASK,
    ID_BANLIST_COPY_ENTRY,
};

#endif /* PCHAT_BANLISTDIALOG_H */
