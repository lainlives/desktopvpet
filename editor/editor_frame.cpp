// editor_frame.cpp: layout and behaviour of the character editor.
#include "editor_frame.hpp"

#include <wx/filename.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>
#include <wx/textdlg.h>
#include <wx/dirdlg.h>
#include <wx/toolbar.h>

#include <algorithm>
#include <filesystem>

#include "log.hpp"

#ifndef DVP_SOURCE_DIR
#define DVP_SOURCE_DIR "."
#endif

namespace dvp::editor {

EditorFrame::EditorFrame()
    : wxFrame(nullptr, wxID_ANY, "DesktopVPet Character Editor", wxDefaultPosition,
              wxSize(1200, 760)) {
    CreateStatusBar();

    // --- Toolbar ---------------------------------------------------------
    wxToolBar *toolbar = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);
    toolbar->AddTool(wxID_NEW, "New Character", wxNullBitmap, "Create a character folder");
    toolbar->AddTool(wxID_OPEN, "Open", wxNullBitmap, "Open a character folder");
    toolbar->AddTool(wxID_SAVE, "Save", wxNullBitmap, "Save sources.json");
    toolbar->AddTool(ID_PACK, "Pack Sprites", wxNullBitmap,
                     "Run tools/pack_sprites.py to build sheets + character.json");
    toolbar->Realize();
    Bind(wxEVT_TOOL, &EditorFrame::OnNew, this, wxID_NEW);
    Bind(wxEVT_TOOL, &EditorFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_TOOL, &EditorFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_TOOL, &EditorFrame::OnPack, this, ID_PACK);

    // --- Three panes -----------------------------------------------------
    wxSplitterWindow *outer = new wxSplitterWindow(this, wxID_ANY);
    outer->SetMinimumPaneSize(200);
    wxPanel *global_panel = BuildGlobalPanel(outer);

    wxSplitterWindow *inner = new wxSplitterWindow(outer, wxID_ANY);
    inner->SetMinimumPaneSize(240);
    wxPanel *center_panel = BuildCenterPanel(inner);
    wxPanel *right_panel = BuildRightPanel(inner);
    inner->SplitVertically(center_panel, right_panel, 520);

    outer->SplitVertically(global_panel, inner, 300);

    auto *top = new wxBoxSizer(wxVERTICAL);
    top->Add(outer, 1, wxEXPAND);
    SetSizer(top);

    Bind(wxEVT_MENU, &EditorFrame::OnQuit, this, wxID_EXIT);
    RefreshAll();
    SetStatusText("Ready. Use New or Open to begin.");
}

// ---------------------------------------------------------------------------
// Pane construction
// ---------------------------------------------------------------------------

