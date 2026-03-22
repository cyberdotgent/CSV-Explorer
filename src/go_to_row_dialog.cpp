#include "go_to_row_dialog.h"

#include <wx/wx.h>
#include <wx/valtext.h>

namespace {

class GoToRowDialog : public wxDialog {
public:
    GoToRowDialog(wxWindow* parent, int maxRows, int currentRow)
        : wxDialog(parent, wxID_ANY, "Go To Row", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_maxRows(maxRows) {
        auto* root = new wxBoxSizer(wxVERTICAL);

        auto* description = new wxStaticText(
            this,
            wxID_ANY,
            wxString::Format("Enter a row number between 1 and %d.", m_maxRows));
        root->Add(description, 0, wxALL | wxEXPAND, 16);

        auto* maxLabel = new wxStaticText(this, wxID_ANY, wxString::Format("Maximum rows: %d", m_maxRows));
        root->Add(maxLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 16);

        auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);
        rowSizer->Add(new wxStaticText(this, wxID_ANY, "Row"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        wxTextValidator validator(wxFILTER_DIGITS);
        m_rowInput = new wxTextCtrl(
            this,
            wxID_ANY,
            wxString::Format("%d", currentRow + 1),
            wxDefaultPosition,
            FromDIP(wxSize(140, -1)),
            wxTE_PROCESS_ENTER,
            validator);
        rowSizer->Add(m_rowInput, 1, wxEXPAND, 0);
        root->Add(rowSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 16);

        m_errorLabel = new wxStaticText(this, wxID_ANY, {});
        wxColour errorColor(180, 0, 0);
        m_errorLabel->SetForegroundColour(errorColor);
        root->Add(m_errorLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 16);

        auto* buttons = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
        root->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 16);

        SetSizerAndFit(root);
        SetMinSize(FromDIP(wxSize(360, 180)));
        CentreOnParent();

        m_rowInput->Bind(wxEVT_TEXT, &GoToRowDialog::OnTextChanged, this);
        m_rowInput->Bind(wxEVT_TEXT_ENTER, &GoToRowDialog::OnTextEnter, this);

        ValidateInput();
        m_rowInput->SetFocus();
        m_rowInput->SelectAll();
    }

    int GetSelectedRow() const {
        return m_selectedRow;
    }

private:
    void OnTextChanged(wxCommandEvent&) {
        ValidateInput();
    }

    void OnTextEnter(wxCommandEvent&) {
        if (ValidateInput()) {
            EndModal(wxID_OK);
        }
    }

    bool ValidateInput() {
        long value = 0;
        const bool hasValue = m_rowInput->GetValue().ToLong(&value);
        const bool isValid = hasValue && value >= 1 && value <= m_maxRows;

        m_selectedRow = isValid ? static_cast<int>(value - 1) : -1;

        auto* okButton = FindWindow(wxID_OK);
        if (okButton) {
            okButton->Enable(isValid);
        }

        if (isValid) {
            m_errorLabel->SetLabel({});
        } else {
            m_errorLabel->SetLabel(wxString::Format("Row number must be between 1 and %d.", m_maxRows));
        }

        Layout();
        Fit();
        return isValid;
    }

    wxTextCtrl* m_rowInput{nullptr};
    wxStaticText* m_errorLabel{nullptr};
    int m_maxRows{0};
    int m_selectedRow{-1};
};

} // namespace

bool ShowGoToRowDialog(wxWindow* parent, int maxRows, int currentRow, int* selectedRow) {
    if (!selectedRow || maxRows <= 0) {
        return false;
    }

    GoToRowDialog dialog(parent, maxRows, currentRow);
    if (dialog.ShowModal() != wxID_OK) {
        return false;
    }

    *selectedRow = dialog.GetSelectedRow();
    return true;
}
