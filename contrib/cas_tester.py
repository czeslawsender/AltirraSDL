#!/usr/bin/env python3
"""
cas_tester.py  —  Batch .cas file tester for Altirra SDL (macOS / Linux)
=========================================================================

Launches Altirra in test-mode IPC for each .cas file, boots it, monitors
cassette motor state, takes screenshots every 2 seconds, and decides:

  SUCCESS : tape motor ran, then stopped and did NOT restart for 10 s.
            Records the tape position at which motor stopped.
  CRASH   : BOOT ERROR / SELF TEST detected (OCR or timeout), machine
            halted (sim.running → false), or overall timeout exceeded.

Output layout:
  <out>/
    <cas_stem>/
      frame_0001_t0.000.png
      frame_0002_t1.234.png   ← name encodes wall-time seq + tape position
      ...
      result.json
    report.json                ← summary of all tested files

Requirements:
  Python 3.8+
  Optional: Pillow  (pip install Pillow)     — frozen-screen detection
  Optional: pytesseract + tesseract binary   — OCR crash detection

Usage:
  python3 cas_tester.py --altirra ./AltirraSDL --output ./results file1.cas file2.cas
  python3 cas_tester.py --altirra ./AltirraSDL --output ./results *.cas
  python3 cas_tester.py --altirra ./AltirraSDL --output ./results --list files.txt
"""

import argparse
import hashlib
import json
import os
import shutil
import signal
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

# ---------------------------------------------------------------------------
# Tuning constants
# ---------------------------------------------------------------------------
POLL_INTERVAL        = 2.0    # seconds between query_state + screenshot
MOTOR_START_TIMEOUT  = 90.0   # seconds to wait for motor to first run
MOTOR_STOP_GRACE     = 10.0   # seconds after motor stops before declaring success
OVERALL_TIMEOUT      = 300.0  # hard cap per CAS (5 min)
SOCKET_WAIT_TIMEOUT  = 15.0   # seconds to wait for Altirra's socket to appear
FROZEN_THRESHOLD     = 5      # consecutive identical screenshots → frozen

# Text that indicates a crash on the emulated Atari screen
CRASH_KEYWORDS = ["BOOT ERROR", "SELF TEST", "MEMO PAD", "BOOT ERRO"]

# ---------------------------------------------------------------------------
# IPC controller
# ---------------------------------------------------------------------------
class AltirraIPC:
    def __init__(self, sock_path: str, timeout: float = SOCKET_WAIT_TIMEOUT):
        self._sock = None
        self._buf = ""
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
        raise RuntimeError(f"Timed out waiting for Altirra socket: {sock_path}")

    def send(self, cmd: str):
        self._sock.sendall((cmd + "\n").encode())

    def _recv_line(self, timeout: float = 5.0) -> str | None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                chunk = self._sock.recv(4096)
                if chunk:
                    self._buf += chunk.decode(errors="replace")
                elif chunk == b"":
                    return None          # connection closed
            except BlockingIOError:
                pass
            nl = self._buf.find("\n")
            if nl >= 0:
                line = self._buf[:nl].rstrip("\r")
                self._buf = self._buf[nl + 1:]
                return line
            time.sleep(0.02)
        return None

    def command(self, cmd: str, timeout: float = 5.0) -> dict | None:
        try:
            self.send(cmd)
            line = self._recv_line(timeout)
            if line is None:
                return None
            return json.loads(line)
        except (OSError, json.JSONDecodeError):
            return None

    def query_state(self) -> dict | None:
        return self.command("query_state")

    def screenshot(self, path: str) -> dict | None:
        # Altirra saves async on next rendered frame — no need to wait here,
        # the next poll cycle gives it plenty of time.
        return self.command(f'screenshot "{path}"', timeout=3.0)

    def cold_reset(self) -> dict | None:
        return self.command("cold_reset")

    def close(self):
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None