wxPanel *EditorFrame::BuildGlobalPanel(wxWindow *parent) {
    auto *panel = new wxPanel(parent);
    auto *grid = new wxFlexGridSizer(2, 6, 6);
    grid->AddGrowableCol(1, 1);

    auto label = [&](const char *text) { grid->Add(new wxStaticText(panel, wxID_ANY, text)); };

    label("Name:");
    name_ctrl_ = new wxTextCtrl(panel, wxID_ANY);
    grid->Add(name_ctrl_, 1, wxEXPAND);

    label("Frame width:");
    frame_w_ctrl_ = new wxSpinCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS, 1, 4096, model_.frame_width);
    grid->Add(frame_w_ctrl_, 1, wxEXPAND);

    label("Frame height:");
    frame_h_ctrl_ = new wxSpinCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS, 1, 4096, model_.frame_height);
    grid->Add(frame_h_ctrl_, 1, wxEXPAND);

    label("Font:");
    font_ctrl_ = new wxTextCtrl(panel, wxID_ANY);
    grid->Add(font_ctrl_, 1, wxEXPAND);

    label("Font size:");
    font_size_ctrl_ = new wxSpinCtrlDouble(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                           wxSP_ARROW_KEYS, 4, 200, model_.font_size, 1);
    grid->Add(font_size_ctrl_, 1, wxEXPAND);

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(panel, wxID_ANY, "Global attributes"), 0, wxALL, 6);
    sizer->Add(grid, 0, wxEXPAND | wxALL, 6);

    // Dialogue manager.
    sizer->Add(new wxStaticText(panel, wxID_ANY, "Dialogue"), 0, wxLEFT | wxTOP, 6);
    dialogue_list_ = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(-1, 180));
    sizer->Add(dialogue_list_, 1, wxEXPAND | wxALL, 6);
    auto *buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->Add(new wxButton(panel, ID_DIALOGUE_ADD, "Add"), 0, wxRIGHT, 4);
    buttons->Add(new wxButton(panel, ID_DIALOGUE_REMOVE, "Remove"), 0);
    sizer->Add(buttons, 0, wxLEFT | wxBOTTOM, 6);
    panel->SetSizer(sizer);

    // Apply global changes as they happen.
    for (wxWindow *w : {static_cast<wxWindow *>(name_ctrl_), static_cast<wxWindow *>(font_ctrl_)}) {
        w->Bind(wxEVT_TEXT, &EditorFrame::OnGlobalChanged, this);
    }
    frame_w_ctrl_->Bind(wxEVT_SPINCTRL, &EditorFrame::OnGlobalChanged, this);
    frame_h_ctrl_->Bind(wxEVT_SPINCTRL, &EditorFrame::OnGlobalChanged, this);
    font_size_ctrl_->Bind(wxEVT_SPINCTRLDOUBLE, &EditorFrame::OnGlobalChanged, this);
    Bind(wxEVT_BUTTON, &EditorFrame::OnDialogueAdd, this, ID_DIALOGUE_ADD);
    Bind(wxEVT_BUTTON, &EditorFrame::OnDialogueRemove, this, ID_DIALOGUE_REMOVE);
    return panel;
}

