# Achievement data contract v1 · RA-POC-01.2

This directory is the authority for the proposed offline interchange. Consumers pin this directory by **Git commit** in Drakk3m/TreeFrogUI-Enhanced and record that commit alongside schema major 1 in their builds or dependency notes. Follow the contract at that revision. Do not copy and edit competing specifications. Transport the snapshot and its commit reference together. An accepted laboratory binary does not read the proposed directories.

- [Normative interoperability contract](achievement-data-contract-v1.md)
- [Source audit](source-audit.md), [compatibility and migration](compatibility-and-migration.md), [bounds and validation](validation-rules.md), [acceptance checklist](acceptance-checklist.md)
- [Platform mapping and launch routing](platform-and-routing.md), [machine-readable inventory](schemas/platform-mapping-v1.json)
- [ROM Studio agent handoff](studio-integration.md), [FrogUI plan](frogui-integration.md), [PicoArch plan](picoarch-integration.md)
- [JSON Schema](schemas/contract-v1.schema.json), [reference test matrix](fixtures/cases.json), [reference SD fixture](fixtures/reference-sd.zip)

The SD fixture uses the original NES bytes and begins with an explicit empty local snapshot. Partial and full overlays live under fixtures/cases and can be applied only to a disposable extracted fixture. All official-style examples are synthetic metadata. There are no official executable definitions.

Host tools require Python 3.10+, jsonschema 4 and a C compiler. Build the syntax checker from official rcheevos v12.3.0 at fd57e900758a9e3de343e7b0316b8a6059dce228:

    bash tools/build-condition-check.sh /path/to/pinned/rcheevos /path/to/condition-check
    python3 tools/test-fixtures.py --checker /path/to/condition-check
    python3 tools/test-platforms.py
    python3 tools/test-routing.py /path/to/accepted/FrogUI
    python3 tools/validate.py /path/to/extracted/reference-sd --checker /path/to/condition-check

Regenerate fixtures using python3 tools/make-fixtures.py --laboratory PATH_TO_ACCEPTED_LAB_SOURCE. The validator is a strict host fixture/package validator, not a device runtime or a recovery importer. The checklists specify additional cold-start, FAT and console verification required for a future implementation.
