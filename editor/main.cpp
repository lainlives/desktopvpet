// main.cpp: wxWidgets application entry point for the character editor.
#include <wx/wx.h>

#include <wx/image.h>

#include "editor_frame.hpp"

class EditorApp : public wxApp {
public:
    bool OnInit() override {
        wxInitAllImageHandlers();  // enable PNG/JPG/... loading for the preview
        auto *frame = new dvp::editor::EditorFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(EditorApp);
