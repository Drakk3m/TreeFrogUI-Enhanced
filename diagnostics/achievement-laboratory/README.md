# R36SX Achievement Laboratory

This original NROM program changes NES RAM and displays its values. PicoArch evaluates the achievement conditions outside the ROM. Python 3 is sufficient to build either ROM. Run python3 build_original.py or python3 build_readable.py from this directory. Both scripts output r36sx_achievement_lab.nes beside the source. Build the variants one at a time because the second build replaces that output.

| Variant | SHA-256 | NES content MD5 |
| --- | --- | --- |
| Original | d3e2e7b3f200dddd483904b91caa0baaa2970a3e413614cd4df9cfb9098b54ac | 19f8359d5ba99f87f0400f680f5d9ba2 |
| Readable | 469640e879ec66ed7c8bbaafbdbf6d11096abe5b7b8ec2f6501fdfe4bb4cb8f8 | 3def7781b89fc7a9bbe7b0f65f1bb130 |

The operator physically verified the original ROM. Both variants passed host verification. Install the selected ROM under roms/FC on the test SD, the corresponding definition as cubegm/enhanced/ra/local/r36sx-lab/lab.def, and the cover under roms/FC/.res. Install the PNG badges under cubegm/enhanced/ra/local/r36sx-lab/badges. Use FCEUmm, which the FC folder selects by default, and an enabled L2 mapping in FrogUI. Expect raw bit 8 on the test device.

| NES CPU and rcheevos address | Value |
| --- | --- |
| 0010 | Coins. A raises the count. |
| 0011 | Level. START advances it. |
| 0012 | Health, initially 100. B lowers it, DOWN heals. |
| 0013 | Defeated enemy count. UP raises it. |
| 0014 | One-frame enemy defeat pulse. |
| 0015–0017 | Controller previous/current/edge state. |
| 0018 | One-frame damage or streak-reset pulse. |

A unlocks First Coin at one coin and Coin Collector at ten. START advances from level one to two for Next Chapter. UP five times unlocks Monster Hunter. For Untouchable, use UP twice, B once, then UP three times; verify the first two enemies did not survive the damage reset. For Laboratory Master, reach twenty coins and level three while below full health, then press DOWN. Six unlocks total 36 points.

Reset only the lab on a powered-off **test card**. Back up and move the applicable progress snapshot out of cubegm/enhanced/ra/local/r36sx-lab. For the original ROM that means progress.v1. For the readable ROM that means progress-readable.v1, and progress.v1 only if its SHA-256 identity line identifies the readable ROM. Leave public RA cache, other profiles, game saves and official progress untouched. Preserve the lab definition and assets.

See [physical acceptance and architecture](../../docs/ra-poc-01-acceptance.md) and [ROM Studio handoff](../../docs/ra-poc-01-rom-studio.md).
