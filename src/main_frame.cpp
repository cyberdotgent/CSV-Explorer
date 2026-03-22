#include <wx/wx.h>
#include <wx/clipbrd.h>
#include <wx/dnd.h>
#include <wx/fdrepdlg.h>
#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/grid.h>
#include <wx/notebook.h>
#include <wx/textfile.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "about_dialog.h"
#include "config.h"
#include "go_to_row_dialog.h"
#include "main_frame.h"
#include "print_support.h"
#include "unsaved_changes_dialog.h"

namespace {

enum {
    ID_NEW_TAB = wxID_HIGHEST + 1,
    ID_FIND_NEXT,
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

} // namespace

class EditorPage;

class MainFrame : public wxFrame {
public:
    explicit MainFrame(const wxString& initialFile);

    bool OpenDocumentPath(const wxString& path);
    bool OpenDroppedFiles(EditorPage* preferredPage, const wxArrayString& filenames);
    void NotifyPageStateChanged(EditorPage* page);
    EditorPage* GetActivePage() const;

private:
    class FrameFileDropTarget final : public wxFileDropTarget {
    public:
        explicit FrameFileDropTarget(MainFrame& frame)
            : m_frame(frame) {}

        bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override {
            return m_frame.OpenDroppedFiles(m_frame.GetActivePage(), filenames);
        }

    private:
        MainFrame& m_frame;
    };

    void BuildMenuBar();
    void BuildAccelerators();
    void BuildNotebook();
    void BuildStatusBar();
    void ApplyWindowIcon();
    void UpdateFrameTitle();
    void UpdateStatusBar();
    bool ConfirmCloseAllPages();
    EditorPage* CreateBlankTab(bool activate, bool startEditingHeader);
    bool OpenPathInPreferredPage(EditorPage* preferredPage, const wxString& path);

    void OnNewWindow(wxCommandEvent&);
    void OnNewTab(wxCommandEvent&);
    void OnOpen(wxCommandEvent&);
    void OnSave(wxCommandEvent&);
    void OnSaveAs(wxCommandEvent&);
    void OnPrintPreview(wxCommandEvent&);
    void OnPrint(wxCommandEvent&);
    void OnExit(wxCommandEvent&);
    void OnCopy(wxCommandEvent&);
    void OnGoToFirst(wxCommandEvent&);
    void OnGoToLast(wxCommandEvent&);
    void OnGoToRow(wxCommandEvent&);
    void OnFind(wxCommandEvent&);
    void OnFindNext(wxCommandEvent&);
    void OnFindPrevious(wxCommandEvent&);
    void OnAbout(wxCommandEvent&);
    void OnNotebookPageChanged(wxBookCtrlEvent&);
    void OnActivate(wxActivateEvent&);
    void OnClose(wxCloseEvent&);

    wxNotebook* m_notebook{nullptr};
};

class EditorPage : public wxPanel {
public:
    EditorPage(MainFrame& owner, wxWindow* parent);
    ~EditorPage() override;

    bool OpenDocumentFile(const wxString& path);
    bool SaveCurrentFile();
    bool SaveCurrentFileAs();
    bool ConfirmClose();
    void CreateBlankDocument(bool startEditingHeader);
    bool IsEffectivelyEmptyDocument() const;
    bool IsDirty() const {
        return m_isDirty;
    }

    wxString GetDisplayFileName() const;
    wxString GetTabLabel() const;
    PrintableDocument BuildPrintableDocument() const;
    wxString GetStatusPrimaryText() const;
    wxString GetStatusSecondaryText() const;

    void CopySelection();
    void GoToFirst();
    void GoToLast();
    void PromptGoToRow();
    void ShowFindDialog();
    void FindNext();
    void FindPrevious();
    void ShowPrintPreview();
    void ShowPrint();
    void FocusEditor();

private:
    class CsvFileDropTarget final : public wxFileDropTarget {
    public:
        explicit CsvFileDropTarget(EditorPage& page)
            : m_page(page) {}

        bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override {
            return m_page.HandleDroppedFiles(filenames);
        }

    private:
        EditorPage& m_page;
    };

    void BuildGrid();
    void NotifyStateChanged();
    void ResizeOwnerToContent();
    unsigned int GetColumnCount() const;
    void NormalizeRows(unsigned int minimumColumnCount);
    void EnsureRowCount(int desiredRows);
    void EnsureColumnCount(int desiredColumns);
    wxString GetCellText(unsigned int row, unsigned int col) const;
    void SetCellText(unsigned int row, unsigned int col, const wxString& value);
    void RefreshGridFromData();
    int GetActiveRowIndex() const;
    int GetActiveColumnIndex() const;
    void GoToRow(int row);
    wxRect GetHeaderRect(int col) const;
    void RepositionHeaderEditor();
    void BeginHeaderEdit(int col);
    void FocusFirstDataCellForEntry();
    void CommitHeaderEdit();
    void CancelHeaderEdit();
    void SyncCellFromGrid(int row, int col);
    bool SaveToPath(const wxString& path);
    void OpenFileInternal(const wxString& path);
    bool HandleDroppedFiles(const wxArrayString& filenames);
    wxString GetBlockText(int topRow, int leftCol, int bottomRow, int rightCol) const;
    void CopyRow(int row);
    void CopyCell(int row, int column);
    void CopyToClipboard(const wxString& output);
    void CommitActiveEdit();
    void AppendEmptyRow();
    void InsertColumn(int insertAt);
    void InsertRow(int insertAt);
    void MoveToNextEditableCell();
    void SelectCell(unsigned int row, unsigned int col);
    bool FindInData(bool forward);
    void SetDirty(bool dirty);

