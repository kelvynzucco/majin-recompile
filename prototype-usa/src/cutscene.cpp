// SPDX-License-Identifier: GPL-3.0-only
// Majin and the Forsaken Kingdom (USA 0.0.0.2)
// In-game 3D Cutscene & Dialogue Skip / Fast-Forward Hook
#include <windows.h>
#include <atomic>
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

std::atomic<int> s_cutscene_active_frames{0};

} // namespace

void NotifyCutsceneOrDialogueActive() {
  s_cutscene_active_frames.store(15, std::memory_order_relaxed);
}

bool IsCutsceneOrDialogueActive() {
  return s_cutscene_active_frames.load(std::memory_order_relaxed) > 0;
}

void DecrementCutsceneActiveFrames() {
  int cur = s_cutscene_active_frames.load(std::memory_order_relaxed);
  if (cur > 0) {
    s_cutscene_active_frames.store(cur - 1, std::memory_order_relaxed);
  }
}

float GetCutsceneSpeedMultiplier() {
  if (!Enabled()) return 1.0f;
  const int32_t spd = rex::cvar::Query<int32_t>("majin_cutscene_speed");
  if (spd < 2) return 2.0f;
  if (spd > 16) return 16.0f;
  return static_cast<float>(spd);
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
// DIALOGUE / MESSAGE SYSTEM NOTIFICATIONS
// =======================================================================

// CHECK_END_MESSAGE (USA: 0x82183040)
REX_EXTERN(__imp__sub_82183040);
REX_EXTERN(sub_82183040) {
  NotifyCutsceneOrDialogueActive();
  __imp__sub_82183040(ctx, base);
}

// playMessageGetEnd (USA: 0x822355C8)
REX_EXTERN(__imp__sub_822355C8);
REX_EXTERN(sub_822355C8) {
  NotifyCutsceneOrDialogueActive();
  __imp__sub_822355C8(ctx, base);
}

// CAMERA_EVENT_CHECK (USA: 0x82181050)
REX_EXTERN(__imp__sub_82181050);
REX_EXTERN(sub_82181050) {
  NotifyCutsceneOrDialogueActive();
  __imp__sub_82181050(ctx, base);
}

// =======================================================================
// 3D CUTSCENE (DEMO) SYSTEM HOOKS
// =======================================================================

// DEMO_END_CHECK (USA: 0x821836C0)
REX_EXTERN(__imp__sub_821836C0);
REX_EXTERN(sub_821836C0) {
  NotifyCutsceneOrDialogueActive();
  if (ShouldSkip()) {
    ctx.r3.u64 = 1; // Signal demo end to scenario script
    return;
  }
  __imp__sub_821836C0(ctx, base);
}

// DEMO_CAMERA_END_CHECK (USA: 0x82183778)
REX_EXTERN(__imp__sub_82183778);
REX_EXTERN(sub_82183778) {
  NotifyCutsceneOrDialogueActive();
  if (ShouldSkip()) {
    ctx.r3.u64 = 1; // Signal camera demo end
    return;
  }
  __imp__sub_82183778(ctx, base);
}

// setDemoGetEnd (USA: 0x822351D0)
REX_EXTERN(__imp__sub_822351D0);
REX_EXTERN(sub_822351D0) {
  NotifyCutsceneOrDialogueActive();
  if (ShouldSkip()) {
    ctx.r3.u64 = 1; // Signal demo get end to script task
    return;
  }
  __imp__sub_822351D0(ctx, base);
}
