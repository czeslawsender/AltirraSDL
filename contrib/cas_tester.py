#!/usr/bin/env python3
"""
cas_tester.py  —  Batch .cas file tester for Altirra SDL (macOS / Linux)
=========================================================================

Launches Altirra in test-mode IPC for each .cas file, boots it, monitors
cassette motor state, takes screenshots every N emulator frames (default
50 = 1 PAL second), and decides:

  SUCCESS : tape motor ran, then stopped and did NOT restart for
            --grace emulator-seconds. Records tape position at stop.
  CRASH   : machine halted, motor never started, frozen screen,
            OCR hit (pytesseract), or overall timeout exceeded.

After each CAS run, optionally stitches screenshots into a video via ffmpeg,
then generates a summary panel PNG from deduplicated frames and an upscaled
final screenshot.

Output layout:
  <out>/
    <cas_stem>/
      frame_0001_t0.000.bmp   (removed by default; --keep-source to retain)
      frame_0002_t1.000.bmp
      ...
      result.json
      video.mp4          (if ffmpeg available)
      panel.png          (filtered contact sheet)
      final.png          (last frame, upscaled)
    report.json          (cumulative across runs)

Requirements:
  Python 3.8+
  Optional: Pillow      (pip install Pillow)     — frozen-screen + panel
  Optional: pytesseract (pip install pytesseract) — OCR crash detection
  Optional: ffmpeg in PATH                        — video assembly

Standalone post-processing mode (no emulator needed):
  python3 cas_tester.py --output ./results --postprocess-results \\
      --similarity 0.8 --panel-cols 6

Notes:
  - Screenshots are BMP (SDL_SaveBMP). Extension .bmp is used explicitly.
  - Timing is emulator-frame-based via wait_frames IPC command.
    --frames 50 = 1 PAL second, --frames 60 = 1 NTSC second.
  - Altirra is launched with --warp and --novsync automatically.
  - SIO patches are explicitly disabled (--nosiopatch) for authentic
    OS-level tape loading behaviour.

IPC protocol note — screenshot response MUST be consumed:
  The screenshot command sends a JSON acknowledgement back over the socket.
  Failing to read it desynchronises the response stream: every subsequent
  command reads the PREVIOUS command's response instead of its own — causing
  motor=false / pos=0.0 / pc=0x0000 to appear on 2 out of every 3 polls.
  AltirraIPC.screenshot() now calls command() (send + recv) not just send().
"""

import argparse
import hashlib
import json
import math
import os
import shutil
import socket
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

# ---------------------------------------------------------------------------
# Optional dependencies
# ---------------------------------------------------------------------------
try:
    from PIL import Image
    HAS_PILLOW = True
except ImportError:
    HAS_PILLOW = False

try:
    import pytesseract
    HAS_TESSERACT = True
except ImportError:
    HAS_TESSERACT = False

HAS_FFMPEG = shutil.which("ffmpeg") is not None

# ---------------------------------------------------------------------------
# Tuning constants (overridable via CLI)
# ---------------------------------------------------------------------------
FRAMES_PER_SHOT      = 50     # emulator frames between screenshots (50 = 1s PAL)
MOTOR_START_TIMEOUT  = 90.0   # wall-clock seconds to wait for motor to first run
MOTOR_STOP_GRACE     = 10     # emulator-seconds after motor stops before success
OVERALL_TIMEOUT      = 1200.0 # hard wall-clock cap per CAS (20 min)
SOCKET_WAIT_TIMEOUT  = 15.0   # seconds to wait for Altirra socket
FROZEN_THRESHOLD     = 5      # consecutive identical screenshots -> frozen
VIDEO_FPS            = 10     # output video framerate

PANEL_COLS           = 3      # images per row in the summary panel
PANEL_SIMILARITY     = 0.7    # cosine similarity threshold; frames above this vs. the
                              # previously selected frame are skipped (0.0 = keep all)
PANEL_MIN_GAP        = 0.0    # min seconds between selected frames (0 = off)
PANEL_WIDTH          = 1280   # final screenshot width in pixels (panel is not resized)

CRASH_KEYWORDS = ["BOOT ERROR", "SELF TEST", "MEMO PAD", "BOOT ERRO"]

# ---------------------------------------------------------------------------
# Debug helpers
# ---------------------------------------------------------------------------
_debug_enabled = False

def dbg(msg: str):
    """Print a debug line when --debug is active."""
    if _debug_enabled:
        print(f"  [DBG] {msg}", flush=True)


