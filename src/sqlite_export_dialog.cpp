#include "sqlite_import_dialog.h"

#include "sqlite_dialog_common.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/filepicker.h>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <vector>

namespace {

class SqliteExportDialog final : public wxDialog {
public:
    SqliteExportDialog(wxWindow* parent, const ImportedSqliteTable& table)
        : wxDialog(parent, wxID_ANY, "Export To SQLite Database", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_table(table),
          m_columnNames(sqlite_dialog::BuildExportColumnNames(table)) {
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
            wxFLP_SAVE);
        topSizer->Add(m_databasePicker, 0, wxEXPAND | wxALL, FromDIP(12));

        m_databasePathLabel = new wxStaticText(this, wxID_ANY, "No database selected");
        topSizer->Add(m_databasePathLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

        auto* tableLabel = new wxStaticText(this, wxID_ANY, "Table name");
        topSizer->Add(tableLabel, 0, wxLEFT | wxRIGHT, FromDIP(12));

        m_tableNameCtrl = new wxTextCtrl(this, wxID_ANY, sqlite_dialog::SuggestedTableName(table.documentName));
        topSizer->Add(m_tableNameCtrl, 0, wxEXPAND | wxALL, FromDIP(12));

        auto* mappingLabel = new wxStaticText(this, wxID_ANY, "Column types");
        topSizer->Add(mappingLabel, 0, wxLEFT | wxRIGHT, FromDIP(12));

        auto* mappingPanel = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_THEME);
        mappingPanel->SetScrollRate(0, FromDIP(16));
        mappingPanel->SetMinSize(FromDIP(wxSize(-1, 260)));
        mappingPanel->SetMaxSize(FromDIP(wxSize(-1, 260)));

        auto* mappingSizer = new wxFlexGridSizer(3, FromDIP(8), FromDIP(12));
        mappingSizer->AddGrowableCol(1, 1);
        mappingSizer->Add(new wxStaticText(mappingPanel, wxID_ANY, "Column"), 0, wxALIGN_CENTER_VERTICAL);
        mappingSizer->Add(new wxStaticText(mappingPanel, wxID_ANY, "Field name"), 0, wxALIGN_CENTER_VERTICAL);
        mappingSizer->Add(new wxStaticText(mappingPanel, wxID_ANY, "SQLite type"), 0, wxALIGN_CENTER_VERTICAL);

        mappingSizer->Add(new wxStaticText(mappingPanel, wxID_ANY, "ID"), 0, wxALIGN_CENTER_VERTICAL);
        mappingSizer->Add(new wxStaticText(mappingPanel, wxID_ANY, "ID"), 0, wxALIGN_CENTER_VERTICAL);
        mappingSizer->Add(new wxStaticText(mappingPanel, wxID_ANY, "INTEGER PRIMARY KEY AUTOINCREMENT"), 0, wxALIGN_CENTER_VERTICAL);

        static const wxString typeChoices[] = { "TEXT", "INTEGER", "REAL", "NUMERIC", "BLOB" };
        for (size_t i = 0; i < m_columnNames.size(); ++i) {
            const wxString originalLabel = i < m_table.headers.size() && !m_table.headers[i].IsEmpty()
                ? m_table.headers[i]
                : wxString::Format("Column %zu", i + 1);
            mappingSizer->Add(new wxStaticText(mappingPanel, wxID_ANY, originalLabel), 0, wxALIGN_CENTER_VERTICAL);
            mappingSizer->Add(new wxStaticText(mappingPanel, wxID_ANY, m_columnNames[i]), 0, wxALIGN_CENTER_VERTICAL);

            auto* choice = new wxChoice(mappingPanel, wxID_ANY);
            for (const wxString& typeChoice : typeChoices) {
                choice->Append(typeChoice);
            }
            choice->SetStringSelection("TEXT");
            m_typeChoices.push_back(choice);
            mappingSizer->Add(choice, 0, wxEXPAND);
        }

        auto* mappingPanelSizer = new wxBoxSizer(wxVERTICAL);
        mappingPanelSizer->Add(mappingSizer, 0, wxEXPAND | wxALL, FromDIP(12));
        mappingPanel->SetSizer(mappingPanelSizer);
        mappingPanel->FitInside();
        topSizer->Add(mappingPanel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

        m_statusLabel = new wxStaticText(this, wxID_ANY, wxString::Format("%zu rows will be exported.", m_table.rows.size()));
        topSizer->Add(m_statusLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

        auto* buttonSizer = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
        topSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, FromDIP(12));

        SetSizerAndFit(topSizer);
        SetMinSize(FromDIP(wxSize(640, 320)));

        if (wxButton* okButton = dynamic_cast<wxButton*>(FindWindow(wxID_OK))) {
            okButton->SetLabel("Export");
            okButton->Disable();
        }

        m_databasePicker->Bind(wxEVT_FILEPICKER_CHANGED, &SqliteExportDialog::OnDatabaseChanged, this);
        m_tableNameCtrl->Bind(wxEVT_TEXT, &SqliteExportDialog::OnTableNameChanged, this);
        Bind(wxEVT_BUTTON, &SqliteExportDialog::OnExport, this, wxID_OK);
        UpdateExportButton();
    }

private:
    void UpdateExportButton() {
        if (m_databasePathLabel) {
            const wxString path = m_databasePicker->GetPath();
            m_databasePathLabel->SetLabel(path.IsEmpty() ? "No database selected" : path);
            m_databasePathLabel->Wrap(FromDIP(600));
            Layout();
        }

        if (wxWindow* button = FindWindow(wxID_OK)) {
            button->Enable(!m_databasePicker->GetPath().IsEmpty() && !m_tableNameCtrl->GetValue().Trim(true).Trim(false).IsEmpty());
        }
    }

    void OnTableNameChanged(wxCommandEvent&) {
        UpdateExportButton();
    }

    void OnDatabaseChanged(wxFileDirPickerEvent&) {
        UpdateExportButton();
    }

    void OnExport(wxCommandEvent&) {
        std::vector<wxString> columnTypes;
        columnTypes.reserve(m_typeChoices.size());
        for (wxChoice* choice : m_typeChoices) {
            columnTypes.push_back(choice->GetStringSelection().IsEmpty() ? "TEXT" : choice->GetStringSelection());
        }

        const wxString databasePath = m_databasePicker->GetPath();
        const wxString tableName = m_tableNameCtrl->GetValue().Trim(true).Trim(false);

        wxString errorMessage;
        if (!sqlite_dialog::WriteSqliteTable(this, databasePath, tableName, m_columnNames, columnTypes, m_table.rows, &errorMessage)) {
            wxMessageBox(
                wxString::Format(
                    "Unable to export to SQLite database.\n\nDatabase: %s\nTable: %s\n\n%s",
                    databasePath,
                    tableName,
                    errorMessage),
                "Export To SQLite Database",
                wxOK | wxICON_ERROR,
                this);
            return;
        }

        EndModal(wxID_OK);
    }

    const ImportedSqliteTable& m_table;
    std::vector<wxString> m_columnNames;
    wxFilePickerCtrl* m_databasePicker{nullptr};
    wxStaticText* m_databasePathLabel{nullptr};
    wxTextCtrl* m_tableNameCtrl{nullptr};
    wxStaticText* m_statusLabel{nullptr};
    std::vector<wxChoice*> m_typeChoices;
};

} // namespace

bool ShowSqliteExportDialog(wxWindow* parent, const ImportedSqliteTable& table) {
    SqliteExportDialog dialog(parent, table);
    return dialog.ShowModal() == wxID_OK;
}
