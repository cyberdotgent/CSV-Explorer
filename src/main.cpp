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
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(CsvExplorerApp);
