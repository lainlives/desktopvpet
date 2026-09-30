#pragma once

#include <SDL3/SDL.h>

struct TTF_Font;

#include <map>
#include <string>

namespace de {

// Small TTF text helper with a one-entry-per-string texture cache.
class TextRenderer {
public:
    ~TextRenderer();

    bool load(SDL_Renderer *renderer, const std::string &font_path, float point_size);
    void unload();

    bool valid() const { return font_ != nullptr; }

    // Measures a single line of text (unwrapped); returns false if unavailable.
    bool measure(const std::string &text, int *out_w, int *out_h) const;

    // Draws `text` centred inside `rect`, wrapped to the rect width.
    void draw(SDL_Renderer *renderer, const std::string &text, const SDL_FRect &rect,
              SDL_Color color) const;

private:
    SDL_Texture *texture_for(SDL_Renderer *renderer, const std::string &text, SDL_Color color,
                             int *out_w, int *out_h) const;

    SDL_Renderer *renderer_ = nullptr;
    TTF_Font *font_ = nullptr;
    float size_ = 14.0f;
    mutable std::map<std::string, SDL_Texture *> cache_;
};

}  // namespace de
