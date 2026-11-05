# Recompilation Ship's Log and Evidence Journal (USA 0.0.0.2)

This document tracks **all verified technical milestones**, **failures and discarded paths (to prevent repeating mistakes)**, and the **PowerPC Address Translation Table (PAL $\rightarrow$ USA)** for *Majin and the Forsaken Kingdom*.

---

## Anti-Loop and Anti-Hallucination Governance Rules (Active)

1. **Circuit Breaker Rule (Max 2 Attempts):** If any script, build command, or execution step fails **2 consecutive times**, execution must halt immediately. The failure is recorded in the table below and analyzed before attempting any further changes.
2. **Mandatory Binary Evidence (Zero Address Hallucination):** No memory address (`0x82xxxxxx`) can be added to configuration `.toml` files or source code without real hex bytecode extraction from the USA binary.
3. **Checkpointed Phased Execution:** Each task checklist item is verified, logged into reports, and updated here before moving to the next.

---

## 1. Verified Technical Milestones

| Date | Phase | What Was Validated | Evidence / Outcome |
| :--- | :--- | :--- | :--- |
| 2026-11-05 | ISO Inspection | XDVDFS file system inspection on American dump | Partition found at `0xFD90000`, containing `4,740` files (`1,828,104,203` bytes). |
| 2026-11-05 | Data Extraction | Asset and executable extraction | Extracted into `game-data-usa/`. ISO backup preserved. |
| 2026-11-05 | Header Analysis | Title ID, Media ID, and encryption verification | `Title ID: 4E4D0816`, `Media ID: 76FDB642`, `Version: 0.0.0.2`, `SHA256: 291F89E2...BA4C2C`. |
| 2026-11-05 | **Phase 0** | Toolchain and compiler verification | Visual Studio 2022 Community with `MSVC x64 (17.14.29)`, `CMake 3.31.6`, `Ninja 1.12.1`, and `Clang 19.1.5`. Verified `-std=c++23 -fasync-exceptions -gline-tables-only` with code exit `0`. |
| 2026-11-05 | **Phase 2.1** | AES-128 decryption and PE section unpacking | Developed `scripts/unpack-xex-usa.ps1`. Validated `MZ` and `PE\0\0` headers at `0x110`. Extracted 18 PE sections (`loaded-image.bin`, `loaded-text.bin`, `loaded-rdata.bin`, `loaded-pdata.bin`). |
| 2026-11-05 | **Phase 3** | Discovery of the Exact Shift Law (PAL $\rightarrow$ USA) | Proved that between `0x82150000` and `0x821C0000` the offset is `0x0`, and from `0x821C0000` to the end of `.text` the shift is exactly `-0x70` (`-112` bytes). Verified 442 functions with 100% opcode assertions (`353` thunks, `56` variants, `3` member thunks, `30` overrides). |
| 2026-11-05 | **Phase 1** | Local ReXGlue SDK build (v0.10.0) | Cloned `rexglue-sdk` v0.10.0 with 14 submodules, resolved Windows symlinks for `libmspack`, applied audio/logging/framing patches, and verified unit tests (`400008 warnings bounded to 206 records`). |
| 2026-11-05 | **Phase 2.3 & 4** | Complete AOT C++23 Codegen (`rexglue.exe codegen`) | Generated 347 C++23 files in 28.3s (`exit 0`). Recompiled **34,247 PowerPC functions**, including all 442 verified functions from the TOMLs. |
| 2026-11-05 | **Phase 5 & 6** | Native compilation & first D3D12 boot | Compiled native `majin.exe` Release (`[180/180]`). Game booted in Direct3D 12 on NVIDIA GeForce RTX 3060 Ti at 60 FPS (`runtime-usa-20261105-162858.log`). Verified save hooks, thread hooks, and 60 FPS hooks with **zero dispatch errors**. |
| 2026-11-05 | **Phase 7.1** | Native GUI Launcher | Compiled `Majin FK Launcher.exe` with support for `720p/1080p/1440p/4K`, internal scale `1x/2x/3x`, `FXAA`, `Default (RTV)` / `ROV`, `30/60 FPS`, and `Controller / Keyboard+Mouse`. |
| 2026-11-05 | **Phase 7.2** | Shader Cache Consolidation & Blocking Startup Precompile | Unified shader microcode and pipeline description caches across scales. Implemented multi-threaded blocking pre-compilation at startup in `pipeline_cache.cpp` (`191` ROV PSOs created in `529 ms`; `117` RTV PSOs created in `24 ms`). |
| 2026-11-05 | **Phase 7.3** | Default (`RTV` / `kHostRenderTargets`) Black Screen Fix | Fixed black 3D screen by restoring `execute_unclipped_draw_vs_on_cpu = true` in `draw_extent_estimator.cpp` and `primitive_processor_cache_min_indices = 4096` in `primitive_processor.cpp`. Verified: full 3D rendering operational in Default mode! |
| 2026-11-05 | **Phase 7.4** | Telemetry & FPS Audit (`200 FPS` & GPU Load Analysis) | Discovered infinite UI present loop: `AchievementToastDialog` permanently attached to `ImGuiDrawer`, disabling `PaintMode::kGuestOutputThreadImmediately` and presenting duplicate frames at 200 FPS on the UI thread. |
| 2026-11-05 | **Phase 7.5** | GPU Performance & Bandwidth Optimization | Made `AchievementToastDialog` attach on-demand; set `gamma_render_target_as_unorm16 = false` (saving 50% color RT bandwidth); restored `anisotropic_override = -1` (native); set `execute_unclipped_draw_vs_on_cpu_with_scissor = true`; set `readback_memexport = false`. Verified: **99.93% of frames rock-solid at 60 FPS** with 252 precompiled PSOs! |

