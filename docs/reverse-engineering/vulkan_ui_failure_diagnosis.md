# Vulkan Black Screen & UI Failure Diagnosis (Post-Mortem & Resolution)

## Executive Summary
When launching *Majin and the Forsaken Kingdom* with the Vulkan backend after the initial implementation of `VULKAN_NATIVE_RESOLUTION_MOD_PLAN.md`, the game produced a **100% black screen** (even at `RenderScale = 1`).

Static reverse engineering of the recompiled PowerPC code (`majin_recomp.*.cpp`) and the ReXGlue SDK Vulkan backend (`tools/source/rexglue-sdk/src/graphics/vulkan/`) identified **three distinct root causes** that combined to produce the black screen.

---

## 1. Primary Root Cause: Scissor Collapse & Viewport Corruption in `scene_resolution.cpp`
In the initial Phase 3/4 implementation of `prototype-usa/src/scene_resolution.cpp`, three Xbox 360 D3D9 (`D3DDevice`) functions were misidentified and hooked with destructive memory/register writes that ran on **every frame** (because `IsNativeUiActive()` returned `REXCVAR_GET(majin_native_ui)`, which defaulted to `true` even at `RenderScale = 1`):

### A. `sub_8246BAC0` (`D3DDevice_SetScissorRect`) — Collapsed Scissor to `0x0`
- **Actual PPC behavior (`majin_recomp.141.cpp:10459-10506` & `majin_recomp.85.cpp:11223-11347`)**:
  - `D3DDevice` stores the active `D3DVIEWPORT9` floats at:
    - `r3 + 12824` = `Viewport.X`
    - `r3 + 12828` = `Viewport.Y`
    - `r3 + 12832` = `Viewport.Width`
    - `r3 + 12836` = `Viewport.Height`
  - `sub_8246BAC0` clamps the incoming Scissor `RECT` (`[left, top, right, bottom]`) against `[Viewport.X, Viewport.Y, Viewport.X + Viewport.Width, Viewport.Y + Viewport.Height]` before writing `PA_SC_WINDOW_SCISSOR_TL` and `PA_SC_WINDOW_SCISSOR_BR` (`0x2004`).
- **What the buggy hook did**:
  - Assuming `r3 + 12824` and `r3 + 12828` were width and height, the hook wrote `1280.0f` (`0x44A00000`) to `r3 + 12824` (`Viewport.X`) and `720.0f` (`0x44340000`) to `r3 + 12828` (`Viewport.Y`).
  - This shifted the viewport bounds used for scissor clamping to `[1280, 720, 2560, 1440]`.
  - When the game passed a standard 720p scissor rect `[0, 0, 1280, 720]`, `sub_8246BAC0` clamped `left = max(0, 1280) = 1280`, `top = max(0, 720) = 720`, `right = min(1280, 2560) = 1280`, `bottom = min(720, 1440) = 720`.
  - Result: **`PA_SC_WINDOW_SCISSOR` became `(1280, 720)..(1280, 720)` (`width = 0, height = 0`) on every draw call**, discarding 100% of rasterized pixels across the entire game!

### B. `sub_8246C4F0` (`D3DDevice_SetViewport`) — Corrupted Non-720p Passes
- **Actual PPC behavior (`majin_recomp.116.cpp:11007-11034`)**:
  - `D3DDevice_SetViewport(D3DDevice* r3, const D3DVIEWPORT9* r4)`.
- **What the buggy hook did**:
  - Overwrote `r4 + 8` (`Width`) and `r4 + 12` (`Height`) in guest memory to `1280.0f x 720.0f` on **all** `SetViewport` calls globally, corrupting shadow maps, 640x360 post-processing passes, and EDRAM clear quads.

### C. `sub_824684F0` (`D3DDevice_Resolve`) — Corrupted Depth Clear Value
- **Actual PPC behavior (`majin_recomp.114.cpp:2591`)**:
  - `D3DDevice_Resolve`, where `f1` is the `ClearZ` depth clear value when `D3DRESOLVE_CLEARDEPTHSTENCIL` is set.
- **What the buggy hook did**:
  - Overwrote `ctx.f1.f64 = 1.0` on every resolve call.

---

