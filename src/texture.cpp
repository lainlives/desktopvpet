// texture.cpp: texture loading via SDL_image and alpha sampling for hit-tests.
#include "texture.hpp"

#include <SDL3_image/SDL_image.h>

#include <utility>

#include "log.hpp"

namespace dvp {

Texture::~Texture() { reset(); }

Texture::Texture(Texture &&other) noexcept
    : texture_(other.texture_),
      surface_(other.surface_),
      width_(other.width_),
      height_(other.height_) {
    other.texture_ = nullptr;
    other.surface_ = nullptr;
    other.width_ = 0.0f;
    other.height_ = 0.0f;
}

Texture &Texture::operator=(Texture &&other) noexcept {
    if (this != &other) {
        reset();
        texture_ = other.texture_;
        surface_ = other.surface_;
        width_ = other.width_;
        height_ = other.height_;
        other.texture_ = nullptr;
        other.surface_ = nullptr;
        other.width_ = 0.0f;
        other.height_ = 0.0f;
    }
    return *this;
}

bool Texture::load(SDL_Renderer *renderer, const std::string &path) {
    reset();

    SDL_Surface *loaded = IMG_Load(path.c_str());
    if (!loaded) {
        DVP_ERROR("failed to load image '%s': %s", path.c_str(), SDL_GetError());
        return false;
    }

    surface_ = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!surface_) {
        DVP_ERROR("failed to convert '%s': %s", path.c_str(), SDL_GetError());
        return false;
    }

    texture_ = SDL_CreateTextureFromSurface(renderer, surface_);
    if (!texture_) {
        DVP_ERROR("failed to create texture for '%s': %s", path.c_str(), SDL_GetError());
        reset();
        return false;
    }

    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);

    width_ = static_cast<float>(surface_->w);
    height_ = static_cast<float>(surface_->h);
    return true;
}

void Texture::reset() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (surface_) {
        SDL_DestroySurface(surface_);
        surface_ = nullptr;
    }
    width_ = 0.0f;
    height_ = 0.0f;
}

Uint8 Texture::alpha_at(int x, int y) const {
    if (!surface_ || x < 0 || y < 0 || x >= surface_->w || y >= surface_->h) {
        return 0;
    }
    const auto *pixels = static_cast<const Uint8 *>(surface_->pixels);
    const int pitch = surface_->pitch;
    // Surface is RGBA32 => alpha is byte 3 of each pixel.
    return pixels[y * pitch + x * 4 + 3];
}

}  // namespace dvp
