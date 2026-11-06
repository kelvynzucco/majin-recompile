# Local performance diagnostics

The updated Windows candidate can record guest FPS/frame times without MSI Afterburner.
Older downloaded ZIPs do not gain this feature automatically.

Open **Diagnostics / privacy** in the launcher:

- **Record FPS / frame times locally** is on by default and can be disabled.
- **Include optional CPU, RAM and GPU/driver details** is off by default. Enable it
  only if you want those details recorded for support. Preferences save on Play.
- **Create diagnostic ZIP** exports selected local files for review. Nothing is
  uploaded. You decide whether to share the ZIP and with whom.

## What the measurement means

The SDK timestamps consecutive guest GPU swap packets using its monotonic host
clock. This measures the game's frame-production interval, including waits,
loading, pauses and stalls. It does **not** measure GPU execution, input latency,
unique displayed frames or the monitor's refresh rate. An external presentation
overlay can report a different rate, especially when the same image is presented
more than once. Menus, videos and gameplay can also use different frame rates.

The first zero interval is excluded. FPS is calculated from total valid intervals
divided by their summed duration, not an average of instantaneous FPS readings.
P50/P95/P99 use nearest rank over all valid intervals; long intervals are retained.
The one-second series uses windows of at least one measured second, with the last
partial window retained. A long stall remains its full duration rather than being
compressed into a one-second bucket. Loading and idle intervals are not classified
as gameplay or silently removed. Use the same checkpoint/settings for comparisons.

## Files in reports

Each new `runtime-<session>.log` can have:

| Suffix | Contents |
| --- | --- |
| `.performance.csv` | Buffered per-frame SDK counters; `frame_time_us` is microseconds. The SDK integer `fps` column is coarse; the summary recalculates precise averages. Other SDK counters may remain zero in Release and are not used as evidence. |
| `.performance-1s.csv` | Elapsed measured milliseconds, interval count, window duration, average FPS, mean and maximum frame time. Elapsed time starts at the first measured interval, not process launch. |
| `.performance-summary.json` | FPS, mean/min/max/P50/P95/P99 frame times, counts above 33.333/50/100 ms, warning/error counts, texture occurrence/suppression totals, XMA recovery count and selected GPU. |
| `.settings.json` | Render/input settings and capture choices. The ZIP export omits game-data/cache paths. |
| `.hardware.json` | Optional allowlisted OS/build, CPU model/core counts, usable RAM size, GPU names and driver versions. Created only with opt-in. |

A short FPS/frametime summary is appended to the main log after the game exits.
Existing bounded texture warnings retain their original samples and counts;
diagnostics do not bypass texture validation or select another rendering path.

## Privacy and limits

No hardware serials, UUIDs, user/computer names, SIDs, IP/MAC addresses or installed
application lists are queried or exported as hardware fields. The active graphics
adapter already appears in ordinary runtime logs even with optional hardware off.
All installed adapters may be listed when hardware is included; the summary's active
GPU identifies the adapter selected by the runtime. No utilization, temperatures or
power measurements are inferred from the static hardware inventory.

The regular and assisted collectors export a bounded set of session artifacts,
redact known local/user/network paths, and omit separate hardware artifacts unless
opted in. `-SkipHardware` overrides saved consent and `-IncludeHardware`.
Assisted Windows failure events export only time, event/provider and a small
application/module/version/exception allowlist, not full event XML or messages.
Redaction is best effort: always inspect the resulting ZIP before sharing.

CSV writes are buffered, flushed every 60 rows and capped at 2,000,000 rows per
Release process (roughly 9 hours at 60 FPS). The limit logs a warning and stops
capture while the game continues. The final buffered rows, up to 59, can be lost
on hard exit/crash. Capture adds some disk/CPU work; disable it for an overhead
comparison, especially when running from slow/removable storage.

Offline analysis accepts at most 128 MiB, 2,000,000 rows and 4096 characters per
record. Corrupt rows are counted and excluded; missing samples are not converted
into fictitious zero-frame times. Collector ZIPs copy at most 8 MiB per raw frame
CSV and 2 MiB for other artifacts, within a 25 MiB total. Oversized CSV/logs retain
their tail; the CSV header is preserved and truncation is listed in the inventory.
The complete local summary remains distinct from a truncated raw excerpt.

The current build was exercised with actual Release capture on Intel Arc B580.
The mechanism has no vendor-specific counter dependency, but Radeon/NVIDIA and
multi-hour gameplay testing are still pending. This is diagnostic coverage, not
a performance improvement or hardware certification.

## Validation of the updated candidate

