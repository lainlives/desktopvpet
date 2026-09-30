// editor_frame.hpp: the character editor window (toolbar + 3 panes).
#pragma once

#include <wx/wx.h>

#include <wx/choice.h>
#include <wx/dirctrl.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/textctrl.h>

#include "character_model.hpp"
#include "preview_panel.hpp"

namespace dvp::editor {

class EditorFrame : public wxFrame {
public:
    EditorFrame();

private:
    // Toolbar / file menu.
    void OnNew(wxCommandEvent &event);
    void OnOpen(wxCommandEvent &event);
    void OnSave(wxCommandEvent &event);
    void OnPack(wxCommandEvent &event);
    void OnQuit(wxCommandEvent &event);

    // Global attributes.
    void OnGlobalChanged(wxCommandEvent &event);
    void OnDialogueAdd(wxCommandEvent &event);
    void OnDialogueRemove(wxCommandEvent &event);

    // Animations.
    void OnAnimationSelected(wxCommandEvent &event);
    void OnAnimationAdd(wxCommandEvent &event);
    void OnAnimationRemove(wxCommandEvent &event);
    void OnAnimationChanged(wxCommandEvent &event);
    void OnImageAdd(wxCommandEvent &event);
    void OnImageRemove(wxCommandEvent &event);
    void OnBrowseAtlas(wxCommandEvent &event);
    void OnRectPicked(int x, int y, int w, int h);

    // Roles / actions.
    void OnRoleSelected(wxCommandEvent &event);
    void OnVariantAdd(wxCommandEvent &event);
    void OnVariantRemove(wxCommandEvent &event);
    void OnVariantChanged(wxCommandEvent &event);

    // Helpers.
    AnimSource *SelectedAnim();
    std::string SelectedRole();
    void ApplyGlobalFromUI();
    void ApplyAnimationFromUI();
    void ApplyVariantFromUI();
    void RefreshAll();
    void RefreshGlobal();
    void RefreshAnimations();
    void RefreshAnimationDetail();
    void RefreshRoles();
    void RefreshRoleDetail();
    void UpdatePreview();
    void MarkDirty();

    wxPanel *BuildGlobalPanel(wxWindow *parent);
    wxPanel *BuildCenterPanel(wxWindow *parent);
    wxPanel *BuildRightPanel(wxWindow *parent);

    CharacterModel model_;
    bool dirty_ = false;
    bool updating_ = false;  // suppress change events while refreshing the UI

    // Global.
    wxTextCtrl *name_ctrl_ = nullptr;
    wxSpinCtrl *frame_w_ctrl_ = nullptr;
    wxSpinCtrl *frame_h_ctrl_ = nullptr;
    wxTextCtrl *font_ctrl_ = nullptr;
    wxSpinCtrlDouble *font_size_ctrl_ = nullptr;
    wxListBox *dialogue_list_ = nullptr;

    // Animations.
    wxListBox *anim_list_ = nullptr;
    wxSpinCtrlDouble *anim_fps_ctrl_ = nullptr;
    wxCheckBox *anim_loop_ctrl_ = nullptr;
    wxChoice *anim_mode_ctrl_ = nullptr;
    wxListBox *image_list_ = nullptr;
    wxTextCtrl *atlas_ctrl_ = nullptr;
    wxSpinCtrl *cols_ctrl_ = nullptr;
    wxSpinCtrl *frames_ctrl_ = nullptr;
    wxSpinCtrl *start_ctrl_ = nullptr;

    // Roles.
    wxListBox *role_list_ = nullptr;
    wxListBox *variant_list_ = nullptr;
    wxChoice *variant_anim_ctrl_ = nullptr;
    wxSpinCtrl *variant_weight_ctrl_ = nullptr;

    // Preview / assets.
    PreviewPanel *preview_ = nullptr;
    wxGenericDirCtrl *assets_ = nullptr;

    enum {
        ID_PACK = wxID_HIGHEST + 100,
        ID_ANIM_ADD,
        ID_ANIM_REMOVE,
        ID_IMAGE_ADD,
        ID_IMAGE_REMOVE,
        ID_BROWSE_ATLAS,
        ID_VARIANT_ADD,
        ID_VARIANT_REMOVE,
        ID_DIALOGUE_ADD,
        ID_DIALOGUE_REMOVE
    };
};

}  // namespace dvp::editor
