#include "print_support.h"

#include "print_support_internal.h"

#include <algorithm>

#include <wx/printdlg.h>

namespace {

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
