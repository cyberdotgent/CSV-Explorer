#pragma once

#include "sqlite_import_dialog.h"

#include <vector>

#include <wx/string.h>

class wxWindow;
class wxChoice;

struct sqlite3;
struct sqlite3_stmt;

namespace sqlite_dialog {

wxString SqliteError(sqlite3* db, const wxString& fallback);
wxString QuoteSqlIdentifier(const wxString& identifier);
wxString BlobToHexString(const void* blob, int size);
bool OpenSqliteDatabase(const wxString& path, int flags, sqlite3** outDb, wxString* errorMessage);
bool LoadSqliteTableNames(const wxString& path, std::vector<wxString>* tableNames, wxString* errorMessage);
bool ReadSqliteTable(const wxString& path, const wxString& tableName, ImportedSqliteTable* importedTable, wxString* errorMessage);
wxString SuggestedTableName(const wxString& documentName);
std::vector<wxString> BuildExportColumnNames(const ImportedSqliteTable& table);
bool ExecuteSql(sqlite3* db, const wxString& sql, wxString* errorMessage);
bool BindSqlValue(sqlite3_stmt* statement, int index, const wxString& typeName, const wxString& value, wxString* errorMessage);
bool WriteSqliteTable(
    wxWindow* parent,
    const wxString& path,
    const wxString& tableName,
    const std::vector<wxString>& columnNames,
    const std::vector<wxString>& columnTypes,
    const std::vector<std::vector<wxString>>& rows,
    wxString* errorMessage);

} // namespace sqlite_dialog
