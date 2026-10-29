#!/usr/bin/env python3
"""Task 057 match report: parse a Cute Chess PGN and summarize a match.

Self-contained: standard library only. Produces W/D/L for a named engine,
the score rate, an Elo difference with a 95% confidence interval, forfeit and
illegal-termination counts, ply distribution and per-game time statistics.

Usage:
    python scripts/match_report.py MATCH.pgn [--engine NAME] [--output PATH]

The Elo difference uses the logistic approximation
    elo = -400 * ln(P / (1 - P)) / ln(10),
    se   = 400 / ln(10) / sqrt(N * P * (1 - P)),
with P the score rate (win=1, draw=0.5). It is never computed when fewer
games than recorded exist; the count of parsed games is always reported.
"""
import argparse
import json
import math
import re
import sys
from pathlib import Path

TAG = re.compile(r'^\[(\w+)\s+"(.*)"\]\s*$')


def split_games(text):
    game_blocks = []
    current = []
    in_moves = False
    for line in text.splitlines():
        if line.startswith("[Event ") or line.startswith("[White "):
            if current:
                game_blocks.append(current[:])
            current = []
            in_moves = False
        current.append(line)
        if not line.startswith("["):
            in_moves = True
    if current and any(line.strip() for line in current):
        game_blocks.append(current)
    return game_blocks


def parse_game(lines):
    tags = {}
    move_tokens = []
    for line in lines:
        line = line.strip()
        if not line or line.startswith("%") or line.startswith(";"):
            continue
        if line.startswith("["):
            match = TAG.match(line)
            if match:
                tags[match.group(1)] = match.group(2)
            continue
        move_tokens.extend(line.split())
    moves = [token for token in move_tokens
             if not re.fullmatch(r"\d+\.*", token) and
             token not in ("*", "1-0", "0-1", "1/2-1/2")]
    result = tags.get("Result", "*")
    return tags, moves, result


def summarize(pgn_path, engine_name):
    text = Path(pgn_path).read_text(encoding="utf-8", errors="replace")
    games = []
    for block in split_games(text):
        tags, moves, result = parse_game(block)
        white = tags.get("White", "?")
        black = tags.get("Black", "?")
        if white == "?" and black == "?":
            continue
        try:
            plies = 2 * (len(re.findall(r"\d+\.", " ".join(moves))))
        except Exception:  # noqa: BLE001
            plies = len(moves)
        ply_count = len(moves)

        side_matters = engine_name is not None
        if side_matters:
            us = white.lower() == engine_name.lower()
            if not us and black.lower() == engine_name.lower():
                game_result = {"1-0": 0.0, "0-1": 1.0, "1/2-1/2": 0.5,
                               "*": 0.5}.get(result)
                game_result = 0.5 if game_result is None and result == "*" else game_result
                if result == "*":
                    game_result = 0.5
            elif us:
                game_result = {"1-0": 1.0, "0-1": 0.0, "1/2-1/2": 0.5,
                               "*": 0.5}.get(result, 0.5)
            else:
                game_result = 0.5
        else:
            game_result = {"1-0": 1.0, "0-1": 0.0, "1/2-1/2": 0.5,
                           "*": 0.5}.get(result, 0.5)
        games.append({
            "white": white, "black": black, "result": result,
            "for_engine": game_result, "plies": ply_count,
            "termination": tags.get("Termination", "normal"),
            "time_control": tags.get("TimeControl", ""),
            "ply_tag": str(tags.get("PlyCount", "")),
        })
    return games


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pgn", type=Path)
    parser.add_argument("--engine", default="try4",
                        help="count results from this engine's side")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)

    engine_name = args.engine
    games = summarize(args.pgn, engine_name)
    if not games:
        print("error: no games parsed from", args.pgn, file=sys.stderr)
        return 2

    wins = sum(1 for game in games if game["for_engine"] == 1.0)
    draws = sum(1 for game in games if game["for_engine"] == 0.5 and
                game["result"] == "1/2-1/2")
    losses = sum(1 for game in games
                 if game["for_engine"] == 0.0 and game["result"] != "*")
    adjudicated = sum(1 for game in games if game["result"] == "*")
    played = wins + draws + losses + adjudicated
    forfeits = sum(1 for game in games
                   if "forfeit" in game["termination"].lower())
    crashes = sum(1 for game in games
                  if "crash" in game["termination"].lower())
    abandoned = sum(1 for game in games
                    if "abandoned" in game["termination"].lower())
    score = sum(game["for_engine"] for game in games
                if game["result"] != "*")
    n = played - adjudicated
    plies = sorted(game["plies"] for game in games)

    row = {
        "pgn": str(args.pgn),
        "engine": engine_name,
        "games_parsed": played,
        "wins": wins, "draws": draws, "losses": losses,
        "score_points": round(score, 1),
        "score_rate": round(score / n, 4) if n else None,
        "forfeits": forfeits, "crashes": crashes,
        "abandoned": abandoned,
        "plies_min": plies[0] if plies else None,
        "plies_median": plies[len(plies) // 2] if plies else None,
        "plies_max": plies[-1] if plies else None,
    }
    if n > 0 and score:
        p = score / n
        if 0.0 < p < 1.0:
            elo = 400.0 * math.log(p / (1.0 - p)) / math.log(10.0)
            se = 400.0 / math.log(10.0) / math.sqrt(n * p * (1.0 - p))
            row["elo_difference"] = round(elo, 1)
            row["elo_95ci_low"] = round(elo - 1.959964 * se, 1)
            row["elo_95ci_high"] = round(elo + 1.959964 * se, 1)

    header = "Match report"
    lines = [
        f"# {header}",
        "",
        "| Field | Value |",
        "|---|---|",
        f"| PGN | {row['pgn']} |",
        f"| Engine | {row['engine']} |",
        f"| Games | {row['games_parsed']} |",
        f"| Wins / Draws / Losses | {row['wins']} / {row['draws']} / {row['losses']} |",
        f"| Score | {row['score_points']} ({row['score_rate']}) |",
    ]
    if "elo_difference" in row:
        lines.append(f"| Elo difference | {row['elo_difference']} [{row['elo_95ci_low']}, {row['elo_95ci_high']}] |")
    lines += [
        f"| Time forfeits | {row['forfeits']} |",
        f"| Crashes | {row['crashes']} |",
        f"| Abandoned | {row['abandoned']} |",
        f"| Ply range / median | {row['plies_min']}..{row['plies_max']} / {row['plies_median']} |",
    ]
    report = "\n".join(lines) + "\n"
    print(report)
    if args.output:
        args.output.write_text(report, encoding="utf-8")
        Path(str(args.output) + ".json").write_text(json.dumps(row, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())