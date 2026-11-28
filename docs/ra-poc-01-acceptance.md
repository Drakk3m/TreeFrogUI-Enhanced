# RA-POC-01 physical acceptance

The operator confirmed 10/10 tests on R36SX. This record refers to the original laboratory ROM and the revised HUD/count builds. The readable variant was verified in host emulation.

| Physical test | Observed result |
| --- | --- |
| Unlock and return to FrogUI | Pass |
| Colored badge and Local status | Pass |
| Home counter updates | Pass |
| Full console shutdown and restart | Pass |
| Progress and counters persist | Pass |
| Duplicate unlock suppression | Pass |
| Multiple unlocks in one session | Pass |
| Earlier save state keeps durable unlocks | Pass |
| Other ROMs leave runtime inactive | Pass |
| Official statistics excluded | Pass |

| Component | Exact tested provenance |
| --- | --- |
| FrogUI | Source 15fbea977ee4e7e01d2a1659b8fa4a2e981aba9b. MIPS binary SHA-256 8a325bc493ec2cc5bf016d69c421d116c054fc480d66296ae532377ad4a1e737. |
| PicoArch | Source 840f52ddc23a518aa26b50d92457e5f9ad211866, based on 26d0872c2a994da5be353020e56a661a9f2ddfa0, libretro-common 88252880232fb8488296dc0cd4c57d6fb8159b3d. MIPS binary SHA-256 a41a6d663a089afcf69699310921da3d7ba2f97c697fbb3e8c9e248af39fd3c1. |
| FCEUmm on test SD | Existing v1.8.0_u binary SHA-256 c318c4217bf8c0396161e9a896289d85aa92219e5ef6abf44e89161c2e4954d8. Exact device-core source revision unknown. Host FCEUmm source e6111e684e7a7761f3f1d6c80d0a825e2c8cdc7e. |
| rcheevos | v12.3.0, fd57e900758a9e3de343e7b0316b8a6059dce228. |
| ROM | Original Mapper 0, SHA-256 d3e2e7b3f200dddd483904b91caa0baaa2970a3e413614cd4df9cfb9098b54ac. |
| Installed package sequence | Original test payload, full-path runtime fix, revised HUD package and final count-only FrogUI. Keep the R36SX L2 mapping enabled. The consolidated package records the final byte set. |

PicoArch owns retro_run, core loading, save-state restoration, audio and the compositor. The runtime verifies the actual content bytes or reads the effective path for a full-path core. The FCEUmm test core exposes 2 KiB of NES CPU RAM through RETRO_MEMORY_SYSTEM_RAM. The rcheevos APIs used are rc_runtime_init, rc_runtime_activate_achievement, rc_runtime_validate_addresses, rc_runtime_do_frame, rc_runtime_reset and rc_runtime_destroy. Checks run after forward frames, following a six-frame warmup. Rewind skips evaluation and clears transient state. Successful save-state loads and emulator resets also clear transient state. Durable unlocks are immutable within the identity/revision scope.

The active queue is bounded to six events. A timed top-right badge/title panel slides in and out using PicoArch's existing RGB565 compositor buffer, so the game frame remains borrowed and unmodified. A short PCM chime mixes with the active audio path. Save failures are logged and use a gray notification badge. Existing unlock bits and session event deduplication prevent duplicate notifications. Game Details and the existing gallery use identity-bound progress, grayscale for locked and color for unlocked. Home counts set bits for the installed laboratory definition on frontend startup. The number contains diagnostic records only, even though the HUD also shows a linked RA avatar when available.

Scope and risks: only the laboratory ROM identities and FCEUmm are covered. This is local soft-mode evaluation. Physical timing, peak target memory, behavior across FAT power loss and other core memory maps are unmeasured. Full fsync is synchronous at unlock and may stall a frame. A failed directory sync after rename can leave uncertain durability. An earlier save state may permit earning future unlocks while never clearing ones already saved. The namespace is single-profile and compiled for two diagnostic ROMs. The public cache, official totals and submission flow are outside this runtime.

Use the annotated acceptance tags in both source histories and the original ROM and definition as the recommended RA-POC-02 baseline. Keep P9/P10 verification records separate. Start any later core/provider generalization from the tested contract without changing this diagnostic fixture.
