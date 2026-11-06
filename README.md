<p align="center">
  <img src="assets/launcher/logo.png" alt="Majin Recompile" width="900">
</p>

<p align="center">
  An unofficial Windows recompilation of <strong>Majin and the Forsaken Kingdom (Xbox 360)</strong>.<br>
  Windows x64 · Direct3D 12 · Dual PAL & NTSC-U Edition Support · Enhanced Hybrid Recompilation
</p>

---

## Overview

**Majin Recompile** is an advanced static recompilation project that translates the original PowerPC machine code of *Majin and the Forsaken Kingdom* (Xbox 360) into native, high-performance C++23. It utilizes the **ReXGlue SDK (0.10.0)** and a customized Direct3D 12 graphics runtime derived from Xenia to deliver modern PC features, smooth 60 FPS gameplay, and selectable high-resolution rendering.

> [!IMPORTANT]
> **Strict Legal & Copyright Notice:**  
> This repository distributes **NO copyrighted game assets, ISOs, executables, textures, audio, video, or precompiled game binaries**.  
> Users must provide their own legally acquired copy of *Majin and the Forsaken Kingdom* for the Xbox 360. All decompilation, function mapping, and static recompilation steps are performed locally using the scripts in this repository.

---

## Supported Game Editions

| Region / Edition | Executable Version | Title ID | Media ID | Support Status |
| :--- | :---: | :---: | :---: | :---: |
| **European / PAL** | `0.0.0.3` | `4E4D0816` | `67BD87EF` | Supported (`prototype/`) |
| **American / NTSC-U** | `0.0.0.2` | `4E4D0816` | `76FDB642` | Supported (`prototype-usa/`) |

---

## Key Enhancements in this Fork

### 1. Full American (NTSC-U 0.0.0.2) Static Recompilation
- **Deterministic Shift Law Discovery:** Discovered that between `.text` base `0x82150000` and `0x821C0000`, 100% of addresses match between PAL and USA. Beyond `0x821C0000`, every function is shifted by exactly **`-0x70` bytes (`-112` bytes / 28 instructions)** due to an alignment padding difference in PAL.
- **Complete PowerPC Function Coverage:** Mapped and verified **34,247 functions** via `.pdata` exception tables, virtual dispatch thunks (353 thunks, 56 variants, 3 member thunks), and vtable analysis, with **zero unresolved branch targets**.
- **Ported Runtime Hooks:** USA-specific native hooks for asynchronous `WriteFile` save completion, overlapped I/O results, thread descriptors, and 60 FPS frame delta interpolation.

### 2. Multi-Threaded Startup Shader Pre-Compilation
- Implemented multi-threaded blocking PSO pre-compilation during title initialization in Direct3D 12 (`pipeline_cache.cpp`).
- Translates microcode and compiles all known graphics pipeline descriptions on background worker threads before game launch.
- Eliminates in-game shader compilation stutters (over **250+ Direct3D 12 pipelines** compiled in ~1.5s on boot).

### 3. Direct3D 12 Default (`RTV` / Host Render Targets) Fix for NVIDIA & AMD GPUs
- Fixed a critical regression in `draw_extent_estimator.cpp` where `execute_unclipped_draw_vs_on_cpu` was inverted to `false`.
- In the original behavior, unclipped 2D/screen-space clear draws claimed all 2048 tiles of the 10 MB Xbox 360 EDRAM, triggering destructive full-range resolves that blacked out 3D scenes on NVIDIA and AMD GPUs in Default mode.
- Restoring CPU-side vertex height estimation allows **Default (`RTV`)** mode to render complete 3D graphics with standard Render Target Views, without requiring slow Rasterizer-Ordered View (ROV) interlocks.

### 4. UI Presenter Infinite Loop Fix (Resolving the 200 FPS Drain)
- Fixed an architectural bug where `AchievementToastDialog` was permanently registered to `ImGuiDrawer`, preventing the UI drawer list from ever becoming empty.
- This forced the presenter into `PaintMode::kUIThreadOnRequest` with uncapped `swap_chain->Present(0, ALLOW_TEARING)`, running upscale pixel shaders and command list submissions in an infinite loop at 200 FPS even during black screens or idle pauses.
- Toasts now connect on-demand only when active, keeping the presenter in **`PaintMode::kGuestOutputThreadImmediately`** and presenting exclusively at the game's actual 30/60 FPS cadence.