---

## 2. Failure Registry & Discarded Paths (Do Not Repeat)

| Attempt / Event | What Happened (Failure) | Root Cause | Permanent Resolution |
| :--- | :--- | :--- | :--- |
| Direct import via PAL script (`disc-image.cs`) | Returned `Supported: False` and refused extraction. | Script compares SHA-256 with the hardcoded PAL hash (`660CAE38...`). | Created standalone region-free extractor in `scripts/extract-iso.ps1`. |
| Download precompiled SDK zip | Blocked by Windows Defender (`PUA:Win32/GameHack`). | Unsigned reverse-engineering tool binaries trigger heuristic alerts. | Build local SDK directly from audited source code via `scripts/build-sdk-local.ps1`. |
| 3D rendering with `RenderingPath = Default` on NVIDIA/AMD (Pre-Test 2) | FMVs, main menu, pause, and map rendered normally, but 3D in-game scene was black. | `execute_unclipped_draw_vs_on_cpu` was inverted to `false` in `draw_extent_estimator.cpp`. Unclipped screen-space draws claimed all 2048 EDRAM tiles, triggering destructive resolves that corrupted 3D render targets in RTV mode. | Restored `execute_unclipped_draw_vs_on_cpu = true` in `draw_extent_estimator.cpp` and set `--gpu_allow_invalid_fetch_constants=true`. |
| External overlay showing fixed `200 FPS` with high GPU load | Game internally rendered at 60 FPS (`16.6ms`), but external GPU overlays reported 200 FPS. | `AchievementToastDialog` registered `ImGuiDrawer` permanently on boot. With dialogs non-empty, `ImGuiDrawer::Draw()` called `RequestUIPaintFromUIThread()` in an infinite loop, forcing upscale + ImGui + `Present(0, ALLOW_TEARING)` at 200 FPS. | Made `AchievementToastDialog` attach only when a toast is queued and detach when idle, keeping `Presenter` in `PaintMode::kGuestOutputThreadImmediately` (presenting only real 30/60 FPS game frames). |

---

## 3. Comparative PowerPC Translation Table (PAL `0.0.0.3` vs USA `0.0.0.2`)

