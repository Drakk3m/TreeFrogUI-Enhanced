# Contract validation checklist

- [x] Accepted published branches/tags and draft PR states verified.
- [x] Original ROM rebuilt and SHA-256 equals the physically accepted fixture.
- [x] Original six byte-exact executable conditions, titles, descriptions, points and badge associations represented.
- [x] Schema and semantic fixture checks include locked, partial, full, duplicate, unsupported, malformed and unknown states.
- [x] Valid second game fixture with a new content hash accepted without a compiled per-game list.
- [x] Trusted checker parses with pinned rcheevos. No service logic or definitions consulted.
- [ ] Generalized runtime and Studio implementation, including cross-card import and physical test-card verification. Future phase.
- [ ] Actual target storage, peak memory, display, audio and core compatibility beyond the accepted legacy NES POC. Future phase.

The accepted original RA-POC-01 operator record remains 10/10. Its result does not qualify the proposed interchange runtime.

## Reconciliation

- [x] Verified old FC export and actual Studio nes export with the proposed alias/console gate.
- [x] Verified native aliases, CubeGM association, relocation, cross-platform rejection, unknown aliases, conflicting content, and same names on different platforms.
- [x] Verified host launch default and explicit per-ROM FCEUmm override.
- [ ] Studio Apply/Create SD writer and Enhanced compatibility reader changes under their separate implementation phases.
- [ ] Original NES through generalized runtime on hardware.