    void OnContextCopyRow(wxCommandEvent&);
    void OnContextCopyCell(wxCommandEvent&);
    void OnInsertColumnBefore(wxCommandEvent&);
    void OnInsertColumnAfter(wxCommandEvent&);
    void OnInsertRowBefore(wxCommandEvent&);
    void OnInsertRowAfter(wxCommandEvent&);
    void ShowContextMenu(const wxPoint& position);
    void ShowHeaderContextMenu(const wxPoint& position);
    void OnCellLeftDClick(wxGridEvent& event);
    void OnCellRightClick(wxGridEvent& event);
    void OnLabelLeftDClick(wxGridEvent& event);
    void OnLabelRightClick(wxGridEvent& event);
    void OnEditorShown(wxGridEvent& event);
    void OnCellChanged(wxGridEvent& event);
    void OnSelectCell(wxGridEvent& event);
    void OnGridCharHook(wxKeyEvent& event);
    void OnHeaderEditorEnter(wxCommandEvent&);
    void OnHeaderEditorKillFocus(wxFocusEvent& event);
    void OnHeaderEditorCharHook(wxKeyEvent& event);
    void OnGridResized(wxSizeEvent& event);
    void OnGridWindowLeftDClick(wxMouseEvent& event);
    void OnFindDialog(wxFindDialogEvent& event);
    void OnFindDialogClose(wxFindDialogEvent&);

    MainFrame& m_owner;
    wxGrid* m_grid{nullptr};
    wxTextCtrl* m_headerEditor{nullptr};
    std::vector<std::vector<wxString>> m_rows;
    std::vector<wxString> m_headers;
    wxString m_currentFile;
    wxString m_documentName{"untitled.csv"};
    wxFindReplaceData m_findData{wxFR_DOWN};
    wxFindReplaceDialog* m_findDialog{nullptr};
    wxPrintData m_printData;
    size_t m_lastFindIndex{0};
    bool m_lastFindValid{false};
    bool m_isDirty{false};
    bool m_isRefreshingGrid{false};
    int m_contextRow{-1};
    int m_contextColumn{-1};
    int m_activeHeaderColumn{-1};
};

static MainFrame* CreateAndShowMainFrame(const wxString& initialFile) {
    auto* frame = new MainFrame(initialFile);
    frame->Show();
    frame->Raise();
    if (wxTheApp) {
        wxTheApp->SetTopWindow(frame);
    }
    return frame;
}

static MainFrame* FindOtherMainFrame(const MainFrame* current) {
    for (wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext()) {
        auto* frame = dynamic_cast<MainFrame*>(node->GetData());
        if (frame && frame != current) {
            return frame;
        }
    }
    return nullptr;
}

MainFrame::MainFrame(const wxString& initialFile)
    : wxFrame(nullptr, wxID_ANY, CSV_EXPLORER_NAME, wxDefaultPosition, wxSize(900, 600)) {
    SetDropTarget(new FrameFileDropTarget(*this));
    BuildMenuBar();
    BuildAccelerators();
    BuildNotebook();
    BuildStatusBar();
    ApplyWindowIcon();

    EditorPage* page = CreateBlankTab(true, initialFile.IsEmpty());
    if (!initialFile.IsEmpty()) {
        OpenPathInPreferredPage(page, initialFile);
    }
}

void MainFrame::BuildMenuBar() {
    auto* fileMenu = new wxMenu();
    fileMenu->Append(wxID_NEW, "&New Window\tCtrl+N");
    fileMenu->Append(ID_NEW_TAB, "New &Tab\tCtrl+T");
    fileMenu->Append(wxID_OPEN, "&Open...\tCtrl+O");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_SAVE, "&Save\tCtrl+S");
    fileMenu->Append(wxID_SAVEAS, "Save &As...\tCtrl+Shift+S");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_PREVIEW, "Print Pre&view\tCtrl+Shift+P");
    fileMenu->Append(wxID_PRINT, "&Print...\tCtrl+P");
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
    goToMenu->Append(ID_GO_TO_FIRST, "Go to &First\tCtrl+Up");
    goToMenu->Append(ID_GO_TO_LAST, "Go to &Last\tCtrl+Down");
    goToMenu->Append(ID_GO_TO_ROW, "Go to &Row...\tCtrl+G");
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

