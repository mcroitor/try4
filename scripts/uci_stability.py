#!/usr/bin/env python3
"""Protocol-level UCI stability run: real processes, legal-move validation.

This is a stability harness, not a strength measurement. It plays complete games
between two instances of the same UCI executable, validates every engine move by
replaying it through the repository CLI (whose move generation is covered by the
perft suite), records per-move wall time and clock usage, and probes
infinite/stop/isready latency. Failures are counted and archived; they are never
silently skipped.

The default control is 1000 ms + 50 ms increment with ``movestogo 200``. That
asks the engine for roughly 30 ms per move, so a 120-ply game stays well inside
the clock while still exercising the real clock fields. Use ``--limit-games``
for a smoke run.
"""

import argparse
import concurrent.futures
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import queue
import random
import re
import subprocess
import sys
import threading
import time

SENTINEL = None


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round(fraction * (len(ordered) - 1)))))
    return round(ordered[index], 3)


def stats(values):
    if not values:
        return {"count": 0}
    return {
        "count": len(values),
        "min": round(min(values), 3),
        "p50": percentile(values, 0.5),
        "p95": percentile(values, 0.95),
        "p99": percentile(values, 0.99),
        "max": round(max(values), 3),
        "mean": round(sum(values) / len(values), 3),
    }


class ProtocolError(RuntimeError):
    pass


class Engine:
    """One UCI process with a reader thread and deadline-aware waits."""

    def __init__(self, executable, arguments=(), cwd=None):
        self.executable = str(executable)
        self.arguments = [str(argument) for argument in arguments]
        self.cwd = str(cwd) if cwd is not None else None
        self.process = None
        self.lines = queue.Queue()
        self.reader = None
        self.stderr_text = ""
        self.start()

    def start(self):
        self.close_quietly()
        self.lines = queue.Queue()
        self.stderr_text = ""
        self.process = subprocess.Popen(
            [self.executable, *self.arguments],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
            cwd=self.cwd,
        )

        def read():
            try:
                for line in self.process.stdout:
                    self.lines.put((line.strip(), time.perf_counter()))
            finally:
                self.lines.put(SENTINEL)

        def read_error():
            try:
                self.stderr_text = self.process.stderr.read()
            except Exception:  # pragma: no cover - defensive teardown path
                self.stderr_text = ""

        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        self.error_reader = threading.Thread(target=read_error, daemon=True)
        self.error_reader.start()

    def send(self, text):
        if self.process is None or self.process.poll() is not None:
            raise ProtocolError("engine exited before command: " + text)
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()

    def wait_for(self, prefix, timeout, started=None):
        deadline = time.perf_counter() + timeout
        while True:
            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                raise TimeoutError(f"timed out after {timeout:.1f}s waiting for {prefix}")
            try:
                item = self.lines.get(timeout=remaining)
            except queue.Empty:
                raise TimeoutError(f"timed out after {timeout:.1f}s waiting for {prefix}")
            if item is SENTINEL:
                code = self.process.poll()
                raise ProtocolError(f"engine exited ({code}) while waiting for {prefix}: "
                                    f"{self.stderr_text.strip()}")
            line, stamp = item
            if line.startswith(prefix):
                return line, stamp

    def drain(self):
        collected = []
        while True:
            try:
                item = self.lines.get_nowait()
            except queue.Empty:
                return collected
            if item is SENTINEL:
                return collected
            collected.append(item[0])

    def close_quietly(self, timeout=5):
        process = self.process
        if process is None:
            return
        try:
            if process.poll() is None:
                try:
                    process.stdin.write("quit\n")
                    process.stdin.flush()
                except OSError:
                    pass
                try:
                    process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        finally:
            for stream in (process.stdin, process.stdout, process.stderr):
                try:
                    stream.close()
                except Exception:
                    pass
            if self.reader is not None:
                self.reader.join(timeout=2)
            if getattr(self, "error_reader", None) is not None:
                self.error_reader.join(timeout=2)
            self.process = None

    @property
    def alive(self):
        return self.process is not None and self.process.poll() is None


