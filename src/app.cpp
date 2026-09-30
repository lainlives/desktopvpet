#include "app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include "log.hpp"
#include <vector>

#include <SDL3_ttf/SDL_ttf.h>

#include "platform_clickthrough.hpp"

namespace de {

namespace {
constexpr float kShapeInterval = 0.05f;  // update silhouette at ~20 Hz
constexpr float kAutoDialogueInterval = 5.0f;

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
        DE_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetAppMetadata("DesktopEcto", "0.1.0", "dev.desktopecto");

    const std::string root = resolve_asset_root(asset_root);
    if (root.empty()) {
        DE_ERROR("could not locate the assets directory (pass it as argv[1])");
        return false;
    }
    DE_INFO("asset root: %s", root.c_str());

    WindowConfig cfg;
    cfg.title = "DesktopEcto";

    // Span the union of all displays so the pet can roam across monitors
    // (works on X11; Wayland composites a single surface per output).
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
    DE_INFO("click-through mode: %s", mode_name);

    const std::string char_dir = root + "/characters/" + character_name;
    if (!character_.load(window_.renderer(), char_dir)) {
        DE_ERROR("failed to load character from '%s'", char_dir.c_str());
        return false;
    }

    pet_.set_frame_size(character_.frame_width(), character_.frame_height());
    pet_.set_position(static_cast<float>(cfg.width) * 0.5f,
                      static_cast<float>(cfg.height) * 0.5f);

    dialogue_.set_strings(character_.dialogue_strings());
    dialogue_.set_auto_chance(6, 3.0f);

    const std::string font_path = root + "/" + character_.font();
    if (!ui_text_.load(window_.renderer(), font_path, character_.font_size())) {
        DE_WARN("UI text disabled: could not load font '%s'", font_path.c_str());
    }

    menu_.set_items({
        MenuItem{"Exit", 0, MenuItem::Kind::Normal, false, true, {}},
        MenuItem{"Force sort?", -1, MenuItem::Kind::Normal, false, true,
                 {
                     MenuItem{"Disabled", 1, MenuItem::Kind::Checkable, false, true, {}},
                     MenuItem{"Top", 2, MenuItem::Kind::Checkable, true, true, {}},
                     MenuItem{"Bottom", 3, MenuItem::Kind::Checkable, false, true, {}},
                 }},
    });
    window_.set_sorting(sort_mode_);

