# contrib/

Utility scripts for working with AltirraSDL from the outside.

---

## cas_tester.py — Batch .cas / .wav / .mp3 tape tester

Automated batch-testing of Atari 8-bit cassette images. Accepts FUJI-chunk
`.cas` digital dumps, `.wav` analogue captures of real tapes, and `.mp3`
files (transparently converted to WAV via ffmpeg).
The script launches AltirraSDL for each file, boots it, monitors tape
motor state and CPU/memory state via the built-in IPC channel, captures
BMP screenshots every N emulator frames (default 50 = 1 PAL second),
classifies each run as **success** or **crash**, optionally stitches
the screenshots into an MP4 video, then generates a summary contact-sheet
panel and an upscaled final screenshot from the deduplicated frames.

### Prerequisites

| Requirement | Notes |
|---|---|
| AltirraSDL (this repo) | Built from `tape-testing-automation` branch or later |
| Python 3.8+ | Standard library only for core operation |
| `Pillow` *(optional)* | `pip install Pillow` — enables frozen-screen detection and panel generation |
| `pytesseract` + `tesseract` *(optional)* | `pip install pytesseract` + system tesseract — enables OCR crash detection |
| `ffmpeg` *(optional)* | In `PATH` — enables video assembly and MP3 → WAV conversion |

### Quick start

```bash
# Test a single file
python3 contrib/cas_tester.py \
    --altirra ./AltirraSDL \
    --output  ./results \
    /path/to/game.cas

# Test a whole folder
python3 contrib/cas_tester.py \
    --altirra ./AltirraSDL \
    --output  ./results \
    /path/to/tapes/*.cas

# Test from a list file (one path per line, # = comment)
python3 contrib/cas_tester.py \
    --altirra ./AltirraSDL \
    --output  ./results \
    --list    my_tapes.txt

# Re-generate panels for existing results (no emulator needed)
python3 contrib/cas_tester.py \
    --output ./results \
    --postprocess-results \
    --similarity 0.8 \
    --panel-cols 6
```

### Options

#### Emulation

| Flag | Default | Description |
|---|---|---|
| `--altirra PATH` | *(required for testing)* | Path to AltirraSDL executable |
| `--output DIR` | *(required)* | Root output directory |
| `--list FILE` | — | Text file with one `.cas` or `.wav` path per line |
| `--frames N` | 50 | Emulator frames between screenshots (50 = 1 s PAL, 60 = 1 s NTSC) |
| `--motor-timeout S` | 90 | Wall-clock seconds to wait for tape motor to start before declaring crash |
| `--grace S` | 20 | Emulator-seconds motor must stay off before declaring success |
| `--timeout S` | 1200 | Hard wall-clock cap per CAS (20 min) |
| `--emu-timeout S` | 1320 | Hard emulated-seconds cap per CAS (22 min of machine time) |
| `--video-fps N` | 10 | Output video framerate |
| `--no-video` | — | Skip ffmpeg video assembly |
| `--ntsc` / `--pal` | — | Override video standard |
| `--with-basic` | — | Boot with BASIC enabled (only START pressed, no OPTION). Default is BASIC off via `--nobasic` |
| `--debug` | — | Print every IPC send/recv line (verbose) |

#### Panel / post-processing

| Flag | Default | Description |
|---|---|---|
| `--panel-cols N` | 3 | Images per row in the summary panel |
| `--similarity F` | 0.7 | Cosine similarity threshold (0.0–1.0). Consecutive frames with similarity ≥ this value vs. the last selected frame are filtered out. Higher = more aggressive filtering |
| `--min-gap S` | 0 | Minimum time distance in seconds between selected panel frames (0 = off) |
| `--panel-width PX` | 1280 | Width of the upscaled final screenshot in pixels (height scales proportionally). The panel itself is not resized |
| `--keep-source` | — | Keep source BMP frames after panel generation (default: delete them) |
| `--no-panel` | — | Skip panel generation and BMP cleanup entirely |
| `--postprocess-results` | — | Re-run panel generation on existing result directories (no emulator needed, `--altirra` not required) |

#### Post-run actions

| Flag | Default | Description |
|---|---|---|
| `--move-cas` | — | After testing, move each tape file (`.cas`, `.wav`, or `.mp3`) to a `done/` folder (created as a sibling of `--output`). For MP3 runs the original MP3 is moved, not the temp WAV |

