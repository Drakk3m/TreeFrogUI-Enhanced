# Enhanced theme resources, version 1

P3 adds an optional external theme for the existing Enhanced grid. The live effects are platform icons and the selected platform or shared Home backdrop. The navigation model keeps platform and game identity and has no dependency on filenames, colors, image formats, sound or the active theme. Home, CoverFlow, Details, Gallery and the future Settings view keep their existing P2 scope.

## Audit and integration

| Existing mechanism | P3 decision |
| --- | --- |
| `settings_load_file`, `settings_save_file`, `settings_apply_style_defaults` and the legacy `render_theme_load` | Keep the existing settings file and renderer. Enhanced resources use a separate namespace inside `frogui/` alongside the existing `schemes/`, `theme-packs/`, `icon-packs/`, `system-icons/`, `fonts/` and `lang/`. The current theme still determines layout, colors and fonts. |
| `system_icon_path`, `system_icon_load` and `system_icon_draw` | Resolve Enhanced logical IDs before the existing path fallback. Use the same RGBA-to-RGB565 conversion and alpha blend into the current frame. The existing 12-slot decoded LRU owns pixels. The eight-item Enhanced page holds borrowed pointers prepared outside drawing on a page, view, listing, theme or icon-pack change. |
| `load_banner_for_view`, `banner_queue_image` and `banner_render` | Honor the user's background toggle and fixed wallpaper first. Use the same decoder and row-batched RGB565 conversion for Enhanced assets. A shared backdrop is reused when successive selections resolve to the same active resource. Custom backdrops use the active and pending buffers, with no extra decoded ring or disk cache. Legacy fallbacks retain their normal behavior. |
| `load_image_thumbnail` and the existing box-art path | The legacy loader retains its 250×250 RGB565 and alpha buffers. P3 reserves placeholder IDs. CoverFlow and per-game media remain consumers for a later phase. |
| `system_carousel_start`, `system_carousel_ease` and `render_system_carousel` | Preserve the legacy 12-rendered-frame smoothstep and its carry-forward offset. Future Home can adapt the interpolation to P2's noncircular window and consume duration and reduced-motion settings here. P3 launches no new animation or audio engine. |
| `tr`, `tr_or` and `lang/builtin/` | Existing localization and fallback continue to own UI text. A manifest name is optional theme metadata and plays no role in navigation or localized UI lookup. |

The old loaders accept conventional filenames, but lack versioning, logical IDs, role-specific bounds and package containment. The manager therefore owns **descriptors and validation**, while native FrogUI loaders continue to own decoded pixels. Ordinary P3 paint and idle work do not invoke the manager or allocate another image. Existing legacy screens can still perform the I/O they performed before P3.

## Installation and export contract

Choose a directory under `/mnt/sdcard/frogui/enhanced/themes/` and place `theme.ini` at its root. The folder selector uses lowercase ASCII letters, digits, underscores and hyphens, up to 63 bytes. The default folder is `default`. With Enhanced Home on, an optional `enhanced_theme=<folder>` in the existing `frogui/settings.txt` selects another package on next start. `enhanced_theme=none` keeps the Enhanced layout with native resources. Enhanced Home off gives the original renderer and resources.

Example for a 640×480 theme:

```ini
[theme]
manifest_version=1
id=default
name=Sample theme
screen_width=640
screen_height=480

[presentation]
transition_ms=180
icon_scale_percent=115
icon_spacing=8
background_crossfade=off
coverflow_neighbors=1
coverflow_side_scale_percent=75
coverflow_spacing=8
reduced_motion=off
navigation_sounds=off

[resource.home.background]
path=art/home.jpg

[resource.platform.nes.background]
path=art/nes.jpg

[resource.platform.nes.icon]
path=icons/nes.png

[resource.settings.appearance.icon]
path=icons/appearance.png

[resource.cover.placeholder]
path=icons/no-cover.png

[resource.achievement.badge]
path=icons/no-badge.png

[resource.navigation.sound]
path=sounds/nav.wav
```

