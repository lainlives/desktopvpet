// character_model.hpp: data model for the character editor (sources.json).
#pragma once

#include <map>
#include <string>
#include <vector>

namespace dvp::editor {

// A source rectangle used in "rects" mode.
struct AnimRect {
    int x = 0;
    int y = 0;
    int w = 112;
    int h = 112;
};

// One animation as authored in sources.json.
struct AnimSource {
    std::string name;
    double fps = 5.0;
    bool loop = true;
    std::string mode = "images";  // "images" | "atlas" | "rects"

    std::vector<std::string> images;  // images mode (paths relative to src_dir)
    std::string atlas;                // atlas/rects source sheet
    int cols = 0;                     // atlas mode
    int frames = 1;                   // atlas mode
    int start = 0;                    // atlas mode: first frame index
    std::vector<AnimRect> rects;      // rects mode
};

// One weighted choice in an action role.
struct RoleVariant {
    std::string animation;
    int weight = 1;
};

// The whole character directory, loaded from (and saved to) sources.json.
struct CharacterModel {
    std::string dir;  // character folder path

    std::string name = "character";
    int frame_width = 112;
    int frame_height = 112;
    std::string src_dir = "src";
    std::string font = "fonts/DejaVuSans.ttf";
    double font_size = 15.0;
    std::vector<std::string> dialogue;

    // role name -> weighted variants
    std::map<std::string, std::vector<RoleVariant>> actions;
    std::vector<AnimSource> animations;

    // Loads sources.json, falling back to character.json (read-only seed).
    bool Load(const std::string &directory, std::string *error);
    // Writes sources.json into `dir`.
    bool Save(std::string *error) const;

    AnimSource *FindAnimation(const std::string &animation_name);

    std::vector<std::string> AnimationNames() const;
    static const std::vector<std::string> &RoleNames();
};

}  // namespace dvp::editor
