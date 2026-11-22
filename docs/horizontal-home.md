# P4 Horizontal Home

P4 starts from the physically accepted P3 consolidation at 806d5d1c3bce00e9b32a0cd0cadc16778f08e2bd. Enhanced Home is selected by the existing graphical Home preference.

## Baseline audit

| Area | Existing owner and P4 integration |
| --- | --- |
| Rendering | The provisional Enhanced grid is prepared before paint by `prepare_enhanced_grid`. `render_system_grid` also serves a classic layout, so classic keeps that renderer. P4 replaces only the Enhanced presentation. |
| Navigation | `navigation_model.c` supplies the existing horizontal bounds and intent contract. FrogUI's adapter recognizes Home and Library and resolves the model's identities against the current listing. |
| Animation | The legacy horizontal view uses a 12-rendered-frame smoothstep and carries the current visual offset when interrupted. P4 reuses the interpolation with a noncircular target. |
| Icon decoding | The native 12-entry icon LRU owns RGB565 and alpha buffers. P3 resolves logical icon IDs before native paths. Drawing rescales those decoded pixels and alpha blends into the existing framebuffer. |
| Background | `load_banner_for_view` integrates the selected Background Theme Pack, optional pack manifest, imported schemes, wallpaper and user background toggle. `banner.c` owns active and pending targets. |
| Discovery and file cache | `scan_directory` owns the listing, cache and visibility settings. The Settings rebuild action invalidates and scans through the existing path. |
| Input and destinations | `input.c` applies the active button map. Repeat uses a 300 ms initial delay and a 70 ms interval. Settings and Apps already have handlers and return paths in the core. |
| Display | The accepted R36SX target is 640×480 RGB565. Clip moving artwork at the edges and leave the header and bottom legends separate from ribbon geometry. |

## Live behavior and memory

The live listing retains `selected_index` and `scroll_offset`. The P2 window computes both from the current listing and enforces the five-slot noncircular rule. Home uses Left and Right for platforms, A for the existing library, Start for the existing Settings and R1 for Apps when the existing Apps-in-Settings preference leaves Apps as a tab. B restores the Games listing from Apps. When Apps in Settings is active, Apps remains available through Settings.

The ribbon reuses the classic smoothstep over 12 rendered steps. A new direction starts from the visible fractional position. Travel is bounded to three slots if input overtakes rendering, keeping the union of visible icons within eight borrowed references. The native LRU remains at 12 decoded icons. Theme icon spacing and scale are honored, and either the native animation preference or the pack's reduced-motion preference can settle motion immediately. The previously allocated background and framebuffer paths are reused. Only a few scalar presentation fields were added to static storage. The ribbon performs no explicit per-frame asset load or heap allocation. Existing font shaping and glyph caches retain their original ownership.

The shared text renderer now converts signed pixel x positions to 26.6 coordinates by multiplication, since clipping a moving label can start at a negative x. The sanitizer found that the previous left shift of a negative signed value was undefined.

Game launch still uses the existing handoff. As established in P2, there is no verified launcher signal distinguishing a post-game FrogUI process from a cold boot. No new persisted resume identity was added.

## Offline RetroAchievements profile header

The P2+RA contract defines navigation and ownership. It contains no profile schema or reader. The local FrogUI source has an artwork scraper but no authoritative exporter. The ROM Studio maintainer confirmed that Studio v87.9.1 exports `gamepack.json`, achievement badge PNGs, and `rom_identity.json` to `cubegm/enhanced/ra/` through Review Changes → Apply Changes. This report is maintainer evidence. No Studio source or sample export was available locally to verify additional fields. Those named outputs establish catalog, artwork and ROM identity. Linked profile and local unlock records have yet to be established. The profile header is absent in this build.

To enable the username, supply ROM Studio's serializer or export specification and a sanitized versioned SD sample showing the exact path, a linked-profile identifier and the linked username, including its encoding and the unlinked representation. Existing artwork matching, definition catalogs and badge files do not establish linkage.

To enable the count, supply the matching complete local unlock snapshot or equivalent authoritative distinct aggregate with its provenance. For record-based exports, supply the profile association, achievement identifier and uniqueness scope, local unlock evidence, remote confirmation as a separate state, and the semantics that distinguish a complete empty snapshot from absent or incomplete data. Include samples for verified zero, repeated records, local-only and remote-confirmed records, and malformed data. Keep credentials outside the export.

The future reader can load once during frontend initialization and on new frontend sessions after gameplay. If a safe source change event becomes available, reuse it. Existing P4 rendering should read only the validated in-memory snapshot. No RA parser, renderer, database, or synthetic count is included.

## Physical acceptance and rollback

Perform the following by hand on the working TreeFrogUI test card. Before copying, power off, back up the existing `cubegm/cores/frogui_libretro.so` and `frogui/settings.txt`, and verify both backups are readable. Replace only that shared object and verify the P4 checksum. Keep the original card untouched.

1. Start with Enhanced enabled. Check the full-screen background, ribbon and icons, selection highlight, readable labels and status information. Repeat with Spanish and English, including one long platform name.
2. Tap and hold both directions, reverse direction mid-transition and reach both ends. Verify left anchoring, noncircular movement, responsive scaling and no stalls. Exercise a listing with fewer platforms than visible slots. Test an empty library on a backed-up test setup.
3. Confirm a platform, navigate inside its Library and return with B. Reopen Settings with Start, return, and check platform identity. Open Apps from the Home route and return. Verify existing controls within both destinations.
4. Switch between legacy and manifest-enabled packs, try a missing/corrupt icon or backdrop, and check fixed wallpaper, disabled backgrounds, imported schemes and reduced motion. Rebuild the file cache and verify the ribbon reflects the listing and Hide Empty Folders.
5. Launch a known ROM, confirm audio and video, exit normally, and verify frontend recovery, settings, history and favorites. Record whether the device launcher preserves the platform on restart.
6. Reboot and confirm Settings persistence. Turn Enhanced off and check the original behavior. Verify that the upper-right header stays free of synthetic RetroAchievements data while the export is unavailable.

For rollback, power off and restore the saved binary. Restore `frogui/settings.txt` if the test changed preferences you want to undo. Verify the restored binary against your backup before booting, then repeat a known game launch.

## Build and verification environment

Use Ubuntu 24.04 in WSL2 and the accepted SF3000 SDK. From FrogUI, set the compiler prefix and sysroot and invoke its normal frontend build:

```sh
sdk="$HOME/r36sx-p04/sdk/mipsel-buildroot-linux-gnu_sdk-buildroot"
export MIPS="$sdk/opt/ext-toolchain/bin/mips-mti-linux-gnu-"
export SYSROOT="$sdk/mipsel-buildroot-linux-gnu/sysroot"
export OUTDIR="$HOME/r36sx-p04/p4/out"
sh build_libretro.sh
```

Compare ELF flags, versioned imports, dynamic dependencies, and exported `retro_*` symbols to accepted P3. The host previews use the actual FrogUI paint functions at 640×480 with fixture platform IDs, staged TreeFrogUI fonts and icons, and a flat RGB565 background. They show layout under repeatable inputs and do not measure device latency or available RAM.
