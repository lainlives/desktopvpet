// animation.hpp: one animation clip (source sheet key, frame rects, fps, loop).
#pragma once

#include <SDL3/SDL.h>

#include <map>
#include <string>
#include <vector>

namespace dvp {

// A single animation clip. `frames` are source-pixel rectangles inside the
// texture identified by `source` (a key owned by Character).
struct Animation {
    std::string name;
    std::string source;
    std::vector<SDL_Rect> frames;
    float fps = 5.0f;
    bool loop = true;

    bool empty() const { return frames.empty(); }
    float frame_time() const { return fps > 0.0f ? 1.0f / fps : 0.0f; }
};

using AnimationSet = std::map<std::string, Animation>;

}  // namespace dvp