October 23: 52 synthetic checks passed for weighted FPS, percentiles, stalled and
invalid intervals, bounded CSV tails, hardware opt-in/opt-out, path redaction and
Windows failure-event fields. The launcher/privacy layout passed automated checks.
Isolated Release startup/menu sessions on Arc B580 captured 510 valid intervals in
Default and 444 in ROV; a separate opt-out session created no frame CSV. All three
exited normally with no error/critical records. Active saves and caches were verified
unchanged. These short runs prove capture, not gameplay performance or multi-hour
stability, and their averages must not be compared as equivalent workloads.

## Windowed mode and live title FPS

Windowed mode uses a normal resizable Windows frame with minimize, maximize and
close buttons. When the requested output size would exceed the monitor's usable
desktop area, the initial client size is reduced proportionally and centered,
leaving the caption/taskbar accessible. Internal render scale is unchanged.
The chosen output size is therefore an initial maximum rather than permission
to hide the caption beyond the screen. Maximizing/restoring remains available.

The title shows `Majin Recompile | <n> FPS` only in windowed mode. It refreshes
twice per second using the latest completed interval window of at least one
second, rounded to a whole FPS. It measures guest swaps, not monitor refresh or
input latency. The title counter works with CSV recording off, does not create
an overlay and continues after the CSV row limit. Before the first sample it
shows `... FPS`; minimized windows report zero until restored. The SDK integer
FPS column may use this rolling value; offline summaries still derive precise
averages from individual frame intervals.

October 23: 54 native helper checks passed. Isolated real-window tests confirmed
caption/minimize/maximize/system menu/resizing, work-area bounds on the 1360x768
display, minimize/maximize/restore, numeric FPS with CSV off and on, no title FPS
in fullscreen, CSV capture when enabled and normal close. These were startup/menu
tests and do not establish gameplay performance. Both the project Release entry
and the separate candidate are updated together to avoid stale executables.

## Rendering path and GPU load

The launcher's **Rendering path** now defaults to **Default**. Default passes an
empty `render_target_path_d3d12`, so the runtime chooses by GPU vendor: host
render targets (RTV) on NVIDIA and AMD, and ROV on Intel, where the RTV stencil
path is unreliable. **ROV Compatibility** forces EDRAM emulation in pixel shaders
on every GPU; the runtime itself notes that this path is generally much slower.
Earlier builds defaulted to ROV and labelled it as recommended for NVIDIA/AMD.
Saved launcher settings keep their previous value, so anyone upgrading from a
build that saved ROV should select Default once in the launcher.

November 6: on an RTX 3060 Ti (driver 617.42) with i7-9700K, 1920x1080 windowed,
render scale 1x, 60 FPS mode, early cave gameplay, 60 s each:

| Path | Avg power | P95 power | Avg graphics clock | Frame rate |
| --- | ---: | ---: | ---: | ---: |
| ROV | 76 W | 104 W | 1679 MHz | 60 |
| Default (RTV) | 54 W | 64 W | 1209 MHz | 60 |

Title screen, intro video and this gameplay segment rendered correctly with
Default. The two runs used nearby but not identical positions, and this is a
short sample, not a full-campaign compatibility check. If a scene renders
incorrectly with Default, switch that session to ROV and report the location.

### Measuring GPU load (NVIDIA)

`scripts/measure-gpu.ps1` samples `nvidia-smi` twice per second while
`majin.exe` runs and prints average/P50/P95/max for utilization, power, graphics
clock and VRAM. Raw samples go to `reports/gpu-<label>-<time>.csv`. It reads
counters only and changes no settings. Compare paths at the same checkpoint and
settings; prefer power and clock over utilization, which stays similar while the
GPU boosts to different clocks.

```powershell
.\scripts\measure-gpu.ps1 -Label rtv-1080p -Seconds 60 -WaitForGame
```

## Render scale and upscaling

Render scale multiplies the internal resolution of every pass, and the EDRAM
emulation work (resolves, format conversions and render-to-texture effects)
grows with the pixel count. November 6, RTX 3060 Ti, Default path, 1920x1080
window, 60 FPS mode, standing still in the same room for 40 s per run:

| Settings | Avg power | Avg utilization | Avg graphics clock |
| --- | ---: | ---: | ---: |
| 1x + FXAA | 63 W | 37% | 1490 MHz |
| 2x + FXAA | 199 W | 91% | 1895 MHz |
| 2x, no FXAA | 198 W | 88% | 1895 MHz |

2x held 60 FPS but ran at the card's 200 W power limit; FXAA cost is negligible.
The launcher's **Upscaler** option (`run-usa.ps1 -Upscaler fsr|cas|bilinear`,
runtime `present_effect`) applies AMD FSR 1 or CAS when the image is scaled to
the window. FSR 1 is spatial, runs on any GPU vendor and adds little cost, so
1x + FSR 1 is the lighter alternative to 2x. FSR 2/3/4 and DLSS are not
offered: they need per-pixel motion vectors and depth that this game does not
produce. `bilinear` keeps the original plain stretch.
