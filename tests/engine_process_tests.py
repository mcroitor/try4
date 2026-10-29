"""Real UCI/configuration process tests; no third-party Python dependencies."""
import argparse
from pathlib import Path
import queue
import re
import shutil
import subprocess
import tempfile
import threading
import time
import unittest


class Engine:
    def __init__(self, executable, args=(), cwd=None):
        self.process = subprocess.Popen([str(executable), *map(str, args)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding="utf-8", cwd=cwd, bufsize=1)
        self.lines = queue.Queue()
        self.trace = []
        def read():
            for line in self.process.stdout:
                self.lines.put(line.strip())
        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()

    def send(self, text):
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()

    def until(self, prefix, timeout=10):
        end = time.monotonic() + timeout
        while True:
            try:
                line = self.lines.get(timeout=max(0, end - time.monotonic()))
            except queue.Empty:
                raise AssertionError(f"Timed out waiting for {prefix}: {self.trace}")
            self.trace.append(line)
            if line.startswith(prefix):
                return line

    def quiet(self, duration=0.05):
        try:
            line = self.lines.get(timeout=duration)
            self.trace.append(line)
            raise AssertionError(f"Unexpected response: {line}")
        except queue.Empty:
            pass

    def close(self):
        if self.process.poll() is None:
            self.send("quit")
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
                raise AssertionError("Engine hung on quit")
        self.reader.join(timeout=2)
        self.process.stdin.close()
        self.process.stdout.close()
        stderr = self.process.stderr.read()
        self.process.stderr.close()
        if self.process.returncode != 0 or stderr:
            raise AssertionError(f"Engine exited {self.process.returncode}: {stderr}")


class ProcessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="try4 process ")
        self.root = Path(self.directory.name)
        self.config = self.root / "engine config.ini"
        self.config.write_text("", encoding="utf-8")
        self.engines = []

    def tearDown(self):
        try:
            for engine in self.engines:
                engine.close()
        finally:
            self.directory.cleanup()

    def engine(self):
        engine = Engine(EXECUTABLE, ["--config", self.config], cwd=self.root)
        self.engines.append(engine)
        engine.send("uci")
        engine.until("uciok")
        return engine

    def test_natural_depth_time_nodes_and_mate(self):
        e = self.engine()
        self.assertFalse(any("UCI_Chess960" in line for line in e.trace))
        e.send("ucinewgame")
        e.quiet()
        e.send("position\tstartpos\n go\tdepth   2")
        e.until("bestmove ")
        self.assertTrue(any(line.startswith("info depth 2 ") for line in e.trace))
        e.send("go movetime 100")
        start = time.monotonic()
        e.until("bestmove ")
        self.assertLess(time.monotonic() - start, 2)
        e.send("go nodes 100")
        e.until("bestmove ")
        e.send("position fen 7k/8/5K2/6Q1/8/8/8/8 w - - 0 1\ngo depth 1")
        self.assertEqual(e.until("bestmove "), "bestmove g5g7")
        self.assertTrue(any("score mate 1 " in line for line in e.trace))
        # Transactional rejection: illegal suffix must not leave a partial board.
        e.send("position startpos moves e2e4 illegal\ngo depth 1")
        self.assertEqual(e.until("bestmove "), "bestmove g5g7")

    def test_info_streams_each_completed_depth(self):
        e = self.engine()
        e.send("position startpos\ngo depth 4")
        e.until("bestmove ")
        infos = [line for line in e.trace if line.startswith("info depth ")]
        depths = [int(re.match(r"info depth (\d+) ", line).group(1)) for line in infos]
        self.assertEqual(depths, [1, 2, 3, 4])
        self.assertEqual(sum(line.startswith("bestmove ") for line in e.trace), 1)
        for line in infos:
            self.assertRegex(line, r" score (cp|mate) -?\d+ ")
            self.assertRegex(line, r" nodes \d+ nps \d+ time \d+")
            self.assertIn(" pv ", line)

    def test_infinite_terminal_result_waits_for_stop_and_isready_works(self):
        e = self.engine()
        e.send("position fen 7k/8/5K2/6Q1/8/8/8/8 w - - 0 1\ngo infinite")
        e.until("info depth ")
        e.quiet(0.15)
        e.send("isready")
        self.assertEqual(e.until("readyok"), "readyok")
        e.quiet(0.05)
        start = time.monotonic()
        e.send("stop")
        self.assertEqual(e.until("bestmove "), "bestmove g5g7")
        latency = time.monotonic() - start
        print(f"terminal infinite stop latency: {latency * 1000:.2f} ms")
        self.assertLess(latency, 0.25)
        e.quiet()
        e.send("position startpos\ngo infinite")
        e.send("isready")
        e.until("readyok")
        e.send("setoption name Hash value 8")
        e.until("bestmove ")  # Option mutation joins the previous search.
        e.send("go depth 1")
        e.until("bestmove ")

    def test_unsupported_go_and_invalid_option_do_not_corrupt_state(self):
        e = self.engine()
        for command in ("go ponder", "go searchmoves e2e4", "go depth -1", "go depth 999999999999"):
            e.send(command)
            e.until("info string Unsupported or invalid go command")
        e.send("setoption name Hash value -1\nsetoption name Threads value 999999999999")
        e.send("go depth 1")
        e.until("bestmove ")

    def test_configuration_book_precedence_and_newgame(self):
        book = self.root / "book #1;test=a.txt"
        book.write_text("only\te2e4 e7e5\n")
        self.config.write_text("[uci]\nhash_mb=24\nbook_file=book #1;test=a.txt\n"
                               "[search]\nforcing_checks=true\n", encoding="utf-8")
        e = self.engine()
        self.assertIn("option name Hash type spin default 24 min 1 max 131072", e.trace)
        e.send("position startpos\ngo depth 1")
        self.assertEqual(e.until("bestmove "), "bestmove e2e4")
        e.send("setoption name OwnBook value false\nucinewgame\nposition startpos\ngo depth 1")
        e.until("bestmove ")
        self.assertTrue(any(line.startswith("info depth 1 ") for line in e.trace))
        e.send("setoption\tname  OwnBook\tvalue true\nposition startpos\ngo depth 1")
        self.assertEqual(e.until("bestmove "), "bestmove e2e4")

    def test_startup_failure_version_and_executable_adjacent_discovery(self):
        copied = self.root / EXECUTABLE.name
        shutil.copy2(EXECUTABLE, copied)
        discovered = self.root / "try4.ini"
        discovered.write_text("[uci]\nhash_mb=24\n")
        result = subprocess.run([str(copied)], input="uci\nquit\n", text=True,
                                capture_output=True, timeout=10, cwd=SOURCE)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("default 24 ", result.stdout)
        discovered.write_text("[bad]\n")
        for arguments in ([], ["--config", self.root / "missing.ini"], ["--config"]):
            result = subprocess.run([str(copied), *map(str, arguments)], input="", text=True,
                                    capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "")
            self.assertTrue(result.stderr)
        result = subprocess.run([str(copied), "--version"], text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0)
        self.assertTrue(result.stdout.startswith("try4 "))
        result = subprocess.run([str(copied), "--config", str(self.config)], input="uci\nquit\n",
                                text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("default 16 ", result.stdout)

    def test_configured_material_changes_search_score(self):
        fen = "7k/8/8/8/8/8/P7/7K w - - 0 1"
        scores = []
        for pawn in (120, 220):
            self.config.write_text(f"[evaluation]\nmaterial_eg=0,{pawn},300,320,540,900,0\n")
            e = self.engine()
            e.send(f"position fen {fen}\ngo depth 1")
            e.until("bestmove ")
            info = next(line for line in e.trace if line.startswith("info depth 1 "))
            scores.append(int(re.search(r"score cp (-?\d+)", info).group(1)))
        self.assertEqual(scores[1] - scores[0], 100)

    def test_quit_during_search_exits_cleanly(self):
        e = self.engine()
        e.send("position startpos\ngo infinite")
        e.until("info depth ")
        start = time.monotonic()
        e.send("quit")
        e.process.wait(timeout=5)
        self.assertLess(time.monotonic() - start, 1.0)
        self.assertEqual(e.process.returncode, 0)
        self.assertEqual(e.process.stderr.read(), "")

    def test_book_miss_and_tight_clock_still_finish(self):
        book = self.root / "miss-book.txt"
        book.write_text("only\td2d4 d7d5\n")
        self.config.write_text(f"[uci]\nbook_file={book.name}\n", encoding="utf-8")
        e = self.engine()
        e.send("position startpos moves e2e4\ngo wtime 50 btime 50 winc 0 binc 0")
        start = time.monotonic()
        bestmove = e.until("bestmove ")
        self.assertLess(time.monotonic() - start, 1.0)
        self.assertNotEqual(bestmove, "bestmove 0000")
        self.assertFalse(any("Book move" in line for line in e.trace))
        e.send("setoption name BookFile value missing-book.bin")
        e.until("info string Book load failed:")
        e.send("position startpos\ngo wtime 50 btime 50 winc 0 binc 0")
        self.assertLess(time.monotonic() - start, 2.5)
        e.until("bestmove ")

    def test_protocol_stress_stop_restart_and_option_reload(self):
        e = self.engine()
        stop_latencies = []
        for iteration in range(12):
            e.send("isready")
            e.until("readyok")
            e.send(f"setoption name Hash value {8 if iteration % 2 else 16}")
            e.send("ucinewgame")
            e.quiet(0.02)  # ucinewgame must not emit an unsolicited readyok.
            e.send("position startpos\ngo infinite")
            e.send("isready")
            e.until("readyok")
            start = time.monotonic()
            e.send("stop")
            e.until("bestmove ")
            stop_latencies.append(time.monotonic() - start)
            e.quiet(0.02)  # Exactly one bestmove per go/stop transaction.
        e.send("position startpos\ngo depth 2")
        e.until("bestmove ")
        e.send("setoption name Threads value 2")  # Rejected, prior value retained.
        e.send("go nodes 50")
        e.until("bestmove ")
        self.assertLess(max(stop_latencies), 0.25)
        print(f"stress stop latency max: {max(stop_latencies) * 1000:.2f} ms")

    def test_forcing_config_finds_quiet_mate_finish(self):
        fen = "r2qkbnr/ppp2ppp/2np4/4N3/2B1P3/2N4P/PPPP1PP1/R1BbK2R w KQkq - 0 7"
        for checks in ("false", "true"):
            self.config.write_text(f"[search]\nresign_cp=0\n"
                                   f"forcing_checks={checks}\nforcing_max_plies=2\n")
            e = self.engine()
            e.send(f"position fen {fen}\ngo depth 1")
            self.assertEqual(e.until("bestmove "), "bestmove c4f7")
            info = next(line for line in e.trace if line.startswith("info depth 1 "))
            self.assertEqual("score mate 2 " in info, checks == "true")
            if checks == "true":
                losing = "r2qkbnr/ppp2Bpp/2np4/4N3/4P3/2N4P/PPPP1PP1/R1BbK2R b KQkq - 0 7"
                e.send(f"position fen {losing}\ngo depth 1")
                self.assertEqual(e.until("bestmove "), "bestmove e8e7")
                self.assertTrue(any("score mate -1 " in line for line in e.trace))

    def test_resigns_below_default_threshold(self):
        # Black stands roughly a queen and a half short; the completed score
        # is far under the built-in -600 cp threshold.
        e = self.engine()
        e.send("position fen 3k4/8/8/8/8/8/8/QQ4K1 b - - 0 1\ngo movetime 300")
        self.assertIn("threshold -600", e.until("info string resign score cp "))
        self.assertEqual(e.until("bestmove "), "bestmove 0000")
        self.assertEqual(sum(line.startswith("bestmove ") for line in e.trace), 1)

    def test_resign_disabled_by_option(self):
        e = self.engine()
        e.send("setoption name Resign value 0")
        e.send("position fen 3k4/8/8/8/8/8/8/QQ4K1 b - - 0 1\ngo movetime 300")
        best = e.until("bestmove ")
        self.assertNotEqual(best, "bestmove 0000")
        self.assertFalse(any("info string resign" in line for line in e.trace))

    def test_negative_mate_resigns_with_mate_notation(self):
        # Bxf7+ was played; black is in check with the forced eviction to be
        # mated next ply by Nd5#. Its completed score is a negative mate.
        e = self.engine()
        e.send("position fen r2qkbnr/ppp2Bpp/2np4/4N3/4P3/2N4P/PPPP1PP1/R1BbK2R b KQkq - 0 7\ngo movetime 300")
        self.assertIn("info string resign score mate", e.until("info string resign "))
        self.assertEqual(e.until("bestmove "), "bestmove 0000")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()
    EXECUTABLE = args.engine.resolve()
    SOURCE = args.source.resolve()
    unittest.main(argv=[__file__], verbosity=2)
