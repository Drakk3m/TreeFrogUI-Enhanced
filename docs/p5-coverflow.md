# P5 CoverFlow library

P5 is based on the accepted P4.2 commit 2f058cbbd65c3f48533e61c10e38e675b36bcd1d and binary SHA-256 8474274242139af32f8b1460cad372d1eeaf6b95ab86d858a014d7afbfe4485b. The previous physical record is 26/26 passed. P5 requires its own physical acceptance.

## Preimplementation audit and decision

| Area | Existing evidence and reuse |
| --- | --- |
| Routing and identity | `navigation_model.c:route_screen` defines Home → Library → Game Details, Details → Library, and a separate Details confirm → launch effect. P4.2's live browser still launches regular ROM entries directly. The Library uses the existing `entries` and `selected_index`. The adapter can use the P2 model for the minimal Details route with the ROM basename identity and authoritative current listing. |
| Input and launch | `frogui_libretro.c:handle_input`, `launch_by_path`, and `request_game_launch` own the established button semantics, core resolution, recent history and launcher handoff. Keep the handoff unchanged. Any new Details action calls it explicitly. Back from the existing browser root restores the selected platform by folder name. Full post-game process reconstruction has no stable launcher signal. |
| RGB565 draw | `render.c:render_set_geometry` and `render_thumbnail_contained` already support an aspect-fit clipped RGB565 draw with optional alpha. `carousel_smoothstep` in FrogUI supplies the P4 motion interpolation. The 640×480 framebuffer alone is 614,400 bytes; P5 does not require another framebuffer. |
| Artwork | `render.c:load_game_artwork` searches the existing box-art paths under `.res`, `Imgs`, `images` and `Images`, including raw RGB565 and the existing port path. The native miss table is keyed by ROM path and folder signatures. `load_image_thumbnail` reduces decoded RGBA into native static 250×250 RGB565 and alpha scratch buffers. |
| Cache | `frogui_libretro.c:render_game_boxart_pane` owns a four-slot decoded LRU that caches misses and falls back to title-screen art. Factor it for reuse and allow the bounded Enhanced window to request an allocation-limited decode. Retain legacy lookup behavior. |
| Decode and SD cost | The classic decoder directly invokes stb on the file. A valid large header can allocate memory before thumbnail reduction. Use the existing P3 allocation-limited decoder for a bounded Enhanced load, selected unchanged by the same naming rules. Load on listing/theme/selection changes and keep misses in the existing caches. Work per browse step must remain bounded independently of discovery count. |
| Themes | `enhanced_theme_sync`, `load_banner_for_view` and the P3 resource manager retain the selected Background Theme Pack, wallpaper, source bounds and native fallback. Optional cover placeholder semantics and reduced motion remain on the same pack. |
| Size and performance | `scan_directory` already supports large libraries and its native cache allows up to 100,000 cached entries. This is a parser safety cap rather than a physical capacity guarantee. The previously inspected R36SX device tree declares about 175.6 MiB. Actual working RAM, SD read latency, and frame time on the console still need measurement. |

**Decision · aspect-fit RGB565 carousel with depth from size and draw order.** Center one larger cover, place smaller covers beside it and paint the farthest first. Use existing clipping and smoothstep. Avoid perspective warps, reflection buffers, duplicate decoders, and another layout or audio engine. Scale with the native bounded blit. Keep a fixed window of cached decoded covers and distinguish retained RAM, temporary decode allocations, and host-only timings.

## Implementation and resource budget

The existing classic side-panel LRU now also serves CoverFlow. Classic retains four slots and its title-screen fallback. Enhanced can retain seven slots, including misses and the previous/current visible spans during a slide. It retains one aspect-fit thumbnail variant per game, at most 192 × 192 RGB565 with optional 8-bit alpha. The payload ceiling is 774,144 bytes (756 KiB) plus seven cache entries and borrowed pointers. There is no second full-screen surface. The existing thumbnail scratch remains 187,500 bytes shared with other native callers.

