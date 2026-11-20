# Enhanced navigation contract

P2 builds and tests the model used by Enhanced UI surfaces. P1's platform grid and FrogUI's browser and Settings renderer remain the live UI. Game Details and the new Settings categories have model routes only. The model has no renderer, ROM scanner, storage directory, or theme dependency.

## Screen contract

| Screen | D-pad | A | B | Start |
| --- | --- | --- | --- | --- |
| Platform Home | Move selection within the platform view | Open selected discovered platform | Stay at Home | Open Settings Hub |
| Game Library | Move within the current collection | Open details for a discovered game | Return to platform selection | Open Settings Hub |
| Game Details | Scroll details vertically | Request launch of the selected game | Return to the same game in Library | Open Settings Hub |
| Settings Hub | Select a category | Enter selected category | Return to previous screen and selection | Stay in Settings |
| Settings category | Select a row or adjust a control via its adapter | Open subcategory or activate enabled control | Restore parent and its selection | Stay in Settings |

The live P1 grid keeps its existing directional behavior and launches games through FrogUI's browser. The future horizontal Home and Game Details consume the model intents when their views arrive. Details scrolling never emits a launch intent. A missing game falls back to the library and a missing platform falls back to Home. ROM discovery and the launcher remain authoritative. The navigation adapter must resolve identities in the current listing before enabling a selection or invoking a launch.

## Settings

The Hub has Appearance, Games & Emulation, Controls, System and Advanced. Titles and short descriptions are localization keys resolved by the existing i18n service when wiring the future renderer. An optional external icon is an opaque resource reference. A missing reference leaves a text row. Categories can contain nested categories or toggle, choice, action and range controls. Status rows stay visible as read-only information. Disabled actions stay visible and consume confirmation without activation.

The bounded stack retains the parent category and its cursor. A full stack declines further descent. Opening Settings snapshots navigation context, including selected platform and game identities. Exiting validates that context against current listings. If a setting invalidates a directory or ROM selection, use the closest valid parent. Existing handlers still own application, dependencies, localization and persistence. The future Settings view should adapt rows to these handlers, including their range bounds and step values. It should use this same stack and avoid creating a second Settings navigator.

### FrogUI control inventory

Source of truth is `settings_rows` in `frogui_libretro.c`, together with `settings_row_enabled`, `handle_settings_menu` and the existing Settings persistence. This is a proposed mapping. All 41 non-header rows are accounted for, including informational rows.

| Future category | Toggle | Choice | Range | Action or status |
| --- | --- | --- | --- | --- |
| Appearance | Enhanced graphical platform home, decorative pills, centered text, friendly system names, battery colour mode, stock battery indicator, background images, animations, rounded recents screenshots | UI theme, background theme pack, Settings style, icon pack, font, wallpaper, wallpaper image fit | Font size, background dim | |
| Games & Emulation | Hide extensions, hide empty folders, Game Switcher, start in Recents, Quick Resume, auto-save/load, custom aspect ratios | ROM source | | OTG storage status |
| Controls | | | | Button mapping wizard |
| System | Menu sounds, disable sleep | Language | Brightness, volume | Version status |
| Advanced | Apps in Settings, file cache | | RTC time increment | Open Apps, rebuild file cache, manual RTC date/time editor, RTC sync from USB |

Keep pack/background availability rules for imported schemes, dependency on enabled backgrounds and icon-pack support, OTG availability and device-specific RTC controls. Language restart currently reopens the Language row via a one-shot flag. Keep that behavior during migration. Layout cycling via L2, core selection and extension-filter editing via the existing SELECT picker, and in-game PicoArch options are separate control surfaces. Future Settings may link to these existing actions. Their storage and handlers remain authoritative.

## Horizontal platform state

The selected index and first visible index are logical coordinates of the present listing. A platform's identity is its discovered identifier, never its index. For count N and width W (at least one), clamp selection to the listing and offset to [0, max(0, N − W)]. An empty listing has no selection. A single platform has index and offset zero.

- On initial selection at the beginning of the row, the selected platform occupies the leftmost slot.
- On Right, increment selection if possible. While the viewport can scroll and the selection is anchored on its left edge, advance the viewport along with it. When the last platform is visible, offset stays fixed and the selection moves right across the row.
- On Left, decrement selection if possible. Keep the viewport fixed until selection reaches its left boundary. Once the next selection would lie left of the viewport, move that viewport left by one.
- Clamp at the ends. With N=7 and W=4, Right produces (selected, first) = (0,0), (1,1), (2,2), (3,3), (4,3), (5,3), (6,3). Left reverses those positions.
- Accept logical moves immediately regardless of presentation progress. The transition target is the new logical selection. Presentation should continue from its current visual position, zoom the departing icon out and zoom the target in. Progress is transient and never serialized. Invalid or missing resources affect drawing only.
- P4 can supply external duration, selected scale, spacing, background behavior, optional sounds and reduced motion. The frame loop should interpolate existing cached assets, with no full-asset reload or allocation per frame. Reduced motion settles directly at the current target.

## Existing FrogUI animation audit

The older horizontal view is reached with Enhanced Home disabled and the existing horizontal layout selected. In `handle_input`, `input_repeat` moves `selected_index` with modulo wrap and calls `system_carousel_start(-1/+1)`. `input.c` uses elapsed milliseconds for input repeat (300 ms initial delay, 70 ms interval). The animation itself is frame-based. `SYSTEM_CAROUSEL_FRAMES` is 12, and `render_system_carousel` advances its counter on rendered frames. `system_carousel_ease` uses smoothstep, p² × (3 − 2p), where p is the current frame divided by 12. There is no guaranteed wall-clock duration.

`system_carousel_start` carries the current visible offset into the new slide, then resets progress. A fresh input can interrupt a half-finished slide without a whole-slot jump. Turning animations off settles the transition. Re-entering the carousel also clears old offsets in `retro_run`. While animating, the redraw gate continues drawing. `system_carousel_delta` computes circular distance and the renderer centers text labels in three screen-width units using existing fonts, colors and pills. `ui_transition_start` and `ui_transition_compose` are the separate framebuffer transition for view changes.

P4 should reuse smoothstep and the offset handover approach. The new row needs non-circular coordinates and zoom, so the old renderer and its circular delta should stay with the legacy layout. Duration can be supplied by the future presentation configuration. Keep animation progress and asset loading out of input routing.

## Input, launch and resume

`input_update` combines cubevol shared-memory bits and Libretro button callbacks, applies the existing keymap, and debounces face bits for 65 ms. R36SX's right stick is reported by the existing implementation as the same raw ABXY bits, with no independent stick direction at this layer. Retain the current mapping. Details movement should use logical D-pad events; only a confirm edge requests launch.

`launch_by_path` resolves existing overrides, folder aliases and extension fallbacks. `request_game_launch` writes core and ROM paths to the existing handoff file, records history and asks the frontend to shut down. The device launcher consumes the handoff, runs PicoArch or a standalone app and restarts FrogUI when it exits. A consumed request and a new FrogUI process do not distinguish game return reliably from cold boot. Existing Recents and Quick Resume may still operate by their existing policies.

The optional versioned navigation snapshot is an explicit caller operation. Its validator resolves platform and game identities against the discovered model. Unsupported, malformed and stale snapshots fail safely. No writes in the frame loop, no automatic restoration at startup, and no edits to Settings, history or favourites files. Automatic return to Details is deferred until the runtime exposes an explicit, trustworthy resume signal.