class CliJudge:
    """Replays games through the repository CLI and returns SAN/result data."""

    def __init__(self, executable):
        self.engine = Engine(executable)
        self.engine.send("startpos")
        self.engine.wait_for("ok", 5)

    def reset(self):
        self.engine.send("startpos")
        self.engine.wait_for("ok", 5)

    def apply(self, uci):
        self.engine.send("move " + uci)
        deadline = time.perf_counter() + 5
        while True:
            item = self.engine.lines.get(timeout=max(0, deadline - time.perf_counter()))
            if item is SENTINEL:
                raise ProtocolError("CLI exited while applying " + uci)
            line, _ = item
            if line.startswith("ok ") or line.startswith("error:"):
                return line

    def result(self):
        self.engine.send("result")
        deadline = time.perf_counter() + 5
        while True:
            item = self.engine.lines.get(timeout=max(0, deadline - time.perf_counter()))
            if item is SENTINEL:
                raise ProtocolError("CLI exited while reading result")
            line, _ = item
            if line in {"ongoing", "checkmate", "stalemate", "fifty-move",
                        "threefold-repetition", "insufficient-material"}:
                return line

    def close(self):
        self.engine.close_quietly()


class FailureCounter:
    def __init__(self, lock):
        self.lock = lock
        self.counts = {}

    def add(self, name, detail):
        with self.lock:
            self.counts.setdefault(name, []).append(detail)


def parse_openings(path):
    openings = []
    for number, raw in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), 1):
        text = raw.strip()
        if not text or text.startswith("#"):
            continue
        parts = text.split()
        if len(parts) < 2:
            raise RuntimeError(f"opening line {number} has no moves: {raw!r}")
        openings.append({"id": parts[0], "moves": parts[1:]})
    if not openings:
        raise RuntimeError("opening file contains no openings")
    return openings


def pgn_header(game, version):
    return "\n".join([
        '[Event "try4 UCI stability"]',
        '[Site "local"]',
        f'[Date "{game["date"]}"]',
        f'[Round "{game["id"]}"]',
        f'[White "{game["white"]}"]',
        f'[Black "{game["black"]}"]',
        f'[Result "{game["result"]}"]',
        f'[Opening "{game["opening"]}"]',
        f'[Termination "{game["termination"]}"]',
        f'[Engine "try4 {version}"]',
        f'[TimeControl "{game["time_control"]}"]',
        "",
        game["movetext"] + " " + game["result"],
        "",
    ])


def movetext(san_moves):
    parts = []
    for index, san in enumerate(san_moves):
        if index % 2 == 0:
            if index:
                parts.append("")
            parts.append(f"{index // 2 + 1}.")
        parts.append(san)
    return " ".join(parts)


