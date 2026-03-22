#include "print_support.h"

#include "print_support_internal.h"

#include <algorithm>
#include <functional>
#include <utility>

#include <wx/artprov.h>
#include <wx/dcbuffer.h>
#include <wx/toolbar.h>
#include <wx/wx.h>

namespace {

enum {
    ID_PREVIEW_PREVIOUS_PAGE = wxID_HIGHEST + 500,
    ID_PREVIEW_NEXT_PAGE,
    ID_PREVIEW_PORTRAIT,
    ID_PREVIEW_LANDSCAPE
};

class PrintPreviewPanel final : public wxPanel {
public:
    PrintPreviewPanel(
        wxWindow* parent,
        const PrintableDocument& document,
        bool landscape,
        std::function<void(int)> pageScrollHandler)
        : wxPanel(parent, wxID_ANY),
          m_document(document),
          m_landscape(landscape),
          m_pageScrollHandler(std::move(pageScrollHandler)) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &PrintPreviewPanel::OnPaint, this);
        Bind(wxEVT_MOUSEWHEEL, &PrintPreviewPanel::OnMouseWheel, this);
    }

    void SetPageNumber(int pageNumber) {
        m_pageNumber = std::max(1, pageNumber);
        Refresh();
    }

    void SetLandscape(bool landscape) {
        m_landscape = landscape;
        Refresh();
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(wxColour(208, 208, 208)));
        dc.Clear();

        const wxSize client = GetClientSize();
        const wxSize logicalPage = GetLogicalPageSize(m_landscape);
        const double scale = std::max(
            0.1,
            std::min(
                static_cast<double>(std::max(80, client.GetWidth() - 32)) / logicalPage.GetWidth(),
                static_cast<double>(std::max(80, client.GetHeight() - 32)) / logicalPage.GetHeight()));

        const wxSize drawnPage(
            std::max(1, static_cast<int>(logicalPage.GetWidth() * scale)),
            std::max(1, static_cast<int>(logicalPage.GetHeight() * scale)));
        const wxRect pageRect(
            (client.GetWidth() - drawnPage.GetWidth()) / 2,
            (client.GetHeight() - drawnPage.GetHeight()) / 2,
            drawnPage.GetWidth(),
            drawnPage.GetHeight());

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(160, 160, 160)));
        dc.DrawRectangle(pageRect.x + 8, pageRect.y + 8, pageRect.width, pageRect.height);

        dc.SetBrush(*wxWHITE_BRUSH);
        dc.SetPen(*wxBLACK_PEN);
        dc.DrawRectangle(pageRect);

        dc.SetClippingRegion(pageRect);
        dc.SetDeviceOrigin(pageRect.x, pageRect.y);
        dc.SetUserScale(scale, scale);
        DrawPage(dc, m_document, m_pageNumber, m_landscape);
    }

    void OnMouseWheel(wxMouseEvent& event) {
        if (!m_pageScrollHandler) {
            event.Skip();
            return;
        }

        const int rotation = event.GetWheelRotation();
        if (rotation == 0) {
            event.Skip();
            return;
        }

        m_pageScrollHandler(rotation > 0 ? -1 : 1);
    }

    PrintableDocument m_document;
    bool m_landscape{false};
    int m_pageNumber{1};
    std::function<void(int)> m_pageScrollHandler;
};

class PrintPreviewFrame final : public wxFrame {
public:
    PrintPreviewFrame(wxWindow* parent, const PrintableDocument& document, wxPrintData printData)
        : wxFrame(parent, wxID_ANY, wxString::Format("Print Preview - %s", document.title), wxDefaultPosition, wxSize(1100, 800)),
          m_document(document),
          m_printData(std::move(printData)),
          m_landscape(m_printData.GetOrientation() == wxLANDSCAPE) {
        if (m_printData.GetOrientation() != wxLANDSCAPE && m_printData.GetOrientation() != wxPORTRAIT) {
            m_landscape = GuessLandscapeForPrint(document);
        }

        auto* toolbar = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);
        toolbar->AddTool(ID_PREVIEW_PREVIOUS_PAGE, "Previous", wxArtProvider::GetBitmapBundle(wxART_GO_BACK, wxART_TOOLBAR));
        toolbar->AddTool(ID_PREVIEW_NEXT_PAGE, "Next", wxArtProvider::GetBitmapBundle(wxART_GO_FORWARD, wxART_TOOLBAR));
        toolbar->AddSeparator();
        toolbar->AddRadioTool(ID_PREVIEW_PORTRAIT, "Portrait", wxArtProvider::GetBitmapBundle(wxART_NORMAL_FILE, wxART_TOOLBAR));
        toolbar->AddRadioTool(ID_PREVIEW_LANDSCAPE, "Landscape", wxArtProvider::GetBitmapBundle(wxART_REPORT_VIEW, wxART_TOOLBAR));
        toolbar->AddSeparator();
        toolbar->AddTool(wxID_PRINT, "Print", wxArtProvider::GetBitmapBundle(wxART_PRINT, wxART_TOOLBAR));
        toolbar->AddSeparator();
        m_pageLabel = new wxStaticText(toolbar, wxID_ANY, {});
        toolbar->AddControl(m_pageLabel);
        toolbar->Realize();

