# contrib/

Utility scripts for working with AltirraSDL from the outside.

---

## cas_tester.py — Batch .cas tape tester

Automated batch-testing of Atari 8-bit cassette images (`.cas` files).
The script launches AltirraSDL for each file, boots it, monitors tape
motor state via the built-in IPC channel, captures screenshots every
2 seconds, and classifies each run as **success** or **crash**.

### Prerequisites

| Requirement | Notes |
|---|---|
| AltirraSDL (this repo) | Built from `tape-testing-automation` branch or later |
| Python 3.8+ | Standard library only for core operation |
| `Pillow` *(optional)* | `pip install Pillow` — enables frozen-screen detection |
| `pytesseract` + `tesseract` *(optional)* | `pip install pytesseract` + system tesseract — enables OCR crash detection |

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
```

### Options

| Flag | Default | Description |
|---|---|---|
| `--altirra PATH` | *(required)* | Path to AltirraSDL executable |
| `--output DIR` | *(required)* | Root output directory |
| `--list FILE` | — | Text file with one `.cas` path per line |
| `--motor-timeout S` | 90 | Seconds to wait for tape motor to start before declaring crash |
| `--grace S` | 10 | Seconds motor must stay off before declaring success |
| `--timeout S` | 300 | Hard per-CAS cutoff |
| `--ntsc` / `--pal` | — | Override video standard |

### Output layout

```
results/
  game_name/
    frame_0001_t0.000.png    ← frame sequence + tape position in filename
    frame_0002_t2.341.png
    ...
    result.json              ← full per-CAS data (outcome, tape position, etc.)
  report.json                ← summary across all tested files
```

`result.json` fields:

| Field | Description |
|---|---|
| `outcome` | `"success"`, `"crash"`, `"skipped"`, or `"aborted"` |
| `motor_stopped_at` | Tape position in seconds when motor last stopped (success only) |
| `motor_started` | Whether the tape motor ever ran |
| `crash_reason` | Human-readable description of why it was classified as a crash |
| `screenshots` | Array of `{seq, file, tape_pos, motor, elapsed}` entries |
| `duration_s` | Total wall-clock time for this run |

### Crash detection

The script uses several complementary detection methods, applied in order:

1. **`sim.running = false`** — the emulated CPU halted (illegal opcode, power-off, etc.)
2. **Motor timeout** — the tape motor never started within `--motor-timeout` seconds.
   This catches BOOT ERROR, SELF TEST, and cases where no suitable OS firmware is
   configured.
3. **Frozen screen** — the last 5 screenshots are identical and the motor has never
   started. Requires `Pillow`.
4. **OCR** — tesseract scans each screenshot for the strings `BOOT ERROR`,
   `SELF TEST`, and `MEMO PAD`. Requires `pytesseract` and a system `tesseract`
   installation. Experimental — accuracy depends on emulated display scale.
5. **Hard timeout** — the `--timeout` value is a safety net for tapes that loop
   indefinitely or trigger an infinite-load condition.

### Success detection

**Success** is declared when:

1. The tape motor has run at least once (tape was read), **and**
2. The motor has been continuously off for `--grace` seconds (default 10 s)
   without restarting.

This corresponds to the program finishing its load phase and the OS releasing
the cassette port. The tape position at the moment the motor last stopped is
recorded in `motor_stopped_at` and also encoded in the screenshot filenames
for the grace-period frames.

---

## Emulator changes in this branch

The `tape-testing-automation` branch adds one functional change to AltirraSDL
on top of the macOS portability fixes in `main`:

### `src/AltirraSDL/source/ui_testmode.cpp` — cassette state in `query_state`

The IPC `query_state` command response now includes a `cassette` object:

```json
{
  "ok": true,
  "state": {
    "sim":      { "running": true, "paused": false, ... },
    "cassette": { "motor": true, "position": 12.345, "length": 180.000 },
    "dialogs":  { ... },
    "windows":  [ ... ]
  }
}
```

| Field | Type | Description |
|---|---|---|
| `motor` | bool | Whether the tape transport motor is currently running |
| `position` | float | Current read head position in seconds (3 decimal places) |
| `length` | float | Total tape length in seconds |

This is the data source for all motor-based decisions in `cas_tester.py`.
Without this change the script cannot distinguish a running load from a
crashed machine.

### How the IPC channel works

AltirraSDL's test mode is activated by passing `--test-mode` on the command
line. On POSIX systems (macOS, Linux) it opens a Unix domain socket at:

```
/tmp/altirra-test-<pid>.sock
```

The protocol is simple line-oriented text over a stream socket:

- **Request**: one UTF-8 text line, newline-terminated
- **Response**: one JSON object, newline-terminated

`cas_tester.py` connects to this socket, polls `query_state` every 2 seconds,
and issues `screenshot <path>` to trigger a PNG capture on the next rendered
frame.

All available IPC commands can be listed at runtime by sending `help\n` to the
socket.
