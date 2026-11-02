#!/usr/bin/env python3
"""Cross-check try4's Syzygy root probing against an independent prober.

Requires ``python-chess`` (``pip install python-chess``) and a local Syzygy
directory. The script generates random three-piece KxKy/KyP positions, asks
python-chess for the WDL result, asks a real try4 UCI process for its root
tablebase result, and validates the returned move. It can additionally compare
the WDL sign with try4's own deep search on KQvK/KRvK positions.

This is a development/verification harness, not part of CTest.
"""

import argparse
import datetime
import json
import queue
import random
import subprocess
import sys
import threading
import time
from pathlib import Path

import chess
import chess.syzygy


WDL_NAMES = {
    -2: "loss",
    -1: "blessed-loss",
    0: "draw",
    1: "cursed-win",
    2: "win",
}
NAME_WDL = {name: value for value, name in WDL_NAMES.items()}


class UciEngine:
    def __init__(self, executable, syzygy=None):
        self.process = subprocess.Popen(
            [str(executable)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        self.lines = queue.Queue()
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()
        self.send("uci")
        self.until("uciok")
        if syzygy:
            self.send(f"setoption name SyzygyPath value {syzygy}")
        self.send("isready")
        self.until("readyok")

    def _read(self):
        assert self.process.stdout is not None
        for line in self.process.stdout:
            self.lines.put(line.rstrip("\n"))
        self.lines.put(None)

    def send(self, command):
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def until(self, prefix, timeout=10.0):
        deadline = time.monotonic() + timeout
        collected = []
        while time.monotonic() < deadline:
            try:
                line = self.lines.get(timeout=max(0.01, deadline - time.monotonic()))
            except queue.Empty:
                break
            if line is None:
                raise RuntimeError("engine exited while waiting for " + prefix)
            collected.append(line)
            if line.startswith(prefix):
                return collected
        raise TimeoutError(f"timeout waiting for {prefix!r}: {collected[-5:]}")

    def close(self):
        if self.process.poll() is None:
            try:
                self.send("quit")
                self.process.wait(timeout=5)
            except Exception:
                self.process.kill()


def random_position(rng, kind):
    for _ in range(10000):
        board = chess.Board(None)
        squares = list(range(64))
        rng.shuffle(squares)
        wk, bk, extra = squares[:3]
        board.set_piece_at(wk, chess.Piece(chess.KING, chess.WHITE))
        board.set_piece_at(bk, chess.Piece(chess.KING, chess.BLACK))
        if kind == "KQvK":
            board.set_piece_at(extra, chess.Piece(chess.QUEEN, chess.WHITE))
        elif kind == "KRvK":
            board.set_piece_at(extra, chess.Piece(chess.ROOK, chess.WHITE))
        elif kind == "KBvK":
            board.set_piece_at(extra, chess.Piece(chess.BISHOP, chess.WHITE))
        elif kind == "KNvK":
            board.set_piece_at(extra, chess.Piece(chess.KNIGHT, chess.WHITE))
        elif kind == "KPvK":
            rank = rng.randrange(1, 7)
            file = rng.randrange(8)
            board.set_piece_at(chess.square(file, rank),
                               chess.Piece(chess.PAWN, chess.WHITE))
        else:
            raise ValueError(kind)
        board.turn = rng.choice([chess.WHITE, chess.BLACK])
        board.castling_rights = 0
        board.ep_square = None
        board.halfmove_clock = 0
        board.fullmove_number = 1
        if board.status() != chess.STATUS_VALID:
            continue
        if board.is_game_over():
            continue
        return board
    raise RuntimeError("could not generate a valid position for " + kind)


def parse_root(lines):
    wdl = None
    dtz = None
    bestmove = None
    for line in lines:
        if line.startswith("info string Syzygy WDL "):
            parts = line.split()
            wdl = NAME_WDL.get(parts[4])
            dtz = int(parts[6])
        elif line.startswith("bestmove "):
            bestmove = line.split()[1]
    return wdl, dtz, bestmove


def parse_search_score(lines):
    score = None
    for line in lines:
        if not line.startswith("info depth "):
            continue
        parts = line.split()
        for index, token in enumerate(parts):
            if token == "score" and index + 2 < len(parts):
                kind = parts[index + 1]
                value = int(parts[index + 2])
                score = value if kind == "mate" else value
    return score


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--syzygy", required=True, type=Path)
    parser.add_argument("--positions", type=int, default=200)
    parser.add_argument("--deep-check", type=int, default=60,
                        help="KQvK/KRvK positions checked with depth-8 search")
    parser.add_argument("--seed", type=int, default=35)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    kinds = ["KQvK", "KRvK", "KPvK"]
    tablebase = chess.syzygy.open_tablebase(str(args.syzygy))
    engine = UciEngine(args.engine, args.syzygy)
    records = []
    mismatches = []
    try:
        for index in range(args.positions):
            kind = kinds[index % len(kinds)]
            board = random_position(rng, kind)
            fen = board.fen()
            expected = tablebase.probe_wdl(board)
            engine.send("position fen " + fen)
            engine.send("go depth 1")
            lines = engine.until("bestmove ")
            actual, dtz, bestmove = parse_root(lines)
            legal = False
            move_preserves = None
            if bestmove and bestmove != "0000":
                try:
                    move = board.parse_uci(bestmove)
                    legal = board.is_legal(move)
                    if legal:
                        board.push(move)
                        if not board.is_game_over():
                            after = tablebase.probe_wdl(board)
                            move_preserves = (after == -expected)
                        board.pop()
                except ValueError:
                    legal = False
            record = {
                "kind": kind,
                "fen": fen,
                "expected": expected,
                "actual": actual,
                "dtz": dtz,
                "bestmove": bestmove,
                "legal": legal,
                "move_preserves": move_preserves,
            }
            records.append(record)
            if actual != expected or not legal or move_preserves is False:
                mismatches.append(record)
    finally:
        engine.close()
        tablebase.close()

    deep_records = []
    deep_mismatches = []
    if args.deep_check > 0:
        deep_engine = UciEngine(args.engine, None)
        deep_tablebase = chess.syzygy.open_tablebase(str(args.syzygy))
        try:
            attempts = 0
            while len(deep_records) < args.deep_check and attempts < args.deep_check * 20:
                attempts += 1
                kind = "KQvK" if attempts % 2 == 0 else "KRvK"
                board = random_position(rng, kind)
                expected = deep_tablebase.probe_wdl(board)
                # Drawn KQvK/KRvK positions usually mean the extra piece is
                # hanging; a shallow static search may not see the draw yet, so
                # they are not a useful sign check for this harness.
                if expected == 0:
                    continue
                deep_engine.send("position fen " + board.fen())
                deep_engine.send("go depth 8")
                lines = deep_engine.until("bestmove ")
                score = parse_search_score(lines)
                expected_sign = (expected > 0) - (expected < 0)
                actual_sign = 0 if score is None else (score > 0) - (score < 0)
                record = {
                    "kind": kind,
                    "fen": board.fen(),
                    "wdl": expected,
                    "search_score": score,
                }
                deep_records.append(record)
                if expected_sign != actual_sign:
                    deep_mismatches.append(record)
        finally:
            deep_tablebase.close()
            deep_engine.close()

    report = {
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "engine": str(args.engine),
        "syzygy": str(args.syzygy),
        "seed": args.seed,
        "positions": len(records),
        "mismatches": len(mismatches),
        "deep_positions": len(deep_records),
        "deep_mismatches": len(deep_mismatches),
        "records": records,
        "deep_records": deep_records,
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"root probes: {len(records)}, mismatches: {len(mismatches)}")
    print(f"deep checks: {len(deep_records)}, sign mismatches: {len(deep_mismatches)}")
    if mismatches:
        print("first root mismatch:", mismatches[0])
    if deep_mismatches:
        print("first deep mismatch:", deep_mismatches[0])
    return 1 if mismatches or deep_mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
