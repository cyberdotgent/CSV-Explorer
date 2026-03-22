#include "print_support_internal.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <wx/dcmemory.h>
#include <wx/wx.h>

namespace {

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

} // namespace

wxSize GetLogicalPageSize(bool landscape) {
    const wxSize portrait(1240, 1754);
    return landscape ? wxSize(portrait.GetHeight(), portrait.GetWidth()) : portrait;
}

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