class Worker:
    def __init__(self, engine_path, cli_path, arguments, games, openings,
                 options, failures, lock, stop_probe_every):
        self.engine_path = engine_path
        self.cli_path = cli_path
        self.arguments = arguments
        self.games = games
        self.openings = openings
        self.options = options
        self.failures = failures
        self.lock = lock
        self.stop_probe_every = stop_probe_every
        self.engines = [Engine(engine_path, arguments), Engine(engine_path, arguments)]
        self.judge = CliJudge(cli_path)
        self.version = None
        self.records = []
        self.stop_latencies = []
        self.ready_latencies = []

    def initialize(self):
        for engine in self.engines:
            engine.send("uci")
            line, _ = engine.wait_for("uciok", 10)
            del line
            engine.drain()
            engine.send("setoption name OwnBook value false")
            engine.send("setoption name Threads value 1")
            engine.send("isready")
            engine.wait_for("readyok", 5)
        for engine in self.engines:
            engine.drain()
        version = subprocess.check_output([str(self.engine_path), "--version"],
                                          text=True).strip()
        if not version.startswith("try4 "):
            raise RuntimeError("unexpected --version output: " + version)
        self.version = version

    def restart_engine(self, index, reason):
        self.failures.add("engine_restart", {"reason": reason, "side": index})
        self.engines[index].close_quietly()
        self.engines[index] = Engine(self.engine_path, self.arguments)
        engine = self.engines[index]
        engine.send("uci")
        engine.wait_for("uciok", 10)
        engine.drain()
        engine.send("setoption name OwnBook value false")
        engine.send("setoption name Threads value 1")
        engine.send("isready")
        engine.wait_for("readyok", 5)
        engine.drain()

    def play(self, game_id, opening, white_index):
        options = self.options
        white_engine = self.engines[white_index]
        black_engine = self.engines[1 - white_index]
        game = {
            "id": game_id,
            "date": datetime.date.today().isoformat(),
            "opening": opening["id"],
            "white": f"try4-{'a' if white_index == 0 else 'b'}",
            "black": f"try4-{'b' if white_index == 0 else 'a'}",
            "time_control": f"{options.base_ms}+{options.inc_ms}/movestogo {options.moves_to_go}",
            "result": "1/2-1/2",
            "termination": "max-plies",
            "plies": 0,
            "san_moves": [],
            "uci_moves": [],
            "move_wall_ms": [],
            "white_clock_after_ms": [],
            "black_clock_after_ms": [],
            "illegal": None,
            "protocol_error": None,
            "max_move_ms": 0.0,
        }
        clocks = {0: options.base_ms, 1: options.base_ms}
        errors = []

        if not white_engine.alive or not black_engine.alive:
            errors.append("engine not alive at game start")
            game["protocol_error"] = "; ".join(errors)
            return game

        for engine in (white_engine, black_engine):
            try:
                engine.send("ucinewgame")
                engine.send("isready")
                ready_start = time.perf_counter()
                engine.wait_for("readyok", 5)
                self.ready_latencies.append((time.perf_counter() - ready_start) * 1000)
            except (TimeoutError, ProtocolError, OSError) as error:
                errors.append("newgame/isready: " + str(error))
                game["protocol_error"] = "; ".join(errors)
                return game

        try:
            self.judge.reset()
            for uci in opening["moves"]:
                line = self.judge.apply(uci)
                if not line.startswith("ok "):
                    raise ProtocolError(f"opening move {uci} rejected: {line}")
                parts = line.split(maxsplit=2)
                game["san_moves"].append(parts[2])
                game["uci_moves"].append(uci)
            game["plies"] = len(game["uci_moves"])
        except Exception as error:  # noqa: BLE001 - archive every failure
            errors.append("opening: " + str(error))
            game["protocol_error"] = "; ".join(errors)
            return game

        while game["plies"] < options.max_plies:
            white_to_move = game["plies"] % 2 == 0
            side = 0 if white_to_move else 1
            engine = white_engine if white_to_move else black_engine
            if not engine.alive:
                errors.append(f"engine side {side} exited before move {game['plies']}")
                break

            position = "position startpos"
            if game["uci_moves"]:
                position += " moves " + " ".join(game["uci_moves"])
            command = (f"{position}\ngo wtime {max(0, int(round(clocks[0])))} "
                       f"btime {max(0, int(round(clocks[1])))} "
                       f"winc {options.inc_ms} binc {options.inc_ms} "
                       f"movestogo {options.moves_to_go}")
            state = {"bestmove": None, "infos": [], "unsupported": [],
                     "resigned": False}
            start = time.perf_counter()
            try:
                engine.send(command)
                while state["bestmove"] is None:
                    line, _ = engine.wait_for("", options.timeout_ms / 1000.0)
                    if line.startswith("info string Unsupported") or \
                            line.startswith("info string Invalid"):
                        state["unsupported"].append(line)
                    elif line.startswith("info string resign"):
                        # Task 058: a resignation termination is protocol,
                        # not a protocol failure; the engine stops playing.
                        state["resigned"] = True
                    elif line.startswith("bestmove "):
                        state["bestmove"] = line.split()[1] if len(line.split()) > 1 else ""
                    elif line.startswith("info "):
                        state["infos"].append(line)
                elapsed_ms = (time.perf_counter() - start) * 1000.0
            except (TimeoutError, ProtocolError, OSError) as error:
                elapsed_ms = (time.perf_counter() - start) * 1000.0
                errors.append(f"move {game['plies']} ({'white' if white_to_move else 'black'}): {error}")
                self.failures.add("hang_or_exit", {"game": game_id, "ply": game["plies"],
                                                   "error": str(error)})
                break

            game["move_wall_ms"].append(round(elapsed_ms, 3))
            game["max_move_ms"] = max(game["max_move_ms"], round(elapsed_ms, 3))
            if state["unsupported"]:
                errors.append("; ".join(state["unsupported"]))
                self.failures.add("unsupported_command", {"game": game_id,
                                                          "ply": game["plies"],
                                                          "lines": state["unsupported"]})
            bestmove = state["bestmove"]
            if bestmove == "0000" and state["resigned"]:
                # Task 058: acceptable resignation termination; the game ends
                # with the resigned side losing.
                game["result"] = "1-0" if not white_to_move else "0-1"
                game["termination"] = "resignation"
                self.failures.add("resignation", {"game": game_id,
                                                  "ply": game["plies"]})
                break
            if not bestmove or bestmove == "0000":
                errors.append(f"invalid bestmove at ply {game['plies']}: {bestmove!r}")
                break
            if not re.fullmatch(r"[a-h][1-8][a-h][1-8][qrbn]?", bestmove):
                errors.append(f"malformed bestmove at ply {game['plies']}: {bestmove!r}")
                break

            try:
                line = self.judge.apply(bestmove)
            except Exception as error:  # noqa: BLE001
                errors.append(f"judge replay failed at ply {game['plies']}: {error}")
                break
            if not line.startswith("ok "):
                errors.append(f"illegal move {bestmove} at ply {game['plies']}: {line}")
                self.failures.add("illegal_move", {"game": game_id, "ply": game["plies"],
                                                   "move": bestmove, "response": line,
                                                   "moves": list(game["uci_moves"])})
                break

            parts = line.split(maxsplit=2)
            game["san_moves"].append(parts[2])
            game["uci_moves"].append(bestmove)
            game["plies"] += 1
            clocks[side] -= elapsed_ms
            clocks[side] += options.inc_ms
            game["white_clock_after_ms"].append(round(clocks[0], 3))
            game["black_clock_after_ms"].append(round(clocks[1], 3))
            if clocks[side] <= 0:
                errors.append(f"time forfeit by {'white' if white_to_move else 'black'}: "
                              f"clock {clocks[side]:.1f} ms")
                self.failures.add("time_forfeit", {"game": game_id, "ply": game["plies"],
                                                   "clock_ms": round(clocks[side], 3)})
                game["result"] = "1-0" if not white_to_move else "0-1"
                game["termination"] = "time-forfeit"
                break

            if elapsed_ms > options.forfeit_ms:
                errors.append(f"per-move deadline exceeded at ply {game['plies']}: "
                              f"{elapsed_ms:.1f} ms")
                self.failures.add("deadline_exceeded", {"game": game_id, "ply": game["plies"],
                                                        "elapsed_ms": round(elapsed_ms, 3)})

            try:
                result = self.judge.result()
            except Exception as error:  # noqa: BLE001
                errors.append(f"judge result failed at ply {game['plies']}: {error}")
                break
            if result != "ongoing":
                game["termination"] = result
                if result == "checkmate":
                    game["result"] = "0-1" if white_to_move else "1-0"
                else:
                    game["result"] = "1/2-1/2"
                break

        if errors:
            game["protocol_error"] = "; ".join(errors)
        game["movetext"] = movetext(game["san_moves"])
        return game

    def stop_probe(self, game_id):
        engine = self.engines[game_id % 2]
        try:
            engine.send("position startpos")
            engine.send("go infinite")
            time.sleep(0.02)
            ready_start = time.perf_counter()
            engine.send("isready")
            engine.wait_for("readyok", 5)
            self.ready_latencies.append((time.perf_counter() - ready_start) * 1000)
            stop_start = time.perf_counter()
            engine.send("stop")
            engine.wait_for("bestmove ", 5)
            latency = (time.perf_counter() - stop_start) * 1000
            self.stop_latencies.append(latency)
            trailing = engine.drain()
            for line in trailing:
                if line.startswith("bestmove "):
                    self.failures.add("duplicate_bestmove", {"game": game_id, "probe": True})
        except (TimeoutError, ProtocolError, OSError) as error:
            self.failures.add("stop_probe", {"game": game_id, "error": str(error)})

    def run(self):
        self.initialize()
        for game_id, opening, white_index in self.games:
            if game_id % self.stop_probe_every == 0:
                self.stop_probe(game_id)
            try:
                record = self.play(game_id, opening, white_index)
            except Exception as error:  # noqa: BLE001 - one bad game must not abort the run
                self.failures.add("game_exception", {"game": game_id, "error": str(error)})
                record = {"id": game_id, "opening": opening["id"],
                          "result": "1/2-1/2", "termination": "harness-error",
                          "plies": 0, "movetext": "", "white": "try4", "black": "try4",
                          "date": datetime.date.today().isoformat(),
                          "time_control": f"{self.options.base_ms}+{self.options.inc_ms}",
                          "protocol_error": str(error), "max_move_ms": 0.0}
            self.records.append(record)
            if not self.engines[0].alive:
                self.restart_engine(0, f"after game {game_id}")
            if not self.engines[1].alive:
                self.restart_engine(1, f"after game {game_id}")
        return self.records, self.stop_latencies, self.ready_latencies, self.version

    def close(self):
        for engine in self.engines:
            engine.close_quietly()
        self.judge.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine", required=True, type=Path, help="try4_uci executable")
    parser.add_argument("--cli", type=Path, help="legal-move judge; default is sibling try4")
    parser.add_argument("--openings", type=Path,
                        default=Path(__file__).resolve().parents[1] / "docs/selfplay/openings.txt")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--games", type=int, default=1000)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--base-ms", type=int, default=1000)
    parser.add_argument("--inc-ms", type=int, default=50)
    parser.add_argument("--moves-to-go", type=int, default=200)
    parser.add_argument("--max-plies", type=int, default=120)
    parser.add_argument("--timeout-ms", type=int, default=5000,
                        help="hard harness wait for one bestmove")
    parser.add_argument("--forfeit-ms", type=int, default=1000,
                        help="record a per-move deadline violation above this")
    parser.add_argument("--stop-probe-every", type=int, default=25)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--hardware", default="unspecified")
    parser.add_argument("--label", default="stability")
    args = parser.parse_args()

    engine = args.engine.resolve()
    cli = (args.cli or engine.with_name(engine.name.replace("_uci", ""))).resolve()
    if not engine.exists():
        raise SystemExit(f"engine not found: {engine}")
    if not cli.exists():
        raise SystemExit(f"CLI judge not found: {cli}")
    if args.games <= 0 or args.workers <= 0 or args.max_plies <= 0:
        raise SystemExit("games, workers and max-plies must be positive")

    openings = parse_openings(args.openings)
    version = subprocess.check_output([str(engine), "--version"], text=True).strip()
    randomizer = random.Random(args.seed)
    order = list(range(len(openings)))
    randomizer.shuffle(order)

    assignments = [[] for _ in range(args.workers)]
    for game_id in range(1, args.games + 1):
        opening = openings[order[(game_id - 1) % len(order)]]
        white_index = (game_id // 2) % 2
        assignments[(game_id - 1) % args.workers].append((game_id, opening, white_index))

    lock = threading.Lock()
    failures = FailureCounter(lock)
    common = dict(engine_path=engine, cli_path=cli, arguments=(),
                  openings=openings, options=args, failures=failures, lock=lock,
                  stop_probe_every=max(1, args.stop_probe_every))
    workers = [Worker(games=assignment, **common) for assignment in assignments]

    started = time.perf_counter()
    records = []
    stop_latencies = []
    ready_latencies = []
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
            futures = [pool.submit(worker.run) for worker in workers]
            for future in concurrent.futures.as_completed(futures):
                games, stops, readies, _ = future.result()
                records.extend(games)
                stop_latencies.extend(stops)
                ready_latencies.extend(readies)
    finally:
        for worker in workers:
            worker.close()
    elapsed = time.perf_counter() - started
    records.sort(key=lambda item: item["id"])

    counts = {"1-0": 0, "0-1": 0, "1/2-1/2": 0}
    terminations = {}
    total_plies = 0
    move_times = []
    for record in records:
        counts[record["result"]] = counts.get(record["result"], 0) + 1
        terminations[record["termination"]] = terminations.get(record["termination"], 0) + 1
        total_plies += record["plies"]
        move_times.extend(record.get("move_wall_ms", []))

    failure_summary = {name: len(items) for name, items in failures.counts.items()}
    report = {
        "label": args.label,
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "engine": str(engine),
        "engine_version": version,
        "cli_judge": str(cli),
        "platform": platform.platform(),
        "processor": platform.processor(),
        "hardware": args.hardware,
        "python": sys.version,
        "command": sys.argv,
        "control": {
            "base_ms": args.base_ms,
            "increment_ms": args.inc_ms,
            "moves_to_go": args.moves_to_go,
            "max_plies": args.max_plies,
            "workers": args.workers,
            "seed": args.seed,
            "opening_file": str(args.openings.resolve()),
            "opening_sha256": hashlib.sha256(args.openings.read_bytes()).hexdigest(),
            "move_deadline_violation_ms": args.forfeit_ms,
            "note": "Fixed clock control; identical engines. Stability only, not strength.",
        },
        "totals": {
            "games": len(records),
            "requested_games": args.games,
            "elapsed_s": round(elapsed, 3),
            "games_per_minute": round(len(records) / elapsed * 60, 2) if elapsed else None,
            "result_counts": counts,
            "terminations": terminations,
            "total_plies": total_plies,
            "mean_plies": round(total_plies / len(records), 2) if records else None,
            "protocol_failures": sum(failure_summary.values()),
            "failure_counts": failure_summary,
        },
        "move_wall_ms": stats(move_times),
        "stop_latency_ms": stats(stop_latencies),
        "ready_latency_ms": stats(ready_latencies),
        "failures": failures.counts,
        "games": [
            {
                key: record.get(key) for key in
                ("id", "opening", "white", "black", "result", "termination", "plies",
                 "max_move_ms", "protocol_error")
            }
            for record in records
        ],
    }

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "stability.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    with (args.output / "games.csv").open("w", encoding="utf-8", newline="") as handle:
        handle.write("game,opening,white,black,result,termination,plies,max_move_ms,protocol_error,moves\n")
        for record in records:
            moves = " ".join(record.get("uci_moves", []))
            error = (record.get("protocol_error") or "").replace(",", ";")
            handle.write(f"{record['id']},{record['opening']},{record['white']},"
                         f"{record['black']},{record['result']},{record['termination']},"
                         f"{record['plies']},{record.get('max_move_ms', 0)},{error},{moves}\n")
    with (args.output / "games.pgn").open("w", encoding="utf-8") as handle:
        for record in records:
            handle.write(pgn_header(record, version))
            handle.write("\n")
    (args.output / "summary.txt").write_text(
        f"try4 UCI stability {args.label}\n"
        f"engine: {version}\n"
        f"games: {len(records)}/{args.games} in {elapsed:.1f}s\n"
        f"results W/D/L (white perspective): {counts.get('1-0', 0)}/"
        f"{counts.get('1/2-1/2', 0)}/{counts.get('0-1', 0)}\n"
        f"terminations: {terminations}\n"
        f"protocol failures: {sum(failure_summary.values())} {failure_summary}\n"
        f"move wall ms: {stats(move_times)}\n"
        f"stop latency ms: {stats(stop_latencies)}\n"
        f"ready latency ms: {stats(ready_latencies)}\n",
        encoding="utf-8")

    print(f"games: {len(records)}/{args.games}, {elapsed:.1f}s, "
          f"results {counts}, failures {failure_summary}")
    print(f"move wall ms: {stats(move_times)}")
    print(f"stop latency ms: {stats(stop_latencies)}")
    print(f"reports: {args.output}")
    return 1 if failures.counts else 0


if __name__ == "__main__":
    sys.exit(main())
