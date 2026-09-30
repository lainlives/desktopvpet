// animator.cpp: animator update logic (looping and one-shot clips).
#include "animator.hpp"

#include <algorithm>
#include <cmath>

namespace dvp {

namespace {
SDL_Rect empty_rect() { return SDL_Rect{0, 0, 0, 0}; }
}  // namespace

void Animator::play(const Animation *animation, bool restart) {
    if (!animation) {
        return;
    }
    if (animation_ == animation && !restart) {
        return;
    }
    animation_ = animation;
    frame_ = 0;
    timer_ = 0.0f;
    finished_ = animation->empty();
}

void Animator::update(float dt) {
    if (!animation_ || animation_->empty()) {
        return;
    }
    if (finished_ && !animation_->loop) {
        return;
    }

    const int count = static_cast<int>(animation_->frames.size());
    const float fps = animation_->fps;

    if (fps <= 0.0f) {
        frame_ = animation_->loop ? 0 : count - 1;
        finished_ = !animation_->loop;
        return;
    }

    timer_ += dt;
    if (animation_->loop) {
        const float total = static_cast<float>(count) / fps;
        timer_ = std::fmod(timer_, total);
        frame_ = std::min(count - 1, static_cast<int>(timer_ * fps));
        finished_ = false;
    } else {
        const int f = static_cast<int>(timer_ * fps);
        if (f >= count) {
            frame_ = count - 1;
            finished_ = true;
        } else {
            frame_ = f;
        }
    }
}

const std::string &Animator::name() const {
    static const std::string kEmpty;
    return animation_ ? animation_->name : kEmpty;
}

SDL_Rect Animator::frame_rect() const {
    if (!animation_ || animation_->frames.empty()) {
        return empty_rect();
    }
    int idx = frame_;
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= static_cast<int>(animation_->frames.size())) {
        idx = static_cast<int>(animation_->frames.size()) - 1;
    }
    return animation_->frames[idx];
}

}  // namespace dvp
