#include <wx/wx.h>

#include "main_frame.h"

class CsvExplorerApp : public wxApp {
public:
    bool OnInit() override {
        wxInitAllImageHandlers();

        auto* frame = CreateMainFrame(argc > 1 ? wxString(argv[1]) : wxString());
        SetTopWindow(frame);
        frame->Show();

        for (int i = 2; i < argc; ++i) {
            OpenFileInMainFrame(wxDynamicCast(wxGetActiveWindow(), wxFrame), wxString(argv[i]));
        }

        return true;
    }

#ifdef __WXOSX__
    void MacOpenFile(const wxString& fileName) override {
        wxFrame* frame = wxDynamicCast(wxGetActiveWindow(), wxFrame);
        if (!frame) {
            frame = wxDynamicCast(GetTopWindow(), wxFrame);
        }
        OpenFileInMainFrame(frame, fileName);
    }
#endif
};

wxIMPLEMENT_APP(CsvExplorerApp);
