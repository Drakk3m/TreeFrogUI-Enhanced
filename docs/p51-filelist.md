# P5.1 Per-platform filelist

## Preimplementation audit

The local CubeGM SD copy under Downloads contains nine nonempty CSVs with records. All nine are headerless UTF-8 with LF and exactly three columns in the observed records. Column one is the physical filename, column two is the display name, and column three may be empty or contain text. Five other fixtures are empty or BOM/CRLF-only. The TreeFrogUI release script and local staged overlay confirm the positional schema using a headerless three-column record. FrogUI has no existing CSV reader. None of the sampled records contains a path separator in column one. The firmware's behavior for relative nested paths and non-UTF-8 encodings remains unverified. Support the supplied literal header as an optional interchange convenience, with data-first matching compatible with the stock files.

FrogUI's `scan_directory` owns discovery, filtering, sorting and the cached listing. It excludes CSV from ROM listings. Each `DirEntry` has a 256-byte physical name and a directory flag. The existing cache bounds its reader to 100,000 entries. Entering a nested folder rescans that folder independently. P5's `prepare_coverflow_title` feeds both CoverFlow and minimal Details. The launch action uses `current_path` with `entries[selected_index].name` and the existing `launch_by_path`. The artwork cache uses those same physical components before resolving native art. Identity, native listing/cache bytes, and launch/artwork callers do not need changes.

Read only the top-level platform's optional filelist at the end of its Enhanced scan, including on a native cache hit. Associate rows against physically discovered regular listing records using exact filename bytes. For nested folders fall back to the basename with its final extension removed. No relative path or ambiguous basename matching is used. Classic keeps its current behavior and the same native cache. The shared P5 title preparation consumes the optional title, preserving UTF-8 shortening. Filename fallback in Enhanced applies independently of the classic Hide Extensions preference.

## Policy and bounds

A valid nonempty title on the first matching record wins. An empty/invalid earlier record does not prevent a later valid title. Matching requires exact filename bytes even on case-insensitive media. The discovered listing supplies all row membership, order, artwork paths, navigation identity and launch paths. Recents and Favorites continue using their existing stored names and identifiers.

A read error or file over 1 MiB drops the whole optional presentation layer. Malformed physical lines are skipped independently. A decoded filename or display title can hold up to 255 UTF-8 bytes. Embedded line breaks in fields and nested filename paths are unsupported. The third field is ignored as presentation metadata. The current listing owns all titles. It drops them on rescan, platform exit or deinitialization. The title buffer uses an offset table of at most 400,000 bytes and a pool of at most 524,288 bytes. The remaining ROMs still use their filenames when the title pool is full. Reallocation can transiently hold both the previous and next pool, for a worst-case table-and-pool peak of 1,186,432 bytes excluding allocator overhead and small reader buffers. There is no per-movement or per-frame CSV I/O.

## Physical acceptance and rollback

Use a separate TreeFrogUI test SD. With the console powered off, back up the existing shared object at cubegm/cores/frogui_libretro.so and the settings file. Replace only the shared object with the verified candidate. In one existing platform place a local filelist.csv with an exact filename and a custom title, one real filename with an empty title, and a row for a deliberately nonexistent filename. Keep another platform without a CSV and leave another real ROM out of the CSV. Use your own test ROMs. Include stock-style headerless data and an example with a quoted comma or UTF-8 title.

1. Open both platforms in Enhanced. Check custom titles in CoverFlow and Details. Verify filename fallbacks for empty, omitted or missing metadata. Check that the nonexistent ROM never appears and that malformed rows leave the collection usable.
2. Keep the existing artwork for the renamed presentation title. Confirm the same cover appears and the original ROM launches by using A in Details. Verify B restores the game, then the platform on Home.
3. Traverse the Home utilities, test classic mode, and try a larger collection. Confirm an optional CSV refreshes after leaving and reentering a platform. Test a nested folder with a same-named ROM and confirm filename fallback.
4. For rollback, power off and restore the backed-up P5 Home Ribbon Hotfix binary and any changed settings. The filelist itself can stay on the test card.

## Verification

The new reader accepted 853 records across all 14 local CubeGM filelists, matching a standard-library CSV count. Nine included data and five were empty. Host regressions pass with ASan and UBSan. The host scanner fixture associated 1,500 files, retained 38,796 bytes for its table and title pool, and took 1 ms without the CSV or 3 ms with it in this run. Its largest possible pool-growth overlap adds 16,384 bytes for about 55,180 bytes total, excluding the parser stack, stdio and allocator overhead. These figures do not predict SD latency on hardware.

The SF3000 artifact is 1,393,524 bytes, up 2,872 bytes over the accepted P5 hotfix. It retains MIPS32r2 little-endian, o32 and hard-float with the same Libretro exports and direct dependencies and no added versioned imports.