Enhanced probes use the original filename rules. A nonexistent, invalid, oversized or unreadable cover gets the same inexpensive drawn silhouette, independent of the ROM entry. PNG, JPEG and BMP are limited to 1 MiB encoded and 640×480 source dimensions, with the existing P3 cap of 2 MiB per decoder request and 6 MiB cumulative requested. Supported native raw RGB565 sizes remain usable. A decode can briefly hold the encoded buffer and decoder allocations in addition to retained pixels. Rejections are cached in memory and never written to the classic artwork miss table. Decoding happens on transitions or changed visible spans. Cold selection changes may still synchronously read a few covers, so real SD latency remains an acceptance item.

Moving selection uses the existing smoothstep and carries forward the fractional visible position, with bounded travel when repeated input outruns paint. Art loads before rendering, in a span capped by the shared cache. The compositor paints outer neighbors first and selected cover last. A theme can set its existing neighbor count, spacing, side-scale and reduced-motion preference. Eviction, platform exit, pack changes, mode switching and frontend teardown release the relevant cached pixels. Existing background and wallpaper selection still own the backdrop.

## Navigation and limits

Enhanced Library uses the current ROM listing for Left and Right and Audio P4.2 sees the accepted index change. At either boundary the selection stays put. A on a ROM enters the existing P2 Details route through a minimal screen. A on that screen explicitly hands the ROM to the existing launcher. B or a Settings round trip preserves the selected game within the same directory. B from the Library returns to Home and resolves the top-level platform by its folder name, including from a nested collection. A on a folder still browses it. For the horizontally presented library, Left and Right replace the classic list's vertical navigation. Existing search, Favorites and per-ROM options stay on their normal paths.

Details displays the selected title, art and Play/Back controls. No P6 metadata or RA screen is included. A game process may restart FrogUI through the existing launcher. P5 adds no cross-process selection persistence, since the existing handoff cannot distinguish a cold boot from game return. Existing settings and history persistence are untouched.

## Verification results

The sanitizer fixture navigated 180 input/paint steps in a 1,500-entry listing. It retained at most 774,144 bytes of decoded pixels and observed zero artwork cache accesses from the draw path. Its combined input, loading and draw run took 18,152 ms under ASan/UBSan. The native drawing and font paths made 4,320 wrapped malloc calls over those 180 frames. A separate optimized x86-64 run painted 240 warm frames in 99.9 ms, or 0.416 ms per frame, with the cache populated and no SD lookup or input work in the timed loop. These host results do not predict device frame rate or RAM usage.

The SF3000 ELF is 1,390,696 bytes, 15,384 bytes larger than P4.2. It retains little-endian MIPS32r2, o32 hard-float, the same Libretro exports and direct dependencies, and no added versioned imports.

The six captures are deterministic 640×480 host fixtures using original test artwork. They cover first, middle, last, missing, empty and a Unicode long title. The title is shortened on a UTF-8 boundary before paint. Hardware visual and performance evaluation remains pending.

## Physical acceptance and rollback

Use the separate TreeFrogUI test SD. Power off and back up `cubegm/cores/frogui_libretro.so` and `frogui/settings.txt`. Replace only the frontend, verify its SHA-256 and boot. The optional EN/ES language bundle updates the new labels. Save the old language files if replacing them.

1. Open a platform from Enhanced Home. Check a first, middle and last game, including clean margins and consistent highlights. Verify zero, one and two games, a long Unicode title and square, wide or tall art.
2. Check missing, corrupt, unsupported and oversized art. Verify every ROM remains reachable. Try one large collection on the test SD and repeatedly reverse directions while observing responsiveness, audible boundary behavior and the steady backdrop.
3. Confirm reduced motion, fixed wallpaper, pack changes and cache rebuild. Reenter and leave two different platforms repeatedly. Verify the artwork belongs to the current platform.
4. Press A on a game and verify only Details opens. Press B to restore the same game. Repeat a START/Settings round trip. Press A to open Details and again to launch. Verify audio, video and return to FrogUI. Check current launcher behavior for post-game selection and verify Favorites, Recent and settings.
5. Confirm B restores the platform highlight on Home. Disable Enhanced and verify the classic lists, grids and launch behavior. Repeat the game test.

To roll back, power off and restore the P4.2 shared object from the backup, plus settings and language backups as needed. Verify the accepted P4.2 hash before booting.

The earlier P5 hotfix acceptance remains on record. See the [P8 physical retest matrix and outstanding tests](enhanced-navigation.md#current-enhanced-development-baseline) for results on the cumulative candidate.
