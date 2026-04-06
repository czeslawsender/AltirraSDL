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

After each CAS run, optionally stitches screenshots into a video via ffmpeg.

Output layout:
  <out>/
    <cas_stem>/
      frame_0001_t0.000.bmp
      frame_0002_t1.000.bmp
      ...
      result.json
      video.mp4          (if ffmpeg available)
    report.json

Requirements:
  Python 3.8+
  Optional: Pillow      (pip install Pillow)     — frozen-screen detection
  Optional: pytesseract (pip install pytesseract) — OCR crash detection
  Optional: ffmpeg in PATH                        — video assembly

Notes:
  - Screenshots are BMP (SDL_SaveBMP). Extension .bmp is used explicitly.
  - Timing is emulator-frame-based via wait_frames IPC command.
    --frames 50 = 1 PAL second, --frames 60 = 1 NTSC second.
  - Altirra is launched with --warp and --novsync automatically.
"""

import argparse
import hashlib
import json
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

CRASH_KEYWORDS = ["BOOT ERROR", "SELF TEST", "MEMO PAD", "BOOT ERRO"]

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
                    return
                except OSError:
                    pass
            time.sleep(0.2)
        raise RuntimeError(f"Timed out waiting for socket: {sock_path}")

    def send(self, cmd: str):
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
                return line
            time.sleep(0.02)
        return None

    def command(self, cmd: str, timeout: float = 5.0):
        try:
            self.send(cmd)
            line = self._recv_line(timeout)
            if line is None:
                return None
            return json.loads(line)
        except (OSError, json.JSONDecodeError):
            return None

    def query_state(self):
        return self.command("query_state")

    def screenshot(self, path: str):
        self.send(f'screenshot "{path}"')

    def wait_frames(self, n: int, timeout: float = 30.0) -> bool:
        resp = self.command(f"wait_frames {n}", timeout=timeout)
        return resp is not None and resp.get("ok", False)

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
# ffmpeg video assembly
# ---------------------------------------------------------------------------
def make_video(cas_dir: Path, fps: int):
    if not HAS_FFMPEG or fps <= 0:
        return None

    bmps = sorted(cas_dir.glob("frame_*.bmp"))
    if not bmps:
        return None

    concat = cas_dir / "_ffmpeg_list.txt"
    duration = 1.0 / fps
    with open(concat, "w") as f:
        for p in bmps:
            f.write(f"file '{p.name}'\n")
            f.write(f"duration {duration:.6f}\n")

    out = cas_dir / "video.mp4"
    r = subprocess.run(
        ["ffmpeg", "-y", "-f", "concat", "-safe", "0",
         "-i", str(concat),
         "-c:v", "libx264", "-pix_fmt", "yuv420p",
         "-loglevel", "error",
         str(out)],
        cwd=str(cas_dir),
        capture_output=True,
    )
    concat.unlink(missing_ok=True)
    return str(out) if r.returncode == 0 and out.exists() else None


# ---------------------------------------------------------------------------
# Single CAS test
# ---------------------------------------------------------------------------
def test_one_cas(altirra, cas_path, out_dir, extra_args,
                 frames_per_shot, motor_stop_grace, video_fps):

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
        "--tape",   cas_path,
        "--casautoboot",
        "--nobasic",
    ] + extra_args

    proc = None
    ipc  = None
    t_start = time.monotonic()
    motor_was_on    = False
    motor_off_count = 0    # consecutive polls with motor off
    last_hashes    = []
    iteration      = 0

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

            # Query state
            state = ipc.query_state()
            if state is None or not state.get("ok"):
                result["outcome"]      = "crash"
                result["crash_reason"] = "Lost IPC connection"
                break

            sim_st  = state.get("state", {}).get("sim", {})
            cas_st  = state.get("state", {}).get("cassette", {})
            running = sim_st.get("running", True)
            motor   = cas_st.get("motor", False)
            pos     = cas_st.get("position", 0.0)
            length  = cas_st.get("length",   0.0)

            if not running:
                result["outcome"]      = "crash"
                result["crash_reason"] = "Machine halted (sim.running = false)"
                break

            # PC-based crash detection
            cpu_st = state.get("state", {}).get("cpu", {})
            pc = cpu_st.get("pc", 0)
            # Atari XL/XE self-test ROM maps to $5000-$57FF.
            # If PC lands there, the boot failed and self-test took over.
            if 0x5000 <= pc <= 0x57FF:
                result["outcome"]      = "crash"
                result["crash_reason"] = f"SELF TEST detected (PC=${pc:04X}, in $5000-$57FF)"
                break
            # BOOT ERROR display loop lives in $C400-$C4FF in the XL OS.
            # If the motor never started and PC is stuck there, it is a boot error.
            if not motor_was_on and elapsed > 10.0 and 0xC400 <= pc <= 0xC4FF:
                result["outcome"]      = "crash"
                result["crash_reason"] = f"BOOT ERROR detected (PC=${pc:04X}, motor never ran)"
                break

            # Screenshot (always BMP regardless of extension)
            bmp_name = f"frame_{iteration:04d}_t{pos:07.3f}.bmp"
            bmp_path = str(cas_dir / bmp_name)
            ipc.screenshot(bmp_path)
            result["screenshots"].append({
                "seq":      iteration,
                "file":     bmp_name,
                "tape_pos": round(pos, 3),
                "motor":    motor,
                "elapsed":  round(elapsed, 1),
            })

            # Frozen-screen detection (check file from previous iteration)
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

            # OCR (optional)
            if HAS_TESSERACT and len(result["screenshots"]) >= 2:
                prev = str(cas_dir / result["screenshots"][-2]["file"])
                kw = _ocr_for_crash(prev)
                if kw:
                    result["outcome"]      = "crash"
                    result["crash_reason"] = f"OCR detected: {kw}"
                    break

            # Motor state machine — consecutive off-polls required
            if motor:
                # Motor running: reset consecutive-off counter
                motor_was_on    = True
                motor_off_count = 0
            elif motor_was_on:
                # Motor was on, now off — count consecutive off-polls
                motor_off_count += 1
                if motor_off_count == 1:
                    print(f"\n  Motor off at tape {pos:.3f}s "
                          f"(elapsed {elapsed:.1f}s) — "
                          f"need {motor_stop_grace} consecutive off-polls…")
                elif motor_off_count % 5 == 0:
                    print(f"  Still off: {motor_off_count}/{motor_stop_grace} polls", end="\r", flush=True)
                if motor_off_count >= motor_stop_grace:
                    result["outcome"]          = "success"
                    result["motor_stopped_at"] = round(pos, 3)
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

            # Wait N emulator frames (pacing by emulator time, not wall clock)
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
        result["motor_started"] = motor_was_on  # type: ignore

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

    # Video assembly
    if result["outcome"] in ("success", "crash") and video_fps > 0:
        vid = make_video(cas_dir, video_fps)
        if vid:
            result["video"] = os.path.basename(vid)
            print(f"  Video: {vid}")

    (cas_dir / "result.json").write_text(json.dumps(result, indent=2))
    return result


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    global FRAMES_PER_SHOT, MOTOR_START_TIMEOUT, MOTOR_STOP_GRACE
    global OVERALL_TIMEOUT, VIDEO_FPS

    ap = argparse.ArgumentParser(
        description="Batch .cas file tester for Altirra SDL",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--altirra",   required=True, help="Path to AltirraSDL")
    ap.add_argument("--output",    required=True, help="Output directory")
    ap.add_argument("--list",      metavar="FILE",
                    help="Text file with one .cas path per line")
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
    ap.add_argument("cas_files",   nargs="*", help=".cas files to test")
    args = ap.parse_args()

    FRAMES_PER_SHOT     = args.frames
    MOTOR_START_TIMEOUT = args.motor_timeout
    MOTOR_STOP_GRACE    = args.grace
    OVERALL_TIMEOUT     = args.timeout
    VIDEO_FPS           = args.video_fps

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

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"Altirra:     {args.altirra}")
    print(f"Output:      {out_dir}")
    print(f"Frames/shot: {FRAMES_PER_SHOT}  Grace: {MOTOR_STOP_GRACE}s  Timeout: {OVERALL_TIMEOUT}s")
    print(f"Pillow:      {'yes' if HAS_PILLOW else 'no'}")
    print(f"pytesseract: {'yes' if HAS_TESSERACT else 'no'}")
    print(f"ffmpeg:      {'yes' if HAS_FFMPEG and vfps > 0 else 'no'}")
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
                         FRAMES_PER_SHOT, MOTOR_STOP_GRACE, vfps)
        results.append(r)

        icon   = "\u2713" if r["outcome"] == "success" else "\u2717"
        detail = (f"tape stopped at {r['motor_stopped_at']}s"
                  if r["outcome"] == "success"
                  else r.get("crash_reason", ""))
        print(f"  {icon} {r['outcome'].upper()}  {detail}  "
              f"({r['duration_s']:.1f}s, {len(r.get('screenshots',[]))} frames)")
        print()

    report = {
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
    rp.write_text(json.dumps(report, indent=2))

    print("=" * 60)
    print(f"  success={report['success']}  crash={report['crash']}  "
          f"skipped={report['skipped']}  aborted={report['aborted']}")
    print(f"  Report: {rp}")
    print("=" * 60)


if __name__ == "__main__":
    main()
