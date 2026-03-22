#pragma once

class wxFrame;
class wxString;

wxFrame* CreateMainFrame(const wxString& initialFile);
bool OpenFileInMainFrame(wxFrame* frame, const wxString& path);