wxPanel *EditorFrame::BuildCenterPanel(wxWindow *parent) {
    auto *panel = new wxPanel(parent);
    auto *notebook = new wxNotebook(panel, wxID_ANY);

    // --- Animations tab ---
    auto *anim_page = new wxPanel(notebook);
    auto *anim_sizer = new wxBoxSizer(wxHORIZONTAL);

    auto *list_col = new wxBoxSizer(wxVERTICAL);
    list_col->Add(new wxStaticText(anim_page, wxID_ANY, "Animations"), 0, wxBOTTOM, 4);
    anim_list_ = new wxListBox(anim_page, wxID_ANY, wxDefaultPosition, wxSize(160, -1));
    list_col->Add(anim_list_, 1, wxEXPAND);
    auto *anim_btns = new wxBoxSizer(wxHORIZONTAL);
    anim_btns->Add(new wxButton(anim_page, ID_ANIM_ADD, "Add"), 0, wxRIGHT, 4);
    anim_btns->Add(new wxButton(anim_page, ID_ANIM_REMOVE, "Remove"), 0);
    list_col->Add(anim_btns, 0, wxTOP, 4);
    anim_sizer->Add(list_col, 0, wxEXPAND | wxALL, 6);

    auto *detail = new wxBoxSizer(wxVERTICAL);
    auto *grid = new wxFlexGridSizer(2, 6, 6);
    grid->AddGrowableCol(1, 1);
    grid->Add(new wxStaticText(anim_page, wxID_ANY, "FPS:"));
    anim_fps_ctrl_ = new wxSpinCtrlDouble(anim_page, wxID_ANY, "", wxDefaultPosition,
                                          wxDefaultSize, wxSP_ARROW_KEYS, 0.1, 120, 5, 0.5);
    grid->Add(anim_fps_ctrl_, 1, wxEXPAND);
    grid->Add(new wxStaticText(anim_page, wxID_ANY, "Loop:"));
    anim_loop_ctrl_ = new wxCheckBox(anim_page, wxID_ANY, "loop");
    grid->Add(anim_loop_ctrl_);
    grid->Add(new wxStaticText(anim_page, wxID_ANY, "Source mode:"));
    wxArrayString modes;
    modes.Add("images");
    modes.Add("atlas");
    modes.Add("rects");
    anim_mode_ctrl_ = new wxChoice(anim_page, wxID_ANY, wxDefaultPosition, wxDefaultSize, modes);
    grid->Add(anim_mode_ctrl_, 1, wxEXPAND);
    detail->Add(grid, 0, wxEXPAND | wxALL, 6);

    detail->Add(new wxStaticLine(anim_page), 0, wxEXPAND | wxALL, 4);
    detail->Add(new wxStaticText(anim_page, wxID_ANY, "Images (loose frames)"), 0, wxLEFT, 6);
    image_list_ = new wxListBox(anim_page, wxID_ANY, wxDefaultPosition, wxSize(-1, 90));
    detail->Add(image_list_, 1, wxEXPAND | wxALL, 6);
    auto *img_btns = new wxBoxSizer(wxHORIZONTAL);
    img_btns->Add(new wxButton(anim_page, ID_IMAGE_ADD, "Add files"), 0, wxRIGHT, 4);
    img_btns->Add(new wxButton(anim_page, ID_IMAGE_REMOVE, "Remove"), 0);
    detail->Add(img_btns, 0, wxLEFT | wxBOTTOM, 6);

    detail->Add(new wxStaticLine(anim_page), 0, wxEXPAND | wxALL, 4);
    auto *atlas_grid = new wxFlexGridSizer(2, 6, 6);
    atlas_grid->AddGrowableCol(1, 1);
    atlas_grid->Add(new wxStaticText(anim_page, wxID_ANY, "Atlas:"));
    auto *atlas_row = new wxBoxSizer(wxHORIZONTAL);
    atlas_ctrl_ = new wxTextCtrl(anim_page, wxID_ANY);
    atlas_row->Add(atlas_ctrl_, 1, wxEXPAND);
    atlas_row->Add(new wxButton(anim_page, ID_BROWSE_ATLAS, "..."), 0, wxLEFT, 4);
    atlas_grid->Add(atlas_row, 1, wxEXPAND);
    atlas_grid->Add(new wxStaticText(anim_page, wxID_ANY, "Cols:"));
    cols_ctrl_ = new wxSpinCtrl(anim_page, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, 0, 4096, 0);
    atlas_grid->Add(cols_ctrl_, 1, wxEXPAND);
    atlas_grid->Add(new wxStaticText(anim_page, wxID_ANY, "Frames:"));
    frames_ctrl_ = new wxSpinCtrl(anim_page, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                  wxSP_ARROW_KEYS, 1, 4096, 1);
    atlas_grid->Add(frames_ctrl_, 1, wxEXPAND);
    atlas_grid->Add(new wxStaticText(anim_page, wxID_ANY, "Start:"));
    start_ctrl_ = new wxSpinCtrl(anim_page, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                 wxSP_ARROW_KEYS, 0, 100000, 0);
    atlas_grid->Add(start_ctrl_, 1, wxEXPAND);
    detail->Add(atlas_grid, 0, wxEXPAND | wxALL, 6);

    anim_sizer->Add(detail, 1, wxEXPAND);
    anim_page->SetSizer(anim_sizer);
    notebook->AddPage(anim_page, "Animations");

    // --- Actions tab ---
    auto *role_page = new wxPanel(notebook);
    auto *role_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto *role_col = new wxBoxSizer(wxVERTICAL);
    role_col->Add(new wxStaticText(role_page, wxID_ANY, "Roles"), 0, wxBOTTOM, 4);
    wxArrayString roles;
    for (const std::string &r : CharacterModel::RoleNames()) {
        roles.Add(r);
    }
    role_list_ = new wxListBox(role_page, wxID_ANY, wxDefaultPosition, wxSize(160, -1), roles);
    role_col->Add(role_list_, 1, wxEXPAND);
    role_sizer->Add(role_col, 0, wxEXPAND | wxALL, 6);

    auto *var_col = new wxBoxSizer(wxVERTICAL);
    var_col->Add(new wxStaticText(role_page, wxID_ANY, "Variants (animation + weight)"), 0,
                 wxBOTTOM, 4);
    variant_list_ = new wxListBox(role_page, wxID_ANY, wxDefaultPosition, wxSize(-1, 200));
    var_col->Add(variant_list_, 1, wxEXPAND);
    auto *var_btns = new wxBoxSizer(wxHORIZONTAL);
    var_btns->Add(new wxButton(role_page, ID_VARIANT_ADD, "Add"), 0, wxRIGHT, 4);
    var_btns->Add(new wxButton(role_page, ID_VARIANT_REMOVE, "Remove"), 0);
    var_col->Add(var_btns, 0, wxTOP | wxBOTTOM, 4);
    auto *var_grid = new wxFlexGridSizer(2, 6, 6);
    var_grid->AddGrowableCol(1, 1);
    var_grid->Add(new wxStaticText(role_page, wxID_ANY, "Animation:"));
    variant_anim_ctrl_ = new wxChoice(role_page, wxID_ANY);
    var_grid->Add(variant_anim_ctrl_, 1, wxEXPAND);
    var_grid->Add(new wxStaticText(role_page, wxID_ANY, "Weight:"));
    variant_weight_ctrl_ = new wxSpinCtrl(role_page, wxID_ANY, "", wxDefaultPosition,
                                          wxDefaultSize, wxSP_ARROW_KEYS, 1, 1000, 1);
    var_grid->Add(variant_weight_ctrl_, 1, wxEXPAND);
    var_col->Add(var_grid, 0, wxEXPAND);
    role_sizer->Add(var_col, 1, wxEXPAND | wxALL, 6);
    role_page->SetSizer(role_sizer);
    notebook->AddPage(role_page, "Actions");

    auto *panel_sizer = new wxBoxSizer(wxVERTICAL);
    panel_sizer->Add(notebook, 1, wxEXPAND);
    panel->SetSizer(panel_sizer);

    // Bindings.
    anim_list_->Bind(wxEVT_LISTBOX, &EditorFrame::OnAnimationSelected, this);
    Bind(wxEVT_BUTTON, &EditorFrame::OnAnimationAdd, this, ID_ANIM_ADD);
    Bind(wxEVT_BUTTON, &EditorFrame::OnAnimationRemove, this, ID_ANIM_REMOVE);
    Bind(wxEVT_BUTTON, &EditorFrame::OnImageAdd, this, ID_IMAGE_ADD);
    Bind(wxEVT_BUTTON, &EditorFrame::OnImageRemove, this, ID_IMAGE_REMOVE);
    Bind(wxEVT_BUTTON, &EditorFrame::OnBrowseAtlas, this, ID_BROWSE_ATLAS);
    anim_fps_ctrl_->Bind(wxEVT_SPINCTRLDOUBLE, &EditorFrame::OnAnimationChanged, this);
    anim_loop_ctrl_->Bind(wxEVT_CHECKBOX, &EditorFrame::OnAnimationChanged, this);
    anim_mode_ctrl_->Bind(wxEVT_CHOICE, &EditorFrame::OnAnimationChanged, this);
    cols_ctrl_->Bind(wxEVT_SPINCTRL, &EditorFrame::OnAnimationChanged, this);
    frames_ctrl_->Bind(wxEVT_SPINCTRL, &EditorFrame::OnAnimationChanged, this);
    start_ctrl_->Bind(wxEVT_SPINCTRL, &EditorFrame::OnAnimationChanged, this);
    atlas_ctrl_->Bind(wxEVT_TEXT, &EditorFrame::OnAnimationChanged, this);

    role_list_->Bind(wxEVT_LISTBOX, &EditorFrame::OnRoleSelected, this);
    variant_list_->Bind(wxEVT_LISTBOX, &EditorFrame::OnRoleSelected, this);
    Bind(wxEVT_BUTTON, &EditorFrame::OnVariantAdd, this, ID_VARIANT_ADD);
    Bind(wxEVT_BUTTON, &EditorFrame::OnVariantRemove, this, ID_VARIANT_REMOVE);
    variant_anim_ctrl_->Bind(wxEVT_CHOICE, &EditorFrame::OnVariantChanged, this);
    variant_weight_ctrl_->Bind(wxEVT_SPINCTRL, &EditorFrame::OnVariantChanged, this);
    return panel;
}

