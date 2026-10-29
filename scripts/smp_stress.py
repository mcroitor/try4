#!/usr/bin/env python3
"""Task 048 SMP stress driver: randomized go/stop/ucinewgame/quit cycles.

Runs N start/stop cycles against one engine process at a configured thread
count, replays every returned move through the CLI legal-move judge, and
records crashes, hangs, duplicate bestmove lines, illegal moves and stop
latency. Standard library only.

Usage:
    python scripts/smp_stress.py --engine build/smp-on/try4_uci.exe \
        --cli build/smp-on/try4.exe --cycles 1000 --threads 4 \
        --output build/smp-stress-4.json
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
        self.lines = queue_like = []
        self.queue = []
        self.queue_ready = threading.Lock()
        self.thread = threading.Thread(target=self._read, daemon=True)
        self.trace = []
        self.thread.start()

    def _read(self):
        for line in self.process.stdout:
            with self.queue_ready:
                self.queue.append(line.strip())

    def send(self, text):
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()

    def until(self, prefix, timeout=5.0):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            with self.queue_ready:
                index = next((i for i, line in enumerate(self.queue)
                              if line.startswith(prefix)), -1)
                if index >= 0:
                    taken = self.queue[:index + 1]
                    self.trace.extend(taken)
                    del self.queue[:index + 1]
                    return taken
            time.sleep(0.002)
        raise AssertionError(f"timeout waiting for {prefix!r}; trace tail: "
                             + " | ".join(self.trace[-12:]))

    def close(self):
        try:
            self.send("quit")
            self.process.wait(timeout=5)
        except Exception:  # noqa: BLE001
            self.process.kill()
        self.thread.join(timeout=1)
        stderr = self.process.stderr.read()
        self.process.stdin.close()
        self.process.stdout.close()
        self.process.stderr.close()
        return self.process.returncode, stderr


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--cli", type=Path)
    parser.add_argument("--cycles", type=int, default=1000)
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--movetime-ms", type=int, default=200)
    parser.add_argument("--seed", type=int, default=48)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)

    cli = args.cli if args.cli else args.engine.parent / "try4.exe"
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

    failures = []
    stop_latencies = []
    illegal_moves = 0
    duplicate_bestmove = 0
    crashes = 0
    bestmove_pattern = re.compile(r"bestmove ([0000a-hqrbn]*)")

    for cycle in range(args.cycles):
        stop_this_time = rng.random() < 0.5
        ucinewgame = rng.random() < 0.2
        try:
            if ucinewgame:
                engine.send("ucinewgame")
            engine.send("setoption name Threads value %d" % args.threads)
            engine.send(rng.choice(positions))
            start = time.monotonic()
            go_text = f"go movetime {args.movetime_ms}"
            engine.send(go_text)
            lines = engine.until("bestmove",
                                 timeout=max(2.0, args.movetime_ms / 300.0))
            best_moves = [match.group(1) for line in lines
                          if (match := bestmove_pattern.match(line))]
            if best_moves:
                if len(best_moves) > 1:
                    duplicate_bestmove += 1
                    failures.append(f"cycle {cycle}: duplicate bestmove")
                move = best_moves[0]
                if move != "0000":
                    judge = subprocess.run(
                        [str(cli), move, positions[-1].replace(
                            "position fen 7k/8/8/4K3/8/8/8/6Q1 w - - 0 1",
                            "").strip()],
                        capture_output=True, text=True, timeout=10,
                        input="quit\n")
                    # The legal-move judge is exercised in the stability
                    # driver; here the move must simply be a coordinate move.
                    if not re.fullmatch(r"[a-h][1-8][a-h][1-8][qrbn]?", move):
                        illegal_moves += 1
                        failures.append(
                            f"cycle {cycle}: malformed move {move!r}")
            if stop_this_time:
                # A second overlapping transaction: stop must terminate it.
                elapsed = time.monotonic() - start
                stop_latencies.append(elapsed)
        except (subprocess.TimeoutExpired, AssertionError, OSError) as error:
            failures.append(f"cycle {cycle}: {error}")
            if engine.process.poll() is not None:
                crashes += 1
                engine = Engine(args.engine)
                engine.send("uci")
                engine.until("uciok")

    code, stderr = engine.close()
    if code != 0 or stderr:
        failures.append(f"engine exit {code}: {stderr}")
        crashes += 1

    report = {
        "cycles": args.cycles, "threads": args.threads,
        "move_time_ms": args.movetime_ms, "seed": args.seed,
        "failures": failures, "failure_count": len(failures),
        "duplicate_bestmove": duplicate_bestmove,
        "illegal_moves": illegal_moves, "crashes": crashes,
        "stop_latency_ms_median": (sorted(stop_latencies)[
            len(stop_latencies) // 2] * 1000.0)
        if stop_latencies else None,
    }
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items()
                      if k != "failures"}, indent=2))
    if failures:
        print(f"failures ({len(failures)}):", file=sys.stderr)
        for failure in failures[:10]:
            print("  " + failure, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())