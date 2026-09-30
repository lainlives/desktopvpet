// dialogue.hpp: speech-bubble fade state and panel drawing.
#pragma once

#include <SDL3/SDL.h>

#include <random>
#include <string>
#include <vector>

#include "text_renderer.hpp"

namespace dvp {

// Speech-bubble state machine ported from DialogueBox.cs: fade in, hold ~3s,
// fade out. Text rendering is intentionally abstracted; the panel is drawn
// here and a text backend can be plugged in later.
class Dialogue {
public:
    void set_strings(std::vector<std::string> strings);
    void set_auto_chance(int one_in_n, float cooldown_seconds);

    void activate();
    void update(float dt);

    bool active() const { return active_; }
    float alpha() const { return alpha_; }
    const std::string &message() const { return current_; }

    // Screen-space rectangle of the bubble for a given anchor.
    SDL_FRect bubble_rect(float cx, float bottom_y) const;

    // Draws the bubble centred horizontally at (cx), bottom edge at (bottom_y).
    void render(SDL_Renderer *renderer, const TextRenderer &text, float cx, float bottom_y) const;

private:
    enum class Fade { None, In, Out };

    std::vector<std::string> strings_;
    std::string current_;
    Fade fade_ = Fade::None;
    float alpha_ = 0.0f;
    float text_timer_ = -1.0f;
    bool active_ = false;

    int auto_chance_ = 6;
    float cooldown_ = 0.0f;
    float cooldown_remaining_ = 0.0f;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace dvp
