// texture.hpp: RAII SDL_Texture plus a CPU RGBA surface for pixel alpha queries.
#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace dvp {

// Owns an SDL_Texture plus an RGBA32 CPU surface for per-pixel alpha queries
// (needed for click-through and hover hit-testing).
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;
    Texture(Texture &&other) noexcept;
    Texture &operator=(Texture &&other) noexcept;

    bool load(SDL_Renderer *renderer, const std::string &path);
    void reset();

    bool valid() const { return texture_ != nullptr; }
    SDL_Texture *handle() const { return texture_; }
    SDL_Surface *surface() const { return surface_; }
    float width() const { return width_; }
    float height() const { return height_; }

    // Alpha (0-255) of a pixel in the source surface; 0 when out of bounds.
    Uint8 alpha_at(int x, int y) const;

private:
    SDL_Texture *texture_ = nullptr;
    SDL_Surface *surface_ = nullptr;
    float width_ = 0.0f;
    float height_ = 0.0f;
};

}  // namespace dvp
