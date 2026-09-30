#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "character.hpp"
#include "dialogue.hpp"
#include "menu.hpp"
#include "platform_clickthrough.hpp"
#include "pet.hpp"
#include "window.hpp"
#include "text_renderer.hpp"

namespace de {

class App {
public:
    bool init(const std::string &asset_root, const std::string &character_name);
    void run();
    void shutdown();

private:
    void handle_event(const SDL_Event &event);
    void update(float dt);
    void render();
    void update_click_through(float dt);
    void run_selftest();
    void open_menu();
    void apply_sort(int id);

    std::string resolve_asset_root(const std::string &requested) const;

    Window window_;
    Character character_;
    Pet pet_;
    Dialogue dialogue_;
    Menu menu_;
    TextRenderer ui_text_;
    WindowSort sort_mode_ = WindowSort::Top;

    bool running_ = true;
    float mouse_x_ = 0.0f;
    float mouse_y_ = 0.0f;
    bool mouse_down_ = false;
    bool mouse_pressed_ = false;
    bool mouse_released_ = false;
    bool always_on_top_ = true;

    bool no_shape_ = false;
    bool selftest_ = false;
    bool force_talk_ = false;
    float exit_after_ = 0.0f;
    float exit_timer_ = 0.0f;
    float dialogue_timer_ = 0.0f;
    int rng_ = 12345;

    // Cached window-sized ARGB surface used to build the silhouette.
    SDL_Surface *shape_surface_ = nullptr;
    SDL_Rect shape_prev_{0, 0, 0, 0};
    bool shape_prev_valid_ = false;
    bool shape_menu_visible_ = false;
    bool shape_native_ = true;
    platform::InputMode input_mode_ = platform::InputMode::Native;
    float shape_timer_ = 0.0f;
    std::string shape_anim_;
    int shape_frame_ = -1;
    float shape_pet_x_ = 0.0f;
    float shape_pet_y_ = 0.0f;
};

}  // namespace de
