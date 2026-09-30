// app.cpp: wires window, characters, pets, dialogues and the menu together.
#include "app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include <SDL3_ttf/SDL_ttf.h>

#include "log.hpp"
#include "platform_clickthrough.hpp"

namespace dvp {

namespace {
constexpr float kShapeInterval = 0.05f;      // silhouette refresh (~20 Hz) in native modes
constexpr float kAutoDialogueInterval = 5.0f;
constexpr float kPetSpacing = 150.0f;        // horizontal spawn spacing
constexpr int kCharacterMenuBase = 100;      // menu id of character 0 (100, 101, ...)

// Maps the pet's behaviour state to a semantic animation role.
AnimRole role_for_pet(const Pet &pet) {
    if (pet.impact_active()) {
        return AnimRole::FallImpact;
    }
    switch (pet.state()) {
        case PetState::Walking:    return AnimRole::Walk;
        case PetState::Spinny:     return AnimRole::Spinny;
        case PetState::DancyDance: return AnimRole::Dance;
        case PetState::Hovered:
        case PetState::Dragged:    return AnimRole::Clicked;
        case PetState::Idle:
        default:                   return AnimRole::Idle;
    }
}
}  // namespace

bool App::init(const std::string &asset_root, const std::string &character_name) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        DVP_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetAppMetadata("DesktopVPet", "0.1.0", "dev.desktopvpet");

    root_ = resolve_asset_root(asset_root);
    if (root_.empty()) {
        DVP_ERROR("could not locate the assets directory (pass it as argv[1])");
        return false;
    }
    DVP_INFO("asset root: %s", root_.c_str());

    WindowConfig cfg;
    cfg.title = "DesktopVPet";

    // Span the union of all displays so pets can roam across monitors (works on
    // X11; Wayland composites a single surface per output).
    SDL_Rect bounds{0, 0, 1280, 720};
    int display_count = 0;
    if (SDL_DisplayID *displays = SDL_GetDisplays(&display_count)) {
        bool first = true;
        for (int i = 0; i < display_count; ++i) {
            SDL_Rect b{0, 0, 0, 0};
            if (!SDL_GetDisplayBounds(displays[i], &b)) {
                continue;
            }
            if (first) {
                bounds = b;
                first = false;
            } else {
                const int x0 = std::min(bounds.x, b.x);
                const int y0 = std::min(bounds.y, b.y);
                const int x1 = std::max(bounds.x + bounds.w, b.x + b.w);
                const int y1 = std::max(bounds.y + bounds.h, b.y + b.h);
                bounds = SDL_Rect{x0, y0, x1 - x0, y1 - y0};
            }
        }
        SDL_free(displays);
    }
    cfg.width = bounds.w;
    cfg.height = bounds.h;
    cfg.maximized = false;

    if (!window_.create(cfg)) {
        return false;
    }
    if (bounds.x != 0 || bounds.y != 0) {
        SDL_SetWindowPosition(window_.handle(), bounds.x, bounds.y);
    }

    input_mode_ = platform::detect_input_mode(window_.handle());
    shape_native_ = input_mode_ == platform::InputMode::Native ||
                    input_mode_ == platform::InputMode::Wayland;
    const char *mode_name = "transparent window (none)";
    if (input_mode_ == platform::InputMode::Native) {
        mode_name = "native input region";
    } else if (input_mode_ == platform::InputMode::Wayland) {
        mode_name = "wayland input region";
    } else if (input_mode_ == platform::InputMode::ShapeMask) {
        mode_name = "renderer shape mask";
    }
    DVP_INFO("click-through mode: %s", mode_name);

    // --- Discover and load characters ------------------------------------
    const std::vector<std::string> names = scan_characters(root_);
    if (names.empty()) {
        DVP_ERROR("no character folders found under '%s/characters'", root_.c_str());
        return false;
    }

    std::string initial = character_name;
    if (initial.empty() || std::find(names.begin(), names.end(), initial) == names.end()) {
        if (!character_name.empty()) {
            DVP_WARN("character '%s' not found; using '%s'", character_name.c_str(),
                     names.front().c_str());
        }
        initial = names.front();
    }

