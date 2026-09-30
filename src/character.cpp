#include "character.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "config.hpp"
#include "log.hpp"

namespace de {

namespace {

std::string join_path(const std::string &dir, const std::string &file) {
    if (dir.empty()) {
        return file;
    }
    if (dir.back() == '/') {
        return dir + file;
    }
    return dir + "/" + file;
}

}  // namespace

bool Character::load(SDL_Renderer *renderer, const std::string &dir,
                     const std::string &manifest_name) {
    dir_ = dir;
    textures_.clear();
    animations_.clear();
    roles_.clear();
    current_role_ = AnimRole::Count;

    const std::string manifest_path = join_path(dir, manifest_name);
    std::string error;
    Json root = load_json_file(manifest_path, &error);
    if (error.empty() && root.is_object() && root.has("animations")) {
        // fall through
    } else if (!error.empty()) {
        DE_ERROR("character manifest '%s': %s", manifest_path.c_str(), error.c_str());
        return false;
    } else {
        DE_ERROR("character manifest '%s' is missing an 'animations' object",
                 manifest_path.c_str());
        return false;
    }

    name_ = root.get_string("name", "character");
    const int def_w = root.get_int("frame_width", 0);
    const int def_h = root.get_int("frame_height", 0);
    const std::string def_sheet = root.get_string("sheet", "");

    const Json *anims = root.find("animations");
    for (const auto &entry : anims->object) {
        const std::string &key = entry.first;
        const Json &spec = entry.second;
        if (!spec.is_object()) {
            DE_WARN("animation '%s' is not an object, skipping", key.c_str());
            continue;
        }

        Animation anim;
        anim.name = key;
        anim.fps = static_cast<float>(spec.get_number("fps", 5.0));
        anim.loop = spec.get_bool("loop", true);

        std::string image = spec.get_string("image", def_sheet);
        if (image.empty()) {
            image = key + ".png";
        }
        anim.source = image;

        if (!texture_for(image) && !load_texture(renderer, image, image)) {
            DE_WARN("animation '%s' skipped: could not load '%s'", key.c_str(), image.c_str());
            continue;
        }
        const Texture *tex = texture_for(image);

        int fw = spec.get_int("frame_width", def_w > 0 ? def_w : static_cast<int>(tex->height()));
        int fh = spec.get_int("frame_height", def_h > 0 ? def_h : static_cast<int>(tex->height()));
        if (fw <= 0 || fh <= 0) {
            DE_WARN("animation '%s' has invalid frame size %dx%d", key.c_str(), fw, fh);
            continue;
        }

        const Json *frames = spec.find("frames");
        if (frames && frames->is_array()) {
            for (const Json &item : frames->array) {
                if (!item.is_object()) {
                    continue;
                }
                const int x = item.get_int("x", 0);
                const int y = item.get_int("y", 0);
                const int w = item.get_int("w", fw);
                const int h = item.get_int("h", fh);
                anim.frames.push_back(SDL_Rect{x, y, w, h});
            }
        } else {
            int count = (frames && frames->is_number()) ? static_cast<int>(frames->number) : 0;
            if (count <= 0) {
                // Auto-detect a horizontal strip: frame height matches the image
                // height (or the manifest frame height) and frames tile across.
                count = (fw > 0 && static_cast<int>(tex->width()) > fw)
                            ? static_cast<int>(tex->width()) / fw
                            : 1;
            }
            const int col = spec.get_int("col", 0);
            const int row = spec.get_int("row", 0);
            const int x0 = col * fw;
            const int y0 = row * fh;
            for (int i = 0; i < count; ++i) {
                const int x = x0 + i * fw;
                if (x + fw > static_cast<int>(tex->width())) {
                    break;
                }
                anim.frames.push_back(SDL_Rect{x, y0, fw, fh});
            }
        }

        if (anim.frames.empty()) {
            DE_WARN("animation '%s' produced no frames", key.c_str());
            continue;
        }
        animations_[key] = std::move(anim);
    }

    font_ = root.get_string("font", font_);
    font_size_ = static_cast<float>(root.get_number("font_size", font_size_));

    const Json *dialogue = root.find("dialogue");
    if (dialogue && dialogue->is_array()) {
        for (const Json &line : dialogue->array) {
            if (line.is_string()) {
                dialogue_.push_back(line.string);
            }
        }
    }

    if (animations_.empty()) {
        DE_ERROR("character '%s' has no loadable animations", name_.c_str());
        return false;
    }

    // --- Semantic action roles -------------------------------------------
    const std::map<std::string, AnimRole> role_keys = {
        {"idle", AnimRole::Idle},         {"walk", AnimRole::Walk},
        {"clicked", AnimRole::Clicked},   {"fall_impact", AnimRole::FallImpact},
        {"spinny", AnimRole::Spinny},     {"dance", AnimRole::Dance},
        {"talk", AnimRole::Talk},
    };

    auto parse_variants = [](const Json &value) {
        std::vector<AnimVariant> out;
        auto add = [&out](const Json &entry) {
            if (entry.is_string()) {
                out.push_back(AnimVariant{entry.string, 1});
            } else if (entry.is_object()) {
                std::string anim = entry.get_string("animation", entry.get_string("name", ""));
                if (!anim.empty()) {
                    out.push_back(AnimVariant{anim, std::max(1, entry.get_int("weight", 1))});
                }
            }
        };
        if (value.is_array()) {
            for (const Json &entry : value.array) {
                add(entry);
            }
        } else {
            add(value);
        }
        return out;
    };

    if (const Json *actions = root.find("actions")) {
        if (actions->is_object()) {
            for (const auto &entry : actions->object) {
                auto key = role_keys.find(entry.first);
                if (key == role_keys.end()) {
                    DE_WARN("unknown action role '%s'", entry.first.c_str());
                    continue;
                }
                roles_[key->second] = parse_variants(entry.second);
            }
        } else {
            DE_WARN("'actions' must be an object");
        }
    }

    // Drop variants whose clip is missing.
    for (auto it = roles_.begin(); it != roles_.end();) {
        std::vector<AnimVariant> &variants = it->second;
        variants.erase(std::remove_if(variants.begin(), variants.end(),
                                      [&](const AnimVariant &v) {
                                          if (animations_.count(v.name)) {
                                              return false;
                                          }
                                          DE_WARN("action '%s' references missing clip '%s'",
                                                  role_name(it->first), v.name.c_str());
                                          return true;
                                      }),
                       variants.end());
        if (variants.empty()) {
            it = roles_.erase(it);
        } else {
            ++it;
        }
    }

    auto has_role = [&](AnimRole r) { return roles_.count(r) && !roles_[r].empty(); };
    const std::string first_anim = animations_.begin()->first;

    // Fallbacks: idle is the base; everything else degrades to idle/clicked.
    if (!has_role(AnimRole::Idle)) {
        roles_[AnimRole::Idle] = {AnimVariant{has_animation("idle") ? "idle" : first_anim, 1}};
    }
    if (!has_role(AnimRole::Walk)) {
        roles_[AnimRole::Walk] = roles_[AnimRole::Idle];
    }
    if (!has_role(AnimRole::Clicked)) {
        roles_[AnimRole::Clicked] = roles_[AnimRole::Idle];
    }
    if (!has_role(AnimRole::FallImpact)) {
        roles_[AnimRole::FallImpact] = roles_[AnimRole::Clicked];
    }
    if (!has_role(AnimRole::Spinny)) {
        roles_[AnimRole::Spinny] = roles_[AnimRole::Clicked];
    }
    if (!has_role(AnimRole::Dance)) {
        roles_[AnimRole::Dance] = roles_[AnimRole::Idle];
    }
    // Talk intentionally has no fallback: it is opt-in only.

    // Start on the idle role.
    play_role(AnimRole::Idle, true);

    DE_INFO("loaded character '%s' with %zu animation(s)", name_.c_str(), animations_.size());
    return true;
}

bool Character::play(const std::string &animation, bool restart) {
    auto it = animations_.find(animation);
    if (it == animations_.end()) {
        return false;
    }
    animator_.play(&it->second, restart);
    current_role_ = AnimRole::Count;
    return true;
}

bool Character::has_role(AnimRole role) const {
    auto it = roles_.find(role);
    return it != roles_.end() && !it->second.empty();
}

const std::string &Character::pick_variant(const std::vector<AnimVariant> &variants) {
    static const std::string kEmpty;
    if (variants.empty()) {
        return kEmpty;
    }
    int total = 0;
    for (const AnimVariant &v : variants) {
        total += std::max(1, v.weight);
    }
    std::uniform_int_distribution<int> dist(1, total);
    int roll = dist(rng_);
    for (const AnimVariant &v : variants) {
        roll -= std::max(1, v.weight);
        if (roll <= 0) {
            return v.name;
        }
    }
    return variants.back().name;
}

bool Character::play_role(AnimRole role, bool restart) {
    auto it = roles_.find(role);
    if (it == roles_.end() || it->second.empty()) {
        return false;
    }
    if (role == current_role_ && !restart) {
        return true;
    }
    const std::string &name = pick_variant(it->second);
    auto anim = animations_.find(name);
    if (anim == animations_.end()) {
        return false;
    }
    animator_.play(&anim->second, true);
    current_role_ = role;
    if (std::getenv("DE_DEBUG_ROLE")) {
        DE_INFO("role '%s' -> clip '%s'", role_name(role), name.c_str());
    }
    return true;
}

const char *Character::role_name(AnimRole role) {
    switch (role) {
        case AnimRole::Idle: return "idle";
        case AnimRole::Walk: return "walk";
        case AnimRole::Clicked: return "clicked";
        case AnimRole::FallImpact: return "fall_impact";
        case AnimRole::Spinny: return "spinny";
        case AnimRole::Dance: return "dance";
        case AnimRole::Talk: return "talk";
        case AnimRole::Count: break;
    }
    return "?";
}

void Character::update(float dt) { animator_.update(dt); }

const Texture *Character::texture_for(const std::string &source) const {
    auto it = textures_.find(source);
    return it == textures_.end() ? nullptr : it->second.get();
}

bool Character::load_texture(SDL_Renderer *renderer, const std::string &key,
                             const std::string &relpath) {
    auto tex = std::make_unique<Texture>();
    if (!tex->load(renderer, join_path(dir_, relpath))) {
        return false;
    }
    textures_[key] = std::move(tex);
    return true;
}

void Character::render(SDL_Renderer *renderer, float cx, float cy, float angle, bool flip) const {
    const Animation *anim = animator_.animation();
    if (!anim) {
        return;
    }
    const Texture *tex = texture_for(anim->source);
    if (!tex || !tex->valid()) {
        return;
    }
    const SDL_Rect r = animator_.frame_rect();
    const SDL_FRect src{static_cast<float>(r.x), static_cast<float>(r.y),
                        static_cast<float>(r.w), static_cast<float>(r.h)};
    const SDL_FRect dst{cx - src.w * 0.5f, cy - src.h * 0.5f, src.w, src.h};
    SDL_RenderTextureRotated(renderer, tex->handle(), &src, &dst, angle, nullptr,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

bool Character::hit_test(float screen_x, float screen_y, float cx, float cy, float angle) const {
    const Animation *anim = animator_.animation();
    if (!anim) {
        return false;
    }
    const Texture *tex = texture_for(anim->source);
    if (!tex) {
        return false;
    }
    const SDL_Rect r = animator_.frame_rect();

    float lx = screen_x - cx;
    float ly = screen_y - cy;

    // Undo rotation so the test is in the frame's local space.
    const float rad = -angle * 3.14159265358979323846f / 180.0f;
    const float cs = std::cos(rad);
    const float sn = std::sin(rad);
    const float rx = lx * cs - ly * sn;
    const float ry = lx * sn + ly * cs;

    const int px = static_cast<int>(rx + static_cast<float>(r.w) * 0.5f);
    const int py = static_cast<int>(ry + static_cast<float>(r.h) * 0.5f);
    if (px < 0 || py < 0 || px >= r.w || py >= r.h) {
        return false;
    }
    return tex->alpha_at(r.x + px, r.y + py) > 127;
}

SDL_Surface *Character::current_source_surface() const {
    const Animation *anim = animator_.animation();
    if (!anim) {
        return nullptr;
    }
    const Texture *tex = texture_for(anim->source);
    return tex ? tex->surface() : nullptr;
}

SDL_Rect Character::current_frame_rect() const { return animator_.frame_rect(); }

void Character::unload() {
    animator_ = Animator{};
    animations_.clear();
    roles_.clear();
    current_role_ = AnimRole::Count;
    textures_.clear();
    dialogue_.clear();
    name_ = "character";
}

float Character::frame_width() const {
    const SDL_Rect r = animator_.frame_rect();
    return static_cast<float>(r.w);
}

float Character::frame_height() const {
    const SDL_Rect r = animator_.frame_rect();
    return static_cast<float>(r.h);
}

}  // namespace de
