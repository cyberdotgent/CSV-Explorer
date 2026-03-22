#include "sqlite_import_dialog.h"

#include <sqlite3.h>

#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/filepicker.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/window.h>

#include <vector>

namespace {

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

bool OpenSqliteDatabase(const wxString& path, sqlite3** outDb, wxString* errorMessage) {
    *outDb = nullptr;
    const wxCharBuffer utf8Path = path.utf8_str();
    const int result = sqlite3_open_v2(utf8Path.data(), outDb, SQLITE_OPEN_READONLY, nullptr);
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
    if (!OpenSqliteDatabase(path, &db, errorMessage)) {
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
    if (!OpenSqliteDatabase(path, &db, errorMessage)) {
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

class SqliteImportDialog final : public wxDialog {
public:
    explicit SqliteImportDialog(wxWindow* parent)
        : wxDialog(parent, wxID_ANY, "Import From SQLite Database", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
        auto* topSizer = new wxBoxSizer(wxVERTICAL);

        auto* databaseLabel = new wxStaticText(this, wxID_ANY, "SQLite database");
        topSizer->Add(databaseLabel, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

        m_databasePicker = new wxFilePickerCtrl(
            this,
            wxID_ANY,
            {},
            "Choose a SQLite database",
            "SQLite files (*.db;*.sqlite;*.sqlite3)|*.db;*.sqlite;*.sqlite3|All files (*.*)|*.*",
            wxDefaultPosition,
            wxDefaultSize,
            wxFLP_OPEN | wxFLP_FILE_MUST_EXIST | wxFLP_USE_TEXTCTRL);
        topSizer->Add(m_databasePicker, 0, wxEXPAND | wxALL, FromDIP(12));

        auto* tableLabel = new wxStaticText(this, wxID_ANY, "Table");
        topSizer->Add(tableLabel, 0, wxLEFT | wxRIGHT, FromDIP(12));

        m_tableChoice = new wxChoice(this, wxID_ANY);
        topSizer->Add(m_tableChoice, 0, wxEXPAND | wxALL, FromDIP(12));

        m_statusLabel = new wxStaticText(this, wxID_ANY, {});
        topSizer->Add(m_statusLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

        auto* buttonSizer = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
        topSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, FromDIP(12));

        SetSizerAndFit(topSizer);
        SetMinSize(FromDIP(wxSize(520, 220)));

        if (wxButton* okButton = dynamic_cast<wxButton*>(FindWindow(wxID_OK))) {
            okButton->SetLabel("Import");
            okButton->Disable();
        }

        m_databasePicker->Bind(wxEVT_FILEPICKER_CHANGED, &SqliteImportDialog::OnDatabaseChanged, this);
        m_tableChoice->Bind(wxEVT_CHOICE, &SqliteImportDialog::OnTableChanged, this);
        Bind(wxEVT_BUTTON, &SqliteImportDialog::OnImport, this, wxID_OK);
    }

    bool GetImportedTable(ImportedSqliteTable* importedTable) const {
        if (!m_hasImportedTable) {
            return false;
        }
        *importedTable = m_importedTable;
        return true;
    }

private:
    void ReloadTables() {
        m_tables.clear();
        m_tableChoice->Clear();
        m_statusLabel->SetLabel({});

        const wxString path = m_databasePicker->GetPath();
        if (path.IsEmpty()) {
            UpdateImportButton();
            return;
        }

        wxString errorMessage;
        if (!LoadSqliteTableNames(path, &m_tables, &errorMessage)) {
            m_statusLabel->SetLabel(errorMessage);
            UpdateImportButton();
            Layout();
            return;
        }

        for (const wxString& tableName : m_tables) {
            m_tableChoice->Append(tableName);
        }

        if (m_tables.empty()) {
            m_statusLabel->SetLabel("No importable tables were found in the selected SQLite database.");
        } else {
            m_tableChoice->SetSelection(0);
        }

        UpdateImportButton();
        Layout();
    }

    void UpdateImportButton() {
        if (wxWindow* button = FindWindow(wxID_OK)) {
            button->Enable(!m_databasePicker->GetPath().IsEmpty() && m_tableChoice->GetSelection() != wxNOT_FOUND);
        }
    }

    void OnDatabaseChanged(wxFileDirPickerEvent&) {
        ReloadTables();
    }

    void OnTableChanged(wxCommandEvent&) {
        UpdateImportButton();
    }

    void OnImport(wxCommandEvent&) {
        const int selection = m_tableChoice->GetSelection();
        if (selection == wxNOT_FOUND) {
            return;
        }

        wxString errorMessage;
        if (!ReadSqliteTable(m_databasePicker->GetPath(), m_tableChoice->GetString(selection), &m_importedTable, &errorMessage)) {
            wxMessageBox(errorMessage, "Import SQLite Table", wxOK | wxICON_ERROR, this);
            return;
        }

        m_hasImportedTable = true;
        EndModal(wxID_OK);
    }

    wxFilePickerCtrl* m_databasePicker{nullptr};
    wxChoice* m_tableChoice{nullptr};
    wxStaticText* m_statusLabel{nullptr};
    std::vector<wxString> m_tables;
    ImportedSqliteTable m_importedTable;
    bool m_hasImportedTable{false};
};

} // namespace

bool ShowSqliteImportDialog(wxWindow* parent, ImportedSqliteTable* importedTable) {
    SqliteImportDialog dialog(parent);
    if (dialog.ShowModal() != wxID_OK) {
        return false;
    }

    return dialog.GetImportedTable(importedTable);
}
