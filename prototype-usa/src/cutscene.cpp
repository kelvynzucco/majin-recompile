// SPDX-License-Identifier: GPL-3.0-only
// Majin and the Forsaken Kingdom (USA 0.0.0.2)
// In-game 3D Cutscene & Dialogue Skip / Fast-Forward Hook
#include <windows.h>
#include <atomic>
#include "generated/default/majin_pch.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/audio/audio_driver.h>

REXCVAR_DEFINE_BOOL(majin_skip_cutscenes, true, "Gameplay",
    "Allow fast-forwarding in-game cutscenes and dialogue by holding Select/Back on controller or Tab on keyboard");

REXCVAR_DEFINE_INT32(majin_cutscene_speed, 4, "Gameplay",
    "In-game cutscene and dialogue fast-forward speed multiplier (2, 4, 6, 8)");

namespace {

bool Enabled() {
  static const bool enabled = rex::cvar::Query<bool>("majin_skip_cutscenes");
  return enabled;
}

std::atomic<int> s_cutscene_active_frames{0};
std::atomic<int> s_turbo_cooldown_frames{0};
std::atomic<int> s_continuous_turbo_frames{0};
std::atomic<bool> s_must_release_button{false};

constexpr int kMaxContinuousTurboFrames = 150; // Auto-stop limit (~5s real time)

bool Span(uint32_t addr, uint32_t size) {
  return addr >= 0x10000 && addr <= UINT32_MAX - size;
}

bool IsDemoNearingEnd(uint8_t* base) {
  // 0x82C273D0 is the engine scenario manager root
  const uint32_t r31 = REX_LOAD_U32(0x82C273D0);
  if (!Span(r31, 0x15148)) return false;
  const uint32_t r30 = r31 + 65536 + 20804; // r31 + 0x15144
  const uint32_t r8 = REX_LOAD_U32(r30);
  if (!Span(r8, 128)) return false;
  const uint32_t r11 = REX_LOAD_U32(r8 + 120);
  if (!Span(r11, 32)) return false;
  const uint32_t r4 = REX_LOAD_U32(r11 + 20);
  if (!Span(r4, 104)) return false;
  const uint32_t state96 = REX_LOAD_U32(r4 + 96);
  const uint32_t state92 = REX_LOAD_U32(r4 + 92);
  // States 1 or 2 indicate main cutscene sequence has completed and outro/camera settle has begun
  return (state96 == 1 || state96 == 2 || state92 == 1 || state92 == 2);
}

} // namespace

void OnCutsceneOrDialogueEnded() {
  s_cutscene_active_frames.store(0, std::memory_order_relaxed);
  s_continuous_turbo_frames.store(0, std::memory_order_relaxed);
  s_turbo_cooldown_frames.store(90, std::memory_order_relaxed); // ~3 seconds cooldown
  s_must_release_button.store(true, std::memory_order_relaxed);
  rex::audio::SetPlaybackFrequencyRatio(1.0f);
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

bool IsTurboAllowed() {
  if (!Enabled()) return false;

  // Check button release latch
  if (s_must_release_button.load(std::memory_order_relaxed)) {
    if (!IsBackOrTabHeld()) {
      s_must_release_button.store(false, std::memory_order_relaxed);
    } else {
      return false; // Still holding button from cutscene -> ignore during gameplay
    }
  }

  // Check cooldown
  if (s_turbo_cooldown_frames.load(std::memory_order_relaxed) > 0) {
    return false;
  }

  // Must have cutscene or dialogue actively requesting frames
  if (s_cutscene_active_frames.load(std::memory_order_relaxed) <= 0) {
    return false;
  }

  // Auto-stop trigger: disengage after continuous turbo burst
  if (s_continuous_turbo_frames.load(std::memory_order_relaxed) >= kMaxContinuousTurboFrames) {
    s_must_release_button.store(true, std::memory_order_relaxed);
    return false;
  }

  return true;
}

void OnTurboFrameTick() {
  s_continuous_turbo_frames.fetch_add(1, std::memory_order_relaxed);
}

void OnTurboDisengaged() {
  s_continuous_turbo_frames.store(0, std::memory_order_relaxed);
}

void TickCutsceneTiming() {
  int cd = s_turbo_cooldown_frames.load(std::memory_order_relaxed);
  if (cd > 0) {
    s_turbo_cooldown_frames.store(cd - 1, std::memory_order_relaxed);
  }
  int act = s_cutscene_active_frames.load(std::memory_order_relaxed);
  if (act > 0) {
    s_cutscene_active_frames.store(act - 1, std::memory_order_relaxed);
  }
}

float GetCutsceneSpeedMultiplier() {
  if (!Enabled()) return 1.0f;
  const int32_t spd = rex::cvar::Query<int32_t>("majin_cutscene_speed");
  if (spd < 2) return 2.0f;
  if (spd > 16) return 16.0f;
  return static_cast<float>(spd);
}

// =======================================================================
// DIALOGUE / MESSAGE SYSTEM NOTIFICATIONS
// =======================================================================

// CHECK_END_MESSAGE (USA: 0x82183040)
// Returns 0 while message is displayed/active, 1 when message has ended
REX_EXTERN(__imp__sub_82183040);
REX_EXTERN(sub_82183040) {
  __imp__sub_82183040(ctx, base);
  if (ctx.r3.u32 == 1) {
    // Dialogue box finished
    s_cutscene_active_frames.store(0, std::memory_order_relaxed);
  } else {
    // Dialogue box actively waiting/displaying
    s_cutscene_active_frames.store(3, std::memory_order_relaxed);
  }
}

// =======================================================================
// 3D CUTSCENE (DEMO) SYSTEM HOOKS
// =======================================================================

// DEMO_END_CHECK (USA: 0x821836C0)
// Returns 0 while cutscene is playing, 1 when cutscene has ended
REX_EXTERN(__imp__sub_821836C0);
REX_EXTERN(sub_821836C0) {
  __imp__sub_821836C0(ctx, base);
  if (ctx.r3.u32 == 1) {
    OnCutsceneOrDialogueEnded();
  } else {
    // Cutscene is running: check if nearing end (closing seconds trigger)
    if (IsDemoNearingEnd(base)) {
      OnCutsceneOrDialogueEnded();
    } else {
      s_cutscene_active_frames.store(3, std::memory_order_relaxed);
    }
  }
}

// DEMO_CAMERA_END_CHECK (USA: 0x82183778)
// Returns 0 while cutscene camera is active, 1 when camera demo has ended
REX_EXTERN(__imp__sub_82183778);
REX_EXTERN(sub_82183778) {
  __imp__sub_82183778(ctx, base);
  if (ctx.r3.u32 == 1) {
    OnCutsceneOrDialogueEnded();
  } else {
    s_cutscene_active_frames.store(3, std::memory_order_relaxed);
  }
}