void MainFrame::BuildAccelerators() {
    wxAcceleratorEntry entries[] = {
        { wxACCEL_CTRL, 'N', wxID_NEW },
        { wxACCEL_CTRL, 'T', ID_NEW_TAB },
        { wxACCEL_CTRL, 'O', wxID_OPEN },
        { wxACCEL_CTRL, 'S', wxID_SAVE },
        { wxACCEL_CTRL | wxACCEL_SHIFT, 'S', wxID_SAVEAS },
        { wxACCEL_CTRL, 'P', wxID_PRINT },
        { wxACCEL_CTRL | wxACCEL_SHIFT, 'P', wxID_PREVIEW },
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

    SetAcceleratorTable(wxAcceleratorTable(static_cast<int>(sizeof(entries) / sizeof(entries[0])), entries));
}

void MainFrame::BuildNotebook() {
    m_notebook = new wxNotebook(this, wxID_ANY);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_notebook, 1, wxEXPAND | wxALL, 0);
    SetSizer(sizer);

    Bind(wxEVT_MENU, &MainFrame::OnNewWindow, this, wxID_NEW);
    Bind(wxEVT_MENU, &MainFrame::OnNewTab, this, ID_NEW_TAB);
    Bind(wxEVT_MENU, &MainFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &MainFrame::OnSaveAs, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &MainFrame::OnPrintPreview, this, wxID_PREVIEW);
    Bind(wxEVT_MENU, &MainFrame::OnPrint, this, wxID_PRINT);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnCopy, this, wxID_COPY);
    Bind(wxEVT_MENU, &MainFrame::OnGoToFirst, this, ID_GO_TO_FIRST);
    Bind(wxEVT_MENU, &MainFrame::OnGoToLast, this, ID_GO_TO_LAST);
    Bind(wxEVT_MENU, &MainFrame::OnGoToRow, this, ID_GO_TO_ROW);
    Bind(wxEVT_MENU, &MainFrame::OnFind, this, wxID_FIND);
    Bind(wxEVT_MENU, &MainFrame::OnFindNext, this, ID_FIND_NEXT);
    Bind(wxEVT_MENU, &MainFrame::OnFindPrevious, this, ID_FIND_PREVIOUS);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &MainFrame::OnNotebookPageChanged, this);
    Bind(wxEVT_ACTIVATE, &MainFrame::OnActivate, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
}

void MainFrame::BuildStatusBar() {
    CreateStatusBar(2);
    const int widths[] = { -1, FromDIP(180) };
    GetStatusBar()->SetStatusWidths(2, widths);
    UpdateStatusBar();
}

void MainFrame::ApplyWindowIcon() {
#ifdef __WXMSW__
    SetIcon(wxICON(IDI_APP_ICON));
    return;
#endif

    wxIcon icon = LoadAppIcon();
    if (icon.IsOk()) {
        SetIcon(icon);
    }
}

EditorPage* MainFrame::CreateBlankTab(bool activate, bool startEditingHeader) {
    auto* page = new EditorPage(*this, m_notebook);
    const size_t pageIndex = static_cast<size_t>(m_notebook->GetPageCount());
    m_notebook->AddPage(page, "untitled.csv", activate);
    page->CreateBlankDocument(startEditingHeader);
    NotifyPageStateChanged(page);

    if (!activate && pageIndex < static_cast<size_t>(m_notebook->GetPageCount())) {
        m_notebook->SetSelection(m_notebook->GetSelection());
    }

    return page;
}

EditorPage* MainFrame::GetActivePage() const {
    if (!m_notebook) {
        return nullptr;
    }

    const int selection = m_notebook->GetSelection();
    if (selection == wxNOT_FOUND) {
        return nullptr;
    }

    return dynamic_cast<EditorPage*>(m_notebook->GetPage(static_cast<size_t>(selection)));
}

void MainFrame::NotifyPageStateChanged(EditorPage* page) {
    if (!page || !m_notebook) {
        return;
    }

    const int index = m_notebook->FindPage(page);
    if (index != wxNOT_FOUND) {
        m_notebook->SetPageText(static_cast<size_t>(index), page->GetTabLabel());
    }

    if (page == GetActivePage()) {
        UpdateFrameTitle();
        UpdateStatusBar();
    }
}

void MainFrame::UpdateFrameTitle() {
    EditorPage* page = GetActivePage();
    const wxString label = page ? page->GetTabLabel() : "untitled.csv";
    SetTitle(wxString::Format("%s - %s", CSV_EXPLORER_NAME, label));
}

void MainFrame::UpdateStatusBar() {
    if (!GetStatusBar()) {
        return;
    }

    EditorPage* page = GetActivePage();
    if (!page) {
        SetStatusText({}, 0);
        SetStatusText({}, 1);
        return;
    }

    SetStatusText(page->GetStatusPrimaryText(), 0);
    SetStatusText(page->GetStatusSecondaryText(), 1);
}

bool MainFrame::OpenPathInPreferredPage(EditorPage* preferredPage, const wxString& path) {
    if (preferredPage && preferredPage->IsEffectivelyEmptyDocument()) {
        m_notebook->SetSelection(static_cast<size_t>(m_notebook->FindPage(preferredPage)));
        return preferredPage->OpenDocumentFile(path);
    }

    CreateAndShowMainFrame(path);
    return true;
}

bool MainFrame::OpenDocumentPath(const wxString& path) {
    return OpenPathInPreferredPage(GetActivePage(), path);
}

bool MainFrame::OpenDroppedFiles(EditorPage* preferredPage, const wxArrayString& filenames) {
    if (filenames.empty()) {
        return false;
    }

    bool openedAny = false;
    bool firstFile = true;
    for (const wxString& filename : filenames) {
        if (firstFile) {
            openedAny = OpenPathInPreferredPage(preferredPage, filename) || openedAny;
            firstFile = false;
        } else {
            CreateAndShowMainFrame(filename);
            openedAny = true;
        }
    }

    return openedAny;
}

bool MainFrame::ConfirmCloseAllPages() {
    if (!m_notebook) {
        return true;
    }

    for (size_t i = 0; i < m_notebook->GetPageCount(); ++i) {
        auto* page = dynamic_cast<EditorPage*>(m_notebook->GetPage(i));
        if (page && !page->ConfirmClose()) {
            return false;
        }
    }

    return true;
}

