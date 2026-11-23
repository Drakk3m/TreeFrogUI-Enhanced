# P4.2 navigation and UI audio

Base is accepted P4.1, commit 75619bc183db3d735f85c3bcab0193ee3c5a8fb2. The supplied physical record is 18/18 passed. P4.2 requires its own device acceptance.

## Source audit and decision

| Existing owner | Finding and smallest extension |
| --- | --- |
| `navigation_model.c`, the Enhanced Settings adapter and the P4.1 Home projection | The bounded navigation model already owns Settings snapshots and horizontal window math. The live browser owns stable platform identity. Keep the Home projection and motion, adding a centered viewport policy and retaining the current anchor near each boundary. |
| `render_enhanced_home`, `render_recents_horizontal` and Apps/Settings drawing in FrogUI | P4.1 already removes the top tabs from Home. Secondary views still share native heading and hint paths. Keep their renderer and content, change the Enhanced heading and render Recents bounds as native chevrons. |
| `handle_input`, `switch_main_tab`, `settings_rows` and i18n | Existing Start/Y/shoulder routes depend on context and theme/prefs. Preserve public behavior, reuse the current settings table, save file and localized labels, and use the navigation adapter for return context. |
| `ui_menu_tick`, `retro_get_system_av_info` and the two Libretro audio setters | FrogUI advertises 44,100 Hz and 60 fps. The existing short click generates signed 16-bit stereo and submits a bounded one-shot through the batch callback, with a per-sample fallback. FrogUI has no mixer, dedicated audio device, or independent streaming thread. |
| `settings_apply`, `settings_write_volume`, `cube_set_i2so_output_muted` and launch request handlers | System volume is applied by the existing cubevol/I2SO path. The startup mute is reasserted briefly when menu sound is off. Game and app handoffs restore output before transferring ownership. The existing sound preference is persisted as `menu_sounds`. |
| Staged PicoArch reference `core.c` | The loader registers both audio callbacks and reinitializes the platform from the core's advertised rate. Both callbacks forward stereo signed 16-bit frames through `plat_sound_write` only while audio is enabled and the host is running. PicoArch owns output buffering, pause and device lifecycle. FrogUI is relaunched by the existing launcher after gameplay. No PicoArch, driver or launcher change is needed. |
| P3 `theme_resource.c` | The pack-local manifest already reserves `navigation.sound` and validates regular-file containment plus a RIFF/WAVE header. It does not validate PCM contents or play anything. Extend this role with four optional event IDs, retaining the earlier generic ID as a movement alias. |

**Decision B — extend partial audio support.** Reuse the existing callback submission and system volume path. Preserve the classic generated tick and its stored setting. Enhanced uses one independent saved boolean and bounded mono PCM clips converted to the existing stereo submission buffer on accepted events. Resolve file overrides on pack or mode changes and use original generated tones otherwise. No decoding or SD access in normal frame processing and no new audio ownership. Device testing must still verify audibility, short-batch behavior, volume, pause/resume and game handoff.

## Navigation and resource integration

Home uses offset clamp(selected − 2, 0, count − 5) in the same five-slot window and retains the P4.1 motion state. Enhanced Recents uses one-step left/right navigation in both list and GameSwitcher presentations with fixed edge chevrons. The P2 Settings snapshot can now return to the Recents and Apps root views, preserving their selection and Recents fullscreen state. Home, Library, Recents and Apps reuse their existing handlers. The established shoulder routes remain active, including R1 from Recents back to Home and Home's compatible Apps shortcut. Y still opens Favorites where available.

The four built-in tones are generated from original triangular waveforms in the core. One short sound is submitted per accepted input update. Directional movement, nondirectional selection, confirmation and cancellation resolve through the same dispatch. Rejected navigation at the ends does not submit audio. There is a minimum 55 ms interval and playback also waits out the nominal current clip duration. Game handoff is never delayed for a tone. PicoArch/device volume and mute remain authoritative.

Audio adds four static mono caches of 8,820 bytes each, plus the platform-sized frame-count fields. The stereo submission buffer is shared with the existing classic click. WAV validation reads at most one 9,332-byte temporary blob at a time. A theme change builds or validates the four clips outside rendering. An accepted event copies up to 4,410 samples into the existing stereo buffer. There is no per-frame allocation, file read, mixer thread or idle PCM stream. Actual CPU, audibility and playback latency on R36SX have not been measured.

## Physical acceptance and rollback

Use the established TreeFrogUI test SD. Back up `cubegm/cores/frogui_libretro.so` and `frogui/settings.txt` with the console powered off. Replace the shared object and verify its checksum. The bundled EN/ES language resources are optional if the existing UI Sounds English fallback is acceptable. Back up both language files before replacing either.

1. Verify smart centering across the middle, stops at both boundaries, rapid reversal and reduced motion. Test a sparse library and select each utility. Confirm no wrapping.
2. Test Recents with zero, one and several entries in list mode and GameSwitcher. Compare the chevrons at both ends and in the middle. Verify the existing shoulders and Y behavior. Open Settings using START from Home, Library, Recents and Apps, then press B and verify each return context. Check classic tabs and behavior with Enhanced disabled.
3. With UI Sounds off, verify silence and the saved preference after restart. Turn it on. Exercise movement, the shoulder routes, confirm and back. Verify that attempting to go past a bound is silent and fast changes do not stack sounds.
4. Copy the optional sample theme into the test SD, select it in Background Theme Pack and compare its move and confirm sounds to the built-in defaults. Test an absent and malformed WAV. Switch packs and toggle Enhanced off/on. Verify independent classic Menu Sounds.
5. Set volume to zero and increase it using the device controls. Verify mute, audibility and usable input during audio. Launch a known game, test game audio and return. Repeat after sleep or restart where supported.
6. To roll back, power off and restore the accepted P4.1 binary and any settings or language backups. Verify the restored version before running a game.
