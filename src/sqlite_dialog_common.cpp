#include "sqlite_dialog_common.h"

#include <sqlite3.h>

#include <wx/choice.h>
#include <wx/filepicker.h>
#include <wx/filename.h>
#include <wx/progdlg.h>
#include <wx/string.h>
#include <wx/window.h>

#include <algorithm>
#include <cstring>

namespace sqlite_dialog {

wxString SqliteError(sqlite3* db, const wxString& fallback) {
    if (!db) {
        return fallback;
    }

    const char* message = sqlite3_errmsg(db);
    if (!message || *message == '\0') {
        return fallback;
    }

    return wxString::FromUTF8(message);
}

wxString QuoteSqlIdentifier(const wxString& identifier) {
    wxString quoted = identifier;
    quoted.Replace("\"", "\"\"");
    return "\"" + quoted + "\"";
}

wxString BlobToHexString(const void* blob, int size) {
    if (!blob || size <= 0) {
        return {};
    }

    const unsigned char* bytes = static_cast<const unsigned char*>(blob);
    wxString hex = "0x";
    static constexpr char digits[] = "0123456789ABCDEF";
    for (int i = 0; i < size; ++i) {
        hex += digits[bytes[i] >> 4];
        hex += digits[bytes[i] & 0x0F];
    }
    return hex;
}

bool OpenSqliteDatabase(const wxString& path, int flags, sqlite3** outDb, wxString* errorMessage) {
    *outDb = nullptr;
    const wxCharBuffer utf8Path = path.utf8_str();
    const int result = sqlite3_open_v2(utf8Path.data(), outDb, flags, nullptr);
    if (result == SQLITE_OK) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = SqliteError(*outDb, "Unable to open SQLite database.");
    }
    if (*outDb) {
        sqlite3_close(*outDb);
        *outDb = nullptr;
    }
    return false;
}

bool LoadSqliteTableNames(const wxString& path, std::vector<wxString>* tableNames, wxString* errorMessage) {
    tableNames->clear();

    sqlite3* db = nullptr;
    if (!OpenSqliteDatabase(path, SQLITE_OPEN_READONLY, &db, errorMessage)) {
        return false;
    }

    static constexpr const char* query =
        "SELECT name "
        "FROM sqlite_master "
        "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' "
        "ORDER BY name";

    sqlite3_stmt* statement = nullptr;
    const int prepareResult = sqlite3_prepare_v2(db, query, -1, &statement, nullptr);
    if (prepareResult != SQLITE_OK) {
        if (errorMessage) {
            *errorMessage = SqliteError(db, "Unable to read table list from SQLite database.");
        }
        sqlite3_close(db);
        return false;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(statement, 0);
        if (text) {
            tableNames->push_back(wxString::FromUTF8(reinterpret_cast<const char*>(text)));
        }
    }

    sqlite3_finalize(statement);
    sqlite3_close(db);
    return true;
}

bool ReadSqliteTable(const wxString& path, const wxString& tableName, ImportedSqliteTable* importedTable, wxString* errorMessage) {
    importedTable->documentName = tableName + ".csv";
    importedTable->headers.clear();
    importedTable->rows.clear();

    sqlite3* db = nullptr;
    if (!OpenSqliteDatabase(path, SQLITE_OPEN_READONLY, &db, errorMessage)) {
        return false;
    }

    const wxString query = "SELECT * FROM " + QuoteSqlIdentifier(tableName);
    sqlite3_stmt* statement = nullptr;
    const wxCharBuffer utf8Query = query.utf8_str();
    const int prepareResult = sqlite3_prepare_v2(db, utf8Query.data(), -1, &statement, nullptr);
    if (prepareResult != SQLITE_OK) {
        if (errorMessage) {
            *errorMessage = SqliteError(db, "Unable to read selected SQLite table.");
        }
        sqlite3_close(db);
        return false;
    }

    const int columnCount = sqlite3_column_count(statement);
    importedTable->headers.reserve(static_cast<size_t>(columnCount));
    for (int col = 0; col < columnCount; ++col) {
        const char* name = sqlite3_column_name(statement, col);
        importedTable->headers.push_back(name ? wxString::FromUTF8(name) : wxString::Format("Column %d", col + 1));
    }

    int stepResult = SQLITE_ROW;
    while ((stepResult = sqlite3_step(statement)) == SQLITE_ROW) {
        std::vector<wxString> row;
        row.reserve(static_cast<size_t>(columnCount));
        for (int col = 0; col < columnCount; ++col) {
            switch (sqlite3_column_type(statement, col)) {
            case SQLITE_NULL:
                row.emplace_back();
                break;
            case SQLITE_BLOB:
                row.push_back(BlobToHexString(sqlite3_column_blob(statement, col), sqlite3_column_bytes(statement, col)));
                break;
            default: {
                const unsigned char* value = sqlite3_column_text(statement, col);
                row.push_back(value ? wxString::FromUTF8(reinterpret_cast<const char*>(value)) : wxString());
                break;
            }
            }
        }
        importedTable->rows.push_back(std::move(row));
    }

    sqlite3_finalize(statement);
    sqlite3_close(db);

    if (stepResult != SQLITE_DONE) {
        if (errorMessage) {
            *errorMessage = "Unable to finish reading the selected SQLite table.";
        }
        return false;
    }

    return true;
}

