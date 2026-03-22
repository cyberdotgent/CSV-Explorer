#pragma once

class wxString;
class wxWindow;

enum class DirtyFileAction {
    Save,
    SaveAs,
    Discard,
    Cancel
};

DirtyFileAction ShowDirtyFileDialog(wxWindow* parent, const wxString& documentName);