wxPanel *EditorFrame::BuildRightPanel(wxWindow *parent) {
    auto *panel = new wxPanel(parent);
    auto *notebook = new wxNotebook(panel, wxID_ANY);

    preview_ = new PreviewPanel(notebook);
    preview_->SetRectPickedCallback(
        [this](int x, int y, int w, int h) { OnRectPicked(x, y, w, h); });
    notebook->AddPage(preview_, "Preview");

    assets_ = new wxGenericDirCtrl(notebook, wxID_ANY, wxGetCwd(), wxDefaultPosition,
                                   wxDefaultSize, wxDIRCTRL_3D_INTERNAL);
    notebook->AddPage(assets_, "Assets");

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(notebook, 1, wxEXPAND);
    panel->SetSizer(sizer);
    return panel;
}

// ---------------------------------------------------------------------------
// Selection helpers
// ---------------------------------------------------------------------------

AnimSource *EditorFrame::SelectedAnim() {
    const int index = anim_list_ ? anim_list_->GetSelection() : wxNOT_FOUND;
    if (index == wxNOT_FOUND || index >= static_cast<int>(model_.animations.size())) {
        return nullptr;
    }
    return &model_.animations[static_cast<std::size_t>(index)];
}

std::string EditorFrame::SelectedRole() {
    const int index = role_list_ ? role_list_->GetSelection() : wxNOT_FOUND;
    const auto &roles = CharacterModel::RoleNames();
    if (index < 0 || index >= static_cast<int>(roles.size())) {
        return {};
    }
    return roles[static_cast<std::size_t>(index)];
}

