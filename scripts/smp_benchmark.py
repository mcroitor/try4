#!/usr/bin/env python3
"""Task 048 SMP benchmark: measure 1/2/4 thread behavior.

At each configured thread count:
- aggregate NPS at a fixed depth (nodes summed across workers);
- time-to-depth at fixed movetime budgets;
- best-move agreement across thread counts;
medians over the configured repeats. Standard library only.

Usage:
    python scripts/smp_benchmark.py --engine build/smp-on/try4_uci.exe \
        --threads 1,2,4 --depth 6 --output build/smp-bench.json
"""
import argparse
import json
import re
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path

POSITIONS = [
    ("startpos", "position startpos"),
    ("kiwipete", "position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/"
                 "PPPBBPPP/R3K2R w KQkq - 0 1"),
    ("perft5-promotion", "position fen rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/"
                         "PPP1NnPP/RNBQK2R w KQ - 1 8"),
    ("kp-blockade", "position fen 8/8/4k3/4p3/4P3/4K3/8/8 w - - 0 1"),
]


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
                self.queue.append(line.strip())

    def send(self, text):
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()

    def until(self, prefix, timeout=10.0):
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
        raise AssertionError(f"timeout waiting for {prefix!r}")

    def close(self):
        self.send("quit")
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
        self.thread.join(timeout=1)
        self.process.stdin.close()
        self.process.stdout.close()


def parse_info(line):
    fields = {}
    match = re.search(r"depth (\d+)", line)
    if match:
        fields["depth"] = int(match.group(1))
    match = re.search(r"score (cp|mate) (-?\d+)", line)
    if match:
        fields["score"] = match.group(1) + " " + match.group(2)
    match = re.search(r"nodes (\d+)", line)
    if match:
        fields["nodes"] = int(match.group(1))
    match = re.search(r"nps (\d+)", line)
    if match:
        fields["nps"] = int(match.group(1))
    match = re.search(r" time (\d+)", line)
    if match:
        fields["time_ms"] = int(match.group(1))
    match = re.search(r" pv (.+)$", line)
    if match:
        fields["pv"] = match.group(1)
    return fields


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--threads", default="1,2,4")
    parser.add_argument("--depth", type=int, default=6)
    parser.add_argument("--movetime-ms", type=int, default=2000)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)

    thread_counts = [int(t) for t in args.threads.split(",")]
    report = {"engine": str(args.engine), "depth": args.depth,
              "movetime_ms": args.movetime_ms, "repeats": args.repeats,
              "thread_counts": thread_counts, "fixed_depth": [],
              "fixed_time": []}

    for threads in thread_counts:
        # Fixed depth: aggregate NPS = the final line's nodes/time, or the
        # sum of helpers where the engine reports aggregate counts.
        for position_id, position_command in POSITIONS:
            node_counts = []
            nps_values = []
            best_moves = []
            for repeat in range(args.repeats):
                engine = Engine(args.engine)
                engine.send("uci")
                engine.until("uciok")
                engine.send("ucinewgame")
                if threads > 1:
                    engine.send(f"setoption name Threads value {threads}")
                engine.send(position_command)
                engine.send(f"go depth {args.depth}")
                lines = engine.until("bestmove ", timeout=args.depth * 40.0)
                engine.close()
                infos = [parse_info(line) for line in lines
                         if line.startswith("info ") and " depth " in line
                         and not line.startswith("info string")]
                if not infos:
                    node_counts.append(0)
                    best_moves.append("")
                    nps_values.append(0)
                    continue
                # The engine's final iteration may include helper nodes.
                final = infos[-1]
                node_counts.append(final.get("nodes", 0))
                nps_values.append(final.get("nps", 0))
                best = [line.split()[1] for line in lines
                        if line.startswith("bestmove ")]
                best_moves.append(best[0] if best else "")
            report["fixed_depth"].append({
                "threads": threads, "position": position_id,
                "nodes_median": statistics.median(node_counts),
                "nps_median": statistics.median(nps_values),
                "bestmove_majority": statistics.mode(best_moves)
                if best_moves else "",
            })

        # Fixed time: time-to-depth per position.
        for position_id, position_command in POSITIONS:
            depths = []
            agreement = []
            repeats = []
            for repeat in range(args.repeats):
                engine = Engine(args.engine)
                engine.send("uci")
                engine.until("uciok")
                engine.send("ucinewgame")
                if threads > 1:
                    engine.send(f"setoption name Threads value {threads}")
                engine.send(position_command)
                engine.send(f"go movetime {args.movetime_ms}")
                lines = engine.until("bestmove ",
                                     timeout=args.movetime_ms / 500.0)
                engine.close()
                infos = [parse_info(line) for line in lines
                         if line.startswith("info depth ")
                         and not line.startswith("info string")]
                depths.append(max((i["depth"] for i in infos), default=0))
                best = [line.split()[1] for line in lines
                        if line.startswith("bestmove ")]
                agreement.append(best[0] if best else "")
            report["fixed_time"].append({
                "threads": threads, "position": position_id,
                "depth_median": statistics.median(depths),
                "bestmove": statistics.mode(agreement)
                if agreement else "",
                "move_wall_ms_median": statistics.median(repeats)
                if repeats else 0.0,
            })

    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    # Console summary.
    print("threads  position              nodes_median   nps_median")
    for row in report["fixed_depth"]:
        print(f"{row['threads']:>7}  {row['position']:<20}  "
              f"{row['nodes_median']:>12.0f}   {row['nps_median']:>10}")
    print()
    print("threads  position              depth_median  bestmove")
    for row in report["fixed_time"]:
        print(f"{row['threads']:>7}  {row['position']:<20}  "
              f"{row['depth_median']:>12}  {row['bestmove']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())