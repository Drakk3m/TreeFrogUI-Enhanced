# Enhanced theme resources, version 1

A native FrogUI background pack can also provide resources for the existing Enhanced grid. The live effects are platform icons and the selected platform or shared Home backdrop. The navigation model keeps platform and game identity and has no dependency on filenames, colors, image formats, sound or the active theme. Home, CoverFlow, Details, Gallery and the future Settings view keep their existing P2 scope.

## Audit and integration

| Existing mechanism | P3 decision |
| --- | --- |
| `theme_pack_scan`, `settings_load_file`, `settings_save_file` and `load_banner_for_view` | The existing registry scans directories under `frogui/theme-packs/`. Named packs provide art per system plus a shared `main.*` fallback. Appearance → **Background Theme Pack** selects a folder and persists its name as `theme_pack` in `frogui/settings.txt`. The Default menu entry uses the original flat art in `frogui/`. The legacy reader does not require a manifest or interpret `theme.ini`. |
| `icon_pack_scan` and `system_icon_path` | Icon packs already offer a separate legacy icon override. Missing icons fall back to `system-icons/` and then initials. The optional resource manifest may supply individual Enhanced overrides and reuses this existing chain whenever an override is missing. |
| `render_scheme_scan`, `render_theme_load`, `render_theme_set_screen` | `schemes/<name>.ini` and optional screen INIs under `schemes/<name>/640x480/scheme/` control layout, colors, metrics and scheme artwork. Imported schemes take precedence and disable the Background Theme Pack control. They remain a separate existing renderer configuration. Fonts and localization continue to be managed by their existing selectors and loaders. |
| `system_icon_path`, `system_icon_load` and `system_icon_draw` | Resolve Enhanced logical IDs before the existing path fallback. Use the same RGBA-to-RGB565 conversion and alpha blend into the current frame. The existing 12-slot decoded LRU owns pixels. The eight-item Enhanced page holds borrowed pointers prepared outside drawing on a page, view, listing, theme or icon-pack change. |
| `load_banner_for_view`, `banner_queue_image` and `banner_render` | Honor the user's background toggle and fixed wallpaper first. Use the same decoder and row-batched RGB565 conversion for Enhanced assets. A shared backdrop is reused when successive selections resolve to the same active resource. Custom backdrops use the active and pending buffers, with no extra decoded ring or disk cache. Legacy fallbacks retain their normal behavior. |
| `load_image_thumbnail` and the existing box-art path | The legacy loader retains its 250×250 RGB565 and alpha buffers. P3 reserves placeholder IDs. CoverFlow and per-game media remain consumers for a later phase. |
| `system_carousel_start`, `system_carousel_ease` and `render_system_carousel` | Preserve the legacy 12-rendered-frame smoothstep and its carry-forward offset. Future Home can adapt the interpolation to P2's noncircular window and consume duration and reduced-motion settings here. P3 launches no new animation or audio engine. |
| `tr`, `tr_or` and `lang/builtin/` | Existing localization and fallback continue to own UI text. A manifest name is optional theme metadata and plays no role in navigation or localized UI lookup. |

The background-pack loader accepts conventional filenames and ignores manifest metadata. That makes a pack-local optional manifest a compatible extension. The old loaders lack logical IDs, role-specific bounds and package containment. The manager therefore owns **descriptors and validation**, while native FrogUI loaders continue to own decoded pixels. Ordinary P3 paint and idle work do not invoke the manager or allocate another image. Existing legacy screens can still perform the I/O they performed before P3.

## Installation and export contract

Install a named folder under `/mnt/sdcard/frogui/theme-packs/`. Use the native filenames at its root. If desired, add `theme.ini` at that same root. In **Settings → Appearance → Background Theme Pack**, select the pack once. With the built-in renderer and Enhanced Home active, the optional manifest is loaded for that selection. With Enhanced Home off, the same pack supplies ordinary background art through its existing filenames. With an imported scheme active, its existing background policy wins. The stock Default selection, a pack without a manifest and an incompatible manifest all keep native behavior.

Example for a 640×480 theme:

```ini
[theme]
manifest_version=1
id=consolidation_sample
name=Consolidation Sample
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
path=main.png

[resource.platform.nes.background]
path=nes.png

[resource.platform.nes.icon]
path=icons/nes.png

[resource.settings.appearance.icon]
path=icons/appearance.png

[resource.cover.placeholder]
path=icons/no-cover.png

[resource.achievement.badge]
path=icons/no-badge.png

[resource.ra.trophy]
path=icons/achievements.png

[resource.navigation.move.sound]
path=sounds/move.wav

[resource.navigation.confirm.sound]
path=sounds/confirm.wav
```

