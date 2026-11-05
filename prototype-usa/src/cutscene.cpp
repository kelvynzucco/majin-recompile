// SPDX-License-Identifier: GPL-3.0-only
// Majin and the Forsaken Kingdom (USA 0.0.0.2)
// In-game 3D Cutscene Skip Hook
#include <windows.h>
#include "generated/default/majin_pch.h"
#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(majin_skip_cutscenes, true, "Gameplay",
    "Allow skipping in-game 3D cutscenes by holding Select/Back on controller or Tab on keyboard");

namespace {

bool Enabled() {
  static const bool enabled = rex::cvar::Query<bool>("majin_skip_cutscenes");
  return enabled;
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
          if (state.Gamepad.wButtons & 0x0020) { // XINPUT_GAMEPAD_BACK
            return true;
          }
        }
      }
    }
  }

  return false;
}

// Require holding for ~15 consecutive calls to avoid accidental triggers
bool ShouldSkipCutscene() {
  static int hold_frames = 0;
  static bool announced = false;
  if (IsBackOrTabHeld()) {
    hold_frames++;
    if (hold_frames >= 15) {
      if (!announced) {
        REXLOG_INFO("[Majin Cutscene Skip] In-game 3D cutscene skipped by user request (Select/Tab held)");
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

// 1. DEMO_END_CHECK (USA: 0x821836C0)
REX_EXTERN(__imp__sub_821836C0);
REX_EXTERN(sub_821836C0) {
  if (ShouldSkipCutscene()) {
    ctx.r3.u64 = 1; // Signal demo end to scenario script
    return;
  }
  __imp__sub_821836C0(ctx, base);
}

// 2. DEMO_CAMERA_END_CHECK (USA: 0x82183778)
REX_EXTERN(__imp__sub_82183778);
REX_EXTERN(sub_82183778) {
  if (ShouldSkipCutscene()) {
    ctx.r3.u64 = 1; // Signal camera demo end
    return;
  }
  __imp__sub_82183778(ctx, base);
}

// 3. setDemoGetEnd (USA: 0x822351D0)
REX_EXTERN(__imp__sub_822351D0);
REX_EXTERN(sub_822351D0) {
  if (ShouldSkipCutscene()) {
    ctx.r3.u64 = 1; // Signal demo get end to script task
    return;
  }
  __imp__sub_822351D0(ctx, base);
}
