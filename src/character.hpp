// character.hpp: loads a character manifest and maps semantic roles to clips.
#pragma once

#include <SDL3/SDL.h>

#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "animation.hpp"
#include "animator.hpp"
#include "texture.hpp"

namespace dvp {

// Semantic animation slots. A character maps these to its own animation clips
// in the manifest so gameplay code never hard-codes clip names.
enum class AnimRole {
    Idle,
    Walk,
    Clicked,     // hovered / dragged
    FallImpact,  // hard landing (defaults to Clicked)
    Spinny,
    Dance,
    Talk,        // optional, shown while a dialogue bubble is visible
    Count,
};

// One weighted choice for a role.
struct AnimVariant {
    std::string name;
    int weight = 1;
};

// A swappable character: a folder containing a JSON manifest plus one or more
// sprite sheets. Animations can each point at their own sheet ("one sheet per
// activity") or share a common atlas. `actions` maps roles to clips.
class Character {
public:
    // Loads <dir>/<manifest_name>. Returns false (with a logged error) on failure.
    bool load(SDL_Renderer *renderer, const std::string &dir,
              const std::string &manifest_name = "character.json");

    // Raw clip playback (used for debugging / explicit overrides).
    bool play(const std::string &animation, bool restart = false);

    // Plays the clip bound to `role`. Weighted roles (e.g. idle variants) pick
    // a new clip when the role becomes active. No-op if the role is unchanged.
    bool play_role(AnimRole role, bool restart = false);
    bool has_role(AnimRole role) const;

    void update(float dt);

    // Draws the current frame centred at (cx, cy), rotated by `angle` degrees.
    void render(SDL_Renderer *renderer, float cx, float cy, float angle, bool flip) const;

    // True when a screen point lands on an opaque pixel of the current frame.
    bool hit_test(float screen_x, float screen_y, float cx, float cy, float angle) const;

    float frame_width() const;
    float frame_height() const;

    const std::string &name() const { return name_; }
    const std::string &current_animation() const { return animator_.name(); }
    bool has_animation(const std::string &n) const { return animations_.count(n) > 0; }
    int current_frame_index() const { return animator_.frame_index(); }

    // Source surface + frame rect for the frame being drawn. Used to build the
    // window silhouette for click-through.
    SDL_Surface *current_source_surface() const;
    SDL_Rect current_frame_rect() const;

    const std::vector<std::string> &dialogue_strings() const { return dialogue_; }
    const std::string &font() const { return font_; }
    float font_size() const { return font_size_; }

    // Releases SDL textures. Must be called before SDL_Quit()/renderer teardown.
    void unload();

    static const char *role_name(AnimRole role);

private:
    const Texture *texture_for(const std::string &source) const;
    bool load_texture(SDL_Renderer *renderer, const std::string &key, const std::string &relpath);

    const std::string &pick_variant(const std::vector<AnimVariant> &variants);

    std::string dir_;
    std::string name_ = "character";
    std::map<std::string, std::unique_ptr<Texture>> textures_;
    AnimationSet animations_;
    Animator animator_;
    std::map<AnimRole, std::vector<AnimVariant>> roles_;
    AnimRole current_role_ = AnimRole::Count;
    std::vector<std::string> dialogue_;
    std::string font_ = "fonts/DejaVuSans.ttf";
    float font_size_ = 15.0f;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace dvp
