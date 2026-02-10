/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Preferences Dialog implementation
 * Replicates HexChat preferences with tree-based category navigation:
 * - Interface: Appearance, Input box, User list, Channel switcher, Colors
 * - Chatting: General, Alerts, Sounds, Logging, Advanced
 * - Network: Network setup, File transfers, Identd
 */

#include "PreferencesDialog.h"
#include "MainWindow.h"
#include "palette.h"
#include "fe-wx.h"
#include <wx/fontdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/dirdlg.h>
#include <wx/scrolwin.h>
#include <wx/colordlg.h>

extern "C" {
extern MainWindow *g_main_window;

#include "../common/pchat.h"
#include "../common/pchatc.h"
#include "../common/cfgfiles.h"
#include "../common/text.h"
}

/* te[] and sound_files[] defined in text.c; NUM_XP from textenums.h (included via text.h) */
extern "C" {
extern struct text_event te[];
extern char *sound_files[];
}

wxBEGIN_EVENT_TABLE(PreferencesDialog, wxDialog)
    EVT_BUTTON(wxID_OK, PreferencesDialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, PreferencesDialog::OnCancel)
    EVT_TREE_SEL_CHANGED(ID_PREF_CATEGORY_TREE, PreferencesDialog::OnCategoryChanged)
    EVT_BUTTON(ID_PREF_FONT_BROWSE, PreferencesDialog::OnFontBrowse)
    EVT_BUTTON(ID_PREF_BG_BROWSE, PreferencesDialog::OnBgBrowse)
    EVT_BUTTON(ID_PREF_SOUND_BROWSE, PreferencesDialog::OnSoundBrowse)
    EVT_BUTTON(ID_PREF_SOUND_PLAY, PreferencesDialog::OnSoundPlay)
wxEND_EVENT_TABLE()

PreferencesDialog::PreferencesDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, wxT("Preferences - PChat"),
               wxDefaultPosition, wxSize(700, 520),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    CreateLayout();
    LoadSettings();
    Centre();
}

PreferencesDialog::~PreferencesDialog()
{
}

void PreferencesDialog::CreateLayout()
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* Horizontal split: category tree | settings panel */
    wxBoxSizer *hSizer = new wxBoxSizer(wxHORIZONTAL);

    /* Category tree (left side) */
    m_category_tree = new wxTreeCtrl(this, ID_PREF_CATEGORY_TREE,
                                      wxDefaultPosition, wxSize(160, -1),
                                      wxTR_HAS_BUTTONS | wxTR_SINGLE |
                                      wxTR_HIDE_ROOT | wxTR_NO_LINES);

    wxTreeItemId root = m_category_tree->AddRoot(wxT("Categories"));

    /* Interface */
    wxTreeItemId interface_node = m_category_tree->AppendItem(root,
                                                               wxT("Interface"));
    wxTreeItemId appearance = m_category_tree->AppendItem(interface_node,
                                                           wxT("Appearance"));
    m_category_tree->AppendItem(interface_node, wxT("Input box"));
    m_category_tree->AppendItem(interface_node, wxT("User list"));
    m_category_tree->AppendItem(interface_node, wxT("Channel switcher"));
    m_category_tree->AppendItem(interface_node, wxT("Colors"));

    /* Chatting */
    wxTreeItemId chatting_node = m_category_tree->AppendItem(root,
                                                              wxT("Chatting"));
    m_category_tree->AppendItem(chatting_node, wxT("General"));
    m_category_tree->AppendItem(chatting_node, wxT("Alerts"));
    m_category_tree->AppendItem(chatting_node, wxT("Sounds"));
    m_category_tree->AppendItem(chatting_node, wxT("Logging"));
    m_category_tree->AppendItem(chatting_node, wxT("Advanced"));

    /* Network */
    wxTreeItemId network_node = m_category_tree->AppendItem(root,
                                                             wxT("Network"));
    m_category_tree->AppendItem(network_node, wxT("Network setup"));
    m_category_tree->AppendItem(network_node, wxT("File transfers"));
    m_category_tree->AppendItem(network_node, wxT("Identd"));

    m_category_tree->ExpandAll();

    hSizer->Add(m_category_tree, 0, wxEXPAND | wxRIGHT, 8);

    /* Settings panel (right side) */
    m_settings_panel = new wxPanel(this, wxID_ANY);
    m_settings_sizer = new wxBoxSizer(wxVERTICAL);
    m_settings_panel->SetSizer(m_settings_sizer);

    /* Create all pages */
    wxPanel *appearancePage = CreateAppearancePage(m_settings_panel);
    wxPanel *inputPage = CreateInputBoxPage(m_settings_panel);
    wxPanel *userListPage = CreateUserListPage(m_settings_panel);
    wxPanel *chanSwitcherPage = CreateChannelSwitcherPage(m_settings_panel);
    wxPanel *colorsPage = CreateColorsPage(m_settings_panel);
    wxPanel *chatGeneralPage = CreateChattingGeneralPage(m_settings_panel);
    wxPanel *alertsPage = CreateAlertsPage(m_settings_panel);
    wxPanel *soundsPage = CreateSoundsPage(m_settings_panel);
    wxPanel *loggingPage = CreateLoggingPage(m_settings_panel);
    wxPanel *advancedPage = CreateAdvancedPage(m_settings_panel);
    wxPanel *netSetupPage = CreateNetworkSetupPage(m_settings_panel);
    wxPanel *fileXferPage = CreateFileTransfersPage(m_settings_panel);
    wxPanel *identdPage = CreateIdentdPage(m_settings_panel);

    /* Map tree items to pages (by walking the tree) */
    wxTreeItemIdValue cookie;

    /* Interface children */
    wxTreeItemId item = m_category_tree->GetFirstChild(interface_node, cookie);
    m_pages[item] = appearancePage;                         /* Appearance */
    item = m_category_tree->GetNextChild(interface_node, cookie);
    m_pages[item] = inputPage;                              /* Input box */
    item = m_category_tree->GetNextChild(interface_node, cookie);
    m_pages[item] = userListPage;                           /* User list */
    item = m_category_tree->GetNextChild(interface_node, cookie);
    m_pages[item] = chanSwitcherPage;                       /* Channel switcher */
    item = m_category_tree->GetNextChild(interface_node, cookie);
    m_pages[item] = colorsPage;                             /* Colors */

    /* Chatting children */
    item = m_category_tree->GetFirstChild(chatting_node, cookie);
    m_pages[item] = chatGeneralPage;                        /* General */
    item = m_category_tree->GetNextChild(chatting_node, cookie);
    m_pages[item] = alertsPage;                             /* Alerts */
    item = m_category_tree->GetNextChild(chatting_node, cookie);
    m_pages[item] = soundsPage;                             /* Sounds */
    item = m_category_tree->GetNextChild(chatting_node, cookie);
    m_pages[item] = loggingPage;                            /* Logging */
    item = m_category_tree->GetNextChild(chatting_node, cookie);
    m_pages[item] = advancedPage;                           /* Advanced */

    /* Network children */
    item = m_category_tree->GetFirstChild(network_node, cookie);
    m_pages[item] = netSetupPage;                           /* Network setup */
    item = m_category_tree->GetNextChild(network_node, cookie);
    m_pages[item] = fileXferPage;                           /* File transfers */
    item = m_category_tree->GetNextChild(network_node, cookie);
    m_pages[item] = identdPage;                             /* Identd */

    /* Hide all pages, show Appearance by default */
    for (auto &pair : m_pages) {
        pair.second->Hide();
        m_settings_sizer->Add(pair.second, 1, wxEXPAND);
    }
    m_current_page = appearancePage;
    m_current_page->Show();
    m_category_tree->SelectItem(appearance);

    hSizer->Add(m_settings_panel, 1, wxEXPAND);
    mainSizer->Add(hSizer, 1, wxEXPAND | wxALL, 8);

    /* OK/Cancel buttons */
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, wxID_CANCEL, wxT("Cancel")),
                  0, wxRIGHT, 8);
    btnSizer->Add(new wxButton(this, wxID_OK, wxT("OK")), 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    SetSizer(mainSizer);
}

