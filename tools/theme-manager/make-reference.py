#!/usr/bin/env python3
"""Build deterministic native FrogUI theme folders with original procedural art."""
import argparse
import json
import struct
import zlib
from pathlib import Path


def png(path, width, height, pixel):
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data
                + struct.pack(">I", zlib.crc32(kind + data)))

    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixel(x, y))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
                     + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def write_pack(catalog, folder, minimal):
    folder.mkdir(parents=True, exist_ok=True)
    title = "Enhanced Minimal" if minimal else "Enhanced Reference"
    lines = ["[theme]", "manifest_version=1", f"id={folder.name}",
             f"name={title}", "screen_width=640", "screen_height=480"]
    if not minimal:
        lines += ["", "[presentation]", "transition_ms=180", "icon_scale_percent=130",
                  "coverflow_neighbors=2", "coverflow_side_scale_percent=75",
                  "recents_screenshot_dim=68", "home_selection_opacity=165",
                  "recents_shadow_opacity=180", "coverflow_selection_opacity=165"]
        (folder / "icons").mkdir(exist_ok=True)

        def backdrop(x, y):
            band = (x + y * 2) // 37 % 9
            return 17 + band, 25 + y // 24, 43 + x // 28, 255

        png(folder / "main.png", 640, 480, backdrop)
        for resource in catalog["resources"]:
            example = resource.get("reference")
            if not example:
                continue
            identifier, name, style = example
            lines += ["", f"[resource.{identifier}]", f"path={name}"]
            if name == "main.png":
                continue

            def mark(x, y):
                dx, dy = x - 47, y - 47
                if style == "trophy":
                    rim = 23 <= y <= 45 and abs(dx) < 21 - (y - 23) // 2
                    handles = 26 <= y <= 38 and 19 <= abs(dx) <= 26
                    stem = 40 <= y <= 60 and abs(dx) < 4
                    base = 59 <= y <= 65 and abs(dx) < 15
                    return 244, 200, 96, 255 if rim or handles or stem or base else 0
                if style == "star":
                    star = [(48, 8), (58, 34), (86, 35), (64, 54), (71, 82),
                            (48, 66), (25, 82), (32, 54), (10, 35), (38, 34)]
                    inside = False
                    for (ax, ay), (bx, by) in zip(star, star[1:] + star[:1]):
                        if (ay <= y < by or by <= y < ay) and x < ax + (y-ay)*(bx-ax)/(by-ay):
                            inside = not inside
                    return 250, 201, 98, 255 if inside else 0
                seed = sum(style.encode())
                if style == "usb":
                    shape = (abs(dx) <= 3 and abs(dy) < 25) or (
                        3 < abs(dx) < 22 and abs(dy + abs(dx)//2) < 4)
                    return 160, 222, 255, 255 if shape else 0
                if style == "recents":
                    radius = dx * dx + dy * dy
                    shape = 21**2 < radius < 27**2 or (
                        abs(dx) < 3 and -16 < dy < 5) or (0 <= dx < 14 and abs(dy - dx//2) < 3)
                    return 146, 211, 255, 255 if shape else 0
                if style == "apps":
                    shape = 7 < abs(dx) < 26 and 7 < abs(dy) < 26
                    return 152, 222, 198, 255 if shape else 0
                if style == "settings":
                    radius = dx * dx + dy * dy
                    teeth = min(abs(dx), abs(dy), abs(abs(dx) - abs(dy))) < 6
                    shape = 14**2 < radius < (29 if teeth else 23)**2
                    return 188, 200, 255, 255 if shape else 0
                if style == "cover":
                    shape = abs(dx) < 23 and abs(dy) < 31 and (
                        abs(dx) > 18 or abs(dy) > 26 or (4 < dy < 9 and abs(dx) < 12))
                    return 165, 203, 255, 255 if shape else 0
                alpha = 255 if abs(dx) + abs(dy) < 37 else 0
                return 55 + seed % 100, 138 + seed % 68, 211 + seed % 40, alpha

            png(folder / name, 96, 96, mark)
    (folder / "theme.ini").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    catalog = json.loads(Path(__file__).with_name("catalog.json").read_text(encoding="utf-8"))
    write_pack(catalog, args.output / "reference", False)
    write_pack(catalog, args.output / "minimal", True)


if __name__ == "__main__":
    main()
