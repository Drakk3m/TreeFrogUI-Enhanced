# P5.2 Offline RetroAchievements Live Area

## Contract audit

The local ROM Studio v87.9.2 exporter (studio/ra_sd_export.py) and existing SD backup agree on the unversioned UTF-8 profile object. It contains exactly username and avatar. The username is trimmed, nonempty, at most 80 characters and contains no ASCII control characters. The avatar value is either the literal avatar.png or null. The exporter validates the account association before exporting PNG bytes. A null value must not cause a stale avatar to be loaded. The inspected backup has a 64 × 64 PNG.

The unversioned rom_identity.json object is keyed by a path relative to the SD root. Values contain platform, console_id, game_id, ra_hash, method, size, mtime_ns and metadata_only. The associated pack is cache/<console_id>/<ra_hash>/gamepack.json. Studio validates schema_version 1 and the console ID, game ID and canonical RA hash before export. The observed achievement entries contain id, title, description, points and badge. Points are properties of definitions. Neither these entries nor the exported profile report earned counts, earned points, hardcore status, unlock dates or completion percentages. Studio's PC profile cache and separate journal handling do not establish exported global unlock statistics or an authoritative console aggregate. The displayed muted zero is a requested visual placeholder for unknown progress. It is not sourced from the profile and must be replaced only with an authoritative earned count. FrogUI performs no count traversal.

The live Enhanced Home renderer already provides a reserved upper-right area between its status row and ribbon. Render the read-only profile there, using the existing font and native RGB565 icon scaler. Validate across a frontend initialization boundary. Use a native neutral glyph if valid profile data has no usable avatar. A missing, invalid or unsupported profile hides the component. No new navigation destination and no cache writes or network calls.

## Resource limits and verification

The profile reader bounds JSON to 4 KiB and rejects unsupported keys, duplicate fields, malformed UTF-8/JSON and oversized names. FrogUI takes a snapshot at frontend initialization, including after the existing game launcher restarts it. The avatar is read only when the profile authorizes the literal basename. The native image reader accepts PNG under its existing 1 MiB encoded / 640 × 480 source limits. Studio can export larger PNG dimensions, in which case FrogUI draws the neutral fallback. The observed 64 × 64 production avatar is within range.

Retained avatar pixels and alpha are 4,800 bytes. The snapshot and icon descriptor add under 1 KiB. Temporary JSON parsing uses a fixed 4 KiB input plus small stack objects. The existing bounded image decode can hold up to 1 MiB of encoded input and is charged at most 6 MiB of decoder allocation requests, with a 2 MiB ceiling for an individual decoder allocation. Scratch conversion reuses the existing thumbnail buffer. No filesystem lookups or image decoding occur during profile draw.

The parser accepted the local Studio profile without printing its private contents. Host tests use synthetic identity and PNG fixtures. A verified earned-statistics test requires an authoritative export with account association, eligible unlock identity and completeness. The available pack data cannot provide that evidence. The bundled visual capture uses a mock gamertag and original geometric avatar.

## Physical retest and rollback

With the console powered off, back up cubegm/cores/frogui_libretro.so on the separate test SD. Install the candidate and verify its checksum. Use a ROM Studio export with profile.json and avatar.png under cubegm/enhanced/ra.

1. Check the gamertag, avatar, trophy and muted placeholder zero in the upper right. Verify no overlap with the ribbon or battery, including with reduced motion and long names.
2. Test the missing or invalid profile and avatar cases on the test SD. A valid profile without usable art should show a neutral icon. Check that Classic has its normal Home.
3. Traverse Recents, Apps and Settings. Open a platform, verify P5.1 CSV titles, enter Details and launch a known game. Confirm the frontend and profile return.
4. To roll back, power off and restore the saved binary. The RA export can stay on the SD. Use the accepted P5 hotfix for a physically accepted rollback point.
