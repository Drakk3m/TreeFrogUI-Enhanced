# Enhanced navigation contract

P2 builds and tests the model used by Enhanced UI surfaces. P1's platform grid and FrogUI's browser and Settings renderer remain the live UI. Game Details and the new Settings categories have model routes only. The model has no renderer, ROM scanner, storage directory, or theme dependency.

## Screen contract

| Screen | D-pad | A | B | Start |
| --- | --- | --- | --- | --- |
| Platform Home | Move selection within the platform view | Open selected discovered platform | Stay at Home | Open Settings Hub |
| Game Library | Move within the current collection | Open details for a discovered game | Return to platform selection | Open Settings Hub |
| Game Details | Scroll details vertically | Request launch of the selected game. A separate gallery action opens Achievement Gallery when available. | Return to the same game in Library | Open Settings Hub |
| Achievement Gallery | Reserved for the future gallery view | No launch | Return to Game Details for the same platform and game | Open Settings Hub |
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

## Offline RetroAchievements

P2 reserves Achievement Gallery as a detail destination in the existing navigation model. A future explicit Gallery affordance opens it from Game Details only when the selected platform and game resolve and ROM Studio's offline set is available and valid. Confirm on Game Details still requests the existing launch independently of gallery availability. Back restores Game Details for the same platform and game. If the game disappears, use the same Library or Home fallback as the rest of P2. Start opens the existing Settings route from the gallery and restores the gallery context when it is still valid. Gallery does not consume a Settings nesting level. The versioned snapshot still stores only platform and game identities. It cannot restore a gallery session, grant an achievement, or change cold boot.

The future Settings path is **Games & Emulation → RetroAchievements**. It is a reserved category in the model. The current Settings renderer and preferences stay in use. The category contains no live controls in this phase.

| Future control | Semantics | When it can be exposed |
| --- | --- | --- |
| Enable RetroAchievements | Toggle that controls evaluation policy by the runtime. | Once an evaluator and a supported preference path exist. |
| Offline achievement availability | Read-only status for the selected game and validated cache. | Once ROM Studio exports the supported catalog contract. |
| Runtime compatibility status | Read-only capability and session state for the selected core, build and content. | Once the runtime reports compatibility and activity for those inputs. |
| Local unlock progress | Read-only progress sourced from local evaluation records. | Once those records have defined provenance and durable storage. |
| Synchronization status | Read-only state sourced from the ROM Studio workflow and remote confirmation. | Once the workflow and upstream service support are verified. |

### State semantics

| Dimension | Distinct states | What qualifies |
| --- | --- | --- |
| Offline catalog | No matching set, set available offline | Match the discovered ROM identity to a structurally valid supported export. A malformed, stale or unsupported export is unavailable to the gallery. |
| Evaluation | Unsupported, inactive, active | Unsupported means the particular core/content/runtime path lacks confirmed capability. Inactive means a supported evaluator is not currently evaluating, including when disabled. Active requires a running eligible session. Cached definitions alone establish none of these. |
| Unlock record | Locked or unknown, unlocked locally, synchronized remotely | Local unlock requires evidence from an evaluator. Remote synchronization requires confirmation for that unlock from the authorized workflow. A pending or rejected operation leaves the local record local. |

### Responsibility and dependency matrix

| Owner | Responsibilities | Inputs and outputs |
| --- | --- | --- |
| ROM Studio | Authoritative ROM matching, offline cache/export and any supported account or synchronization workflow. | Owns the association between stable game identities, compatible content hashes and RA game/achievement IDs. Prepares titles, descriptions, badges and definitions. A future export can include local unlock status and pending synchronization information from known provenance. |
| Enhanced frontend | Browse available offline data and route to the gallery. Render cache and progress statuses when supplied. | Reads the verified export and runtime status through adapters. No separate downloader, hashing pipeline, credential storage or unlock evaluation. Gameplay uses the existing launch path even when RA data is absent. |
| PicoArch / rcheevos | Potential future evaluation while a game is running. | Requires a validated core memory view and lifecycle integration. Emits local evaluation records with provenance if such a runtime is built. P2 introduces no runtime, core or launcher changes. |
| RetroAchievements service | Authority for remote unlock status and eligibility. | Any later synchronization depends on documented service support, authorized sessions and explicit confirmation. A desktop queue does not guarantee remote acceptance. |

No authoritative ROM Studio source or export specification was found in the local workspace, Downloads, or the inspected R36SX backup archive. The TreeFrogUI web scraper uses RetroAchievements for artwork matching. That code has no authority over ROM Studio's catalog format or handheld evaluation. Before defining a reader, inspect the Studio identity and hash resolver, export serializer, achievement/definition and badge cache, unlock record model and synchronization workflow. Request sample exports covering absent sets, hash variants, multidisc, badges, local-only records, failed/pending sync and remotely confirmed unlocks. No storage schema, hash algorithm, filenames or credential handling are introduced here. Console exports must contain no account credentials, tokens or API keys.