void MainFrame::OnNewWindow(wxCommandEvent&) {
    CreateAndShowMainFrame({});
}

void MainFrame::OnNewTab(wxCommandEvent&) {
    CreateBlankTab(true, true);
}

void MainFrame::OnOpen(wxCommandEvent&) {
    wxFileDialog dialog(
        this,
        "Open CSV file",
        {},
        {},
        "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) {
        OpenDocumentPath(dialog.GetPath());
    }
}

void MainFrame::OnSave(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->SaveCurrentFile();
    }
}

void MainFrame::OnSaveAs(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->SaveCurrentFileAs();
    }
}

void MainFrame::OnPrintPreview(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->ShowPrintPreview();
    }
}

void MainFrame::OnPrint(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->ShowPrint();
    }
}

void MainFrame::OnExit(wxCommandEvent&) {
    Close();
}

void MainFrame::OnCopy(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->CopySelection();
    }
}

void MainFrame::OnGoToFirst(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->GoToFirst();
    }
}

void MainFrame::OnGoToLast(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->GoToLast();
    }
}

void MainFrame::OnGoToRow(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->PromptGoToRow();
    }
}

void MainFrame::OnFind(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->ShowFindDialog();
    }
}

void MainFrame::OnFindNext(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->FindNext();
    }
}

void MainFrame::OnFindPrevious(wxCommandEvent&) {
    if (EditorPage* page = GetActivePage()) {
        page->FindPrevious();
    }
}

void MainFrame::OnAbout(wxCommandEvent&) {
    ShowAboutDialog(this);
}

void MainFrame::OnNotebookPageChanged(wxBookCtrlEvent& event) {
    UpdateFrameTitle();
    UpdateStatusBar();
    if (EditorPage* page = GetActivePage()) {
        page->FocusEditor();
    }
    event.Skip();
}

void MainFrame::OnActivate(wxActivateEvent& event) {
    if (event.GetActive() && wxTheApp) {
        wxTheApp->SetTopWindow(this);
    }
    event.Skip();
}

void MainFrame::OnClose(wxCloseEvent& event) {
    if (!ConfirmCloseAllPages()) {
        event.Veto();
        return;
    }

    MainFrame* otherFrame = FindOtherMainFrame(this);
    if (wxTheApp && wxTheApp->GetTopWindow() == this) {
        wxTheApp->SetTopWindow(otherFrame);
    }

    event.Skip();

    if (!otherFrame && wxTheApp) {
        wxTheApp->CallAfter([]
        {
            if (wxTheApp) {
                wxTheApp->ExitMainLoop();
            }
        });
    }
}

EditorPage::EditorPage(MainFrame& owner, wxWindow* parent)
    : wxPanel(parent),
      m_owner(owner) {
    SetDropTarget(new CsvFileDropTarget(*this));
    BuildGrid();

    Bind(wxEVT_MENU, &EditorPage::OnContextCopyRow, this, ID_CONTEXT_COPY_ROW);
    Bind(wxEVT_MENU, &EditorPage::OnContextCopyCell, this, ID_CONTEXT_COPY_CELL);
    Bind(wxEVT_MENU, &EditorPage::OnInsertRowBefore, this, ID_INSERT_ROW_BEFORE);
    Bind(wxEVT_MENU, &EditorPage::OnInsertRowAfter, this, ID_INSERT_ROW_AFTER);
    Bind(wxEVT_MENU, &EditorPage::OnInsertColumnBefore, this, ID_INSERT_COLUMN_BEFORE);
    Bind(wxEVT_MENU, &EditorPage::OnInsertColumnAfter, this, ID_INSERT_COLUMN_AFTER);
    Bind(wxEVT_FIND, &EditorPage::OnFindDialog, this);
    Bind(wxEVT_FIND_NEXT, &EditorPage::OnFindDialog, this);
    Bind(wxEVT_FIND_CLOSE, &EditorPage::OnFindDialogClose, this);
}

EditorPage::~EditorPage() {
    if (m_findDialog) {
        m_findDialog->Destroy();
        m_findDialog = nullptr;
    }
}

