// SPDX-License-Identifier: GPL-3.0-only
// Majin and the Forsaken Kingdom (USA 0.0.0.2)
// In-game 3D Cutscene & Dialogue Skip / Fast-Forward Hook
#include <windows.h>
#include "generated/default/majin_pch.h"
#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(majin_skip_cutscenes, true, "Gameplay",
    "Allow skipping in-game cutscenes and dialogue by holding Select/Back on controller or Tab on keyboard");

REXCVAR_DEFINE_INT32(majin_cutscene_speed, 4, "Gameplay",
    "In-game cutscene and dialogue fast-forward speed multiplier (2, 4, 6, 8)");

namespace {

bool Enabled() {
  static const bool enabled = rex::cvar::Query<bool>("majin_skip_cutscenes");
  return enabled;
}

} // namespace

float GetCutsceneSpeedMultiplier() {
  if (!Enabled()) return 1.0f;
  const int32_t spd = rex::cvar::Query<int32_t>("majin_cutscene_speed");
  if (spd < 2) return 2.0f;
  if (spd > 16) return 16.0f;
  return static_cast<float>(spd);
}

bool IsInGameCutsceneOrDialogue(uint8_t* base, uint32_t object) {
  if (!base || !object || object < 0x10000 || object > 0x8FFFFFFF) return false;

  // 1. Message / Dialogue active check (engine + 0x15140)
  const uint32_t msg_mgr = REX_LOAD_U32(object + 0x15140);
  if (msg_mgr >= 0x10000 && msg_mgr < 0x8FFFFFFF) {
    const uint32_t msg_active = REX_LOAD_U32(msg_mgr + 168); // 0xA8
    if (msg_active == 1) return true;
  }

  // 2. 3D Cutscene / Demo active check (engine + 0x15144)
  const uint32_t demo_mgr = REX_LOAD_U32(object + 0x15144);
  if (demo_mgr >= 0x10000 && demo_mgr < 0x8FFFFFFF) {
    const uint32_t demo_obj = REX_LOAD_U32(demo_mgr);
    if (demo_obj >= 0x10000 && demo_obj < 0x8FFFFFFF) {
      const uint32_t demo_active = REX_LOAD_U32(demo_obj + 120);
      if (demo_active != 0) return true;
    }
  }

  // 3. Cinematic Camera event check (engine + 0x15128)
  const uint32_t cam_mgr = REX_LOAD_U32(object + 0x15128);
  if (cam_mgr >= 0x10000 && cam_mgr < 0x8FFFFFFF) {
    const uint32_t cam_event = REX_LOAD_U32(cam_mgr + 0x51C0);
    if (cam_event >= 0x10000 && cam_event < 0x8FFFFFFF) {
      const uint32_t cam_active = REX_LOAD_U32(cam_event + 5936);
      if (cam_active != 0) return true;
    }
  }

  return false;
}

bool IsBackOrTabHeld() {
  if (!Enabled()) return false;

  // Keyboard check: Tab key or Backspace
  if ((GetAsyncKeyState(VK_TAB) & 0x8000) != 0 || (GetAsyncKeyState(VK_BACK) & 0x8000) != 0) {
    return true;
  }

  // Gamepad check: XInput controllers (Back / Select button = 0x0020)
  static HMODULE xinput = []() -> HMODULE {
    HMODULE h = LoadLibraryA("xinput1_4.dll");
    if (!h) h = LoadLibraryA("xinput1_3.dll");
    if (!h) h = LoadLibraryA("xinput9_1_0.dll");
    return h;
  }();

  if (xinput) {
    typedef DWORD (WINAPI *PFN_XInputGetState)(DWORD dwUserIndex, void* pState);
    static PFN_XInputGetState pfnGetState = (PFN_XInputGetState)GetProcAddress(xinput, "XInputGetState");
    if (pfnGetState) {
      struct {
        DWORD dwPacketNumber;
        struct {
          WORD wButtons;
          BYTE bLeftTrigger;
          BYTE bRightTrigger;
          SHORT sThumbLX;
          SHORT sThumbLY;
          SHORT sThumbRX;
          SHORT sThumbRY;
        } Gamepad;
      } state{};
      for (DWORD i = 0; i < 4; ++i) {
        if (pfnGetState(i, &state) == 0) {
          if (state.Gamepad.wButtons & 0x0020) { // XINPUT_GAMEPAD_BACK (Select)
            return true;
          }
        }
      }
    }
  }

  return false;
}

namespace {

bool ShouldSkip() {
  static int hold_frames = 0;
  static bool announced = false;
  if (IsBackOrTabHeld()) {
    hold_frames++;
    if (hold_frames >= 5) { // Responsive skip after holding ~5 frames (~0.1s)
      if (!announced) {
        REXLOG_INFO("[Majin Skip] In-game scene/dialogue skip active (Select/Tab held)");
        announced = true;
      }
      return true;
    }
  } else {
    hold_frames = 0;
    announced = false;
  }
  return false;
}

} // namespace

// =======================================================================
// 3D CUTSCENE (DEMO) SYSTEM HOOKS
// =======================================================================

// DEMO_END_CHECK (USA: 0x821836C0)
REX_EXTERN(__imp__sub_821836C0);
REX_EXTERN(sub_821836C0) {
  if (ShouldSkip()) {
    ctx.r3.u64 = 1; // Signal demo end to scenario script
    return;
  }
  __imp__sub_821836C0(ctx, base);
}

// DEMO_CAMERA_END_CHECK (USA: 0x82183778)
REX_EXTERN(__imp__sub_82183778);
REX_EXTERN(sub_82183778) {
  if (ShouldSkip()) {
    ctx.r3.u64 = 1; // Signal camera demo end
    return;
  }
  __imp__sub_82183778(ctx, base);
}

// setDemoGetEnd (USA: 0x822351D0)
REX_EXTERN(__imp__sub_822351D0);
REX_EXTERN(sub_822351D0) {
  if (ShouldSkip()) {
    ctx.r3.u64 = 1; // Signal demo get end to script task
    return;
  }
  __imp__sub_822351D0(ctx, base);
}
