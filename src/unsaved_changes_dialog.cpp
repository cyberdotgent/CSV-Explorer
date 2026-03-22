#include "unsaved_changes_dialog.h"

#include <wx/wx.h>

DirtyFileAction ShowDirtyFileDialog(wxWindow* parent, const wxString& documentName) {
    wxDialog dialog(
        parent,
        wxID_ANY,
        "Unsaved Changes",
        wxDefaultPosition,
        wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(
        new wxStaticText(
            &dialog,
            wxID_ANY,
            wxString::Format("Save changes to %s before continuing?", documentName)),
        0,
        wxALL | wxEXPAND,
        16);

    root->Add(
        new wxStaticText(
            &dialog,
            wxID_ANY,
            "Choose Save to write to the current file, Save As to choose a new path, Don't Save to discard changes, or Cancel to stay here."),
        0,
        wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
        16);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* saveButton = new wxButton(&dialog, wxID_YES, "Save");
    auto* saveAsButton = new wxButton(&dialog, wxID_APPLY, "Save As...");
    auto* discardButton = new wxButton(&dialog, wxID_NO, "Don't Save");
    auto* cancelButton = new wxButton(&dialog, wxID_CANCEL, "Cancel");

    buttons->Add(saveButton, 0, wxRIGHT, 8);
    buttons->Add(saveAsButton, 0, wxRIGHT, 8);
    buttons->Add(discardButton, 0, wxRIGHT, 8);
    buttons->Add(cancelButton, 0, 0, 0);
    root->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxALIGN_RIGHT, 16);

    dialog.SetSizerAndFit(root);
    dialog.SetMinSize(dialog.FromDIP(wxSize(520, 180)));
    dialog.CentreOnParent();
    dialog.SetEscapeId(wxID_CANCEL);
    dialog.SetAffirmativeId(wxID_YES);
    saveButton->SetDefault();
    saveButton->SetFocus();
    saveButton->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_YES); });
    saveAsButton->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_APPLY); });
    discardButton->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_NO); });
    cancelButton->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_CANCEL); });

    const int result = dialog.ShowModal();
    if (result == wxID_YES) {
        return DirtyFileAction::Save;
    }
    if (result == wxID_APPLY) {
        return DirtyFileAction::SaveAs;
    }
    if (result == wxID_NO) {
        return DirtyFileAction::Discard;
    }
    return DirtyFileAction::Cancel;
}
