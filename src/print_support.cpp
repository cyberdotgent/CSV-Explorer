#include "print_support.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include <wx/artprov.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/printdlg.h>
#include <wx/toolbar.h>
#include <wx/wx.h>

namespace {

enum {
    ID_PREVIEW_PREVIOUS_PAGE = wxID_HIGHEST + 500,
    ID_PREVIEW_NEXT_PAGE,
    ID_PREVIEW_PORTRAIT,
    ID_PREVIEW_LANDSCAPE
};

wxSize GetLogicalPageSize(bool landscape) {
    const wxSize portrait(1240, 1754);
    return landscape ? wxSize(portrait.GetHeight(), portrait.GetWidth()) : portrait;
}

struct PrintLayout {
    wxFont titleFont;
    wxFont headerFont;
    wxFont cellFont;
    int marginLeft{64};
    int marginTop{64};
    int marginRight{64};
    int marginBottom{64};
    int titleHeight{0};
    int headerHeight{0};
    int rowHeight{0};
    int footerHeight{0};
    int tableTop{0};
    int rowsPerPage{1};
    int pageCount{1};
    std::vector<int> columnWidths;
};

wxString GetHeaderTitle(const PrintableDocument& document, int col) {
    if (col >= 0 && col < static_cast<int>(document.headers.size()) && !document.headers[col].IsEmpty()) {
        return document.headers[col];
    }
    return wxString::Format("Column %d", col + 1);
}

unsigned int GetColumnCount(const PrintableDocument& document) {
    unsigned int columnCount = static_cast<unsigned int>(document.headers.size());
    for (const auto& row : document.rows) {
        columnCount = std::max(columnCount, static_cast<unsigned int>(row.size()));
    }
    return std::max(columnCount, 1u);
}

wxString GetCellText(const PrintableDocument& document, unsigned int row, unsigned int col) {
    if (row < document.rows.size() && col < document.rows[row].size()) {
        return document.rows[row][col];
    }
    return {};
}

void DrawClippedText(wxDC& dc, const wxString& text, const wxRect& rect, int horizontalPadding = 8) {
    wxDCClipper clipper(dc, rect);
    const int textHeight = dc.GetTextExtent("Ag").GetHeight();
    const int textY = rect.y + std::max(0, (rect.height - textHeight) / 2);
    dc.DrawText(text, rect.x + horizontalPadding, textY);
}

PrintLayout BuildPrintLayout(wxDC& dc, const PrintableDocument& document, const wxSize& pageSize) {
    PrintLayout layout;

    const int baseSize = std::max(18, pageSize.GetHeight() / 110);
    layout.titleFont = wxFont(wxFontInfo(baseSize + 6).Family(wxFONTFAMILY_SWISS).Bold());
    layout.headerFont = wxFont(wxFontInfo(baseSize + 1).Family(wxFONTFAMILY_SWISS).Bold());
    layout.cellFont = wxFont(wxFontInfo(baseSize).Family(wxFONTFAMILY_SWISS));

    layout.marginLeft = std::max(48, pageSize.GetWidth() / 18);
    layout.marginRight = layout.marginLeft;
    layout.marginTop = std::max(48, pageSize.GetHeight() / 20);
    layout.marginBottom = std::max(48, pageSize.GetHeight() / 20);

    dc.SetFont(layout.titleFont);
    layout.titleHeight = dc.GetTextExtent(document.title.IsEmpty() ? "CSV Explorer" : document.title).GetHeight() + 20;

    dc.SetFont(layout.headerFont);
    layout.headerHeight = dc.GetTextExtent("Ag").GetHeight() + 22;

    dc.SetFont(layout.cellFont);
    layout.rowHeight = dc.GetTextExtent("Ag").GetHeight() + 18;
    layout.footerHeight = dc.GetTextExtent("Page 999").GetHeight() + 16;

    layout.tableTop = layout.marginTop + layout.titleHeight + 12;

    const unsigned int columnCount = GetColumnCount(document);
    const int availableWidth = std::max(120, pageSize.GetWidth() - layout.marginLeft - layout.marginRight);

    layout.columnWidths.resize(columnCount, 80);
    int totalWidth = 0;
    for (unsigned int col = 0; col < columnCount; ++col) {
        dc.SetFont(layout.headerFont);
        int width = dc.GetTextExtent(GetHeaderTitle(document, static_cast<int>(col))).GetWidth() + 26;

        dc.SetFont(layout.cellFont);
        const unsigned int sampleRows = std::min<unsigned int>(static_cast<unsigned int>(document.rows.size()), 40u);
        for (unsigned int row = 0; row < sampleRows; ++row) {
            width = std::max(width, dc.GetTextExtent(GetCellText(document, row, col)).GetWidth() + 26);
        }

        width = std::clamp(width, 70, 260);
        layout.columnWidths[col] = width;
        totalWidth += width;
    }

    if (totalWidth > availableWidth && totalWidth > 0) {
        const double scale = static_cast<double>(availableWidth) / static_cast<double>(totalWidth);
        int adjustedTotal = 0;
        for (int& width : layout.columnWidths) {
            width = std::max(44, static_cast<int>(std::floor(width * scale)));
            adjustedTotal += width;
        }

        int remaining = availableWidth - adjustedTotal;
        for (int i = 0; remaining > 0 && !layout.columnWidths.empty(); ++i, --remaining) {
            layout.columnWidths[static_cast<size_t>(i) % layout.columnWidths.size()] += 1;
        }
    }

    const int availableHeight = std::max(
        layout.rowHeight,
        pageSize.GetHeight() - layout.tableTop - layout.marginBottom - layout.footerHeight - layout.headerHeight);
    layout.rowsPerPage = std::max(1, availableHeight / layout.rowHeight);

    const int totalRows = std::max(1, static_cast<int>(document.rows.size()));
    layout.pageCount = std::max(1, static_cast<int>(std::ceil(static_cast<double>(totalRows) / layout.rowsPerPage)));
    return layout;
}

void DrawPage(wxDC& dc, const PrintableDocument& document, int pageNumber, bool landscape) {
    const wxSize pageSize = GetLogicalPageSize(landscape);
    const PrintLayout layout = BuildPrintLayout(dc, document, pageSize);

    dc.SetBackground(wxBrush(*wxWHITE));
    dc.Clear();

    dc.SetTextForeground(*wxBLACK);
    dc.SetPen(*wxBLACK_PEN);

    dc.SetFont(layout.titleFont);
    dc.DrawText(document.title.IsEmpty() ? "untitled.csv" : document.title, layout.marginLeft, layout.marginTop);

    int x = layout.marginLeft;
    int y = layout.tableTop;

    dc.SetFont(layout.headerFont);
    dc.SetBrush(wxBrush(wxColour(240, 240, 240)));
    for (size_t col = 0; col < layout.columnWidths.size(); ++col) {
        const wxRect rect(x, y, layout.columnWidths[col], layout.headerHeight);
        dc.DrawRectangle(rect);
        DrawClippedText(dc, GetHeaderTitle(document, static_cast<int>(col)), rect);
        x += layout.columnWidths[col];
    }

    dc.SetFont(layout.cellFont);
    dc.SetBrush(*wxWHITE_BRUSH);

    const int startRow = (pageNumber - 1) * layout.rowsPerPage;
    const int endRow = std::min(startRow + layout.rowsPerPage, std::max(1, static_cast<int>(document.rows.size())));
    y += layout.headerHeight;

    for (int row = startRow; row < endRow; ++row) {
        x = layout.marginLeft;
        for (size_t col = 0; col < layout.columnWidths.size(); ++col) {
            const wxRect rect(x, y, layout.columnWidths[col], layout.rowHeight);
            dc.DrawRectangle(rect);
            DrawClippedText(dc, GetCellText(document, static_cast<unsigned int>(row), static_cast<unsigned int>(col)), rect);
            x += layout.columnWidths[col];
        }
        y += layout.rowHeight;
    }

    dc.SetFont(layout.cellFont);
    const wxString footer = wxString::Format("Page %d of %d", pageNumber, layout.pageCount);
    const int footerY = pageSize.GetHeight() - layout.marginBottom + 8;
    dc.DrawText(footer, layout.marginLeft, footerY);
}

int GetPageCount(const PrintableDocument& document, bool landscape) {
    wxBitmap bitmap(8, 8);
    wxMemoryDC dc(bitmap);
    return BuildPrintLayout(dc, document, GetLogicalPageSize(landscape)).pageCount;
}

class CsvPrintout final : public wxPrintout {
public:
    CsvPrintout(const PrintableDocument& document, bool landscape)
        : wxPrintout(document.title), m_document(document), m_landscape(landscape) {}

