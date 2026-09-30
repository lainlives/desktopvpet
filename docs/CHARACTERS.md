# Character files

A **character** is a self-contained folder under `assets/characters/<name>/`.
Swapping characters is just a matter of pointing the app at a different folder:

```sh
build/bin/desktop_vpet assets/characters/ecto
```

Layout of a character folder:

```
assets/characters/ecto/
  character.json          # runtime manifest (read by the app)
  ecto_<activity>.png     # one sprite sheet per activity (generated)
  sources.json            # authoring input (edit this)
  src/                    # loose source frames / atlases (edit these)
```

`character.json` is generated from `sources.json` by `tools/pack_sprites.py`
(see [Authoring](#authoring-sourcesjson--packer)); you normally edit the sources
and regenerate rather than hand-editing the manifest.

## `character.json`

| Key | Scope | Meaning |
| --- | --- | --- |
| `name` | top | Display name; used for the window title (`Desktop <name>`) |
| `frame_width`, `frame_height` | top / per animation | Frame size in pixels. Defaults to the image height (square frames) |
| `sheet` | top | Default image for animations that omit `image` |
| `dialogue` | top | Array of speech-bubble strings |
| `font` | top | Font path relative to the asset root (default `fonts/DejaVuSans.ttf`) |
| `font_size` | top | Point size (default 15) |
| `actions` | top | Maps semantic [roles](#animation-roles-actions) to clips |
| `animations` | top | **Required.** Map of clip name -> clip definition |

### Animations

Each entry in `animations` describes one clip. Because the runtime auto-detects a
horizontal strip (frame height = image height, `frames = image width / frame
height`), a minimal entry is just an image and playback settings:

```json
{
  "animations": {
    "idle":       { "image": "ecto_idle.png",       "fps": 5,  "loop": true },
    "dancydance": { "image": "ecto_dancydance.png", "fps": 10, "loop": true }
  }
}
```

Per-clip keys:

| Key | Meaning |
| --- | --- |
| `image` | Sheet/filename for this clip (relative to the character folder) |
| `fps` | Playback speed (default 5) |
| `loop` | Loop the clip (default `true`) |
| `frame_width`, `frame_height` | Override the frame size for this clip |
| `frames` | A count (laid out from `row`/`col`) **or** an array of `{x,y,w,h}` rects |
| `row`, `col` | Grid origin when `frames` is a count |

The sheet can be shared between clips (a common atlas) or split per activity.
Explicit rects are useful for irregular atlases:

```json
"blink": { "image": "atlas.png", "fps": 8,
           "frames": [ {"x":0,"y":0,"w":112,"h":112},
                       {"x":112,"y":0,"w":112,"h":112} ] }
```

### Animation roles (`actions`)

Gameplay never hard-codes clip names; it asks for a **role**. `actions` maps each
role to one or more clips:

| Role | Plays when | Fallback when unset |
| --- | --- | --- |
| `idle` | resting (may have weighted variants) | first clip |
| `walk` | walking | `idle` |
| `clicked` | hovered or dragged | `idle` |
| `fall_impact` | hard landing | `clicked` |
| `spinny` | flung / spinning fast | `clicked` |
| `dance` | dance action | `idle` |
| `talk` | while a dialogue bubble is visible | *none — opt-in* |

A role accepts a string, a single object, or an array of them for **weighted
variants** (`weight` defaults to 1). This makes one idle rarer than another:

```json
"actions": {
  "idle": [
    { "animation": "idle",      "weight": 4 },
    { "animation": "idle_look", "weight": 1 }
  ],
  "walk":    { "animation": "walk" },
  "clicked": { "animation": "hover" },
  "spinny":  { "animation": "spinny" },
  "dance":   { "animation": "dancydance" },
  "talk":    { "animation": "hover" }
}
```

Notes:

- `fall_impact` is omitted in the example above, so it inherits `clicked`.
- A role re-rolls its weighted variant when it becomes active (e.g. each time the
  pet returns to idle).
- `talk` only applies while a bubble is visible and the pet is not being dragged.
- Clip names referenced by a role must exist in `animations`; missing ones are
  dropped with a warning.
- If `actions` is absent entirely, roles are derived from conventional clip names
  (`idle`, `walk`, `hover`, `spinny`, `dancydance`), so simple characters need no
  `actions` block.

Add `"talk"` only if you want a dedicated talking pose; otherwise the bubble
appears without changing the animation.

## Authoring: `sources.json` + packer

`sources.json` is the authoring file. It holds the real loose frames in `src/`
and is compiled into horizontal sheets plus `character.json`:

```sh
python3 tools/pack_sprites.py assets/characters/ecto
```

Everything in `sources.json` except the authoring-only keys (`src_dir`,
`animations`) is copied verbatim into `character.json`, so `dialogue`, `font`,
`font_size` and `actions` are authored there too.

Each animation in `sources.json` produces one sheet (`<name>_<activity>.png`) and
supports one of:

| Key | Meaning |
| --- | --- |
| `images` | List of files, one frame each (relative to `src_dir`) |
| `atlas` + `cols` + `frames` (+ `start`) | Row-major grid crop from a single sheet |
| `rects` + `atlas` | Explicit `{x,y,w,h}` source rectangles |

Example:

```json
{
  "name": "Ecto",
  "frame_width": 112,
  "frame_height": 112,
  "src_dir": "src",
  "dialogue": ["im normal im normal im normal", "*explodes u*"],
  "actions": {
    "idle": [ { "animation": "idle", "weight": 4 },
              { "animation": "idle_look", "weight": 1 } ],
    "walk": { "animation": "walk" },
    "clicked": { "animation": "hover" },
    "talk": { "animation": "hover" }
  },
  "animations": {
    "idle":       { "fps": 5,  "images": ["idle.png"] },
    "idle_look":  { "fps": 2,  "images": ["walk1.png"] },
    "hover":      { "fps": 5,  "images": ["shocked.png"] },
    "spinny":     { "fps": 5,  "images": ["spinny1.png"] },
    "walk":       { "fps": 2,  "images": ["walk1.png", "walk2.png"] },
    "dancydance": { "fps": 10, "atlas": "test2.png", "cols": 3, "frames": 8 }
  }
}
```

## Adding a new character

1. `mkdir -p assets/characters/<name>/src` and drop your frames there.
2. Copy `sources.json` from an existing character and edit the animation entries.
3. Run `python3 tools/pack_sprites.py assets/characters/<name>`.
4. Launch: `build/bin/desktop_vpet assets/characters/<name>`.

The window title becomes `Desktop <name>` using the manifest's `name`.
