#include "dialogue.hpp"

namespace de {

void Dialogue::set_strings(std::vector<std::string> strings) {
    strings_ = std::move(strings);
}

void Dialogue::set_auto_chance(int one_in_n, float cooldown_seconds) {
    auto_chance_ = one_in_n > 0 ? one_in_n : 1;
    cooldown_ = cooldown_seconds;
}

void Dialogue::activate() {
    if (strings_.empty()) {
        return;
    }
    std::uniform_int_distribution<std::size_t> pick(0, strings_.size() - 1);
    current_ = strings_[pick(rng_)];
    fade_ = Fade::In;
    text_timer_ = 0.0f;
    active_ = true;
    cooldown_remaining_ = cooldown_;
}

void Dialogue::update(float dt) {
    if (cooldown_remaining_ > 0.0f) {
        cooldown_remaining_ -= dt;
    }

    switch (fade_) {
        case Fade::In:
            alpha_ += dt * 3.0f;
            if (alpha_ >= 1.0f) {
                alpha_ = 1.0f;
                fade_ = Fade::None;
            }
            break;
        case Fade::Out:
            alpha_ -= dt * 3.0f;
            if (alpha_ <= 0.0f) {
                alpha_ = 0.0f;
                fade_ = Fade::None;
            }
            break;
        case Fade::None:
            break;
    }

    if (text_timer_ != -1.0f) {
        text_timer_ += dt;
        if (text_timer_ > 3.0f) {
            fade_ = Fade::Out;
            text_timer_ = -1.0f;
            active_ = false;
        }
    }
}

SDL_FRect Dialogue::bubble_rect(float cx, float bottom_y) const {
    const float w = 240.0f;
    const float h = 64.0f;
    return SDL_FRect{cx - w * 0.5f, bottom_y - h, w, h};
}

void Dialogue::render(SDL_Renderer *renderer, const TextRenderer &text, float cx,
                      float bottom_y) const {
    if (alpha_ <= 0.001f) {
        return;
    }

    const float w = 240.0f;
    const float h = 64.0f;
    const float x = cx - w * 0.5f;
    const float y = bottom_y - h;

    const Uint8 a = static_cast<Uint8>(alpha_ * 158.0f);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Body.
    SDL_SetRenderDrawColor(renderer, 45, 45, 45, a);
    const SDL_FRect body{x, y, w, h};
    SDL_RenderFillRect(renderer, &body);

    // Border (approximated with a second, inset fill in the accent colour).
    SDL_SetRenderDrawColor(renderer, 106, 161, 138, static_cast<Uint8>(alpha_ * 160.0f));
    const SDL_FRect border{x + 3, y + 3, w - 6, h - 6};
    SDL_RenderFillRect(renderer, &border);
    SDL_SetRenderDrawColor(renderer, 45, 45, 45, a);
    const SDL_FRect inner{x + 5, y + 5, w - 10, h - 10};
    SDL_RenderFillRect(renderer, &inner);

    const SDL_FRect text_rect{x + 10.0f, y + 8.0f, w - 20.0f, h - 16.0f};
    const SDL_Color text_color{235, 235, 235, static_cast<Uint8>(alpha_ * 255.0f)};
    text.draw(renderer, current_, text_rect, text_color);
}

}  // namespace de