        m_previewPanel = new PrintPreviewPanel(
            this,
            m_document,
            m_landscape,
            [this](int delta) { ChangePage(delta); });
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_previewPanel, 1, wxEXPAND, 0);
        SetSizer(sizer);

        Bind(wxEVT_TOOL, &PrintPreviewFrame::OnPreviousPage, this, ID_PREVIEW_PREVIOUS_PAGE);
        Bind(wxEVT_TOOL, &PrintPreviewFrame::OnNextPage, this, ID_PREVIEW_NEXT_PAGE);
        Bind(wxEVT_TOOL, &PrintPreviewFrame::OnPortrait, this, ID_PREVIEW_PORTRAIT);
        Bind(wxEVT_TOOL, &PrintPreviewFrame::OnLandscape, this, ID_PREVIEW_LANDSCAPE);
        Bind(wxEVT_TOOL, &PrintPreviewFrame::OnPrint, this, wxID_PRINT);

        UpdateOrientation();
        CentreOnParent();
    }

private:
    void UpdateOrientation() {
        m_printData.SetOrientation(m_landscape ? wxLANDSCAPE : wxPORTRAIT);
        m_pageCount = GetPageCount(m_document, m_landscape);
        m_currentPage = std::clamp(m_currentPage, 1, std::max(1, m_pageCount));
        m_previewPanel->SetLandscape(m_landscape);
        m_previewPanel->SetPageNumber(m_currentPage);

        auto* toolbar = GetToolBar();
        if (toolbar) {
            toolbar->ToggleTool(ID_PREVIEW_PORTRAIT, !m_landscape);
            toolbar->ToggleTool(ID_PREVIEW_LANDSCAPE, m_landscape);
            toolbar->EnableTool(ID_PREVIEW_PREVIOUS_PAGE, m_currentPage > 1);
            toolbar->EnableTool(ID_PREVIEW_NEXT_PAGE, m_currentPage < m_pageCount);
        }

        if (m_pageLabel) {
            m_pageLabel->SetLabel(wxString::Format("Page %d of %d", m_currentPage, m_pageCount));
            GetToolBar()->Realize();
        }
        Layout();
    }

    void OnPreviousPage(wxCommandEvent&) {
        ChangePage(-1);
    }

    void OnNextPage(wxCommandEvent&) {
        ChangePage(1);
    }

    void OnPortrait(wxCommandEvent&) {
        m_landscape = false;
        UpdateOrientation();
    }

    void OnLandscape(wxCommandEvent&) {
        m_landscape = true;
        UpdateOrientation();
    }

    void OnPrint(wxCommandEvent&) {
        ShowPrintDialogForDocument(this, m_document, m_printData);
    }

    void ChangePage(int delta) {
        const int nextPage = std::clamp(m_currentPage + delta, 1, m_pageCount);
        if (nextPage == m_currentPage) {
            return;
        }

        m_currentPage = nextPage;
        UpdateOrientation();
    }

    PrintableDocument m_document;
    wxPrintData m_printData;
    PrintPreviewPanel* m_previewPanel{nullptr};
    wxStaticText* m_pageLabel{nullptr};
    bool m_landscape{false};
    int m_currentPage{1};
    int m_pageCount{1};
};

} // namespace

void ShowPrintPreviewWindow(wxWindow* parent, const PrintableDocument& document, wxPrintData& printData) {
    if (printData.GetOrientation() != wxLANDSCAPE && printData.GetOrientation() != wxPORTRAIT) {
        printData.SetOrientation(GuessLandscapeForPrint(document) ? wxLANDSCAPE : wxPORTRAIT);
    }

    auto* frame = new PrintPreviewFrame(parent, document, printData);
    frame->Show();
}