    for (const std::string &name : names) {
        auto instance = std::make_unique<PetInstance>();
        instance->id = name;
        const std::string dir = root_ + "/characters/" + name;
        if (!instance->character.load(window_.renderer(), dir)) {
            DVP_WARN("skipping character '%s' (load failed)", name.c_str());
            continue;
        }
        instance->dialogue.set_strings(instance->character.dialogue_strings());
        instance->dialogue.set_auto_chance(6, 3.0f);
        instance->active = (name == initial);
        pets_.push_back(std::move(instance));
    }
    if (pets_.empty()) {
        DVP_ERROR("no loadable characters under '%s/characters'", root_.c_str());
        return false;
    }

    // Shared UI font, taken from the first loaded character.
    const std::string font_path = root_ + "/" + pets_.front()->character.font();
    if (!ui_text_.load(window_.renderer(), font_path, pets_.front()->character.font_size())) {
        DVP_WARN("UI text disabled: could not load font '%s'", font_path.c_str());
    }

    // DVP_ALL=1 activates every detected character (useful for testing).
    if (std::getenv("DVP_ALL")) {
        for (auto &pet : pets_) {
            pet->active = true;
        }
    }
    for (std::size_t i = 0; i < pets_.size(); ++i) {
        if (pets_[i]->active) {
            spawn_pet(*pets_[i], static_cast<int>(i));
        }
    }

    build_menu();
    window_.set_sorting(sort_mode_);
    refresh_window_title();
    DVP_INFO("loaded %zu character(s)", pets_.size());

    no_shape_ = std::getenv("DVP_NO_SHAPE") != nullptr;
    selftest_ = std::getenv("DVP_SELFTEST") != nullptr;
    force_talk_ = std::getenv("DVP_TALK") != nullptr;
    if (const char *exit_after = std::getenv("DVP_EXIT_AFTER")) {
        exit_after_ = std::strtof(exit_after, nullptr);
    }

    return true;
}

std::string App::resolve_asset_root(const std::string &requested) const {
    std::vector<std::string> candidates;
    if (!requested.empty()) {
        candidates.push_back(requested);
    }
    if (const char *base = SDL_GetBasePath()) {
        // SDL_GetBasePath returns a cached, SDL-owned string: do NOT free it.
        candidates.emplace_back(std::string(base) + "assets");
    }
    candidates.emplace_back("assets");
    candidates.emplace_back("../assets");

    for (const std::string &candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::is_directory(candidate + "/characters", ec)) {
            return candidate;
        }
    }
    return {};
}

