# FrogUI implementation plan

1. Preserve ra_lab_gallery.h and legacy rendering paths behind existing dispatch. Add an opt-in versioned catalog reader outside rendering, using the native launcher selection and Game Details. Apply path and content association hints without hashing the entire SD. Missing metadata and corrupt optional assets use ordinary fallbacks.
2. Extend the shared gallery model with identity, metadata, compatibility and progress as independent inputs. Reuse the L2 route, cached thumbnails and existing monochrome/color/Local indicators. Unknown shows neutral state, never locked official progress. Existing public entries are metadata-only until independent evidence is available.
3. At frontend init and return from game, read the scoped snapshots, union valid IDs and update galleries/Home with a single bounded model. Counts are distinct IDs in the displayed scope; points are a separate total. Keep diagnostics separated from normal/official aggregation. Do not evaluate conditions in FrogUI. Keep Classics, launch paths, navigation, RA profile and avatar rendering behavior.
4. Once PicoArch has current compatibility evidence, show concise missing definition, unsupported core/provider, or unknown state without blocking launch. Take runtime evidence from an agreed host result channel in a later implementation, not a writable SD capability assertion.

Verify the original lab first with the ordinary POC still working. Expand only after test-card acceptance, including restart, load-state and no duplicate popups on prior unlocks. No edits to rendering or production runtime in RA-POC-01.2.

## RA-POC-01.3 reconciliation gate

Use the pinned contract revision distributed with the handoff. Implement a general normalization boundary using existing platform/folder mappings. Legacy public association accepts the documented logical token or launcher alias only after unique canonical and console matching. Keep exact path, stat and pack cross-checks. v1 consumes canonical lowercase platform and actual SD-relative locator. Do not alter ordinary ROM discovery, default core or gallery presentation under the alias adapter. Test the included Studio export through the real compatibility reader once this adapter exists.

Test the normal roms/nes launch defaults to QuickNES and the existing exact per-ROM override selects the approved FCEUmm. Keep roms/FC default and legacy regression behavior. Use the same gallery/Home for generalized verified progress, keeping legacy public metadata and diagnostic totals distinct. Fail closed for unknown platform/core/console, unsupported provider, alias collisions and mismatched ROM identity.