    bool OnPrintPage(int pageNum) override {
        wxDC* dc = GetDC();
        if (!dc) {
            return false;
        }

        const wxSize logicalPage = GetLogicalPageSize(m_landscape);
        int actualPageWidth = logicalPage.GetWidth();
        int actualPageHeight = logicalPage.GetHeight();
        GetPageSizePixels(&actualPageWidth, &actualPageHeight);

        const double scale = std::min(
            static_cast<double>(actualPageWidth) / logicalPage.GetWidth(),
            static_cast<double>(actualPageHeight) / logicalPage.GetHeight());
        const int offsetX = std::max(0, static_cast<int>((actualPageWidth - logicalPage.GetWidth() * scale) / 2.0));
        const int offsetY = std::max(0, static_cast<int>((actualPageHeight - logicalPage.GetHeight() * scale) / 2.0));

        dc->SetDeviceOrigin(offsetX, offsetY);
        dc->SetUserScale(scale, scale);
        DrawPage(*dc, m_document, pageNum, m_landscape);
        return true;
    }

    bool HasPage(int pageNum) override {
        return pageNum >= 1 && pageNum <= GetPageCount(m_document, m_landscape);
    }

    void GetPageInfo(int* minPage, int* maxPage, int* pageFrom, int* pageTo) override {
        const int pageCount = GetPageCount(m_document, m_landscape);
        *minPage = 1;
        *maxPage = pageCount;
        *pageFrom = 1;
        *pageTo = pageCount;
    }

private:
    PrintableDocument m_document;
    bool m_landscape{false};
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

bool GuessLandscapeForPrint(const PrintableDocument& document) {
    const unsigned int columnCount = GetColumnCount(document);
    if (columnCount >= 7) {
        return true;
    }

    size_t maxMeasuredWidth = 0;
    for (unsigned int col = 0; col < columnCount; ++col) {
        maxMeasuredWidth += GetHeaderTitle(document, static_cast<int>(col)).Length();
        const unsigned int sampleRows = std::min<unsigned int>(static_cast<unsigned int>(document.rows.size()), 20u);
        for (unsigned int row = 0; row < sampleRows; ++row) {
            maxMeasuredWidth = std::max(maxMeasuredWidth, static_cast<size_t>(GetCellText(document, row, col).Length()));
        }
    }

    return columnCount >= 5 || maxMeasuredWidth > 90;
}

bool ShowPrintDialogForDocument(wxWindow* parent, const PrintableDocument& document, wxPrintData& printData) {
    const bool landscape = printData.GetOrientation() == wxLANDSCAPE;
    printData.SetOrientation(landscape ? wxLANDSCAPE : wxPORTRAIT);

    wxPrintDialogData dialogData(printData);
    dialogData.EnableSelection(false);
    dialogData.EnablePageNumbers(true);

    wxPrinter printer(&dialogData);
    CsvPrintout printout(document, printData.GetOrientation() == wxLANDSCAPE);
    const bool printed = printer.Print(parent, &printout, true);
    if (printed) {
        printData = printer.GetPrintDialogData().GetPrintData();
    }
    return printed;
}

void ShowPrintPreviewWindow(wxWindow* parent, const PrintableDocument& document, wxPrintData& printData) {
    if (printData.GetOrientation() != wxLANDSCAPE && printData.GetOrientation() != wxPORTRAIT) {
        printData.SetOrientation(GuessLandscapeForPrint(document) ? wxLANDSCAPE : wxPORTRAIT);
    }

    auto* frame = new PrintPreviewFrame(parent, document, printData);
    frame->Show();
}