std::vector<std::string> App::scan_characters(const std::string &root) const {
    std::vector<std::string> names;
    std::error_code ec;
    const std::string dir = root + "/characters";
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
        std::error_code is_dir_ec;
        if (!entry.is_directory(is_dir_ec)) {
            continue;
        }
        std::error_code exists_ec;
        if (std::filesystem::exists(entry.path() / "character.json", exists_ec)) {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

void App::spawn_pet(PetInstance &pet, int index) {
    pet.pet.set_frame_size(pet.character.frame_width(), pet.character.frame_height());
    const float n = static_cast<float>(pets_.size());
    const float cx = window_.width() * 0.5f +
                     (static_cast<float>(index) - (n - 1.0f) * 0.5f) * kPetSpacing;
    const float cy = window_.height() * 0.45f;
    pet.pet.set_position(std::clamp(cx, 80.0f, window_.width() - 80.0f), cy);
    pet.spawned = true;
}

void App::build_menu() {
    std::vector<MenuItem> items;
    items.push_back(MenuItem{"Exit", 0, MenuItem::Kind::Normal, false, true, {}});

    // Only offer character selection when more than one character exists.
    if (pets_.size() > 1) {
        MenuItem characters;
        characters.label = "Characters";
        characters.id = -1;
        for (std::size_t i = 0; i < pets_.size(); ++i) {
            characters.submenu.push_back(MenuItem{
                pets_[i]->character.name(), kCharacterMenuBase + static_cast<int>(i),
                MenuItem::Kind::Checkable, pets_[i]->active, true, {}});
        }
        items.push_back(std::move(characters));
    }

    items.push_back(MenuItem{
        "Force sort?", -1, MenuItem::Kind::Normal, false, true,
        {
            MenuItem{"Disabled", 1, MenuItem::Kind::Checkable,
                     sort_mode_ == WindowSort::Disabled, true, {}},
            MenuItem{"Top", 2, MenuItem::Kind::Checkable, sort_mode_ == WindowSort::Top, true, {}},
            MenuItem{"Bottom", 3, MenuItem::Kind::Checkable,
                     sort_mode_ == WindowSort::Bottom, true, {}},
        }});
    menu_.set_items(std::move(items));
}

void App::refresh_window_title() {
    std::vector<std::string> active;
    for (const auto &pet : pets_) {
        if (pet->active) {
            active.push_back(pet->character.name());
        }
    }
    std::string title = "DesktopVPet";
    if (active.size() == 1) {
        title = "Desktop " + active.front();
    } else if (!active.empty()) {
        title = "Desktop " + active.front();
        for (std::size_t i = 1; i < active.size(); ++i) {
            title += ", " + active[i];
        }
    }
    SDL_SetWindowTitle(window_.handle(), title.c_str());
}

int App::active_count() const {
    int count = 0;
    for (const auto &pet : pets_) {
        if (pet->active) {
            ++count;
        }
    }
    return count;
}

int App::topmost_pet_at(float x, float y) const {
    // Iterate back-to-front: the last active pet is drawn on top.
    for (int i = static_cast<int>(pets_.size()) - 1; i >= 0; --i) {
        const PetInstance &p = *pets_[i];
        if (!p.active) {
            continue;
        }
        if (p.character.hit_test(x, y, p.pet.x(), p.pet.y(), p.pet.angle())) {
            return i;
        }
    }
    return -1;
}

void App::run() {
    if (selftest_) {
        run_selftest();
        return;
    }

    Uint64 last = SDL_GetTicksNS();
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handle_event(event);
        }

        const Uint64 now = SDL_GetTicksNS();
        float dt = static_cast<float>(now - last) / 1.0e9f;
        last = now;
        update(dt);
        render();
    }
}

void App::run_selftest() {
    SDL_Renderer *renderer = window_.renderer();
    constexpr int kSize = 256;
    SDL_Texture *target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_TARGET, kSize, kSize);
    if (!target) {
        DVP_ERROR("selftest: could not create render target: %s", SDL_GetError());
        return;
    }
    SDL_SetRenderTarget(renderer, target);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    int drawn = 0;
    for (const auto &pet : pets_) {
        if (!pet->active) {
            continue;
        }
        const float x = kSize * 0.5f + (drawn - 0.5f) * 96.0f;
        pet->character.render(renderer, x, kSize * 0.5f, 0.0f, false);
        ++drawn;
    }
    if (!pets_.empty()) {
        PetInstance &first = *pets_.front();
        first.dialogue.activate();
        first.dialogue.update(1.0f);  // fade in
        first.dialogue.render(renderer, ui_text_, kSize * 0.5f, kSize * 0.5f + 70.0f);
    }
    menu_.refresh_layout(ui_text_);
    menu_.open(static_cast<float>(kSize - 2), static_cast<float>(kSize - 2),
               static_cast<float>(kSize), static_cast<float>(kSize));
    menu_.on_motion(static_cast<float>(kSize - 4), static_cast<float>(kSize - 4) - 24.0f);
    menu_.render(renderer, ui_text_);

    int opaque = 0;
    SDL_Surface *shot = SDL_RenderReadPixels(renderer, nullptr);
    if (shot) {
        SDL_Surface *argb = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(shot);
        if (argb) {
            const auto *pixels = static_cast<const Uint8 *>(argb->pixels);
            for (int y = 0; y < argb->h; ++y) {
                const Uint8 *row = pixels + static_cast<std::size_t>(y) * argb->pitch;
                for (int x = 0; x < argb->w; ++x) {
                    if (row[x * 4 + 3] > 0) {
                        ++opaque;
                    }
                }
            }
            SDL_DestroySurface(argb);
        }
    } else {
        DVP_ERROR("selftest: RenderReadPixels failed: %s", SDL_GetError());
    }
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(target);
    DVP_INFO("selftest: %d opaque pixels, %d character(s), font=%s", opaque, drawn,
             ui_text_.valid() ? "yes" : "no");
}