    no_shape_ = std::getenv("DE_NO_SHAPE") != nullptr;
    selftest_ = std::getenv("DE_SELFTEST") != nullptr;
    force_talk_ = std::getenv("DE_TALK") != nullptr;
    if (const char *exit_after = std::getenv("DE_EXIT_AFTER")) {
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
        DE_ERROR("selftest: could not create render target: %s", SDL_GetError());
        return;
    }
    SDL_SetRenderTarget(renderer, target);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    character_.render(renderer, kSize * 0.5f, kSize * 0.5f, 0.0f, false);
    dialogue_.activate();
    dialogue_.update(1.0f);  // fade in
    dialogue_.render(renderer, ui_text_, kSize * 0.5f, kSize * 0.5f + 70.0f);
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
        DE_ERROR("selftest: RenderReadPixels failed: %s", SDL_GetError());
    }
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(target);
    DE_INFO("selftest: %d opaque pixels rendered for animation '%s' (font=%s)", opaque,
            character_.current_animation().c_str(), ui_text_.valid() ? "yes" : "no");
}

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
                    apply_sort(static_cast<int>(sort_mode_) == 0 ? 2 : 1);
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
                if (character_.hit_test(mouse_x_, mouse_y_, pet_.x(), pet_.y(), pet_.angle())) {
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
                        apply_sort(id);
                    }
                } else if (character_.hit_test(mouse_x_, mouse_y_, pet_.x(), pet_.y(),
                                               pet_.angle())) {
                    mouse_down_ = true;
                    mouse_pressed_ = true;
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

void App::apply_sort(int id) {
    switch (id) {
        case 0:
            running_ = false;
            return;
        case 1:
            sort_mode_ = WindowSort::Disabled;
            break;
        case 2:
            sort_mode_ = WindowSort::Top;
            break;
        case 3:
            sort_mode_ = WindowSort::Bottom;
            break;
        default:
            return;
    }
    menu_.set_checked(1, sort_mode_ == WindowSort::Disabled);
    menu_.set_checked(2, sort_mode_ == WindowSort::Top);
    menu_.set_checked(3, sort_mode_ == WindowSort::Bottom);
    window_.set_sorting(sort_mode_);
}

void App::update(float dt) {
    if (dt <= 0.0f) {
        dt = 1.0f / 60.0f;
    }
    dt = std::min(dt, 0.05f);

    float mx = mouse_x_;
    float my = mouse_y_;
    SDL_GetMouseState(&mx, &my);

    PetInput input;
    input.mouse_x = mx;
    input.mouse_y = my;
    input.mouse_down = mouse_down_ && !menu_.visible();
    input.mouse_pressed = mouse_pressed_;
    input.mouse_released = mouse_released_;
    input.mouse_on_pet = character_.hit_test(mx, my, pet_.x(), pet_.y(), pet_.angle());

    pet_.update(dt, static_cast<float>(window_.width()),
                static_cast<float>(window_.height()), input);

    mouse_pressed_ = false;
    mouse_released_ = false;

    if (const char *forced = std::getenv("DE_ANIM")) {
        if (character_.has_animation(forced)) {
            character_.play(forced);
        }
    } else {
        AnimRole role = role_for_pet(pet_);
        if (dialogue_.alpha() > 0.01f && pet_.state() != PetState::Dragged &&
            character_.has_role(AnimRole::Talk)) {
            role = AnimRole::Talk;
        }
        character_.play_role(role);
    }
    character_.update(dt);

    if (force_talk_ && dialogue_.alpha() <= 0.01f) {
        dialogue_.activate();
    }
    dialogue_.update(dt);
    dialogue_timer_ += dt;
    if (dialogue_timer_ > kAutoDialogueInterval) {
        dialogue_timer_ = 0.0f;
        if (!dialogue_.active() && !dialogue_.message().empty()) {
            rng_ = rng_ * 1103515245 + 12345;
            if ((rng_ & 0x7fffffff) % 6 == 0) {
                dialogue_.activate();
            }
        }
    }

    update_click_through(dt);

    if (exit_after_ > 0.0f) {
        exit_timer_ += dt;
        if (exit_timer_ >= exit_after_) {
            running_ = false;
        }
    }
}

void App::render() {
    SDL_Renderer *renderer = window_.renderer();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    character_.render(renderer, pet_.x(), pet_.y(), pet_.angle(), pet_.facing_left());

    const float bubble_bottom = pet_.y() - character_.frame_height() * 0.5f - 6.0f;
    dialogue_.render(renderer, ui_text_, pet_.x(), bubble_bottom);
    menu_.render(renderer, ui_text_);

    SDL_RenderPresent(renderer);
}

void App::update_click_through(float dt) {
    if (no_shape_ || input_mode_ == platform::InputMode::None) {
        return;
    }
    shape_timer_ += dt;

    SDL_Rect frame_rect = character_.current_frame_rect();
    const int frame = character_.current_frame_index();
    const bool anim_changed = character_.current_animation() != shape_anim_;
    const bool frame_changed = frame != shape_frame_;
    const bool moved = std::fabs(pet_.x() - shape_pet_x_) > 0.5f ||
                       std::fabs(pet_.y() - shape_pet_y_) > 0.5f;
    // While the menu is open (or just closed) its input rects must stay in
    // sync, including when the hovered submenu changes.
    const bool menu_dirty = menu_.visible() || shape_menu_visible_;

    if (!anim_changed && !frame_changed && !moved && !menu_dirty) {
        return;
    }
    // Animation/frame/menu changes update immediately; pure movement is
    // throttled, unless the platform uses the (expensive, whole-output)
    // renderer shape mask, in which case we must update every frame to avoid
    // clipping the moving sprite.
    const float interval = shape_native_ ? kShapeInterval : 0.0f;
    if (!anim_changed && !frame_changed && !menu_dirty && shape_timer_ < interval) {
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

    SDL_Surface *src = character_.current_source_surface();
    if (!src) {
        return;
    }

    SDL_Rect dst{static_cast<int>(pet_.x() - character_.frame_width() * 0.5f),
                 static_cast<int>(pet_.y() - character_.frame_height() * 0.5f),
                 frame_rect.w, frame_rect.h};
    SDL_SetSurfaceBlendMode(shape_surface_, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(src, &frame_rect, shape_surface_, &dst);

    SDL_Rect content = dst;

    // Composite UI panels into the silhouette so they are visible and
    // interactive on platforms where SDL masks the whole output by the shape
    // (Wayland/macOS), and included in the native input region (X11/Win32).
    auto add_rect = [&](SDL_FRect r) {
        SDL_Rect ir{static_cast<int>(r.x), static_cast<int>(r.y),
                    static_cast<int>(r.w + 0.5f), static_cast<int>(r.h + 0.5f)};
        SDL_FillSurfaceRect(shape_surface_, &ir, 0xFFFFFFFFu);
        const int x0 = std::min(content.x, ir.x);
        const int y0 = std::min(content.y, ir.y);
        const int x1 = std::max(content.x + content.w, ir.x + ir.w);
        const int y1 = std::max(content.y + content.h, ir.y + ir.h);
        content = SDL_Rect{x0, y0, x1 - x0, y1 - y0};
    };

    const float bubble_bottom = pet_.y() - character_.frame_height() * 0.5f - 6.0f;
    if (dialogue_.alpha() > 0.01f) {
        add_rect(dialogue_.bubble_rect(pet_.x(), bubble_bottom));
    }
    if (menu_.visible()) {
        for (const SDL_Rect &r : menu_.interactive_rects()) {
            add_rect(SDL_FRect{static_cast<float>(r.x), static_cast<float>(r.y),
                               static_cast<float>(r.w), static_cast<float>(r.h)});
        }
    }

    shape_native_ = platform::apply_window_shape(window_.handle(), shape_surface_, content);
    if (std::getenv("DE_DEBUG_SHAPE")) {
        DE_INFO("shape: anim=%s frame=%d native=%d content=%d,%d %dx%d",
                character_.current_animation().c_str(), frame, shape_native_ ? 1 : 0,
                content.x, content.y, content.w, content.h);
    }
    shape_prev_ = content;
    shape_prev_valid_ = true;

    shape_anim_ = character_.current_animation();
    shape_frame_ = frame;
    shape_pet_x_ = pet_.x();
    shape_pet_y_ = pet_.y();
}

void App::shutdown() {
    platform::clear_window_shape(window_.handle());
    if (shape_surface_) {
        SDL_DestroySurface(shape_surface_);
        shape_surface_ = nullptr;
    }
    // Textures must be released while the renderer is still alive.
    ui_text_.unload();
    character_.unload();
    TTF_Quit();
    window_.destroy();
    SDL_Quit();
}

}  // namespace de
