# P10 source inventory

P9 was audited at d3586d00b7d2b3939fbc0b36d02ef72c5a42b186, descending from P8 documentation commit 59d22de8c481e6c32f86baeefa8291e71d5d669e and tested P8 tag d694a794122f40bf6ed1859b17874a6456b55a9a. The local P9 artifact matches the recorded checksum. P9 physical acceptance remains outstanding.

| Layer | P9 mechanism and ownership | Compatibility decision |
| --- | --- | --- |
| Discovery and persistence | Native theme-pack, icon-pack and scheme registries with selections in settings.txt. Optional pack-local theme.ini. | Keep selector, default pack, imported schemes and legacy settings unchanged. |
| Pack resources | theme_resource.c indexes optional v1 mappings; bounded descriptor cache, confined directory-FD reads, nonregular files and symlinks rejected. | Keep resource validation separate from decoded pixel ownership. |
| Images | frogui_libretro.c rechecks encoded bytes at native decode; banner.c and render.c own conversion, image clipping and compositing. | Supported inputs are 8-bit noninterlaced PNG, JPEG and BMP. Up to 1 MiB encoded. Native opaque backgrounds fit using wallpaper policy; transparent icons use aspect-fit RGB565 with a separate alpha plane. |
| Background lifetime | Banner owns active/pending buffers; settings toggle, fixed wallpaper and imported schemes take precedence. | Keep precedence and do not add a second full-screen cache. |
| Icon lifetime | Existing 12-entry system icon LRU owns decoded assets; active Home/Apps/Settings presentation may borrow pointers. | Resolve outside drawing, release at generation change and preserve native fallbacks. |
| RAW | render.c legacy load_raw_rgb565_mode is a ROM-art path; firmware resources may use other unknown layouts. | Protected. No inferred geometry or new pack RAW support. |
| Font and labels | Existing font picker, FreeType/HarfBuzz and localization; theme package does not change locale or own the font selector. | Keep the native font and size controls; no new loader and no pack-relative executable content. |
| Sounds | Existing pack-local WAV roles in frogui_libretro.c and theme_resource.c. Prepared after theme change; fixed-size PCM storage. | 44.1 kHz PCM16 mono, up to 100 ms; use the native clip on failure. User setting controls playback. |
| User colors | Built-in render theme, imported render schemes and user custom colors already supply the shared COLOR roles. | Preserve ownership and existing precedence. New optional effect values should defer to established colors when unspecified. |

Classification: **1** already themeable, **2** themeable with view-specific or native fallbacks, **3** hardcoded suitable for optional styling, **4** protected renderer or data, **5** geometry/encoding unverified.

| Consumer | P9 resource and implementation | Class and fallback |
| --- | --- | --- |
| Home/ribbon | home.background, platform.<key>.background and platform.<key>.icon; native platform/icon/background lookup. 640×480 background bound; 256×256 icon input, 160 px prepared edge. | 1/2. Platform to Home to native background; icon pack to system icon to initials. |
| Home utilities/Favorites | platform.favourites.icon is the exact P9 Favorites ID. Utilities have native glyph fallback. Home renderer computes enlarged selection geometry and label position. | 2/3. Native star and utility symbols; noncircular selection and clipping are protected behavior. |
| Library/Favorites CoverFlow | Shared card renderer, cover placeholders, shadow, selected border, side scaling. ROM artwork uses original path in render.c. | 2/3/4. Native card if no valid image. Keep ROM artwork, navigation and launch outside theme ownership. |
| Recents | enhanced_recents.h draws selected screenshot, metadata, perspective box, spine and panel. Selected runtime capture is bounded to 320×240 RGB565; missing images use platform art or native placeholders. | 2/3/4. Theme can style treatments, never choose a capture, change its identity or modify the file. |
| Apps | app.<key>.icon in existing shared CoverFlow; app_defs identity. | 1/4. Native symbols when absent; executables and discovery protected. |
| Enhanced Settings | settings.<key>.icon category artwork; enhanced_settings_render.h draws focus, panel, toggle, slider, selectors and disabled state using shared colors. | 1/3/4. Category text remains readable without icons. Widget semantics, value application and persistence stay native. |
| Game Details | Shared cover thumb and details header, background, chevrons, metadata text, favorite mark and RA indication in frogui_libretro.c. | 2/3/4. Existing local metadata and launch identities stay protected. |
| RA Gallery/Live Area | ra.trophy; achievement.badge is recognized by the manifest; real badges and avatars are RA data loaded independently. | 1/2/4. UI can replace generic imagery. Unknown status remains unknown; no synthesized progress. |
| HUD/dialogs/hints | Native text, battery/input geometry, confirmation and warning drawing; labels localized using lang/builtin. | 3/4. Shared colors can safely style the interface. Preserve meaning and input cues. |
| Empty/loading states | Built-in labels and symbols drawn by the active view. | 3/4. Optional empty markers may be skinned; loading visibility and navigation remain renderer responsibilities. |
| Firmware RAW | No verified contract for unrelated binary assets. | 5. Read-only; no preview dimensions or conversion proposed. |

The active package can reference only its own resources. Game art, saves, gameplay captures, RA progress and real RA media are independently owned. The runtime does not read the tooling catalog.