- Use UTF-8 text without a BOM. Each setting is `key=value`. Whole-line comments start with `#` or `;`. The manifest is at most 16 KiB, with up to 128 recognized mappings. `manifest_version=1` is required. ID and name are optional metadata. Provide both screen fields for exact panel matching, or omit both for portable native scaling. A malformed required field or unsupported version prevents activation. Unknown keys and resource IDs in v1 are ignored for forwards compatibility. Use a single `path` per logical ID.
- Paths are relative to the theme directory, up to 255 bytes, with forward slashes. Absolute paths, drive prefixes, backslashes, empty or traversal components, symlink components and nonregular files are rejected. The manager opens assets relative to a held directory descriptor. A failed reload leaves the previous manifest intact. Switching or rebooting starts a fresh generation and discards the previous Enhanced decoded resources.
- Platform keys use the lowercase ROM folder when it is an ASCII token. Folder display labels and physical asset filenames can change independently. The supported IDs are `home.background`, `platform.<key>.background`, `platform.<key>.icon`, `settings.<key>.icon`, `cover.placeholder`, `achievement.badge`, `ra.trophy` and the optional navigation sound IDs described below.
- Export **8-bit, non-interlaced PNG**, JPEG or BMP. Prefer PNG with alpha for icons, JPEG for opaque backgrounds. Each file is at most **1 MiB**. Use **640×480 or smaller** for backgrounds, **256×256 or smaller** for icons/placeholders and preferably **160×160 or smaller** for platform icons. Dimensions are read from the image, then checked again against the bytes passed to the decoder.
- Fallback order for backgrounds is platform-specific → shared Home → existing FrogUI background logic → solid background. The explicit wallpaper and disabling backgrounds take priority over Enhanced art. For icons it is Enhanced → native pack → native system icon → initials. Missing or corrupt files leave ordinary platform navigation usable. The future Settings, cover and badge consumers will decide their own native fallback.
- Publish a complete package in a new named pack folder, validate its files and select it using Background Theme Pack. Keep the previous package until the replacement is verified. Avoid replacing assets under an active pack in place. Change selections or restart to reload the cache. Explicit wallpaper still takes priority and the background toggle suppresses Enhanced backdrops.

## SD layout and ownership

In the P3 preview, native packs and Enhanced resources used separate folders and persisted selections:

```text
frogui/
  settings.txt                 theme_pack and enhanced_theme were both active
  theme-packs/<pack>/          native background artwork
  enhanced/themes/<id>/       Enhanced theme.ini and mapped assets
```

The consolidated installation uses:

```text
frogui/
  settings.txt                 user preferences, including the single theme_pack selector
  theme-packs/                 native TreeFrogUI background pack registry
    <pack>/
      main.png                 ordinary native fallback
      nes.png                  ordinary per-system art
      theme.ini                optional Enhanced resources in the same package
      icons/                   optional pack-local files referenced by logical IDs
  icon-packs/                  existing shared native icon sets
  system-icons/                native default icon assets
  schemes/                     existing renderer schemes and their artwork
  fonts/                       existing fonts
  lang/                        localization assets
  .cache/                      generated FrogUI cache
roms/                          user library and per-game media
```

The initial P3 preview used `frogui/enhanced/themes/<id>/theme.ini` and an independent `enhanced_theme` key. Consolidation leaves that directory and setting untouched as migration evidence. No external theme is loaded from that path. No theme is chosen from the old key. An existing legacy key is retained when settings are saved for rollback.

### Manual migration and rollback

1. Back up `frogui/settings.txt` and keep the old `enhanced/themes/` folders. Confirm the desired native Background Theme Pack in Settings.
2. To adopt a preview theme, **copy** its manifest and the referenced relative files into a named folder in `frogui/theme-packs/`. Add or retain `main.*` and per-system files at the root for native use. You can reuse those same files in the manifest. Keep filenames distinct where needed to preserve existing art. If the target has its own manifest, use a new named folder or reconcile manifests manually.
3. Select that folder once under Background Theme Pack. Do not change the old `enhanced_theme` key. The new binary ignores it for selection.
4. Roll back from a powered-off test SD by restoring the original P3 preview binary or the accepted P2 + RA binary. If you want the old settings exactly as before, copy back the settings backup while the console is off. The old folders remain available. No cleanup is necessary for the new implementation.

