#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "text_renderer.hpp"

namespace de {

struct MenuItem {
    enum class Kind { Normal, Checkable, Separator };

    std::string label;
    int id = -1;
    Kind kind = Kind::Normal;
    bool checked = false;
    bool enabled = true;
    std::vector<MenuItem> submenu;  // non-empty => opens to the right on hover
};

// A small immediate-mode-ish popup menu. One level of submenu is supported,
// which covers the original PetMenu (Exit + Force-sort submenu).
class Menu {
public:
    void set_items(std::vector<MenuItem> items);
    void set_checked(int id, bool checked);

    void open(float x, float y, float screen_w, float screen_h);
    void close();
    bool visible() const { return visible_; }
    bool contains(float x, float y) const;

    // Feed pointer motion; updates hover state.
    void on_motion(float x, float y);

    // Feed a left click. Returns the activated item id, or -1.
    int on_click(float x, float y);

    void render(SDL_Renderer *renderer, const TextRenderer &text);

    // Computes row/panel sizes from the font without drawing. Call after open().
    void refresh_layout(const TextRenderer &text);

    // Bounding rectangles of the visible menu panels (for input regions).
    std::vector<SDL_Rect> interactive_rects() const;

private:
    float item_height(const TextRenderer &text) const;
    float menu_width(const TextRenderer &text) const;
    SDL_FRect item_rect(int index) const;
    SDL_FRect submenu_item_rect(int parent_index, int child_index) const;
    void update_submenu_geometry();
    float submenu_width(const std::vector<MenuItem> &items, const TextRenderer &text) const;

    std::vector<MenuItem> items_;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float row_h_ = 26.0f;
    float pad_ = 10.0f;
    float width_ = 120.0f;
    float sub_width_ = 120.0f;
    float sub_x_ = 0.0f;
    float sub_y_ = 0.0f;
    float sub_h_ = 0.0f;
    float screen_w_ = 1920.0f;
    float screen_h_ = 1080.0f;
    bool visible_ = false;
    int hover_ = -1;
    int hover_sub_ = -1;
    int open_sub_ = -1;  // parent item whose submenu is shown
};

}  // namespace de
