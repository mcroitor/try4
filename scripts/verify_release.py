#!/usr/bin/env python3
"""Verify a CPack ZIP, its checksum, and the extracted engine entry points."""

import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--expected-version")
    args = parser.parse_args()
    version = args.expected_version
    if version is None:
        cmake = (Path(__file__).resolve().parents[1] / "CMakeLists.txt").read_text()
        version = re.search(r"project\(try4 VERSION ([0-9.]+)", cmake).group(1)
    archive = args.archive.resolve()
    if not archive.name.startswith(f"try4-{version}-"):
        raise RuntimeError(f"Wrong archive version: {archive.name}")
    tag = os.environ.get("GITHUB_REF", "")
    if tag.startswith("refs/tags/") and tag != f"refs/tags/v{version}":
        raise RuntimeError(f"Tag {tag} does not match version {version}")
    checksum = Path(str(archive) + ".sha256").read_text().split()[0].lower()
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if checksum != digest:
        raise RuntimeError("Archive SHA-256 mismatch")

    with tempfile.TemporaryDirectory(prefix="try4-package-") as directory:
        root = Path(directory)
        with zipfile.ZipFile(archive) as package:
            package.extractall(root)
        suffix = ".exe" if os.name == "nt" else ""
        engines = list(root.rglob("try4_uci" + suffix))
        if len(engines) != 1:
            raise RuntimeError("Package must contain exactly one UCI engine")
        engine = engines[0]
        # ZIP's Unix mode bits need explicit restoration with Python zipfile.
        for exe in (engine, engine.with_name("try4" + suffix)):
            exe.chmod(0o755)
        env = os.environ.copy()
        # Exclude compiler/MinGW directories to catch accidental DLL dependencies.
        env["PATH"] = (str(Path(env.get("SystemRoot", "C:/Windows")) / "System32")
                       if os.name == "nt" else "/usr/bin:/bin")

        def run(exe, *arguments, input_text=None):
            return subprocess.run(
                [str(exe), *arguments], input=input_text, text=True,
                capture_output=True, check=True, timeout=15, cwd=root, env=env
            ).stdout

        for exe in (engine, engine.with_name("try4" + suffix)):
            if run(exe, "--version").strip() != f"try4 {version}":
                raise RuntimeError(f"Version mismatch: {exe}")
        output = run(engine, input_text="uci\nisready\nposition startpos\ngo depth 1\nstop\nquit\n")
        for expected in (f"id name try4 {version}", "uciok", "readyok", "bestmove "):
            if expected not in output:
                raise RuntimeError(f"Packaged engine missing response: {expected}")
        examples = list(root.rglob("try4.ini"))
        if len(examples) != 1 or examples[0].parent.name != "examples":
            raise RuntimeError("Expected one non-active configuration example")
        config_dir = root / "config with spaces"
        config_dir.mkdir()
        config = config_dir / "engine.ini"
        config.write_text("[uci]\nhash_mb=24\nbook_file=relative book.txt\n"
                          "[evaluation]\nmaterial_eg=0,120,300,320,540,900,0\n"
                          "[search]\nforcing_checks=true\n", encoding="utf-8")
        (config_dir / "relative book.txt").write_text("only\te2e4 e7e5\n", encoding="utf-8")
        output = run(engine, "--config", str(config), input_text="uci\nisready\nquit\n")
        if "option name Hash type spin default 24 " not in output:
            raise RuntimeError("Packaged explicit configuration not applied")
        output = run(engine, "--config", str(config),
                     input_text="position startpos\ngo depth 1\nquit\n")
        if "bestmove e2e4" not in output:
            raise RuntimeError("Packaged relative BookFile was not resolved against the config")
        output = run(engine, "--config", str(config),
                     input_text="uci\nsetoption name Hash value 32\nisready\n"
                                "position startpos\ngo depth 1\nquit\n")
        if "bestmove " not in output:
            raise RuntimeError("Packaged GUI-style option override failed")
        bad = config_dir / "bad.ini"
        bad.write_text("[bad]\n", encoding="utf-8")
        failure = subprocess.run([str(engine), "--config", str(bad)], input="uci\nquit\n",
                                 text=True, capture_output=True, timeout=15, cwd=root, env=env)
        if failure.returncode == 0 or failure.stdout:
            raise RuntimeError("Packaged invalid configuration did not fail cleanly")
        for filename in ("LICENSE", "README.md", "CHANGELOG.md", "THIRD_PARTY_NOTICES.md"):
            if not list(root.rglob(filename)):
                raise RuntimeError(f"Missing package documentation: {filename}")
        if list(root.rglob("*.bk")) or list(root.rglob("tournament.txt")):
            raise RuntimeError("Opening data must not be bundled")
    print(f"PASS {archive.name}: SHA-256, versions, extracted UCI handshake, docs")


if __name__ == "__main__":
    main()
