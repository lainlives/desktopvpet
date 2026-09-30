# DesktopEcto (C++/SDL3 port)

A port of the Godot/C# "Desktop Buddy" pet (`DesktopEcto/`) to C++ with SDL3.
The pet is a transparent, borderless, always-on-top overlay that walks, dances,
gets dragged and occasionally talks.

This repository is **work in progress**. The current milestone is a working
framework: window/transparency, data-driven sprite-sheet animation, a
character-swap system, a lightweight DIY physics/state machine and a
click-through hook. See [Status](#status--roadmap) for what is not done yet.

## Layout

```
CMakeLists.txt          # builds vendored SDL3 + SDL3_image, then desktop_ecto
src/
  main.cpp              # arg parsing, app lifetime
  app.{hpp,cpp}         # SDL init, main loop, event handling, render, silhouette
  window.{hpp,cpp}      # transparent/borderless window + renderer, SDL_SetWindowShape
  platform_clickthrough.{hpp,cpp}  # native input regions (X11 / Win32 / macOS)
  config.{hpp,cpp}      # tiny dependency-free JSON reader (character manifests)
  texture.{hpp,cpp}     # RAII SDL_Texture + CPU surface for alpha hit-tests
  animation.hpp         # Animation clip (source image + frame rects + fps + loop)
  animator.{hpp,cpp}    # playback
  character.{hpp,cpp}   # manifest -> animation set; swappable per folder
  pet.{hpp,cpp}         # state machine + DIY physics (gravity, floor, righting)
  dialogue.{hpp,cpp}    # speech-bubble fade state machine + panel
  menu.{hpp,cpp}        # popup menu (Exit + Force-sort submenu)
  text_renderer.{hpp,cpp}  # SDL_ttf text with a small texture cache
  log.hpp
assets/characters/ecto/ # runtime character (generated sheets + character.json)
assets/fonts/           # bundled DejaVuSans.ttf for dialogue/menu text
tools/pack_sprites.py   # packs source frames into one sheet per activity
DesktopEcto/            # original Godot project (reference, untouched)
sdl/SDL, sdl/SDL_image, sdl/SDL_ttf   # git submodules
```

## Prerequisites

- CMake >= 3.20, a C++17 compiler, Ninja (recommended).
- Linux: X11/Wayland + OpenGL dev libraries (SDL detects these).
- FreeType + HarfBuzz dev packages (SDL_ttf uses the system copies; on
  Debian/Ubuntu `libfreetype-dev libharfbuzz-dev`, on Fedora
  `freetype-devel harfbuzz-devel`).
- `wayland-client` dev files (optional; enables Wayland per-pixel
  click-through — Debian/Ubuntu `libwayland-dev`, Fedora `wayland-devel`).
- Initialize submodules, including SDL_image's bundled codecs:

```sh
git submodule update --init --recursive
git -C sdl/SDL_image submodule update --init --depth 1 external/zlib external/libpng
```

(Only PNG/JPEG-class images are needed; the heavy vendored codecs — AVIF, JXL,
WebP, TIFF — are disabled in `CMakeLists.txt`. SDL_ttf is built against the
system FreeType/HarfBuzz, so its large vendored submodules are not fetched.)

## Build

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

The output is self-contained in `build/bin/`:

```
build/bin/desktop_ecto    # single binary (SDL statically linked by default)
build/bin/assets/...
```

Useful options:

| Option | Default | Meaning |
| --- | --- | --- |
| `-DDESKTOPECTO_USE_SYSTEM_SDL=ON` | `OFF` | Link system SDL3/SDL3_image instead of the submodules |
| `-DDESKTOPECTO_SDL_SHARED=ON` | `OFF` | Build/link SDL as shared libraries (staged next to the exe) |
| `-DDESKTOPECTO_COPY_ASSETS=OFF` | `ON` | Don't copy `assets/` next to the executable |

### Windows (MinGW-w64 cross build)

```sh
git -C sdl/SDL_ttf submodule update --init --depth 1 external/freetype
cmake -S . -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
  -DDESKTOPECTO_SDL_SHARED=ON
cmake --build build-mingw
# -> build-mingw/bin/desktop_ecto.exe + SDL3*.dll + assets/
```

The MinGW C/C++ runtime is linked statically (`-static-libgcc -static-libstdc++
-static`), so only the three SDL DLLs travel with the executable. Cross builds
use SDL_ttf's vendored FreeType (see the extra submodule step). Native MSVC or
MinGW builds work from the same project; the Win32 click-through uses
`SetWindowRgn` and "send to bottom" uses `SetWindowPos(HWND_BOTTOM)`.

The Windows binary has been smoke-tested under Wine (`DE_SELFTEST=1` renders the
pet, dialogue text and menu).

## Run

```sh
./build/bin/desktop_ecto [asset_root] [character]
# defaults: asset_root=auto-detected, character=ecto
```

- Left-drag the pet; **right-click** the pet for the menu (Exit / Force sort →
  Disabled / Top / Bottom); `Esc` closes the menu or quits; `T` toggles
  always-on-top; `Ctrl+C` quits.
- Environment:
  - `SDL_VIDEODRIVER=x11` (or `wayland`) to force a backend.
  - `DE_NO_SHAPE=1` disables silhouette/click-through updates.
  - `DE_SHAPE=1` forces the SDL renderer shape-mask path (see below).
  - `DE_ANIM=<name>` forces a single animation (e.g. `dancydance`) for testing.
  - `DE_DEBUG_SHAPE=1` logs each silhouette update (anim/frame/position).
  - `DE_DEBUG_ROLE=1` logs every semantic role -> clip change.
  - `DE_TALK=1` keeps a dialogue bubble active (to exercise the talk role).
  - `DE_SELFTEST=1` renders one frame offscreen, logs the opaque pixel count and exits.
  - `DE_EXIT_AFTER=<seconds>` quits automatically (used by tests).

## Characters & sprite sheets

A character is a folder (`assets/characters/<name>/`) containing
`character.json` and one or more sheets. **Each animation can point at its own
sheet** ("one sheet per activity"), share an atlas, or use loose images. Because
the runtime auto-detects a horizontal strip (frame height = image height,
`frames = width / frame_height`), a manifest entry can be as small as:

```json
{ "animations": {
    "idle":       { "image": "ecto_idle.png",       "fps": 5,  "loop": true },
    "dancydance": { "image": "ecto_dancydance.png", "fps": 10, "loop": true }
} }
```

Manifest schema (all optional except `animations`):

| Key | Scope | Meaning |
| --- | --- | --- |
| `name` | top | Display name |
| `frame_width`, `frame_height` | top / per animation | Frame size; defaults to image height (square) |
| `sheet` | top | Default image for animations that omit `image` |
| `dialogue` | top | Array of speech-bubble strings |
| `font`, `font_size` | top | Font path relative to the asset root and point size (default `fonts/DejaVuSans.ttf`, 15) |
| `animations.<name>.image` | per animation | Sheet/filename for this activity |
| `animations.<name>.fps`, `.loop` | per animation | Playback |
| `animations.<name>.frames` | per animation | Number (count, laid out from `row`/`col`) or an array of `{x,y,w,h}` rects |
| `animations.<name>.row`, `.col` | per animation | Grid origin when `frames` is a count |

To author a character, edit the loose frames in `src/`, update `sources.json`,
then regenerate the sheets and manifest:

```sh
python3 tools/pack_sprites.py assets/characters/ecto
```

`sources.json` per animation supports `images` (one file per frame), `atlas`
(`cols`/`frames`/`start` grid crop) or `rects`. It is copied verbatim (minus the
authoring keys) into `character.json`, so `dialogue`, `font`, `font_size` and
`actions` live there too.

### Animation roles (`actions`)

It never hard-codes clip names; it asks for a **role**. The `actions`
object maps each role to one or more clips:

| Role | When it plays | Fallback when unset |
| --- | --- | --- |
| `idle` | resting (can have weighted variants) | first clip |
| `walk` | walking | `idle` |
| `clicked` | hovered or dragged | `idle` |
| `fall_impact` | hard landing | `clicked` |
| `spinny` | flung / spinning fast | `clicked` |
| `dance` | dance action | `idle` |
| `talk` | while a dialogue bubble is visible | *none (opt-in)* |

Each role accepts a string, a single `{ "animation": "...", "weight": N }`, or an
array of those (weighted variants). Weights default to 1. To give the pet several
idles where one is rarer:

```json
"actions": {
  "idle": [
    { "animation": "idle",      "weight": 4 },
    { "animation": "idle_look", "weight": 1 }
  ],
  "walk":    { "animation": "walk" },
  "clicked": { "animation": "hover" },
  "talk":    { "animation": "hover" }
}
```

`fall_impact` is intentionally omitted above, so it inherits `clicked`. The talk
role only applies while a bubble is showing and the pet is not being dragged.
If there is no `actions` object at all, roles are derived from the conventional
clip names (`idle`, `walk`, `hover`, `spinny`, `dancydance`).

## Transparency & click-through notes

- The window is created with `SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS |
  SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_MAXIMIZED`; the renderer clears to
  `(0,0,0,0)` and the compositor supplies the alpha.
- At startup the app picks an **input mode** (logged as `click-through mode:`):

  | Backend | Mode | Behaviour |
  | --- | --- | --- |
  | X11 / XWayland | `native input region` | `XShapeCombineRectangles(ShapeInput)` built from the pet/dialogue/menu alpha — full click-through. |
  | Windows | `native input region` | `SetWindowRgn` built from the same alpha. |
  | Wayland | `wayland input region` | A per-pixel `wl_surface_set_input_region` region built from the pet/dialogue/menu alpha — clicks outside pass through and hover still works. Falls back to `transparent window (none)` if `wayland-client` was unavailable at build time. |
  | macOS | `renderer shape mask` | `SDL_SetWindowShape`, which routes input as well. |

- On Wayland SDL's renderer shape mask was found to clip/stale the sprite as it
  animated, so it is **not** used; instead we bind `wl_compositor` from the
  `wl_display`/`wl_surface` SDL exposes and set a per-pixel input region
  (`wl_surface_set_input_region`), which needs `wayland-client`. `DE_SHAPE=1`
  forces the old mask path instead.
- Upstream SDL has since added `SDL_SetWindowMousePassthrough` (on Wayland an
  empty `wl_surface_set_input_region`); it is whole-window only, so it cannot do
  per-pixel click-through — the region approach here is preferred.
- In native/shape modes the silhouette is the union of the pet, the dialogue
  bubble and the open menu. Movement updates are throttled to ~20 Hz; frame and
  menu changes update immediately.
- The popup menu is clamped to the window, and its submenu flips left/up when it
  would run off an edge.

## Status & roadmap

Done:
- CMake build of vendored SDL3 + SDL3_image; self-contained output.
- Transparent overlay window, renderer, VSync loop.
- JSON-driven animation system, character swap by folder, sheet packer.
- Pet state machine (`idle/hover/spinny/dragged/walking/dancydance`) with DIY
  gravity/floor/wall collision, drag-fling and upright righting.
- Silhouette-based click-through (X11/Win32/macOS) and hover hit-testing.
- SDL_ttf dialogue text (wrapped, centred, cached) with a bundled font.
- Right-click popup menu (Exit / Force-sort submenu) with checkmarks.
- Right-click popup menu (Exit / Force-sort submenu) with checkmarks; its input
  region is unioned with the pet's silhouette so it stays clickable.
- Z-order control: Top/Disabled via SDL, Bottom via X11 `XLowerWindow` /
  Win32 `SetWindowPos(HWND_BOTTOM)`.
- Window spans the union of all displays (multi-monitor on X11); on Wayland the
  pet uses the transparent surface plus a per-pixel input region, so animation
  no longer clips to a stale silhouette.
- MinGW-w64 cross build producing a self-contained `.exe` (verified under Wine).
- Headless render self-test (`DE_SELFTEST=1`).

### Known limitations

- **Multi-monitor (Wayland).** The window spans the union of all displays, so
  on X11 the pet can be dragged between monitors. A Wayland surface belongs to a
  single output, so the pet is confined to one monitor there; run with
  `SDL_VIDEODRIVER=x11` (XWayland) to roam across monitors.
- **Wayland click-through** needs `wayland-client` dev files at build time; if
  the app logs `transparent window (none)` the whole window receives input. It
  is also subject to compositor support (some compositors ignore input regions).
- **Menu is mouse-only** (no arrow-key/Enter navigation yet).
- **Spinny/righting tuning** and soft-body deformation are not ported (the
  original shipped a `softbody2d` addon that the C# pet did not use).
- macOS has not been test-built here.
