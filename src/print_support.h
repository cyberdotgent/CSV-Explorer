#pragma once

#include <vector>

#include <wx/print.h>
#include <wx/string.h>

class wxWindow;

struct PrintableDocument {
    wxString title;
    std::vector<wxString> headers;
    std::vector<std::vector<wxString>> rows;
};

bool GuessLandscapeForPrint(const PrintableDocument& document);
bool ShowPrintDialogForDocument(wxWindow* parent, const PrintableDocument& document, wxPrintData& printData);
void ShowPrintPreviewWindow(wxWindow* parent, const PrintableDocument& document, wxPrintData& printData);