### 5. GPU Efficiency & VRAM Bandwidth Tuning
- **32-Bit Gamma Render Target Emulation:** Switched `gamma_render_target_as_unorm16` to `false` (emulating gamma render targets using 32bpp `R8G8B8A8_UNORM` instead of 64bpp `R16G16B16A16_UNORM`), saving 50% color render target VRAM bandwidth.
- **Native Texture Filtering:** Restored `anisotropic_override = -1` (honoring original game texture sampling instead of forcing 4x AF across all textures).
- **Scissored Draw Estimation:** Enabled `execute_unclipped_draw_vs_on_cpu_with_scissor = true` to prevent false EDRAM tile ownership claims during 3D scene passes.
- **Memexport Pipeline De-Stall:** Disabled `readback_memexport` to eliminate synchronous GPU-to-CPU readbacks during gameplay.
- **Result:** **99.93% of frame times locked at 60 FPS (`<= 16.6ms`)**.

### 6. Enhanced Native GUI Launcher
- **Modern UI/UX Design:** Dark Slate cards with zero image layering dependencies or DPI scaling clipping.
- **Resolution Output:** Selectable output between `720p` (`1280x720`), `1080p` (`1920x1080`), `1440p` (`2560x1440`), and `4K` (`3840x2160`).
- **Internal Draw Scaling:** `1x` (native 720p), `2x` (1440p), and `3x` (4K).
- **Rendering Backends:** **Default** (recommended) lets the runtime pick by GPU vendor: host render targets (RTV) on NVIDIA/AMD, ROV on Intel. **ROV Compatibility** forces pixel-shader EDRAM emulation everywhere; on an RTX 3060 Ti at 1080p it drew ~30% more average power (76 W vs 54 W) and ~60% higher peaks than Default during gameplay, so use it only as a fallback for rendering errors.
- **Input Profiles:** Controller (via SDL3 / XInput) and Keyboard + Mouse.
- **Framerate:** Original (30 FPS) or 60 FPS mode.

### 7. Synchronized In-Game Cutscene & Dialogue Fast-Forward
- **Dynamic Audio Frequency Scaling via SDL3:** When holding `Select` / `Back` (gamepad) or `Tab` (keyboard), the audio stream scales playback frequency cleanly via `SDL_SetAudioStreamFrequencyRatio(stream, multiplier)`.
- **Clock Delta Synchronization:** Delta time in `timing.cpp` scales synchronously by the exact same multiplier (`2x`, `4x`, `6x`, `8x`).
- **Reactive Scenario Event Detection:** In-game dialogue (`CHECK_END_MESSAGE`), camera events, and 3D demo cutscenes dynamically notify the runtime when active. Fast-forward is strictly active during in-game cutscenes and dialogues, preventing any acceleration during normal free-roam gameplay or pre-rendered FMVs.
- **Clean Voice Playback Restoration:** Speech finishes naturally in 1/Nth the time, the sound engine gracefully releases voice channels, and releasing the button restores 100% normal audio without voice muting.
- **Configurable in Launcher:** Toggle on/off and selectable speed multiplier (`2x`, `4x (Default)`, `6x`, `8x`).

---

## Controls

| Action | Xbox Controller | PlayStation Layout Reference | Keyboard / Mouse |
| :--- | :--- | :--- | :--- |
| **Move** | Left Stick | Left Stick | **W / A / S / D** |
| **Camera** | Right Stick | Right Stick | **Mouse / I / J / K / L** |
| **Jump / Confirm** | A | Cross | **Space** |
| **Run / Dash** | Hold B + Move | Hold Circle + Move | **Hold Shift + WASD** |
| **Interact / Back** | B | Circle | **E** |
| **Attack** | X | Square | **Left Mouse Button** |
| **Roll** | Y | Triangle | **R** |
| **Crouch** | LT | L2 | **Hold Q** |
| **Command Majin** | RT | R2 | **Right Mouse Button** |
| **Throw Mode** | LB | L1 | **F** |
| **Ability** | RB | R1 | **C** |
| **D-Pad** | D-Pad | D-Pad | **Arrow Keys** |
| **Pause Menu** | Start / Menu | Options | **Enter** |
| **Map / Menu** | Back / View | Share / Touchpad | **Tab** |
| **Quit Game** | Guide / Esc | PS / Esc | **Esc** |

