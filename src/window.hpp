// window.hpp: transparent borderless overlay window + renderer.
#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace dvp {

enum class WindowSort { Disabled, Top, Bottom };

struct WindowConfig {
    std::string title = "DesktopVPet";
    int width = 1280;
    int height = 720;
    bool borderless = true;
    bool transparent = true;
    bool always_on_top = true;
    bool no_focus = false;
    bool maximized = true;
};

// Owns the SDL window + renderer. Handles the transparent/borderless
// "desktop overlay" setup and exposes SDL_SetWindowShape for click-through.
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    bool create(const WindowConfig &config);
    void destroy();

    SDL_Window *handle() const { return window_; }
    SDL_Renderer *renderer() const { return renderer_; }

    int width() const;
    int height() const;

    void set_always_on_top(bool on_top);
    void set_sorting(WindowSort sort);
    void request_attention();

    // Sets the window silhouette used for both rendering alpha and input
    // routing. Fully transparent regions pass mouse clicks through.
    // `shape` must be ARGB32; SDL copies it, so the caller may free it.
    bool set_shape(SDL_Surface *shape);
    void clear_shape();

private:
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
};

}  // namespace dvp