void EditorPage::BuildGrid() {
    m_grid = new wxGrid(this, wxID_ANY);
    m_grid->CreateGrid(0, 0);
    m_grid->EnableEditing(true);
    m_grid->EnableDragRowSize(false);
    m_grid->EnableDragColSize(true);
    m_grid->SetDefaultCellOverflow(false);
    m_grid->SetRowLabelSize(0);
    m_grid->SetColLabelAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
    m_grid->SetDropTarget(new CsvFileDropTarget(*this));
    m_grid->GetGridWindow()->SetDropTarget(new CsvFileDropTarget(*this));

    m_grid->Bind(wxEVT_GRID_CELL_LEFT_DCLICK, &EditorPage::OnCellLeftDClick, this);
    m_grid->Bind(wxEVT_GRID_CELL_RIGHT_CLICK, &EditorPage::OnCellRightClick, this);
    m_grid->Bind(wxEVT_GRID_LABEL_LEFT_DCLICK, &EditorPage::OnLabelLeftDClick, this);
    m_grid->Bind(wxEVT_GRID_LABEL_RIGHT_CLICK, &EditorPage::OnLabelRightClick, this);
    m_grid->Bind(wxEVT_GRID_SELECT_CELL, &EditorPage::OnSelectCell, this);
    m_grid->Bind(wxEVT_GRID_CELL_CHANGED, &EditorPage::OnCellChanged, this);
    m_grid->Bind(wxEVT_GRID_EDITOR_SHOWN, &EditorPage::OnEditorShown, this);
    m_grid->Bind(wxEVT_CHAR_HOOK, &EditorPage::OnGridCharHook, this);
    m_grid->Bind(wxEVT_SIZE, &EditorPage::OnGridResized, this);
    m_grid->GetGridWindow()->Bind(wxEVT_LEFT_DCLICK, &EditorPage::OnGridWindowLeftDClick, this);

    m_headerEditor = new wxTextCtrl(
        m_grid->GetGridColLabelWindow(),
        wxID_ANY,
        {},
        wxDefaultPosition,
        wxDefaultSize,
        wxTE_PROCESS_ENTER);
    m_headerEditor->Hide();
    m_headerEditor->Bind(wxEVT_TEXT_ENTER, &EditorPage::OnHeaderEditorEnter, this);
    m_headerEditor->Bind(wxEVT_KILL_FOCUS, &EditorPage::OnHeaderEditorKillFocus, this);
    m_headerEditor->Bind(wxEVT_CHAR_HOOK, &EditorPage::OnHeaderEditorCharHook, this);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_grid, 1, wxEXPAND | wxALL, 0);
    SetSizer(sizer);
}

void EditorPage::NotifyStateChanged() {
    m_owner.NotifyPageStateChanged(this);
}

void EditorPage::SetDirty(bool dirty) {
    if (m_isDirty == dirty) {
        return;
    }

    m_isDirty = dirty;
    NotifyStateChanged();
}

unsigned int EditorPage::GetColumnCount() const {
    unsigned int columnCount = static_cast<unsigned int>(m_headers.size());
    for (const auto& row : m_rows) {
        columnCount = std::max(columnCount, static_cast<unsigned int>(row.size()));
    }
    return columnCount;
}

void EditorPage::NormalizeRows(unsigned int minimumColumnCount) {
    const unsigned int columnCount = std::max(GetColumnCount(), minimumColumnCount);
    m_headers.resize(columnCount);
    for (auto& row : m_rows) {
        row.resize(columnCount);
    }
}

void EditorPage::EnsureRowCount(int desiredRows) {
    const int currentRows = m_grid->GetNumberRows();
    if (currentRows < desiredRows) {
        m_grid->AppendRows(desiredRows - currentRows);
    } else if (currentRows > desiredRows) {
        m_grid->DeleteRows(0, currentRows - desiredRows);
    }
}

void EditorPage::EnsureColumnCount(int desiredColumns) {
    const int currentColumns = m_grid->GetNumberCols();
    if (currentColumns < desiredColumns) {
        m_grid->AppendCols(desiredColumns - currentColumns);
    } else if (currentColumns > desiredColumns) {
        m_grid->DeleteCols(desiredColumns, currentColumns - desiredColumns);
    }
}

wxString EditorPage::GetCellText(unsigned int row, unsigned int col) const {
    if (row < m_rows.size() && col < m_rows[row].size()) {
        return m_rows[row][col];
    }
    return {};
}