// Routes SDL events: quit/keys, mouse (menu first, then pet drag), resizes.
void App::handle_event(const SDL_Event &event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
            running_ = false;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    if (menu_.visible()) {
                        menu_.close();
                    } else {
                        running_ = false;
                    }
                } else if (event.key.scancode == SDL_SCANCODE_T) {
                    apply_menu_action(static_cast<int>(sort_mode_) == 0 ? 2 : 1);
                }
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            mouse_x_ = event.motion.x;
            mouse_y_ = event.motion.y;
            if (menu_.visible()) {
                menu_.on_motion(mouse_x_, mouse_y_);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_RIGHT) {
                mouse_x_ = event.button.x;
                mouse_y_ = event.button.y;
                if (topmost_pet_at(mouse_x_, mouse_y_) >= 0) {
                    open_menu();
                } else if (menu_.visible()) {
                    menu_.close();
                }
            } else if (event.button.button == SDL_BUTTON_LEFT) {
                mouse_x_ = event.button.x;
                mouse_y_ = event.button.y;
                if (menu_.visible()) {
                    const int id = menu_.on_click(mouse_x_, mouse_y_);
                    if (id >= 0) {
                        apply_menu_action(id);
                    }
                } else {
                    const int index = topmost_pet_at(mouse_x_, mouse_y_);
                    if (index >= 0) {
                        drag_index_ = index;
                        mouse_down_ = true;
                        mouse_pressed_ = true;
                    }
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                mouse_down_ = false;
                mouse_released_ = true;
            }
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED:
            if (shape_surface_) {
                SDL_DestroySurface(shape_surface_);
                shape_surface_ = nullptr;
                shape_prev_valid_ = false;
            }
            break;
        default:
            break;
    }
}

void App::open_menu() {
    menu_.refresh_layout(ui_text_);
    menu_.open(mouse_x_, mouse_y_, static_cast<float>(window_.width()),
               static_cast<float>(window_.height()));
}

void App::apply_menu_action(int id) {
    // Exit.
    if (id == 0) {
        running_ = false;
        return;
    }
    // Force sort (one-hot).
    if (id >= 1 && id <= 3) {
        switch (id) {
            case 1: sort_mode_ = WindowSort::Disabled; break;
            case 2: sort_mode_ = WindowSort::Top; break;
            case 3: sort_mode_ = WindowSort::Bottom; break;
            default: return;
        }
        menu_.set_checked(1, sort_mode_ == WindowSort::Disabled);
        menu_.set_checked(2, sort_mode_ == WindowSort::Top);
        menu_.set_checked(3, sort_mode_ == WindowSort::Bottom);
        window_.set_sorting(sort_mode_);
        return;
    }
    // Character toggles (multi-select).
    if (id >= kCharacterMenuBase) {
        const int index = id - kCharacterMenuBase;
        if (index < 0 || index >= static_cast<int>(pets_.size())) {
            return;
        }
        PetInstance &pet = *pets_[index];
        if (pet.active) {
            if (active_count() <= 1) {
                menu_.set_checked(id, true);  // always keep at least one pet
                return;
            }
            pet.active = false;
        } else {
            pet.active = true;
            if (!pet.spawned) {
                spawn_pet(pet, index);
            }
        }
        menu_.set_checked(id, pet.active);
        refresh_window_title();
    }
}

