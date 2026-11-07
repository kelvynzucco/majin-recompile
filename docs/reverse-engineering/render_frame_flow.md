# Majin and the Forsaken Kingdom - Render Frame Flow Analysis

## Overview
This document outlines the per-frame rendering flow in the recompiled Xbox 360 binary (`majin_recomp.*.cpp`), detailing the verified identities of the Xbox 360 D3D9 (`D3DDevice`) functions, the engine's 3D and 2D/UI passes, and how they interact with the ReXGlue Vulkan backend.

## 1. Frame Boundaries & Verified D3DDevice Symbols
In the recompiled code, the main render loop is driven by the `CGameProject` runtime and dispatches commands into Xbox 360 D3D9 (`D3DDevice*` in `r3`):

| Recompiled Symbol | Real Xbox 360 D3D9 / Engine Identity | Role & Struct Layout |
| :--- | :--- | :--- |
| **`sub_82475748`** | **`D3DDevice_Present`** / End-of-Frame Swap | Finalizes the command buffer, calls `sub_824684F0` (`D3DDevice_Resolve`) to resolve EDRAM to the frontbuffer texture, and triggers presentation (`IssueSwap`). |
| **`sub_824684F0`** | **`D3DDevice_Resolve`** | Resolves EDRAM render targets to textures in shared memory (`RB_MODECONTROL = kCopy`) and optionally clears EDRAM color/depth (`f1` = `ClearZ`). |
| **`sub_8246C4F0`** | **`D3DDevice_SetViewport`** | Reads `const D3DVIEWPORT9*` from `r4` (`X` at `+0`, `Y` at `+4`, `Width` at `+8`, `Height` at `+12`, `MinZ` at `+16`, `MaxZ` at `+20`) and calls `sub_8246C208`. |
| **`sub_8246C208`** | **`D3DDevice_SetViewport_Internal`** | Writes the active `D3DVIEWPORT9` into `D3DDevice` at `r3 + 12824` (`X`), `r3 + 12828` (`Y`), `r3 + 12832` (`Width`), `r3 + 12836` (`Height`), `r3 + 12840` (`MinZ`), `r3 + 12844` (`MaxZ`), updates `PA_CL_VPORT_*`, and invokes `sub_8246BAC0(r3, r3 + 12852)`. |
| **`sub_8246BAC0`** | **`D3DDevice_SetScissorRect`** | Reads `const RECT*` from `r4` (`left, top, right, bottom`), clamps it against `[Viewport.X, Viewport.Y, Viewport.X + Width, Viewport.Y + Height]` stored at `r3 + 12824..12836`, and emits PM4 packets for `PA_SC_WINDOW_SCISSOR_TL/BR` (`0x2004`). |
| **`sub_82985580`** | **`CGameProject::RenderUI`** | Entry point for the 2D HUD, menus, and text overlay pass. |

## 2. 3D Scene Rendering Pass
Before the UI pass, the engine renders the 3D world:
1. **Shadow & Depth Pre-passes**: Renders shadow maps and depth buffers using sub-720p or custom viewports via `sub_8246C4F0` (`D3DDevice_SetViewport`).
2. **Main Geometry & Lighting**: Renders into EDRAM at base 1280x720 (`PA_CL_VTE_CNTL.vport_x/y_scale_ena = 1`) and resolves intermediate buffers via `sub_824684F0` (`D3DDevice_Resolve`).
3. **Post-Processing**: Bloom, DOF, and tone-mapping passes sample resolved EDRAM textures (including 640x360 half-res targets) and write back to the main EDRAM color target.

## 3. 2D UI & HUD Rendering Pass
After the 3D scene and post-processing are complete:
1. **UI Entry (`sub_82985580`)**:
   - Invoked near the end of the frame (`majin_recomp.226.cpp`).
   - Iterates over active UI layers (`CMenuMap`, `COSD`, `CWindowBase`, `CRichText`) and dispatches 2D primitive/sprite draws.
2. **Viewport & Scissor Behavior**:
   - 2D UI shaders frequently render in pre-transformed screen space (`PA_CL_VTE_CNTL.vport_x/y_scale_ena = 0`) or standard 1280x720 orthographic space.
   - In ReXGlue's Vulkan backend, `draw_util::GetHostViewportInfo` (`draw.cpp:176-333`) automatically multiplies both `scale[i]` and `viewport_axis_extent` by `draw_resolution_scale_x/y`, so `ndc_scale` and `PA_SC_WINDOW_SCISSOR` remain mathematically consistent at both 1x (720p) and >1x (e.g. 1440p) without mutating guest `D3DDevice` memory.

## 4. End-of-Frame Resolve & Presentation
1. **`sub_82475748` (`D3DDevice_Present`)**:
   - Calls `sub_824684F0` (`D3DDevice_Resolve`) to copy the final EDRAM color buffer to the frontbuffer texture in shared memory.
   - Emits the swap command processed by `VulkanCommandProcessor::IssueSwap`, which applies gamma + FSR/CAS upscaling and presents to the host swapchain.
