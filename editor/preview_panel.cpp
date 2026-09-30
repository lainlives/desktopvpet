// preview_panel.cpp: frame loading + drawing and the drag-to-slice helper.
#include "preview_panel.hpp"

#include <wx/dcbuffer.h>

#include <algorithm>

namespace dvp::editor {

PreviewPanel::PreviewPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE),
      timer_(this) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(*wxBLACK);

    Bind(wxEVT_PAINT, &PreviewPanel::OnPaint, this);
    Bind(wxEVT_TIMER, &PreviewPanel::OnTimer, this);
    Bind(wxEVT_LEFT_DOWN, &PreviewPanel::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &PreviewPanel::OnLeftUp, this);
}

void PreviewPanel::SetSource(const CharacterModel &model, const AnimSource *anim) {
    frames_.clear();
    frame_ = 0;
    fps_ = 5.0;
    rect_mode_ = false;
    atlas_path_.clear();

    if (!anim) {
        timer_.Stop();
        Refresh();
        return;
    }
    fps_ = anim->fps;
    rect_mode_ = (anim->mode == "rects");
    atlas_path_ = model.dir + "/" + model.src_dir + "/" + anim->atlas;

    auto load = [](const std::string &path, wxImage &out) {
        return out.LoadFile(wxString::FromUTF8(path), wxBITMAP_TYPE_ANY);
    };

    if (anim->mode == "images") {
        for (const std::string &name : anim->images) {
            wxImage img;
            if (load(model.dir + "/" + model.src_dir + "/" + name, img)) {
                frames_.push_back(std::move(img));
            }
        }
    } else {
        wxImage atlas;
        if (!anim->atlas.empty() && load(atlas_path_, atlas)) {
            const int fw = std::max(1, model.frame_width);
            const int fh = std::max(1, model.frame_height);
            if (anim->mode == "atlas") {
                int cols = anim->cols > 0 ? anim->cols : std::max(1, atlas.GetWidth() / fw);
                for (int i = 0; i < anim->frames; ++i) {
                    const int idx = anim->start + i;
                    const int cx = (idx % cols) * fw;
                    const int cy = (idx / cols) * fh;
                    if (cx + fw > atlas.GetWidth() || cy + fh > atlas.GetHeight()) {
                        break;
                    }
                    frames_.push_back(atlas.GetSubImage(wxRect(cx, cy, fw, fh)));
                }
            } else {
                for (const AnimRect &r : anim->rects) {
                    if (r.x + r.w > atlas.GetWidth() || r.y + r.h > atlas.GetHeight()) {
                        continue;
                    }
                    frames_.push_back(atlas.GetSubImage(wxRect(r.x, r.y, r.w, r.h)));
                }
            }
        }
    }

    if (!frames_.empty() && fps_ > 0.0 && frames_.size() > 1) {
        timer_.Start(static_cast<int>(1000.0 / fps_));
    } else {
        timer_.Stop();
    }
    Refresh();
}

void PreviewPanel::OnTimer(wxTimerEvent &) {
    if (frames_.size() > 1) {
        frame_ = (frame_ + 1) % static_cast<int>(frames_.size());
        Refresh();
    }
}

void PreviewPanel::OnPaint(wxPaintEvent &) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();

    const wxSize size = GetClientSize();
    if (frames_.empty()) {
        dc.SetTextForeground(*wxLIGHT_GREY);
        dc.DrawText(_("No preview (add images or an atlas)"), wxPoint(10, 10));
        scale_ = 1.0;
        offset_ = wxPoint(0, 0);
        frame_size_ = wxSize(0, 0);
        return;
    }

    wxImage &img = frames_[static_cast<std::size_t>(frame_) % frames_.size()];
    frame_size_ = wxSize(img.GetWidth(), img.GetHeight());
    const double sx = static_cast<double>(size.x) / std::max(1, img.GetWidth());
    const double sy = static_cast<double>(size.y) / std::max(1, img.GetHeight());
    scale_ = std::min({sx, sy, 8.0});
    const int w = std::max(1, static_cast<int>(img.GetWidth() * scale_));
    const int h = std::max(1, static_cast<int>(img.GetHeight() * scale_));
    offset_ = wxPoint((size.x - w) / 2, (size.y - h) / 2);

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(*wxWHITE_BRUSH);
    // Checkerboard-ish background so transparency is visible.
    dc.SetBackground(wxBrush(*wxBLACK));
    dc.DrawRectangle(offset_.x - 1, offset_.y - 1, w + 2, h + 2);
    dc.DrawBitmap(wxBitmap(img.Scale(w, h, wxIMAGE_QUALITY_NEAREST)), offset_.x, offset_.y,
                  false);

    if (dragging_) {
        const wxPoint cur = wxGetMousePosition() - ClientToScreen(wxPoint(0, 0));
        dc.SetPen(*wxGREEN_PEN);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(wxRect(drag_start_, cur));
    }
}

wxPoint PreviewPanel::ToImage(const wxPoint &panel_pt) const {
    if (scale_ <= 0.0) {
        return wxPoint(0, 0);
    }
    return wxPoint(static_cast<int>((panel_pt.x - offset_.x) / scale_),
                   static_cast<int>((panel_pt.y - offset_.y) / scale_));
}

void PreviewPanel::OnLeftDown(wxMouseEvent &event) {
    if (rect_mode_ && !frames_.empty()) {
        dragging_ = true;
        drag_start_ = event.GetPosition();
        CaptureMouse();
    }
}

void PreviewPanel::OnLeftUp(wxMouseEvent &event) {
    if (!dragging_) {
        return;
    }
    dragging_ = false;
    if (HasCapture()) {
        ReleaseMouse();
    }
    const wxPoint a = ToImage(drag_start_);
    const wxPoint b = ToImage(event.GetPosition());
    const int x = std::min(a.x, b.x);
    const int y = std::min(a.y, b.y);
    const int w = std::abs(b.x - a.x);
    const int h = std::abs(b.y - a.y);
    if (w > 0 && h > 0 && on_rect_) {
        on_rect_(x, y, w, h);
    }
}

}  // namespace dvp::editor