// One simulation step: input, pet physics, animation roles, dialogue, silhouette.
void App::update(float dt) {
    if (dt <= 0.0f) {
        dt = 1.0f / 60.0f;
    }
    dt = std::min(dt, 0.05f);

    float mx = mouse_x_;
    float my = mouse_y_;
    SDL_GetMouseState(&mx, &my);

    const int hover_index = menu_.visible() ? -1 : topmost_pet_at(mx, my);
    const bool menu_open = menu_.visible();

    for (std::size_t i = 0; i < pets_.size(); ++i) {
        PetInstance &p = *pets_[i];
        if (!p.active) {
            continue;
        }
        const int index = static_cast<int>(i);

        PetInput input;
        input.mouse_x = mx;
        input.mouse_y = my;
        input.mouse_on_pet = (index == hover_index);
        input.mouse_down = !menu_open && (drag_index_ == index || drag_index_ == -1);
        input.mouse_pressed = !menu_open && (index == hover_index) && mouse_pressed_;
        input.mouse_released = (drag_index_ == index) && mouse_released_;

        p.pet.update(dt, static_cast<float>(window_.width()),
                     static_cast<float>(window_.height()), input);

        // Animation role (with a talk override while a bubble is visible).
        if (const char *forced = std::getenv("DVP_ANIM")) {
            if (p.character.has_animation(forced)) {
                p.character.play(forced);
            }
        } else {
            AnimRole role = role_for_pet(p.pet);
            if (p.dialogue.alpha() > 0.01f && p.pet.state() != PetState::Dragged &&
                p.character.has_role(AnimRole::Talk)) {
                role = AnimRole::Talk;
            }
            p.character.play_role(role);
        }
        p.character.update(dt);

        // Per-pet dialogue scheduling.
        if (force_talk_ && p.dialogue.alpha() <= 0.01f) {
            p.dialogue.activate();
        }
        p.dialogue.update(dt);
        p.dialogue_timer += dt;
        if (p.dialogue_timer > kAutoDialogueInterval) {
            p.dialogue_timer = 0.0f;
            if (!p.dialogue.active() && !p.dialogue.message().empty()) {
                p.dialogue_rng = p.dialogue_rng * 1103515245u + 12345u;
                if ((p.dialogue_rng & 0x7fffffffu) % 6 == 0) {
                    p.dialogue.activate();
                }
            }
        }
    }

    if (mouse_released_) {
        drag_index_ = -1;
    }
    mouse_pressed_ = false;
    mouse_released_ = false;

    update_click_through(dt);

    if (exit_after_ > 0.0f) {
        exit_timer_ += dt;
        if (exit_timer_ >= exit_after_) {
            running_ = false;
        }
    }
}

// Clears to transparent, draws every active pet + dialogue, then the menu.
void App::render() {
    SDL_Renderer *renderer = window_.renderer();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    for (const auto &pet : pets_) {
        if (!pet->active) {
            continue;
        }
        const Pet &p = pet->pet;
        const Character &c = pet->character;
        c.render(renderer, p.x(), p.y(), p.angle(), p.facing_left());
        const float bubble_bottom = p.y() - c.frame_height() * 0.5f - 6.0f;
        pet->dialogue.render(renderer, ui_text_, p.x(), bubble_bottom);
    }
    menu_.render(renderer, ui_text_);
    SDL_RenderPresent(renderer);
}

