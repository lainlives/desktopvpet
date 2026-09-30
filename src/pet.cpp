#include "pet.hpp"

#include <algorithm>
#include <cmath>

namespace de {

namespace {
constexpr float kGravity = 1500.0f;      // px/s^2
constexpr float kGroundMargin = 32.0f;   // gap from window bottom to feet
constexpr float kWallMargin = 8.0f;
constexpr float kBounce = 0.25f;
constexpr float kLinearDamp = 4.0f;      // per second
constexpr float kAngularDamp = 3.0f;     // per second
constexpr float kMaxDragSpeed = 4000.0f;
}  // namespace

Pet::Pet() = default;

void Pet::set_frame_size(float w, float h) {
    half_w_ = w * 0.5f;
    half_h_ = h * 0.5f;
}

void Pet::set_position(float x, float y) {
    x_ = x;
    y_ = y;
    vx_ = 0.0f;
    vy_ = 0.0f;
    angle_ = 0.0f;
    angular_velocity_ = 0.0f;
}

bool Pet::resting() const {
    return std::fabs(vx_) < 10.0f && std::fabs(vy_) < 10.0f &&
           std::fabs(angular_velocity_) < 3.0f;
}

bool Pet::rotating_fast() const { return std::fabs(angular_velocity_) > 10.0f; }

bool Pet::fast() const {
    const float speed_sq = vx_ * vx_ + vy_ * vy_;
    return speed_sq > 1000.0f * 1000.0f;
}

bool Pet::fell_over() const { return angle_ > 15.0f || angle_ < -15.0f; }

void Pet::start_action() {
    std::uniform_int_distribution<int> roll(1, 100);
    const int value = roll(rng_);
    if (value < 33) {
        std::uniform_int_distribution<int> dir(0, 1);
        facing_left_ = dir(rng_) == 1;
        walk_speed_ = facing_left_ ? -60.0f : 60.0f;
        state_ = PetState::Walking;
    } else if (value < 66) {
        state_ = PetState::DancyDance;
        dance_timer_ = 2.5f;
    } else {
        state_ = PetState::Idle;
    }
}

void Pet::update(float dt, float world_w, float world_h, const PetInput &input) {
    if (dt <= 0.0f) {
        return;
    }
    dt = std::min(dt, 0.05f);
    if (impact_timer_ > 0.0f) {
        impact_timer_ -= dt;
    }

    // --- State selection (mirrors EctoRigidTest._PhysicsProcess) ------------
    if (!input.mouse_down && input.mouse_on_pet) {
        if (state_ != PetState::Dragged && state_ != PetState::Spinny) {
            state_ = PetState::Hovered;
        }
    } else if (state_ == PetState::Hovered) {
        state_ = PetState::Idle;
    }

    if (input.mouse_pressed && input.mouse_on_pet) {
        state_ = PetState::Dragged;
    }

    if ((rotating_fast() || fast()) && !resting() && state_ == PetState::Idle) {
        spinny_timer_ = 0.0f;
        state_ = PetState::Spinny;
    } else if (state_ == PetState::Spinny && resting()) {
        spinny_timer_ += dt;
        if (spinny_timer_ > 0.1f) {
            spinny_timer_ = 0.0f;
            state_ = PetState::Idle;
        }
    }

    if (state_ == PetState::Dragged && input.mouse_released) {
        state_ = PetState::Idle;
    }

    // --- Per-state behaviour ------------------------------------------------
    if (state_ == PetState::Dragged) {
        const float target_x = input.mouse_x;
        const float target_y = std::min(input.mouse_y, world_h - kGroundMargin - half_h_);
        float nvx = (target_x - x_) / dt;
        float nvy = (target_y - y_) / dt;
        vx_ = std::clamp(nvx, -kMaxDragSpeed, kMaxDragSpeed);
        vy_ = std::clamp(nvy, -kMaxDragSpeed, kMaxDragSpeed);
        x_ = target_x;
        y_ = target_y;
        angular_velocity_ *= std::exp(-8.0f * dt);
        angle_ += angular_velocity_ * dt;
        action_timer_ = 0.0f;
        return;
    }

    if (state_ == PetState::Hovered) {
        // Still simulate gravity/damping so the pet settles while hovered.
    }

    if (state_ == PetState::Spinny) {
        // Let physics carry the spin out; no extra gravity override needed.
    }

    if (state_ == PetState::Walking) {
        if (x_ > world_w - 200.0f && walk_speed_ > 0.0f) {
            walk_speed_ = -walk_speed_;
            facing_left_ = true;
        } else if (x_ < 200.0f && walk_speed_ < 0.0f) {
            walk_speed_ = -walk_speed_;
            facing_left_ = false;
        }
        vx_ = walk_speed_;
    }

    if (state_ == PetState::DancyDance) {
        vx_ *= std::exp(-6.0f * dt);
        dance_timer_ -= dt;
        if (dance_timer_ <= 0.0f) {
            state_ = PetState::Idle;
        }
    }

    if (state_ == PetState::Idle || state_ == PetState::Hovered) {
        if (fell_over()) {
            vy_ -= 50.0f * dt;
            angular_velocity_ += -angle_ * 6.0f * dt;
        } else if (resting() && state_ == PetState::Idle) {
            action_timer_ += dt;
            if (action_timer_ > 1.0f) {
                action_timer_ = 0.0f;
                start_action();
            }
        }
    }

    // --- Integrate ----------------------------------------------------------
    vy_ += kGravity * dt;
    vx_ -= vx_ * kLinearDamp * dt;
    vy_ -= vy_ * (state_ == PetState::Walking ? kLinearDamp : 0.5f) * dt;
    angular_velocity_ -= angular_velocity_ * kAngularDamp * dt;

    x_ += vx_ * dt;
    y_ += vy_ * dt;
    angle_ += angular_velocity_ * dt;

    // --- Collisions ---------------------------------------------------------
    const float floor_y = world_h - kGroundMargin - half_h_;
    if (y_ >= floor_y) {
        y_ = floor_y;
        if (vy_ > 300.0f) {
            impact_timer_ = 0.35f;
        }
        if (std::fabs(vy_) > 40.0f) {
            vy_ = -vy_ * kBounce;
            // Convert some impact into spin, like a knocked-over toy.
            angular_velocity_ += vx_ * 0.02f;
        } else {
            vy_ = 0.0f;
        }
        vx_ *= std::exp(-8.0f * dt);
        if (state_ == PetState::Walking) {
            vx_ = walk_speed_;
        }
    }

    const float left = kWallMargin + half_w_;
    const float right = world_w - kWallMargin - half_w_;
    if (x_ < left) {
        x_ = left;
        vx_ = std::fabs(vx_) * kBounce;
        if (state_ == PetState::Walking) {
            walk_speed_ = std::fabs(walk_speed_);
            facing_left_ = false;
        }
    } else if (x_ > right) {
        x_ = right;
        vx_ = -std::fabs(vx_) * kBounce;
        if (state_ == PetState::Walking) {
            walk_speed_ = -std::fabs(walk_speed_);
            facing_left_ = true;
        }
    }

    // Right itself when tilted while grounded.
    if (y_ >= floor_y - 0.5f) {
        if (fell_over()) {
            angular_velocity_ += -angle_ * 8.0f * dt;
        } else {
            angle_ *= std::exp(-6.0f * dt);
            angular_velocity_ *= std::exp(-6.0f * dt);
        }
    }
}

}  // namespace de