### Runtime questions to close before implementation

- Prove safe memory reads for each core, platform and content combination. The inspected PicoArch reference resolves Libretro memory accessors and uses them for save RAM. Its environment handler does not implement the memory-map or achievement-support commands declared in the Libretro header. These findings leave RA memory maps, address validity and per-core support unverified. FrogUI itself returns no game memory.
- Establish evaluation timing and lifecycle on the emulation thread, including core load/unload, reset, pause, disc change, fast-forward and save-state/rewind behavior. Confirm the cost and available memory on R36SX.
- Define durable local unlock recording across shutdown, write interruption and repeated evaluation. Keep origin data sufficient for ROM Studio to distinguish local-only, pending, rejected and confirmed states. Keep the existing launch request unchanged.
- Verify the policy and API for receiving credit. The upstream FAQ does not support starting a fully offline RA session. Its reconnect guidance concerns an already-started session and does not establish support for retroactive submissions from a disconnected handheld. Treat any proposed later submission as unresolved until the service confirms the workflow.

Sources: [PicoArch memory accessors](https://github.com/tzubertowski/TreeFrogUI_picoarch/blob/2ab990d490b4d6aa5872ca98c85c643df7b4db7b/core.c#L70), [rcheevos client integration](https://github.com/RetroAchievements/rcheevos/wiki/rc_client-integration), [RetroAchievements FAQ](https://docs.retroachievements.org/general/faq.html#can-i-earn-achievements-offline).

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

## Current Enhanced development baseline

The physically tested frontend is P8 commit d694a794122f40bf6ed1859b17874a6456b55a9a, tagged enhanced-p8-physical-baseline. The tested frogui_libretro.so SHA-256 is 765202300f9cb27b32146bf666138d530c6aa2e6ffc635e643e182f05d6f29dd. This is a development baseline with open validation, not a production release. Later documentation commits do not change the tested implementation.

| Component | Passed | Pending | Failed |
|---|---:|---:|---:|
| P5 CoverFlow retest | 0 | 5 | 0 |
| P6 Game Details | 8 | 0 | 0 |
| P6 RetroAchievements | 2 | 5 | 0 |
| P7 Enhanced Settings | 12 | 0 | 0 |
| P8 Apps CoverFlow | 10 | 0 | 0 |
| General regressions | 5 | 0 | 0 |
| Total | 37 | 10 | 0 |

The operator reported no observations. P6 Game Details, P7 and P8 are physically accepted. The P5 retest is additional coverage on this specific binary. Earlier physical acceptance for the P5 Home Ribbon hotfix (tag p5-home-hotfix-accepted), P5.1 filelist and P5.2 HUD remains valid at the revisions tested. The P5.1 and P5.2 integrations are recorded in fork PRs #2–#4.

### Pending physical validation

**P5 CoverFlow**

- Load real artwork from the SD.
- Check missing and corrupt artwork fallback.
- Measure large-library navigation performance on the SD.
- Verify Reduced Motion.
- Reach every Home Ribbon destination.

**P6 RetroAchievements**

- Check L2 availability for games with valid RA packages.
- Check achievement badges, descriptions and points.
- Check vertical achievement navigation.
- Confirm the gallery returns to the same game.
- Confirm unknown progress is never shown as verified unlocks.

FrogUI validates the exported file identity and package agreement without independently recalculating the ROM content hash. A replacement retaining size and modification time could pass this check. See [offline game information](enhanced-game-information.md) for the format and limitations.

### Settings and Apps architecture

P7 keeps one settings table, persistence path, and set of adjustment and activation operations. Classic retains its navigation and presentation adapter. Enhanced uses the same operations through a two-panel NavModel adapter, retaining origin and category context.

P8 shares the game CoverFlow window, easing, distance scaling, draw order and aspect-fit blitter. Apps discovery remains in scan_apps_tab and app_defs. Selection invokes the existing activation and confirmation paths. Apps artwork is resolved by stable virtual key through [the theme resource contract](enhanced-theme-resources.md#application-icons). ROM discovery, identity and artwork resolution remain separate.

### Remaining frontend roadmap

1. P9 — Enhanced Recents & Favorites. Start from the exact P8 development-baseline tag above.
2. P10 — Theme System Consolidation.
3. P11 — Frontend Final Audit.

PicoArch changes, core compatibility and the RetroAchievements runtime are separate future work. Complete the outstanding retests and review the cumulative frontend PR before authorizing integration. Update the parent submodule only after the frontend integration has been authorized and its resulting source tree is verified.

### P9 candidate scope

The P9 branch is based on the P8 documentation tip and leaves PR #7 and its tested implementation tag independent. Enhanced Home now orders Favorites, platforms, Recents, Apps and Settings. Favorites shares library CoverFlow, while Recents uses its own selected-game presentation. See [collection adapters and file contracts](enhanced-game-information.md#p9-collection-adapters). P9 awaits physical acceptance. The ten P8 retests above remain pending. P10 and P11 have not started.
