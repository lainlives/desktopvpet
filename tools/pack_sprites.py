#!/usr/bin/env python3
"""Pack a character's source frames into one horizontal sprite sheet per
animation, and emit the runtime manifest (character.json).

Layout:
    assets/characters/<name>/
        sources.json          # authoring input (edit this)
        src/*.png             # loose frames / atlases (edit these)
        ecto_<activity>.png   # generated sheet per activity
        character.json        # generated runtime manifest

Run:  python3 tools/pack_sprites.py assets/characters/ecto

sources.json schema::

    {
      "name": "Ecto",
      "frame_width": 112,
      "frame_height": 112,
      "dialogue": ["line one", "line two"],
      "animations": {
        "idle":       {"fps": 5,  "loop": true, "images": ["idle.png"]},
        "walk":       {"fps": 2,  "images": ["walk1.png", "walk2.png"]},
        "dancydance": {"fps": 10, "atlas": "test2.png", "cols": 3, "frames": 8}
      }
    }

Per-animation frame sources (pick one):
  * "images": [relative paths]      - each file is one frame
  * "atlas": relative path,
    "cols": int, "frames": int      - row-major grid crop
  * "rects": [{"x","y","w","h"}]    - explicit source rectangles
"""

from __future__ import annotations

import json
import os
import sys

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    sys.exit("Pillow is required: pip install Pillow")


def load_frame(path: str, fw: int, fh: int) -> Image.Image:
    img = Image.open(path).convert("RGBA")
    if img.width != fw or img.height != fh:
        # Normalise to the frame size, anchored at the top-left.
        canvas = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
        canvas.paste(img, (0, 0))
        img = canvas
    return img


def frames_for(spec: dict, src_dir: str, fw: int, fh: int) -> list[Image.Image]:
    if "images" in spec:
        return [load_frame(os.path.join(src_dir, name), fw, fh) for name in spec["images"]]

    if "atlas" in spec:
        atlas = Image.open(os.path.join(src_dir, spec["atlas"])).convert("RGBA")
        cols = int(spec.get("cols", atlas.width // fw) or 1)
        count = int(spec.get("frames", cols))
        start = int(spec.get("start", 0))
        frames = []
        for i in range(start, start + count):
            col = i % cols
            row = i // cols
            box = (col * fw, row * fh, col * fw + fw, row * fh + fh)
            frames.append(atlas.crop(box))
        return frames

    if "rects" in spec:
        atlas = Image.open(os.path.join(src_dir, spec["atlas"])).convert("RGBA")
        frames = []
        for r in spec["rects"]:
            box = (r["x"], r["y"], r["x"] + r.get("w", fw), r["y"] + r.get("h", fh))
            frames.append(atlas.crop(box))
        return frames

    raise ValueError("animation needs one of: images, atlas, rects")


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2

    char_dir = os.path.abspath(sys.argv[1])
    sources_path = os.path.join(char_dir, "sources.json")
    if not os.path.isfile(sources_path):
        print(f"missing {sources_path}")
        return 1

    with open(sources_path, "r", encoding="utf-8") as fh:
        sources = json.load(fh)

    name = sources.get("name", os.path.basename(char_dir))
    fw = int(sources["frame_width"])
    fh = int(sources["frame_height"])
    src_dir = os.path.join(char_dir, sources.get("src_dir", "src"))

    # Forward everything except the authoring-only keys; the runtime manifest
    # therefore carries dialogue, font, actions, etc.
    manifest = {k: v for k, v in sources.items() if k not in ("src_dir", "animations")}
    manifest.setdefault("name", name)
    manifest["frame_width"] = fw
    manifest["frame_height"] = fh
    manifest["animations"] = {}

    for anim_name, spec in sources.get("animations", {}).items():
        frames = frames_for(spec, src_dir, fw, fh)
        if not frames:
            print(f"warning: '{anim_name}' produced no frames, skipping")
            continue

        sheet = Image.new("RGBA", (fw * len(frames), fh), (0, 0, 0, 0))
        for i, frame in enumerate(frames):
            sheet.paste(frame, (i * fw, 0))

        out_name = f"{name.lower()}_{anim_name}.png"
        sheet.save(os.path.join(char_dir, out_name))

        manifest["animations"][anim_name] = {
            "image": out_name,
            "fps": spec.get("fps", 5),
            "loop": spec.get("loop", True),
        }
        print(f"{anim_name}: {len(frames)} frame(s) -> {out_name} "
              f"({sheet.width}x{sheet.height})")

    with open(os.path.join(char_dir, "character.json"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2)
        fh.write("\n")
    print(f"wrote {os.path.join(char_dir, 'character.json')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