# ---------------------------------------------------------------------------
# Screenshot helpers
# ---------------------------------------------------------------------------
def _img_hash(path: str) -> str | None:
    """SHA-1 of PNG file bytes — cheap frozen-screen detector."""
    if not HAS_PILLOW or not os.path.exists(path):
        if os.path.exists(path):
            # Fallback: hash the raw file bytes
            return hashlib.sha1(open(path, "rb").read()).hexdigest()
        return None
    try:
        return hashlib.sha1(open(path, "rb").read()).hexdigest()
    except OSError:
        return None


def _ocr_for_crash(path: str) -> str | None:
    """
    Run OCR on a screenshot and return the first matching crash keyword,
    or None. Only called if pytesseract + tesseract are available.
    """
    if not HAS_TESSERACT or not os.path.exists(path):
        return None
    try:
        img = Image.open(path)
        text = pytesseract.image_to_string(img).upper()
        for kw in CRASH_KEYWORDS:
            if kw in text:
                return kw
    except Exception:
        pass
    return None


# ---------------------------------------------------------------------------
# Single CAS test
# ---------------------------------------------------------------------------
def test_one_cas(altirra: str, cas_path: str, out_dir: Path,
                 extra_args: list[str]) -> dict:
    """
    Boot one .cas file, monitor it, return a result dict.
    """
    cas_name = Path(cas_path).stem
    cas_dir = out_dir / cas_name
    cas_dir.mkdir(parents=True, exist_ok=True)

    result = {
        "cas": cas_path,
        "stem": cas_name,
        "outcome": "unknown",
        "motor_stopped_at": None,    # tape position in seconds when motor stopped
        "motor_started": False,
        "crash_reason": None,
        "screenshots": [],
        "duration_s": 0.0,
        "timestamp": datetime.now().isoformat(),
    }

    # Build Altirra command
    # --casautoboot: auto-press Play on boot (no manual CLOAD needed)
    # --nobasic:     disable built-in BASIC (most games need this)
    # --tape:        load the cassette image
    cmd = [
        altirra,
        "--test-mode",
        "--tape", cas_path,
        "--casautoboot",
        "--nobasic",
    ] + extra_args

    proc = None
    ipc  = None
    t_start = time.monotonic()

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        sock_path = f"/tmp/altirra-test-{proc.pid}.sock"
        print(f"  Launched PID {proc.pid}, socket: {sock_path}")

        try:
            ipc = AltirraIPC(sock_path, timeout=SOCKET_WAIT_TIMEOUT)
        except RuntimeError as e:
            result["outcome"]      = "crash"
            result["crash_reason"] = f"IPC socket did not appear: {e}"
            result["duration_s"]   = time.monotonic() - t_start
            return result

        print(f"  IPC connected.")

        # ----------------------------------------------------------------
        # Main monitoring loop
        # ----------------------------------------------------------------
        frame_seq     = 0
        motor_was_on  = False         # ever been on?
        motor_off_at  = None          # monotonic time when motor last stopped
        motor_off_pos = None          # tape position when motor last stopped
        last_hashes   = []            # ring buffer of recent screenshot hashes

        while True:
            elapsed = time.monotonic() - t_start

            if elapsed > OVERALL_TIMEOUT:
                result["outcome"]      = "crash"
                result["crash_reason"] = f"Overall timeout ({OVERALL_TIMEOUT:.0f}s) exceeded"
                break

            # --- Query emulator state ---
            state = ipc.query_state()
            if state is None or not state.get("ok"):
                result["outcome"]      = "crash"
                result["crash_reason"] = "Lost IPC connection"
                break

            sim_state = state.get("state", {}).get("sim", {})
            cas_state = state.get("state", {}).get("cassette", {})

            running      = sim_state.get("running", True)
            motor_on     = cas_state.get("motor", False)
            tape_pos     = cas_state.get("position", 0.0)
            tape_len     = cas_state.get("length", 0.0)

            # --- Screenshot ---
            frame_seq += 1
            png_name = f"frame_{frame_seq:04d}_t{tape_pos:07.3f}.png"
            png_path = str(cas_dir / png_name)
            ipc.screenshot(png_path)

            # Give Altirra one frame (≈16 ms) to write the file,
            # then record it.  If file doesn't appear yet that's OK —
            # we still log the intent.
            time.sleep(0.1)
            result["screenshots"].append({
                "seq":      frame_seq,
                "file":     png_name,
                "tape_pos": round(tape_pos, 3),
                "motor":    motor_on,
                "elapsed":  round(elapsed, 1),
            })

            # --- Frozen-screen detection ---
            h = _img_hash(png_path)
            if h:
                last_hashes.append(h)
                if len(last_hashes) > FROZEN_THRESHOLD:
                    last_hashes.pop(0)
                # If all recent hashes are identical AND motor has never run
                # past the timeout → crash
                if (len(last_hashes) == FROZEN_THRESHOLD
                        and len(set(last_hashes)) == 1
                        and not motor_was_on
                        and elapsed > MOTOR_START_TIMEOUT):
                    result["outcome"]      = "crash"
                    result["crash_reason"] = "Screen frozen + motor never started (BOOT ERROR?)"
                    break

            # --- OCR crash detection (optional) ---
            if HAS_TESSERACT:
                kw = _ocr_for_crash(png_path)
                if kw:
                    result["outcome"]      = "crash"
                    result["crash_reason"] = f"OCR detected: {kw}"
                    break

            # --- Machine halted ---
            if not running:
                result["outcome"]      = "crash"
                result["crash_reason"] = "Emulated machine halted (sim.running = false)"
                break

            # --- Motor state tracking ---
            if motor_on:
                motor_was_on = True
                motor_off_at = None     # reset grace timer while motor runs

            elif motor_was_on and not motor_on:
                # Motor was running and just stopped (or was already stopped)
                if motor_off_at is None:
                    motor_off_at  = time.monotonic()
                    motor_off_pos = tape_pos
                    print(f"  Motor stopped at tape position {tape_pos:.3f}s "
                          f"(elapsed {elapsed:.1f}s) — waiting {MOTOR_STOP_GRACE:.0f}s grace…")

                grace_elapsed = time.monotonic() - motor_off_at
                if grace_elapsed >= MOTOR_STOP_GRACE:
                    # Motor has been off for the full grace period → SUCCESS
                    result["outcome"]        = "success"
                    result["motor_stopped_at"] = round(motor_off_pos, 3)
                    break

            elif not motor_was_on and elapsed > MOTOR_START_TIMEOUT:
                # Motor never started within timeout
                result["outcome"]      = "crash"
                result["crash_reason"] = (
                    f"Motor never started within {MOTOR_START_TIMEOUT:.0f}s "
                    "(BOOT ERROR, SELF TEST, or missing OS?)"
                )
                break

            # Print compact status line
            motor_chr = "▶" if motor_on else "■"
            print(f"  [{elapsed:6.1f}s] {motor_chr} tape={tape_pos:.3f}/{tape_len:.3f}s  "
                  f"frame={frame_seq:04d}", end="\r", flush=True)

            time.sleep(max(0, POLL_INTERVAL - 0.1))  # subtract screenshot write delay

        print()   # clear the \r status line

    except KeyboardInterrupt:
        result["outcome"]      = "aborted"
        result["crash_reason"] = "Interrupted by user"

    finally:
        result["duration_s"] = round(time.monotonic() - t_start, 1)
        result["motor_started"] = motor_was_on  # type: ignore[possibly-undefined]

        if ipc:
            ipc.close()
        if proc and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()

        # Clean up socket file
        sock_path = f"/tmp/altirra-test-{proc.pid}.sock" if proc else None
        if sock_path and os.path.exists(sock_path):
            try:
                os.unlink(sock_path)
            except OSError:
                pass

    # Write per-CAS result JSON
    result_path = cas_dir / "result.json"
    result_path.write_text(json.dumps(result, indent=2))

    return result


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    global MOTOR_START_TIMEOUT, MOTOR_STOP_GRACE, OVERALL_TIMEOUT

    ap = argparse.ArgumentParser(
        description="Batch .cas file tester for Altirra SDL",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--altirra", required=True,
                    help="Path to AltirraSDL executable")
    ap.add_argument("--output", required=True,
                    help="Output directory for screenshots and reports")
    ap.add_argument("--list", metavar="FILE",
                    help="Text file with one .cas path per line")
    ap.add_argument("--motor-timeout", type=float, default=MOTOR_START_TIMEOUT,
                    help=f"Seconds to wait for motor to start (default {MOTOR_START_TIMEOUT})")
    ap.add_argument("--grace", type=float, default=MOTOR_STOP_GRACE,
                    help=f"Seconds after motor stops before declaring success (default {MOTOR_STOP_GRACE})")
    ap.add_argument("--timeout", type=float, default=OVERALL_TIMEOUT,
                    help=f"Hard timeout per CAS in seconds (default {OVERALL_TIMEOUT})")
    ap.add_argument("--ntsc", action="store_true",
                    help="Force NTSC video standard")
    ap.add_argument("--pal", action="store_true",
                    help="Force PAL video standard")
    ap.add_argument("cas_files", nargs="*",
                    help=".cas files to test")
    args = ap.parse_args()

    # Adjust global tuning from args
    MOTOR_START_TIMEOUT = args.motor_timeout
    MOTOR_STOP_GRACE    = args.grace
    OVERALL_TIMEOUT     = args.timeout

    # Collect CAS files
    cas_files = list(args.cas_files)
    if args.list:
        with open(args.list) as f:
            cas_files += [l.strip() for l in f if l.strip() and not l.startswith("#")]
    if not cas_files:
        ap.error("No .cas files specified.")

    # Extra Altirra flags
    extra = []
    if args.ntsc:
        extra.append("--ntsc")
    elif args.pal:
        extra.append("--pal")

    # Output directory
    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Announce capabilities
    print(f"Altirra: {args.altirra}")
    print(f"Output:  {out_dir}")
    print(f"Pillow:      {'yes' if HAS_PILLOW else 'no (frozen-screen detection disabled)'}")
    print(f"pytesseract: {'yes' if HAS_TESSERACT else 'no (OCR crash detection disabled)'}")
    print(f"Files to test: {len(cas_files)}")
    print()

    results = []
    for i, cas in enumerate(cas_files, 1):
        cas = os.path.expanduser(cas)
        if not os.path.exists(cas):
            print(f"[{i}/{len(cas_files)}] SKIP  {cas}  (file not found)")
            results.append({"cas": cas, "outcome": "skipped",
                             "crash_reason": "file not found"})
            continue

        print(f"[{i}/{len(cas_files)}] Testing: {cas}")
        r = test_one_cas(args.altirra, cas, out_dir, extra)
        results.append(r)

        icon = "✓" if r["outcome"] == "success" else "✗"
        detail = (f"tape stopped at {r['motor_stopped_at']}s"
                  if r["outcome"] == "success"
                  else r.get("crash_reason", ""))
        print(f"  {icon} {r['outcome'].upper()}  —  {detail}  "
              f"({r['duration_s']:.1f}s, {len(r.get('screenshots', []))} frames)")
        print()

    # Summary report
    report = {
        "generated": datetime.now().isoformat(),
        "altirra": args.altirra,
        "total": len(results),
        "success": sum(1 for r in results if r["outcome"] == "success"),
        "crash":   sum(1 for r in results if r["outcome"] == "crash"),
        "skipped": sum(1 for r in results if r["outcome"] == "skipped"),
        "aborted": sum(1 for r in results if r["outcome"] == "aborted"),
        "results": results,
    }
    report_path = out_dir / "report.json"
    report_path.write_text(json.dumps(report, indent=2))

    print("=" * 60)
    print(f"Results:  {report['success']} success / {report['crash']} crash / "
          f"{report['skipped']} skipped / {report['aborted']} aborted")
    print(f"Report:   {report_path}")
    print("=" * 60)


if __name__ == "__main__":
    main()
