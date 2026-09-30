// app.hpp: application shell (init, main loop, events, render, silhouette).
#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

#include "character.hpp"
#include "dialogue.hpp"
#include "menu.hpp"
#include "pet.hpp"
#include "platform_clickthrough.hpp"
#include "text_renderer.hpp"
#include "window.hpp"

namespace dvp {

// One pet on screen: its character, DIY physics/behaviour and dialogue.
struct PetInstance {
    std::string id;  // character folder name
    Character character;
    Pet pet;
    Dialogue dialogue;
    bool active = false;   // currently shown / simulated
    bool spawned = false;  // has a position assigned

    // Per-pet silhouette cache (to know when the union mask must be rebuilt).
    std::string shape_anim;
    int shape_frame = -1;
    float shape_x = 0.0f;
    float shape_y = 0.0f;

    // Auto-dialogue scheduling.
    float dialogue_timer = 0.0f;
    unsigned dialogue_rng = 12345u;
};

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

    void build_menu();
    void open_menu();
    void apply_menu_action(int id);
    void refresh_window_title();
    void spawn_pet(PetInstance &pet, int index);
    int topmost_pet_at(float x, float y) const;
    int active_count() const;

    std::string resolve_asset_root(const std::string &requested) const;
    std::vector<std::string> scan_characters(const std::string &root) const;

    Window window_;
    Menu menu_;
    TextRenderer ui_text_;
    WindowSort sort_mode_ = WindowSort::Top;

    std::vector<std::unique_ptr<PetInstance>> pets_;
    std::string root_;
    int drag_index_ = -1;  // index of the pet currently dragged, or -1

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

    // Cached window-sized ARGB surface used to build the silhouette union.
    SDL_Surface *shape_surface_ = nullptr;
    SDL_Rect shape_prev_{0, 0, 0, 0};
    bool shape_prev_valid_ = false;
    bool shape_menu_visible_ = false;
    bool shape_native_ = true;
    platform::InputMode input_mode_ = platform::InputMode::Native;
    float shape_timer_ = 0.0f;
};

}  // namespace dvp
