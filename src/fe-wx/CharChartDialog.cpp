/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Character Chart Dialog - Unicode character browser
 */

#include "CharChartDialog.h"
#include "MainWindow.h"
#include <wx/clipbrd.h>

extern MainWindow *g_main_window;

wxBEGIN_EVENT_TABLE(CharChartDialog, wxDialog)
    EVT_GRID_SELECT_CELL(CharChartDialog::OnCellSelected)
    EVT_BUTTON(ID_CHART_INSERT, CharChartDialog::OnInsert)
    EVT_CLOSE(CharChartDialog::OnCloseWindow)
wxEND_EVENT_TABLE()

CharChartDialog::CharChartDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, wxT("Character Chart - PChat"),
               wxDefaultPosition, wxSize(520, 460),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    /* Block selector */
    wxBoxSizer *topSizer = new wxBoxSizer(wxHORIZONTAL);
    topSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Block:")),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_block_choice = new wxChoice(this, wxID_ANY);
    m_block_choice->Append(wxT("Basic Latin (U+0020..U+007F)"));
    m_block_choice->Append(wxT("Latin-1 Supplement (U+0080..U+00FF)"));
    m_block_choice->Append(wxT("Latin Extended-A (U+0100..U+017F)"));
    m_block_choice->Append(wxT("Latin Extended-B (U+0180..U+024F)"));
    m_block_choice->Append(wxT("Cyrillic (U+0400..U+04FF)"));
    m_block_choice->Append(wxT("Greek (U+0370..U+03FF)"));
    m_block_choice->Append(wxT("Box Drawing (U+2500..U+257F)"));
    m_block_choice->Append(wxT("Arrows (U+2190..U+21FF)"));
    m_block_choice->Append(wxT("Mathematical Operators (U+2200..U+22FF)"));
    m_block_choice->Append(wxT("Miscellaneous Symbols (U+2600..U+26FF)"));
    m_block_choice->SetSelection(0);
    topSizer->Add(m_block_choice, 1, wxEXPAND);
    mainSizer->Add(topSizer, 0, wxEXPAND | wxALL, 4);

    /* Character grid */
    m_grid = new wxGrid(this, wxID_ANY);
    m_grid->CreateGrid(16, 16);
    m_grid->SetDefaultColSize(30, true);
    m_grid->SetDefaultRowSize(28, true);
    m_grid->SetColLabelSize(20);
    m_grid->SetRowLabelSize(40);

    wxFont gridFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                     wxFONTWEIGHT_NORMAL);
    m_grid->SetDefaultCellFont(gridFont);
    m_grid->SetDefaultCellAlignment(wxALIGN_CENTRE, wxALIGN_CENTRE);

    /* Set hex column labels */
    for (int c = 0; c < 16; c++) {
        m_grid->SetColLabelValue(c, wxString::Format(wxT("%X"), c));
    }

    mainSizer->Add(m_grid, 1, wxEXPAND | wxALL, 4);

    /* Bottom bar: info + insert button */
    wxBoxSizer *bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    m_info_label = new wxStaticText(this, wxID_ANY,
                                     wxT("Select a character"));
    bottomSizer->Add(m_info_label, 1, wxALIGN_CENTER_VERTICAL);
    bottomSizer->Add(new wxButton(this, ID_CHART_INSERT, wxT("Insert")),
                      0, wxLEFT, 8);
    bottomSizer->Add(new wxButton(this, wxID_CLOSE, wxT("Close")),
                      0, wxLEFT, 8);
    mainSizer->Add(bottomSizer, 0, wxEXPAND | wxALL, 4);

    SetSizer(mainSizer);
    Centre();

    /* Connect block change */
    m_block_choice->Bind(wxEVT_CHOICE,
                          &CharChartDialog::OnBlockChanged, this);

    /* Populate initial block */
    PopulateGrid(0x0020);
}

CharChartDialog::~CharChartDialog()
{
}

void CharChartDialog::PopulateGrid(int base_cp)
{
    m_base_codepoint = base_cp;

    for (int r = 0; r < 16; r++) {
        m_grid->SetRowLabelValue(r,
            wxString::Format(wxT("%03X"), (base_cp / 16 + r)));
        for (int c = 0; c < 16; c++) {
            int cp = base_cp + r * 16 + c;
            if (cp >= 0 && cp <= 0x10FFFF) {
                wxString ch;
                ch << wxUniChar(cp);
                m_grid->SetCellValue(r, c, ch);
            } else {
                m_grid->SetCellValue(r, c, wxEmptyString);
            }
        }
    }
}

void CharChartDialog::OnBlockChanged(wxCommandEvent &event)
{
    int bases[] = {
        0x0020,  /* Basic Latin */
        0x0080,  /* Latin-1 Supplement */
        0x0100,  /* Latin Extended-A */
        0x0180,  /* Latin Extended-B */
        0x0400,  /* Cyrillic */
        0x0370,  /* Greek */
        0x2500,  /* Box Drawing */
        0x2190,  /* Arrows */
        0x2200,  /* Mathematical Operators */
        0x2600   /* Miscellaneous Symbols */
    };

    int sel = m_block_choice->GetSelection();
    if (sel >= 0 && sel < (int)(sizeof(bases) / sizeof(bases[0]))) {
        PopulateGrid(bases[sel]);
    }
}

void CharChartDialog::OnCellSelected(wxGridEvent &event)
{
    int r = event.GetRow();
    int c = event.GetCol();
    int cp = m_base_codepoint + r * 16 + c;

    wxString ch;
    ch << wxUniChar(cp);
    m_info_label->SetLabel(
        wxString::Format(wxT("%s  U+%04X"), ch, cp));

    event.Skip();
}

void CharChartDialog::OnInsert(wxCommandEvent &event)
{
    int r = m_grid->GetGridCursorRow();
    int c = m_grid->GetGridCursorCol();
    if (r < 0 || c < 0) return;

    int cp = m_base_codepoint + r * 16 + c;
    wxString ch;
    ch << wxUniChar(cp);

    /* Insert directly into the main window's input box */
    if (g_main_window) {
        g_main_window->InsertInputText(ch);
    }
}

void CharChartDialog::OnCloseWindow(wxCloseEvent &event)
{
    Hide();
}