wxPanel *PreferencesDialog::CreateAppearancePage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    /* Title */
    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("Appearance"));
    wxFont titleFont = title->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleFont.SetPointSize(titleFont.GetPointSize() + 2);
    title->SetFont(titleFont);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* General */
    wxStaticBoxSizer *generalBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                         wxT("General"));
    wxWindow *genParent = generalBox->GetStaticBox();
    wxFlexGridSizer *genGrid = new wxFlexGridSizer(2, 2, 4, 8);
    genGrid->AddGrowableCol(1, 1);

    genGrid->Add(new wxStaticText(genParent, wxID_ANY, wxT("Language:")),
                 0, wxALIGN_CENTER_VERTICAL);
    m_language = new wxChoice(genParent, wxID_ANY);
    /* Matches langsmenu[] in fe-gtk3/setup.c — indices map 1:1
       to the languages[] array in cfgfiles.c (pchat_gui_lang). */
    static const wxChar *const langNames[] = {
        wxT("Afrikaans"), wxT("Albanian"), wxT("Amharic"), wxT("Asturian"),
        wxT("Azerbaijani"), wxT("Basque"), wxT("Belarusian"), wxT("Bulgarian"),
        wxT("Catalan"), wxT("Chinese (Simplified)"), wxT("Chinese (Traditional)"),
        wxT("Czech"), wxT("Danish"), wxT("Dutch"), wxT("English (British)"),
        wxT("English"), wxT("Estonian"), wxT("Finnish"), wxT("French"),
        wxT("Galician"), wxT("German"), wxT("Greek"), wxT("Gujarati"),
        wxT("Hindi"), wxT("Hungarian"), wxT("Indonesian"), wxT("Italian"),
        wxT("Japanese"), wxT("Kannada"), wxT("Kinyarwanda"), wxT("Korean"),
        wxT("Latvian"), wxT("Lithuanian"), wxT("Macedonian"), wxT("Malay"),
        wxT("Malayalam"), wxT("Norwegian (Bokmal)"), wxT("Norwegian (Nynorsk)"),
        wxT("Polish"), wxT("Portuguese"), wxT("Portuguese (Brazilian)"),
        wxT("Punjabi"), wxT("Russian"), wxT("Serbian"), wxT("Slovak"),
        wxT("Slovenian"), wxT("Spanish"), wxT("Swedish"), wxT("Thai"),
        wxT("Turkish"), wxT("Ukrainian"), wxT("Vietnamese"), wxT("Walloon"),
    };
    for (size_t li = 0; li < sizeof(langNames)/sizeof(langNames[0]); li++)
        m_language->Append(langNames[li]);
    genGrid->Add(m_language, 1, wxEXPAND);

    genGrid->Add(new wxStaticText(genParent, wxID_ANY, wxT("Main font:")),
                 0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer *fontSizer = new wxBoxSizer(wxHORIZONTAL);
    m_font_entry = new wxTextCtrl(genParent, wxID_ANY);
    fontSizer->Add(m_font_entry, 1, wxEXPAND | wxRIGHT, 4);
    fontSizer->Add(new wxButton(genParent, ID_PREF_FONT_BROWSE, wxT("Browse...")),
                   0);
    genGrid->Add(fontSizer, 1, wxEXPAND);

    generalBox->Add(genGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(generalBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Text Box */
    wxStaticBoxSizer *textBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Text Box"));
    wxWindow *tbParent = textBox->GetStaticBox();
    wxBoxSizer *row1 = new wxBoxSizer(wxHORIZONTAL);
    m_colored_nicks = new wxCheckBox(tbParent, wxID_ANY,
                                      wxT("Colored nick names"));
    m_indent_nicks = new wxCheckBox(tbParent, wxID_ANY,
                                     wxT("Indent nick names"));
    row1->Add(m_colored_nicks, 0, wxRIGHT, 32);
    row1->Add(m_indent_nicks, 0);
    textBox->Add(row1, 0, wxALL, 4);

    m_show_marker = new wxCheckBox(tbParent, wxID_ANY, wxT("Show marker line"));
    textBox->Add(m_show_marker, 0, wxALL, 4);

    wxBoxSizer *bgSizer = new wxBoxSizer(wxHORIZONTAL);
    bgSizer->Add(new wxStaticText(tbParent, wxID_ANY, wxT("Background image:")),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_bg_image = new wxTextCtrl(tbParent, wxID_ANY);
    bgSizer->Add(m_bg_image, 1, wxEXPAND | wxRIGHT, 4);
    bgSizer->Add(new wxButton(tbParent, ID_PREF_BG_BROWSE, wxT("Browse...")), 0);
    textBox->Add(bgSizer, 0, wxEXPAND | wxALL, 4);

    sizer->Add(textBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Transparency Settings */
    wxStaticBoxSizer *transBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                       wxT("Transparency Settings"));
    wxWindow *transParent = transBox->GetStaticBox();
    wxBoxSizer *opacitySizer = new wxBoxSizer(wxHORIZONTAL);
    opacitySizer->Add(new wxStaticText(transParent, wxID_ANY,
                                        wxT("Window opacity:")),
                      0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_opacity_slider = new wxSlider(transParent, wxID_ANY, 255, 0, 255);
    opacitySizer->Add(m_opacity_slider, 1, wxEXPAND);
    transBox->Add(opacitySizer, 0, wxEXPAND | wxALL, 4);
    sizer->Add(transBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Timestamps */
    wxStaticBoxSizer *stampBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                       wxT("Timestamps"));
    wxWindow *stampParent = stampBox->GetStaticBox();
    m_timestamps = new wxCheckBox(stampParent, wxID_ANY, wxT("Enable timestamps"));
    stampBox->Add(m_timestamps, 0, wxALL, 4);

    wxBoxSizer *fmtSizer = new wxBoxSizer(wxHORIZONTAL);
    fmtSizer->Add(new wxStaticText(stampParent, wxID_ANY,
                                    wxT("Timestamp format:")),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_timestamp_format = new wxTextCtrl(stampParent, wxID_ANY);
    fmtSizer->Add(m_timestamp_format, 1, wxEXPAND);
    stampBox->Add(fmtSizer, 0, wxEXPAND | wxALL, 4);
    sizer->Add(stampBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Title Bar */
    wxStaticBoxSizer *titleBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                       wxT("Title Bar"));
    wxWindow *titleParent = titleBox->GetStaticBox();
    wxBoxSizer *tbRow = new wxBoxSizer(wxHORIZONTAL);
    m_show_chan_modes = new wxCheckBox(titleParent, wxID_ANY,
                                       wxT("Show channel modes"));
    m_show_num_users = new wxCheckBox(titleParent, wxID_ANY,
                                       wxT("Show number of users"));
    m_show_nickname = new wxCheckBox(titleParent, wxID_ANY,
                                      wxT("Show nickname"));
    tbRow->Add(m_show_chan_modes, 0, wxRIGHT, 16);
    tbRow->Add(m_show_num_users, 0, wxRIGHT, 16);
    tbRow->Add(m_show_nickname, 0);
    titleBox->Add(tbRow, 0, wxALL, 4);
    sizer->Add(titleBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateInputBoxPage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("Input Box"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Input Box Appearance */
    wxStaticBoxSizer *appBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Input Box Appearance"));
    m_input_style = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Use the text box font and colors"));
    appBox->Add(m_input_style, 0, wxALL, 4);
    m_input_nick = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Show nick box"));
    appBox->Add(m_input_nick, 0, wxALL, 4);
    m_input_icon = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Show user mode icon in nick box"));
    appBox->Add(m_input_icon, 0, wxALL, 4);
    m_input_attr = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Render colors and attributes"));
    appBox->Add(m_input_attr, 0, wxALL, 4);
    sizer->Add(appBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Spell Checking */
    wxStaticBoxSizer *spellBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                       wxT("Spell Checking"));
    m_input_spell = new wxCheckBox(spellBox->GetStaticBox(), wxID_ANY,
        wxT("Enable spell checking"));
    spellBox->Add(m_input_spell, 0, wxALL, 4);

    wxBoxSizer *langSizer = new wxBoxSizer(wxHORIZONTAL);
    langSizer->Add(new wxStaticText(spellBox->GetStaticBox(), wxID_ANY,
        wxT("Dictionaries:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_spell_langs = new wxTextCtrl(spellBox->GetStaticBox(), wxID_ANY);
    m_spell_langs->SetMaxLength(64);
    langSizer->Add(m_spell_langs, 1, wxEXPAND);
    spellBox->Add(langSizer, 0, wxEXPAND | wxALL, 4);
    spellBox->Add(new wxStaticText(spellBox->GetStaticBox(), wxID_ANY,
        wxT("Separate multiple dictionaries with commas (e.g. en_US,fr)")),
        0, wxLEFT | wxBOTTOM, 4);
    sizer->Add(spellBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Nick Completion */
    wxStaticBoxSizer *compBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Nick Completion"));
    wxFlexGridSizer *compGrid = new wxFlexGridSizer(3, 2, 4, 8);
    compGrid->AddGrowableCol(1, 1);

    compGrid->Add(new wxStaticText(compBox->GetStaticBox(), wxID_ANY,
        wxT("Suffix:")), 0, wxALIGN_CENTER_VERTICAL);
    m_completion_suffix = new wxTextCtrl(compBox->GetStaticBox(), wxID_ANY);
    m_completion_suffix->SetMaxLength(4);
    compGrid->Add(m_completion_suffix, 1, wxEXPAND);

    compGrid->Add(new wxStaticText(compBox->GetStaticBox(), wxID_ANY,
        wxT("Sort:")), 0, wxALIGN_CENTER_VERTICAL);
    m_completion_sort = new wxChoice(compBox->GetStaticBox(), wxID_ANY);
    m_completion_sort->Append(wxT("A-Z"));
    m_completion_sort->Append(wxT("Last-spoke order"));
    compGrid->Add(m_completion_sort, 1, wxEXPAND);

    compGrid->Add(new wxStaticText(compBox->GetStaticBox(), wxID_ANY,
        wxT("Max nicks:")), 0, wxALIGN_CENTER_VERTICAL);
    m_completion_amount = new wxSpinCtrl(compBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 1000);
    compGrid->Add(m_completion_amount, 1, wxEXPAND);

    compBox->Add(compGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(compBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Input Interpretation */
    wxStaticBoxSizer *interpBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                        wxT("Input Interpretation"));
    m_input_perc_ascii = new wxCheckBox(interpBox->GetStaticBox(), wxID_ANY,
        wxT("Interpret %nnn as ASCII value"));
    interpBox->Add(m_input_perc_ascii, 0, wxALL, 4);
    m_input_perc_color = new wxCheckBox(interpBox->GetStaticBox(), wxID_ANY,
        wxT("Interpret %C, %B as Color, Bold etc"));
    interpBox->Add(m_input_perc_color, 0, wxALL, 4);
    sizer->Add(interpBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateUserListPage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("User List"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Appearance */
    wxStaticBoxSizer *appBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Appearance"));
    m_ulist_hide = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Hide user list"));
    appBox->Add(m_ulist_hide, 0, wxALL, 4);
    m_ulist_style = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Use the text box font and colors"));
    appBox->Add(m_ulist_style, 0, wxALL, 4);
    m_ulist_show_hosts = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Show hostnames"));
    appBox->Add(m_ulist_show_hosts, 0, wxALL, 4);
    m_ulist_color = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Color nick names"));
    appBox->Add(m_ulist_color, 0, wxALL, 4);
    m_ulist_count = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Show user count in channels"));
    appBox->Add(m_ulist_count, 0, wxALL, 4);
    m_ulist_icons = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Show mode icons"));
    appBox->Add(m_ulist_icons, 0, wxALL, 4);
    m_ulist_buttons = new wxCheckBox(appBox->GetStaticBox(), wxID_ANY,
        wxT("Show userlist buttons"));
    appBox->Add(m_ulist_buttons, 0, wxALL, 4);

    wxFlexGridSizer *ulGrid = new wxFlexGridSizer(2, 2, 4, 8);
    ulGrid->AddGrowableCol(1, 1);

    ulGrid->Add(new wxStaticText(appBox->GetStaticBox(), wxID_ANY,
        wxT("Sorted by:")), 0, wxALIGN_CENTER_VERTICAL);
    m_ulist_sort = new wxChoice(appBox->GetStaticBox(), wxID_ANY);
    m_ulist_sort->Append(wxT("A-Z, Ops first"));
    m_ulist_sort->Append(wxT("A-Z"));
    m_ulist_sort->Append(wxT("Z-A, Ops last"));
    m_ulist_sort->Append(wxT("Z-A"));
    m_ulist_sort->Append(wxT("Unsorted"));
    ulGrid->Add(m_ulist_sort, 1, wxEXPAND);

    ulGrid->Add(new wxStaticText(appBox->GetStaticBox(), wxID_ANY,
        wxT("Show at:")), 0, wxALIGN_CENTER_VERTICAL);
    m_ulist_pos = new wxChoice(appBox->GetStaticBox(), wxID_ANY);
    m_ulist_pos->Append(wxT("Left (Upper)"));
    m_ulist_pos->Append(wxT("Left (Lower)"));
    m_ulist_pos->Append(wxT("Right (Upper)"));
    m_ulist_pos->Append(wxT("Right (Lower)"));
    ulGrid->Add(m_ulist_pos, 1, wxEXPAND);

    appBox->Add(ulGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(appBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Away Tracking */
    wxStaticBoxSizer *awayBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Away Tracking"));
    m_away_track = new wxCheckBox(awayBox->GetStaticBox(), wxID_ANY,
        wxT("Track Away status of users"));
    awayBox->Add(m_away_track, 0, wxALL, 4);

    wxBoxSizer *awaySzr = new wxBoxSizer(wxHORIZONTAL);
    awaySzr->Add(new wxStaticText(awayBox->GetStaticBox(), wxID_ANY,
        wxT("On channels smaller than:")), 0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_away_size_max = new wxSpinCtrl(awayBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 10000);
    awaySzr->Add(m_away_size_max, 0);
    awayBox->Add(awaySzr, 0, wxALL, 4);
    sizer->Add(awayBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Action */
    wxStaticBoxSizer *actBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Action"));
    wxBoxSizer *dblSzr = new wxBoxSizer(wxHORIZONTAL);
    dblSzr->Add(new wxStaticText(actBox->GetStaticBox(), wxID_ANY,
        wxT("Double-click command:")), 0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_ulist_doubleclick = new wxTextCtrl(actBox->GetStaticBox(), wxID_ANY);
    m_ulist_doubleclick->SetMaxLength(256);
    dblSzr->Add(m_ulist_doubleclick, 1, wxEXPAND);
    actBox->Add(dblSzr, 0, wxEXPAND | wxALL, 4);
    sizer->Add(actBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Meters */
    wxStaticBoxSizer *mtrBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Meters"));
    wxFlexGridSizer *mtrGrid = new wxFlexGridSizer(2, 2, 4, 8);
    mtrGrid->AddGrowableCol(1, 1);

    mtrGrid->Add(new wxStaticText(mtrBox->GetStaticBox(), wxID_ANY,
        wxT("Lag meter:")), 0, wxALIGN_CENTER_VERTICAL);
    m_lagometer = new wxChoice(mtrBox->GetStaticBox(), wxID_ANY);
    m_lagometer->Append(wxT("Off"));
    m_lagometer->Append(wxT("Graphical"));
    m_lagometer->Append(wxT("Text"));
    m_lagometer->Append(wxT("Both"));
    mtrGrid->Add(m_lagometer, 1, wxEXPAND);

    mtrGrid->Add(new wxStaticText(mtrBox->GetStaticBox(), wxID_ANY,
        wxT("Throttle meter:")), 0, wxALIGN_CENTER_VERTICAL);
    m_throttlemeter = new wxChoice(mtrBox->GetStaticBox(), wxID_ANY);
    m_throttlemeter->Append(wxT("Off"));
    m_throttlemeter->Append(wxT("Graphical"));
    m_throttlemeter->Append(wxT("Text"));
    m_throttlemeter->Append(wxT("Both"));
    mtrGrid->Add(m_throttlemeter, 1, wxEXPAND);

    mtrBox->Add(mtrGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(mtrBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateChannelSwitcherPage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY,
                                            wxT("Channel Switcher"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Tabs/Windows */
    wxStaticBoxSizer *tabBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Tabs or Windows"));
    wxFlexGridSizer *tabGrid = new wxFlexGridSizer(3, 2, 4, 8);
    tabGrid->AddGrowableCol(1, 1);

    tabGrid->Add(new wxStaticText(tabBox->GetStaticBox(), wxID_ANY,
        wxT("Open channels in:")), 0, wxALIGN_CENTER_VERTICAL);
    m_tab_chans = new wxChoice(tabBox->GetStaticBox(), wxID_ANY);
    m_tab_chans->Append(wxT("Windows"));
    m_tab_chans->Append(wxT("Tabs"));
    tabGrid->Add(m_tab_chans, 1, wxEXPAND);

    tabGrid->Add(new wxStaticText(tabBox->GetStaticBox(), wxID_ANY,
        wxT("Open dialogs in:")), 0, wxALIGN_CENTER_VERTICAL);
    m_tab_dialogs = new wxChoice(tabBox->GetStaticBox(), wxID_ANY);
    m_tab_dialogs->Append(wxT("Windows"));
    m_tab_dialogs->Append(wxT("Tabs"));
    tabGrid->Add(m_tab_dialogs, 1, wxEXPAND);

    tabGrid->Add(new wxStaticText(tabBox->GetStaticBox(), wxID_ANY,
        wxT("Open utilities in:")), 0, wxALIGN_CENTER_VERTICAL);
    m_tab_utils = new wxChoice(tabBox->GetStaticBox(), wxID_ANY);
    m_tab_utils->Append(wxT("Windows"));
    m_tab_utils->Append(wxT("Tabs"));
    tabGrid->Add(m_tab_utils, 1, wxEXPAND);

    tabBox->Add(tabGrid, 0, wxEXPAND | wxALL, 4);

    /* Switcher type — Tabs vs Tree (matches GTK3 swtype[]) */
    wxBoxSizer *swSizer = new wxBoxSizer(wxHORIZONTAL);
    swSizer->Add(new wxStaticText(tabBox->GetStaticBox(), wxID_ANY,
        wxT("Switcher type:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_tab_layout = new wxChoice(tabBox->GetStaticBox(), wxID_ANY);
    m_tab_layout->Append(wxT("Tabs"));   /* 0 */
    m_tab_layout->Append(wxT("Tree"));   /* maps to backend value 2 */
    swSizer->Add(m_tab_layout, 0);
    tabBox->Add(swSizer, 0, wxALL, 4);

    sizer->Add(tabBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Tab Options */
    wxStaticBoxSizer *optBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Options"));
    m_tab_sort = new wxCheckBox(optBox->GetStaticBox(), wxID_ANY,
        wxT("Sort tabs"));
    optBox->Add(m_tab_sort, 0, wxALL, 4);
    m_tab_icons = new wxCheckBox(optBox->GetStaticBox(), wxID_ANY,
        wxT("Show icons in tabs"));
    optBox->Add(m_tab_icons, 0, wxALL, 4);
    m_tab_server = new wxCheckBox(optBox->GetStaticBox(), wxID_ANY,
        wxT("Open extra tab for server messages"));
    optBox->Add(m_tab_server, 0, wxALL, 4);
    m_tab_autoopen_dialog = new wxCheckBox(optBox->GetStaticBox(), wxID_ANY,
        wxT("Open new tab on private message"));
    optBox->Add(m_tab_autoopen_dialog, 0, wxALL, 4);
    m_tab_dots = new wxCheckBox(optBox->GetStaticBox(), wxID_ANY,
        wxT("Show dotted lines in channel tree"));
    optBox->Add(m_tab_dots, 0, wxALL, 4);
    m_tab_scrollchans = new wxCheckBox(optBox->GetStaticBox(), wxID_ANY,
        wxT("Scroll mouse-wheel to change tabs"));
    optBox->Add(m_tab_scrollchans, 0, wxALL, 4);
    m_tab_middleclose = new wxCheckBox(optBox->GetStaticBox(), wxID_ANY,
        wxT("Middle click to close tab"));
    optBox->Add(m_tab_middleclose, 0, wxALL, 4);
    m_tab_small = new wxCheckBox(optBox->GetStaticBox(), wxID_ANY,
        wxT("Smaller text"));
    optBox->Add(m_tab_small, 0, wxALL, 4);
    sizer->Add(optBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Position & Behavior */
    wxStaticBoxSizer *posBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Position && Behavior"));
    wxFlexGridSizer *posGrid = new wxFlexGridSizer(4, 2, 4, 8);
    posGrid->AddGrowableCol(1, 1);

    posGrid->Add(new wxStaticText(posBox->GetStaticBox(), wxID_ANY,
        wxT("Focus new tabs:")), 0, wxALIGN_CENTER_VERTICAL);
    m_tab_newtofront = new wxChoice(posBox->GetStaticBox(), wxID_ANY);
    m_tab_newtofront->Append(wxT("Never"));
    m_tab_newtofront->Append(wxT("Always"));
    m_tab_newtofront->Append(wxT("Only requested tabs"));
    posGrid->Add(m_tab_newtofront, 1, wxEXPAND);

    posGrid->Add(new wxStaticText(posBox->GetStaticBox(), wxID_ANY,
        wxT("Placement of notices:")), 0, wxALIGN_CENTER_VERTICAL);
    m_notice_pos = new wxChoice(posBox->GetStaticBox(), wxID_ANY);
    m_notice_pos->Append(wxT("Automatic"));
    m_notice_pos->Append(wxT("In an extra tab"));
    m_notice_pos->Append(wxT("In the front tab"));
    posGrid->Add(m_notice_pos, 1, wxEXPAND);

    posGrid->Add(new wxStaticText(posBox->GetStaticBox(), wxID_ANY,
        wxT("Show switcher at:")), 0, wxALIGN_CENTER_VERTICAL);
    m_tab_pos = new wxChoice(posBox->GetStaticBox(), wxID_ANY);
    m_tab_pos->Append(wxT("Left (Upper)"));   /* 0 */
    m_tab_pos->Append(wxT("Left (Lower)"));   /* 1 */
    m_tab_pos->Append(wxT("Right (Upper)"));  /* 2 */
    m_tab_pos->Append(wxT("Right (Lower)"));  /* 3 */
    m_tab_pos->Append(wxT("Top"));            /* 4 */
    m_tab_pos->Append(wxT("Bottom"));         /* 5 */
    m_tab_pos->Append(wxT("Hidden"));         /* 6 */
    posGrid->Add(m_tab_pos, 1, wxEXPAND);

    posGrid->Add(new wxStaticText(posBox->GetStaticBox(), wxID_ANY,
        wxT("Shorten labels to:")), 0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer *truncSzr = new wxBoxSizer(wxHORIZONTAL);
    m_tab_trunc = new wxSpinCtrl(posBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 99);
    truncSzr->Add(m_tab_trunc, 0, wxRIGHT, 4);
    truncSzr->Add(new wxStaticText(posBox->GetStaticBox(), wxID_ANY,
        wxT("letters")), 0, wxALIGN_CENTER_VERTICAL);
    posGrid->Add(truncSzr, 1, wxEXPAND);

    posBox->Add(posGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(posBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateColorsPage(wxWindow *parent)
{
    wxScrolledWindow *page = new wxScrolledWindow(parent, wxID_ANY);
    page->SetScrollRate(0, 10);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("Colors"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    m_colors_changed = false;

    /* Helper to create a color swatch button */
    auto makeColorBtn = [&](wxWindow *par, int idx) -> wxButton* {
        wxButton *btn = new wxButton(par, wxID_ANY, wxEmptyString,
            wxDefaultPosition, wxSize(28, 22));
        btn->SetBackgroundColour(wx_palette_get(idx));
        m_color_buttons[idx] = btn;

        btn->Bind(wxEVT_BUTTON, [this, idx](wxCommandEvent &) {
            wxColourData cd;
            cd.SetColour(wx_palette_get(idx));
            wxColourDialog dlg(this, &cd);
            if (dlg.ShowModal() == wxID_OK) {
                wxColour c = dlg.GetColourData().GetColour();
                wx_palette_set(idx, c);
                m_color_buttons[idx]->SetBackgroundColour(c);
                m_color_buttons[idx]->Refresh();
                m_colors_changed = true;
            }
        });
        return btn;
    };

    /* --- mIRC Colors (0-15) --- */
    wxStaticBoxSizer *mircBox = new wxStaticBoxSizer(wxVERTICAL, page,
        wxT("mIRC Colors"));
    wxGridSizer *mircGrid = new wxGridSizer(2, 8, 2, 2);
    for (int i = 0; i < 16; i++)
        mircGrid->Add(makeColorBtn(mircBox->GetStaticBox(), i), 0, wxEXPAND);
    mircBox->Add(mircGrid, 0, wxALL, 4);
    sizer->Add(mircBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* --- Local Colors (16-31) --- */
    wxStaticBoxSizer *localBox = new wxStaticBoxSizer(wxVERTICAL, page,
        wxT("Local Colors"));
    wxGridSizer *localGrid = new wxGridSizer(2, 8, 2, 2);
    for (int i = 16; i < 32; i++)
        localGrid->Add(makeColorBtn(localBox->GetStaticBox(), i), 0, wxEXPAND);
    localBox->Add(localGrid, 0, wxALL, 4);
    sizer->Add(localBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* --- Special Colors --- */
    wxStaticBoxSizer *specBox = new wxStaticBoxSizer(wxVERTICAL, page,
        wxT("Interface Colors"));
    wxFlexGridSizer *specGrid = new wxFlexGridSizer(5, 4, 4, 12);

    struct { int idx; const char *label; } specials[] = {
        { COL_FG,       "Text Foreground" },
        { COL_BG,       "Text Background" },
        { COL_MARK_FG,  "Selected FG" },
        { COL_MARK_BG,  "Selected BG" },
        { COL_MARKER,   "Marker Line" },
        { COL_NEW_DATA, "New Data Tab" },
        { COL_HILIGHT,  "Highlight Tab" },
        { COL_NEW_MSG,  "New Message Tab" },
        { COL_AWAY,     "Away User" },
        { COL_SPELL,    "Spell Checker" },
    };
    for (auto &sp : specials) {
        specGrid->Add(new wxStaticText(specBox->GetStaticBox(), wxID_ANY,
            wxString::FromUTF8(sp.label)), 0, wxALIGN_CENTER_VERTICAL);
        specGrid->Add(makeColorBtn(specBox->GetStaticBox(), sp.idx),
            0, wxALIGN_CENTER_VERTICAL);
    }
    specBox->Add(specGrid, 0, wxALL, 4);
    sizer->Add(specBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Strip Colors — these are regular prefs and work fine */
    wxStaticBoxSizer *stripBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                       wxT("Strip Colors From"));
    m_strip_msg = new wxCheckBox(stripBox->GetStaticBox(), wxID_ANY,
        wxT("Messages"));
    stripBox->Add(m_strip_msg, 0, wxALL, 4);
    m_strip_replay = new wxCheckBox(stripBox->GetStaticBox(), wxID_ANY,
        wxT("Scrollback"));
    stripBox->Add(m_strip_replay, 0, wxALL, 4);
    m_strip_topic = new wxCheckBox(stripBox->GetStaticBox(), wxID_ANY,
        wxT("Topic"));
    stripBox->Add(m_strip_topic, 0, wxALL, 4);
    sizer->Add(stripBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateChattingGeneralPage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("General"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Default Messages */
    wxStaticBoxSizer *msgBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Default Messages"));
    wxFlexGridSizer *msgGrid = new wxFlexGridSizer(3, 2, 4, 8);
    msgGrid->AddGrowableCol(1, 1);

    msgGrid->Add(new wxStaticText(msgBox->GetStaticBox(), wxID_ANY,
        wxT("Quit:")), 0, wxALIGN_CENTER_VERTICAL);
    m_quit_reason = new wxTextCtrl(msgBox->GetStaticBox(), wxID_ANY);
    m_quit_reason->SetMaxLength(256);
    msgGrid->Add(m_quit_reason, 1, wxEXPAND);

    msgGrid->Add(new wxStaticText(msgBox->GetStaticBox(), wxID_ANY,
        wxT("Leave channel:")), 0, wxALIGN_CENTER_VERTICAL);
    m_part_reason = new wxTextCtrl(msgBox->GetStaticBox(), wxID_ANY);
    m_part_reason->SetMaxLength(256);
    msgGrid->Add(m_part_reason, 1, wxEXPAND);

    msgGrid->Add(new wxStaticText(msgBox->GetStaticBox(), wxID_ANY,
        wxT("Away:")), 0, wxALIGN_CENTER_VERTICAL);
    m_away_reason = new wxTextCtrl(msgBox->GetStaticBox(), wxID_ANY);
    m_away_reason->SetMaxLength(256);
    msgGrid->Add(m_away_reason, 1, wxEXPAND);

    msgBox->Add(msgGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(msgBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Away */
    wxStaticBoxSizer *awayBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Away"));
    m_away_show_once = new wxCheckBox(awayBox->GetStaticBox(), wxID_ANY,
        wxT("Show away messages only once"));
    awayBox->Add(m_away_show_once, 0, wxALL, 4);
    m_away_auto_unmark = new wxCheckBox(awayBox->GetStaticBox(), wxID_ANY,
        wxT("Automatically unmark away on activity"));
    awayBox->Add(m_away_auto_unmark, 0, wxALL, 4);
    sizer->Add(awayBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Display */
    wxStaticBoxSizer *dispBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Display"));
    m_irc_raw_modes = new wxCheckBox(dispBox->GetStaticBox(), wxID_ANY,
        wxT("Display MODEs in raw form"));
    dispBox->Add(m_irc_raw_modes, 0, wxALL, 4);
    m_notify_whois = new wxCheckBox(dispBox->GetStaticBox(), wxID_ANY,
        wxT("WHOIS on notify"));
    dispBox->Add(m_notify_whois, 0, wxALL, 4);
    m_irc_conf_mode = new wxCheckBox(dispBox->GetStaticBox(), wxID_ANY,
        wxT("Hide join and part messages"));
    dispBox->Add(m_irc_conf_mode, 0, wxALL, 4);
    m_irc_hide_nickchange = new wxCheckBox(dispBox->GetStaticBox(), wxID_ANY,
        wxT("Hide nick change messages"));
    dispBox->Add(m_irc_hide_nickchange, 0, wxALL, 4);
    sizer->Add(dispBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateAlertsPage(wxWindow *parent)
{
    wxScrolledWindow *page = new wxScrolledWindow(parent, wxID_ANY);
    page->SetScrollRate(0, 10);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("Alerts"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Alert grid: each row = alert type, 3 columns = Chan/Priv/Hilight */
    wxStaticBoxSizer *alertBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                       wxT("Alerts"));
    wxFlexGridSizer *alertGrid = new wxFlexGridSizer(5, 4, 4, 8);

    /* Column headers */
    alertGrid->Add(new wxStaticText(alertBox->GetStaticBox(), wxID_ANY,
        wxT("")), 0);
    wxStaticText *h1 = new wxStaticText(alertBox->GetStaticBox(), wxID_ANY,
        wxT("Channel"));
    h1->SetFont(h1->GetFont().Bold());
    alertGrid->Add(h1, 0, wxALIGN_CENTER);
    wxStaticText *h2 = new wxStaticText(alertBox->GetStaticBox(), wxID_ANY,
        wxT("Private"));
    h2->SetFont(h2->GetFont().Bold());
    alertGrid->Add(h2, 0, wxALIGN_CENTER);
    wxStaticText *h3 = new wxStaticText(alertBox->GetStaticBox(), wxID_ANY,
        wxT("Highlight"));
    h3->SetFont(h3->GetFont().Bold());
    alertGrid->Add(h3, 0, wxALIGN_CENTER);

    /* Tray balloons */
    alertGrid->Add(new wxStaticText(alertBox->GetStaticBox(), wxID_ANY,
        wxT("Show tray balloons:")), 0, wxALIGN_CENTER_VERTICAL);
    m_balloon_chans = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_balloon_chans, 0, wxALIGN_CENTER);
    m_balloon_priv = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_balloon_priv, 0, wxALIGN_CENTER);
    m_balloon_hilight = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_balloon_hilight, 0, wxALIGN_CENTER);

    /* Blink tray */
    alertGrid->Add(new wxStaticText(alertBox->GetStaticBox(), wxID_ANY,
        wxT("Blink tray icon:")), 0, wxALIGN_CENTER_VERTICAL);
    m_tray_chans = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_tray_chans, 0, wxALIGN_CENTER);
    m_tray_priv = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_tray_priv, 0, wxALIGN_CENTER);
    m_tray_hilight = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_tray_hilight, 0, wxALIGN_CENTER);

    /* Flash taskbar */
    alertGrid->Add(new wxStaticText(alertBox->GetStaticBox(), wxID_ANY,
        wxT("Blink task bar:")), 0, wxALIGN_CENTER_VERTICAL);
    m_flash_chans = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_flash_chans, 0, wxALIGN_CENTER);
    m_flash_priv = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_flash_priv, 0, wxALIGN_CENTER);
    m_flash_hilight = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_flash_hilight, 0, wxALIGN_CENTER);

    /* Beep */
    alertGrid->Add(new wxStaticText(alertBox->GetStaticBox(), wxID_ANY,
        wxT("Make a beep sound:")), 0, wxALIGN_CENTER_VERTICAL);
    m_beep_chans = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_beep_chans, 0, wxALIGN_CENTER);
    m_beep_priv = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_beep_priv, 0, wxALIGN_CENTER);
    m_beep_hilight = new wxCheckBox(alertBox->GetStaticBox(), wxID_ANY, wxT(""));
    alertGrid->Add(m_beep_hilight, 0, wxALIGN_CENTER);

    alertBox->Add(alertGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(alertBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Omit Alerts */
    wxStaticBoxSizer *omitBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Omit Alerts"));
    m_away_omit_alerts = new wxCheckBox(omitBox->GetStaticBox(), wxID_ANY,
        wxT("Omit alerts when away"));
    omitBox->Add(m_away_omit_alerts, 0, wxALL, 4);
    m_focus_omitalerts = new wxCheckBox(omitBox->GetStaticBox(), wxID_ANY,
        wxT("Omit alerts while window is focused"));
    omitBox->Add(m_focus_omitalerts, 0, wxALL, 4);
    sizer->Add(omitBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* System Tray */
    wxStaticBoxSizer *trayBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("System Tray"));
    m_gui_tray = new wxCheckBox(trayBox->GetStaticBox(), wxID_ANY,
        wxT("Enable system tray icon"));
    trayBox->Add(m_gui_tray, 0, wxALL, 4);
    m_tray_minimize = new wxCheckBox(trayBox->GetStaticBox(), wxID_ANY,
        wxT("Minimize to tray"));
    trayBox->Add(m_tray_minimize, 0, wxALL, 4);
    m_tray_close = new wxCheckBox(trayBox->GetStaticBox(), wxID_ANY,
        wxT("Close to tray"));
    trayBox->Add(m_tray_close, 0, wxALL, 4);
    m_tray_away = new wxCheckBox(trayBox->GetStaticBox(), wxID_ANY,
        wxT("Automatically mark away/back"));
    trayBox->Add(m_tray_away, 0, wxALL, 4);
    m_tray_quiet = new wxCheckBox(trayBox->GetStaticBox(), wxID_ANY,
        wxT("Only show tray balloons when hidden/minimized"));
    trayBox->Add(m_tray_quiet, 0, wxALL, 4);
    sizer->Add(trayBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Highlighted Messages */
    wxStaticBoxSizer *hilBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Highlighted Messages"));
    wxFlexGridSizer *hilGrid = new wxFlexGridSizer(3, 2, 4, 8);
    hilGrid->AddGrowableCol(1, 1);

    hilGrid->Add(new wxStaticText(hilBox->GetStaticBox(), wxID_ANY,
        wxT("Extra words to highlight:")), 0, wxALIGN_CENTER_VERTICAL);
    m_extra_hilight = new wxTextCtrl(hilBox->GetStaticBox(), wxID_ANY);
    m_extra_hilight->SetMaxLength(300);
    hilGrid->Add(m_extra_hilight, 1, wxEXPAND);

    hilGrid->Add(new wxStaticText(hilBox->GetStaticBox(), wxID_ANY,
        wxT("Nicks not to highlight:")), 0, wxALIGN_CENTER_VERTICAL);
    m_no_hilight = new wxTextCtrl(hilBox->GetStaticBox(), wxID_ANY);
    m_no_hilight->SetMaxLength(300);
    hilGrid->Add(m_no_hilight, 1, wxEXPAND);

    hilGrid->Add(new wxStaticText(hilBox->GetStaticBox(), wxID_ANY,
        wxT("Nicks to always highlight:")), 0, wxALIGN_CENTER_VERTICAL);
    m_nick_hilight = new wxTextCtrl(hilBox->GetStaticBox(), wxID_ANY);
    m_nick_hilight->SetMaxLength(300);
    hilGrid->Add(m_nick_hilight, 1, wxEXPAND);

    hilBox->Add(hilGrid, 0, wxEXPAND | wxALL, 4);
    hilBox->Add(new wxStaticText(hilBox->GetStaticBox(), wxID_ANY,
        wxT("Separate multiple words with commas.")),
        0, wxLEFT | wxBOTTOM, 4);
    sizer->Add(hilBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateSoundsPage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("Sounds"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Sound events list — populated in LoadSettings from te[] */
    m_sound_list = new wxListCtrl(page, wxID_ANY, wxDefaultPosition,
        wxSize(-1, 200), wxLC_REPORT | wxLC_SINGLE_SEL);
    m_sound_list->InsertColumn(0, wxT("Event"), wxLIST_FORMAT_LEFT, 200);
    m_sound_list->InsertColumn(1, wxT("Sound File"), wxLIST_FORMAT_LEFT, 250);
    sizer->Add(m_sound_list, 1, wxEXPAND | wxBOTTOM, 4);

    /* When a row is selected, update the file entry */
    m_sound_list->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent &evt) {
        int idx = evt.GetIndex();
        wxString sf = m_sound_list->GetItemText(idx, 1);
        m_sound_file->SetValue(sf);
    });

    /* Sound file entry + Browse + Play */
    wxBoxSizer *sfSizer = new wxBoxSizer(wxHORIZONTAL);
    sfSizer->Add(new wxStaticText(page, wxID_ANY, wxT("Sound file:")),
        0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    m_sound_file = new wxTextCtrl(page, wxID_ANY);
    sfSizer->Add(m_sound_file, 1, wxEXPAND | wxRIGHT, 4);

    wxButton *browseBtn = new wxButton(page, ID_PREF_SOUND_BROWSE,
                                        wxT("Browse..."));
    sfSizer->Add(browseBtn, 0, wxRIGHT, 4);

    wxButton *playBtn = new wxButton(page, ID_PREF_SOUND_PLAY, wxT("Play"));
    sfSizer->Add(playBtn, 0);

    /* Apply button — set the current file entry into the selected row */
    wxButton *applyBtn = new wxButton(page, wxID_ANY, wxT("Apply"));
    sfSizer->Add(applyBtn, 0, wxLEFT, 4);
    applyBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        long sel = m_sound_list->GetNextItem(-1, wxLIST_NEXT_ALL,
                                              wxLIST_STATE_SELECTED);
        if (sel >= 0) {
            m_sound_list->SetItem(sel, 1, m_sound_file->GetValue());
        }
    });

    sizer->Add(sfSizer, 0, wxEXPAND | wxALL, 4);

    sizer->Add(new wxStaticText(page, wxID_ANY,
        wxT("Select an event, enter or browse a sound file, then click Apply.")),
        0, wxLEFT, 4);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateLoggingPage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("Logging"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Logging */
    wxStaticBoxSizer *logBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Logging"));
    m_logging = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY,
        wxT("Enable logging of conversations to disk"));
    logBox->Add(m_logging, 0, wxALL, 4);
    m_log_timestamps = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY,
        wxT("Insert timestamps in logs"));
    logBox->Add(m_log_timestamps, 0, wxALL, 4);

    wxFlexGridSizer *logGrid = new wxFlexGridSizer(2, 2, 4, 8);
    logGrid->AddGrowableCol(1, 1);

    logGrid->Add(new wxStaticText(logBox->GetStaticBox(), wxID_ANY,
        wxT("Log filename:")), 0, wxALIGN_CENTER_VERTICAL);
    m_logmask = new wxTextCtrl(logBox->GetStaticBox(), wxID_ANY);
    m_logmask->SetMaxLength(256);
    logGrid->Add(m_logmask, 1, wxEXPAND);

    logGrid->Add(new wxStaticText(logBox->GetStaticBox(), wxID_ANY,
        wxT("Log timestamp format:")), 0, wxALIGN_CENTER_VERTICAL);
    m_log_timestamp_fmt = new wxTextCtrl(logBox->GetStaticBox(), wxID_ANY);
    m_log_timestamp_fmt->SetMaxLength(64);
    logGrid->Add(m_log_timestamp_fmt, 1, wxEXPAND);

    logBox->Add(logGrid, 0, wxEXPAND | wxALL, 4);

    logBox->Add(new wxStaticText(logBox->GetStaticBox(), wxID_ANY,
        wxT("%s=Server %c=Channel %n=Network. See strftime docs for timestamp.")),
        0, wxLEFT | wxBOTTOM, 4);
    sizer->Add(logBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Scrollback */
    wxStaticBoxSizer *sbBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                    wxT("Scrollback"));
    m_text_replay = new wxCheckBox(sbBox->GetStaticBox(), wxID_ANY,
        wxT("Display scrollback from previous session"));
    sbBox->Add(m_text_replay, 0, wxALL, 4);

    wxBoxSizer *linesSzr = new wxBoxSizer(wxHORIZONTAL);
    linesSzr->Add(new wxStaticText(sbBox->GetStaticBox(), wxID_ANY,
        wxT("Scrollback lines:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_text_max_lines = new wxSpinCtrl(sbBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 100000);
    linesSzr->Add(m_text_max_lines, 0);
    sbBox->Add(linesSzr, 0, wxALL, 4);
    sizer->Add(sbBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* URL Grabber */
    wxStaticBoxSizer *urlBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("URL Grabber"));
    m_url_logging = new wxCheckBox(urlBox->GetStaticBox(), wxID_ANY,
        wxT("Enable logging of URLs to disk"));
    urlBox->Add(m_url_logging, 0, wxALL, 4);
    m_url_grabber = new wxCheckBox(urlBox->GetStaticBox(), wxID_ANY,
        wxT("Enable URL grabber"));
    urlBox->Add(m_url_grabber, 0, wxALL, 4);

    wxBoxSizer *urlLimitSzr = new wxBoxSizer(wxHORIZONTAL);
    urlLimitSzr->Add(new wxStaticText(urlBox->GetStaticBox(), wxID_ANY,
        wxT("Maximum URLs to grab:")), 0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_url_grabber_limit = new wxSpinCtrl(urlBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 9999);
    urlLimitSzr->Add(m_url_grabber_limit, 0);
    urlBox->Add(urlLimitSzr, 0, wxALL, 4);
    sizer->Add(urlBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateAdvancedPage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("Advanced"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Clipboard */
    wxStaticBoxSizer *clipBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Clipboard"));
    m_autocopy_text = new wxCheckBox(clipBox->GetStaticBox(), wxID_ANY,
        wxT("Auto copy selected text"));
    clipBox->Add(m_autocopy_text, 0, wxALL, 4);
    m_autocopy_stamp = new wxCheckBox(clipBox->GetStaticBox(), wxID_ANY,
        wxT("Auto include timestamps in copy"));
    clipBox->Add(m_autocopy_stamp, 0, wxALL, 4);
    m_autocopy_color = new wxCheckBox(clipBox->GetStaticBox(), wxID_ANY,
        wxT("Auto include color info in copy"));
    clipBox->Add(m_autocopy_color, 0, wxALL, 4);
    sizer->Add(clipBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Identity */
    wxStaticBoxSizer *idBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                    wxT("Identity"));
    wxFlexGridSizer *idGrid = new wxFlexGridSizer(2, 2, 4, 8);
    idGrid->AddGrowableCol(1, 1);

    idGrid->Add(new wxStaticText(idBox->GetStaticBox(), wxID_ANY,
        wxT("Real name:")), 0, wxALIGN_CENTER_VERTICAL);
    m_real_name = new wxTextCtrl(idBox->GetStaticBox(), wxID_ANY);
    m_real_name->SetMaxLength(127);
    idGrid->Add(m_real_name, 1, wxEXPAND);

    idGrid->Add(new wxStaticText(idBox->GetStaticBox(), wxID_ANY,
        wxT("Alt. fonts:")), 0, wxALIGN_CENTER_VERTICAL);
    m_font_alternative = new wxTextCtrl(idBox->GetStaticBox(), wxID_ANY);
    idGrid->Add(m_font_alternative, 1, wxEXPAND);

    idBox->Add(idGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(idBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Miscellaneous */
    wxStaticBoxSizer *miscBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Miscellaneous"));
    m_gui_compact = new wxCheckBox(miscBox->GetStaticBox(), wxID_ANY,
        wxT("Display lists in compact mode"));
    miscBox->Add(m_gui_compact, 0, wxALL, 4);
    m_cap_server_time = new wxCheckBox(miscBox->GetStaticBox(), wxID_ANY,
        wxT("Use server time if supported"));
    miscBox->Add(m_cap_server_time, 0, wxALL, 4);
    sizer->Add(miscBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Reconnection */
    wxStaticBoxSizer *reconBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                       wxT("Reconnection"));
    m_auto_reconnect = new wxCheckBox(reconBox->GetStaticBox(), wxID_ANY,
        wxT("Auto reconnect on disconnect"));
    reconBox->Add(m_auto_reconnect, 0, wxALL, 4);

    wxFlexGridSizer *reconGrid = new wxFlexGridSizer(3, 2, 4, 8);
    reconGrid->AddGrowableCol(1, 1);

    reconGrid->Add(new wxStaticText(reconBox->GetStaticBox(), wxID_ANY,
        wxT("Reconnect delay (sec):")), 0, wxALIGN_CENTER_VERTICAL);
    m_reconnect_delay = new wxSpinCtrl(reconBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 9999);
    reconGrid->Add(m_reconnect_delay, 0);

    reconGrid->Add(new wxStaticText(reconBox->GetStaticBox(), wxID_ANY,
        wxT("Auto join delay (sec):")), 0, wxALIGN_CENTER_VERTICAL);
    m_join_delay = new wxSpinCtrl(reconBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 9999);
    reconGrid->Add(m_join_delay, 0);

    reconGrid->Add(new wxStaticText(reconBox->GetStaticBox(), wxID_ANY,
        wxT("Ban type:")), 0, wxALIGN_CENTER_VERTICAL);
    m_ban_type = new wxChoice(reconBox->GetStaticBox(), wxID_ANY);
    m_ban_type->Append(wxT("*!*@*.host"));
    m_ban_type->Append(wxT("*!*@domain"));
    m_ban_type->Append(wxT("*!*user@*.host"));
    m_ban_type->Append(wxT("*!*user@domain"));
    reconGrid->Add(m_ban_type, 0, wxEXPAND);

    reconBox->Add(reconGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(reconBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateNetworkSetupPage(wxWindow *parent)
{
    wxScrolledWindow *page = new wxScrolledWindow(parent, wxID_ANY);
    page->SetScrollRate(0, 10);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY,
                                            wxT("Network Setup"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Your Address */
    wxStaticBoxSizer *addrBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Your Address"));
    wxFlexGridSizer *addrGrid = new wxFlexGridSizer(2, 2, 4, 8);
    addrGrid->AddGrowableCol(1, 1);

    addrGrid->Add(new wxStaticText(addrBox->GetStaticBox(), wxID_ANY,
        wxT("Bind to:")), 0, wxALIGN_CENTER_VERTICAL);
    m_bind_host = new wxTextCtrl(addrBox->GetStaticBox(), wxID_ANY);
    m_bind_host->SetMaxLength(127);
    addrGrid->Add(m_bind_host, 1, wxEXPAND);

    addrGrid->Add(new wxStaticText(addrBox->GetStaticBox(), wxID_ANY,
        wxT("DCC IP address:")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_ip = new wxTextCtrl(addrBox->GetStaticBox(), wxID_ANY);
    addrGrid->Add(m_dcc_ip, 1, wxEXPAND);

    addrBox->Add(addrGrid, 0, wxEXPAND | wxALL, 4);
    addrBox->Add(new wxStaticText(addrBox->GetStaticBox(), wxID_ANY,
        wxT("Only useful for binding to a specific NIC/address.")),
        0, wxLEFT | wxBOTTOM, 4);

    m_dcc_ip_from_server = new wxCheckBox(addrBox->GetStaticBox(), wxID_ANY,
        wxT("Get DCC IP address from IRC server"));
    addrBox->Add(m_dcc_ip_from_server, 0, wxALL, 4);
    sizer->Add(addrBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* DCC Ports */
    wxStaticBoxSizer *portBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("DCC Ports"));
    wxFlexGridSizer *portGrid = new wxFlexGridSizer(2, 2, 4, 8);

    portGrid->Add(new wxStaticText(portBox->GetStaticBox(), wxID_ANY,
        wxT("First DCC send port:")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_port_first = new wxSpinCtrl(portBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 65535);
    portGrid->Add(m_dcc_port_first, 0);

    portGrid->Add(new wxStaticText(portBox->GetStaticBox(), wxID_ANY,
        wxT("Last DCC send port:")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_port_last = new wxSpinCtrl(portBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 65535);
    portGrid->Add(m_dcc_port_last, 0);

    portBox->Add(portGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(portBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Proxy Server */
    wxStaticBoxSizer *proxyBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                       wxT("Proxy Server"));
    wxFlexGridSizer *proxyGrid = new wxFlexGridSizer(6, 2, 4, 8);
    proxyGrid->AddGrowableCol(1, 1);

    proxyGrid->Add(new wxStaticText(proxyBox->GetStaticBox(), wxID_ANY,
        wxT("Type:")), 0, wxALIGN_CENTER_VERTICAL);
    m_proxy_type = new wxChoice(proxyBox->GetStaticBox(), wxID_ANY);
    m_proxy_type->Append(wxT("(disabled)"));
    m_proxy_type->Append(wxT("Wingate"));
    m_proxy_type->Append(wxT("SOCKS4"));
    m_proxy_type->Append(wxT("SOCKS5"));
    m_proxy_type->Append(wxT("HTTP"));
    proxyGrid->Add(m_proxy_type, 1, wxEXPAND);

    proxyGrid->Add(new wxStaticText(proxyBox->GetStaticBox(), wxID_ANY,
        wxT("Hostname:")), 0, wxALIGN_CENTER_VERTICAL);
    m_proxy_host = new wxTextCtrl(proxyBox->GetStaticBox(), wxID_ANY);
    m_proxy_host->SetMaxLength(64);
    proxyGrid->Add(m_proxy_host, 1, wxEXPAND);

    proxyGrid->Add(new wxStaticText(proxyBox->GetStaticBox(), wxID_ANY,
        wxT("Port:")), 0, wxALIGN_CENTER_VERTICAL);
    m_proxy_port = new wxSpinCtrl(proxyBox->GetStaticBox(), wxID_ANY,
        wxT("3128"), wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 1, 65535);
    proxyGrid->Add(m_proxy_port, 0);

    proxyGrid->Add(new wxStaticText(proxyBox->GetStaticBox(), wxID_ANY,
        wxT("Use for:")), 0, wxALIGN_CENTER_VERTICAL);
    m_proxy_use = new wxChoice(proxyBox->GetStaticBox(), wxID_ANY);
    m_proxy_use->Append(wxT("All Connections"));
    m_proxy_use->Append(wxT("IRC Server Only"));
    m_proxy_use->Append(wxT("DCC Get Only"));
    proxyGrid->Add(m_proxy_use, 1, wxEXPAND);

    proxyGrid->Add(new wxStaticText(proxyBox->GetStaticBox(), wxID_ANY,
        wxT("Username:")), 0, wxALIGN_CENTER_VERTICAL);
    m_proxy_user = new wxTextCtrl(proxyBox->GetStaticBox(), wxID_ANY);
    m_proxy_user->SetMaxLength(256);
    proxyGrid->Add(m_proxy_user, 1, wxEXPAND);

    proxyGrid->Add(new wxStaticText(proxyBox->GetStaticBox(), wxID_ANY,
        wxT("Password:")), 0, wxALIGN_CENTER_VERTICAL);
    m_proxy_pass = new wxTextCtrl(proxyBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    m_proxy_pass->SetMaxLength(256);
    proxyGrid->Add(m_proxy_pass, 1, wxEXPAND);

    proxyBox->Add(proxyGrid, 0, wxEXPAND | wxALL, 4);

    m_proxy_auth = new wxCheckBox(proxyBox->GetStaticBox(), wxID_ANY,
        wxT("Use proxy authentication (HTTP/SOCKS5)"));
    proxyBox->Add(m_proxy_auth, 0, wxALL, 4);

    sizer->Add(proxyBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateFileTransfersPage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY,
                                            wxT("File Transfers"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    /* Receiving Files */
    wxStaticBoxSizer *recvBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Receiving Files"));
    wxFlexGridSizer *recvGrid = new wxFlexGridSizer(4, 2, 4, 8);
    recvGrid->AddGrowableCol(1, 1);

    recvGrid->Add(new wxStaticText(recvBox->GetStaticBox(), wxID_ANY,
        wxT("Auto accept:")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_auto_recv = new wxChoice(recvBox->GetStaticBox(), wxID_ANY);
    m_dcc_auto_recv->Append(wxT("Ask for confirmation"));
    m_dcc_auto_recv->Append(wxT("Ask for download folder"));
    m_dcc_auto_recv->Append(wxT("Save without interaction"));
    recvGrid->Add(m_dcc_auto_recv, 1, wxEXPAND);

    recvGrid->Add(new wxStaticText(recvBox->GetStaticBox(), wxID_ANY,
        wxT("Download to:")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_dir = new wxTextCtrl(recvBox->GetStaticBox(), wxID_ANY);
    recvGrid->Add(m_dcc_dir, 1, wxEXPAND);

    recvGrid->Add(new wxStaticText(recvBox->GetStaticBox(), wxID_ANY,
        wxT("Move completed to:")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_completed_dir = new wxTextCtrl(recvBox->GetStaticBox(), wxID_ANY);
    recvGrid->Add(m_dcc_completed_dir, 1, wxEXPAND);

    recvGrid->Add(new wxStaticText(recvBox->GetStaticBox(), wxID_ANY,
        wxT("Block size:")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_blocksize = new wxSpinCtrl(recvBox->GetStaticBox(), wxID_ANY,
        wxT("4096"), wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 256, 65536);
    recvGrid->Add(m_dcc_blocksize, 0);

    recvBox->Add(recvGrid, 0, wxEXPAND | wxALL, 4);

    m_dcc_save_nick = new wxCheckBox(recvBox->GetStaticBox(), wxID_ANY,
        wxT("Save nick name in filenames"));
    recvBox->Add(m_dcc_save_nick, 0, wxALL, 4);

    sizer->Add(recvBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Auto-open Windows */
    wxStaticBoxSizer *autoBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                      wxT("Auto-open Windows"));
    m_autoopen_send = new wxCheckBox(autoBox->GetStaticBox(), wxID_ANY,
        wxT("Auto open Send window"));
    autoBox->Add(m_autoopen_send, 0, wxALL, 4);
    m_autoopen_recv = new wxCheckBox(autoBox->GetStaticBox(), wxID_ANY,
        wxT("Auto open Receive window"));
    autoBox->Add(m_autoopen_recv, 0, wxALL, 4);
    m_autoopen_chat = new wxCheckBox(autoBox->GetStaticBox(), wxID_ANY,
        wxT("Auto open Chat window"));
    autoBox->Add(m_autoopen_chat, 0, wxALL, 4);
    sizer->Add(autoBox, 0, wxEXPAND | wxBOTTOM, 8);

    /* Speed Limits */
    wxStaticBoxSizer *spdBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                     wxT("Speed Limits (bytes/sec, 0=unlimited)"));
    wxFlexGridSizer *spdGrid = new wxFlexGridSizer(4, 2, 4, 8);

    spdGrid->Add(new wxStaticText(spdBox->GetStaticBox(), wxID_ANY,
        wxT("Max upload (one):")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_max_send = new wxSpinCtrl(spdBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 10000000);
    spdGrid->Add(m_dcc_max_send, 0);

    spdGrid->Add(new wxStaticText(spdBox->GetStaticBox(), wxID_ANY,
        wxT("Max download (one):")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_max_get = new wxSpinCtrl(spdBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 10000000);
    spdGrid->Add(m_dcc_max_get, 0);

    spdGrid->Add(new wxStaticText(spdBox->GetStaticBox(), wxID_ANY,
        wxT("Combined upload:")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_global_max_send = new wxSpinCtrl(spdBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 10000000);
    spdGrid->Add(m_dcc_global_max_send, 0);

    spdGrid->Add(new wxStaticText(spdBox->GetStaticBox(), wxID_ANY,
        wxT("Combined download:")), 0, wxALIGN_CENTER_VERTICAL);
    m_dcc_global_max_get = new wxSpinCtrl(spdBox->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 10000000);
    spdGrid->Add(m_dcc_global_max_get, 0);

    spdBox->Add(spdGrid, 0, wxEXPAND | wxALL, 4);
    sizer->Add(spdBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

wxPanel *PreferencesDialog::CreateIdentdPage(wxWindow *parent)
{
    wxPanel *page = new wxPanel(parent, wxID_ANY);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(page, wxID_ANY, wxT("Identd"));
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 2);
    title->SetFont(f);
    sizer->Add(title, 0, wxBOTTOM, 12);

    wxStaticBoxSizer *identBox = new wxStaticBoxSizer(wxVERTICAL, page,
                                                       wxT("Identd Server"));
    m_identd_server = new wxCheckBox(identBox->GetStaticBox(), wxID_ANY,
        wxT("Enable built-in identd server"));
    identBox->Add(m_identd_server, 0, wxALL, 4);

    wxBoxSizer *portSizer = new wxBoxSizer(wxHORIZONTAL);
    portSizer->Add(new wxStaticText(identBox->GetStaticBox(), wxID_ANY,
        wxT("Port:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_identd_port = new wxSpinCtrl(identBox->GetStaticBox(), wxID_ANY,
        wxT("113"), wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 1, 65535);
    portSizer->Add(m_identd_port, 0);
    identBox->Add(portSizer, 0, wxALL, 4);
    sizer->Add(identBox, 0, wxEXPAND);

    page->SetSizer(sizer);
    return page;
}

void PreferencesDialog::LoadSettings()
{
    /* ===== Appearance ===== */
    if (prefs.pchat_gui_lang >= 0 && prefs.pchat_gui_lang < (int)m_language->GetCount())
        m_language->SetSelection(prefs.pchat_gui_lang);
    else
        m_language->SetSelection(15); /* English */
    m_font_entry->SetValue(wxString::FromUTF8(prefs.pchat_text_font_main));
    m_colored_nicks->SetValue(prefs.pchat_text_color_nicks);
    m_indent_nicks->SetValue(prefs.pchat_text_indent);
    m_show_marker->SetValue(prefs.pchat_text_show_marker);
    m_bg_image->SetValue(wxString::FromUTF8(prefs.pchat_text_background));
    m_opacity_slider->SetValue(prefs.pchat_gui_transparency);
    m_timestamps->SetValue(prefs.pchat_stamp_text);
    m_timestamp_format->SetValue(
        wxString::FromUTF8(prefs.pchat_stamp_text_format));
    m_show_chan_modes->SetValue(prefs.pchat_gui_win_modes);
    m_show_num_users->SetValue(prefs.pchat_gui_win_ucount);
    m_show_nickname->SetValue(prefs.pchat_gui_win_nick);

    /* ===== Input Box ===== */
    m_input_style->SetValue(prefs.pchat_gui_input_style);
    m_input_nick->SetValue(prefs.pchat_gui_input_nick);
    m_input_icon->SetValue(prefs.pchat_gui_input_icon);
    m_input_attr->SetValue(prefs.pchat_gui_input_attr);
    m_input_spell->SetValue(prefs.pchat_gui_input_spell);
    m_spell_langs->SetValue(wxString::FromUTF8(prefs.pchat_text_spell_langs));
    m_completion_suffix->SetValue(
        wxString::FromUTF8(prefs.pchat_completion_suffix));
    m_completion_sort->SetSelection(prefs.pchat_completion_sort);
    m_completion_amount->SetValue(prefs.pchat_completion_amount);
    m_input_perc_ascii->SetValue(prefs.pchat_input_perc_ascii);
    m_input_perc_color->SetValue(prefs.pchat_input_perc_color);

    /* ===== User List ===== */
    m_ulist_hide->SetValue(prefs.pchat_gui_ulist_hide);
    m_ulist_style->SetValue(prefs.pchat_gui_ulist_style);
    m_ulist_show_hosts->SetValue(prefs.pchat_gui_ulist_show_hosts);
    m_ulist_color->SetValue(prefs.pchat_gui_ulist_color);
    m_ulist_count->SetValue(prefs.pchat_gui_ulist_count);
    m_ulist_icons->SetValue(prefs.pchat_gui_ulist_icons);
    m_ulist_buttons->SetValue(prefs.pchat_gui_ulist_buttons);
    m_ulist_sort->SetSelection(prefs.pchat_gui_ulist_sort);
    m_ulist_pos->SetSelection(prefs.pchat_gui_ulist_pos);
    m_away_track->SetValue(prefs.pchat_away_track);
    m_away_size_max->SetValue(prefs.pchat_away_size_max);
    m_ulist_doubleclick->SetValue(
        wxString::FromUTF8(prefs.pchat_gui_ulist_doubleclick));
    m_lagometer->SetSelection(prefs.pchat_gui_lagometer);
    m_throttlemeter->SetSelection(prefs.pchat_gui_throttlemeter);

    /* ===== Channel Switcher ===== */
    /* tab_layout: backend 0=Tabs, 2=Tree; wx choice 0=Tabs, 1=Tree */
    m_tab_layout->SetSelection(prefs.pchat_gui_tab_layout == 2 ? 1 : 0);
    m_tab_chans->SetSelection(prefs.pchat_gui_tab_chans ? 1 : 0);
    m_tab_dialogs->SetSelection(prefs.pchat_gui_tab_dialogs ? 1 : 0);
    m_tab_utils->SetSelection(prefs.pchat_gui_tab_utils ? 1 : 0);
    m_tab_sort->SetValue(prefs.pchat_gui_tab_sort);
    m_tab_icons->SetValue(prefs.pchat_gui_tab_icons);
    m_tab_server->SetValue(prefs.pchat_gui_tab_server);
    m_tab_autoopen_dialog->SetValue(prefs.pchat_gui_autoopen_dialog);
    m_tab_dots->SetValue(prefs.pchat_gui_tab_dots);
    m_tab_scrollchans->SetValue(prefs.pchat_gui_tab_scrollchans);
    m_tab_middleclose->SetValue(prefs.pchat_gui_tab_middleclose);
    m_tab_small->SetValue(prefs.pchat_gui_tab_small);
    m_tab_newtofront->SetSelection(prefs.pchat_gui_tab_newtofront);
    m_notice_pos->SetSelection(prefs.pchat_irc_notice_pos);
    m_tab_pos->SetSelection(prefs.pchat_gui_tab_pos);
    m_tab_trunc->SetValue(prefs.pchat_gui_tab_trunc);

    /* ===== Colors ===== */
    m_strip_msg->SetValue(prefs.pchat_text_stripcolor_msg);
    m_strip_replay->SetValue(prefs.pchat_text_stripcolor_replay);
    m_strip_topic->SetValue(prefs.pchat_text_stripcolor_topic);

    /* ===== Chatting > General ===== */
    m_quit_reason->SetValue(wxString::FromUTF8(prefs.pchat_irc_quit_reason));
    m_part_reason->SetValue(wxString::FromUTF8(prefs.pchat_irc_part_reason));
    m_away_reason->SetValue(wxString::FromUTF8(prefs.pchat_away_reason));
    m_away_show_once->SetValue(prefs.pchat_away_show_once);
    m_away_auto_unmark->SetValue(prefs.pchat_away_auto_unmark);
    m_irc_raw_modes->SetValue(prefs.pchat_irc_raw_modes);
    m_notify_whois->SetValue(prefs.pchat_notify_whois_online);
    m_irc_conf_mode->SetValue(prefs.pchat_irc_conf_mode);
    m_irc_hide_nickchange->SetValue(prefs.pchat_irc_hide_nickchange);

    /* ===== Alerts ===== */
    m_balloon_chans->SetValue(prefs.pchat_input_balloon_chans);
    m_balloon_priv->SetValue(prefs.pchat_input_balloon_priv);
    m_balloon_hilight->SetValue(prefs.pchat_input_balloon_hilight);
    m_tray_chans->SetValue(prefs.pchat_input_tray_chans);
    m_tray_priv->SetValue(prefs.pchat_input_tray_priv);
    m_tray_hilight->SetValue(prefs.pchat_input_tray_hilight);
    m_flash_chans->SetValue(prefs.pchat_input_flash_chans);
    m_flash_priv->SetValue(prefs.pchat_input_flash_priv);
    m_flash_hilight->SetValue(prefs.pchat_input_flash_hilight);
    m_beep_chans->SetValue(prefs.pchat_input_beep_chans);
    m_beep_priv->SetValue(prefs.pchat_input_beep_priv);
    m_beep_hilight->SetValue(prefs.pchat_input_beep_hilight);
    m_away_omit_alerts->SetValue(prefs.pchat_away_omit_alerts);
    m_focus_omitalerts->SetValue(prefs.pchat_gui_focus_omitalerts);
    m_gui_tray->SetValue(prefs.pchat_gui_tray);
    m_tray_minimize->SetValue(prefs.pchat_gui_tray_minimize);
    m_tray_close->SetValue(prefs.pchat_gui_tray_close);
    m_tray_away->SetValue(prefs.pchat_gui_tray_away);
    m_tray_quiet->SetValue(prefs.pchat_gui_tray_quiet);
    m_extra_hilight->SetValue(
        wxString::FromUTF8(prefs.pchat_irc_extra_hilight));
    m_no_hilight->SetValue(wxString::FromUTF8(prefs.pchat_irc_no_hilight));
    m_nick_hilight->SetValue(wxString::FromUTF8(prefs.pchat_irc_nick_hilight));

    /* ===== Logging ===== */
    m_logging->SetValue(prefs.pchat_irc_logging);
    m_log_timestamps->SetValue(prefs.pchat_stamp_log);
    m_text_replay->SetValue(prefs.pchat_text_replay);
    m_text_max_lines->SetValue(prefs.pchat_text_max_lines);
    m_logmask->SetValue(wxString::FromUTF8(prefs.pchat_irc_logmask));
    m_log_timestamp_fmt->SetValue(
        wxString::FromUTF8(prefs.pchat_stamp_log_format));
    m_url_logging->SetValue(prefs.pchat_url_logging);
    m_url_grabber->SetValue(prefs.pchat_url_grabber);
    m_url_grabber_limit->SetValue(prefs.pchat_url_grabber_limit);

    /* ===== Advanced ===== */
    m_autocopy_text->SetValue(prefs.pchat_text_autocopy_text);
    m_autocopy_stamp->SetValue(prefs.pchat_text_autocopy_stamp);
    m_autocopy_color->SetValue(prefs.pchat_text_autocopy_color);
    m_real_name->SetValue(wxString::FromUTF8(prefs.pchat_irc_real_name));
    m_font_alternative->SetValue(
        wxString::FromUTF8(prefs.pchat_text_font_alternative));
    m_gui_compact->SetValue(prefs.pchat_gui_compact);
    m_cap_server_time->SetValue(prefs.pchat_irc_cap_server_time);
    m_auto_reconnect->SetValue(prefs.pchat_net_auto_reconnect);
    m_reconnect_delay->SetValue(prefs.pchat_net_reconnect_delay);
    m_join_delay->SetValue(prefs.pchat_irc_join_delay);
    m_ban_type->SetSelection(prefs.pchat_irc_ban_type);

    /* ===== Network Setup ===== */
    m_bind_host->SetValue(wxString::FromUTF8(prefs.pchat_net_bind_host));
    m_dcc_ip_from_server->SetValue(prefs.pchat_dcc_ip_from_server);
    m_dcc_ip->SetValue(wxString::FromUTF8(prefs.pchat_dcc_ip));
    m_dcc_port_first->SetValue(prefs.pchat_dcc_port_first);
    m_dcc_port_last->SetValue(prefs.pchat_dcc_port_last);
    m_proxy_type->SetSelection(prefs.pchat_net_proxy_type);
    m_proxy_host->SetValue(wxString::FromUTF8(prefs.pchat_net_proxy_host));
    m_proxy_port->SetValue(prefs.pchat_net_proxy_port);
    m_proxy_use->SetSelection(prefs.pchat_net_proxy_use);
    m_proxy_auth->SetValue(prefs.pchat_net_proxy_auth);
    m_proxy_user->SetValue(wxString::FromUTF8(prefs.pchat_net_proxy_user));
    m_proxy_pass->SetValue(wxString::FromUTF8(prefs.pchat_net_proxy_pass));

    /* ===== File Transfers ===== */
    m_dcc_auto_recv->SetSelection(prefs.pchat_dcc_auto_recv);
    m_dcc_dir->SetValue(wxString::FromUTF8(prefs.pchat_dcc_dir));
    m_dcc_completed_dir->SetValue(
        wxString::FromUTF8(prefs.pchat_dcc_completed_dir));
    m_dcc_save_nick->SetValue(prefs.pchat_dcc_save_nick);
    m_autoopen_send->SetValue(prefs.pchat_gui_autoopen_send);
    m_autoopen_recv->SetValue(prefs.pchat_gui_autoopen_recv);
    m_autoopen_chat->SetValue(prefs.pchat_gui_autoopen_chat);
    m_dcc_max_send->SetValue(prefs.pchat_dcc_max_send_cps);
    m_dcc_max_get->SetValue(prefs.pchat_dcc_max_get_cps);
    m_dcc_global_max_send->SetValue(prefs.pchat_dcc_global_max_send_cps);
    m_dcc_global_max_get->SetValue(prefs.pchat_dcc_global_max_get_cps);
    m_dcc_blocksize->SetValue(prefs.pchat_dcc_blocksize);

    /* ===== Identd ===== */
    m_identd_server->SetValue(prefs.pchat_identd_server);
    m_identd_port->SetValue(prefs.pchat_identd_port);

    /* ===== Sounds ===== */
    m_sound_list->DeleteAllItems();
    for (int i = 0; i < NUM_XP; i++) {
        long idx = m_sound_list->InsertItem(i, wxString::FromUTF8(te[i].name));
        if (sound_files[i] && sound_files[i][0])
            m_sound_list->SetItem(idx, 1, wxString::FromUTF8(sound_files[i]));
        else
            m_sound_list->SetItem(idx, 1, wxEmptyString);
    }
}

void PreferencesDialog::SaveSettings()
{
    /* Helper to safely copy string prefs */
    auto saveStr = [](char *dest, const wxTextCtrl *ctrl, size_t maxlen) {
        strncpy(dest, ctrl->GetValue().utf8_str().data(), maxlen - 1);
        dest[maxlen - 1] = '\0';
    };

    /* ===== Appearance ===== */
    prefs.pchat_gui_lang = m_language->GetSelection();
    saveStr(prefs.pchat_text_font_main, m_font_entry, FONTNAMELEN);
    prefs.pchat_text_color_nicks = m_colored_nicks->GetValue();
    prefs.pchat_text_indent = m_indent_nicks->GetValue();
    prefs.pchat_text_show_marker = m_show_marker->GetValue();
    saveStr(prefs.pchat_text_background, m_bg_image, PATHLEN);
    prefs.pchat_gui_transparency = m_opacity_slider->GetValue();
    prefs.pchat_stamp_text = m_timestamps->GetValue();
    saveStr(prefs.pchat_stamp_text_format, m_timestamp_format, 64);
    prefs.pchat_gui_win_modes = m_show_chan_modes->GetValue();
    prefs.pchat_gui_win_ucount = m_show_num_users->GetValue();
    prefs.pchat_gui_win_nick = m_show_nickname->GetValue();

    /* ===== Input Box ===== */
    prefs.pchat_gui_input_style = m_input_style->GetValue();
    prefs.pchat_gui_input_nick = m_input_nick->GetValue();
    prefs.pchat_gui_input_icon = m_input_icon->GetValue();
    prefs.pchat_gui_input_attr = m_input_attr->GetValue();
    prefs.pchat_gui_input_spell = m_input_spell->GetValue();
    saveStr(prefs.pchat_text_spell_langs, m_spell_langs, 64);
    saveStr(prefs.pchat_completion_suffix, m_completion_suffix,
            sizeof(prefs.pchat_completion_suffix));
    prefs.pchat_completion_sort = m_completion_sort->GetSelection();
    prefs.pchat_completion_amount = m_completion_amount->GetValue();
    prefs.pchat_input_perc_ascii = m_input_perc_ascii->GetValue();
    prefs.pchat_input_perc_color = m_input_perc_color->GetValue();

    /* ===== User List ===== */
    prefs.pchat_gui_ulist_hide = m_ulist_hide->GetValue();
    prefs.pchat_gui_ulist_style = m_ulist_style->GetValue();
    prefs.pchat_gui_ulist_show_hosts = m_ulist_show_hosts->GetValue();
    prefs.pchat_gui_ulist_color = m_ulist_color->GetValue();
    prefs.pchat_gui_ulist_count = m_ulist_count->GetValue();
    prefs.pchat_gui_ulist_icons = m_ulist_icons->GetValue();
    prefs.pchat_gui_ulist_buttons = m_ulist_buttons->GetValue();
    prefs.pchat_gui_ulist_sort = m_ulist_sort->GetSelection();
    prefs.pchat_gui_ulist_pos = m_ulist_pos->GetSelection();
    prefs.pchat_away_track = m_away_track->GetValue();
    prefs.pchat_away_size_max = m_away_size_max->GetValue();
    saveStr(prefs.pchat_gui_ulist_doubleclick, m_ulist_doubleclick, 256);
    prefs.pchat_gui_lagometer = m_lagometer->GetSelection();
    prefs.pchat_gui_throttlemeter = m_throttlemeter->GetSelection();

    /* ===== Channel Switcher ===== */
    prefs.pchat_gui_tab_layout = (m_tab_layout->GetSelection() == 1) ? 2 : 0;
    prefs.pchat_gui_tab_chans = (m_tab_chans->GetSelection() == 1);
    prefs.pchat_gui_tab_dialogs = (m_tab_dialogs->GetSelection() == 1);
    prefs.pchat_gui_tab_utils = (m_tab_utils->GetSelection() == 1);
    prefs.pchat_gui_tab_sort = m_tab_sort->GetValue();
    prefs.pchat_gui_tab_icons = m_tab_icons->GetValue();
    prefs.pchat_gui_tab_server = m_tab_server->GetValue();
    prefs.pchat_gui_autoopen_dialog = m_tab_autoopen_dialog->GetValue();
    prefs.pchat_gui_tab_dots = m_tab_dots->GetValue();
    prefs.pchat_gui_tab_scrollchans = m_tab_scrollchans->GetValue();
    prefs.pchat_gui_tab_middleclose = m_tab_middleclose->GetValue();
    prefs.pchat_gui_tab_small = m_tab_small->GetValue();
    prefs.pchat_gui_tab_newtofront = m_tab_newtofront->GetSelection();
    prefs.pchat_irc_notice_pos = m_notice_pos->GetSelection();
    prefs.pchat_gui_tab_pos = m_tab_pos->GetSelection();
    prefs.pchat_gui_tab_trunc = m_tab_trunc->GetValue();

    /* ===== Colors ===== */
    prefs.pchat_text_stripcolor_msg = m_strip_msg->GetValue();
    prefs.pchat_text_stripcolor_replay = m_strip_replay->GetValue();
    prefs.pchat_text_stripcolor_topic = m_strip_topic->GetValue();

    /* ===== Chatting > General ===== */
    saveStr(prefs.pchat_irc_quit_reason, m_quit_reason, 256);
    saveStr(prefs.pchat_irc_part_reason, m_part_reason, 256);
    saveStr(prefs.pchat_away_reason, m_away_reason, 256);
    prefs.pchat_away_show_once = m_away_show_once->GetValue();
    prefs.pchat_away_auto_unmark = m_away_auto_unmark->GetValue();
    prefs.pchat_irc_raw_modes = m_irc_raw_modes->GetValue();
    prefs.pchat_notify_whois_online = m_notify_whois->GetValue();
    prefs.pchat_irc_conf_mode = m_irc_conf_mode->GetValue();
    prefs.pchat_irc_hide_nickchange = m_irc_hide_nickchange->GetValue();

    /* ===== Alerts ===== */
    prefs.pchat_input_balloon_chans = m_balloon_chans->GetValue();
    prefs.pchat_input_balloon_priv = m_balloon_priv->GetValue();
    prefs.pchat_input_balloon_hilight = m_balloon_hilight->GetValue();
    prefs.pchat_input_tray_chans = m_tray_chans->GetValue();
    prefs.pchat_input_tray_priv = m_tray_priv->GetValue();
    prefs.pchat_input_tray_hilight = m_tray_hilight->GetValue();
    prefs.pchat_input_flash_chans = m_flash_chans->GetValue();
    prefs.pchat_input_flash_priv = m_flash_priv->GetValue();
    prefs.pchat_input_flash_hilight = m_flash_hilight->GetValue();
    prefs.pchat_input_beep_chans = m_beep_chans->GetValue();
    prefs.pchat_input_beep_priv = m_beep_priv->GetValue();
    prefs.pchat_input_beep_hilight = m_beep_hilight->GetValue();
    prefs.pchat_away_omit_alerts = m_away_omit_alerts->GetValue();
    prefs.pchat_gui_focus_omitalerts = m_focus_omitalerts->GetValue();
    prefs.pchat_gui_tray = m_gui_tray->GetValue();
    prefs.pchat_gui_tray_minimize = m_tray_minimize->GetValue();
    prefs.pchat_gui_tray_close = m_tray_close->GetValue();
    prefs.pchat_gui_tray_away = m_tray_away->GetValue();
    prefs.pchat_gui_tray_quiet = m_tray_quiet->GetValue();
    saveStr(prefs.pchat_irc_extra_hilight, m_extra_hilight, 300);
    saveStr(prefs.pchat_irc_no_hilight, m_no_hilight, 300);
    saveStr(prefs.pchat_irc_nick_hilight, m_nick_hilight, 300);

    /* ===== Logging ===== */
    prefs.pchat_irc_logging = m_logging->GetValue();
    prefs.pchat_stamp_log = m_log_timestamps->GetValue();
    prefs.pchat_text_replay = m_text_replay->GetValue();
    prefs.pchat_text_max_lines = m_text_max_lines->GetValue();
    saveStr(prefs.pchat_irc_logmask, m_logmask, 256);
    saveStr(prefs.pchat_stamp_log_format, m_log_timestamp_fmt, 64);
    prefs.pchat_url_logging = m_url_logging->GetValue();
    prefs.pchat_url_grabber = m_url_grabber->GetValue();
    prefs.pchat_url_grabber_limit = m_url_grabber_limit->GetValue();

    /* ===== Advanced ===== */
    prefs.pchat_text_autocopy_text = m_autocopy_text->GetValue();
    prefs.pchat_text_autocopy_stamp = m_autocopy_stamp->GetValue();
    prefs.pchat_text_autocopy_color = m_autocopy_color->GetValue();
    saveStr(prefs.pchat_irc_real_name, m_real_name, 127);
    saveStr(prefs.pchat_text_font_alternative, m_font_alternative,
            sizeof(prefs.pchat_text_font_alternative));
    prefs.pchat_gui_compact = m_gui_compact->GetValue();
    prefs.pchat_irc_cap_server_time = m_cap_server_time->GetValue();
    prefs.pchat_net_auto_reconnect = m_auto_reconnect->GetValue();
    prefs.pchat_net_reconnect_delay = m_reconnect_delay->GetValue();
    prefs.pchat_irc_join_delay = m_join_delay->GetValue();
    prefs.pchat_irc_ban_type = m_ban_type->GetSelection();

    /* ===== Network Setup ===== */
    saveStr(prefs.pchat_net_bind_host, m_bind_host, 128);
    prefs.pchat_dcc_ip_from_server = m_dcc_ip_from_server->GetValue();
    saveStr(prefs.pchat_dcc_ip, m_dcc_ip, sizeof(prefs.pchat_dcc_ip));
    prefs.pchat_dcc_port_first = m_dcc_port_first->GetValue();
    prefs.pchat_dcc_port_last = m_dcc_port_last->GetValue();
    prefs.pchat_net_proxy_type = m_proxy_type->GetSelection();
    saveStr(prefs.pchat_net_proxy_host, m_proxy_host, 64);
    prefs.pchat_net_proxy_port = m_proxy_port->GetValue();
    prefs.pchat_net_proxy_use = m_proxy_use->GetSelection();
    prefs.pchat_net_proxy_auth = m_proxy_auth->GetValue();
    saveStr(prefs.pchat_net_proxy_user, m_proxy_user, 256);
    saveStr(prefs.pchat_net_proxy_pass, m_proxy_pass, 256);

    /* ===== File Transfers ===== */
    prefs.pchat_dcc_auto_recv = m_dcc_auto_recv->GetSelection();
    saveStr(prefs.pchat_dcc_dir, m_dcc_dir, PATHLEN);
    saveStr(prefs.pchat_dcc_completed_dir, m_dcc_completed_dir, PATHLEN);
    prefs.pchat_dcc_save_nick = m_dcc_save_nick->GetValue();
    prefs.pchat_gui_autoopen_send = m_autoopen_send->GetValue();
    prefs.pchat_gui_autoopen_recv = m_autoopen_recv->GetValue();
    prefs.pchat_gui_autoopen_chat = m_autoopen_chat->GetValue();
    prefs.pchat_dcc_max_send_cps = m_dcc_max_send->GetValue();
    prefs.pchat_dcc_max_get_cps = m_dcc_max_get->GetValue();
    prefs.pchat_dcc_global_max_send_cps = m_dcc_global_max_send->GetValue();
    prefs.pchat_dcc_global_max_get_cps = m_dcc_global_max_get->GetValue();
    prefs.pchat_dcc_blocksize = m_dcc_blocksize->GetValue();

    /* ===== Identd ===== */
    prefs.pchat_identd_server = m_identd_server->GetValue();
    prefs.pchat_identd_port = m_identd_port->GetValue();

    /* ===== Sounds ===== */
    for (int i = 0; i < NUM_XP; i++) {
        wxString sf = m_sound_list->GetItemText(i, 1);
        if (sound_files[i])
            g_free(sound_files[i]);
        if (!sf.IsEmpty())
            sound_files[i] = g_strdup(sf.utf8_str().data());
        else
            sound_files[i] = NULL;
    }
    sound_save();

    /* ===== Colors ===== */
    if (m_colors_changed) {
        wx_palette_save();
    }

    save_config();
}

void PreferencesDialog::OnOK(wxCommandEvent &event)
{
    SaveSettings();
    if (m_colors_changed && g_main_window)
        g_main_window->ApplyPaletteColors();
    EndModal(wxID_OK);
}

void PreferencesDialog::OnCancel(wxCommandEvent &event)
{
    EndModal(wxID_CANCEL);
}

void PreferencesDialog::OnCategoryChanged(wxTreeEvent &event)
{
    wxTreeItemId item = event.GetItem();
    auto it = m_pages.find(item);
    if (it != m_pages.end()) {
        if (m_current_page) {
            m_current_page->Hide();
        }
        m_current_page = it->second;
        m_current_page->Show();
        m_settings_panel->Layout();
    }
}

void PreferencesDialog::OnFontBrowse(wxCommandEvent &event)
{
    wxFontDialog dlg(this);
    if (dlg.ShowModal() == wxID_OK) {
        wxFont font = dlg.GetFontData().GetChosenFont();
        wxString desc = wxString::Format(wxT("%s %d"),
                                          font.GetFaceName(), font.GetPointSize());
        m_font_entry->SetValue(desc);
    }
}

void PreferencesDialog::OnBgBrowse(wxCommandEvent &event)
{
    wxFileDialog dlg(this, wxT("Select background image"),
                      wxEmptyString, wxEmptyString,
                      wxT("Image files (*.png;*.jpg;*.bmp)|*.png;*.jpg;*.bmp|All files (*.*)|*.*"),
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        m_bg_image->SetValue(dlg.GetPath());
    }
}

void PreferencesDialog::OnSoundBrowse(wxCommandEvent &event)
{
    /* Default to <config>/sounds directory */
    char *sounds_dir = g_build_filename(get_xdir(), PCHAT_SOUND_DIR, NULL);
    wxString defaultDir = wxString::FromUTF8(sounds_dir);
    g_free(sounds_dir);

    wxFileDialog dlg(this, wxT("Select a sound file"),
                      defaultDir, wxEmptyString,
                      wxT("Sound files (*.wav;*.ogg;*.mp3)|*.wav;*.ogg;*.mp3|All files (*.*)|*.*"),
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        wxString path = dlg.GetPath();
        /* If file is in the default sounds dir, store just the basename */
        wxFileName fn(path);
        wxFileName dirFn(defaultDir + wxFileName::GetPathSeparator());
        if (fn.GetPath() == dirFn.GetPath())
            m_sound_file->SetValue(fn.GetFullName());
        else
            m_sound_file->SetValue(path);
    }
}

void PreferencesDialog::OnSoundPlay(wxCommandEvent &event)
{
    wxString file = m_sound_file->GetValue();
    if (!file.IsEmpty())
        sound_play(file.utf8_str().data(), FALSE);
}