wxString SuggestedTableName(const wxString& documentName) {
    wxString tableName = wxFileName(documentName).GetName();
    if (tableName.IsEmpty()) {
        tableName = documentName;
    }
    if (tableName.IsEmpty()) {
        tableName = "imported_data";
    }

    for (size_t i = 0; i < tableName.Length(); ++i) {
        const wxUniChar ch = tableName[i];
        if (!(wxIsalnum(ch) || ch == '_')) {
            tableName[i] = '_';
        }
    }

    if (tableName.IsEmpty() || wxIsdigit(tableName[0])) {
        tableName.Prepend("table_");
    }
    return tableName;
}

std::vector<wxString> BuildExportColumnNames(const ImportedSqliteTable& table) {
    const size_t columnCount = table.headers.size();
    std::vector<wxString> names;
    names.reserve(columnCount);

    for (size_t i = 0; i < columnCount; ++i) {
        wxString name = table.headers[i];
        if (name.IsEmpty()) {
            name = wxString::Format("Column_%zu", i + 1);
        }

        for (size_t j = 0; j < name.Length(); ++j) {
            const wxUniChar ch = name[j];
            if (!(wxIsalnum(ch) || ch == '_')) {
                name[j] = '_';
            }
        }

        if (name.IsEmpty() || wxIsdigit(name[0])) {
            name.Prepend("Column_");
        }

        if (name.CmpNoCase("ID") == 0) {
            name += "_value";
        }

        const wxString baseName = name;
        int suffix = 2;
        while (std::any_of(names.begin(), names.end(), [&name](const wxString& existing) {
            return existing.CmpNoCase(name) == 0;
        })) {
            name = wxString::Format("%s_%d", baseName, suffix++);
        }

        names.push_back(name);
    }

    return names;
}

bool ExecuteSql(sqlite3* db, const wxString& sql, wxString* errorMessage) {
    char* rawError = nullptr;
    const wxCharBuffer utf8Sql = sql.utf8_str();
    const int result = sqlite3_exec(db, utf8Sql.data(), nullptr, nullptr, &rawError);
    if (result == SQLITE_OK) {
        return true;
    }

    if (errorMessage) {
        wxString sqliteMessage = rawError ? wxString::FromUTF8(rawError) : SqliteError(db, "SQLite command failed.");
        *errorMessage = wxString::Format("%s\n\nSQL:\n%s", sqliteMessage, sql);
    }
    if (rawError) {
        sqlite3_free(rawError);
    }
    return false;
}

bool BindSqlValue(sqlite3_stmt* statement, int index, const wxString& typeName, const wxString& value, wxString* errorMessage) {
    if (value.IsEmpty()) {
        return sqlite3_bind_null(statement, index) == SQLITE_OK;
    }

    if (typeName == "INTEGER") {
        long long integerValue = 0;
        if (value.ToLongLong(&integerValue)) {
            return sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(integerValue)) == SQLITE_OK;
        }
    } else if (typeName == "REAL") {
        double realValue = 0.0;
        if (value.ToDouble(&realValue)) {
            return sqlite3_bind_double(statement, index, realValue) == SQLITE_OK;
        }
    } else if (typeName == "NUMERIC") {
        long long integerValue = 0;
        if (value.ToLongLong(&integerValue)) {
            return sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(integerValue)) == SQLITE_OK;
        }
        double realValue = 0.0;
        if (value.ToDouble(&realValue)) {
            return sqlite3_bind_double(statement, index, realValue) == SQLITE_OK;
        }
    } else if (typeName == "BLOB") {
        const wxCharBuffer utf8Value = value.utf8_str();
        return sqlite3_bind_blob(statement, index, utf8Value.data(), static_cast<int>(strlen(utf8Value.data())), SQLITE_TRANSIENT) == SQLITE_OK;
    }

    const wxCharBuffer utf8Value = value.utf8_str();
    const int result = sqlite3_bind_text(statement, index, utf8Value.data(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK && errorMessage) {
        *errorMessage = "Unable to bind a value for SQLite export.";
    }
    return result == SQLITE_OK;
}

