// character_model.cpp: sources.json <-> CharacterModel conversion.
#include "character_model.hpp"

#include <filesystem>

#include "config.hpp"

namespace dvp::editor {

namespace {

Json js_string(const std::string &s) {
    Json j;
    j.type = Json::Type::String;
    j.string = s;
    return j;
}
Json js_number(double d) {
    Json j;
    j.type = Json::Type::Number;
    j.number = d;
    return j;
}
Json js_bool(bool b) {
    Json j;
    j.type = Json::Type::Bool;
    j.boolean = b;
    return j;
}
Json js_array() {
    Json j;
    j.type = Json::Type::Array;
    return j;
}
Json js_object() {
    Json j;
    j.type = Json::Type::Object;
    return j;
}

std::vector<RoleVariant> parse_variants(const Json &value) {
    std::vector<RoleVariant> out;
    auto add = [&out](const Json &entry) {
        if (entry.is_string()) {
            out.push_back(RoleVariant{entry.string, 1});
        } else if (entry.is_object()) {
            RoleVariant v;
            v.animation = entry.get_string("animation", entry.get_string("name", ""));
            v.weight = entry.get_int("weight", 1);
            if (!v.animation.empty()) {
                out.push_back(v);
            }
        }
    };
    if (value.is_array()) {
        for (const Json &e : value.array) {
            add(e);
        }
    } else {
        add(value);
    }
    return out;
}

}  // namespace

const std::vector<std::string> &CharacterModel::RoleNames() {
    static const std::vector<std::string> kRoles = {
        "idle", "walk", "clicked", "fall_impact", "spinny", "dance", "talk"};
    return kRoles;
}

AnimSource *CharacterModel::FindAnimation(const std::string &animation_name) {
    for (AnimSource &a : animations) {
        if (a.name == animation_name) {
            return &a;
        }
    }
    return nullptr;
}

std::vector<std::string> CharacterModel::AnimationNames() const {
    std::vector<std::string> names;
    names.reserve(animations.size());
    for (const AnimSource &a : animations) {
        names.push_back(a.name);
    }
    return names;
}

bool CharacterModel::Load(const std::string &directory, std::string *error) {
    dir = directory;
    animations.clear();
    actions.clear();
    dialogue.clear();

    std::error_code ec;
    std::string path = dir + "/sources.json";
    if (!std::filesystem::exists(path, ec)) {
        path = dir + "/character.json";
    }
    if (!std::filesystem::exists(path, ec)) {
        if (error) {
            *error = "no sources.json or character.json in " + dir;
        }
        return false;
    }

    std::string parse_error;
    Json root = load_json_file(path, &parse_error);
    if (!parse_error.empty() || !root.is_object()) {
        if (error) {
            *error = parse_error.empty() ? "invalid JSON: " + path : parse_error;
        }
        return false;
    }

    name = root.get_string("name", name);
    frame_width = root.get_int("frame_width", frame_width);
    frame_height = root.get_int("frame_height", frame_height);
    src_dir = root.get_string("src_dir", src_dir);
    font = root.get_string("font", font);
    font_size = root.get_number("font_size", font_size);

    if (const Json *lines = root.find("dialogue")) {
        if (lines->is_array()) {
            for (const Json &line : lines->array) {
                if (line.is_string()) {
                    dialogue.push_back(line.string);
                }
            }
        }
    }

    if (const Json *acts = root.find("actions")) {
        if (acts->is_object()) {
            for (const auto &kv : acts->object) {
                actions[kv.first] = parse_variants(kv.second);
            }
        }
    }

    if (const Json *anims = root.find("animations")) {
        if (anims->is_object()) {
            for (const auto &kv : anims->object) {
                const Json &spec = kv.second;
                if (!spec.is_object()) {
                    continue;
                }
                AnimSource a;
                a.name = kv.first;
                a.fps = spec.get_number("fps", a.fps);
                a.loop = spec.get_bool("loop", a.loop);

                if (const Json *images = spec.find("images")) {
                    a.mode = "images";
                    if (images->is_array()) {
                        for (const Json &im : images->array) {
                            if (im.is_string()) {
                                a.images.push_back(im.string);
                            }
                        }
                    }
                } else if (const Json *atlas = spec.find("atlas")) {
                    a.atlas = atlas->is_string() ? atlas->string : "";
                    a.cols = spec.get_int("cols", a.cols);
                    a.frames = spec.get_int("frames", a.frames);
                    a.start = spec.get_int("start", a.start);
                    if (const Json *rects = spec.find("rects"); rects && rects->is_array()) {
                        a.mode = "rects";
                        for (const Json &r : rects->array) {
                            if (r.is_object()) {
                                a.rects.push_back(AnimRect{r.get_int("x", 0), r.get_int("y", 0),
                                                           r.get_int("w", frame_width),
                                                           r.get_int("h", frame_height)});
                            }
                        }
                    } else {
                        a.mode = "atlas";
                    }
                } else {
                    a.mode = "images";
                    std::string image = spec.get_string("image", "");
                    if (!image.empty()) {
                        a.images.push_back(image);
                    }
                }
                animations.push_back(std::move(a));
            }
        }
    }

    return true;
}

bool CharacterModel::Save(std::string *error) const {
    Json root = js_object();
    root.object["name"] = js_string(name);
    root.object["frame_width"] = js_number(frame_width);
    root.object["frame_height"] = js_number(frame_height);
    root.object["src_dir"] = js_string(src_dir);
    root.object["font"] = js_string(font);
    root.object["font_size"] = js_number(font_size);

    if (!dialogue.empty()) {
        Json lines = js_array();
        for (const std::string &line : dialogue) {
            lines.array.push_back(js_string(line));
        }
        root.object["dialogue"] = std::move(lines);
    }

    if (!actions.empty()) {
        Json acts = js_object();
        for (const auto &kv : actions) {
            Json variants = js_array();
            for (const RoleVariant &v : kv.second) {
                Json entry = js_object();
                entry.object["animation"] = js_string(v.animation);
                entry.object["weight"] = js_number(v.weight);
                variants.array.push_back(std::move(entry));
            }
            acts.object[kv.first] = std::move(variants);
        }
        root.object["actions"] = std::move(acts);
    }

    Json anims = js_object();
    for (const AnimSource &a : animations) {
        Json spec = js_object();
        spec.object["fps"] = js_number(a.fps);
        spec.object["loop"] = js_bool(a.loop);
        if (a.mode == "images") {
            Json images = js_array();
            for (const std::string &im : a.images) {
                images.array.push_back(js_string(im));
            }
            spec.object["images"] = std::move(images);
        } else if (a.mode == "atlas") {
            spec.object["atlas"] = js_string(a.atlas);
            spec.object["cols"] = js_number(a.cols);
            spec.object["frames"] = js_number(a.frames);
            spec.object["start"] = js_number(a.start);
        } else {  // rects
            spec.object["atlas"] = js_string(a.atlas);
            Json rects = js_array();
            for (const AnimRect &r : a.rects) {
                Json e = js_object();
                e.object["x"] = js_number(r.x);
                e.object["y"] = js_number(r.y);
                e.object["w"] = js_number(r.w);
                e.object["h"] = js_number(r.h);
                rects.array.push_back(std::move(e));
            }
            spec.object["rects"] = std::move(rects);
        }
        anims.object[a.name] = std::move(spec);
    }
    root.object["animations"] = std::move(anims);

    return save_json_file(dir + "/sources.json", root, error);
}

}  // namespace dvp::editor