void EditorPage::SetCellText(unsigned int row, unsigned int col, const wxString& value) {
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

void EditorPage::RefreshGridFromData() {
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
    NotifyStateChanged();
}

bool EditorPage::IsEffectivelyEmptyDocument() const {
    if (!m_currentFile.IsEmpty()) {
        return false;
    }

    if (GetColumnCount() != 1 || m_headers.size() != 1) {
        return false;
    }

    const wxString& header = m_headers[0];
    if (!header.IsEmpty() && header != "New column" && header != "Column 1") {
        return false;
    }

    for (const auto& row : m_rows) {
        for (const auto& cell : row) {
            if (!cell.IsEmpty()) {
                return false;
            }
        }
    }

    return true;
}

wxString EditorPage::GetDisplayFileName() const {
    if (!m_currentFile.IsEmpty()) {
        return wxFileName(m_currentFile).GetFullName();
    }
    return m_documentName;
}

wxString EditorPage::GetTabLabel() const {
    wxString label = GetDisplayFileName();
    if (m_isDirty) {
        label += " *";
    }
    return label;
}

PrintableDocument EditorPage::BuildPrintableDocument() const {
    PrintableDocument document;
    document.title = GetDisplayFileName();
    document.headers = m_headers;
    document.rows = m_rows;
    return document;
}

wxString EditorPage::GetStatusPrimaryText() const {
    return m_isDirty ? "Modified" : wxString();
}

wxString EditorPage::GetStatusSecondaryText() const {
    const int totalRows = static_cast<int>(m_rows.size());
    const int selectedRow = m_grid ? m_grid->GetGridCursorRow() : -1;
    return wxString::Format(
        "Row %d of %d",
        totalRows > 0 && selectedRow >= 0 ? selectedRow + 1 : 0,
        totalRows);
}

int EditorPage::GetActiveRowIndex() const {
    if (m_contextRow >= 0) {
        return m_contextRow;
    }
    return m_grid ? m_grid->GetGridCursorRow() : -1;
}

int EditorPage::GetActiveColumnIndex() const {
    if (m_contextColumn >= 0) {
        return m_contextColumn;
    }
    return m_grid ? m_grid->GetGridCursorCol() : -1;
}

void EditorPage::GoToRow(int row) {
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

wxRect EditorPage::GetHeaderRect(int col) const {
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

void EditorPage::RepositionHeaderEditor() {
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

void EditorPage::BeginHeaderEdit(int col) {
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

void EditorPage::FocusFirstDataCellForEntry() {
    if (!m_grid || m_grid->GetNumberCols() <= 0 || m_grid->GetNumberRows() <= 0) {
        return;
    }

    SelectCell(0, 0);
    m_grid->EnableCellEditControl();
}

void EditorPage::CommitHeaderEdit() {
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

void EditorPage::CancelHeaderEdit() {
    if (!m_headerEditor) {
        return;
    }

    m_headerEditor->Hide();
    m_activeHeaderColumn = -1;
}

void EditorPage::SyncCellFromGrid(int row, int col) {
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

bool EditorPage::SaveToPath(const wxString& path) {
    CommitHeaderEdit();
    CommitActiveEdit();

    wxFFile file(path, "w");
    if (!file.IsOpened()) {
        wxMessageBox(wxString::Format("Unable to write file:\n%s", path), "Save CSV", wxOK | wxICON_ERROR, this);
        return false;
    }

    std::vector<wxString> headers = m_headers;
    headers.resize(GetColumnCount());
    if (!file.Write(BuildCsvLine(headers) + "\n")) {
        wxMessageBox(wxString::Format("Unable to write file:\n%s", path), "Save CSV", wxOK | wxICON_ERROR, this);
        return false;
    }

    for (const auto& row : m_rows) {
        std::vector<wxString> normalizedRow = row;
        normalizedRow.resize(GetColumnCount());
        if (!file.Write(BuildCsvLine(normalizedRow) + "\n")) {
            wxMessageBox(wxString::Format("Unable to write file:\n%s", path), "Save CSV", wxOK | wxICON_ERROR, this);
            return false;
        }
    }

    m_currentFile = path;
    m_documentName = wxFileName(path).GetFullName();
    SetDirty(false);
    NotifyStateChanged();
    return true;
}

bool EditorPage::SaveCurrentFile() {
    if (m_currentFile.IsEmpty()) {
        return SaveCurrentFileAs();
    }
    return SaveToPath(m_currentFile);
}

bool EditorPage::SaveCurrentFileAs() {
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

bool EditorPage::ConfirmClose() {
    CommitHeaderEdit();
    if (!m_isDirty || IsEffectivelyEmptyDocument()) {
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

void EditorPage::CreateBlankDocument(bool startEditingHeader) {
    CancelHeaderEdit();
    CommitActiveEdit();

    m_rows.assign(1, std::vector<wxString>(1, wxString()));
    m_headers.assign(1, wxString());
    m_currentFile.clear();
    m_documentName = "untitled.csv";
    m_lastFindValid = false;
    m_lastFindIndex = 0;
    m_contextRow = -1;
    m_contextColumn = -1;

    RefreshGridFromData();
    ResizeOwnerToContent();
    m_isDirty = false;
    NotifyStateChanged();

    if (startEditingHeader && m_grid->GetNumberCols() > 0) {
        BeginHeaderEdit(0);
    }
}

void EditorPage::OpenFileInternal(const wxString& path) {
    CancelHeaderEdit();

    std::vector<std::vector<wxString>> rows;
    std::vector<wxString> headers;
    if (!wxFileExists(path)) {
        wxMessageBox(wxString::Format("File not found:\n%s", path), "Open CSV", wxOK | wxICON_ERROR, this);
        return;
    }
    if (!ParseCsvFile(path, headers, rows)) {
        wxMessageBox(wxString::Format("Unable to read file:\n%s", path), "Open CSV", wxOK | wxICON_ERROR, this);
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
    m_isDirty = false;
    NotifyStateChanged();
    ResizeOwnerToContent();
}

bool EditorPage::OpenDocumentFile(const wxString& path) {
    if (!ConfirmClose()) {
        return false;
    }

    const wxString previousPath = m_currentFile;
    OpenFileInternal(path);
    return m_currentFile == path && previousPath != path ? true : m_currentFile == path;
}

bool EditorPage::HandleDroppedFiles(const wxArrayString& filenames) {
    return m_owner.OpenDroppedFiles(this, filenames);
}

void EditorPage::ResizeOwnerToContent() {
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

    const int frameExtraX = m_owner.GetSize().GetWidth() - m_owner.GetClientSize().GetWidth();
    const int frameExtraY = m_owner.GetSize().GetHeight() - m_owner.GetClientSize().GetHeight();
    const int verticalScrollbar = 18;
    const int horizontalScrollbar = 18;
    const int headerHeight = rowHeight + 12;
    const unsigned int visibleRows = std::min<unsigned int>(rows, 24u);

    const int desiredWidth = totalWidth + frameExtraX + verticalScrollbar + m_grid->GetRowLabelSize() + 4;
    const int desiredHeight = static_cast<int>(visibleRows * rowHeight + headerHeight + frameExtraY + horizontalScrollbar + 24);

    const int newWidth = std::max(640, std::min(desiredWidth, maxWidth));
    const int newHeight = std::max(360, std::min(desiredHeight, maxHeight));
    m_owner.SetSize(newWidth, newHeight);
    m_owner.Centre();
}

wxString EditorPage::GetBlockText(int topRow, int leftCol, int bottomRow, int rightCol) const {
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

void EditorPage::CopyToClipboard(const wxString& output) {
    if (output.IsEmpty()) {
        return;
    }

    auto* text = new wxTextDataObject(output);
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(text);
        wxTheClipboard->Close();
    }
}

void EditorPage::CopySelection() {
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

void EditorPage::CopyRow(int row) {
    if (row < 0 || row >= static_cast<int>(m_rows.size()) || m_grid->GetNumberCols() == 0) {
        return;
    }
    CopyToClipboard(GetBlockText(row, 0, row, m_grid->GetNumberCols() - 1));
}

void EditorPage::CopyCell(int row, int column) {
    if (row < 0 || column < 0) {
        return;
    }
    CopyToClipboard(GetCellText(static_cast<unsigned int>(row), static_cast<unsigned int>(column)));
}

void EditorPage::CommitActiveEdit() {
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

void EditorPage::AppendEmptyRow() {
    m_rows.emplace_back(GetColumnCount());
    RefreshGridFromData();
}

void EditorPage::InsertColumn(int insertAt) {
    CommitHeaderEdit();
    CommitActiveEdit();

    insertAt = std::clamp(insertAt, 0, static_cast<int>(GetColumnCount()));
    m_headers.insert(m_headers.begin() + insertAt, "New column");
    for (auto& row : m_rows) {
        row.insert(row.begin() + insertAt, wxString());
    }

    NormalizeRows(static_cast<unsigned int>(m_headers.size()));
    RefreshGridFromData();
    ResizeOwnerToContent();
    SetDirty(true);
    BeginHeaderEdit(insertAt);
}

void EditorPage::InsertRow(int insertAt) {
    CommitHeaderEdit();
    CommitActiveEdit();

    insertAt = std::clamp(insertAt, 0, static_cast<int>(m_rows.size()));
    m_rows.insert(m_rows.begin() + insertAt, std::vector<wxString>(GetColumnCount(), wxString()));

    RefreshGridFromData();
    SetDirty(true);

    if (GetColumnCount() > 0) {
        SelectCell(static_cast<unsigned int>(insertAt), 0);
        m_grid->EnableCellEditControl();
    }
}

void EditorPage::MoveToNextEditableCell() {
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

void EditorPage::SelectCell(unsigned int row, unsigned int col) {
    m_grid->ClearSelection();
    m_grid->SetGridCursor(static_cast<int>(row), static_cast<int>(col));
    m_grid->SelectBlock(static_cast<int>(row), static_cast<int>(col), static_cast<int>(row), static_cast<int>(col), false);
    m_grid->MakeCellVisible(static_cast<int>(row), static_cast<int>(col));
    m_grid->SetFocus();
    NotifyStateChanged();
}

bool EditorPage::FindInData(bool forward) {
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
            const size_t lastIndex = m_lastFindIndex;
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

void EditorPage::GoToFirst() {
    GoToRow(0);
}

void EditorPage::GoToLast() {
    if (!m_rows.empty()) {
        GoToRow(static_cast<int>(m_rows.size()) - 1);
    }
}

void EditorPage::PromptGoToRow() {
    if (m_rows.empty()) {
        return;
    }

    int selectedRow = -1;
    const int currentRow = std::max(0, m_grid->GetGridCursorRow());
    if (ShowGoToRowDialog(this, static_cast<int>(m_rows.size()), currentRow, &selectedRow)) {
        GoToRow(selectedRow);
    }
}

void EditorPage::ShowFindDialog() {
    if (!m_findDialog) {
        m_findDialog = new wxFindReplaceDialog(this, &m_findData, "Find");
    }
    m_findDialog->Show();
    m_findDialog->Raise();
}

void EditorPage::FindNext() {
    if (m_findData.GetFindString().IsEmpty()) {
        ShowFindDialog();
        return;
    }
    if (!FindInData(true)) {
        wxMessageBox("No matching data found.", "Find", wxOK | wxICON_INFORMATION, this);
    }
}

void EditorPage::FindPrevious() {
    if (m_findData.GetFindString().IsEmpty()) {
        ShowFindDialog();
        return;
    }
    if (!FindInData(false)) {
        wxMessageBox("No matching data found.", "Find", wxOK | wxICON_INFORMATION, this);
    }
}

void EditorPage::ShowPrintPreview() {
    CommitHeaderEdit();
    CommitActiveEdit();
    if (m_printData.GetOrientation() != wxLANDSCAPE && m_printData.GetOrientation() != wxPORTRAIT) {
        m_printData.SetOrientation(GuessLandscapeForPrint(BuildPrintableDocument()) ? wxLANDSCAPE : wxPORTRAIT);
    }
    ShowPrintPreviewWindow(this, BuildPrintableDocument(), m_printData);
}

void EditorPage::ShowPrint() {
    CommitHeaderEdit();
    CommitActiveEdit();
    if (m_printData.GetOrientation() != wxLANDSCAPE && m_printData.GetOrientation() != wxPORTRAIT) {
        m_printData.SetOrientation(GuessLandscapeForPrint(BuildPrintableDocument()) ? wxLANDSCAPE : wxPORTRAIT);
    }
    ShowPrintDialogForDocument(this, BuildPrintableDocument(), m_printData);
}

void EditorPage::FocusEditor() {
    if (m_headerEditor && m_headerEditor->IsShown()) {
        m_headerEditor->SetFocus();
        return;
    }
    if (m_grid) {
        m_grid->SetFocus();
    }
}

void EditorPage::OnContextCopyRow(wxCommandEvent&) {
    CopyRow(GetActiveRowIndex());
}

void EditorPage::OnContextCopyCell(wxCommandEvent&) {
    CopyCell(GetActiveRowIndex(), GetActiveColumnIndex());
}

void EditorPage::OnInsertColumnBefore(wxCommandEvent&) {
    const int col = GetActiveColumnIndex();
    if (col >= 0) {
        InsertColumn(col);
    }
}

void EditorPage::OnInsertColumnAfter(wxCommandEvent&) {
    const int col = GetActiveColumnIndex();
    if (col >= 0) {
        InsertColumn(col + 1);
    }
}

void EditorPage::OnInsertRowBefore(wxCommandEvent&) {
    const int row = GetActiveRowIndex();
    if (row >= 0) {
        InsertRow(row);
    }
}

void EditorPage::OnInsertRowAfter(wxCommandEvent&) {
    const int row = GetActiveRowIndex();
    if (row >= 0) {
        InsertRow(row + 1);
    }
}

void EditorPage::ShowContextMenu(const wxPoint& position) {
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

void EditorPage::ShowHeaderContextMenu(const wxPoint& position) {
    wxMenu menu;
    menu.Append(ID_INSERT_COLUMN_BEFORE, "Insert column &before");
    menu.Append(ID_INSERT_COLUMN_AFTER, "Insert column &after");
    const bool hasTarget = m_contextColumn >= 0;
    menu.Enable(ID_INSERT_COLUMN_BEFORE, hasTarget);
    menu.Enable(ID_INSERT_COLUMN_AFTER, hasTarget);
    m_grid->GetGridColLabelWindow()->PopupMenu(&menu, position);
}

void EditorPage::OnCellLeftDClick(wxGridEvent& event) {
    CommitHeaderEdit();
    SelectCell(static_cast<unsigned int>(event.GetRow()), static_cast<unsigned int>(event.GetCol()));
    m_grid->EnableCellEditControl();
}

void EditorPage::OnCellRightClick(wxGridEvent& event) {
    CancelHeaderEdit();
    m_contextRow = event.GetRow();
    m_contextColumn = event.GetCol();
    m_grid->SetGridCursor(m_contextRow, m_contextColumn);
    ShowContextMenu(event.GetPosition());
}

void EditorPage::OnLabelLeftDClick(wxGridEvent& event) {
    if (event.GetRow() == -1 && event.GetCol() >= 0) {
        BeginHeaderEdit(event.GetCol());
        return;
    }
    event.Skip();
}

void EditorPage::OnLabelRightClick(wxGridEvent& event) {
    if (event.GetRow() == -1 && event.GetCol() >= 0) {
        CommitActiveEdit();
        m_contextRow = -1;
        m_contextColumn = event.GetCol();
        ShowHeaderContextMenu(event.GetPosition());
        return;
    }
    event.Skip();
}

void EditorPage::OnEditorShown(wxGridEvent& event) {
    m_contextRow = event.GetRow();
    m_contextColumn = event.GetCol();
    event.Skip();
}

void EditorPage::OnCellChanged(wxGridEvent& event) {
    if (!m_isRefreshingGrid) {
        SyncCellFromGrid(event.GetRow(), event.GetCol());
    }
    event.Skip();
}

void EditorPage::OnSelectCell(wxGridEvent& event) {
    CommitHeaderEdit();
    event.Skip();
    NotifyStateChanged();
}

void EditorPage::OnGridCharHook(wxKeyEvent& event) {
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

void EditorPage::OnHeaderEditorEnter(wxCommandEvent&) {
    CommitHeaderEdit();
    FocusFirstDataCellForEntry();
}

void EditorPage::OnHeaderEditorKillFocus(wxFocusEvent& event) {
    CommitHeaderEdit();
    event.Skip();
}

void EditorPage::OnHeaderEditorCharHook(wxKeyEvent& event) {
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
            BeginHeaderEdit(std::max(0, currentColumn - 1));
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

void EditorPage::OnGridResized(wxSizeEvent& event) {
    RepositionHeaderEditor();
    event.Skip();
}

void EditorPage::OnGridWindowLeftDClick(wxMouseEvent& event) {
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

void EditorPage::OnFindDialog(wxFindDialogEvent& event) {
    m_findData.SetFindString(event.GetFindString());
    const bool forward = (event.GetFlags() & wxFR_DOWN) != 0;
    if (!FindInData(forward)) {
        wxMessageBox("No matching data found.", "Find", wxOK | wxICON_INFORMATION, this);
    }
}

void EditorPage::OnFindDialogClose(wxFindDialogEvent&) {
    if (m_findDialog) {
        m_findDialog->Destroy();
        m_findDialog = nullptr;
    }
}

wxFrame* CreateMainFrame(const wxString& initialFile) {
    return new MainFrame(initialFile);
}

bool OpenFileInMainFrame(wxFrame* frame, const wxString& path) {
    auto* mainFrame = dynamic_cast<MainFrame*>(frame);
    if (!mainFrame) {
        CreateAndShowMainFrame(path);
        return true;
    }

    return mainFrame->OpenDocumentPath(path);
}
