// platform_clickthrough.cpp: X11 / Wayland / Win32 / macOS input region handling.
#include "platform_clickthrough.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "log.hpp"

#if defined(__linux__)
#include <dlfcn.h>
#endif

#if defined(DVP_HAVE_WAYLAND)
#include <wayland-client.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace dvp::platform {

namespace {

// Merges horizontal runs of opaque pixels into rectangles. Only the `content`
// bounds are scanned.
std::vector<SDL_Rect> opaque_runs(SDL_Surface *shape, const SDL_Rect &content) {
    std::vector<SDL_Rect> rects;
    if (!shape || shape->format != SDL_PIXELFORMAT_ARGB32) {
        return rects;
    }

    const int x0 = SDL_max(0, content.x);
    const int y0 = SDL_max(0, content.y);
    const int x1 = SDL_min(shape->w, content.x + content.w);
    const int y1 = SDL_min(shape->h, content.y + content.h);
    if (x0 >= x1 || y0 >= y1) {
        return rects;
    }

    const auto *pixels = static_cast<const Uint8 *>(shape->pixels);
    const int pitch = shape->pitch;

    for (int y = y0; y < y1; ++y) {
        const Uint8 *row = pixels + y * pitch;
        int run_start = -1;
        for (int x = x0; x < x1; ++x) {
            const Uint8 alpha = row[x * 4 + 3];
            const bool opaque = alpha > 127;
            if (opaque && run_start < 0) {
                run_start = x;
            } else if (!opaque && run_start >= 0) {
                rects.push_back(SDL_Rect{run_start, y, x - run_start, 1});
                run_start = -1;
            }
        }
        if (run_start >= 0) {
            rects.push_back(SDL_Rect{run_start, y, x1 - run_start, 1});
        }
    }
    return rects;
}

#if defined(__linux__)
using DisplayPtr = void *;
using XWindowId = unsigned long;
struct XRectangle {
    short x;
    short y;
    unsigned short width;
    unsigned short height;
};
using XShapeCombineRectanglesFn = void (*)(DisplayPtr, XWindowId, int, int, int,
                                           XRectangle *, int, int, int);
using XShapeCombineMaskFn = void (*)(DisplayPtr, XWindowId, int, int, int,
                                     unsigned long, int);
using XSyncFn = int (*)(DisplayPtr, int);
using XLowerWindowFn = void (*)(DisplayPtr, XWindowId);

constexpr int kShapeInput = 2;
constexpr int kShapeSet = 0;
constexpr int kUnsorted = 0;

struct X11Api {
    DisplayPtr display = nullptr;
    XWindowId window = 0;
    XShapeCombineRectanglesFn combine_rectangles = nullptr;
    XSyncFn sync = nullptr;
    XLowerWindowFn lower_window = nullptr;
};

bool load_x11(SDL_Window *window, X11Api *out) {
    out->display = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                          SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    out->window = static_cast<XWindowId>(
        SDL_GetNumberProperty(SDL_GetWindowProperties(window),
                              SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    if (!out->display || !out->window) {
        return false;
    }

    void *xext = dlopen("libXext.so.6", RTLD_NOW | RTLD_GLOBAL);
    if (!xext) {
        xext = dlopen("libXext.so", RTLD_NOW | RTLD_GLOBAL);
    }
    void *x11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
    if (!x11) {
        x11 = dlopen("libX11.so", RTLD_NOW | RTLD_GLOBAL);
    }
    if (!xext || !x11) {
        return false;
    }
    out->combine_rectangles = reinterpret_cast<XShapeCombineRectanglesFn>(
        dlsym(xext, "XShapeCombineRectangles"));
    out->sync = reinterpret_cast<XSyncFn>(dlsym(x11, "XSync"));
    out->lower_window = reinterpret_cast<XLowerWindowFn>(dlsym(x11, "XLowerWindow"));
    return out->combine_rectangles != nullptr;
}
// ---- Wayland input region ---------------------------------------------
// The vendored SDL has no SDL_SetWindowMousePassthrough, and that API is
// whole-window only anyway. SDL does expose the wl_display/wl_surface, so bind
// wl_compositor ourselves and set a per-pixel input region: clicks outside the
// pet pass through while the compositor still delivers pointer events over it
// (so hover keeps working). Requires wayland-client at build time.
#if defined(DVP_HAVE_WAYLAND)
wl_display *g_wl_display = nullptr;
wl_surface *g_wl_surface = nullptr;
wl_compositor *g_wl_compositor = nullptr;
bool g_wl_ready = false;
bool g_wl_loaded = false;

void wl_registry_global(void *, wl_registry *registry, uint32_t name, const char *interface,
                        uint32_t version) {
    if (g_wl_compositor) {
        return;
    }
    if (interface && std::strcmp(interface, wl_compositor_interface.name) == 0) {
        const uint32_t v = version < 4 ? version : 4;
        g_wl_compositor = static_cast<wl_compositor *>(
            wl_registry_bind(registry, name, &wl_compositor_interface, v));
    }
}

void wl_registry_global_remove(void *, wl_registry *, uint32_t) {}

const wl_registry_listener g_wl_listener{&wl_registry_global, &wl_registry_global_remove};

bool wayland_init(SDL_Window *window) {
    if (g_wl_loaded) {
        return g_wl_ready;
    }
    g_wl_loaded = true;
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    g_wl_display = static_cast<wl_display *>(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
    g_wl_surface = static_cast<wl_surface *>(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
    if (!g_wl_display || !g_wl_surface) {
        return false;
    }
    wl_registry *registry = wl_display_get_registry(g_wl_display);
    if (!registry) {
        return false;
    }
    wl_registry_add_listener(registry, &g_wl_listener, nullptr);
    wl_display_roundtrip(g_wl_display);
    g_wl_ready = g_wl_compositor != nullptr;
    return g_wl_ready;
}

bool wayland_set_input_region(const std::vector<SDL_Rect> &rects) {
    if (!g_wl_ready) {
        return false;
    }
    wl_region *region = wl_compositor_create_region(g_wl_compositor);
    if (!region) {
        return false;
    }
    for (const SDL_Rect &r : rects) {
        if (r.w > 0 && r.h > 0) {
            wl_region_add(region, r.x, r.y, r.w, r.h);
        }
    }
    // Applied by the surface at its next commit (SDL presents every frame).
    wl_surface_set_input_region(g_wl_surface, region);
    wl_region_destroy(region);
    return true;
}

void wayland_clear_input_region() {
    if (g_wl_ready) {
        wl_surface_set_input_region(g_wl_surface, nullptr);
    }
}
#else
bool wayland_init(SDL_Window *) { return false; }
bool wayland_set_input_region(const std::vector<SDL_Rect> &) { return false; }
void wayland_clear_input_region() {}
#endif  // DVP_HAVE_WAYLAND

#endif  // __linux__

}  // namespace

InputMode detect_input_mode(SDL_Window *window) {
#if defined(_WIN32)
    (void)window;
    return InputMode::Native;
#elif defined(__APPLE__)
    (void)window;
    return InputMode::ShapeMask;
#elif defined(__linux__)
    if (std::getenv("DVP_SHAPE")) {
        return InputMode::ShapeMask;
    }
    X11Api api;
    if (window && load_x11(window, &api)) {
        return InputMode::Native;
    }
    if (window && wayland_init(window)) {
        return InputMode::Wayland;
    }
    return InputMode::None;
#else
    (void)window;
    return InputMode::None;
#endif
}

bool apply_window_shape(SDL_Window *window, SDL_Surface *shape, const SDL_Rect &content) {
    if (!window || !shape) {
        return false;
    }

#if defined(__APPLE__)
    // SDL handles both visuals and input routing on macOS from the shape
    // surface (the renderer also masks by it).
    return SDL_SetWindowShape(window, shape);
#elif defined(_WIN32)
    HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) {
        return false;
    }
    const std::vector<SDL_Rect> rects = opaque_runs(shape, content);
    HRGN region = CreateRectRgn(0, 0, 0, 0);
    if (!region) {
        return false;
    }
    for (const SDL_Rect &r : rects) {
        HRGN part = CreateRectRgn(r.x, r.y, r.x + r.w, r.y + r.h);
        CombineRgn(region, region, part, RGN_OR);
        DeleteObject(part);
    }
    SetWindowRgn(hwnd, region, TRUE);  // ownership transferred to the window
    return true;
#elif defined(__linux__)
    X11Api api;
    if (load_x11(window, &api)) {
        const std::vector<SDL_Rect> rects = opaque_runs(shape, content);
        std::vector<XRectangle> xrects;
        xrects.reserve(rects.size());
        for (const SDL_Rect &r : rects) {
            xrects.push_back(XRectangle{static_cast<short>(r.x), static_cast<short>(r.y),
                                        static_cast<unsigned short>(r.w),
                                        static_cast<unsigned short>(r.h)});
        }
        api.combine_rectangles(api.display, api.window, kShapeInput, 0, 0,
                               xrects.empty() ? nullptr : xrects.data(),
                               static_cast<int>(xrects.size()), kShapeSet, kUnsorted);
        if (api.sync) {
            api.sync(api.display, 0);
        }
        return true;
    }
    if (wayland_set_input_region(opaque_runs(shape, content))) {
        return true;
    }
    // ShapeMask mode (macOS, or Wayland with DVP_SHAPE=1): let SDL mask the
    // output. App composites the pet + dialogue + menu into `shape`.
    SDL_SetWindowShape(window, shape);
    return false;
#else
    return SDL_SetWindowShape(window, shape);
#endif
}

void send_to_bottom(SDL_Window *window) {
    if (!window) {
        return;
    }
#if defined(_WIN32)
    HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (hwnd) {
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
#elif defined(__linux__)
    X11Api api;
    if (load_x11(window, &api) && api.lower_window) {
        api.lower_window(api.display, api.window);
        if (api.sync) {
            api.sync(api.display, 0);
        }
    }
#else
    SDL_SetWindowAlwaysOnTop(window, false);
#endif
}

void clear_window_shape(SDL_Window *window) {
    if (!window) {
        return;
    }
#if defined(__linux__)
    X11Api api;
    if (load_x11(window, &api)) {
        api.combine_rectangles(api.display, api.window, kShapeInput, 0, 0, nullptr, 0,
                               kShapeSet, kUnsorted);
        if (api.sync) {
            api.sync(api.display, 0);
        }
    }
    wayland_clear_input_region();
#endif
    SDL_SetWindowShape(window, nullptr);
}

}  // namespace dvp::platform
