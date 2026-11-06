# Validated XMA recovery

Base: ReXGlue v0.10.0, f5337cdc947ff6d4c4196737e2c807a48f2a1fc2.

The context patch preserves a buffer when an entirely zero packet is followed by a plausible independent XMA2 frame in that same buffer, with no active loop. Other cases retain the SDK behavior. Detailed zero-copy diagnostics are retained. Helper tests cover recovery and rejected boundary cases.

User validated Caprakan battle music and menu music on September 11, 2026; full campaign and video synchronization remain unverified. The runtime patch originates from work under the ReXGlue BSD-3-Clause terms; preserve LICENSES/ReXGlue.txt. Project-authored additions use GPL-3.0-only; SDK-derived portions retain upstream BSD notices. See COPYRIGHT.md.

scripts/build-patched-runtime.ps1 retrieves the pinned source/dependencies, applies this patch idempotently and builds the runtime. scripts/build.ps1 installs it AFTER CMake copies packaged DLLs. Debug and Release are play-tested on limited routes. Sources here contain no game data.

## Bounded texture warning logging - October 2, 2026

texture-warning-logging.patch wraps the GPU logger's sinks for one exact warning family: texture fetch descriptors reported with an invalid type. The first eight messages remain, followed by a representative sample and count summary every 4096 occurrences. A final pending count is emitted when logs are flushed. Other warning families and all errors/critical messages pass through unchanged. The filter uses a mutex and constant-size state. It does not enable gpu_allow_invalid_fetch_constants or alter texture validation, rendering or saves.

The concurrent regression test generates 400008 warnings and retains 206 records including preserved unrelated warnings/errors. It verifies final counts and no duplicate final summary. The regular runtime build applies this patch and runs its test along with XMA recovery tests. Release runtime built successfully; updated game-session validation is pending. Diagnostic graphics source modifications remain excluded: only rexruntime is deployed, with the packaged GPU DLL unchanged.

The 20:59 Release gameplay session exposed warn-level auto-flush defeating summary aggregation (529687 summaries, 80970220 bytes). texture-warning-flush.patch raises GPU automatic flush to error level while preserving stricter configured levels, periodic flush, explicit final flush and immediate errors. A real spdlog logger regression now exercises warn auto-flush: 100000 warnings retain 57 records after explicit flush. Release rebuilt and deployed to the local Release Test; Subsequent native-720p gameplay validated bounded logging (65936-byte log, final summary of 28785 occurrences). Rendering and saves are unchanged.

## Opt-out Release frame CSV

release-frame-capture.patch enables only the existing lightweight frame CSV path
at guest GPU swap packets when perf_log_csv is nonempty. The stock SDK compiles
this call site out in Release even when its counter registry is built. MajinApp
opens the existing writer before GPU startup. Release capture stops after two
million rows and warns; no render, texture-validation, shader or scheduling logic
is changed. Debug keeps the original profiler path; Tracy remains disabled in Release.

The build script also builds/deploys rexgpu-xenos. It verifies the normalized
patched command-processor hash and refuses other tracked GPU-source modifications,
so abandoned graphics experiments cannot silently enter a normal build. Source
companions include the patched command processor. See PERFORMANCE_DIAGNOSTICS.md
for metric meanings, buffering limits and privacy choices.

The window-title follow-up uses release_frame_timing.h for a completed one-second
FPS window and keeps atomic frame snapshots active independently of CSV capture
when windowed title FPS is requested. It does not alter rendering, input or the
CSV frame-time samples. release-frame-capture-legacy.patch allows known prior
capture-only checkouts to migrate safely; unrelated graphics edits still fail
the source audit. The source companion includes the small timing helper.

## Multi-Threaded Startup PSO Pre-Compilation (`d3d12-blocking-pso-precompile.patch`)

