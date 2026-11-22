# P4.1 Unified Home

P4.1 starts from P4 commit d1e1ed9f0f88b6c8b967e53e0435f518bd848e48. P4 passed 19 of 19 checks on the R36SX according to the supplied acceptance record. P4.1 requires its own device pass.

## Baseline audit

| Area | Baseline and P4.1 integration |
| --- | --- |
| Listing | `scan_directory` owns real ROM discovery, visibility and the file cache. Utilities belong to the Home item projection and never enter the ROM listing or its cache. |
| Movement | P4 computes the live viewport with P2's `NavHorizontalWindow`. The same navigation rule applies to the unified list. P4's frame-based smoothstep and bounded icon look-ahead can be reused. |
| Destination handlers | Recents, Apps and Settings already have live view transitions. Home uses these existing handlers and preserves a typed return selection. The browser preserves platform identity when backing out of a library. |
| Settings context | `open_enhanced_settings` uses the P2 navigation model to capture the current context. Settings can rebuild the ROM listing, so utility identities must be independent of the old platform count. |
| Artwork | P3/P4 already own asset validation, aspect-fit RGB565 blending, the 12-entry native icon cache and the active/pending background buffers. Use these existing paths for platform art and the shared/native backdrop for the utility items. |
| Geometry | The staged TreeFrogUI installation has 108 PNG system icons, with common dimensions including 93×130, 130×130, 160×91 and 240×50. A square paint box needs aspect fit. On a 640×480 target the selected title can use a wide, bounded clip below the ribbon. |
| HUD | The P4 render called the classic heading and legend plus separate shortcut text. P4.1 keeps battery and essential controls on Enhanced Home and leaves the classic tabs and help on the classic rendering paths. The upper-right area remains available for a future read-only RA profile. |

## Implementation and host evidence

The Home projection has one typed selection. A platform uses the native browser entry and each utility keeps a kind independent of the directory count. P2's horizontal window computes the viewport for the full list. P2's Settings return continues to resolve a real platform by name. The utility kind survives that same Settings trip, including a listing rebuild. Recents and Apps use the original handlers and return through the existing Games tab backend. A opens Apps even when its classic tab is hidden by the Apps-in-Settings preference.

The native Favorites sentinel remains in the host listing for the classic frontend. Enhanced filters that sentinel out of the ribbon and exposes the existing Favorites view via Y when favorites exist. START and the previous R1 shortcut remain usable in their original contexts. The Home hint reuses the A portion of the localized native legend and leaves the old legend unchanged elsewhere.

At 640×480 the five slot centers are 115 px apart. Aspect-fit boxes settle at 119 px selected and 67 px neighboring, a ratio of 1.78. The sizes do not include a three-pixel highlight margin. The selected box is bounded by the panel while entering from an edge. Native utility symbols need no assets or separate image cache. Platforms still resolve the existing theme icons. A utility highlight uses the shared Enhanced background and then native main art. The native wallpaper, imported scheme and background toggle rules still apply.

The host renders both use 640×480 and the same fixture assets. The Enhanced platform title background spans x=140–498 and the Settings title background spans x=256–383. The P4.1 Enhanced top row contains no tab pixels in the fixture and the classic capture retains them. The host fixture has no device battery samples. Physical testing must exercise both stock and frontend battery modes.

Host tests cover the first and last positions, zero and one platform, platform and utility ordering, Settings round trips, Apps, Recents, the cache rebuild, theme fallback and interruption/reduced-motion behavior. The accepted game handoff is checked with a host fixture. Game execution and a physical return remain device checks.

## Hardware acceptance and rollback

Use the separate TreeFrogUI test SD. With the console powered off, back up `cubegm/cores/frogui_libretro.so` and `frogui/settings.txt`. Replace only the frontend and verify the delivered SHA-256.

1. Start with Enhanced enabled. Verify that platforms appear first, then Recents, Apps and Settings in that order. Test a no-platform setup and a one-platform setup. Confirm that the legacy top tabs and the Start/R1 shortcut labels are absent.
2. Check the battery/status area, the centered title and the selected icon at the left and right boundaries and during movement. Verify that the icons keep their proportions, that long titles remain legible and that the title clears the bottom hint. Exercise EN and ES.
3. Use both directions, hold a direction to its boundary and reverse mid-transition. Confirm that the ribbon never wraps. Enable reduced motion and repeat.
4. Open a platform, Recents, Apps and Settings with A. Back out from each and verify that Home restores its item. Highlight Recents and press START, then B. Repeat from Apps, Settings and a platform. Verify shortcuts and the existing Apps-in-Settings behavior.
5. Rebuild the file cache after a library change. Confirm platform visibility follows the existing setting and each utility appears once. Test missing icons, a pack switch, a shared fallback, an imported scheme and a fixed wallpaper.
6. Launch a known game, check audio and video, then return through the original launcher. Verify that the frontend remains usable and that preferences, favorites and history persist. Disable Enhanced and confirm that classic tabs and hints appear as before.

Rollback with the console powered off. Restore the backed-up frontend and restore the saved settings if desired, verify the binary and restart. The previously accepted P4 is the rollback baseline.
