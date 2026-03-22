#pragma once

#include "print_support.h"

#include <wx/dc.h>
#include <wx/gdicmn.h>
#include <wx/string.h>

wxSize GetLogicalPageSize(bool landscape);
wxString GetHeaderTitle(const PrintableDocument& document, int col);
unsigned int GetColumnCount(const PrintableDocument& document);
wxString GetCellText(const PrintableDocument& document, unsigned int row, unsigned int col);
void DrawPage(wxDC& dc, const PrintableDocument& document, int pageNumber, bool landscape);
int GetPageCount(const PrintableDocument& document, bool landscape);
