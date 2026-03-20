#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/fdrepdlg.h>
#include <wx/textfile.h>
#include <wx/filename.h>
#include <wx/clipbrd.h>
#include <wx/mstream.h>
#include <wx/image.h>

#include <memory>
#include <vector>
#include <algorithm>

#include "config.h"
#include "wxcsv_png_data.h"

namespace {

enum {
    ID_FIND_NEXT = wxID_HIGHEST + 1,
    ID_FIND_PREVIOUS,
    ID_CONTEXT_COPY_ROW,
    ID_CONTEXT_COPY_CELL
};

std::vector<wxString> ParseCsvLine(const wxString& line) {
    std::vector<wxString> fields;
    wxString field;
    bool inQuotes = false;

    for (size_t i = 0; i < line.Length(); ++i) {
        const wxUniChar ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.Length() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += ch;
        }
    }
    fields.push_back(field);
    return fields;
}

bool ParseCsvFile(const wxString& path, std::vector<wxString>& outHeaders, std::vector<std::vector<wxString>>& outRows) {
    wxTextFile file(path);
    if (!file.Open(path)) {
        return false;
    }

    outHeaders.clear();
    outRows.clear();
    const size_t lineCount = file.GetLineCount();
    if (lineCount > 0) {
        wxString headerLine = file.GetLine(0);
        if (!headerLine.empty() && headerLine.Last() == '\r') {
            headerLine.Truncate(headerLine.Length() - 1);
        }
        outHeaders = ParseCsvLine(headerLine);
    }
    for (size_t i = 1; i < lineCount; ++i) {
        wxString line = file.GetLine(i);
        if (!line.empty() && line.Last() == '\r') {
            line.Truncate(line.Length() - 1);
        }
        outRows.push_back(ParseCsvLine(line));
    }
    return true;
}

bool ContainsText(const wxString& source, const wxString& target, bool matchCase) {
    if (target.IsEmpty()) {
        return false;
    }
    if (matchCase) {
        return source.Find(target) != wxNOT_FOUND;
    }
    return source.Lower().Find(target.Lower()) != wxNOT_FOUND;
}

} // namespace

class CsvDataModel : public wxDataViewVirtualListModel {
public:
    void SetRows(std::vector<std::vector<wxString>> rows, unsigned int minimumColumnCount = 0) {
        m_rows = std::move(rows);
        m_columnCount = 0;
        for (const auto& row : m_rows) {
            m_columnCount = std::max(m_columnCount, static_cast<unsigned int>(row.size()));
        }
        m_columnCount = std::max(m_columnCount, minimumColumnCount);
        Reset(m_rows.size());
    }

    unsigned int GetColumnCount() const override {
        return m_columnCount;
    }

    wxString GetColumnType(unsigned int) const override {
        return "string";
    }

    unsigned int GetCount() const override {
        return static_cast<unsigned int>(m_rows.size());
    }

    void GetValueByRow(wxVariant& variant, unsigned int row, unsigned int col) const override {
        variant = GetCellText(row, col);
    }

    bool SetValueByRow(const wxVariant&, unsigned int, unsigned int) override {
        return false;
    }

    wxString GetCellText(unsigned int row, unsigned int col) const {
        if (row < m_rows.size() && col < m_rows[row].size()) {
            return m_rows[row][col];
        }
        return {};
    }

private:
    std::vector<std::vector<wxString>> m_rows;
    unsigned int m_columnCount{0};
};

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& initialFile)
        : wxFrame(nullptr, wxID_ANY, WXCsv_NAME, wxDefaultPosition, wxSize(900, 600)) {
        BuildMenuBar();
        BuildAccelerators();
        BuildDataView();
        ApplyWindowIcon();
        UpdateTitle();
        if (!initialFile.IsEmpty()) {
            OpenFile(initialFile);
        }
    }

