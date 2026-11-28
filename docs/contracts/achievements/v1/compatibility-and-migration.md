# Compatibility and migration

Legacy and proposed v1 can coexist on the same test SD. The accepted binary ignores the proposed paths. The new fixture is an interchange test and cannot replace the working laboratory. Keep the accepted tags, binary hashes and P8/P9/P10 physical records intact.

## Legacy read path

Read the seven LF-separated ASCII legacy progress lines in order: R36SX-LAB-PROGRESS 1; local-player; r36sx-lab; NES; exact lowercase ROM SHA-256; revision 1; two lowercase hex digits in 00–3f. Require final LF, no extra lines, exact identity and known immutable definition. Zero mask is explicitly locked locally and absent or malformed progress is unknown. The original uses progress.v1. The readable variant uses progress-readable.v1 and can union a same-identity older readable record from progress.v1. This reader cannot infer a clock or player ownership.

Legacy lab.def has ten LF-terminated ASCII lines: magic R36SX-LAB-DEF 1, r36sx-lab, NES, exact ROM SHA-256, and six exact condition strings. Compare against the accepted fixture before migration. A supplied legacy mask maps bit zero to ID 1 and so on. Offer an explicit association to a new local profile UUID; resolve the immutable new set/content digest and derive the new event IDs, with timestamp unknown. Perform the same migration twice to get the same IDs. Keep old files unchanged. Validate a migrated export in isolation before enabling any in-place transition. This phase contains no migration writer.

Maintain the public v1 compatibility reader. Keep definition_fingerprint_md5 opaque. Public flags do not replace executable validation. A future authorized provider may link service IDs only with a verified mapping, physically tested adapter and authorization process. Independent local/official states are not inferred from presence of cache files. Keep multi-core and authorization changes separate from contract v1 Phase 1.

Open dependencies: exact Studio RA-02.1 audited source/export; device FAT behavior and memory/latency measurements; trust policy and registry generation for capability metadata; loader-controlled unarchived content, then archive and fileset adapters; trusted clock signal; UI for local profile/set selection and authorized official mapping. Defaults fail closed while preserving gameplay. No future producer should quietly qualify a second core based on compatibility of metadata alone.