| Component / Symbol | PAL Address (`0.0.0.3`) | USA Address (`0.0.0.2`) | Verified PowerPC Opcodes / Assembly | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Image Base** | `0x82000000` | `0x82000000` | Fixed virtual base for XEX2 executable | Verified |
| **Entry Point (CRT Start)** | `0x8267xxxx` | **`0x8267C090`** | `7D8802A6` (`mflr r12`), `4815FA15` (`bl 0x827DBAA4`), `9421FE10` (`stwu r1,-0x1F0(r1)`) | Verified |
| **`.rdata` Section (Vtables)** | `0x82000600` | **`0x82000600 .. 0x82119B3C`** | `1,152,316` bytes (`0x11953C`) of C++ constants and vtables | Verified |
| **`.pdata` Table (Exceptions)** | `0x82119C00` | **`0x82119C00 .. 0x82147450`** | `186,448` bytes (`23,306` function entries of 8 bytes each) | Verified |
| **`.text` Section (CPU Code)** | `0x82150000` | **`0x82150000 .. 0x82ADEDC4`** | `10,022,340` bytes (`0x98EDC4`) of PowerPC instructions | Verified |
| **`.data` Section (Globals)** | `0x82B40000` | **`0x82B40000 .. 0x82DFC0A0`** | `2,867,360` bytes (`0x2BC0A0`) | Verified |
| **`WriteFile` Wrapper (Save)** | `0x8267B6C8` | **`0x8267B658`** (`-0x70`) | `7D8802A6` (`mflr r12`), `48160451`, `9421FF80` (`.pdata=True`) | Verified |
| **Save Caller (`LR`)** | `0x821B9DF4` | **`0x821B9DF4`** (`+0x0`) | `0x821B9DF0`: `484C1869` (`bl 0x8267B658`, return `LR=0x821B9DF4`) | Verified |
| **`GetLastError` Wrapper** | `0x8267B190` | **`0x8267B120`** (`-0x70`) | `48002288` (`b 0x8267D3A8`) | Verified |
| **`ContentCreateEx` Wrapper** | `0x8267AFE0` | **`0x8267AF70`** (`-0x70`) | `7D8802A6 9181FFF8 FBE1FFF0 9421FF90` (`.pdata=True`) | Verified |
| **`GetOverlappedResult` Wrapper** | `0x8267B510` | **`0x8267B4A0`** (`-0x70`) | `7D8802A6 9181FFF8 FBC1FFE8 FBE1FFF0` (`.pdata=True`) | Verified |
| **Engine Delta Time (`30.0f`)** | `0x82162238` | **`0x82162238`** (`+0x0`) | `7D8802A6 4867986D DBE1FFD0 9421FF70` (called at `0x821616E8`: `48000B51`) | Verified |
| **`SceneTitleTitle::Update`** | `0x821CC390` | **`0x821CC320`** (`-0x70`) | `7D8802A6 4860F775 DBE1FFB0 9421FF00` (`.pdata=True`, vtable `0x820EC3C4` slot 7) | Verified |
| **Vtable `SceneTitleTitle`** | `0x820EC3C4` | **`0x820EC3C4`** (`+0x0`) | Loaded at `0x821CBEA0`: `3D60820F 392BC3C4 91230000` (`stw r9,0(r3)`) | Verified |
| **Member Thunks (5 insns)** | `0x824419C0`, `0x82743130`, `0x82750410` | **`0x82441950`, `0x827430C0`, `0x827503A0`** | `8063xxxx 81630000 814Bxxxx 7D4903A6 4E800420` (`-0x70`) | Verified |
| **Overrides (`30` functions)** | `0x822855F0 .. 0x829CDEE8` | **`0x82285580 .. 0x829CDE78`** (`-0x70`) | 100% verified by opcode in `prototype-usa/analysis-overrides.toml` | Verified |

---

## 4. Frame Time Telemetry Across Optimizations

| Session Log | Configuration | PSOs at Boot | Total Frames | Frames <= 20ms (60 FPS) | Stutter Spikes (> 50ms) |
| :--- | :--- | :---: | :---: | :---: | :---: |
| `runtime-usa-20261105-183229` (1st Default Test) | `1x Native` | `117` | `24,932` | `99.60%` | **24** |
| `runtime-usa-20261105-185837` (2nd Test — 8 min) | `2x Scale + FXAA` | `214` | `28,598` | `99.58%` | **10** |
| `runtime-usa-20261105-190704` (3rd Test — 2 min) | `1x Native` | **`252`** | `7,192` | **`99.93%`** | **2** |

---

## Phase 8: In-Game 3D Cutscene Skip Hook

- **Problem:** Unlike 2D FMVs (which supported holding `Back` to skip), in-engine 3D cutscenes and dialogues could not be skipped in the original game.
- **Implementation:**
  - Added [`prototype-usa/src/cutscene.cpp`](file:///C:/Users/kelvy/Desktop/Recomp/prototype-usa/src/cutscene.cpp) intercepting scenario demo end checks:
    - `DEMO_END_CHECK` (`0x821836C0`)
    - `DEMO_CAMERA_END_CHECK` (`0x82183778`)
    - `setDemoGetEnd` (`0x822351D0`)
  - Defined CVAR `majin_skip_cutscenes` (default `true`).
  - Holding `Select/Back` on controller (`0x0020`) or `Tab` on keyboard for ~15 frames (~0.5s) immediately triggers `return 1` on demo completion checks, causing the scenario engine to jump straight to post-demo cleanup (`DEMO_PLAY_POST_FUNCTION`) and restore gameplay control.
  - Added Launcher GUI option **"Skip cutscenes"** (`Hold Select / Tab` vs `Disabled`) and integrated with [`run-usa.ps1`](file:///C:/Users/kelvy/Desktop/Recomp/scripts/run-usa.ps1).