void EditorFrame::MarkDirty() {
    dirty_ = true;
    SetStatusText("Modified");
}

// ---------------------------------------------------------------------------
// Refresh (model -> UI)
// ---------------------------------------------------------------------------

void EditorFrame::RefreshAll() {
    updating_ = true;
    RefreshGlobal();
    RefreshAnimations();
    RefreshRoles();
    updating_ = false;
    UpdatePreview();
}

void EditorFrame::RefreshGlobal() {
    name_ctrl_->SetValue(model_.name);
    frame_w_ctrl_->SetValue(model_.frame_width);
    frame_h_ctrl_->SetValue(model_.frame_height);
    font_ctrl_->SetValue(model_.font);
    font_size_ctrl_->SetValue(model_.font_size);
    dialogue_list_->Clear();
    for (const std::string &line : model_.dialogue) {
        dialogue_list_->Append(line);
    }
}

void EditorFrame::RefreshAnimations() {
    const int previous = anim_list_->GetSelection();
    anim_list_->Clear();
    for (const AnimSource &a : model_.animations) {
        anim_list_->Append(a.name);
    }
    if (!model_.animations.empty()) {
        anim_list_->SetSelection(previous >= 0 && previous < static_cast<int>(model_.animations.size())
                                     ? previous
                                     : 0);
    }
    RefreshAnimationDetail();
}

void EditorFrame::RefreshAnimationDetail() {
    const bool was_updating = updating_;
    updating_ = true;
    AnimSource *a = SelectedAnim();
    const bool enabled = a != nullptr;
    anim_fps_ctrl_->Enable(enabled);
    anim_loop_ctrl_->Enable(enabled);
    anim_mode_ctrl_->Enable(enabled);

    // Animation choices for role mapping (shared list).
    variant_anim_ctrl_->Clear();
    for (const std::string &n : model_.AnimationNames()) {
        variant_anim_ctrl_->Append(n);
    }

    if (a) {
        anim_fps_ctrl_->SetValue(a->fps);
        anim_loop_ctrl_->SetValue(a->loop);
        anim_mode_ctrl_->SetStringSelection(a->mode);
        const bool images = a->mode == "images";
        const bool atlas = a->mode == "atlas";
        image_list_->Enable(images);
        atlas_ctrl_->Enable(!images);
        cols_ctrl_->Enable(atlas);
        frames_ctrl_->Enable(atlas);
        start_ctrl_->Enable(atlas);

        image_list_->Clear();
        for (const std::string &im : a->images) {
            image_list_->Append(im);
        }
        atlas_ctrl_->SetValue(a->atlas);
        cols_ctrl_->SetValue(a->cols);
        frames_ctrl_->SetValue(a->frames);
        start_ctrl_->SetValue(a->start);
    } else {
        image_list_->Clear();
        atlas_ctrl_->Clear();
        cols_ctrl_->SetValue(0);
        frames_ctrl_->SetValue(1);
        start_ctrl_->SetValue(0);
    }
    updating_ = was_updating;
}

