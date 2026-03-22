#include <wx/wx.h>

#include "main_frame.h"

class CsvExplorerApp : public wxApp {
public:
    bool OnInit() override {
        wxInitAllImageHandlers();

        wxString initialFile;
        if (argc > 1) {
            initialFile = wxString(argv[1]);
        }

        auto* frame = CreateMainFrame(initialFile);
        SetTopWindow(frame);
        frame->Show();
        return true;
    }

#ifdef __WXOSX__
    void MacOpenFile(const wxString& fileName) override {
        wxFrame* frame = wxDynamicCast(GetTopWindow(), wxFrame);
        if (!frame) {
            return;
        }

        OpenFileInMainFrame(frame, fileName);
    }
#endif
};

wxIMPLEMENT_APP(CsvExplorerApp);