# ---------------------------------------------------------------------------
# IPC controller
# ---------------------------------------------------------------------------
class AltirraIPC:
    def __init__(self, sock_path: str, timeout: float = SOCKET_WAIT_TIMEOUT):
        self._sock = None
        self._buf  = ""
        self._connect(sock_path, timeout)

    def _connect(self, sock_path: str, timeout: float):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if os.path.exists(sock_path):
                try:
                    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    s.connect(sock_path)
                    s.setblocking(False)
                    self._sock = s
                    dbg(f"Connected to {sock_path}")
                    return
                except OSError:
                    pass
            time.sleep(0.2)
        raise RuntimeError(f"Timed out waiting for socket: {sock_path}")

    def send(self, cmd: str):
        dbg(f"-> {cmd!r}")
        self._sock.sendall((cmd + "\n").encode())

    def _recv_line(self, timeout: float = 5.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                chunk = self._sock.recv(4096)
                if chunk == b"":
                    return None
                if chunk:
                    self._buf += chunk.decode(errors="replace")
            except BlockingIOError:
                pass
            nl = self._buf.find("\n")
            if nl >= 0:
                line = self._buf[:nl].rstrip("\r")
                self._buf = self._buf[nl + 1:]
                dbg(f"<- {line!r}")
                return line
            time.sleep(0.02)
        dbg("_recv_line: timeout")
        return None

    def command(self, cmd: str, timeout: float = 5.0):
        """Send cmd and return the parsed JSON response, or None on error."""
        try:
            self.send(cmd)
            line = self._recv_line(timeout)
            if line is None:
                dbg(f"command({cmd!r}): no response (timeout)")
                return None
            return json.loads(line)
        except (OSError, json.JSONDecodeError) as e:
            dbg(f"command({cmd!r}): error {e}")
            return None

    def query_state(self):
        return self.command("query_state")

    def screenshot(self, path: str):
        """
        Request a screenshot and consume the acknowledgement response.

        CRITICAL: the emulator sends {"ok":true,"path":"..."} for every
        screenshot command.  This response MUST be read here.  If we only
        call send() the ack sits in the socket buffer, and the next call
        (e.g. query_state) reads it instead of its own reply — fully
        desynchronising the response stream.  This was causing motor=false,
        pos=0.0, pc=$0000 on 2 of every 3 polling iterations.
        """
        resp = self.command(f'screenshot "{path}"')
        if resp is None or not resp.get("ok"):
            dbg(f"screenshot: unexpected response: {resp}")

    def wait_frames(self, n: int, timeout: float = 30.0) -> bool:
        resp = self.command(f"wait_frames {n}", timeout=timeout)
        ok = resp is not None and resp.get("ok", False)
        if not ok:
            dbg(f"wait_frames({n}): failed or timeout — resp={resp}")
        return ok

    def close(self):
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None


# ---------------------------------------------------------------------------
# Screenshot / OCR helpers
# ---------------------------------------------------------------------------
def _file_hash(path: str):
    try:
        return hashlib.sha1(open(path, "rb").read()).hexdigest()
    except OSError:
        return None

def _ocr_for_crash(path: str):
    if not HAS_TESSERACT or not os.path.exists(path):
        return None
    try:
        text = pytesseract.image_to_string(Image.open(path)).upper()
        for kw in CRASH_KEYWORDS:
            if kw in text:
                return kw
    except Exception:
        pass
    return None


# ---------------------------------------------------------------------------
# Panel helpers — similarity filtering + contact-sheet assembly
# ---------------------------------------------------------------------------
_THUMB_SIZE = (32, 24)          # thumbnail for cosine-similarity comparison


def _image_vector(path: str):
    """Load image → greyscale thumbnail → flat pixel list."""
    if not HAS_PILLOW:
        return None
    try:
        return list(
            Image.open(path).convert("L")
                 .resize(_THUMB_SIZE, Image.LANCZOS)
                 .getdata()
        )
    except Exception:
        return None


def _cosine_sim(a, b):
    """Cosine similarity between two equal-length numeric lists."""
    dot  = sum(x * y for x, y in zip(a, b))
    ma   = math.sqrt(sum(x * x for x in a))
    mb   = math.sqrt(sum(x * x for x in b))
    if ma == 0.0 or mb == 0.0:
        return 0.0
    return dot / (ma * mb)


def _tape_pos_from_bmp(name: str) -> float:
    """Extract tape position from a filename like frame_0012_t003.456.bmp."""
    try:
        return float(name.rsplit("_t", 1)[1].rsplit(".bmp", 1)[0])
    except (IndexError, ValueError):
        return 0.0


def postprocess_cas_dir(cas_dir: Path, *,
                        panel_cols: int    = PANEL_COLS,
                        similarity: float  = PANEL_SIMILARITY,
                        min_gap: float     = PANEL_MIN_GAP,
                        panel_width: int   = PANEL_WIDTH,
                        keep_source: bool  = False):
    """
    Post-process a single CAS result directory:
      1. Filter BMP frames by cosine similarity (+ optional min time gap).
      2. Build a contact-sheet PNG panel from the surviving frames.
      3. Upscale the *last* frame to panel_width as the "final" screenshot.
      4. Remove source BMPs (unless keep_source).

    Requires Pillow.  Silently skips if not available.
    """
    if not HAS_PILLOW:
        print("  [panel] Pillow not installed — skipping")
        return

    bmps = sorted(cas_dir.glob("frame_*.bmp"))
    if not bmps:
        return

    # ---- 1. Filter by similarity + optional time gap -----------------------
    selected = [bmps[0]]
    ref_vec  = _image_vector(str(bmps[0]))
    ref_pos  = _tape_pos_from_bmp(bmps[0].name)

    for bmp in bmps[1:]:
        cur_pos = _tape_pos_from_bmp(bmp.name)

        # Enforce minimum time gap
        if min_gap > 0.0 and (cur_pos - ref_pos) < min_gap:
            continue

        cur_vec = _image_vector(str(bmp))
        if ref_vec is not None and cur_vec is not None:
            sim = _cosine_sim(ref_vec, cur_vec)
            if sim >= similarity:
                continue            # too similar to the last selected frame

        selected.append(bmp)
        ref_vec = cur_vec
        ref_pos = cur_pos

    # Always include the very last frame even if it was filtered out
    if selected[-1] != bmps[-1]:
        selected.append(bmps[-1])

    print(f"  [panel] {len(bmps)} frames → {len(selected)} unique "
          f"(similarity={similarity}, min_gap={min_gap}s)")

    # ---- 2. Build contact-sheet panel --------------------------------------
    thumbs = []
    for p in selected:
        try:
            thumbs.append(Image.open(str(p)))
        except Exception:
            pass

    if not thumbs:
        return

    tw, th = thumbs[0].size         # all screenshots are the same size
    cols   = min(panel_cols, len(thumbs))
    rows   = math.ceil(len(thumbs) / cols)
    panel  = Image.new("RGB", (cols * tw, rows * th))

    for idx, img in enumerate(thumbs):
        r, c = divmod(idx, cols)
        panel.paste(img, (c * tw, r * th))

    panel_path = cas_dir / "panel.png"
    panel.save(str(panel_path))
    print(f"  [panel] {panel_path.name}  "
          f"{panel.width}×{panel.height}  ({cols}×{rows} grid)")

    # ---- 3. Upscale last frame ---------------------------------------------
    try:
        last = Image.open(str(bmps[-1]))
        lw, lh = last.size
        ls     = panel_width / lw
        last   = last.resize(
            (panel_width, round(lh * ls)),
            Image.LANCZOS,
        )
        final_path = cas_dir / "final.png"
        last.save(str(final_path))
        print(f"  [panel] {final_path.name}  {last.width}×{last.height}")
    except Exception as e:
        print(f"  [panel] final frame upscale failed: {e}")

    # Close images before deleting source files
    for img in thumbs:
        img.close()

    # ---- 4. Optionally remove source BMPs ----------------------------------
    if not keep_source:
        removed = 0
        for bmp in bmps:
            try:
                bmp.unlink()
                removed += 1
            except OSError:
                pass
        print(f"  [panel] removed {removed} source BMP(s)")


# ---------------------------------------------------------------------------
# ffmpeg video assembly
# ---------------------------------------------------------------------------
def make_video(cas_dir: Path, fps: int):
    if not HAS_FFMPEG:
        print("  [video] ffmpeg not found in PATH — skipping")
        return None
    if fps <= 0:
        return None

    # Give async screenshots a moment to flush to disk.
    # Screenshots are queued via IPC and written on the next rendered frame;
    # the last one may still be in-flight when we arrive here.
    time.sleep(0.5)

    bmps = sorted(cas_dir.glob("frame_*.bmp"))
    if not bmps:
        print(f"  [video] No BMP frames found in {cas_dir.resolve()} — skipping")
        return None

    # IMPORTANT: use a relative output filename (just the basename), not an
    # absolute or script-relative path.  ffmpeg resolves both -i and the
    # output path relative to cwd.  Passing an absolute path that was built
    # from a relative out_dir causes "No such file or directory" because
    # ffmpeg tries to CREATE that absolute path from within cwd, which would
    # require the entire parent chain to already exist under cwd.
    out_name = "video.mp4"
    out      = cas_dir / out_name          # for existence-check after the run

    print(f"  [video] {len(bmps)} frames @ {fps}fps")
    print(f"  [video] output : {out.resolve()}")
    print(f"  [video] cwd    : {cas_dir.resolve()}")

    # Concat demuxer list — all filenames relative to cas_dir
    concat   = cas_dir / "_ffmpeg_list.txt"
    duration = 1.0 / fps
    with open(concat, "w") as f:
        for p in bmps:
            f.write(f"file '{p.name}'\n")
            f.write(f"duration {duration:.6f}\n")

    cmd = [
        "ffmpeg", "-y",
        "-f", "concat", "-safe", "0",
        "-i", concat.name,       # relative to cwd — lives in cas_dir
        "-c:v", "libx264",
        "-pix_fmt", "yuv420p",
        "-loglevel", "warning",  # warnings + errors; switch to "verbose" if needed
        out_name,                # relative to cwd — resolves to cas_dir/video.mp4
    ]

    print(f"  [video] cmd: {' '.join(cmd)}")

    r = subprocess.run(
        cmd,
        cwd=str(cas_dir),        # both -i and output resolve relative to here
        capture_output=True,
        text=True,
    )

    concat.unlink(missing_ok=True)

    # Print ffmpeg output unconditionally so failures are always diagnosable
    if r.stdout.strip():
        print("  [ffmpeg stdout]")
        for line in r.stdout.strip().splitlines():
            print(f"    {line}")
    if r.stderr.strip():
        print("  [ffmpeg stderr]")
        for line in r.stderr.strip().splitlines():
            print(f"    {line}")

    if r.returncode != 0:
        print(f"  [video] ffmpeg failed (exit code {r.returncode})")
        return None

    if not out.exists():
        print(f"  [video] ffmpeg exit 0 but {out.resolve()} missing — bug")
        return None

    size_kb = out.stat().st_size // 1024
    print(f"  [video] OK — {size_kb} KB")
    return str(out)


# ---------------------------------------------------------------------------
# Single CAS test
# ---------------------------------------------------------------------------
def test_one_cas(altirra, cas_path, out_dir, extra_args,
                 frames_per_shot, motor_stop_grace, video_fps,
                 panel_opts=None):

    cas_name = Path(cas_path).stem
    cas_dir  = out_dir / cas_name
    cas_dir.mkdir(parents=True, exist_ok=True)

    result = {
        "cas":              cas_path,
        "stem":             cas_name,
        "outcome":          "unknown",
        "motor_stopped_at": None,
        "motor_started":    False,
        "crash_reason":     None,
        "screenshots":      [],
        "video":            None,
        "duration_s":       0.0,
        "timestamp":        datetime.now().isoformat(),
    }

    cmd = [
        altirra,
        "--test-mode",
        "--warp",
        "--novsync",
        # Disable SIO acceleration patches — we want the OS to drive tape
        # loading via real SIO routines so motor timing, IRG handling and
        # error recovery behave exactly as on real hardware.
        "--nosiopatch",
        "--tape",   cas_path,
        "--casautoboot",
        "--nobasic",
    ] + extra_args

    proc = None
    ipc  = None
    t_start = time.monotonic()
    motor_was_on    = False
    motor_off_count = 0    # consecutive polls with motor off after first run
    last_motor_pos  = 0.0  # last known tape position while motor was on
    last_hashes    = []
    iteration      = 0

    dbg(f"Launching: {' '.join(cmd)}")

    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL)
        sock_path = f"/tmp/altirra-test-{proc.pid}.sock"
        print(f"  PID {proc.pid} | socket: {sock_path}")

        try:
            ipc = AltirraIPC(sock_path, timeout=SOCKET_WAIT_TIMEOUT)
        except RuntimeError as e:
            result.update(outcome="crash",
                          crash_reason=f"IPC socket did not appear: {e}",
                          duration_s=round(time.monotonic() - t_start, 1))
            return result

        print("  IPC connected.")

        while True:
            elapsed = time.monotonic() - t_start
            iteration += 1

            if elapsed > OVERALL_TIMEOUT:
                result["outcome"]      = "crash"
                result["crash_reason"] = f"Overall timeout ({OVERALL_TIMEOUT:.0f}s)"
                break

            # ---- Query state ------------------------------------------------
            state = ipc.query_state()
            if state is None or not state.get("ok"):
                result["outcome"]      = "crash"
                result["crash_reason"] = "Lost IPC connection"
                break

            sim_st  = state.get("state", {}).get("sim", {})
            cas_st  = state.get("state", {}).get("cassette", {})
            mem_st  = state.get("state", {}).get("memory", {})
            cpu_st  = state.get("state", {}).get("cpu", {})

            running = sim_st.get("running", True)
            motor   = cas_st.get("motor", False)
            pos     = cas_st.get("position", 0.0)
            length  = cas_st.get("length",   0.0)
            pc      = cpu_st.get("pc", 0)
            # PORTB ($D301 on XL/XE):
            #   bit 0 = 1: OS+self-test ROM enabled
            #   bit 7 = 0: self-test ROM mapped to $5000-$57FF (1 = RAM there)
            # Defaults to 0xFF (all bits set) if the emulator doesn't report it
            # yet (old build without the memory section in query_state).
            portb   = mem_st.get("portb", 0xFF)

            dbg(f"iter={iteration:04d} elapsed={elapsed:.2f}s motor={motor} "
                f"pos={pos:.3f} pc=${pc:04X} portb=${portb:02X}")

            if not running:
                result["outcome"]      = "crash"
                result["crash_reason"] = "Machine halted (sim.running = false)"
                break

            # ---- PC-based crash detection -----------------------------------

            # XL/XE self-test ROM is mapped to $5000-$57FF only when BOTH:
            #   - PORTB bit 0 = 1  (OS ROM enabled — self-test is part of OS)
            #   - PORTB bit 7 = 0  (self-test ROM banked in at $5000-$57FF)
            # When bit 7 = 1, ordinary RAM is visible at $5000-$57FF and a
            # program loaded from tape may legitimately execute there.
            os_rom_enabled   = bool(portb & 0x01)
            self_test_mapped = not bool(portb & 0x80)  # bit 7: 0=self-test, 1=RAM
            if 0x5000 <= pc <= 0x57FF and os_rom_enabled and self_test_mapped:
                result["outcome"]      = "crash"
                result["crash_reason"] = (
                    f"SELF TEST detected "
                    f"(PC=${pc:04X}, in $5000-$57FF, OS ROM enabled, "
                    f"self-test banked in, PORTB=${portb:02X})"
                )
                break

            # BOOT ERROR handler in the XL OS lives around $C400-$C4FF.
            # Only flag this if the motor never ran (genuine boot failure, not
            # code loaded into $C400 area by the tape).
            if not motor_was_on and elapsed > 10.0 and 0xC400 <= pc <= 0xC4FF:
                result["outcome"]      = "crash"
                result["crash_reason"] = (
                    f"BOOT ERROR detected "
                    f"(PC=${pc:04X}, motor never ran, elapsed {elapsed:.1f}s)"
                )
                break

            # ---- Screenshot -------------------------------------------------
            bmp_name = f"frame_{iteration:04d}_t{pos:07.3f}.bmp"
            bmp_path = str(cas_dir / bmp_name)
            # Must call screenshot() via command(), not send(), so the ack
            # response is consumed and the stream stays synchronised.
            ipc.screenshot(bmp_path)
            result["screenshots"].append({
                "seq":      iteration,
                "file":     bmp_name,
                "tape_pos": round(pos, 3),
                "motor":    motor,
                "pc":       f"${pc:04X}",
                "portb":    f"${portb:02X}",
                "elapsed":  round(elapsed, 1),
            })

            # ---- Frozen-screen detection (needs Pillow) ---------------------
            if len(result["screenshots"]) >= 2:
                prev = str(cas_dir / result["screenshots"][-2]["file"])
                h = _file_hash(prev)
                if h:
                    last_hashes.append(h)
                    if len(last_hashes) > FROZEN_THRESHOLD:
                        last_hashes.pop(0)
                    if (len(last_hashes) == FROZEN_THRESHOLD
                            and len(set(last_hashes)) == 1
                            and not motor_was_on
                            and elapsed > MOTOR_START_TIMEOUT):
                        result["outcome"]      = "crash"
                        result["crash_reason"] = "Frozen screen + motor never started"
                        break

            # ---- OCR (optional) ---------------------------------------------
            if HAS_TESSERACT and len(result["screenshots"]) >= 2:
                prev = str(cas_dir / result["screenshots"][-2]["file"])
                kw = _ocr_for_crash(prev)
                if kw:
                    result["outcome"]      = "crash"
                    result["crash_reason"] = f"OCR detected: {kw}"
                    break

            # ---- Motor state machine ----------------------------------------
            # The Atari OS keeps the motor running across adjacent records when
            # the inter-record gap is short (the motor relay is never de-asserted
            # between them).  We therefore require motor_stop_grace *consecutive*
            # off-polls before declaring success, to avoid false positives during
            # normal multi-record loads.
            if motor:
                motor_was_on    = True
                motor_off_count = 0
                if pos > 0.0:
                    last_motor_pos = pos
            elif motor_was_on:
                motor_off_count += 1
                if motor_off_count == 1:
                    print(f"\n  Motor off at tape {pos:.3f}s "
                          f"(elapsed {elapsed:.1f}s) — "
                          f"need {motor_stop_grace} consecutive off-polls…")
                elif motor_off_count % 5 == 0:
                    print(f"  Still off: {motor_off_count}/{motor_stop_grace} polls",
                          end="\r", flush=True)
                if motor_off_count >= motor_stop_grace:
                    result["outcome"]          = "success"
                    result["motor_stopped_at"] = round(last_motor_pos, 3)
                    break
            elif not motor_was_on and elapsed > MOTOR_START_TIMEOUT:
                result["outcome"]      = "crash"
                result["crash_reason"] = (
                    f"Motor never started in {MOTOR_START_TIMEOUT:.0f}s"
                )
                break

            mc = "\u25b6" if motor else "\u25a0"
            print(f"  [{elapsed:6.1f}s] {mc} {pos:.3f}/{length:.3f}s  "
                  f"iter={iteration:04d}", end="\r", flush=True)

            # Pace to emulator time: block until the emulator has rendered
            # another frames_per_shot frames.  In --warp mode the emulator
            # runs much faster than real time, so we're really pacing to
            # emulated seconds, not wall-clock seconds.
            if not ipc.wait_frames(frames_per_shot, timeout=30.0):
                result["outcome"]      = "crash"
                result["crash_reason"] = "wait_frames timed out (emulator frozen?)"
                break

        print()

    except KeyboardInterrupt:
        result["outcome"]      = "aborted"
        result["crash_reason"] = "Interrupted by user"

    finally:
        result["duration_s"]    = round(time.monotonic() - t_start, 1)
        result["motor_started"] = motor_was_on

        if ipc:
            ipc.close()
        if proc and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()

        if proc:
            sp = f"/tmp/altirra-test-{proc.pid}.sock"
            if os.path.exists(sp):
                try:
                    os.unlink(sp)
                except OSError:
                    pass

    # ---- Video assembly -----------------------------------------------------
    if result["outcome"] in ("success", "crash") and video_fps > 0:
        print(f"  [video] assembling {len(result['screenshots'])} frames…")
        vid = make_video(cas_dir, video_fps)
        if vid:
            result["video"] = os.path.basename(vid)
            print(f"  Video: {vid}")

    # ---- Panel / cleanup ----------------------------------------------------
    if result["outcome"] in ("success", "crash") and panel_opts is not None:
        postprocess_cas_dir(cas_dir, **panel_opts)

    (cas_dir / "result.json").write_text(json.dumps(result, indent=2))
    return result


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    global FRAMES_PER_SHOT, MOTOR_START_TIMEOUT, MOTOR_STOP_GRACE
    global OVERALL_TIMEOUT, VIDEO_FPS, _debug_enabled

    ap = argparse.ArgumentParser(
        description="Batch .cas file tester for Altirra SDL",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--altirra",   help="Path to AltirraSDL (required for testing)")
    ap.add_argument("--output",    required=True, help="Output directory")
    ap.add_argument("--list",      metavar="FILE",
                    help="Text file with one .cas path per line")

    # Emulation tuning
    ap.add_argument("--frames",    type=int, default=FRAMES_PER_SHOT,
                    help=f"Emulator frames/screenshot (default {FRAMES_PER_SHOT} = 1s PAL)")
    ap.add_argument("--motor-timeout", type=float, default=MOTOR_START_TIMEOUT,
                    help=f"Wall-clock seconds to wait for motor (default {MOTOR_START_TIMEOUT})")
    ap.add_argument("--grace",     type=int, default=MOTOR_STOP_GRACE,
                    help=f"Emulator-seconds motor off for success (default {MOTOR_STOP_GRACE})")
    ap.add_argument("--timeout",   type=float, default=OVERALL_TIMEOUT,
                    help=f"Wall-clock hard cap per CAS (default {OVERALL_TIMEOUT})")
    ap.add_argument("--video-fps", type=int, default=VIDEO_FPS,
                    help=f"Output video fps (default {VIDEO_FPS})")
    ap.add_argument("--no-video",  action="store_true", help="Skip ffmpeg")
    ap.add_argument("--ntsc",      action="store_true", help="Force NTSC")
    ap.add_argument("--pal",       action="store_true", help="Force PAL")
    ap.add_argument("--debug",     action="store_true",
                    help="Print every IPC send/recv line (verbose)")

    # Panel / post-processing
    ap.add_argument("--panel-cols", type=int, default=PANEL_COLS,
                    help=f"Images per row in summary panel (default {PANEL_COLS})")
    ap.add_argument("--similarity", type=float, default=PANEL_SIMILARITY,
                    help=f"Cosine similarity threshold 0.0–1.0; higher = more "
                         f"filtering (default {PANEL_SIMILARITY})")
    ap.add_argument("--min-gap",   type=float, default=PANEL_MIN_GAP,
                    help=f"Min seconds between selected panel frames "
                         f"(default {PANEL_MIN_GAP}, 0 = off)")
    ap.add_argument("--panel-width", type=int, default=PANEL_WIDTH,
                    help=f"Final screenshot width in pixels "
                         f"(default {PANEL_WIDTH})")
    ap.add_argument("--keep-source", action="store_true",
                    help="Keep source BMP frames (default: delete after panel)")
    ap.add_argument("--no-panel",  action="store_true",
                    help="Skip panel generation and BMP cleanup entirely")

    # Post-run actions
    ap.add_argument("--move-cas",  action="store_true",
                    help="Move each .cas to a done/ folder after testing")
    ap.add_argument("--postprocess-results", action="store_true",
                    help="Re-run panel generation on existing result dirs "
                         "(no emulator needed)")

    ap.add_argument("cas_files",   nargs="*", help=".cas files to test")
    args = ap.parse_args()

    FRAMES_PER_SHOT     = args.frames
    MOTOR_START_TIMEOUT = args.motor_timeout
    MOTOR_STOP_GRACE    = args.grace
    OVERALL_TIMEOUT     = args.timeout
    VIDEO_FPS           = args.video_fps
    _debug_enabled      = args.debug

    out_dir = Path(args.output)

    # ---- Build panel options dict ------------------------------------------
    panel_opts = None
    if not args.no_panel:
        panel_opts = dict(
            panel_cols  = args.panel_cols,
            similarity  = args.similarity,
            min_gap     = args.min_gap,
            panel_width = args.panel_width,
            keep_source = args.keep_source,
        )

    # ---- Postprocess-only mode ---------------------------------------------
    if args.postprocess_results:
        if not out_dir.is_dir():
            ap.error(f"Output directory does not exist: {out_dir}")
        if panel_opts is None:
            ap.error("--postprocess-results requires panel generation "
                     "(don't combine with --no-panel)")

        subdirs = sorted(d for d in out_dir.iterdir()
                         if d.is_dir() and (d / "result.json").exists())
        print(f"Post-processing {len(subdirs)} result dir(s) in {out_dir}")
        for i, sd in enumerate(subdirs, 1):
            print(f"[{i}/{len(subdirs)}] {sd.name}")
            postprocess_cas_dir(sd, **panel_opts)
        print("Done.")
        return

    # ---- Normal test mode --------------------------------------------------
    if not args.altirra:
        ap.error("--altirra is required for testing "
                 "(use --postprocess-results to re-process existing output)")

    cas_files = list(args.cas_files)
    if args.list:
        with open(args.list) as f:
            cas_files += [l.strip() for l in f
                          if l.strip() and not l.startswith("#")]
    if not cas_files:
        ap.error("No .cas files specified.")

    extra = []
    if args.ntsc:  extra.append("--ntsc")
    elif args.pal: extra.append("--pal")

    vfps = 0 if args.no_video else VIDEO_FPS

    out_dir.mkdir(parents=True, exist_ok=True)

    # done/ sits next to the output dir, e.g. results/ → done/
    done_dir = out_dir.parent / "done"

    print(f"Altirra:     {args.altirra}")
    print(f"Output:      {out_dir.resolve()}")
    print(f"Frames/shot: {FRAMES_PER_SHOT}  Grace: {MOTOR_STOP_GRACE}s  "
          f"Timeout: {OVERALL_TIMEOUT}s")
    print(f"Pillow:      {'yes' if HAS_PILLOW else 'no'}")
    print(f"pytesseract: {'yes' if HAS_TESSERACT else 'no'}")
    print(f"ffmpeg:      {'yes' if HAS_FFMPEG and vfps > 0 else 'no'}")
    print(f"Panel:       {'yes' if panel_opts else 'no'}"
          + (f"  cols={args.panel_cols} sim={args.similarity} "
             f"gap={args.min_gap}s width={args.panel_width}px"
             if panel_opts else ""))
    print(f"Keep source: {'yes' if args.keep_source else 'no'}")
    print(f"Move CAS:    {'yes' if args.move_cas else 'no'}")
    print(f"Debug IPC:   {'yes' if _debug_enabled else 'no'}")
    print(f"Files: {len(cas_files)}")
    print()

    results = []
    for i, cas in enumerate(cas_files, 1):
        cas = os.path.expanduser(cas)
        if not os.path.exists(cas):
            print(f"[{i}/{len(cas_files)}] SKIP {cas}  (not found)")
            results.append({"cas": cas, "outcome": "skipped",
                             "crash_reason": "file not found"})
            continue

        print(f"[{i}/{len(cas_files)}] {cas}")
        r = test_one_cas(args.altirra, cas, out_dir, extra,
                         FRAMES_PER_SHOT, MOTOR_STOP_GRACE, vfps,
                         panel_opts=panel_opts)
        results.append(r)

        icon   = "\u2713" if r["outcome"] == "success" else "\u2717"
        detail = (f"tape stopped at {r['motor_stopped_at']}s"
                  if r["outcome"] == "success"
                  else r.get("crash_reason", ""))
        print(f"  {icon} {r['outcome'].upper()}  {detail}  "
              f"({r['duration_s']:.1f}s, {len(r.get('screenshots',[]))} frames)")

        # ---- Move .cas to done/ -------------------------------------------
        if args.move_cas and r["outcome"] in ("success", "crash"):
            done_dir.mkdir(parents=True, exist_ok=True)
            dest = done_dir / Path(cas).name
            try:
                shutil.move(cas, str(dest))
                print(f"  Moved → {dest}")
            except OSError as e:
                print(f"  [move-cas] failed: {e}")

        print()

    run = {
        "generated": datetime.now().isoformat(),
        "altirra":   args.altirra,
        "total":     len(results),
        "success":   sum(1 for r in results if r["outcome"] == "success"),
        "crash":     sum(1 for r in results if r["outcome"] == "crash"),
        "skipped":   sum(1 for r in results if r["outcome"] == "skipped"),
        "aborted":   sum(1 for r in results if r["outcome"] == "aborted"),
        "results":   results,
    }

    rp = out_dir / "report.json"

    # Load existing report and append this run, or start fresh.
    report = {"runs": []}
    if rp.exists():
        try:
            existing = json.loads(rp.read_text())
            if isinstance(existing, dict) and "runs" in existing:
                report = existing
            else:
                report["runs"].append(existing)
        except (json.JSONDecodeError, OSError):
            pass

    report["runs"].append(run)

    all_results = [r for rn in report["runs"] for r in rn.get("results", [])]
    report["total"]   = len(all_results)
    report["success"] = sum(1 for r in all_results if r.get("outcome") == "success")
    report["crash"]   = sum(1 for r in all_results if r.get("outcome") == "crash")
    report["skipped"] = sum(1 for r in all_results if r.get("outcome") == "skipped")
    report["aborted"] = sum(1 for r in all_results if r.get("outcome") == "aborted")
    report["updated"] = run["generated"]

    rp.write_text(json.dumps(report, indent=2))

    print("=" * 60)
    print(f"  This run:  success={run['success']}  crash={run['crash']}  "
          f"skipped={run['skipped']}  aborted={run['aborted']}")
    print(f"  All runs:  success={report['success']}  crash={report['crash']}  "
          f"skipped={report['skipped']}  aborted={report['aborted']}  "
          f"({len(report['runs'])} run(s))")
    print(f"  Report: {rp}")
    print("=" * 60)


if __name__ == "__main__":
    main()