private:
    void BuildMenuBar() {
        auto* fileMenu = new wxMenu();
        fileMenu->Append(wxID_OPEN, "&Open...\tCtrl+O");
        fileMenu->Append(wxID_EXIT, "E&xit\tCtrl+Q");

        auto* editMenu = new wxMenu();
        editMenu->Append(wxID_COPY, "&Copy\tCtrl+C");
        editMenu->Append(wxID_FIND, "&Find...\tCtrl+F");
        editMenu->Append(ID_FIND_NEXT, "Find &Next\tCtrl+G");
        editMenu->Append(ID_FIND_PREVIOUS, "Find &Previous\tShift+Ctrl+G");

        auto* helpMenu = new wxMenu();
        helpMenu->Append(wxID_ABOUT, "&About");

        auto* bar = new wxMenuBar();
        bar->Append(fileMenu, "&File");
        bar->Append(editMenu, "&Edit");
        bar->Append(helpMenu, "&Help");
        SetMenuBar(bar);
    }

    void BuildDataView() {
        m_dataView = new wxDataViewCtrl(
            this,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxDV_MULTIPLE);

        m_model = std::make_unique<CsvDataModel>();
        m_dataView->AssociateModel(m_model.get());

        BuildColumns({});
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_dataView, 1, wxEXPAND | wxALL, 0);
        SetSizer(sizer);
    }

    void BuildAccelerators() {
        wxAcceleratorEntry entries[] = {
            { wxACCEL_CTRL, 'O', wxID_OPEN },
            { wxACCEL_CTRL, 'Q', wxID_EXIT },
            { wxACCEL_CTRL, 'C', wxID_COPY },
            { wxACCEL_CTRL, 'F', wxID_FIND },
            { wxACCEL_CTRL, 'G', ID_FIND_NEXT },
            { wxACCEL_CTRL | wxACCEL_SHIFT, 'G', ID_FIND_PREVIOUS }
        };
        const int entryCount = static_cast<int>(sizeof(entries) / sizeof(entries[0]));
        wxAcceleratorTable accelerators(entryCount, entries);
        SetAcceleratorTable(accelerators);
    }

    void ApplyWindowIcon() {
        wxIcon icon;
#ifdef __WXMSW__
        SetIcon(wxICON(IDI_APP_ICON));
        return;
#endif

        wxMemoryInputStream stream(assets_wxcsv_png, assets_wxcsv_png_len);
        wxImage image;
        if (image.LoadFile(stream, wxBITMAP_TYPE_PNG)) {
            wxBitmap bitmap(image);
            icon.CopyFromBitmap(bitmap);
            SetIcon(icon);
        }
    }

    void BuildColumns(const std::vector<wxString>& headers) {
        while (m_dataView->GetColumnCount() > 0) {
            m_dataView->DeleteColumn(m_dataView->GetColumn(0));
        }

        const unsigned int columns = static_cast<unsigned int>(headers.size());
        if (columns == 0) {
            return;
        }
        for (unsigned int i = 0; i < columns; ++i) {
            auto* renderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
            const wxString title = (!headers.empty() && i < headers.size() && !headers[i].IsEmpty())
                ? headers[i]
                : wxString::Format("Column %u", i + 1);
            auto* col = new wxDataViewColumn(
                title,
                renderer,
                i,
                160,
                wxALIGN_LEFT,
                wxDATAVIEW_COL_RESIZABLE);
            m_dataView->AppendColumn(col);
        }
    }

    void UpdateTitle() {
        if (m_currentFile.IsEmpty()) {
            SetTitle(WXCsv_NAME);
        } else {
            SetTitle(wxString::Format("%s - %s", WXCsv_NAME, wxFileName(m_currentFile).GetFullName()));
        }
    }

    void OpenFile(const wxString& path) {
        std::vector<std::vector<wxString>> rows;
        std::vector<wxString> headers;
        if (!wxFileExists(path)) {
            wxMessageBox(wxString::Format("File not found:\n%s", path), "Open CSV");
            return;
        }
        if (!ParseCsvFile(path, headers, rows)) {
            wxMessageBox(wxString::Format("Unable to read file:\n%s", path), "Open CSV");
            return;
        }

        m_model->SetRows(std::move(rows), static_cast<unsigned int>(headers.size()));
        m_headers = std::move(headers);
        BuildColumns(m_headers);
        m_dataView->UnselectAll();

        m_currentFile = path;
        m_lastFindValid = false;
        m_lastFindIndex = 0;
        UpdateTitle();
        ResizeToCsvContent();
    }

    void ResizeToCsvContent() {
        if (!m_dataView || !m_model || m_model->GetCount() == 0 || m_model->GetColumnCount() == 0) {
            return;
        }

        wxSize displaySize = wxGetDisplaySize();
        const int maxWidth = displaySize.GetWidth() * 66 / 100;
        const int maxHeight = displaySize.GetHeight() * 66 / 100;

        wxClientDC dc(this);
        dc.SetFont(m_dataView->GetFont());
        const int charHeight = dc.GetTextExtent("M").GetHeight();
        const int rowHeight = std::max(22, charHeight + 8);
        const unsigned int columns = m_model->GetColumnCount();
        const unsigned int rows = m_model->GetCount();
        const unsigned int sampleRows = std::min<unsigned int>(rows, 120u);

        int totalWidth = 0;
        for (unsigned int col = 0; col < columns; ++col) {
            wxString header = wxString::Format("Column %u", col + 1);
            if (col < m_headers.size() && !m_headers[col].IsEmpty()) {
                header = m_headers[col];
            }

            int colWidth = dc.GetTextExtent(header).GetWidth() + 32;
            for (unsigned int row = 0; row < sampleRows; ++row) {
                colWidth = std::max(colWidth, dc.GetTextExtent(m_model->GetCellText(row, col)).GetWidth() + 32);
            }
            totalWidth += std::clamp(colWidth, 100, 400);
        }

        const int frameExtraX = GetSize().GetWidth() - GetClientSize().GetWidth();
        const int frameExtraY = GetSize().GetHeight() - GetClientSize().GetHeight();
        const int verticalScrollbar = 18;
        const int horizontalScrollbar = 18;
        const int headerHeight = rowHeight + 12;
        const unsigned int visibleRows = std::min<unsigned int>(rows, 24u);

        int desiredWidth = totalWidth + frameExtraX + verticalScrollbar + 4;
        int desiredHeight = static_cast<int>(visibleRows * rowHeight + headerHeight + frameExtraY + horizontalScrollbar + 24);

        int newWidth = std::max(640, std::min(desiredWidth, maxWidth));
        int newHeight = std::max(360, std::min(desiredHeight, maxHeight));
        SetSize(newWidth, newHeight);
        Centre();
    }

    void CopySelection() {
        wxDataViewItemArray selected;
        m_dataView->GetSelections(selected);
        if (selected.empty()) {
            wxDataViewItem current = m_dataView->GetCurrentItem();
            if (current.IsOk()) {
                selected.push_back(current);
            }
        }
        if (selected.empty()) {
            return;
        }

        const unsigned int cols = m_model->GetColumnCount();
        wxString output;
        for (size_t i = 0; i < selected.size(); ++i) {
            const unsigned int row = m_model->GetRow(selected[i]);
            if (!output.empty()) {
                output << '\n';
            }
            for (unsigned int col = 0; col < cols; ++col) {
                if (col > 0) {
                    output << '\t';
                }
                output << m_model->GetCellText(row, col);
            }
        }
        if (output.IsEmpty()) {
            return;
        }

        CopyToClipboard(output);
    }

    void CopyRow(const wxDataViewItem& item) {
        if (!item.IsOk()) {
            return;
        }
        const unsigned int row = m_model->GetRow(item);
        const unsigned int cols = m_model->GetColumnCount();
        wxString output;
        for (unsigned int col = 0; col < cols; ++col) {
            if (col > 0) {
                output << '\t';
            }
            output << m_model->GetCellText(row, col);
        }
        CopyToClipboard(output);
    }

    void CopyCell(const wxDataViewItem& item, int column) {
        if (!item.IsOk()) {
            return;
        }
        if (column < 0 || column >= static_cast<int>(m_model->GetColumnCount())) {
            return;
        }
        const unsigned int row = m_model->GetRow(item);
        const wxString output = m_model->GetCellText(row, static_cast<unsigned int>(column));
        CopyToClipboard(output);
    }

    void CopyToClipboard(const wxString& output) {
        if (output.IsEmpty()) {
            return;
        }
        auto* text = new wxTextDataObject(output);
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(text);
            wxTheClipboard->Close();
        }
    }

    void OnContextCopyRow(wxCommandEvent&) {
        wxDataViewItem item = m_dataView->GetCurrentItem();
        if (m_contextItem.IsOk()) {
            item = m_contextItem;
        }
        CopyRow(item);
    }

    void OnContextCopyCell(wxCommandEvent&) {
        int col = m_contextColumn;
        wxDataViewItem item = m_dataView->GetCurrentItem();
        if (m_contextItem.IsOk()) {
            item = m_contextItem;
        }
        if (col < 0 && item.IsOk()) {
            col = m_dataView->GetColumnPosition(m_dataView->GetCurrentColumn());
        }
        CopyCell(item, col);
    }

    void OnContextMenu(wxDataViewEvent& event) {
        m_contextItem = event.GetItem();
        m_contextColumn = event.GetColumn();

        if (!m_contextItem.IsOk()) {
            m_contextItem = m_dataView->GetCurrentItem();
        }

        wxMenu menu;
        menu.Append(ID_CONTEXT_COPY_ROW, "&Copy row");
        menu.Append(ID_CONTEXT_COPY_CELL, "Copy &cell");
        const bool hasTarget = m_contextItem.IsOk();
        menu.Enable(ID_CONTEXT_COPY_ROW, hasTarget);
        menu.Enable(ID_CONTEXT_COPY_CELL, hasTarget);

        const int x = wxGetMousePosition().x;
        const int y = wxGetMousePosition().y;
        m_dataView->PopupMenu(&menu, m_dataView->ScreenToClient(wxPoint(x, y)));
    }

    bool FindInData(bool forward) {
        const wxString query = m_findData.GetFindString();
        const bool matchCase = (m_findData.GetFlags() & wxFR_MATCHCASE) != 0;
        const unsigned int rows = m_model->GetCount();
        const unsigned int cols = m_model->GetColumnCount();
        const size_t total = static_cast<size_t>(rows) * cols;
        if (query.IsEmpty() || total == 0) {
            return false;
        }

        for (size_t step = 0; step < total; ++step) {
            size_t linearIndex = 0;
            if (m_lastFindValid) {
                const size_t lastIndex = static_cast<size_t>(m_lastFindIndex);
                if (forward) {
                    linearIndex = (lastIndex + 1 + step) % total;
                } else {
                    linearIndex = (lastIndex + total - 1 - step) % total;
                }
            } else if (forward) {
                linearIndex = step;
            } else {
                linearIndex = total - 1 - step;
            }

            const unsigned int row = static_cast<unsigned int>(linearIndex / cols);
            const unsigned int col = static_cast<unsigned int>(linearIndex % cols);
            const wxString value = m_model->GetCellText(row, col);
            if (ContainsText(value, query, matchCase)) {
                SelectCell(row, col);
                m_lastFindValid = true;
                m_lastFindIndex = linearIndex;
                return true;
            }
        }
        return false;
    }

    void SelectCell(unsigned int row, unsigned int col) {
        wxDataViewItem item = m_model->GetItem(row);
        m_dataView->UnselectAll();
        m_dataView->Select(item);
        m_dataView->SetCurrentItem(item);
        m_dataView->EnsureVisible(item);
        m_dataView->SetFocus();
    }

    void OnOpen(wxCommandEvent&) {
        wxFileDialog dialog(
            this,
            "Open CSV file",
            {},
            {},
            "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() == wxID_OK) {
            OpenFile(dialog.GetPath());
        }
    }

    void OnExit(wxCommandEvent&) {
        Close();
    }

    void OnCopy(wxCommandEvent&) {
        CopySelection();
    }

    void OnFind(wxCommandEvent&) {
        if (!m_findDialog) {
            m_findDialog = new wxFindReplaceDialog(this, &m_findData, "Find");
        }
        m_findDialog->Show();
        m_findDialog->Raise();
    }

    void OnFindNext(wxCommandEvent&) {
        if (m_findData.GetFindString().IsEmpty()) {
            wxCommandEvent evt;
            OnFind(evt);
            return;
        }
        if (!FindInData(true)) {
            wxMessageBox("No matching data found.", "Find");
        }
    }

    void OnFindPrevious(wxCommandEvent&) {
        if (m_findData.GetFindString().IsEmpty()) {
            wxCommandEvent evt;
            OnFind(evt);
            return;
        }
        if (!FindInData(false)) {
            wxMessageBox("No matching data found.", "Find");
        }
    }

    void OnFindDialog(wxFindDialogEvent& event) {
        m_findData.SetFindString(event.GetFindString());
        const bool forward = (event.GetFlags() & wxFR_DOWN) != 0;
        if (!FindInData(forward)) {
            wxMessageBox("No matching data found.", "Find");
        }
    }

    void OnFindDialogClose(wxFindDialogEvent&) {
        if (m_findDialog) {
            m_findDialog->Destroy();
            m_findDialog = nullptr;
        }
    }

    void OnAbout(wxCommandEvent&) {
        wxMessageBox(
            wxString::Format("%s %s", WXCsv_NAME, WXCsv_VERSION),
            "About wxCsv",
            wxOK | wxICON_INFORMATION,
            this);
    }

    void OnClose(wxCloseEvent& event) {
        if (m_findDialog) {
            m_findDialog->Destroy();
            m_findDialog = nullptr;
        }
        if (m_dataView) {
            m_dataView->AssociateModel(nullptr);
        }
        event.Skip();
    }

