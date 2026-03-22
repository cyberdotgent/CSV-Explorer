#include "sqlite_import_dialog.h"

#include "sqlite_dialog_common.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/filepicker.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <vector>

namespace {

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
            "SQLite databases (*.sqlite)|*.sqlite|All files (*.*)|*.*",
            wxDefaultPosition,
            wxDefaultSize,
            wxFLP_OPEN | wxFLP_FILE_MUST_EXIST);
        topSizer->Add(m_databasePicker, 0, wxEXPAND | wxALL, FromDIP(12));

        m_databasePathLabel = new wxStaticText(this, wxID_ANY, "No database selected");
        topSizer->Add(m_databasePathLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

        auto* tableLabel = new wxStaticText(this, wxID_ANY, "Table");
        topSizer->Add(tableLabel, 0, wxLEFT | wxRIGHT, FromDIP(12));

        m_tableChoice = new wxChoice(this, wxID_ANY);
        topSizer->Add(m_tableChoice, 0, wxEXPAND | wxALL, FromDIP(12));

        m_statusLabel = new wxStaticText(this, wxID_ANY, {});
        topSizer->Add(m_statusLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

        auto* buttonSizer = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
        topSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, FromDIP(12));

        SetSizerAndFit(topSizer);
        SetMinSize(FromDIP(wxSize(720, 240)));

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
        m_databasePathLabel->SetLabel(path.IsEmpty() ? "No database selected" : path);
        m_databasePathLabel->Wrap(FromDIP(680));

        if (path.IsEmpty()) {
            UpdateImportButton();
            Layout();
            return;
        }

        wxString errorMessage;
        if (!sqlite_dialog::LoadSqliteTableNames(path, &m_tables, &errorMessage)) {
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
        if (!sqlite_dialog::ReadSqliteTable(m_databasePicker->GetPath(), m_tableChoice->GetString(selection), &m_importedTable, &errorMessage)) {
            wxMessageBox(errorMessage, "Import SQLite Table", wxOK | wxICON_ERROR, this);
            return;
        }

        m_hasImportedTable = true;
        EndModal(wxID_OK);
    }

    wxFilePickerCtrl* m_databasePicker{nullptr};
    wxStaticText* m_databasePathLabel{nullptr};
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