ROM Studio can export one folder with conventional native artwork and optional v1 manifest resources. This contract neither changes the library metadata format nor requires a separate export destination.

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

## Optional UI sound overrides

FrogUI P4.2 recognizes `navigation.move.sound`, `navigation.select.sound`, `navigation.confirm.sound`, and `navigation.cancel.sound` in the selected Background Theme Pack. `navigation.sound` is a backward-compatible movement alias. Keep `manifest_version=1`. A pack can override any subset, independently of the reserved presentation key `navigation_sounds`. The user preference **UI Sounds**, under General in Enhanced Settings, controls playback. It defaults to off.

Use RIFF/WAVE PCM format tag 1, 44,100 Hz, signed little-endian PCM16 mono. Clips may contain at most 4,410 frames (100 ms). The complete file is capped at 9,332 bytes. FrogUI validates the declared RIFF length, chunk bounds and PCM format, duplicates samples into both Libretro channels, and attenuates overrides by 12 dB before the native system volume path. Use an audible cue of at least about 60 ms for R36SX testing. Invalid or missing clips use the corresponding built-in waveform. Disabled sounds and unavailable output callbacks produce silence.

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

## Application icons

Enhanced Apps accepts optional app.<clave>.icon entries in the existing theme manifest. The key is the stable app_defs ID, such as usbmode, ebook or frogshell. Use theme-local asset paths. For example:

~~~ini
[resource.app.usbmode.icon]
path=icons/usb.png
~~~

PNG alpha and aspect ratio are preserved. Files follow the ordinary icon limits, with decoded artwork retained at up to 160 pixels per edge in the existing icon cache. The Apps view resolves visible artwork outside drawing and uses native fallback symbols for missing or invalid resources. Existing packs need no new files.

## Favorites ribbon icon

Enhanced Home reuses the optional platform.favourites.icon resource for the Favorites destination. It is a virtual destination and has no ROM directory. The resource uses existing icon limits and aspect-fit alpha drawing. If the resource is absent or cannot be decoded, Home uses its native star symbol. The empty collection uses the same native star silhouette and themed text colors. Existing themes need no changes.

## P10 resource resolution

The pack-local manifest remains version 1. The P10 additions are optional. Settings, theme selection, fonts, user colors, imported schemes and native pack artwork keep their existing persistence and precedence. The manager is active only for Enhanced mode and native rendered themes, with the selected Background Theme Pack. A disabled background, an explicit wallpaper and an imported scheme preserve their native behavior.

| Resource | Consumer and precedence |
| --- | --- |
| home.favourites.icon | Home Favorites artwork. Valid override, then platform.favourites.icon (P9), then native icon art or star. |
| home.recents.icon, home.apps.icon, home.settings.icon | Home destinations. Valid override, then the individual native glyph. |
| cover.placeholder | Missing cover treatment in Game CoverFlow, Favorites, Details and Recents. A real library cover wins. Recents prefers its valid themed platform icon before the generic placeholder. The native silhouette or box remains the final fallback. |
| achievement.badge | Gallery placeholder only. A real badge wins. The established trophy treatment remains the final fallback. |
| ra.trophy, settings.<key>.icon, app.<key>.icon | Existing meanings and native fallbacks remain available. Settings keys are category keys, not individual operation identifiers. |

The following optional [presentation] values accept integer opacity in 0–255. Defaults recreate the existing treatment: home_selection_opacity=140, coverflow_selection_opacity=142, recents_screenshot_dim=96 and recents_shadow_opacity=166. A value of zero removes the corresponding effect. Shared renderer colors still drive text, panel, selection and disabled colors. Reduced Motion remains a user accessibility preference.

The existing decoded icon LRU remains capped at 12 entries. Optional generic cover, badge and Recents platform handles borrow this storage and are cleared on eviction and theme changes. The existing screenshot lifetime and 320×240 RGB565 budget are unchanged. The banner continues to own active/pending frames. A malformed manifest leaves native assets available. Malformed optional image data falls through to the consumer's native fallback. Themes may omit the manifest entirely.

See [source inventory](p10-theme-inventory.md) and [ROM Studio integration](rom-studio-theme-manager.md). Per-option Settings art, per-role HUD/dialog art, external arrows, dedicated loading art and an avatar placeholder have no editable identifiers in this candidate. Their current native rendering and shared colors are preserved. Unknown RAW layouts remain read-only. Temporary text rendering allocations remain a P11 audit item.
