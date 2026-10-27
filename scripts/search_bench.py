#!/usr/bin/env python3
"""Search benchmark driver for try4 (task 049).

Runs fixed-depth and fixed-time searches over a versioned position set, records
depth/score/nodes/NPS/time/PV, validates every returned move through the
debugging CLI, and writes JSON, CSV and Markdown reports.

The runner is intentionally a manual Release gate, not a CI test: hosted runner
timing is too noisy to gate pull requests.

Example:
    python scripts/search_bench.py --engine build/bench-release/try4_uci.exe \
      --cli build/bench-release/try4.exe --depths 1,2,3,4,5,6 \
      --movetime-ms 2000 --repeats 3 --output build/bench-baseline
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import platform
import queue
import re
import statistics
import subprocess
import sys
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Dict, Iterable, List, Optional

SCRIPT_VERSION = 1
REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_POSITIONS = REPO_ROOT / "tests" / "data" / "bench" / "search.tsv"
DEFAULT_DEPTHS = "1,2,3,4,5,6,7"

INT_TOKEN = re.compile(r"^-?\d+$")


class BenchmarkError(RuntimeError):
    """A recoverable benchmark failure (timeout, bad protocol, illegal move)."""


class EngineTimeout(BenchmarkError):
    """The engine did not answer within the configured timeout."""


def pin_to_cpu(pid: int, cpu: int) -> None:
    """Pin a process to one logical CPU to reduce hybrid-core NPS noise."""
    total = os.cpu_count() or 0
    if total and cpu >= total:
        raise BenchmarkError(f"--cpu {cpu} is outside 0..{total - 1}")
    if sys.platform.startswith("win"):
        import ctypes

        process_query_information = 0x0400
        process_set_information = 0x0200
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.OpenProcess(
            process_query_information | process_set_information, False, pid
        )
        if not handle:
            raise BenchmarkError(f"OpenProcess failed while pinning to CPU {cpu}")
        try:
            if not kernel32.SetProcessAffinityMask(handle, 1 << cpu):
                raise BenchmarkError(
                    f"SetProcessAffinityMask failed while pinning to CPU {cpu}"
                )
        finally:
            kernel32.CloseHandle(handle)
    elif hasattr(os, "sched_setaffinity"):
        os.sched_setaffinity(pid, {cpu})
    else:
        raise BenchmarkError("CPU affinity is not supported on this platform")


def set_process_priority(pid: int, priority: str) -> None:
    if priority == "normal":
        return
    if sys.platform.startswith("win"):
        import ctypes

        process_set_information = 0x0200
        high_priority_class = 0x0080
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.OpenProcess(process_set_information, False, pid)
        if not handle:
            raise BenchmarkError(
                f"OpenProcess failed while setting priority {priority}"
            )
        try:
            if not kernel32.SetPriorityClass(handle, high_priority_class):
                raise BenchmarkError(
                    f"SetPriorityClass failed while setting priority {priority}"
                )
        finally:
            kernel32.CloseHandle(handle)
    elif hasattr(os, "setpriority"):
        try:
            os.setpriority(os.PRIO_PROCESS, pid, -5)
        except OSError as error:
            raise BenchmarkError(f"setpriority failed: {error}") from error
    else:
        raise BenchmarkError("process priority is not supported on this platform")


class Engine:
    """Minimal line-oriented UCI/CLI process wrapper with a reader thread."""

    def __init__(
        self, path: Path, timeout_ms: int, cpu: int = -1, priority: str = "normal"
    ) -> None:
        self.path = path
        self.timeout_ms = timeout_ms
        self._lines: "queue.Queue[Optional[str]]" = queue.Queue()
        self._process = subprocess.Popen(
            [str(path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        if cpu >= 0:
            pin_to_cpu(self._process.pid, cpu)
        set_process_priority(self._process.pid, priority)
        self._reader = threading.Thread(target=self._pump, daemon=True)
        self._reader.start()

    def _pump(self) -> None:
        assert self._process.stdout is not None
        try:
            for line in self._process.stdout:
                self._lines.put(line.rstrip("\r\n"))
        finally:
            self._lines.put(None)

    def send(self, command: str) -> None:
        if self._process.poll() is not None:
            raise BenchmarkError(
                f"engine exited with code {self._process.returncode} before "
                f"command: {command}"
            )
        assert self._process.stdin is not None
        self._process.stdin.write(command + "\n")
        self._process.stdin.flush()

    def read_until(
        self,
        predicate: Callable[[str], bool],
        timeout_ms: Optional[int] = None,
        deadline: Optional[float] = None,
        collect: Optional[List[str]] = None,
    ) -> str:
        limit_ms = timeout_ms if timeout_ms is not None else self.timeout_ms
        if deadline is None:
            deadline = time.monotonic() + limit_ms / 1000.0
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise EngineTimeout(
                    f"timeout after {limit_ms} ms waiting for engine output"
                )
            try:
                line = self._lines.get(timeout=remaining)
            except queue.Empty:
                raise EngineTimeout(
                    f"timeout after {limit_ms} ms waiting for engine output"
                ) from None
            if line is None:
                raise BenchmarkError(
                    f"engine exited with code {self._process.poll()}"
                )
            if collect is not None:
                collect.append(line)
            if predicate(line):
                return line

    def close(self) -> None:
        try:
            if self._process.poll() is None:
                self.send("quit")
                self._process.wait(timeout=5)
        except Exception:
            pass
        finally:
            if self._process.poll() is None:
                self._process.kill()
                self._process.wait(timeout=5)


def engine_version(path: Path) -> str:
    try:
        completed = subprocess.run(
            [str(path), "--version"],
            capture_output=True,
            text=True,
            timeout=10,
        )
    except Exception:
        return "unknown"
    text = (completed.stdout + completed.stderr).strip()
    return text.splitlines()[0] if text else "unknown"


def read_cmake_cache(start: Path) -> Dict[str, str]:
    """Find a CMake cache near the executable and return the keys we report."""
    wanted = ["CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER", "CMAKE_CXX_COMPILER_ID"]
    directory = start
    for _ in range(3):
        cache = directory / "CMakeCache.txt"
        if cache.is_file():
            values: Dict[str, str] = {}
            for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
                for key in wanted:
                    prefix = key + ":"
                    if line.startswith(prefix):
                        values[key] = line.split("=", 1)[1]
            return values
        if directory.parent == directory:
            break
        directory = directory.parent
    return {}


def compiler_description(cache: Dict[str, str]) -> str:
    """Return '<compiler name> <first version line>' when it is resolvable."""
    compiler = cache.get("CMAKE_CXX_COMPILER", "")
    if not compiler:
        return "unknown"
    name = Path(compiler).name
    try:
        completed = subprocess.run(
            [compiler, "--version"],
            capture_output=True,
            text=True,
            timeout=10,
        )
    except Exception:
        return name
    text = (completed.stdout + completed.stderr).strip()
    if not text:
        return name
    version_line = text.splitlines()[0].strip()
    if version_line.startswith(name):
        return version_line
    return f"{name} ({version_line})"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_positions(path: Path) -> List[Dict[str, str]]:
    positions: List[Dict[str, str]] = []
    seen = set()
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = raw.split("\t")
        if len(fields) != 3:
            raise BenchmarkError(
                f"{path}:{number}: expected id<TAB>kind<TAB>fen, got {len(fields)} fields"
            )
        position_id, kind, fen = (field.strip() for field in fields)
        if not position_id or not kind or not fen:
            raise BenchmarkError(f"{path}:{number}: empty id, kind or FEN")
        if position_id in seen:
            raise BenchmarkError(f"{path}:{number}: duplicate id {position_id}")
        seen.add(position_id)
        positions.append({"id": position_id, "kind": kind, "fen": fen})
    if not positions:
        raise BenchmarkError(f"{path}: no positions")
    return positions


def parse_depths(text: str) -> List[int]:
    depths: List[int] = []
    for token in text.split(","):
        token = token.strip()
        if not token:
            continue
        if not INT_TOKEN.match(token) or int(token) <= 0:
            raise BenchmarkError(f"invalid depth: {token!r}")
        depths.append(int(token))
    if not depths:
        raise BenchmarkError("at least one depth is required")
    return sorted(set(depths))


def parse_info(line: str) -> Optional[Dict[str, object]]:
    tokens = line.split()
    if len(tokens) < 3 or tokens[0] != "info":
        return None
    info: Dict[str, object] = {}
    index = 1
    while index < len(tokens):
        token = tokens[index]
        if token == "pv":
            info["pv"] = tokens[index + 1 :]
            break
        if token == "score" and index + 2 < len(tokens):
            info["score_type"] = tokens[index + 1]
            try:
                info["score_value"] = int(tokens[index + 2])
            except ValueError:
                info["score_value"] = 0
            index += 3
            continue
        if index + 1 < len(tokens):
            value = tokens[index + 1]
            if INT_TOKEN.match(value):
                info[token] = int(value)
                index += 2
                continue
        index += 1
    if "depth" not in info:
        return None
    return info


def run_search(
    engine: Engine,
    fen: str,
    depth: Optional[int],
    movetime_ms: Optional[int],
    timeout_ms: int,
) -> Dict[str, object]:
    deadline = time.monotonic() + timeout_ms / 1000.0
    engine.send("ucinewgame")
    engine.send("isready")
    engine.read_until(
        lambda line: line.strip() == "readyok", timeout_ms, deadline=deadline
    )
    engine.send(f"position fen {fen}")
    if depth is not None:
        engine.send(f"go depth {depth}")
        limit = {"type": "depth", "value": depth}
    else:
        assert movetime_ms is not None
        engine.send(f"go movetime {movetime_ms}")
        limit = {"type": "movetime", "value": movetime_ms}

    started = time.perf_counter()
    info: Optional[Dict[str, object]] = None
    bestmove: Optional[str] = None
    while True:
        line = engine.read_until(
            lambda candidate: candidate.startswith("info ")
            or candidate.startswith("bestmove"),
            timeout_ms,
            deadline=deadline,
        )
        if line.startswith("info "):
            parsed = parse_info(line)
            if parsed is not None:
                info = parsed
            continue
        tokens = line.split()
        bestmove = tokens[1] if len(tokens) > 1 else None
        break
    wall_ms = int(round((time.perf_counter() - started) * 1000))

    if info is None:
        raise BenchmarkError(f"no info line before bestmove (unexpected output)")
    if not bestmove or bestmove == "0000":
        raise BenchmarkError(f"search returned no legal move in position {fen}")

    nodes = int(info.get("nodes", 0))
    engine_nps = int(info.get("nps", 0))
    engine_ms = int(info.get("time", 0))
    # Very fast searches report time 0 and therefore nps 0. Fall back to the
    # wall clock so shallow benchmark rows still show a usable rate.
    ms = engine_ms if engine_ms > 0 else wall_ms
    nps = engine_nps if engine_nps > 0 else int(round(nodes * 1000 / max(1, wall_ms)))

    return {
        "limit": limit,
        "depth": int(info.get("depth", 0)),
        "score_type": str(info.get("score_type", "cp")),
        "score_value": int(info.get("score_value", 0)),
        "nodes": nodes,
        "nps": nps,
        "nps_engine": engine_nps,
        "ms": ms,
        "wall_ms": wall_ms,
        "pv": [str(move) for move in info.get("pv", [])],
        "bestmove": bestmove,
    }


class Judge:
    """Legality oracle backed by the debugging CLI (fen + move commands)."""

    def __init__(self, path: Path, timeout_ms: int = 10000) -> None:
        self._engine = Engine(path, timeout_ms)
        self._cache: Dict[tuple, bool] = {}

    def legal(self, fen: str, move: str) -> bool:
        key = (fen, move)
        cached = self._cache.get(key)
        if cached is not None:
            return cached
        self._engine.send(f"fen {fen}")
        response = self._engine.read_until(
            lambda line: line.strip() == "ok" or line.startswith("error")
        )
        if response.startswith("error"):
            raise BenchmarkError(f"judge rejected FEN {fen}: {response}")
        self._engine.send(f"move {move}")
        response = self._engine.read_until(
            lambda line: line.startswith("ok ") or line.startswith("error")
        )
        result = response.startswith("ok ")
        self._cache[key] = result
        return result

    def close(self) -> None:
        self._engine.close()


def median(values: Iterable[float]) -> float:
    return float(statistics.median(list(values)))


def summarize_runs(runs: List[Dict[str, object]]) -> Dict[str, object]:
    """Group runs and compute medians, spread and determinism evidence."""
    groups: Dict[tuple, List[Dict[str, object]]] = {}
    for run in runs:
        limit = run["limit"]
        key = (run["position"], limit["type"], limit["value"])  # type: ignore[index]
        groups.setdefault(key, []).append(run)

    summary: Dict[str, object] = {"groups": [], "failures": []}
    for (position, limit_type, value), items in sorted(groups.items()):
        nodes = [int(item["nodes"]) for item in items]
        nps = [int(item["nps"]) for item in items]
        ms = [int(item["ms"]) for item in items]
        depths = [int(item["depth"]) for item in items]
        pvs = [tuple(item["pv"]) for item in items]
        bestmoves = [str(item["bestmove"]) for item in items]
        nodes_identical = len(set(nodes)) == 1
        pvs_identical = len(set(pvs)) == 1
        moves_identical = len(set(bestmoves)) == 1
        median_nps = median(nps)
        spread = (
            (max(nps) - min(nps)) / median_nps * 100 if median_nps > 0 else 0.0
        )
        summary["groups"].append(
            {
                "position": position,
                "kind": items[0]["kind"],
                "limit_type": limit_type,
                "limit_value": value,
                "runs": len(items),
                "depth": int(median(depths)),
                "nodes": nodes,
                "nodes_median": int(median(nodes)),
                "ms_median": median(ms),
                "nps_min": min(nps),
                "nps_median": median_nps,
                "nps_max": max(nps),
                "nps_spread_percent": round(spread, 2),
                "nodes_identical": nodes_identical,
                "pvs_identical": pvs_identical,
                "moves_identical": moves_identical,
                "deterministic": nodes_identical
                and pvs_identical
                and moves_identical,
                "bestmove": items[0]["bestmove"],
                "score_type": items[0]["score_type"],
                "score_value": items[0]["score_value"],
                "pv": items[0]["pv"],
            }
        )
        if limit_type == "depth" and not (
            nodes_identical and pvs_identical and moves_identical
        ):
            summary["failures"].append(
                f"{position} depth {value}: repeats are not deterministic "
                f"(nodes={nodes}, pv_identical={pvs_identical}, "
                f"bestmove_identical={moves_identical})"
            )
    return summary


def run_to_record(run: Dict[str, object], build_type: str) -> Dict[str, object]:
    limit = run["limit"]
    return {
        "position": run["position"],
        "kind": run["kind"],
        "build_type": build_type,
        "limit": {"type": limit["type"], "value": limit["value"]},  # type: ignore[index]
        "depth": run["depth"],
        "score": f"{run['score_type']} {run['score_value']}",
        "score_type": run["score_type"],
        "score_value": run["score_value"],
        "nodes": run["nodes"],
        "nps": run["nps"],
        "ms": run["ms"],
        "wall_ms": run["wall_ms"],
        "pv": run["pv"],
        "bestmove": run["bestmove"],
        "legal": run["legal"],
    }


def write_outputs(
    output: Path,
    meta: Dict[str, object],
    runs: List[Dict[str, object]],
    summary: Dict[str, object],
) -> None:
    output.mkdir(parents=True, exist_ok=True)

    (output / "bench.json").write_text(
        json.dumps(
            {
                "meta": meta,
                "runs": [
                    run_to_record(run, str(meta.get("build_type", "unspecified")))
                    for run in runs
                ],
                "summary": summary,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )

    with (output / "bench.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "position",
                "kind",
                "build_type",
                "limit_type",
                "limit_value",
                "depth",
                "score",
                "nodes",
                "nps",
                "ms",
                "wall_ms",
                "bestmove",
                "legal",
                "pv",
            ]
        )
        for run in runs:
            writer.writerow(
                [
                    run["position"],
                    run["kind"],
                    meta.get("build_type", "unspecified"),
                    run["limit"]["type"],
                    run["limit"]["value"],
                    run["depth"],
                    f"{run['score_type']} {run['score_value']}",
                    run["nodes"],
                    run["nps"],
                    run["ms"],
                    run["wall_ms"],
                    run["bestmove"],
                    run["legal"],
                    " ".join(run["pv"]),
                ]
            )

    lines: List[str] = []
    lines.append("# try4 search benchmark")
    lines.append("")
    lines.append("| Field | Value |")
    lines.append("|---|---|")
    for key in [
        "date",
        "engine",
        "engine_version",
        "build_type",
        "compiler",
        "cpu",
        "os",
        "positions_file",
        "positions_sha256",
        "positions_selected",
        "hash_mb",
        "hash_default_mb",
        "cpu_affinity",
        "priority",
        "options",
        "repeats",
        "depths",
        "movetime_ms",
        "warmup_ms",
        "timeout_ms",
    ]:
        value = meta.get(key, "")
        if isinstance(value, list):
            value = "; ".join(str(item) for item in value) or "none"
        lines.append(f"| {key} | {value} |")
    lines.append("")

    depth_groups = [
        group for group in summary["groups"] if group["limit_type"] == "depth"
    ]
    if depth_groups:
        lines.append("## Fixed depth")
        lines.append("")
        lines.append(
            "| Position | Kind | Depth | Nodes (median) | ms (median) | NPS (median) | "
            "Score | Best move | Nodes/PV stable |"
        )
        lines.append("|---|---|---:|---:|---:|---:|---|---|---|")
        for group in depth_groups:
            score = f"{group['score_type']} {group['score_value']}"
            lines.append(
                f"| {group['position']} | {group['kind']} | {group['depth']} | "
                f"{group['nodes_median']:,} | {group['ms_median']:.0f} | "
                f"{group['nps_median']:,.0f} | {score} | {group['bestmove']} | "
                f"{'yes' if group['deterministic'] else 'no'} |"
            )
        lines.append("")

    time_groups = [
        group for group in summary["groups"] if group["limit_type"] == "movetime"
    ]
    if time_groups:
        lines.append("## Fixed time")
        lines.append("")
        lines.append(
            "| Position | Kind | movetime (ms) | Depth (median) | Nodes (median) | "
            "NPS min | NPS median | NPS max | NPS spread | Score | Best move |"
        )
        lines.append("|---|---|---:|---:|---:|---:|---:|---:|---:|---|---|")
        for group in time_groups:
            score = f"{group['score_type']} {group['score_value']}"
            lines.append(
                f"| {group['position']} | {group['kind']} | {group['limit_value']} | "
                f"{group['depth']} | {group['nodes_median']:,} | "
                f"{group['nps_min']:,.0f} | {group['nps_median']:,.0f} | "
                f"{group['nps_max']:,.0f} | {group['nps_spread_percent']:.1f}% | "
                f"{score} | {group['bestmove']} |"
            )
        lines.append("")

    lines.append("## Integrity")
    lines.append("")
    failures = summary["failures"]
    if failures:
        for failure in failures:
            lines.append(f"- FAIL: {failure}")
    else:
        lines.append(
            "- Every fixed-depth node count, PV and best move was identical across repeats."
        )
        lines.append("- Every returned best move was accepted by the CLI judge.")
    lines.append("")

    (output / "bench.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_arguments(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run try4 fixed-depth and fixed-time search benchmarks."
    )
    parser.add_argument("--engine", required=True, type=Path, help="try4_uci executable")
    parser.add_argument(
        "--cli",
        type=Path,
        help="legal-move judge; defaults to the sibling try4 executable",
    )
    parser.add_argument(
        "--positions",
        type=Path,
        default=DEFAULT_POSITIONS,
        help="TSV with id<TAB>kind<TAB>fen",
    )
    parser.add_argument(
        "--depths", default=DEFAULT_DEPTHS, help="comma-separated fixed depths"
    )
    parser.add_argument(
        "--only",
        default="",
        help="comma-separated position ids to run; empty runs the whole file",
    )
    parser.add_argument(
        "--movetime-ms",
        type=int,
        default=0,
        help="fixed movetime in ms; 0 disables fixed-time runs",
    )
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--timeout-ms", type=int, default=30000)
    parser.add_argument(
        "--hash-mb",
        type=int,
        default=0,
        help="optional Hash value sent before the runs; 0 keeps the default",
    )
    parser.add_argument(
        "--label", default="search-bench", help="free-form label stored in meta"
    )
    parser.add_argument(
        "--warmup-ms",
        type=int,
        default=0,
        help="discarded search on the first position before measuring (0 disables)",
    )
    parser.add_argument(
        "--cpu",
        type=int,
        default=-1,
        help="pin the engine to a zero-based logical CPU; -1 disables pinning",
    )
    parser.add_argument(
        "--priority",
        choices=["normal", "high"],
        default="normal",
        help="engine process priority class",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="exit nonzero when fixed-time NPS spread exceeds 5%%",
    )
    return parser.parse_args(argv)


def default_cli(engine: Path) -> Path:
    name = "try4.exe" if engine.suffix.lower() == ".exe" else "try4"
    candidate = engine.with_name(name)
    if not candidate.is_file():
        raise BenchmarkError(
            f"cannot find the CLI judge at {candidate}; pass --cli explicitly"
        )
    return candidate


def main(argv: Optional[List[str]] = None) -> int:
    arguments = parse_arguments(argv)
    engine_path = arguments.engine.resolve()
    if not engine_path.is_file():
        print(f"error: engine not found: {engine_path}", file=sys.stderr)
        return 2
    try:
        cli_path = (arguments.cli or default_cli(engine_path)).resolve()
    except BenchmarkError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    if not cli_path.is_file():
        print(f"error: CLI judge not found: {cli_path}", file=sys.stderr)
        return 2
    if arguments.repeats < 1:
        print("error: --repeats must be >= 1", file=sys.stderr)
        return 2
    positions_path = arguments.positions.resolve()
    try:
        positions = parse_positions(positions_path)
        depths = parse_depths(arguments.depths)
    except (BenchmarkError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    selected = [token.strip() for token in arguments.only.split(",") if token.strip()]
    if selected:
        known = {position["id"] for position in positions}
        missing = sorted(set(selected) - known)
        if missing:
            print(f"error: unknown position id(s): {', '.join(missing)}", file=sys.stderr)
            return 2
        positions = [position for position in positions if position["id"] in selected]
    if arguments.movetime_ms <= 0 and not depths:
        print("error: enable fixed-depth or fixed-time runs", file=sys.stderr)
        return 2

    cache = read_cmake_cache(engine_path.parent)
    options = ["setoption name OwnBook value false"]
    if arguments.hash_mb > 0:
        options.append(f"setoption name Hash value {arguments.hash_mb}")
    meta: Dict[str, object] = {
        "date": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "label": arguments.label,
        "script_version": SCRIPT_VERSION,
        "engine": str(engine_path),
        "engine_version": engine_version(engine_path),
        "build_type": cache.get("CMAKE_BUILD_TYPE", "unspecified"),
        "compiler": compiler_description(cache),
        "cpu": platform.processor() or platform.machine(),
        "os": platform.platform(),
        "positions_file": str(positions_path),
        "positions_sha256": sha256_file(positions_path),
        "positions_selected": selected or "all",
        "hash_mb": arguments.hash_mb if arguments.hash_mb > 0 else "default",
        "hash_default_mb": "unknown",
        "cpu_affinity": arguments.cpu if arguments.cpu >= 0 else "none",
        "priority": arguments.priority,
        "options": options,
        "repeats": arguments.repeats,
        "depths": depths,
        "movetime_ms": arguments.movetime_ms,
        "warmup_ms": arguments.warmup_ms,
        "timeout_ms": arguments.timeout_ms,
    }

    engine = Engine(
        engine_path, arguments.timeout_ms, arguments.cpu, arguments.priority
    )
    judge = Judge(cli_path)
    runs: List[Dict[str, object]] = []
    failures: List[str] = []
    try:
        uci_lines: List[str] = []
        engine.send("uci")
        engine.read_until(
            lambda line: line.strip() == "uciok",
            arguments.timeout_ms,
            collect=uci_lines,
        )
        for line in uci_lines:
            if line.startswith("option name Hash "):
                match = re.search(r"default (\d+)", line)
                if match:
                    meta["hash_default_mb"] = int(match.group(1))
        for option in options:
            engine.send(option)
        engine.send("isready")
        engine.read_until(lambda line: line.strip() == "readyok", arguments.timeout_ms)

        if arguments.warmup_ms > 0:
            print(f"warm-up {arguments.warmup_ms} ms", flush=True)
            run_search(
                engine,
                positions[0]["fen"],
                depth=None,
                movetime_ms=arguments.warmup_ms,
                timeout_ms=max(arguments.timeout_ms, arguments.warmup_ms + 5000),
            )

        plan: List[Dict[str, object]] = []
        if arguments.movetime_ms > 0:
            plan.append({"type": "movetime", "value": arguments.movetime_ms})
        for depth in depths:
            plan.append({"type": "depth", "value": depth})

        aborted = False
        for limit in plan:
            for position in positions:
                for repeat in range(arguments.repeats):
                    label = (
                        f"{position['id']} {limit['type']}={limit['value']} "
                        f"run {repeat + 1}/{arguments.repeats}"
                    )
                    print(label, flush=True)
                    try:
                        result = run_search(
                            engine,
                            position["fen"],
                            depth=int(limit["value"]) if limit["type"] == "depth" else None,
                            movetime_ms=(
                                int(limit["value"]) if limit["type"] == "movetime" else None
                            ),
                            timeout_ms=arguments.timeout_ms,
                        )
                    except EngineTimeout as error:
                        failures.append(f"{label}: {error}")
                        failures.append(
                            "benchmark aborted after timeout; remaining cases were skipped"
                        )
                        print(f"  error: {error}", file=sys.stderr)
                        aborted = True
                        break
                    except BenchmarkError as error:
                        failures.append(f"{label}: {error}")
                        print(f"  error: {error}", file=sys.stderr)
                        continue
                    legal = judge.legal(position["fen"], str(result["bestmove"]))
                    if not legal:
                        failures.append(
                            f"{label}: illegal bestmove {result['bestmove']}"
                        )
                    if (
                        limit["type"] == "movetime"
                        and int(result["wall_ms"]) > int(limit["value"]) + 100
                    ):
                        failures.append(
                            f"{label}: wall time {result['wall_ms']} ms overran "
                            f"the {limit['value']} ms budget by more than 100 ms"
                        )
                    result.update(
                        {
                            "position": position["id"],
                            "kind": position["kind"],
                            "fen": position["fen"],
                            "legal": legal,
                        }
                    )
                    runs.append(result)
                if aborted:
                    break
            if aborted:
                break
    except (BenchmarkError, EngineTimeout) as error:
        failures.append(f"startup: {error}")
        print(f"error: {error}", file=sys.stderr)
    finally:
        engine.close()
        judge.close()

    summary = summarize_runs(runs)
    summary["failures"].extend(failures)
    write_outputs(arguments.output.resolve(), meta, runs, summary)

    depth_runs = [run for run in runs if run["limit"]["type"] == "depth"]
    time_runs = [run for run in runs if run["limit"]["type"] == "movetime"]
    print("")
    print(f"fixed-depth searches: {len(depth_runs)}")
    print(f"fixed-time searches: {len(time_runs)}")
    print(f"reports: {arguments.output.resolve()}")

    spread_warning = False
    for group in summary["groups"]:
        if group["limit_type"] == "movetime" and group["nps_spread_percent"] > 5.0:
            spread_warning = True
            print(
                f"warning: {group['position']} NPS spread "
                f"{group['nps_spread_percent']:.1f}% exceeds 5%",
                file=sys.stderr,
            )

    if summary["failures"]:
        for failure in summary["failures"]:
            print(f"failure: {failure}", file=sys.stderr)
        return 1
    if spread_warning and arguments.strict:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
