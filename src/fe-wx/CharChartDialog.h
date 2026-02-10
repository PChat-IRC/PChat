/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Character Chart Dialog - replicates HexChat character chart
 */

#ifndef PCHAT_CHARCHARTDIALOG_H
#define PCHAT_CHARCHARTDIALOG_H

#include <wx/wx.h>
#include <wx/grid.h>

class CharChartDialog : public wxDialog
{
public:
    CharChartDialog(wxWindow *parent);
    ~CharChartDialog();

private:
    void OnCloseWindow(wxCloseEvent &event);
    void OnCellSelected(wxGridEvent &event);
    void OnInsert(wxCommandEvent &event);
    void OnBlockChanged(wxCommandEvent &event);
    void PopulateGrid(int base_cp);

    wxChoice *m_block_choice;
    wxGrid *m_grid;
    wxStaticText *m_info_label;
    int m_base_codepoint = 0;

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_CHART_INSERT = wxID_HIGHEST + 900,
};

#endif /* PCHAT_CHARCHARTDIALOG_H */
