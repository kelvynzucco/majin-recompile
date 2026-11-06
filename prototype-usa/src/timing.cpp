// SPDX-License-Identifier: GPL-3.0-only
// Adapted for Majin and the Forsaken Kingdom (USA 0.0.0.2)
#include "generated/default/majin_pch.h"
#include "equivalent_counter.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/audio/audio_driver.h>
#include <bit>
#include <cmath>

REXCVAR_DEFINE_BOOL(majin_60fps, false, "Graphics",
    "Opt-in 60 FPS test mode for the USA 0.0.0.2 revision");

namespace {
thread_local uint32_t main_object = 0;
constexpr uint32_t kRatio = 0x150F8, kRate = 0x15104;
bool Enabled() {
  static const bool enabled = rex::cvar::Query<bool>("majin_60fps");
  return enabled;
}
bool Span(uint32_t object, uint32_t size) {
  return object >= 0x10000 && object <= UINT32_MAX - size;
}
float ReadFloat(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(REX_LOAD_U32(address));
}
}

extern bool IsBackOrTabHeld();
extern bool IsTurboAllowed();
extern void OnTurboFrameTick();
extern void OnTurboDisengaged();
extern void TickCutsceneTiming();
extern float GetCutsceneSpeedMultiplier();

// Below 0x821C0000: USA address is identical to PAL (0x82162238, caller LR 0x821616EC)
REX_EXTERN(__imp__sub_82162238);
REX_EXTERN(sub_82162238) {
  const uint32_t object = ctx.r3.u32;
  const bool caller = uint32_t(ctx.lr) == 0x821616EC;
  __imp__sub_82162238(ctx, base);
  if (!caller || !Span(object, kRate + 4)) return;

  const float rate = ReadFloat(base, object + kRate);
  float ratio = ReadFloat(base, object + kRatio);

  const bool turbo = IsTurboAllowed() && IsBackOrTabHeld();

  if (turbo && std::isfinite(ratio)) {
    OnTurboFrameTick();
    const float speed_mult = GetCutsceneSpeedMultiplier();
    ratio *= speed_mult;
    REX_STORE_U32(object + kRatio, std::bit_cast<uint32_t>(ratio));
    REX_STORE_U32(object + 0x150F4, std::bit_cast<uint32_t>(ratio / rate));
    rex::audio::SetPlaybackFrequencyRatio(speed_mult);
  } else {
    OnTurboDisengaged();
    rex::audio::SetPlaybackFrequencyRatio(1.0f);
  }

  TickCutsceneTiming();

  if (!Enabled()) return;
  main_object = rate == 30.0f && std::isfinite(ratio) && ratio >= 0 && ratio <= 64
                    ? object : 0;
  static const bool announced = [] {
    REXLOG_INFO("[Majin 60 FPS USA] opt-in mode; engine time calculation unchanged; title idle timer normalized");
    return true;
  }();
  (void)announced;
}

// Above 0x821C0000: PAL 0x821CC390 -> USA 0x821CC320 (delta -0x70). Vtable in .rdata is 0x820EC3C4.
REX_EXTERN(__imp__sub_821CC320);
REX_EXTERN(sub_821CC320) {
  if (!Enabled()) { __imp__sub_821CC320(ctx, base); return; }
  const uint32_t object = ctx.r3.u32;
  thread_local majin::timing::EquivalentCounter counter;
  thread_local uint32_t owner = 0;
  if (!main_object || !Span(object, 100) ||
      REX_LOAD_U32(object) != 0x820EC3C4 ||
      REX_LOAD_U32(object + 48) != 6 || REX_LOAD_U32(object + 88) > 870) {
    counter.Reset(); owner = 0;
    __imp__sub_821CC320(ctx, base); return;
  }
  if (owner != object) { counter.Reset(); owner = object; }
  const double ratio = ReadFloat(base, main_object + kRatio);
  REX_STORE_U32(object + 88,
      counter.BeforeIncrement(REX_LOAD_U32(object + 88), ratio));
  __imp__sub_821CC320(ctx, base);
}