## 2. Secondary Root Cause: `vulkan_dynamic_rendering=true` Breaks Both `fbo` and `fsi` in `rexgpu-xenos.dll`
- **Why `fsi` failed with `vulkan_dynamic_rendering=true`**:
  - When `use_dynamic_rendering` is `true`, `SubmitBarriersAndEnterRenderTargetCacheRenderPass` bypasses `fsi_render_pass_` (which provides the mandatory `VK_DEPENDENCY_BY_REGION_BIT` subpass dependencies for `VK_EXT_fragment_shader_interlock`) and begins a dynamic rendering pass with 0 color/depth attachments (`render_pass_key.depth_and_color_used == 0`), breaking fragment shader interlock EDRAM rendering.
- **Why `fbo` (`Path::kHostRenderTargets`) missed the entire Menu/UI and most 3D objects when `vulkan_dynamic_rendering=true`**:
  - Every `VulkanRenderTarget` creates two color `VkImageView` handles:
    1. `view_color_attachment_` (`VK_FORMAT_R8G8B8A8_UNORM`, `VK_FORMAT_R16G16B16A16_SFLOAT`, etc.) for normal guest draws.
    2. `view_color_transfer_` (`VK_FORMAT_R8G8B8A8_UINT`, `VK_FORMAT_R16G16B16A16_UINT`, etc.) for EDRAM ownership transfers and resolve clears (`PerformTransfersAndResolveClears`).
  - During `PerformTransfersAndResolveClears` (`render_target_cache.cpp:4771, 5280`), the 4-argument overload `SubmitBarriersAndEnterRenderTargetCacheRenderPass(render_pass, transfer_framebuffer, transfer_dest_view, !i)` starts a 1-attachment rendering pass using `view_color_transfer_` (`UINT`) and sets `current_framebuffer_ = transfer_framebuffer` and `current_render_pass_ = VK_NULL_HANDLE`.
  - However, `transfer_framebuffer` is looked up via `GetFramebuffer(transfer_framebuffer_key)` with `depth_and_color_used = 0b00010` (Color RT0 only, no depth) or `0b00001` (Depth only) — which returns the **exact same `Framebuffer*` pointer** used by subsequent 1-attachment guest draws (all 2D menu/UI draws on RT0 without depth, post-process passes on RT0 without depth, and depth-only prepasses)!
  - In traditional `VkRenderPass` mode (`vulkan_dynamic_rendering = false`), `current_render_pass_` is `GetTransferRenderPass(...)`, which differs from `GetHostRenderTargetsRenderPass(...)`, so the next guest draw ends the `UINT` transfer pass and begins the `UNORM`/`SFLOAT` host render pass.
  - With `vulkan_dynamic_rendering = true`, `SubmitBarriersAndEnterRenderTargetCacheRenderPass` checked `if (in_render_pass_ && current_framebuffer_ == framebuffer && current_render_pass_ == VK_NULL_HANDLE) return;` (`command_processor.cpp:3099-3103`). Because `current_framebuffer_ == framebuffer` was `true` for every 1-attachment draw, it **never exited the `VK_FORMAT_*_UINT` transfer pass**! All 2D menu/UI draws and single-RT passes attempted to render with `UNORM`/`SFLOAT` guest pipelines into the still-open `UINT` transfer image view, while only 3D draws binding both Depth + Color RT0 (`depth_and_color_used = 0b00011`, a 2-attachment `Framebuffer*`) switched out of the transfer pass.
  - Additionally, in `IssueDraw` (`command_processor.cpp:3873`), `CmdVkBindPipeline` was called *before* `SubmitBarriersAndEnterRenderTargetCacheRenderPass` (`command_processor.cpp:4115`), binding the new guest pipeline while the previous render pass was still active.

---

