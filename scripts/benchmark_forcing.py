#!/usr/bin/env python3
"""Reproducible small UCI A/B benchmark; not an Elo or stability test."""
import argparse
import datetime
import json
from pathlib import Path
import platform
import queue
import re
import subprocess
import tempfile
import threading
import time

POSITIONS = {
    "startpos": "startpos",
    "legal-mate": "fen r2qkbnr/ppp2ppp/2np4/4N3/2B1P3/2N4P/PPPP1PP1/R1BbK2R w KQkq - 0 7",
    "kiwipete": "fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "hanging-queen": "fen rnb1kbnr/pppp1ppp/8/4p3/4P2q/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1",
}


class Engine:
    def __init__(self, executable, config):
        self.process = subprocess.Popen([str(executable), "--config", str(config)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1)
        self.lines = queue.Queue()

        def read():
            for line in self.process.stdout:
                self.lines.put(line.strip())

        self.thread = threading.Thread(target=read, daemon=True)
        self.thread.start()

    def send(self, text):
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()

    def until(self, prefix, timeout=10):
        end = time.perf_counter() + timeout
        seen = []
        while True:
            try:
                line = self.lines.get(timeout=max(0, end - time.perf_counter()))
            except queue.Empty:
                raise RuntimeError(f"timeout waiting for {prefix}: {seen}")
            seen.append(line)
            if line.startswith(prefix):
                return line, seen

    def close(self):
        if self.process.poll() is None:
            self.send("quit")
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.thread.join(timeout=1)
        error = self.process.stderr.read()
        self.process.stdin.close()
        self.process.stdout.close()
        self.process.stderr.close()
        if self.process.returncode or error:
            raise RuntimeError(f"engine failure: {error}")


def parse_info(lines):
    info = None
    for line in lines:
        if line.startswith("info depth "):
            info = line
    score = None
    mate = None
    nodes = None
    if info:
        match = re.search(r"score (cp|mate) (-?\d+)", info)
        if match:
            if match.group(1) == "cp":
                score = int(match.group(2))
            else:
                mate = int(match.group(2))
        match = re.search(r"nodes (\d+)", info)
        if match:
            nodes = int(match.group(1))
    return {"info": info, "score_cp": score, "score_mate": mate, "nodes": nodes}


def measure(engine, position, limit):
    start = time.perf_counter()
    engine.send(f"position {position}\ngo {limit}")
    line, lines = engine.until("bestmove ")
    bestmove = line.split()[1] if len(line.split()) > 1 else None
    result = parse_info(lines)
    result.update(bestmove=bestmove, wall_ms=round((time.perf_counter() - start) * 1000, 3))
    return result


def measure_stop(engine, position):
    engine.send(f"position {position}\ngo infinite")
    time.sleep(0.02)
    start = time.perf_counter()
    engine.send("stop")
    _, lines = engine.until("bestmove ")
    return {"wall_ms": round((time.perf_counter() - start) * 1000, 3),
            "bestmove": next((line.split()[1] for line in lines
                              if line.startswith("bestmove ")), None)}


def load_tactical(path):
    cases = []
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        text = raw.strip()
        if not text or text.startswith("#"):
            continue
        fields = [field.strip() for field in text.split("\t")]
        if len(fields) != 7:
            raise RuntimeError("malformed tactical row: " + raw)
        cases.append({"id": fields[0], "phase": fields[1], "fen": fields[2],
                      "depth": int(fields[3]), "expect": fields[4],
                      "avoid": fields[5], "note": fields[6]})
    return cases


def check_tactical(case, result):
    passed = result["bestmove"] is not None
    details = []
    for token in case["expect"].split(";"):
        if token.startswith("mate:"):
            plies = int(token[5:])
            expected_mate = (plies // 2) if plies % 2 == 0 else (plies + 1) // 2
            ok = result["score_mate"] == expected_mate
            passed = passed and ok
            details.append({"expect": token, "ok": ok,
                            "observed": result["score_mate"]})
        elif token.startswith("best:"):
            ok = result["bestmove"] == token[5:]
            passed = passed and ok
            details.append({"expect": token, "ok": ok,
                            "observed": result["bestmove"]})
        elif token == "positive":
            ok = result["score_cp"] is not None and result["score_cp"] > 0
            passed = passed and ok
            details.append({"expect": token, "ok": ok,
                            "observed": result["score_cp"]})
        elif token.startswith("score:"):
            ok = result["score_cp"] == int(token[6:])
            passed = passed and ok
            details.append({"expect": token, "ok": ok,
                            "observed": result["score_cp"]})
        else:
            passed = False
            details.append({"expect": token, "ok": False,
                            "observed": "unknown expectation"})
    if case["avoid"] != "-" and result["bestmove"] == case["avoid"]:
        passed = False
        details.append({"expect": "avoid:" + case["avoid"], "ok": False,
                        "observed": result["bestmove"]})
    return passed, details


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tactical", type=Path,
                        default=Path(__file__).resolve().parents[1] /
                                "tests/data/tactical/positions.tsv")
    args = parser.parse_args()
    executable = args.engine.resolve()
    example_dir = Path(__file__).resolve().parents[1]
    example = example_dir / "config/try4.ini"
    snapshots = {"builtins": "", "default-file": example.read_text(),
                 "forcing": example.read_text().replace("forcing_checks=false",
                                                        "forcing_checks=true")}
    tactical_cases = load_tactical(args.tactical)
    report = {"version": subprocess.check_output([str(executable), "--version"], text=True).strip(),
              "platform": platform.platform(), "processor": platform.processor(),
              "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "positions": POSITIONS,
              "tactical_corpus": str(args.tactical.resolve()),
              "configurations": snapshots, "runs": [], "tactical": [],
              "stop_probes": [], "summary": {}}
    with tempfile.TemporaryDirectory(prefix="try4 benchmark ") as temp:
        for name, text in snapshots.items():
            config = Path(temp) / "engine.ini"
            config.write_text(text)
            engine = Engine(executable, config)
            try:
                for position_id, position in POSITIONS.items():
                    for limit in ("depth 1", "nodes 1000", "movetime 100"):
                        result = measure(engine, position, limit)
                        result.update(configuration=name, position=position_id,
                                      limit=limit)
                        report["runs"].append(result)
                    probe = measure_stop(engine, position)
                    probe.update(configuration=name, position=position_id)
                    report["stop_probes"].append(probe)
                for case in tactical_cases:
                    result = measure(engine, "fen " + case["fen"],
                                     "depth " + str(case["depth"]))
                    passed, details = check_tactical(case, result)
                    result.update(configuration=name, case=case["id"],
                                  passed=passed, checks=details)
                    report["tactical"].append(result)
            finally:
                engine.close()

    for name in snapshots:
        runs = [run for run in report["runs"] if run["configuration"] == name]
        cases = [case for case in report["tactical"] if case["configuration"] == name]
        probes = [probe for probe in report["stop_probes"] if probe["configuration"] == name]
        report["summary"][name] = {
            "searches": len(runs),
            "tactical_passed": sum(1 for case in cases if case["passed"]),
            "tactical_total": len(cases),
            "tactical_pass_rate": (sum(1 for case in cases if case["passed"]) /
                                   len(cases) if cases else None),
            "stop_latency_ms_max": max((probe["wall_ms"] for probe in probes),
                                       default=None),
            "nodes_total": sum(run["nodes"] or 0 for run in runs),
        }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2))
    print(f"Recorded {len(report['runs'])} searches and {len(report['tactical'])} "
          f"tactical cases: {args.output}")
    for name, summary in report["summary"].items():
        print(f"{name}: searches={summary['searches']} "
              f"tactical={summary['tactical_passed']}/{summary['tactical_total']} "
              f"stop_max={summary['stop_latency_ms_max']} ms "
              f"nodes={summary['nodes_total']}")
    for run in report["runs"]:
        if run["position"] == "legal-mate" and run["limit"] == "depth 1":
            print(run["configuration"], run["info"])


if __name__ == "__main__":
    main()
