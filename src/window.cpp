// window.cpp: window/renderer creation and z-order control.
#include "window.hpp"

#include "log.hpp"
#include "platform_clickthrough.hpp"

namespace dvp {

Window::~Window() { destroy(); }

bool Window::create(const WindowConfig &config) {
    destroy();

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config.transparent) {
        flags |= SDL_WINDOW_TRANSPARENT;
    }
    if (config.borderless) {
        flags |= SDL_WINDOW_BORDERLESS;
    }
    if (config.always_on_top) {
        flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    }
    if (config.no_focus) {
        flags |= SDL_WINDOW_NOT_FOCUSABLE;
    }
    if (config.maximized) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }

    window_ = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
    if (!window_) {
        DVP_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        DVP_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        destroy();
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(renderer_, 1);
    return true;
}

void Window::destroy() {
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

int Window::width() const {
    int w = 0;
    int h = 0;
    if (window_) {
        SDL_GetWindowSize(window_, &w, &h);
    }
    return w;
}

int Window::height() const {
    int w = 0;
    int h = 0;
    if (window_) {
        SDL_GetWindowSize(window_, &w, &h);
    }
    return h;
}

void Window::set_always_on_top(bool on_top) {
    if (window_) {
        SDL_SetWindowAlwaysOnTop(window_, on_top);
    }
}

void Window::set_sorting(WindowSort sort) {
    if (!window_) {
        return;
    }
    switch (sort) {
        case WindowSort::Top:
            SDL_SetWindowAlwaysOnTop(window_, true);
            break;
        case WindowSort::Disabled:
            SDL_SetWindowAlwaysOnTop(window_, false);
            break;
        case WindowSort::Bottom:
            SDL_SetWindowAlwaysOnTop(window_, false);
            platform::send_to_bottom(window_);
            break;
    }
}

void Window::request_attention() {
    if (window_) {
        SDL_FlashWindow(window_, SDL_FLASH_BRIEFLY);
    }
}

bool Window::set_shape(SDL_Surface *shape) {
    if (!window_) {
        return false;
    }
    if (!SDL_SetWindowShape(window_, shape)) {
        DVP_WARN("SDL_SetWindowShape failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

void Window::clear_shape() {
    if (window_) {
        SDL_SetWindowShape(window_, nullptr);
    }
}

}  // namespace dvp
