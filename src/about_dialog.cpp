#include "about_dialog.h"

#include <sqlite3.h>
#include <wx/wx.h>
#include <wx/hyperlink.h>
#include <wx/statline.h>
#include <wx/mstream.h>
#include <wx/image.h>

#include "config.h"
#include "csv_explorer_png_data.h"

namespace {

wxString GetWxWidgetsVersion() {
    return wxString::Format("%d.%d.%d", wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER);
}

wxString GetSqliteVersion() {
    return wxString::FromUTF8(sqlite3_libversion());
}

wxBitmap LoadAppBitmap(int size) {
    wxMemoryInputStream stream(assets_csv_explorer_png, assets_csv_explorer_png_len);
    wxImage image;
    {
        wxLogNull noLog;
        if (!image.LoadFile(stream, wxBITMAP_TYPE_PNG) || !image.IsOk()) {
            return {};
        }
    }
    if (size > 0) {
        image.Rescale(size, size, wxIMAGE_QUALITY_HIGH);
    }
    return wxBitmap(image);
}

class AboutDialog : public wxDialog {
public:
    explicit AboutDialog(wxWindow* parent)
        : wxDialog(parent, wxID_ANY, "About CSV Explorer", wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
        auto* root = new wxBoxSizer(wxVERTICAL);

        wxBitmap logo = LoadAppBitmap(96);
        if (logo.IsOk()) {
            root->Add(new wxStaticBitmap(this, wxID_ANY, logo), 0, wxTOP | wxLEFT | wxRIGHT | wxALIGN_CENTER_HORIZONTAL, 16);
        }

        auto* details = new wxBoxSizer(wxVERTICAL);

        auto* title = new wxStaticText(this, wxID_ANY, CSV_EXPLORER_NAME);
        wxFont titleFont = title->GetFont();
        titleFont.MakeBold();
        titleFont.SetPointSize(titleFont.GetPointSize() + 4);
        title->SetFont(titleFont);
        details->Add(title, 0, wxBOTTOM | wxALIGN_CENTER_HORIZONTAL, 8);

        auto addCenteredLabel = [this, details](const wxString& text, int bottomMargin) {
            auto* label = new wxStaticText(this, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
            details->Add(label, 0, wxBOTTOM | wxALIGN_CENTER_HORIZONTAL, bottomMargin);
        };

        addCenteredLabel(wxString::Format("Version: %s", CSV_EXPLORER_VERSION), 4);
        addCenteredLabel(wxString::Format("Commit: %s", CSV_EXPLORER_GIT_COMMIT_SHORT), 4);
        addCenteredLabel(wxString::Format("License: %s", CSV_EXPLORER_LICENSE), 4);
        addCenteredLabel(wxString::Format("Author: %s", CSV_EXPLORER_AUTHOR), 8);

        auto* repositoryLink = new wxHyperlinkCtrl(
            this,
            wxID_ANY,
            CSV_EXPLORER_REPOSITORY_URL,
            CSV_EXPLORER_REPOSITORY_URL,
            wxDefaultPosition,
            wxDefaultSize,
            wxHL_ALIGN_CENTRE);
        details->Add(repositoryLink, 0, wxBOTTOM | wxALIGN_CENTER_HORIZONTAL, 4);

        root->Add(details, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 16);

        root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 16);

        auto* poweredBy = new wxBoxSizer(wxVERTICAL);

        auto* poweredByTitle = new wxStaticText(this, wxID_ANY, "Powered by");
        wxFont sectionFont = poweredByTitle->GetFont();
        sectionFont.MakeBold();
        poweredByTitle->SetFont(sectionFont);
        poweredBy->Add(poweredByTitle, 0, wxBOTTOM | wxALIGN_CENTER_HORIZONTAL, 8);

        auto addDependencyLabel = [this, poweredBy](const wxString& name, const wxString& version) {
            poweredBy->Add(
                new wxStaticText(this, wxID_ANY, wxString::Format("%s %s", name, version), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL),
                0,
                wxBOTTOM | wxALIGN_CENTER_HORIZONTAL,
                4);
        };

        addDependencyLabel("wxWidgets", GetWxWidgetsVersion());
        addDependencyLabel("SQLite", GetSqliteVersion());

        root->Add(poweredBy, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 16);

        auto* closeButton = new wxButton(this, wxID_OK, "Close");
        closeButton->SetMinSize(FromDIP(wxSize(140, -1)));
        root->Add(closeButton, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxALIGN_CENTER_HORIZONTAL, 16);

        SetSizerAndFit(root);
        SetMinSize(FromDIP(wxSize(460, 220)));
        CentreOnScreen();
        closeButton->SetFocus();

        wxIcon icon = LoadAppIcon();
        if (icon.IsOk()) {
            SetIcon(icon);
        }
    }
};

} // namespace

wxIcon LoadAppIcon() {
    wxBitmap bitmap = LoadAppBitmap(128);
    if (!bitmap.IsOk()) {
        return {};
    }

    wxIcon icon;
    icon.CopyFromBitmap(bitmap);
    return icon;
}

void ShowAboutDialog(wxWindow* parent) {
    AboutDialog dialog(parent);
    dialog.ShowModal();
}
