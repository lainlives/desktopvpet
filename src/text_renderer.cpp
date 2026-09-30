// text_renderer.cpp: font loading, measurement and cached text drawing.
#include "text_renderer.hpp"

#include <SDL3_ttf/SDL_ttf.h>

#include "log.hpp"

namespace dvp {

TextRenderer::~TextRenderer() { unload(); }

bool TextRenderer::load(SDL_Renderer *renderer, const std::string &font_path, float point_size) {
    unload();
    renderer_ = renderer;
    size_ = point_size > 0.0f ? point_size : 14.0f;

    if (!TTF_WasInit() && !TTF_Init()) {
        DVP_ERROR("TTF_Init failed: %s", SDL_GetError());
        return false;
    }

    font_ = TTF_OpenFont(font_path.c_str(), size_);
    if (!font_) {
        DVP_ERROR("TTF_OpenFont('%s') failed: %s", font_path.c_str(), SDL_GetError());
        return false;
    }
    return true;
}

void TextRenderer::unload() {
    for (auto &entry : cache_) {
        if (entry.second) {
            SDL_DestroyTexture(entry.second);
        }
    }
    cache_.clear();
    if (font_) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
    renderer_ = nullptr;
}

bool TextRenderer::measure(const std::string &text, int *out_w, int *out_h) const {
    if (!font_) {
        return false;
    }
    int w = 0;
    int h = 0;
    if (!TTF_GetStringSize(font_, text.c_str(), 0, &w, &h)) {
        return false;
    }
    if (out_w) {
        *out_w = w;
    }
    if (out_h) {
        *out_h = h;
    }
    return true;
}

SDL_Texture *TextRenderer::texture_for(SDL_Renderer *renderer, const std::string &text,
                                       SDL_Color color, int *out_w, int *out_h) const {
    const std::string key = text + "|" + std::to_string(color.r) + "," +
                            std::to_string(color.g) + "," + std::to_string(color.b) + "," +
                            std::to_string(color.a);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (it->second) {
            float fw = 0.0f;
            float fh = 0.0f;
            SDL_GetTextureSize(it->second, &fw, &fh);
            if (out_w) {
                *out_w = static_cast<int>(fw);
            }
            if (out_h) {
                *out_h = static_cast<int>(fh);
            }
        }
        return it->second;
    }

    SDL_Surface *surface = TTF_RenderText_Blended(font_, text.c_str(), 0, color);
    if (!surface) {
        return nullptr;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (out_w) {
        *out_w = surface->w;
    }
    if (out_h) {
        *out_h = surface->h;
    }
    SDL_DestroySurface(surface);
    cache_[key] = texture;
    return texture;
}

void TextRenderer::draw(SDL_Renderer *renderer, const std::string &text, const SDL_FRect &rect,
                        SDL_Color color) const {
    if (!font_ || text.empty()) {
        return;
    }
    int w = 0;
    int h = 0;
    SDL_Texture *texture = texture_for(renderer, text, color, &w, &h);
    if (!texture) {
        return;
    }
    const SDL_FRect dst{rect.x + (rect.w - static_cast<float>(w)) * 0.5f,
                        rect.y + (rect.h - static_cast<float>(h)) * 0.5f,
                        static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderTexture(renderer, texture, nullptr, &dst);
}

}  // namespace dvp
