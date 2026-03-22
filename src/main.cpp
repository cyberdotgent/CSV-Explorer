#include <wx/wx.h>
#include <wx/fdrepdlg.h>
#include <wx/textfile.h>
#include <wx/filename.h>
#include <wx/clipbrd.h>
#include <wx/ffile.h>
#include <wx/grid.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "about_dialog.h"
#include "config.h"
#include "go_to_row_dialog.h"

namespace {

enum {
    ID_FIND_NEXT = wxID_HIGHEST + 1,
    ID_FIND_PREVIOUS,
    ID_GO_TO_FIRST,
    ID_GO_TO_LAST,
    ID_GO_TO_ROW,
    ID_CONTEXT_COPY_ROW,
    ID_CONTEXT_COPY_CELL,
    ID_INSERT_ROW_BEFORE,
    ID_INSERT_ROW_AFTER,
    ID_INSERT_COLUMN_BEFORE,
    ID_INSERT_COLUMN_AFTER
};

enum class DirtyFileAction {
    Save,
    SaveAs,
    Discard,
    Cancel
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

wxString EscapeCsvField(const wxString& value) {
    bool needsQuotes = false;
    wxString escaped;

    for (const wxUniChar ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
            needsQuotes = true;
        } else {
            if (ch == ',' || ch == '\n' || ch == '\r') {
                needsQuotes = true;
            }
            escaped += ch;
        }
    }

    if (needsQuotes) {
        return "\"" + escaped + "\"";
    }
    return escaped;
}

wxString BuildCsvLine(const std::vector<wxString>& fields) {
    wxString line;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            line += ',';
        }
        line += EscapeCsvField(fields[i]);
    }
    return line;
}

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

} // namespace

class MainFrame : public wxFrame {
public:
    explicit MainFrame(const wxString& initialFile)
        : wxFrame(nullptr, wxID_ANY, CSV_EXPLORER_NAME, wxDefaultPosition, wxSize(900, 600)) {
        BuildMenuBar();
        BuildAccelerators();
        BuildGrid();
        BuildStatusBar();
        ApplyWindowIcon();
        if (!initialFile.IsEmpty()) {
            OpenFile(initialFile);
        } else {
            CreateNewFile();
        }
    }

private:
    void BuildMenuBar() {
        auto* fileMenu = new wxMenu();
        fileMenu->Append(wxID_NEW, "&New\tCtrl+N");
        fileMenu->Append(wxID_OPEN, "&Open...\tCtrl+O");
        fileMenu->Append(wxID_SAVE, "&Save\tCtrl+S");
        fileMenu->Append(wxID_SAVEAS, "Save &As...\tCtrl+Shift+S");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_EXIT, "E&xit\tCtrl+Q");

        auto* editMenu = new wxMenu();
        editMenu->Append(wxID_COPY, "&Copy\tCtrl+C");
        auto* insertMenu = new wxMenu();
        insertMenu->Append(ID_INSERT_ROW_BEFORE, "Insert row &before");
        insertMenu->Append(ID_INSERT_ROW_AFTER, "Insert row &after");
        insertMenu->AppendSeparator();
        insertMenu->Append(ID_INSERT_COLUMN_BEFORE, "Insert column &before");
        insertMenu->Append(ID_INSERT_COLUMN_AFTER, "Insert column &after");
        editMenu->AppendSubMenu(insertMenu, "&Insert");
        editMenu->AppendSeparator();
        auto* goToMenu = new wxMenu();
#ifdef __WXOSX__
        goToMenu->Append(ID_GO_TO_FIRST, "Go to &First\tCmd+Up");
        goToMenu->Append(ID_GO_TO_LAST, "Go to &Last\tCmd+Down");
        goToMenu->Append(ID_GO_TO_ROW, "Go to &Row...\tCmd+G");
#else
        goToMenu->Append(ID_GO_TO_FIRST, "Go to &First\tCtrl+Home");
        goToMenu->Append(ID_GO_TO_LAST, "Go to &Last\tCtrl+End");
        goToMenu->Append(ID_GO_TO_ROW, "Go to &Row...\tCtrl+G");
#endif
        editMenu->AppendSubMenu(goToMenu, "&Go To");
        editMenu->AppendSeparator();
        editMenu->Append(wxID_FIND, "&Find...\tCtrl+F");
        editMenu->Append(ID_FIND_NEXT, "Find &Next\tF3");
        editMenu->Append(ID_FIND_PREVIOUS, "Find &Previous\tShift+F3");

        auto* helpMenu = new wxMenu();
        helpMenu->Append(wxID_ABOUT, "&About");