---

## Building from Source

### Prerequisites
1. **Windows 10/11 x64**
2. **Visual Studio 2022** (Community, Professional, Enterprise or the free **Build Tools**) with:
   - **Desktop Development with C++** (`MSVC v143 x64`, `Windows 10/11 SDK`)
   - **Clang:** either the VS component **C++ Clang Compiler for Windows**, or standalone **LLVM 20.1.8** installed to `C:\Program Files\LLVM`
3. **CMake 3.25+** and **Ninja** (from the VS CMake component or installed separately)
4. **PowerShell 7+** (or Windows PowerShell 5.1), **Git** and **Python 3**

The build scripts locate Visual Studio through `vswhere` and pick Clang/CMake/Ninja automatically (`scripts/vs-env.ps1`), so no paths need editing. A minimal setup with winget:

```powershell
winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"
winget install LLVM.LLVM --version 20.1.8
winget install Kitware.CMake Ninja-build.Ninja Python.Python.3.12
```

> [!TIP]
> Keep the working copy outside OneDrive/Dropbox-synced folders: extracted game data, the SDK and build outputs take 20-30 GB.

### Step-by-Step Build Guide

1. **Clone this repository:**
   ```powershell
   git clone https://github.com/kelvynzucco/majin-recompile.git
   cd majin-recompile
   ```

2. **Prepare and patch the local ReXGlue SDK (v0.10.0):**
   ```powershell
   .\scripts\prepare-sdk-source.ps1
   .\scripts\build-sdk-local.ps1
   ```

3. **Extract your legal game data:**
   - Place your legally dumped Xbox 360 ISO in the workspace and extract it:
     ```powershell
     .\scripts\extract-iso.ps1 -IsoPath "path\to\majin.iso" -OutputDir "game-data-usa"
     ```
   - For the USA edition, unpack the executable:
     ```powershell
     .\scripts\unpack-xex-usa.ps1
     ```

4. **Recompile and build the native executable:**
   ```powershell
   .\scripts\build-usa.ps1 -Configuration Release
   ```

5. **Build the GUI Launcher:**
   ```powershell
   .\scripts\build-launcher-usa.ps1
   ```

6. **Play:**
   - Double click `Majin FK Launcher.exe` or launch via script:
     ```powershell
     .\scripts\run-usa.ps1 -RenderingPath Default -Resolution 1080p -FrameRate 60
     ```

---

## Detailed Documentation

- [`SHIP_LOG.md`](SHIP_LOG.md): Complete chronological record of verified milestones, telemetry benchmarks, root cause analyses, and the PAL $\rightarrow$ USA address translation table.
- [`RECOMPILATION_PLAN_USA.md`](RECOMPILATION_PLAN_USA.md): Technical architectural blueprint and phase-by-phase implementation plan.
- [`patches/rexglue-0.10.0/README.md`](patches/rexglue-0.10.0/README.md): Detailed documentation of all SDK audio, GPU, logging, and UI loop patches.

---

## Credits & Acknowledgments

- **Game Republic & Bandai Namco:** Original creators and publishers of *Majin and the Forsaken Kingdom*.
- **[GM-0856](https://github.com/GM-0856/majin-recompile):** Original recompilation research and PAL Alpha implementation.
- **[ReXGlue Project](https://github.com/rexglue/rexglue-sdk) & [Xenia Team](https://github.com/xenia-project/xenia):** Recompilation toolkit and Direct3D 12 Xbox 360 emulation backend.
- **Kelvyn Zucco:** NTSC-U / American static recompilation, Direct3D 12 Default/RTV black screen resolution, startup PSO shader pre-compilation, UI presenter infinite loop fix, and GPU bandwidth optimizations.