void App::update_click_through(float dt) {
    if (no_shape_ || input_mode_ == platform::InputMode::None) {
        return;
    }
    shape_timer_ += dt;

    // Any active pet moving/animating, or the menu, dirties the union mask.
    bool pet_dirty = false;
    for (const auto &pet : pets_) {
        if (!pet->active) {
            continue;
        }
        const int frame = pet->character.current_frame_index();
        if (pet->character.current_animation() != pet->shape_anim || frame != pet->shape_frame ||
            std::fabs(pet->pet.x() - pet->shape_x) > 0.5f ||
            std::fabs(pet->pet.y() - pet->shape_y) > 0.5f) {
            pet_dirty = true;
            break;
        }
    }
    const bool menu_dirty = menu_.visible() || shape_menu_visible_;

    if (!pet_dirty && !menu_dirty) {
        return;
    }
    // Menu changes update immediately; pet movement/anim is throttled in native
    // modes (or every frame when the whole-output renderer mask is in use).
    const float interval = shape_native_ ? kShapeInterval : 0.0f;
    if (!menu_dirty && shape_timer_ < interval) {
        return;
    }
    shape_timer_ = 0.0f;
    shape_menu_visible_ = menu_.visible();

    const int w = window_.width();
    const int h = window_.height();
    if (w <= 0 || h <= 0) {
        return;
    }
    if (!shape_surface_ || shape_surface_->w != w || shape_surface_->h != h) {
        if (shape_surface_) {
            SDL_DestroySurface(shape_surface_);
        }
        shape_surface_ = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB32);
        shape_prev_valid_ = false;
    }
    if (!shape_surface_) {
        return;
    }

    if (shape_prev_valid_) {
        SDL_SetSurfaceBlendMode(shape_surface_, SDL_BLENDMODE_NONE);
        SDL_FillSurfaceRect(shape_surface_, &shape_prev_, 0);
        shape_prev_valid_ = false;
    }
    SDL_SetSurfaceBlendMode(shape_surface_, SDL_BLENDMODE_NONE);

    SDL_Rect content{0, 0, 0, 0};
    bool have_content = false;
    auto union_rect = [&](const SDL_Rect &r) {
        if (!have_content) {
            content = r;
            have_content = true;
            return;
        }
        const int x0 = std::min(content.x, r.x);
        const int y0 = std::min(content.y, r.y);
        const int x1 = std::max(content.x + content.w, r.x + r.w);
        const int y1 = std::max(content.y + content.h, r.y + r.h);
        content = SDL_Rect{x0, y0, x1 - x0, y1 - y0};
    };

    // Blit each active pet's current frame into the silhouette.
    for (std::size_t i = 0; i < pets_.size(); ++i) {
        PetInstance &pet = *pets_[i];
        if (!pet.active) {
            continue;
        }
        SDL_Surface *src = pet.character.current_source_surface();
        if (!src) {
            continue;
        }
        SDL_Rect frame_rect = pet.character.current_frame_rect();
        SDL_Rect dst{static_cast<int>(pet.pet.x() - pet.character.frame_width() * 0.5f),
                     static_cast<int>(pet.pet.y() - pet.character.frame_height() * 0.5f),
                     frame_rect.w, frame_rect.h};
        SDL_BlitSurface(src, &frame_rect, shape_surface_, &dst);
        union_rect(dst);

        if (pet.dialogue.alpha() > 0.01f) {
            const SDL_FRect bubble = pet.dialogue.bubble_rect(
                pet.pet.x(), pet.pet.y() - pet.character.frame_height() * 0.5f - 6.0f);
            SDL_Rect ir{static_cast<int>(bubble.x), static_cast<int>(bubble.y),
                        static_cast<int>(bubble.w + 0.5f), static_cast<int>(bubble.h + 0.5f)};
            SDL_FillSurfaceRect(shape_surface_, &ir, 0xFFFFFFFFu);
            union_rect(ir);
        }

        pet.shape_anim = pet.character.current_animation();
        pet.shape_frame = pet.character.current_frame_index();
        pet.shape_x = pet.pet.x();
        pet.shape_y = pet.pet.y();
    }

    // Menu panels are always interactive.
    if (menu_.visible()) {
        for (const SDL_Rect &r : menu_.interactive_rects()) {
            SDL_FillSurfaceRect(shape_surface_, &r, 0xFFFFFFFFu);
            union_rect(r);
        }
    }

    if (!have_content) {
        return;
    }
    shape_native_ = platform::apply_window_shape(window_.handle(), shape_surface_, content);
    shape_prev_ = content;
    shape_prev_valid_ = true;
}

void App::shutdown() {
    platform::clear_window_shape(window_.handle());
    if (shape_surface_) {
        SDL_DestroySurface(shape_surface_);
        shape_surface_ = nullptr;
    }
    // Textures must be released while the renderer is still alive.
    ui_text_.unload();
    for (auto &pet : pets_) {
        pet->character.unload();
    }
    pets_.clear();
    TTF_Quit();
    window_.destroy();
    SDL_Quit();
}

}  // namespace dvp
