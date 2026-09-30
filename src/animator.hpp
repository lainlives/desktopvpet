// animator.hpp: frame timing and looping for a single Animation.
#pragma once

#include "animation.hpp"

namespace dvp {

// Drives frame selection for one Animation pointer. Owned by Character.
class Animator {
public:
    void play(const Animation *animation, bool restart);
    void update(float dt);

    const Animation *animation() const { return animation_; }
    const std::string &name() const;
    int frame_index() const { return frame_; }
    bool finished() const { return finished_; }

    // Rectangle of the current frame in source pixels. Returns an empty rect
    // when no animation is playing.
    SDL_Rect frame_rect() const;

private:
    const Animation *animation_ = nullptr;
    float timer_ = 0.0f;
    int frame_ = 0;
    bool finished_ = false;
};

}  // namespace dvp