void EditorFrame::RefreshRoles() {
    const bool was_updating = updating_;
    updating_ = true;
    if (role_list_->GetSelection() == wxNOT_FOUND) {
        role_list_->SetSelection(0);
    }
    RefreshRoleDetail();
    updating_ = was_updating;
}

void EditorFrame::RefreshRoleDetail() {
    variant_list_->Clear();
    const std::string role = SelectedRole();
    auto it = model_.actions.find(role);
    if (it != model_.actions.end()) {
        for (const RoleVariant &v : it->second) {
            variant_list_->Append(wxString::Format("%s  (w=%d)", v.animation, v.weight));
        }
    }
    variant_anim_ctrl_->Clear();
    for (const std::string &n : model_.AnimationNames()) {
        variant_anim_ctrl_->Append(n);
    }
}

void EditorFrame::UpdatePreview() {
    preview_->SetSource(model_, SelectedAnim());
}

// ---------------------------------------------------------------------------
// Editing (UI -> model)
// ---------------------------------------------------------------------------

void EditorFrame::ApplyGlobalFromUI() {
    model_.name = name_ctrl_->GetValue().ToStdString();
    model_.frame_width = frame_w_ctrl_->GetValue();
    model_.frame_height = frame_h_ctrl_->GetValue();
    model_.font = font_ctrl_->GetValue().ToStdString();
    model_.font_size = font_size_ctrl_->GetValue();
}

void EditorFrame::ApplyAnimationFromUI() {
    AnimSource *a = SelectedAnim();
    if (!a) {
        return;
    }
    a->fps = anim_fps_ctrl_->GetValue();
    a->loop = anim_loop_ctrl_->GetValue();
    a->mode = anim_mode_ctrl_->GetStringSelection().ToStdString();
    a->atlas = atlas_ctrl_->GetValue().ToStdString();
    a->cols = cols_ctrl_->GetValue();
    a->frames = frames_ctrl_->GetValue();
    a->start = start_ctrl_->GetValue();
}

