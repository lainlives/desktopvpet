// platform_clickthrough.hpp: per-platform click-through and input regions.
#pragma once

#include <SDL3/SDL.h>

namespace dvp::platform {

// How click routing / window shaping is handled on the current backend.
enum class InputMode {
    Native,    // X11 / Win32 native input region (click-through, no visual mask)
    Wayland,   // Wayland wl_surface input region (per-pixel click-through)
    ShapeMask, // SDL renderer shape mask (macOS, or Wayland with DVP_SHAPE=1)
    None,      // rely on the transparent window; whole window receives input
};

// Inspected once after the window is created. `DVP_SHAPE=1` forces ShapeMask.
InputMode detect_input_mode(SDL_Window *window);

// Applies a window silhouette so that alpha-transparent pixels pass mouse
// clicks through to the desktop (and, where the platform supports it, shape
// the window visually). `shape` is an ARGB32 surface the size of the window;
// `content` bounds the region that was actually drawn (the rest is assumed
// transparent) so platforms only need to scan that area.
//
// Returns true when the platform applied a native input region (X11/Win32).
// A false return means SDL's renderer-side shape mask is in use (Wayland /
// macOS), which is more expensive to update.
bool apply_window_shape(SDL_Window *window, SDL_Surface *shape, const SDL_Rect &content);

// Removes any previously applied shape/input region.
void clear_window_shape(SDL_Window *window);

// Pushes the window to the bottom of the stacking order (best effort).
void send_to_bottom(SDL_Window *window);

}  // namespace dvp::platform
