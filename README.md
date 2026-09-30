# DesktopVPet (C++/SDL3 port)

A port of the Godot/C# "Desktop Buddy" pet (`DesktopEcto/`) to C++ with SDL3.
The pet is a transparent, borderless, always-on-top overlay that walks, dances,
gets dragged and occasionally talks.

This repository is **work in progress**. The current milestone is a working
framework: window/transparency, data-driven sprite-sheet animation, a
character-swap system, a lightweight DIY physics/state machine and a
click-through hook. See [Status](#status--roadmap) for what is not done yet.

## Layout

```
CMakeLists.txt          # builds vendored SDL3 + SDL3_image, then desktop_vpet
src/
  main.cpp              # arg parsing, app lifetime
  app.{hpp,cpp}         # SDL init, main loop, event handling, render, silhouette
  window.{hpp,cpp}      # transparent/borderless window + renderer, SDL_SetWindowShape
  platform_clickthrough.{hpp,cpp}  # input regions (X11 / Wayland / Win32 / macOS)
  config.{hpp,cpp}      # tiny dependency-free JSON reader (character manifests)
  texture.{hpp,cpp}     # RAII SDL_Texture + CPU surface for alpha hit-tests
  animation.hpp         # Animation clip (source image + frame rects + fps + loop)
  animator.{hpp,cpp}    # playback
  character.{hpp,cpp}   # manifest -> animation set + semantic roles; swappable
  pet.{hpp,cpp}         # state machine + DIY physics (gravity, floor, righting)
  dialogue.{hpp,cpp}    # speech-bubble fade state machine + panel
  menu.{hpp,cpp}        # popup menu (Exit + Force-sort submenu)
  text_renderer.{hpp,cpp}  # SDL_ttf text with a small texture cache
  log.hpp
assets/characters/ecto/ # runtime character (generated sheets + character.json)
assets/fonts/           # bundled DejaVuSans.ttf for dialogue/menu text
tools/pack_sprites.py   # packs source frames into one sheet per activity
editor/                 # wxWidgets character authoring GUI (separate binary)
docs/CHARACTERS.md      # character file / manifest / role reference
docs/EDITOR.md          # character editor overview
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
- wxWidgets (optional; builds the character editor). Either the system dev
  packages (Debian/Ubuntu `libwxgtk3.2-dev`, Fedora `wxGTK-devel`) **or** the
  vendored `wxWidgets/` submodule:
  `git submodule update --init wxWidgets && git -C wxWidgets submodule update --init --depth 1 3rdparty/pcre 3rdparty/nanosvg`
  (STC is disabled, so its scintilla/lexilla submodules aren't needed).
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
build/bin/desktop_vpet    # single binary (SDL statically linked by default)
build/bin/assets/...
```

Useful options:

| Option | Default | Meaning |
| --- | --- | --- |
| `-DDESKTOPVPET_USE_SYSTEM_SDL=ON` | `OFF` | Link system SDL3/SDL3_image instead of the submodules |
| `-DDESKTOPVPET_SDL_SHARED=ON` | `OFF` | Build/link SDL as shared libraries (staged next to the exe) |
| `-DDESKTOPVPET_COPY_ASSETS=OFF` | `ON` | Don't copy `assets/` next to the executable |

### Windows (MinGW-w64 cross build)

```sh
git -C sdl/SDL_ttf submodule update --init --depth 1 external/freetype
cmake -S . -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
  -DDESKTOPVPET_SDL_SHARED=ON
cmake --build build-mingw
# -> build-mingw/bin/desktop_vpet.exe + SDL3*.dll + assets/
```

The MinGW C/C++ runtime is linked statically (`-static-libgcc -static-libstdc++
-static`), so only the three SDL DLLs travel with the executable. Cross builds
use SDL_ttf's vendored FreeType (see the extra submodule step). Native MSVC or
MinGW builds work from the same project; the Win32 click-through uses
`SetWindowRgn` and "send to bottom" uses `SetWindowPos(HWND_BOTTOM)`.

The Windows binary has been smoke-tested under Wine (`DVP_SELFTEST=1` renders the
pet, dialogue text and menu).

## Character editor

A separate cross-platform GUI (`desktop_vpet_editor`, wxWidgets) authors
character directories: global attributes, dialogue, animation clips, role/weight
mapping, an animated preview with drag-to-slice, and a **Pack Sprites** button
that regenerates the runtime sheets + `character.json`.

```sh
cmake --build build --target desktop_vpet_editor
build/bin/desktop_vpet_editor
```

Options:

| Option | Default | Meaning |
| --- | --- | --- |
| `-DDESKTOPVPET_BUILD_EDITOR` | `ON` native / `OFF` cross | Build the editor |
| `-DDESKTOPVPET_USE_SYSTEM_WX` | `ON` | Prefer system wxWidgets; `OFF` builds the vendored static submodule |

Cross (Windows) builds must opt in with `-DDESKTOPVPET_BUILD_EDITOR=ON`; they use
the vendored wxWidgets, which additionally needs its bundled third-party
submodules (`src/zlib`, `src/png`, `src/expat`, `src/tiff`, `src/jpeg`) and is a
large build. See [`docs/EDITOR.md`](docs/EDITOR.md).

## Run

```sh
./build/bin/desktop_vpet [asset_root] [character]
# defaults: asset_root=auto-detected, character=ecto
```

- Left-drag the pet; **right-click** the pet for the menu (Exit / Force sort →
  Disabled / Top / Bottom); `Esc` closes the menu or quits; `T` toggles
  always-on-top; `Ctrl+C` quits.
- Environment:
  - `SDL_VIDEODRIVER=x11` (or `wayland`) to force a backend.
  - `DVP_NO_SHAPE=1` disables silhouette/click-through updates.
  - `DVP_SHAPE=1` forces the SDL renderer shape-mask path (see below).
  - `DVP_ANIM=<name>` forces a single animation (e.g. `dancydance`) for testing.
  - `DVP_DEBUG_SHAPE=1` logs each silhouette update (anim/frame/position).
  - `DVP_DEBUG_ROLE=1` logs every semantic role -> clip change.
  - `DVP_TALK=1` keeps a dialogue bubble active (to exercise the talk role).
  - `DVP_ALL=1` activates every detected character at startup.
  - `DVP_SELFTEST=1` renders one frame offscreen, logs the opaque pixel count and exits.
  - `DVP_EXIT_AFTER=<seconds>` quits automatically (used by tests).

## Characters & sprite sheets

A character is a folder (`assets/characters/<name>/`) containing `character.json`
and one or more sheets. Animations can each point at their own sheet (one sheet
per activity), share an atlas, or use loose images, and an `actions` block maps
semantic roles (`idle`, `walk`, `clicked`, `fall_impact`, `spinny`, `dance`,
`talk`) to clips with weighted variants and sensible fallbacks.

**Multiple characters:** every folder under `assets/characters/` with a
`character.json` is detected at startup. If more than one is found, the
right-click menu gains a **Characters** submenu that shows/hides each pet
(multi-select). The initial character is the `[character]` argument (default the
first alphabetically); the window title becomes `Desktop <name>` (or the list of
active names).

**See [`docs/CHARACTERS.md`](docs/CHARACTERS.md) for the full format, the role
table, the authoring workflow (`sources.json` + `tools/pack_sprites.py`) and an
example.** Quick reference:

```json
{
  "name": "Ecto",
  "frame_width": 112, "frame_height": 112,
  "dialogue": ["im normal im normal im normal"],
  "actions": {
    "idle": [ { "animation": "idle", "weight": 4 },
              { "animation": "idle_look", "weight": 1 } ],
    "walk": { "animation": "walk" },
    "clicked": { "animation": "hover" },
    "talk": { "animation": "hover" }
  },
  "animations": {
    "idle":       { "image": "ecto_idle.png",       "fps": 5 },
    "dancydance": { "image": "ecto_dancydance.png", "fps": 10 }
  }
}
```

Regenerate a character's sheets and manifest with:

```sh
python3 tools/pack_sprites.py assets/characters/ecto
```

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
  (`wl_surface_set_input_region`), which needs `wayland-client`. `DVP_SHAPE=1`
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
- Right-click popup menu (Exit / Force-sort / Characters submenu) with
  checkmarks; its input region is unioned with the pets' silhouettes.
- Multiple characters: every `assets/characters/*/character.json` is loaded and
  can be shown/hidden from the Characters submenu; the input region and rendering
  cover all active pets.
- Z-order control: Top/Disabled via SDL, Bottom via X11 `XLowerWindow` /
  Win32 `SetWindowPos(HWND_BOTTOM)`.
- Window spans the union of all displays (multi-monitor on X11); on Wayland the
  pet uses the transparent surface plus a per-pixel input region, so animation
  no longer clips to a stale silhouette.
- MinGW-w64 cross build producing a self-contained `.exe` (verified under Wine).
- Headless render self-test (`DVP_SELFTEST=1`).

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