void EditorFrame::ApplyVariantFromUI() {
    const std::string role = SelectedRole();
    const int index = variant_list_->GetSelection();
    auto it = model_.actions.find(role);
    if (it == model_.actions.end() || index < 0 ||
        index >= static_cast<int>(it->second.size())) {
        return;
    }
    RoleVariant &v = it->second[static_cast<std::size_t>(index)];
    v.animation = variant_anim_ctrl_->GetStringSelection().ToStdString();
    v.weight = variant_weight_ctrl_->GetValue();
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

void EditorFrame::OnGlobalChanged(wxCommandEvent &) {
    if (updating_) {
        return;
    }
    ApplyGlobalFromUI();
    MarkDirty();
    UpdatePreview();
}

void EditorFrame::OnDialogueAdd(wxCommandEvent &) {
    const wxString line = wxGetTextFromUser("Speech-bubble line:", "Add dialogue", "", this);
    if (line.IsEmpty()) {
        return;
    }
    model_.dialogue.push_back(line.ToStdString());
    dialogue_list_->Append(line);
    MarkDirty();
}

void EditorFrame::OnDialogueRemove(wxCommandEvent &) {
    const int index = dialogue_list_->GetSelection();
    if (index == wxNOT_FOUND) {
        return;
    }
    dialogue_list_->Delete(static_cast<unsigned>(index));
    model_.dialogue.erase(model_.dialogue.begin() + index);
    MarkDirty();
}

void EditorFrame::OnAnimationSelected(wxCommandEvent &) {
    RefreshAnimationDetail();
    UpdatePreview();
}

void EditorFrame::OnAnimationAdd(wxCommandEvent &) {
    const wxString name = wxGetTextFromUser("Animation name (e.g. idle):", "Add animation",
                                            "idle", this);
    if (name.IsEmpty()) {
        return;
    }
    AnimSource a;
    a.name = name.ToStdString();
    a.mode = "images";
    model_.animations.push_back(a);
    RefreshAnimations();
    anim_list_->SetSelection(static_cast<int>(model_.animations.size()) - 1);
    RefreshAnimationDetail();
    UpdatePreview();
    MarkDirty();
}

void EditorFrame::OnAnimationRemove(wxCommandEvent &) {
    const int index = anim_list_->GetSelection();
    if (index == wxNOT_FOUND) {
        return;
    }
    model_.animations.erase(model_.animations.begin() + index);
    RefreshAnimations();
    UpdatePreview();
    MarkDirty();
}

void EditorFrame::OnAnimationChanged(wxCommandEvent &) {
    if (updating_) {
        return;
    }
    ApplyAnimationFromUI();
    RefreshAnimationDetail();
    UpdatePreview();
    MarkDirty();
}

void EditorFrame::OnImageAdd(wxCommandEvent &) {
    AnimSource *a = SelectedAnim();
    if (!a) {
        return;
    }
    wxFileDialog dialog(this, "Add frame images", wxString::FromUTF8(model_.dir + "/" + model_.src_dir),
                        "", "Images (*.png;*.jpg;*.bmp;*.gif)|*.png;*.jpg;*.bmp;*.gif",
                        wxFD_OPEN | wxFD_MULTIPLE);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    wxArrayString paths;
    dialog.GetPaths(paths);
    for (const wxString &p : paths) {
        const wxFileName fn(p);
        a->images.push_back(fn.GetFullName().ToStdString());
        image_list_->Append(fn.GetFullName());
    }
    MarkDirty();
    UpdatePreview();
}

void EditorFrame::OnImageRemove(wxCommandEvent &) {
    AnimSource *a = SelectedAnim();
    const int index = image_list_->GetSelection();
    if (!a || index == wxNOT_FOUND) {
        return;
    }
    a->images.erase(a->images.begin() + index);
    image_list_->Delete(static_cast<unsigned>(index));
    MarkDirty();
    UpdatePreview();
}

void EditorFrame::OnBrowseAtlas(wxCommandEvent &) {
    wxFileDialog dialog(this, "Choose atlas sheet",
                        wxString::FromUTF8(model_.dir + "/" + model_.src_dir), "",
                        "Images (*.png;*.jpg;*.bmp;*.gif)|*.png;*.jpg;*.bmp;*.gif",
                        wxFD_OPEN);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    const wxFileName fn(dialog.GetPath());
    atlas_ctrl_->SetValue(fn.GetFullName());
    ApplyAnimationFromUI();
    UpdatePreview();
    MarkDirty();
}

void EditorFrame::OnRectPicked(int x, int y, int w, int h) {
    AnimSource *a = SelectedAnim();
    if (!a) {
        return;
    }
    a->rects.push_back(AnimRect{x, y, w, h});
    if (a->mode != "rects") {
        a->mode = "rects";
        anim_mode_ctrl_->SetStringSelection("rects");
    }
    SetStatusText(wxString::Format("Added rect %d,%d %dx%d", x, y, w, h));
    UpdatePreview();
    MarkDirty();
}

void EditorFrame::OnRoleSelected(wxCommandEvent &) {
    RefreshRoleDetail();
}

void EditorFrame::OnVariantAdd(wxCommandEvent &) {
    const std::string role = SelectedRole();
    if (role.empty()) {
        return;
    }
    auto &variants = model_.actions[role];
    RoleVariant v;
    if (!model_.animations.empty()) {
        v.animation = model_.animations.front().name;
    }
    variants.push_back(v);
    RefreshRoleDetail();
    variant_list_->SetSelection(static_cast<int>(variants.size()) - 1);
    MarkDirty();
}

void EditorFrame::OnVariantRemove(wxCommandEvent &) {
    const std::string role = SelectedRole();
    const int index = variant_list_->GetSelection();
    auto it = model_.actions.find(role);
    if (it == model_.actions.end() || index == wxNOT_FOUND) {
        return;
    }
    it->second.erase(it->second.begin() + index);
    if (it->second.empty()) {
        model_.actions.erase(it);
    }
    RefreshRoleDetail();
    MarkDirty();
}

void EditorFrame::OnVariantChanged(wxCommandEvent &) {
    if (updating_) {
        return;
    }
    ApplyVariantFromUI();
    RefreshRoleDetail();
    MarkDirty();
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void EditorFrame::OnNew(wxCommandEvent &) {
    const wxString name = wxGetTextFromUser("New character folder name:", "New Character",
                                            "character", this);
    if (name.IsEmpty()) {
        return;
    }
    wxDirDialog dialog(this, "Choose the parent folder for the new character", wxGetCwd());
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    const std::filesystem::path dir =
        std::filesystem::path(dialog.GetPath().ToStdString()) / name.ToStdString();
    std::error_code ec;
    std::filesystem::create_directories(dir / "src", ec);
    if (ec) {
        wxMessageBox("Could not create " + wxString::FromUTF8(dir.string()), "Error",
                     wxOK | wxICON_ERROR, this);
        return;
    }

    model_ = CharacterModel{};
    model_.dir = dir.string();
    model_.name = name.ToStdString();
    AnimSource idle;
    idle.name = "idle";
    idle.mode = "images";
    model_.animations.push_back(idle);
    model_.actions["idle"] = {RoleVariant{"idle", 1}};

    assets_->SetPath(wxString::FromUTF8(model_.dir + "/" + model_.src_dir));
    RefreshAll();
    MarkDirty();
    SetStatusText("New character: " + wxString::FromUTF8(model_.dir));
}

void EditorFrame::OnOpen(wxCommandEvent &) {
    wxDirDialog dialog(this, "Open a character folder", wxGetCwd());
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    std::string error;
    if (!model_.Load(dialog.GetPath().ToStdString(), &error)) {
        wxMessageBox("Could not load character:\n" + wxString::FromUTF8(error), "Error",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    assets_->SetPath(wxString::FromUTF8(model_.dir + "/" + model_.src_dir));
    RefreshAll();
    dirty_ = false;
    SetStatusText("Opened: " + wxString::FromUTF8(model_.dir));
}

void EditorFrame::OnSave(wxCommandEvent &) {
    if (model_.dir.empty()) {
        wxMessageBox("Use 'New Character' or 'Open' first.", "No character", wxOK | wxICON_INFORMATION,
                     this);
        return;
    }
    ApplyGlobalFromUI();
    std::string error;
    if (!model_.Save(&error)) {
        wxMessageBox("Could not save:\n" + wxString::FromUTF8(error), "Error", wxOK | wxICON_ERROR,
                     this);
        return;
    }
    dirty_ = false;
    SetStatusText("Saved: " + wxString::FromUTF8(model_.dir + "/sources.json"));
}

void EditorFrame::OnPack(wxCommandEvent &) {
    if (model_.dir.empty()) {
        wxMessageBox("Open a character first.", "No character", wxOK | wxICON_INFORMATION, this);
        return;
    }
    // Save first so the packer sees the latest authoring data.
    wxCommandEvent unused;
    OnSave(unused);

    const wxString script =
        wxString::FromUTF8(std::string(DVP_SOURCE_DIR) + "/tools/pack_sprites.py");
    const wxString command = "python3 \"" + script + "\" \"" + wxString::FromUTF8(model_.dir) + "\"";
    wxArrayString output;
    wxArrayString errors;
    const long pid = wxExecute(command, output, errors, wxEXEC_SYNC);
    wxString report;
    for (const wxString &line : output) {
        report += line + "\n";
    }
    for (const wxString &line : errors) {
        report += line + "\n";
    }
    if (pid < 0) {
        report = "Failed to run python3. Is Python 3 + Pillow installed?\n\n" + command;
    } else if (report.IsEmpty()) {
        report = "Packing finished with no output.";
    }
    wxMessageBox(report, "Pack Sprites", wxOK | (pid < 0 ? wxICON_ERROR : wxICON_INFORMATION),
                 this);
}

void EditorFrame::OnQuit(wxCommandEvent &) { Close(true); }

}  // namespace dvp::editor