Fixes the startup shader pipeline initialization loop in `pipeline_cache.cpp` so all available background compilation threads are fully spawned and completed before module launch (`await_creation_completion_event`). This converts in-game pipeline compilation stutters into a ~500ms (ROV) or ~25ms (RTV) pre-compilation step during title boot.

## D3D12 GPU Performance Optimizations & UI Loop Fix (`d3d12-gpu-and-ui-optimizations.patch`)

1. **Host Render Target (Default / RTV) Black Screen Fix:** Restores `execute_unclipped_draw_vs_on_cpu = true` in `draw_extent_estimator.cpp` and `primitive_processor_cache_min_indices = 4096` in `primitive_processor.cpp`, preventing screen-space draws from claiming all 2048 EDRAM tiles and corrupting 3D render targets on NVIDIA and AMD GPUs.
2. **Infinite UI Present Loop Fix:** Fixes `AchievementToastDialog` in `achievement_toast.cpp` and `rex_app.cpp` to only attach to `ImGuiDrawer` when toast notifications are visible, allowing `Presenter` to operate in `PaintMode::kGuestOutputThreadImmediately` and present only at actual guest game frame intervals (30/60 FPS) instead of uncapped 200 FPS UI repaints.
3. **VRAM Bandwidth Reduction:** Sets `gamma_render_target_as_unorm16 = false` in `flags.cpp` (32bpp `R8G8B8A8_UNORM` instead of 64bpp `R16G16B16A16_UNORM`).
4. **Native Anisotropic Filtering:** Restores `anisotropic_override = -1` in `texture/cache.cpp` (using native texture sampling instead of forced 4x AF).
5. **Draw Scissor Optimization:** Enables `execute_unclipped_draw_vs_on_cpu_with_scissor = true` in `draw_extent_estimator.cpp` to prevent spurious EDRAM tile range ownership transfers during 3D gameplay.
6. **Memexport Readback Disable:** Sets `readback_memexport = false` in `command_processor.cpp` to eliminate GPU-CPU pipeline stalls.

This patch is applied after `release-frame-capture.patch` and `fps60.patch`. Its `command_processor.cpp` section contains the `readback_memexport` change, while the Release frame counters come from `release-frame-capture.patch`. The 60 FPS frame pacer (`MajinFramePacer`) is located in `d3d12_presenter.cpp` (`fps60.patch`) at the presentation layer immediately prior to `Present()`, ensuring continuous command submission so the GPU processes draw calls evenly across the 16.6 ms window without 100% burst spikes. (A previous revision repeated them, so `prepare-sdk-source.ps1` failed on a clean checkout.)

## SDL audio playback-ratio hook (`sdl-audio-turbo-frequency.patch`)

Adds `rex::audio::SetPlaybackFrequencyRatio` / `GetPlaybackFrequencyRatio` and applies the ratio in the SDL audio callback with `SDL_SetAudioStreamFrequencyRatio`, clearing the queued stream when leaving fast-forward. `prototype-usa/src/cutscene.cpp` calls it for cutscene/dialogue fast-forward, so the game does not link without it. `prepare-sdk-source.ps1` applies it as step 1.6.

## Built-in FSR 1 / CAS present effects (`builtin-fsr1-cas.patch`)

The D3D12 and Vulkan presenters already contain precompiled AMD FidelityFX FSR 1 (EASU + RCAS) and CAS shaders, and their constants are computed in `presenter.h`. They were nevertheless compiled out unless the external FidelityFX SDK was configured, because the same `REX_HAS_FIDELITYFX_SDK` macro gates them. The patch defines that macro globally for every SDK target and re-exports it `PUBLIC` from `rexruntime`, so all binaries and SDK consumers see identical presenter class layouts. The temporal FSR 2/3 runtime still depends on the real SDK through `REX_HAS_FIDELITYFX_RUNTIME`, which stays off. `prepare-sdk-source.ps1` applies it as step 1.7; `present_effect` then accepts `fsr` and `cas`.
