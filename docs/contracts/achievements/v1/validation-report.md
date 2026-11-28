# RA-POC-01.2 host verification

- JSON Schema 2020-12 compiled and used by the reference validator. Twenty fixture cases passed, consisting of seven accepted variants and thirteen rejected variants. Valid cases include empty, partial and full snapshots, cross-slot duplicates, absent progress, absent metadata and a second ROM identity. The latter is a linkage fixture with an original-derived ROM and a different SHA-256. No on-device evaluation of that variant is asserted.
- The original NES ROM was regenerated from accepted source and compared against the accepted SHA-256. The six strings match lab.def byte-for-byte. Local titles, descriptions, six original badge assets and 36 total points were carried forward. SHA-256 addresses the raw ROM and the compact asset references independently.
- Host condition parser built from pinned rcheevos v12.3.0 with ASan and UBSan. Only independently gated direct-byte capability conditions were submitted. The known official-style fixture contains synthetic public metadata with an opaque fingerprint; no official executable content is included. The legacy FrogUI ra_game host test passed under both sanitizers.
- The legacy examples preserve the accepted locked snapshot and an actual sanitized host-generated six-unlock snapshot. Proposed partially/full unlocked examples are synthetic contract fixtures. They have unknown clock status and no official status.

Reproduce with tools/build-condition-check.sh and tools/test-fixtures.py as shown in README. The validation does not cover physical performance, power-loss recovery on FAT, cross-card Studio database migrations, authorization for official definitions or unregistered provider support. That evidence is reserved for implementation and integration tests.

## RA-POC-01.2.1 reconciliation results

All original 20 passed after adding the semantic platform gate. Eighteen added platform/fixture checks passed, including exact original content and conditions, migration, same-name separation and the byte-preserved actual Studio public export. The accepted C reader's pre-change rejection was verified separately by the previous compatibility audit. The new resolver is a host reference; runtime implementation is pending.

Three host routing checks compiled the unchanged FrogUI resolver and launcher logic with ASan/UBSan, using a marker-only launch sink. They prove QuickNES for Studio's default roms/nes, FCEUmm for accepted roms/FC and precedence of the exact per-ROM FCEUmm override without affecting another ROM. They do not qualify QuickNES or certify hardware execution of generalized v1. Physical acceptance remains restricted to the original lab on the previous firmware. The historical Studio v88.3.23 build was not tested.
