# Character editor (wxWidgets)

`desktop_vpet_editor` is a separate, cross-platform GUI for authoring character
directories. It edits `sources.json` (the authoring file) and can invoke the
packer to regenerate the runtime sheets + `character.json`.

```sh
cmake --build build --target desktop_vpet_editor
build/bin/desktop_vpet_editor
```

The editor is built when wxWidgets is found (`-DDESKTOPVPET_BUILD_EDITOR=OFF` to
disable). It shares the project's JSON reader/writer (`src/config.*`) with the
game binary but has no other dependency on it.

## Layout

```
+---------------------------------------------------------------+
| New Character | Open | Save | Pack Sprites |        (toolbar)   |
+----------------+-------------------------+--------------------+
| Global         | Animations | Actions    | Preview | Assets   |
| - name         | [list]     + playback    | (animated canvas)  |
| - frame w/h    | + source mode fields    |                    |
| - font/size    |                         | drag => slice rect |
| - dialogue []  | role -> variants        | DirCtrl(src/)      |
+----------------+-------------------------+--------------------+
```

- **Toolbar**: New Character, Open, Save, Pack Sprites (runs
  `tools/pack_sprites.py` via `python3`).
- **Left pane (Global)**: `name`, `frame_width`, `frame_height`, `font`,
  `font_size`, and a dialogue line manager (add/remove).
- **Center pane**:
  - *Animations*: add/remove clips; per clip `fps`, `loop`, and source mode
    (`images` / `atlas` / `rects`). Images mode manages a list of loose frames;
    atlas mode edits `atlas`, `cols`, `frames`, `start`.
  - *Actions*: map each semantic role (`idle`, `walk`, `clicked`, `fall_impact`,
    `spinny`, `dance`, `talk`) to weighted variants (animation + weight),
    enabling multi-idle weighting.
- **Right pane**:
  - *Preview*: plays the selected clip at its fps. In `rects` mode, **left-drag
    on the canvas** defines an `x,y,w,h` source rectangle and appends it
    (visual slicer).
  - *Assets*: a directory tree rooted at the character's `src/` folder.

## Implemented

- New / Open / Save of `sources.json` (loads `character.json` as a fallback seed).
- All global attributes and the dialogue list.
- Animation clips with the three source modes and their fields.
- Role/variant mapping including weights.
- Animated preview and drag-to-slice rectangles.
- Pack Sprites integration (saves first, then runs the packer and shows its output).

## Roadmap

- Drag-and-drop from the Asset browser into the images list.
- Rect list editing UI (currently rects are appended by slicing and shown via
  save/round-trip; add a list with remove/select).
- Undo/redo and an unsaved-changes prompt on close.
- Loading the atlas image directly into the preview with a live slice overlay
  while dragging.