        auto* bar = new wxMenuBar();
        bar->Append(fileMenu, "&File");
        bar->Append(editMenu, "&Edit");
        bar->Append(helpMenu, "&Help");
        SetMenuBar(bar);
    }

    void BuildGrid() {
        m_grid = new wxGrid(this, wxID_ANY);
        m_grid->CreateGrid(0, 0);
        m_grid->EnableEditing(true);
        m_grid->EnableDragRowSize(false);
        m_grid->EnableDragColSize(true);
        m_grid->SetDefaultCellOverflow(false);
        m_grid->SetRowLabelSize(0);
        m_grid->SetColLabelAlignment(wxALIGN_LEFT, wxALIGN_CENTER);

        m_grid->Bind(wxEVT_GRID_CELL_LEFT_DCLICK, &MainFrame::OnCellLeftDClick, this);
        m_grid->Bind(wxEVT_GRID_CELL_RIGHT_CLICK, &MainFrame::OnCellRightClick, this);
        m_grid->Bind(wxEVT_GRID_LABEL_LEFT_DCLICK, &MainFrame::OnLabelLeftDClick, this);
        m_grid->Bind(wxEVT_GRID_LABEL_RIGHT_CLICK, &MainFrame::OnLabelRightClick, this);
        m_grid->Bind(wxEVT_GRID_SELECT_CELL, &MainFrame::OnSelectCell, this);
        m_grid->Bind(wxEVT_GRID_CELL_CHANGED, &MainFrame::OnCellChanged, this);
        m_grid->Bind(wxEVT_GRID_EDITOR_SHOWN, &MainFrame::OnEditorShown, this);
        m_grid->Bind(wxEVT_CHAR_HOOK, &MainFrame::OnGridCharHook, this);
        m_grid->Bind(wxEVT_SIZE, &MainFrame::OnGridResized, this);
        m_grid->GetGridWindow()->Bind(wxEVT_LEFT_DCLICK, &MainFrame::OnGridWindowLeftDClick, this);

        m_headerEditor = new wxTextCtrl(
            m_grid->GetGridColLabelWindow(),
            wxID_ANY,
            {},
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_PROCESS_ENTER);
        m_headerEditor->Hide();
        m_headerEditor->Bind(wxEVT_TEXT_ENTER, &MainFrame::OnHeaderEditorEnter, this);
        m_headerEditor->Bind(wxEVT_KILL_FOCUS, &MainFrame::OnHeaderEditorKillFocus, this);
        m_headerEditor->Bind(wxEVT_CHAR_HOOK, &MainFrame::OnHeaderEditorCharHook, this);

        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_grid, 1, wxEXPAND | wxALL, 0);
        SetSizer(sizer);
    }

    void BuildStatusBar() {
        wxFrame::CreateStatusBar(2);
        const int widths[] = { -1, FromDIP(180) };
        GetStatusBar()->SetStatusWidths(2, widths);
        UpdateStatusBar();
    }

    void BuildAccelerators() {
        wxAcceleratorEntry entries[] = {
            { wxACCEL_CTRL, 'N', wxID_NEW },
            { wxACCEL_CTRL, 'O', wxID_OPEN },
            { wxACCEL_CTRL, 'S', wxID_SAVE },
            { wxACCEL_CTRL | wxACCEL_SHIFT, 'S', wxID_SAVEAS },
            { wxACCEL_CTRL, 'Q', wxID_EXIT },
            { wxACCEL_CTRL, 'C', wxID_COPY },
            { wxACCEL_CTRL, 'F', wxID_FIND },
            { wxACCEL_NORMAL, WXK_F3, ID_FIND_NEXT },
            { wxACCEL_SHIFT, WXK_F3, ID_FIND_PREVIOUS },
            { wxACCEL_CTRL, 'G', ID_GO_TO_ROW },
#ifdef __WXOSX__
            { wxACCEL_CTRL, WXK_UP, ID_GO_TO_FIRST },
            { wxACCEL_CTRL, WXK_DOWN, ID_GO_TO_LAST }
#else
            { wxACCEL_CTRL, WXK_HOME, ID_GO_TO_FIRST },
            { wxACCEL_CTRL, WXK_END, ID_GO_TO_LAST }
#endif
        };
        const int entryCount = static_cast<int>(sizeof(entries) / sizeof(entries[0]));
        wxAcceleratorTable accelerators(entryCount, entries);
        SetAcceleratorTable(accelerators);
    }

    void ApplyWindowIcon() {
#ifdef __WXMSW__
        SetIcon(wxICON(IDI_APP_ICON));
        return;
#endif

        wxIcon icon = LoadAppIcon();
        if (icon.IsOk()) {
            SetIcon(icon);
        }
    }

    unsigned int GetColumnCount() const {
        unsigned int columnCount = static_cast<unsigned int>(m_headers.size());
        for (const auto& row : m_rows) {
            columnCount = std::max(columnCount, static_cast<unsigned int>(row.size()));
        }
        return columnCount;
    }

    void NormalizeRows(unsigned int minimumColumnCount) {
        unsigned int columnCount = std::max(GetColumnCount(), minimumColumnCount);
        m_headers.resize(columnCount);
        for (auto& row : m_rows) {
            row.resize(columnCount);
        }
    }

    void EnsureRowCount(int desiredRows) {
        const int currentRows = m_grid->GetNumberRows();
        if (currentRows < desiredRows) {
            m_grid->AppendRows(desiredRows - currentRows);
        } else if (currentRows > desiredRows) {
            m_grid->DeleteRows(0, currentRows - desiredRows);
        }
    }

    void EnsureColumnCount(int desiredColumns) {
        const int currentColumns = m_grid->GetNumberCols();
        if (currentColumns < desiredColumns) {
            m_grid->AppendCols(desiredColumns - currentColumns);
        } else if (currentColumns > desiredColumns) {
            m_grid->DeleteCols(desiredColumns, currentColumns - desiredColumns);
        }
    }

    wxString GetCellText(unsigned int row, unsigned int col) const {
        if (row < m_rows.size() && col < m_rows[row].size()) {
            return m_rows[row][col];
        }
        return {};
    }

    void SetCellText(unsigned int row, unsigned int col, const wxString& value) {
        if (row >= m_rows.size()) {
            return;
        }
        if (col >= m_rows[row].size()) {
            m_rows[row].resize(col + 1);
        }
        m_rows[row][col] = value;
        if (row < static_cast<unsigned int>(m_grid->GetNumberRows()) && col < static_cast<unsigned int>(m_grid->GetNumberCols())) {
            m_grid->SetCellValue(static_cast<int>(row), static_cast<int>(col), value);
        }
    }

    void RefreshGridFromData() {
        const int rows = static_cast<int>(m_rows.size());
        const int columns = static_cast<int>(GetColumnCount());
        m_isRefreshingGrid = true;

        EnsureColumnCount(columns);
        EnsureRowCount(rows);

        for (int col = 0; col < columns; ++col) {
            wxString title = wxString::Format("Column %d", col + 1);
            if (col < static_cast<int>(m_headers.size()) && !m_headers[col].IsEmpty()) {
                title = m_headers[col];
            }
            m_grid->SetColLabelValue(col, title);
        }

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < columns; ++col) {
                m_grid->SetCellValue(row, col, GetCellText(static_cast<unsigned int>(row), static_cast<unsigned int>(col)));
            }
        }

        m_isRefreshingGrid = false;
        RepositionHeaderEditor();
    }

    void UpdateTitle() {
        wxString title = wxString::Format("%s - %s", CSV_EXPLORER_NAME, GetDisplayFileName());
        if (m_isDirty) {
            title += " *";
        }
        SetTitle(title);
    }

    void UpdateStatusBar() {
        if (!GetStatusBar()) {
            return;
        }

        const int totalRows = static_cast<int>(m_rows.size());
        const int selectedRow = m_grid ? m_grid->GetGridCursorRow() : -1;
        wxString status = wxString::Format(
            "Row %d of %d",
            totalRows > 0 && selectedRow >= 0 ? selectedRow + 1 : 0,
            totalRows);
        SetStatusText(m_isDirty ? "Modified" : wxString(), 0);
        SetStatusText(status, 1);
    }

    void SetDirty(bool dirty) {
        if (m_isDirty == dirty) {
            return;
        }

        m_isDirty = dirty;
        UpdateTitle();
        UpdateStatusBar();
    }

    wxString GetDisplayFileName() const {
        if (!m_currentFile.IsEmpty()) {
            return wxFileName(m_currentFile).GetFullName();
        }
        return m_documentName;
    }

    int GetActiveRowIndex() const {
        if (m_contextRow >= 0) {
            return m_contextRow;
        }
        return m_grid ? m_grid->GetGridCursorRow() : -1;
    }

    int GetActiveColumnIndex() const {
        if (m_contextColumn >= 0) {
            return m_contextColumn;
        }
        return m_grid ? m_grid->GetGridCursorCol() : -1;
    }

    void GoToRow(int row) {
        if (!m_grid || m_rows.empty()) {
            return;
        }

        row = std::clamp(row, 0, static_cast<int>(m_rows.size()) - 1);
        int col = m_grid->GetGridCursorCol();
        if (col < 0) {
            col = 0;
        }
        if (m_grid->GetNumberCols() <= 0) {
            return;
        }
        col = std::clamp(col, 0, m_grid->GetNumberCols() - 1);
        SelectCell(static_cast<unsigned int>(row), static_cast<unsigned int>(col));
    }

    wxRect GetHeaderRect(int col) const {
        if (!m_grid || col < 0 || col >= m_grid->GetNumberCols()) {
            return {};
        }

        int x = 0;
        for (int i = 0; i < col; ++i) {
            x += m_grid->GetColSize(i);
        }

        const int scrollOffset = m_grid->GetScrollPos(wxHORIZONTAL) * m_grid->GetScrollLineX();
        x -= scrollOffset;

        return wxRect(x, 0, m_grid->GetColSize(col), m_grid->GetColLabelSize());
    }

    void RepositionHeaderEditor() {
        if (!m_headerEditor || m_activeHeaderColumn < 0) {
            return;
        }

        const wxRect rect = GetHeaderRect(m_activeHeaderColumn);
        if (rect.width <= 0 || rect.height <= 0) {
            m_headerEditor->Hide();
            return;
        }

        const int inset = FromDIP(1);
        m_headerEditor->SetSize(
            rect.x + inset,
            rect.y + inset,
            std::max(10, rect.width - inset * 2),
            std::max(10, rect.height - inset * 2));
        m_headerEditor->Show();
        m_headerEditor->Raise();
    }

    void BeginHeaderEdit(int col) {
        if (!m_headerEditor || col < 0 || col >= static_cast<int>(GetColumnCount())) {
            return;
        }

        CommitActiveEdit();
        m_activeHeaderColumn = col;
        m_headerEditor->ChangeValue(m_headers[col]);
        RepositionHeaderEditor();
        m_headerEditor->SetFocus();
        m_headerEditor->SelectAll();
    }

    void FocusFirstDataCellForEntry() {
        if (!m_grid || m_grid->GetNumberCols() <= 0 || m_grid->GetNumberRows() <= 0) {
            return;
        }

        SelectCell(0, 0);
        m_grid->EnableCellEditControl();
    }

    void CommitHeaderEdit() {
        if (!m_headerEditor || m_activeHeaderColumn < 0) {
            return;
        }

        const int col = m_activeHeaderColumn;
        const wxString updatedValue = m_headerEditor->GetValue();
        m_headerEditor->Hide();
        m_activeHeaderColumn = -1;

        if (col >= static_cast<int>(m_headers.size()) || m_headers[col] == updatedValue) {
            return;
        }

        m_headers[col] = updatedValue;
        m_grid->SetColLabelValue(col, updatedValue.IsEmpty() ? wxString::Format("Column %d", col + 1) : updatedValue);
        SetDirty(true);
    }

    void CancelHeaderEdit() {
        if (!m_headerEditor) {
            return;
        }

        m_headerEditor->Hide();
        m_activeHeaderColumn = -1;
    }

    void SyncCellFromGrid(int row, int col) {
        if (row < 0 || col < 0 || row >= static_cast<int>(m_rows.size())) {
            return;
        }

        const wxString updatedValue = m_grid->GetCellValue(row, col);
        if (GetCellText(static_cast<unsigned int>(row), static_cast<unsigned int>(col)) == updatedValue) {
            return;
        }

        SetCellText(static_cast<unsigned int>(row), static_cast<unsigned int>(col), updatedValue);
        SetDirty(true);
    }

    bool SaveToPath(const wxString& path) {
        CommitHeaderEdit();
        CommitActiveEdit();

        wxFFile file(path, "w");
        if (!file.IsOpened()) {
            wxMessageBox(wxString::Format("Unable to write file:\n%s", path), "Save CSV");
            return false;
        }

        std::vector<wxString> headers = m_headers;
        headers.resize(GetColumnCount());
        if (!file.Write(BuildCsvLine(headers) + "\n")) {
            wxMessageBox(wxString::Format("Unable to write file:\n%s", path), "Save CSV");
            return false;
        }

        for (const auto& row : m_rows) {
            std::vector<wxString> normalizedRow = row;
            normalizedRow.resize(GetColumnCount());
            if (!file.Write(BuildCsvLine(normalizedRow) + "\n")) {
                wxMessageBox(wxString::Format("Unable to write file:\n%s", path), "Save CSV");
                return false;
            }
        }

        m_currentFile = path;
        m_documentName = wxFileName(path).GetFullName();
        SetDirty(false);
        UpdateTitle();
        return true;
    }

    bool SaveCurrentFile() {
        if (m_currentFile.IsEmpty()) {
            return SaveCurrentFileAs();
        }
        return SaveToPath(m_currentFile);
    }

    bool SaveCurrentFileAs() {
        wxFileDialog dialog(
            this,
            "Save CSV file",
            {},
            m_currentFile.IsEmpty() ? m_documentName : wxFileName(m_currentFile).GetFullName(),
            "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() != wxID_OK) {
            return false;
        }
        return SaveToPath(dialog.GetPath());
    }

    bool ConfirmDirtyFileAction() {
        if (!m_isDirty) {
            return true;
        }

        switch (ShowDirtyFileDialog(this, GetDisplayFileName())) {
        case DirtyFileAction::Save:
            return SaveCurrentFile();
        case DirtyFileAction::SaveAs:
            return SaveCurrentFileAs();
        case DirtyFileAction::Discard:
            return true;
        case DirtyFileAction::Cancel:
            return false;
        }

        return false;
    }

    void CreateNewFile() {
        CancelHeaderEdit();
        CommitActiveEdit();

        m_rows.assign(1, std::vector<wxString>(1, wxString()));
        m_headers.assign(1, wxString());
        m_currentFile.clear();
        m_documentName = "untitled.csv";
        m_lastFindValid = false;
        m_lastFindIndex = 0;

        RefreshGridFromData();
        ResizeToCsvContent();
        SetDirty(true);
        UpdateTitle();
        UpdateStatusBar();

        if (m_grid->GetNumberCols() > 0) {
            BeginHeaderEdit(0);
        }
    }

    void OpenFile(const wxString& path) {
        CancelHeaderEdit();
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

        m_rows = std::move(rows);
        m_headers = std::move(headers);
        NormalizeRows(static_cast<unsigned int>(m_headers.size()));
        RefreshGridFromData();
        m_grid->ClearSelection();

        m_currentFile = path;
        m_documentName = wxFileName(path).GetFullName();
        m_lastFindValid = false;
        m_lastFindIndex = 0;
        SetDirty(false);
        UpdateTitle();
        UpdateStatusBar();
        ResizeToCsvContent();
    }

    void ResizeToCsvContent() {
        if (!m_grid || m_rows.empty() || GetColumnCount() == 0) {
            return;
        }

        wxSize displaySize = wxGetDisplaySize();
        const int maxWidth = displaySize.GetWidth() * 66 / 100;
        const int maxHeight = displaySize.GetHeight() * 66 / 100;

        wxClientDC dc(this);
        dc.SetFont(m_grid->GetFont());
        const int charHeight = dc.GetTextExtent("M").GetHeight();
        const int rowHeight = std::max(22, charHeight + 8);
        const unsigned int columns = GetColumnCount();
        const unsigned int rows = static_cast<unsigned int>(m_rows.size());
        const unsigned int sampleRows = std::min<unsigned int>(rows, 120u);

        int totalWidth = 0;
        for (unsigned int col = 0; col < columns; ++col) {
            wxString header = wxString::Format("Column %u", col + 1);
            if (col < m_headers.size() && !m_headers[col].IsEmpty()) {
                header = m_headers[col];
            }

            int colWidth = dc.GetTextExtent(header).GetWidth() + 32;
            for (unsigned int row = 0; row < sampleRows; ++row) {
                colWidth = std::max(colWidth, dc.GetTextExtent(GetCellText(row, col)).GetWidth() + 32);
            }
            colWidth = std::clamp(colWidth, 100, 400);
            totalWidth += colWidth;
            if (col < static_cast<unsigned int>(m_grid->GetNumberCols())) {
                m_grid->SetColSize(static_cast<int>(col), colWidth);
            }
        }

        const int frameExtraX = GetSize().GetWidth() - GetClientSize().GetWidth();
        const int frameExtraY = GetSize().GetHeight() - GetClientSize().GetHeight();
        const int verticalScrollbar = 18;
        const int horizontalScrollbar = 18;
        const int headerHeight = rowHeight + 12;
        const unsigned int visibleRows = std::min<unsigned int>(rows, 24u);

        int desiredWidth = totalWidth + frameExtraX + verticalScrollbar + m_grid->GetRowLabelSize() + 4;
        int desiredHeight = static_cast<int>(visibleRows * rowHeight + headerHeight + frameExtraY + horizontalScrollbar + 24);

        int newWidth = std::max(640, std::min(desiredWidth, maxWidth));
        int newHeight = std::max(360, std::min(desiredHeight, maxHeight));
        SetSize(newWidth, newHeight);
        Centre();
    }

    wxString GetBlockText(int topRow, int leftCol, int bottomRow, int rightCol) const {
        wxString output;
        for (int row = topRow; row <= bottomRow; ++row) {
            if (!output.IsEmpty()) {
                output << '\n';
            }
            for (int col = leftCol; col <= rightCol; ++col) {
                if (col > leftCol) {
                    output << '\t';
                }
                output << GetCellText(static_cast<unsigned int>(row), static_cast<unsigned int>(col));
            }
        }
        return output;
    }

    void CopySelection() {
        const wxGridCellCoordsArray topLeft = m_grid->GetSelectionBlockTopLeft();
        const wxGridCellCoordsArray bottomRight = m_grid->GetSelectionBlockBottomRight();
        if (!topLeft.empty() && topLeft.size() == bottomRight.size()) {
            CopyToClipboard(GetBlockText(
                topLeft[0].GetRow(),
                topLeft[0].GetCol(),
                bottomRight[0].GetRow(),
                bottomRight[0].GetCol()));
            return;
        }

        wxArrayInt selectedRows = m_grid->GetSelectedRows();
        if (!selectedRows.empty()) {
            wxString output;
            for (size_t i = 0; i < selectedRows.size(); ++i) {
                if (!output.IsEmpty()) {
                    output << '\n';
                }
                output << GetBlockText(selectedRows[i], 0, selectedRows[i], m_grid->GetNumberCols() - 1);
            }
            CopyToClipboard(output);
            return;
        }

        wxGridCellCoordsArray selectedCells = m_grid->GetSelectedCells();
        if (!selectedCells.empty()) {
            const wxGridCellCoords& cell = selectedCells[0];
            CopyToClipboard(GetCellText(static_cast<unsigned int>(cell.GetRow()), static_cast<unsigned int>(cell.GetCol())));
            return;
        }

        const int row = m_grid->GetGridCursorRow();
        const int col = m_grid->GetGridCursorCol();
        if (row >= 0 && col >= 0) {
            CopyToClipboard(GetCellText(static_cast<unsigned int>(row), static_cast<unsigned int>(col)));
        }
    }

    void CopyRow(int row) {
        if (row < 0 || row >= static_cast<int>(m_rows.size()) || m_grid->GetNumberCols() == 0) {
            return;
        }
        CopyToClipboard(GetBlockText(row, 0, row, m_grid->GetNumberCols() - 1));
    }

    void CopyCell(int row, int column) {
        if (row < 0 || column < 0) {
            return;
        }
        CopyToClipboard(GetCellText(static_cast<unsigned int>(row), static_cast<unsigned int>(column)));
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

    void CommitActiveEdit() {
        if (!m_grid->IsCellEditControlShown()) {
            return;
        }

        const int row = m_grid->GetGridCursorRow();
        const int col = m_grid->GetGridCursorCol();
        m_grid->SaveEditControlValue();
        const wxString value = m_grid->GetCellValue(row, col);
        m_grid->HideCellEditControl();
        m_grid->DisableCellEditControl();
        SetCellText(static_cast<unsigned int>(row), static_cast<unsigned int>(col), value);
    }

    void AppendEmptyRow() {
        m_rows.emplace_back(GetColumnCount());
        RefreshGridFromData();
        UpdateStatusBar();
    }

    void InsertColumn(int insertAt) {
        CommitHeaderEdit();
        CommitActiveEdit();

        insertAt = std::clamp(insertAt, 0, static_cast<int>(GetColumnCount()));
        m_headers.insert(m_headers.begin() + insertAt, "New column");
        for (auto& row : m_rows) {
            row.insert(row.begin() + insertAt, wxString());
        }

        NormalizeRows(static_cast<unsigned int>(m_headers.size()));
        RefreshGridFromData();
        ResizeToCsvContent();
        SetDirty(true);
        BeginHeaderEdit(insertAt);
    }

    void InsertRow(int insertAt) {
        CommitHeaderEdit();
        CommitActiveEdit();

        insertAt = std::clamp(insertAt, 0, static_cast<int>(m_rows.size()));
        m_rows.insert(m_rows.begin() + insertAt, std::vector<wxString>(GetColumnCount(), wxString()));

        RefreshGridFromData();
        UpdateStatusBar();
        SetDirty(true);

        if (GetColumnCount() > 0) {
            SelectCell(static_cast<unsigned int>(insertAt), 0);
            m_grid->EnableCellEditControl();
        }
    }

    void MoveToNextEditableCell() {
        if (m_grid->GetNumberCols() == 0) {
            return;
        }

        const int currentRow = m_grid->GetGridCursorRow();
        const int currentCol = m_grid->GetGridCursorCol();
        int nextRow = currentRow;
        int nextCol = currentCol + 1;

        if (nextCol >= m_grid->GetNumberCols()) {
            nextCol = 0;
            ++nextRow;
        }

        if (nextRow >= m_grid->GetNumberRows()) {
            AppendEmptyRow();
        }

        SelectCell(static_cast<unsigned int>(nextRow), static_cast<unsigned int>(nextCol));
        m_grid->EnableCellEditControl();
    }

    void OnContextCopyRow(wxCommandEvent&) {
        CopyRow(GetActiveRowIndex());
    }

    void OnContextCopyCell(wxCommandEvent&) {
        const int row = GetActiveRowIndex();
        const int col = GetActiveColumnIndex();
        CopyCell(row, col);
    }

    void OnInsertColumnBefore(wxCommandEvent&) {
        const int col = GetActiveColumnIndex();
        if (col >= 0) {
            InsertColumn(col);
        }
    }

    void OnInsertColumnAfter(wxCommandEvent&) {
        const int col = GetActiveColumnIndex();
        if (col >= 0) {
            InsertColumn(col + 1);
        }
    }

    void OnInsertRowBefore(wxCommandEvent&) {
        const int row = GetActiveRowIndex();
        if (row >= 0) {
            InsertRow(row);
        }
    }

    void OnInsertRowAfter(wxCommandEvent&) {
        const int row = GetActiveRowIndex();
        if (row >= 0) {
            InsertRow(row + 1);
        }
    }

    void ShowContextMenu(const wxPoint& position) {
        wxMenu menu;
        menu.Append(ID_CONTEXT_COPY_ROW, "&Copy row");
        menu.Append(ID_CONTEXT_COPY_CELL, "Copy &cell");
        menu.AppendSeparator();
        menu.Append(ID_INSERT_ROW_BEFORE, "Insert row &before");
        menu.Append(ID_INSERT_ROW_AFTER, "Insert row &after");
        const bool hasTarget = m_contextRow >= 0;
        menu.Enable(ID_CONTEXT_COPY_ROW, hasTarget);
        menu.Enable(ID_CONTEXT_COPY_CELL, hasTarget && m_contextColumn >= 0);
        menu.Enable(ID_INSERT_ROW_BEFORE, hasTarget);
        menu.Enable(ID_INSERT_ROW_AFTER, hasTarget);
        m_grid->PopupMenu(&menu, position);
    }

    void ShowHeaderContextMenu(const wxPoint& position) {
        wxMenu menu;
        menu.Append(ID_INSERT_COLUMN_BEFORE, "Insert column &before");
        menu.Append(ID_INSERT_COLUMN_AFTER, "Insert column &after");
        const bool hasTarget = m_contextColumn >= 0;
        menu.Enable(ID_INSERT_COLUMN_BEFORE, hasTarget);
        menu.Enable(ID_INSERT_COLUMN_AFTER, hasTarget);
        m_grid->GetGridColLabelWindow()->PopupMenu(&menu, position);
    }

    bool FindInData(bool forward) {
        const wxString query = m_findData.GetFindString();
        const bool matchCase = (m_findData.GetFlags() & wxFR_MATCHCASE) != 0;
        const unsigned int rows = static_cast<unsigned int>(m_rows.size());
        const unsigned int cols = GetColumnCount();
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
            const wxString value = GetCellText(row, col);
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
        m_grid->ClearSelection();
        m_grid->SetGridCursor(static_cast<int>(row), static_cast<int>(col));
        m_grid->SelectBlock(static_cast<int>(row), static_cast<int>(col), static_cast<int>(row), static_cast<int>(col), false);
        m_grid->MakeCellVisible(static_cast<int>(row), static_cast<int>(col));
        m_grid->SetFocus();
        UpdateStatusBar();
    }

    void OnOpen(wxCommandEvent&) {
        if (!ConfirmDirtyFileAction()) {
            return;
        }

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

    void OnNew(wxCommandEvent&) {
        if (!ConfirmDirtyFileAction()) {
            return;
        }

        CreateNewFile();
    }

    void OnExit(wxCommandEvent&) {
        Close();
    }

    void OnSave(wxCommandEvent&) {
        SaveCurrentFile();
    }

    void OnSaveAs(wxCommandEvent&) {
        SaveCurrentFileAs();
    }

    void OnCopy(wxCommandEvent&) {
        CopySelection();
    }

    void OnGoToFirst(wxCommandEvent&) {
        GoToRow(0);
    }

    void OnGoToLast(wxCommandEvent&) {
        if (!m_rows.empty()) {
            GoToRow(static_cast<int>(m_rows.size()) - 1);
        }
    }

    void OnGoToRow(wxCommandEvent&) {
        if (m_rows.empty()) {
            return;
        }

        int selectedRow = -1;
        const int currentRow = std::max(0, m_grid->GetGridCursorRow());
        if (ShowGoToRowDialog(this, static_cast<int>(m_rows.size()), currentRow, &selectedRow)) {
            GoToRow(selectedRow);
        }
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
        ShowAboutDialog(this);
    }

    void OnCellLeftDClick(wxGridEvent& event) {
        CommitHeaderEdit();
        SelectCell(static_cast<unsigned int>(event.GetRow()), static_cast<unsigned int>(event.GetCol()));
        m_grid->EnableCellEditControl();
    }

    void OnCellRightClick(wxGridEvent& event) {
        CancelHeaderEdit();
        m_contextRow = event.GetRow();
        m_contextColumn = event.GetCol();
        m_grid->SetGridCursor(m_contextRow, m_contextColumn);
        ShowContextMenu(event.GetPosition());
    }

    void OnLabelLeftDClick(wxGridEvent& event) {
        if (event.GetRow() == -1 && event.GetCol() >= 0) {
            BeginHeaderEdit(event.GetCol());
            return;
        }
        event.Skip();
    }

    void OnLabelRightClick(wxGridEvent& event) {
        if (event.GetRow() == -1 && event.GetCol() >= 0) {
            CommitActiveEdit();
            m_contextRow = -1;
            m_contextColumn = event.GetCol();
            ShowHeaderContextMenu(event.GetPosition());
            return;
        }
        event.Skip();
    }

    void OnEditorShown(wxGridEvent& event) {
        m_contextRow = event.GetRow();
        m_contextColumn = event.GetCol();
        event.Skip();
    }

    void OnCellChanged(wxGridEvent& event) {
        if (!m_isRefreshingGrid) {
            SyncCellFromGrid(event.GetRow(), event.GetCol());
        }
        event.Skip();
    }

    void OnSelectCell(wxGridEvent& event) {
        CommitHeaderEdit();
        event.Skip();
        UpdateStatusBar();
    }

    void OnGridCharHook(wxKeyEvent& event) {
        if (m_activeHeaderColumn >= 0) {
            event.Skip();
            return;
        }

        if (!m_grid->IsCellEditControlShown()) {
            event.Skip();
            return;
        }

        const int keyCode = event.GetKeyCode();
        if (keyCode == WXK_RETURN || keyCode == WXK_NUMPAD_ENTER) {
            CommitActiveEdit();
            return;
        }

        if (keyCode == WXK_TAB && !event.ShiftDown()) {
            CommitActiveEdit();
            MoveToNextEditableCell();
            return;
        }

        event.Skip();
    }

    void OnClose(wxCloseEvent& event) {
        CommitHeaderEdit();
        if (!ConfirmDirtyFileAction()) {
            event.Veto();
            return;
        }

        if (m_findDialog) {
            m_findDialog->Destroy();
            m_findDialog = nullptr;
        }
        event.Skip();
    }

    void OnHeaderEditorEnter(wxCommandEvent&) {
        CommitHeaderEdit();
        FocusFirstDataCellForEntry();
    }

    void OnHeaderEditorKillFocus(wxFocusEvent& event) {
        CommitHeaderEdit();
        event.Skip();
    }

    void OnHeaderEditorCharHook(wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_ESCAPE) {
            CancelHeaderEdit();
            m_grid->SetFocus();
            return;
        }

        if (event.GetKeyCode() == WXK_TAB) {
            const int currentColumn = m_activeHeaderColumn;
            CommitHeaderEdit();

            if (currentColumn < 0) {
                m_grid->SetFocus();
                return;
            }

            if (event.ShiftDown()) {
                const int previousColumn = std::max(0, currentColumn - 1);
                BeginHeaderEdit(previousColumn);
                return;
            }

            if (currentColumn >= static_cast<int>(GetColumnCount()) - 1) {
                InsertColumn(currentColumn + 1);
                return;
            }

            BeginHeaderEdit(currentColumn + 1);
            return;
        }

        event.Skip();
    }

    void OnGridResized(wxSizeEvent& event) {
        RepositionHeaderEditor();
        event.Skip();
    }

    void OnGridWindowLeftDClick(wxMouseEvent& event) {
        int logicalX = 0;
        int logicalY = 0;
        m_grid->CalcUnscrolledPosition(event.GetX(), event.GetY(), &logicalX, &logicalY);

        if (logicalY >= 0 && m_grid->YToRow(logicalY) == wxNOT_FOUND) {
            AppendEmptyRow();
            if (m_grid->GetNumberCols() > 0) {
                SelectCell(static_cast<unsigned int>(m_grid->GetNumberRows() - 1), 0);
                m_grid->EnableCellEditControl();
            }
            return;
        }

        event.Skip();
    }

private:
    wxGrid* m_grid{nullptr};
    wxTextCtrl* m_headerEditor{nullptr};
    std::vector<std::vector<wxString>> m_rows;
    std::vector<wxString> m_headers;
    wxString m_currentFile;
    wxFindReplaceData m_findData{wxFR_DOWN};
    wxFindReplaceDialog* m_findDialog{nullptr};
    size_t m_lastFindIndex{0};
    bool m_lastFindValid{false};
    bool m_isDirty{false};
    bool m_isRefreshingGrid{false};
    wxString m_documentName{"untitled.csv"};
    int m_contextRow{-1};
    int m_contextColumn{-1};
    int m_activeHeaderColumn{-1};

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(wxID_NEW, MainFrame::OnNew)
    EVT_MENU(wxID_OPEN, MainFrame::OnOpen)
    EVT_MENU(wxID_SAVE, MainFrame::OnSave)
    EVT_MENU(wxID_SAVEAS, MainFrame::OnSaveAs)
    EVT_MENU(wxID_EXIT, MainFrame::OnExit)
    EVT_MENU(wxID_COPY, MainFrame::OnCopy)
    EVT_MENU(ID_GO_TO_FIRST, MainFrame::OnGoToFirst)
    EVT_MENU(ID_GO_TO_LAST, MainFrame::OnGoToLast)
    EVT_MENU(ID_GO_TO_ROW, MainFrame::OnGoToRow)
    EVT_MENU(wxID_FIND, MainFrame::OnFind)
    EVT_MENU(ID_FIND_NEXT, MainFrame::OnFindNext)
    EVT_MENU(ID_FIND_PREVIOUS, MainFrame::OnFindPrevious)
    EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)
    EVT_MENU(ID_CONTEXT_COPY_ROW, MainFrame::OnContextCopyRow)
    EVT_MENU(ID_CONTEXT_COPY_CELL, MainFrame::OnContextCopyCell)
    EVT_MENU(ID_INSERT_ROW_BEFORE, MainFrame::OnInsertRowBefore)
    EVT_MENU(ID_INSERT_ROW_AFTER, MainFrame::OnInsertRowAfter)
    EVT_MENU(ID_INSERT_COLUMN_BEFORE, MainFrame::OnInsertColumnBefore)
    EVT_MENU(ID_INSERT_COLUMN_AFTER, MainFrame::OnInsertColumnAfter)
    EVT_CLOSE(MainFrame::OnClose)
    EVT_FIND(wxID_ANY, MainFrame::OnFindDialog)
    EVT_FIND_NEXT(wxID_ANY, MainFrame::OnFindDialog)
    EVT_FIND_CLOSE(wxID_ANY, MainFrame::OnFindDialogClose)
wxEND_EVENT_TABLE()

class CsvExplorerApp : public wxApp {
public:
    bool OnInit() override {
        wxInitAllImageHandlers();

        wxString initialFile;
        if (argc > 1) {
            initialFile = wxString(argv[1]);
        }
        auto* frame = new MainFrame(initialFile);
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(CsvExplorerApp);