- Use UTF-8 text without a BOM. Each setting is `key=value`. Whole-line comments start with `#` or `;`. The manifest is at most 16 KiB, with up to 128 recognized mappings. `manifest_version=1` is required. ID and name are optional metadata. Provide both screen fields for exact panel matching, or omit both for portable native scaling. A malformed required field or unsupported version prevents activation. Unknown keys and resource IDs in v1 are ignored for forwards compatibility. Use a single `path` per logical ID.
- Paths are relative to the theme directory, up to 255 bytes, with forward slashes. Absolute paths, drive prefixes, backslashes, empty or traversal components, symlink components and nonregular files are rejected. The manager opens assets relative to a held directory descriptor. A failed reload leaves the previous manifest intact. Switching or rebooting starts a fresh generation and discards the previous Enhanced decoded resources.
- Platform keys use the lowercase ROM folder when it is an ASCII token. Folder display labels and physical asset filenames can change independently. The supported IDs are `home.background`, `platform.<key>.background`, `platform.<key>.icon`, `settings.<key>.icon`, `cover.placeholder`, `achievement.badge` and `navigation.sound`.
- Export **8-bit, non-interlaced PNG**, JPEG or BMP. Prefer PNG with alpha for icons, JPEG for opaque backgrounds. Each file is at most **1 MiB**. Use **640×480 or smaller** for backgrounds, **256×256 or smaller** for icons/placeholders and preferably **160×160 or smaller** for platform icons. Dimensions are read from the image, then checked again against the bytes passed to the decoder. The reserved navigation sound accepts a RIFF/WAVE descriptor only. P3 performs no sound playback or audio codec validation.
- Fallback order for backgrounds is platform-specific → shared Home → existing FrogUI background logic → solid background. The explicit wallpaper and disabling backgrounds take priority over Enhanced art. For icons it is Enhanced → native pack → native system icon → initials. Missing or corrupt files leave ordinary platform navigation usable. The future Settings, cover, badge and sound consumers will decide their own native fallback.
- Publish a complete immutable package in a new folder, validate its files and then switch `enhanced_theme` on the test card while the console is off. Keep the prior package and previous selector until the replacement is verified. This supports rollback even when file replacement on FAT32 is interrupted. Avoid editing files under a currently active theme.

| Presentation key | Default | Valid values |
| --- | --- | --- |
| `transition_ms` | 180 | 0–500 ms |
| `icon_scale_percent` | 115 | 100–150% |
| `icon_spacing` | 8 | 0–64 logical px |
| `background_crossfade` | off | on/off |
| `coverflow_neighbors` | 1 | 0–2 per side |
| `coverflow_side_scale_percent` | 75 | 50–100% |
| `coverflow_spacing` | 8 | 0–64 logical px |
| `reduced_motion`, `navigation_sounds` | off | on/off |

These parameters are validated and exposed to future consumers. The existing grid geometry, legacy animation and sound behavior continue in P3. No ROM Studio changes are required for P3. This is a theme-package contract only. Game covers, screenshots, and achievement or badge associations still belong to the future ROM Studio data contract.

## Ownership and memory

The R36SX v2.7 test base `cubegm/dtb.bin` identifies Hichip hc16xx and declares a memory region of `0x0af91e50` bytes, about 175.6 MiB. That is a device-tree declaration. Current available RAM, reservations and target peak RSS were unavailable in the local hardware logs, so the sizes below avoid relying on free-memory estimates.

| Resource | Bound or host observation |
| --- | --- |
| Manifest and resource descriptors | 16 KiB parse buffer. At 128 mappings and 16 LRU entries, the x86-64 sanitized test reported 47,536 heap bytes requested by the manager, excluding allocator overhead. |
| Decoded icon ownership | Existing LRU capacity 12. At 160×160 RGB565 plus alpha, pixel storage is at most 76,800 bytes per entry and 921,600 bytes total. The eight page slots borrow pointers. |
| Custom background ownership | One settled render target and at most one pending RGB565 target, 614,400 bytes each at 640×480. No new decoded cache of full-screen assets. The native legacy cache remains available for legacy artwork. |
| Decode guard | At most 1 MiB encoded input. The existing stb decoder is capped for Enhanced calls at 2 MiB per allocation and 6 MiB cumulative requested per decode, including realloc requests. Hitting either cap uses fallback. The limiter is thread-local and leaves the legacy worker unchanged. |
| Measured load | The host integration test loaded a generated 640×480 BMP through the live path. It recorded 7 malloc calls totaling 2,564,646 bytes, with a largest request of 921,654. It recorded zero malloc calls during native row conversion and reuse of the settled backdrop. These are allocation requests on the fixture, rather than peak RSS or a measurement of every codec. |

The manager owns its directory descriptor, manifest table and metadata cache. The decoded icon LRU releases pixels and alpha on eviction, theme switch or unload. The banner owns a submitted RGB buffer until native conversion completes, and releases its old target when replaced. The caller frees the encoded blob immediately after native decoding. A failed decode leaves ownership with the caller and routes to native fallback.