## 3. Tertiary Root Cause: CVar Registration Timing Before `LoadGpuPlugin`
- In `tools/source/rexglue-sdk/src/core/cvar.cpp:298-303`, `rex::cvar::SetFlagByName` (`Source::kRuntime`) checks `GetRegistryIndex().find(name)` and immediately returns `ApplyResult::kRejected` if the CVar has not been registered yet.
- CVars defined inside `rexgpu-xenos.dll` (`vulkan_async_skip_incomplete_frames`, `render_target_path_vulkan`, `vulkan_dynamic_rendering`, `vulkan_allow_present_mode_*`) are only registered when `rex::system::LoadGpuPlugin("xenos", ...)` loads `rexgpu-xenos.dll` inside `MajinApp::OnPreSetup`.
- Previously, `MajinApp::OnPostInitLogging` (which runs *before* `OnPreSetup`) and `MajinApp::OnPreSetup` (on the lines *before* `LoadGpuPlugin`) called `SetFlagByName` for those flags, causing them to be silently rejected (`kRejected`). This is why `runtime-usa-20261106-205044.log` still logged:
  `Skipping Vulkan frame presentation due to async placeholder draw usage in this frame`
  even though `OnPostInitLogging` had attempted to set `vulkan_async_skip_incomplete_frames` to `"false"`.

---

## 4. Applied Fixes
1. **`prototype-usa/src/scene_resolution.cpp` & `scene_resolution.h`**:
   - Removed the destructive hooks on `sub_8246BAC0` (`D3DDevice_SetScissorRect`), `sub_8246C4F0` (`D3DDevice_SetViewport`), and `sub_824684F0` (`D3DDevice_Resolve`).
   - Kept a clean, non-mutating telemetry hook on `sub_82475748` (`D3DDevice_Present`).
   - Fixed `IsNativeUiActive()` to only report active when `GetEffectiveSceneScale() > 1`.
2. **`prototype-usa/src/majin_app.h` & `scripts/run-usa.ps1`**:
   - Moved GPU plugin CVar initialization (`ApplyGpuPluginDefaults()`) to execute **immediately after** `rex::system::LoadGpuPlugin` in `OnPreSetup`, ensuring all `rexgpu-xenos.dll` CVars are registered when `SetFlagByName` is called.
   - Configured `render_target_path_vulkan = "fbo"` (`Path::kHostRenderTargets`, matching D3D12's default `kHostRenderTargets` path) and explicitly set `vulkan_dynamic_rendering = "false"` in both `majin_app.h` and `scripts/run-usa.ps1`.
3. **`tools/source/rexglue-sdk/src/graphics/vulkan/` (`rexgpu-xenos.dll`)**:
   - Changed `vulkan_dynamic_rendering` default to `false` (`command_processor.cpp:67`).
   - Fixed `PerformTransfersAndResolveClears` (`render_target_cache.cpp:5408`) to call `command_processor_.EndRenderPass()` upon completion so `UINT` transfer render passes never leak into subsequent guest draws.
   - Fixed `SubmitBarriersAndEnterRenderTargetCacheRenderPass` (`command_processor.cpp:3162, 3190`) to never alias `current_framebuffer_` on transfer views and to re-bind `current_guest_graphics_pipeline_` when entering a new render pass.
   - Fixed `GetLastUpdateRenderingAttachments` (`render_target_cache.cpp:1544`) and `GetTransferPipelines` (`render_target_cache.cpp:4463`) for dynamic rendering compatibility.
   - Rebuilt `rexgpu-xenos.dll` and `majin.exe`, deployed `rexgpu-xenos.dll` to `tools/sdk/win-amd64/bin/` and `prototype-usa/out/build/win-amd64-release/`, and cleared stale Vulkan pipeline caches in `private/runtime-usa/cache/vulkan-vulkan/`.

---

## 5. Current Testing Status & Remaining Work (06/11/2026)
> [!WARNING]
> **Issue Persists**: Testing via launcher with Vulkan backend confirms the menu remains invisible (black screen) and in-game rendering is only partial. The native Vulkan modification is **not functional yet** and requires further investigation.

### Next Session Investigation Steps
1. **SPIR-V vs DXBC Shader Translation**: Inspect how vertex attributes and system constants are mapped in `SpirvShaderTranslator` for the Game Republic engine compared to `DxbcShaderTranslator`.
2. **Texture Cache & Formats**: Compare `texture_cache.cpp` logs between D3D12 and Vulkan to verify menu sprite sheets and font textures are decoded and bound properly.
3. **Fragment Shader Interlock (`fsi`) Testing**: Evaluate whether `fsi` (with proper subpass dependencies) behaves differently from `fbo` for Xbox 360 EDRAM emulation.
4. **RenderDoc Capture**: Perform a GPU capture of the Vulkan frame to inspect actual pipeline state, scissor/viewport bounds, and color attachment bindings on draw.