Altirra is launched with `--warp`, `--novsync`, `--nosiopatch`, `--nobasic`,
and `--casautoboot` automatically. SIO patches are explicitly disabled so that
the OS drives tape loading via real SIO routines — motor timing, IRG handling,
and error recovery behave exactly as on real hardware.

### Output layout

```
results/
  game_name/
    frame_0001_t000.000.bmp   ← source frames (deleted by default; --keep-source to retain)
    frame_0002_t002.341.bmp
    ...
    result.json               ← full per-CAS data (outcome, tape position, etc.)
    video.mp4                 ← assembled timelapse (if ffmpeg available)
    panel.png                 ← contact sheet of deduplicated frames
    final.png                 ← last frame, upscaled to --panel-width
  report.json                 ← cumulative summary across all runs
done/                         ← tested .cas files land here (--move-cas)
```

### `result.json` fields

| Field | Description |
|---|---|
| `outcome` | `"success"`, `"crash"`, `"skipped"`, or `"aborted"` |
| `cas` | Path to the **original** input file (for MP3 runs, the `.mp3` not the temp `.wav`) |
| `tape_format` | `"cas"`, `"wav"`, or `"mp3"` — auto-detected from file extension |
| `converted` | Boolean: whether the input was transcoded before testing |
| `converted_to` | `"wav"` when `converted=true`, otherwise `null` |
| `with_basic` | Boolean: whether the run was booted with BASIC enabled |
| `motor_stopped_at` | Tape position (seconds) when motor last stopped (success only) |
| `motor_started` | Whether the tape motor ever ran |
| `crash_reason` | Human-readable description of why it was classified as a crash |
| `screenshots` | Array of `{seq, file, tape_pos, motor, pc, portb, elapsed}` entries. The last entry also carries `"final": true` — captured after the termination decision to guarantee the terminal screen state is recorded |
| `video` | Filename of assembled video, or `null` |
| `duration_s` | Total wall-clock time for this run |
| `timestamp` | ISO 8601 timestamp of the run |

### `report.json` format

`report.json` is cumulative — each invocation appends a new run rather than
overwriting. The top-level object carries lifetime aggregates and a `runs`
array with per-invocation detail:

```json
{
  "updated": "2026-04-06T20:00:00.000000",
  "total": 25,
  "success": 18,
  "crash": 6,
  "skipped": 1,
  "aborted": 0,
  "runs": [
    {
      "generated": "2026-04-06T19:00:00.000000",
      "altirra": "./AltirraSDL",
      "total": 10,
      "success": 7,
      "crash": 3,
      "skipped": 0,
      "aborted": 0,
      "results": [ ... ]
    },
    {
      "generated": "2026-04-06T20:00:00.000000",
      "...": "..."
    }
  ]
}
```

If the script encounters a legacy single-run `report.json` (from an older
version without the `runs` array), it is automatically migrated into the
new format as the first entry.

### Panel generation

After video assembly (or independently via `--postprocess-results`), the
script builds two PNG outputs from the captured BMP frames:

**`panel.png`** — a contact-sheet grid of visually distinct frames. The
filtering pipeline works as follows:

1. The first frame is always selected.
2. Each subsequent frame is compared to the last *selected* frame using
   cosine similarity on downscaled greyscale thumbnails (32×24 pixels).
3. If the similarity is ≥ `--similarity` (default 0.7), the frame is
   skipped as a near-duplicate.
4. If `--min-gap` is set, frames closer than that many seconds (by tape
   position) to the last selected frame are also skipped.
5. The last frame is always included regardless of similarity.
6. Surviving frames are tiled into a grid of `--panel-cols` columns
   (default 3) and as many rows as needed, at their original
   resolution.

**`final.png`** — the very last captured frame, upscaled to
`--panel-width` pixels wide (default 1280) with proportional height.
This represents the final state of the emulated screen at the end of
the test.

After panel generation, source BMP files are deleted unless
`--keep-source` is specified. The `.mp4` video is never affected.

Use `--no-panel` to skip panel generation entirely and leave all BMPs
in place.

### Postprocess mode

`--postprocess-results` re-runs panel generation across all existing
result subdirectories under `--output` without launching the emulator.
This is useful for re-tuning `--similarity`, `--panel-cols`, or
`--panel-width` after a batch run, or for generating panels from
results that were originally created with `--no-panel`.

