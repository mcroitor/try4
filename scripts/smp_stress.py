#!/usr/bin/env python3
"""Task 048 SMP stress driver: randomized go/stop/ucinewgame/quit cycles.

Runs N start/stop cycles against one engine process at a configured thread
count, validates every returned move syntactically, and records crashes,
hangs, duplicate bestmove lines, illegal moves and real stop latency
(time from `stop` to `bestmove` on cycles that stop early). Standard library
only; the full legal-move judge lives in scripts/uci_stability.py.

Usage:
    python scripts/smp_stress.py --engine build/smp-on/try4_uci.exe \
        --cycles 1000 --threads 4 --output build/smp-stress-4.json
"""
import argparse
import json
import random
import re
import subprocess
import sys
import threading
import time
from pathlib import Path


class Engine:
    def __init__(self, executable):
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1)
        self.queue = []
        self.lock = threading.Lock()
        self.thread = threading.Thread(target=self._read, daemon=True)
        self.trace = []
        self.thread.start()

    def _read(self):
        for line in self.process.stdout:
            with self.lock:
                self.queue.append(line.rstrip("\n"))

    def send(self, text):
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()

    def until(self, prefix, timeout=5.0):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            with self.lock:
                index = next((i for i, line in enumerate(self.queue)
                              if line.startswith(prefix)), -1)
                if index >= 0:
                    taken = self.queue[:index + 1]
                    self.trace.extend(taken)
                    del self.queue[:index + 1]
                    return taken
            time.sleep(0.002)
        raise AssertionError("timeout waiting for %r; trace tail: %s"
                             % (prefix, " | ".join(self.trace[-12:])))

    def close(self):
        try:
            self.send("quit")
            self.process.wait(timeout=5)
        except Exception:  # noqa: BLE001
            self.process.kill()
            self.process.wait(timeout=2)
        self.thread.join(timeout=1)
        stderr = self.process.stderr.read()
        self.process.stdin.close()
        self.process.stdout.close()
        self.process.stderr.close()
        return self.process.returncode, stderr


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--cycles", type=int, default=1000)
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--movetime-ms", type=int, default=300)
    parser.add_argument("--seed", type=int, default=48)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)

    rng = random.Random(args.seed)
    positions = [
        "position startpos",
        "position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/"
        "R3K2R w KQkq - 0 1",
        "position fen 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "position fen 7k/8/8/4K3/8/8/8/6Q1 w - - 0 1",
    ]

    engine = Engine(args.engine)
    engine.send("uci")
    engine.until("uciok")
    engine.send("isready")
    engine.until("readyok")
    engine.send("setoption name Threads value %d" % args.threads)

    failures = []
    stop_latencies = []
    illegal_moves = 0
    duplicate_bestmove = 0
    crashes = 0
    bestmove_pattern = re.compile(r"^bestmove (\S*)$")

    for cycle in range(args.cycles):
        stop_early = rng.random() < 0.5
        use_newgame = rng.random() < 0.2
        position_text = rng.choice(positions)
        try:
            if use_newgame:
                engine.send("ucinewgame")
            engine.send(position_text)
            engine.send("go movetime %d" % args.movetime_ms)
            if stop_early:
                time.sleep(rng.uniform(0.02, 0.25) * args.movetime_ms / 300.0)
                stop_start = time.monotonic()
                engine.send("stop")
                lines = engine.until("bestmove", timeout=5.0)
                stop_latencies.append(time.monotonic() - stop_start)
            else:
                lines = engine.until(
                    "bestmove", timeout=max(2.0, args.movetime_ms / 200.0))
            best_moves = [m.group(1) for line in lines
                          if (m := bestmove_pattern.match(line))]
            if len(best_moves) > 1:
                duplicate_bestmove += 1
                failures.append("cycle %d: %d bestmove lines"
                                % (cycle, len(best_moves)))
            if best_moves:
                move = best_moves[0]
                if move != "0000" and not re.fullmatch(
                        r"[a-h][1-8][a-h][1-8][qrbn]?", move):
                    illegal_moves += 1
                    failures.append("cycle %d: malformed move %r"
                                    % (cycle, move))
        except (subprocess.TimeoutExpired, AssertionError, OSError) as error:
            failures.append("cycle %d: %s" % (cycle, error))
            if engine.process.poll() is not None:
                crashes += 1
                engine = Engine(args.engine)
                engine.send("uci")
                engine.until("uciok")

    code, stderr = engine.close()
    if code != 0 or stderr:
        failures.append("engine exit %s: %s" % (code, stderr))
        crashes += 1

    def median(values):
        return sorted(values)[len(values) // 2] if values else None

    report = {
        "cycles": args.cycles, "threads": args.threads,
        "move_time_ms": args.movetime_ms, "seed": args.seed,
        "failure_count": len(failures),
        "duplicate_bestmove": duplicate_bestmove,
        "illegal_moves": illegal_moves, "crashes": crashes,
        "stop_latency_ms_median": (round(median(stop_latencies) * 1000.0, 2)
                                   if stop_latencies else None),
        "stop_latency_ms_max": (round(max(stop_latencies) * 1000.0, 2)
                                if stop_latencies else None),
        "stopped_cycles": len(stop_latencies),
    }
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items()
                      if k != "failures"}, indent=2))
    if failures:
        print("failures (%d):" % len(failures), file=sys.stderr)
        for failure in failures[:10]:
            print("  " + failure, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())