private:
    std::unique_ptr<CsvDataModel> m_model;
    wxDataViewCtrl* m_dataView{nullptr};
    wxString m_currentFile;
    wxFindReplaceData m_findData{wxFR_DOWN};
    wxFindReplaceDialog* m_findDialog{nullptr};
    size_t m_lastFindIndex{0};
    bool m_lastFindValid{false};
    std::vector<wxString> m_headers;
    wxDataViewItem m_contextItem;
    int m_contextColumn{-1};

    wxDECLARE_EVENT_TABLE();

};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(wxID_OPEN, MainFrame::OnOpen)
    EVT_MENU(wxID_EXIT, MainFrame::OnExit)
    EVT_MENU(wxID_COPY, MainFrame::OnCopy)
    EVT_MENU(wxID_FIND, MainFrame::OnFind)
    EVT_MENU(ID_FIND_NEXT, MainFrame::OnFindNext)
    EVT_MENU(ID_FIND_PREVIOUS, MainFrame::OnFindPrevious)
    EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)
    EVT_MENU(ID_CONTEXT_COPY_ROW, MainFrame::OnContextCopyRow)
    EVT_MENU(ID_CONTEXT_COPY_CELL, MainFrame::OnContextCopyCell)
    EVT_DATAVIEW_ITEM_CONTEXT_MENU(wxID_ANY, MainFrame::OnContextMenu)
    EVT_CLOSE(MainFrame::OnClose)
    EVT_FIND(wxID_ANY, MainFrame::OnFindDialog)
    EVT_FIND_NEXT(wxID_ANY, MainFrame::OnFindDialog)
    EVT_FIND_CLOSE(wxID_ANY, MainFrame::OnFindDialogClose)
wxEND_EVENT_TABLE()

class wxCsvApp : public wxApp {
public:
    bool OnInit() override {
        wxString initialFile;
        if (argc > 1) {
            initialFile = wxString(argv[1]);
        }
        auto* frame = new MainFrame(initialFile);
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(wxCsvApp);