bool WriteSqliteTable(
    wxWindow* parent,
    const wxString& path,
    const wxString& tableName,
    const std::vector<wxString>& columnNames,
    const std::vector<wxString>& columnTypes,
    const std::vector<std::vector<wxString>>& rows,
    wxString* errorMessage) {
    sqlite3* db = nullptr;
    if (!OpenSqliteDatabase(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, &db, errorMessage)) {
        return false;
    }

    wxString createSql = "CREATE TABLE " + QuoteSqlIdentifier(tableName) + " ("
        + QuoteSqlIdentifier("ID") + " INTEGER PRIMARY KEY AUTOINCREMENT";
    for (size_t i = 0; i < columnNames.size(); ++i) {
        createSql += ", " + QuoteSqlIdentifier(columnNames[i]) + " " + columnTypes[i];
    }
    createSql += ")";

    if (!ExecuteSql(db, createSql, errorMessage)) {
        sqlite3_close(db);
        return false;
    }

    wxString insertSql = "INSERT INTO " + QuoteSqlIdentifier(tableName);
    if (!columnNames.empty()) {
        insertSql += " (";
        for (size_t i = 0; i < columnNames.size(); ++i) {
            if (i > 0) {
                insertSql += ", ";
            }
            insertSql += QuoteSqlIdentifier(columnNames[i]);
        }
        insertSql += ")";
    }
    insertSql += " VALUES (";
    for (size_t i = 0; i < columnNames.size(); ++i) {
        if (i > 0) {
            insertSql += ", ";
        }
        insertSql += wxString::Format("?%zu", i + 1);
    }
    insertSql += ")";

    if (!ExecuteSql(db, "BEGIN IMMEDIATE TRANSACTION", errorMessage)) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    const wxCharBuffer utf8InsertSql = insertSql.utf8_str();
    const int prepareResult = sqlite3_prepare_v2(db, utf8InsertSql.data(), -1, &statement, nullptr);
    if (prepareResult != SQLITE_OK) {
        if (errorMessage) {
            *errorMessage = wxString::Format("%s\n\nSQL:\n%s", SqliteError(db, "Unable to prepare SQLite export statement."), insertSql);
        }
        ExecuteSql(db, "ROLLBACK", nullptr);
        sqlite3_close(db);
        return false;
    }

    wxProgressDialog progressDialog(
        "Export To SQLite Database",
        "Preparing export...",
        std::max(1, static_cast<int>(rows.size())),
        parent,
        wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME);

    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);

        const std::vector<wxString>& row = rows[rowIndex];
        for (size_t col = 0; col < columnNames.size(); ++col) {
            const wxString value = col < row.size() ? row[col] : wxString();
            if (!BindSqlValue(statement, static_cast<int>(col + 1), columnTypes[col], value, errorMessage)) {
                if (errorMessage && errorMessage->IsEmpty()) {
                    *errorMessage = wxString::Format("Unable to bind row %zu, column %zu while exporting.", rowIndex + 1, col + 1);
                }
                sqlite3_finalize(statement);
                ExecuteSql(db, "ROLLBACK", nullptr);
                sqlite3_close(db);
                return false;
            }
        }

        const int stepResult = sqlite3_step(statement);
        if (stepResult != SQLITE_DONE) {
            if (errorMessage) {
                *errorMessage = wxString::Format(
                    "SQLite export failed at row %zu.\n\n%s\n\nSQL:\n%s",
                    rowIndex + 1,
                    SqliteError(db, "Unable to insert row into SQLite table."),
                    insertSql);
            }
            sqlite3_finalize(statement);
            ExecuteSql(db, "ROLLBACK", nullptr);
            sqlite3_close(db);
            return false;
        }

        progressDialog.Update(
            static_cast<int>(rowIndex + 1),
            wxString::Format("Exporting row %zu of %zu", rowIndex + 1, rows.size()));
    }

    sqlite3_finalize(statement);

    if (!ExecuteSql(db, "COMMIT", errorMessage)) {
        ExecuteSql(db, "ROLLBACK", nullptr);
        sqlite3_close(db);
        return false;
    }

    sqlite3_close(db);
    return true;
}

} // namespace sqlite_dialog