```bash
# Regenerate all panels with tighter filtering and wider output
python3 contrib/cas_tester.py \
    --output ./results \
    --postprocess-results \
    --similarity 0.85 \
    --panel-width 960 \
    --panel-cols 5
```

The mode scans for subdirectories containing a `result.json` and
processes each one. Existing `panel.png` and `final.png` files are
overwritten. If `--keep-source` is not set and BMP frames still exist,
they are removed.

### Crash detection

The script uses several complementary detection methods, evaluated each
polling iteration in the following order:

1. **IPC connection lost** — the emulator process died or the socket broke.

2. **Machine halted** — `sim.running = false` in the IPC state (illegal
   opcode, power-off, etc.).

3. **Self-test ROM (PC-based)** — the program counter is in `$5000–$57FF`
   while the XL/XE self-test ROM is actually banked in. This requires
   **both** PORTB bit 0 = 1 (OS ROM enabled) **and** PORTB bit 7 = 0
   (self-test ROM mapped instead of RAM). When bit 7 = 1, ordinary RAM
   occupies that region and a tape-loaded program may legitimately execute
   there — no false positive is raised.

4. **Boot error (PC-based)** — the program counter is in `$C400–$C4FF`
   (the XL OS BOOT ERROR handler), the motor has never run, and at least
   10 seconds have elapsed. The motor guard prevents false positives when
   a loaded program legitimately uses that address range.

5. **Frozen screen** — the last 5 consecutive screenshots are byte-identical,
   the motor has never started, and the motor-start timeout has been
   exceeded. Requires `Pillow`.

6. **OCR** — `pytesseract` scans each screenshot for the strings
   `BOOT ERROR`, `SELF TEST`, `MEMO PAD`, and `BOOT ERRO`. Experimental —
   accuracy depends on emulated display scale. Requires `pytesseract` and
   a system `tesseract` installation.

7. **Motor timeout** — the tape motor never started within
   `--motor-timeout` seconds.

8. **Tape end with motor running** — the tape position has reached the
   end (`pos >= length`) but the motor is still on. The loader is almost
   certainly stuck waiting for more data that will never come. The script
   waits for 10 consecutive samples in this state before declaring a
   crash, to avoid false positives from minor tape-length rounding.

9. **Wall-clock timeout** — `--timeout` seconds (default 1200 = 20 min)
   of real time, safety net for anything that loops indefinitely.

10. **Emulated-time timeout** — `--emu-timeout` seconds (default 1320 =
    22 min) of emulated machine time. Independent of wall-clock; since the
    emulator runs in `--warp` mode, one real second can equal many
    emulated seconds, so this cap catches genuinely long runs regardless
    of warp speed.

11. **`wait_frames` timeout** — the emulator did not advance the requested
    number of frames within 30 seconds (emulator frozen or hung).

### Final screenshot guarantee

After the main polling loop exits — for **any** reason (success, crash,
or timeout) — one last screenshot is captured and appended to
`result.screenshots` with a `"final": true` marker. This ensures the
terminal screen state (BOOT ERROR message, SELF TEST display, success
screen, etc.) is always recorded, even if the crash-detection `break`
fired before that iteration's regular screenshot could be taken.

The panel generator also always force-includes the last frame, so this
final screenshot is guaranteed to appear in `panel.png` and to be the
source image for `final.png`, even if it happens to resemble earlier
frames closely enough to otherwise be filtered out.

### Success detection

**Success** is declared when:

1. The tape motor has run at least once (tape was read), **and**
2. The motor has been continuously off for `--grace` consecutive polling
   iterations (default 20) without restarting.

This corresponds to the program finishing its load phase and the OS releasing
the cassette port. The Atari OS keeps the motor running across adjacent
records when the inter-record gap is short, so the grace period avoids false
positives during normal multi-record loads.

The tape position at the moment the motor last stopped is recorded in
`motor_stopped_at`.

### Tape format support

Three input formats are accepted:

- **`.cas`** — FUJI-chunk digital dump. Passed directly to Altirra via `--tape`.
- **`.wav`** — analogue capture of a real cassette (any sample rate
  Altirra accepts). Passed directly via `--tape`.
