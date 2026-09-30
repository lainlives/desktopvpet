#pragma once

#include <random>

namespace de {

enum class PetState { Idle, Hovered, Spinny, Dragged, Walking, DancyDance };

struct PetInput {
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    bool mouse_down = false;
    bool mouse_pressed = false;
    bool mouse_released = false;
    bool mouse_on_pet = false;
};

// Lightweight 2D "ragdoll": gravity, floor/wall collision, upright righting
// torque and mouse dragging. Mirrors the behaviour of EctoRigidTest.cs without
// pulling in a full physics engine.
class Pet {
public:
    Pet();

    void set_frame_size(float w, float h);
    void set_position(float x, float y);

    void update(float dt, float world_w, float world_h, const PetInput &input);

    bool facing_left() const { return facing_left_; }

    // True briefly after a hard landing (drives the FallImpact role).
    bool impact_active() const { return impact_timer_ > 0.0f; }

    float x() const { return x_; }
    float y() const { return y_; }
    float angle() const { return angle_; }
    PetState state() const { return state_; }

private:
    void start_action();
    bool resting() const;
    bool rotating_fast() const;
    bool fast() const;
    bool fell_over() const;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float vx_ = 0.0f;
    float vy_ = 0.0f;
    float angle_ = 0.0f;
    float angular_velocity_ = 0.0f;

    float half_w_ = 40.0f;
    float half_h_ = 40.0f;

    PetState state_ = PetState::Idle;
    float action_timer_ = 0.0f;
    float spinny_timer_ = 0.0f;
    float dance_timer_ = 0.0f;
    float walk_speed_ = 60.0f;
    float impact_timer_ = 0.0f;
    bool facing_left_ = false;

    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace de
