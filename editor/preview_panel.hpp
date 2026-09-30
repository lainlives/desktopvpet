// preview_panel.hpp: animated canvas that previews one animation's frames.
#pragma once

#include <wx/wx.h>

#include <functional>
#include <vector>

#include "character_model.hpp"

namespace dvp::editor {

// Draws the selected animation at its fps. In "rects" mode a left-drag on the
// canvas emits the picked source rectangle via the callback (visual slicer).
class PreviewPanel : public wxPanel {
public:
    explicit PreviewPanel(wxWindow *parent);

    void SetSource(const CharacterModel &model, const AnimSource *anim);
    void SetRectPickedCallback(std::function<void(int, int, int, int)> cb) {
        on_rect_ = std::move(cb);
    }

private:
    void OnPaint(wxPaintEvent &event);
    void OnTimer(wxTimerEvent &event);
    void OnLeftDown(wxMouseEvent &event);
    void OnLeftUp(wxMouseEvent &event);
    void Reload();

    wxPoint ToImage(const wxPoint &panel_pt) const;

    std::vector<wxImage> frames_;
    int frame_ = 0;
    double fps_ = 5.0;
    bool rect_mode_ = false;
    std::string atlas_path_;
    wxTimer timer_;
    wxPoint drag_start_;
    bool dragging_ = false;
    std::function<void(int, int, int, int)> on_rect_;

    // Cached layout for mouse mapping.
    double scale_ = 1.0;
    wxPoint offset_{0, 0};
    wxSize frame_size_{0, 0};
};

}  // namespace dvp::editor