- **`.mp3`** — automatically converted when ffmpeg is available in PATH.
  The script invokes `ffmpeg -i input.mp3 -ar 44100 -ac 1 output.wav` to
  produce a mono 44.1 kHz temp WAV next to the output directory (44.1 kHz
  is ample for Atari's FSK tones at 3995 Hz and 5327 Hz on NTSC), tests
  that, then deletes it. The result records `tape_format="mp3"`,
  `converted=true`, and `converted_to="wav"`, while `cas` still points
  at the original MP3.

If ffmpeg is not available, `.mp3` inputs are skipped with an
explanatory message (they're not silently ignored).

Format is auto-detected from the file extension and logged in
`result.json`, which is useful when batch-testing mixed inputs or
comparing loader behaviour between digital and analogue sources.

### BASIC boot mode

By default the script launches Altirra with `--nobasic`, which
disables the built-in BASIC cartridge — equivalent to holding OPTION
during boot on real hardware. This is the standard way to boot
machine-language tape loaders on XL/XE.

Pass `--with-basic` to leave BASIC enabled (only START pressed during
boot). This is needed for tapes that load BASIC programs via
`CLOAD`/`ENTER`, or for loaders that expect BASIC ROM to be present.
The boot mode is logged in `result.json` as `with_basic: true/false`.

### PORTB ($D301) reference — 130XE memory banking

The self-test and boot-error detectors inspect PORTB to determine what is
mapped into memory at the program counter's address. The relevant bits for
the 130XE:

| Bit | Address range | 0 | 1 |
|-----|---------------|---|---|
| 7 | `$5000–$57FF` | Self-test ROM | RAM |
| 6 | *(not used)* | — | — |
| 5 | `$4000–$7FFF` | ANTIC accesses extended RAM | ANTIC accesses main RAM |
| 4 | `$4000–$7FFF` | CPU accesses extended RAM | CPU accesses main RAM |
| 3 | `$4000–$7FFF` | Bank selection bit | |
| 2 | `$4000–$7FFF` | Bank selection bit | |
| 1 | `$A000–$BFFF` | Atari BASIC ROM | RAM |
| 0 | `$C000–$FFFF` | RAM | OS ROM |

### IPC protocol note

The `screenshot` IPC command sends a JSON acknowledgement back over the
socket. This response **must** be consumed (read and discarded). If it is
not, the acknowledgement sits in the receive buffer and the next
`query_state` call reads it instead of its own reply — fully
desynchronising the response stream. `AltirraIPC.screenshot()` uses
`command()` (send + recv) rather than `send()` to prevent this.

---

## Emulator changes in this branch

The `tape-testing-automation` branch adds the following to AltirraSDL on
top of the macOS portability fixes in `main`:

### `src/AltirraSDL/source/ui_testmode.cpp` — extended `query_state`

The IPC `query_state` command response now includes `cassette`, `cpu`, and
`memory` objects:

```json
{
  "ok": true,
  "state": {
    "sim":      { "running": true, "paused": false },
    "cassette": { "motor": true, "position": 12.345, "length": 180.000 },
    "cpu":      { "pc": 8192 },
    "memory":   { "portb": 255 },
    "dialogs":  { },
    "windows":  [ ]
  }
}
```

| Section | Field | Type | Description |
|---------|-------|------|-------------|
| `cassette` | `motor` | bool | Whether the tape transport motor is currently running |
| | `position` | float | Current read-head position in seconds |
| | `length` | float | Total tape length in seconds |
| `cpu` | `pc` | int | Current program counter value |
| `memory` | `portb` | int | Current value of PIA port B (`$D301`) |

### How the IPC channel works

AltirraSDL's test mode is activated by passing `--test-mode` on the command
line. On POSIX systems (macOS, Linux) it opens a Unix domain socket at:

```
/tmp/altirra-test-<pid>.sock
```

The protocol is simple line-oriented text over a stream socket:

- **Request**: one UTF-8 text line, newline-terminated
- **Response**: one JSON object, newline-terminated

`cas_tester.py` connects to this socket, polls `query_state` every
`--frames` emulator frames (default 50 = 1 PAL second), and issues
`screenshot "<path>"` to trigger a BMP capture on the next rendered frame.

All available IPC commands can be listed at runtime by sending `help\n` to
the socket.

---

## Author

`cas_tester.py` and the `tape-testing-automation` branch changes by
**krap/NG** — &lt;maciej.grzeszczuk@fhkd.pl&gt;
