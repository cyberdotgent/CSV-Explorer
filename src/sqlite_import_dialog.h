#pragma once

#include <vector>

#include <wx/string.h>

class wxWindow;

struct ImportedSqliteTable {
    wxString documentName;
    std::vector<wxString> headers;
    std::vector<std::vector<wxString>> rows;
};

bool ShowSqliteImportDialog(wxWindow* parent, ImportedSqliteTable* importedTable);
bool ShowSqliteExportDialog(wxWindow* parent, const ImportedSqliteTable& table);
