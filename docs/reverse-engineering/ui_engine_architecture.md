# Majin and the Forsaken Kingdom - UI Engine Architecture

## Overview
Through static analysis of the recompiled codebase and Xbox 360 D3D9 device functions, we mapped the internal UI and 2D rendering subsystem of *Majin and the Forsaken Kingdom*. The game uses a custom, in-house UI framework integrated into its `CGameProject` engine (`D:\ Jenkins\workspace\QuickBuild_Majin_US\program\runtime\GameProject\`).

## 1. Core UI Classes (RTTI & Debug Strings)
- **Base Windowing & Layout**:
  - `CGameProject::CWindowBase`
  - `CGameProject::CSimpleWindow`
  - `CGameProject::CRichText` (text formatting, font glyph rendering, dialogue boxes)
- **HUD & On-Screen Display (OSD)**:
  - `CGameProject::COSD` (Main HUD controller)
  - `CGameProject::CMenuActionCommand` (Context-sensitive action prompts)
  - `CGameProject::CMenuMap`, `CMenuWorldMap`, `CMenuMiniMap`
- **Menus & Screens**:
  - `CGameProject::CMenuMain`, `CMenuTitle`, `CMenuPause`
  - `CGameProject::CMenuSaveLoad`, `CMenuOption`, `CMenuGameOver`

## 2. Primary UI Rendering Functions
- **`sub_82985580` (Main UI Render Entry)**:
  - Located in `majin_recomp.226.cpp`.
  - Iterates over active UI layers (`r3 + 52`, `r3 + 276`) and dispatches draw calls for visible HUD and menu elements.
- **`D3DDevice` Viewport & Scissor Functions**:
  - **`sub_8246C4F0` (`D3DDevice_SetViewport`)** and **`sub_8246C208` (`D3DDevice_SetViewport_Internal`)**:
    - Store the active `D3DVIEWPORT9` in `D3DDevice` at:
      - `r3 + 12824` = `Viewport.X` (`float`)
      - `r3 + 12828` = `Viewport.Y` (`float`)
      - `r3 + 12832` = `Viewport.Width` (`float`)
      - `r3 + 12836` = `Viewport.Height` (`float`)
      - `r3 + 12840` = `Viewport.MinZ` (`float`)
      - `r3 + 12844` = `Viewport.MaxZ` (`float`)
  - **`sub_8246BAC0` (`D3DDevice_SetScissorRect`)**:
    - Reads the guest scissor `RECT` (`left, top, right, bottom`) and clamps it against `[Viewport.X, Viewport.Y, Viewport.X + Width, Viewport.Y + Height]` before writing `PA_SC_WINDOW_SCISSOR_TL/BR`.

## 3. How ReXGlue Scales 3D and 2D UI Natively
- In `tools/source/rexglue-sdk/src/graphics/util/draw.cpp` (`draw_util::GetHostViewportInfo`), ReXGlue inspects `PA_CL_VTE_CNTL`:
  - When `vport_x/y_scale_ena == 1` (3D geometry), `scale[i]` uses `PA_CL_VPORT_XSCALE/YSCALE * draw_resolution_scale`.
  - When `vport_x/y_scale_ena == 0` (pre-transformed 2D UI sprites/quads), `scale[i]` uses `1.0f * draw_resolution_scale`.
  - Because `viewport_axis_extent` is also scaled by `draw_resolution_scale`, `ndc_scale[i] = scale[i] * (2.0f / viewport_axis_extent)` preserves the exact `[0..1280, 0..720]` -> `[-1..1]` NDC mapping while rasterizing into the scaled host framebuffer (`2560x1440` at 2x), and `PA_SC_WINDOW_SCISSOR_TL/BR` is scaled proportionally.
- Consequently, mutating guest `D3DDevice` viewport/scissor memory is unnecessary and harmful (writing `1280.0f` / `720.0f` to `r3 + 12824` / `r3 + 12828` shifts `Viewport.X/Y` to `(1280, 720)` and collapses every scissor rect to `0x0`).
