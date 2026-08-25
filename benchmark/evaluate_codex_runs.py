#!/usr/bin/env python3
"""Compile, execute, and summarize Codex objdump recovery results."""

from __future__ import annotations

import argparse
import csv
import hashlib
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
from typing import Iterable


CASE_COLUMNS = [
    "program",
    "optimization",
    "track",
    "trial",
    "model",
    "reasoning",
    "response_valid",
    "trace_clean",
    "compile_ok",
    "compile_warnings",
    "run_ok",
    "exit_code",
    "stderr_empty",
    "stdout_exact",
    "stdout_token_similarity",
    "reexecutability",
    "model_wall_seconds",
    "run_path",
    "detail_path",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Evaluate recovered C in isolated Docker containers."
    )
    parser.add_argument(
        "--manifest", default="codex_objdump_runs/manifest.tsv", type=Path
    )
    parser.add_argument(
        "--output", default="evaluation/codex-objdump", type=Path
    )
    parser.add_argument("--image", default="type-recovery-bench:ubuntu22.04")
    parser.add_argument("--track", default="asm-only")
    parser.add_argument("--trial", default="trial-1")
    parser.add_argument("--model", help="Evaluate only this exact model ID")
    parser.add_argument("--optimization", help="Evaluate one level, such as O3")
    parser.add_argument("--program", help="Evaluate one program directory name")
    parser.add_argument("--run-timeout", type=int, default=5)
    parser.add_argument("--compile-timeout", type=int, default=60)
    parser.add_argument(
        "--replace",
        action="store_true",
        help="Replace an existing generated output directory",
    )
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def token_similarity(left: bytes, right: bytes) -> float:
    """Normalized token-level Levenshtein similarity in [0, 1]."""
    a = re.findall(rb"\S+", left)
    b = re.findall(rb"\S+", right)
    if not a and not b:
        return 1.0
    if not a or not b:
        return 0.0
    if len(a) > len(b):
        a, b = b, a
    previous = list(range(len(a) + 1))
    for row, token_b in enumerate(b, start=1):
        current = [row]
        for column, token_a in enumerate(a, start=1):
            current.append(
                min(
                    current[-1] + 1,
                    previous[column] + 1,
                    previous[column - 1] + (token_a != token_b),
                )
            )
        previous = current
    distance = previous[-1]
    return 1.0 - distance / max(len(a), len(b))


def run_command(command: list[str], timeout: int) -> subprocess.CompletedProcess[bytes]:
    try:
        return subprocess.run(command, capture_output=True, timeout=timeout, check=False)
    except subprocess.TimeoutExpired as error:
        return subprocess.CompletedProcess(
            command,
            124,
            error.stdout or b"",
            (error.stderr or b"") + b"\nevaluator timeout\n",
        )


def write_bytes(path: Path, value: bytes) -> None:
    path.write_bytes(value)


def write_tsv(path: Path, columns: list[str], rows: Iterable[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def write_case_markdown(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8") as stream:
        stream.write("# Per-case Codex objdump metrics\n\n")
        stream.write(
            "`Output similarity` is normalized token-level Levenshtein "
            "similarity and is diagnostic only. `Behavior pass` requires an "
            "unmodified compile, exit status 0, and byte-exact oracle stdout.\n\n"
        )
        stream.write(
            "| Optimization | Program | Valid | Clean trace | Compiles | "
            "Exits 0 | Behavior pass | Output similarity | Model time (s) |\n"
        )
        stream.write("|---|---|---:|---:|---:|---:|---:|---:|---:|\n")
        for row in rows:
            stream.write(
                f"| {row['optimization']} | {row['program']} | "
                f"{row['response_valid']} | {row['trace_clean']} | "
                f"{row['compile_ok']} | {row['run_ok']} | "
                f"{row['reexecutability']} | "
                f"{float(row['stdout_token_similarity']):.3f} | "
                f"{float(row['model_wall_seconds']):.1f} |\n"
            )


def expected_stdout_path(repo: Path, row: dict[str, str]) -> Path:
    suite_root = {
        "result-only": "artifacts_result_only",
        "readable": "artifacts",
    }.get(row["suite"])
    if suite_root is None:
        raise ValueError(f"no stdout oracle mapping for suite {row['suite']!r}")
    return (
        repo
        / suite_root
        / row["compiler"]
        / row["optimization"]
        / row["program"]
        / "stripped.stdout"
    )


def percent(count: int, total: int) -> str:
    return f"{100.0 * count / total:.1f}%" if total else "n/a"


def main() -> int:
    args = parse_args()
    repo = Path.cwd().resolve()
    manifest = args.manifest.resolve()
    output = args.output.resolve()

    if not manifest.is_file():
        raise SystemExit(f"manifest not found: {manifest}")
    if args.run_timeout < 1 or args.compile_timeout < 1:
        raise SystemExit("timeouts must be positive")
    if shutil.which("docker") is None:
        raise SystemExit("docker is required")
    image_check = subprocess.run(
        ["docker", "image", "inspect", args.image],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if image_check.returncode != 0:
        raise SystemExit(f"Docker image not found: {args.image}")

    if output.exists():
        if not args.replace:
            raise SystemExit(f"output already exists (use --replace): {output}")
        shutil.rmtree(output)
    output.mkdir(parents=True)
    details_root = output / "details"
    details_root.mkdir()

    with manifest.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        required = {
            "status",
            "track",
            "trial",
            "model",
            "reasoning",
            "recovered_sha256",
            "wall_seconds",
            "run_path",
            "suite",
            "compiler",
            "optimization",
            "program",
        }
        if reader.fieldnames is None or not required.issubset(reader.fieldnames):
            raise SystemExit("manifest does not use the organized v2 schema")
        selected = [
            row
            for row in reader
            if row.get("case_id")
            and row["track"] == args.track
            and row["trial"] == args.trial
            and (args.model is None or row["model"] == args.model)
            and (
                args.optimization is None
                or row["optimization"] == args.optimization
            )
            and (args.program is None or row["program"] == args.program)
        ]

    if not selected:
        raise SystemExit("no manifest rows matched the requested condition")
    run_paths = [row["run_path"] for row in selected]
    if len(run_paths) != len(set(run_paths)):
        raise SystemExit("selected manifest rows contain duplicate run paths")

    metrics: list[dict[str, object]] = []
    for index, row in enumerate(selected, start=1):
        run_dir = repo / "codex_objdump_runs" / row["run_path"]
        recovered = run_dir / "recovered.c"
        prohibited = run_dir / "prohibited-tool-events.txt"
        detail_rel = (
            Path(row["optimization"])
            / row["program"]
            / row["track"]
            / row["trial"]
        )
        detail_dir = details_root / detail_rel
        detail_dir.mkdir(parents=True)

        response_valid = (
            row["status"] == "COMPLETE"
            and recovered.is_file()
            and recovered.stat().st_size > 0
            and sha256(recovered) == row["recovered_sha256"]
        )
        trace_clean = prohibited.is_file() and prohibited.stat().st_size == 0
        compile_ok = False
        compile_warnings = 0
        run_ok = False
        exit_code = "not-run"
        stderr_empty = False
        stdout_exact = False
        similarity = 0.0

        if response_valid:
            candidate_source = detail_dir / "recovered.c"
            shutil.copyfile(recovered, candidate_source)
            compile_command = [
                "docker",
                "run",
                "--rm",
                "--platform",
                "linux/amd64",
                "--pull",
                "never",
                "--network",
                "none",
                "--cpus",
                "1",
                "--memory",
                "512m",
                "--pids-limit",
                "64",
                "--cap-drop",
                "ALL",
                "--security-opt",
                "no-new-privileges",
                "-v",
                f"{detail_dir}:/candidate:rw",
                "-w",
                "/candidate",
                args.image,
                "gcc",
                "-std=gnu11",
                "-O0",
                "-Wall",
                "-Wextra",
                "-Wno-unused-function",
                "recovered.c",
                "-o",
                "candidate.elf",
            ]
            compile_result = run_command(compile_command, args.compile_timeout)
            write_bytes(detail_dir / "compile.stdout", compile_result.stdout)
            write_bytes(detail_dir / "compile.stderr", compile_result.stderr)
            (detail_dir / "compile-exit-code.txt").write_text(
                f"{compile_result.returncode}\n", encoding="ascii"
            )
            compile_ok = compile_result.returncode == 0
            compile_warnings = len(re.findall(rb"\bwarning:", compile_result.stderr))

            candidate_elf = detail_dir / "candidate.elf"
            if compile_ok and candidate_elf.is_file():
                os.chmod(candidate_elf, 0o755)
                run_command_line = [
                    "docker",
                    "run",
                    "--rm",
                    "--platform",
                    "linux/amd64",
                    "--pull",
                    "never",
                    "--network",
                    "none",
                    "--read-only",
                    "--tmpfs",
                    "/tmp:rw,noexec,nosuid,size=16m",
                    "--cpus",
                    "1",
                    "--memory",
                    "256m",
                    "--pids-limit",
                    "64",
                    "--cap-drop",
                    "ALL",
                    "--security-opt",
                    "no-new-privileges",
                    "--user",
                    "65534:65534",
                    "-v",
                    f"{candidate_elf}:/candidate/program:ro",
                    "-w",
                    "/tmp",
                    args.image,
                    "timeout",
                    "--signal=KILL",
                    f"{args.run_timeout}s",
                    "/candidate/program",
                ]
                run_result = run_command(
                    run_command_line, args.run_timeout + 15
                )
                write_bytes(detail_dir / "candidate.stdout", run_result.stdout)
                write_bytes(detail_dir / "candidate.stderr", run_result.stderr)
                (detail_dir / "run-exit-code.txt").write_text(
                    f"{run_result.returncode}\n", encoding="ascii"
                )
                exit_code = str(run_result.returncode)
                run_ok = run_result.returncode == 0
                stderr_empty = len(run_result.stderr) == 0

                oracle_path = expected_stdout_path(repo, row)
                if not oracle_path.is_file():
                    raise SystemExit(f"stdout oracle missing: {oracle_path}")
                oracle_stdout = oracle_path.read_bytes()
                stdout_exact = run_ok and run_result.stdout == oracle_stdout
                similarity = (
                    token_similarity(run_result.stdout, oracle_stdout) if run_ok else 0.0
                )

        reexecutability = compile_ok and run_ok and stdout_exact
        metric: dict[str, object] = {
            "program": row["program"],
            "optimization": row["optimization"],
            "track": row["track"],
            "trial": row["trial"],
            "model": row["model"],
            "reasoning": row["reasoning"],
            "response_valid": int(response_valid),
            "trace_clean": int(trace_clean),
            "compile_ok": int(compile_ok),
            "compile_warnings": compile_warnings,
            "run_ok": int(run_ok),
            "exit_code": exit_code,
            "stderr_empty": int(stderr_empty),
            "stdout_exact": int(stdout_exact),
            "stdout_token_similarity": f"{similarity:.4f}",
            "reexecutability": int(reexecutability),
            "model_wall_seconds": row["wall_seconds"],
            "run_path": row["run_path"],
            "detail_path": str(detail_rel),
        }
        metrics.append(metric)
        print(
            f"[{index:02d}/{len(selected):02d}] {row['optimization']} "
            f"{row['program']}: compile={int(compile_ok)} "
            f"run={int(run_ok)} exact={int(stdout_exact)}"
        )

    metrics.sort(key=lambda item: (str(item["optimization"]), str(item["program"])))
    write_tsv(output / "case_metrics.tsv", CASE_COLUMNS, metrics)
    write_case_markdown(output / "case_metrics.md", metrics)

    summary_rows: list[dict[str, object]] = []
    optimizations = sorted({str(row["optimization"]) for row in metrics})
    for optimization in optimizations + ["ALL"]:
        group = (
            metrics
            if optimization == "ALL"
            else [row for row in metrics if row["optimization"] == optimization]
        )
        total = len(group)
        summary_rows.append(
            {
                "optimization": optimization,
                "cases": total,
                "valid_response": percent(
                    sum(int(row["response_valid"]) for row in group), total
                ),
                "clean_trace": percent(
                    sum(int(row["trace_clean"]) for row in group), total
                ),
                "compiles": percent(sum(int(row["compile_ok"]) for row in group), total),
                "runs_exit_0": percent(sum(int(row["run_ok"]) for row in group), total),
                "reexecutability": percent(
                    sum(int(row["reexecutability"]) for row in group), total
                ),
                "mean_stdout_token_similarity": f"{statistics.fmean(float(row['stdout_token_similarity']) for row in group):.3f}",
                "mean_model_wall_seconds": f"{statistics.fmean(float(row['model_wall_seconds']) for row in group):.1f}",
            }
        )

    summary_columns = [
        "optimization",
        "cases",
        "valid_response",
        "clean_trace",
        "compiles",
        "runs_exit_0",
        "reexecutability",
        "mean_stdout_token_similarity",
        "mean_model_wall_seconds",
    ]
    write_tsv(output / "summary_by_optimization.tsv", summary_columns, summary_rows)

    with (output / "summary.md").open("w", encoding="utf-8") as stream:
        stream.write("# Codex objdump evaluation\n\n")
        stream.write(
            f"Condition: `{args.track}`, `{args.trial}`; {len(metrics)} cases. "
            "Candidates were compiled without repair and executed in isolated, "
            "network-disabled containers.\n\n"
        )
        stream.write(
            "Re-executability requires compilation, exit status 0, and byte-for-byte "
            "agreement with the recorded oracle stdout. Token similarity is a "
            "diagnostic partial score, not functional equivalence.\n\n"
        )
        if args.track == "asm-only":
            stream.write(
                "This track omits `.rodata`; therefore exact recovery of output "
                "strings and initialized data is generally underdetermined. Treat "
                "the behavior and similarity columns as source-recovery diagnostics, "
                "not as type-recovery scores.\n\n"
            )
        stream.write("| Optimization | N | Valid | Clean trace | Compiles | Exits 0 | Re-executable | Token similarity | Mean model time (s) |\n")
        stream.write("|---|---:|---:|---:|---:|---:|---:|---:|---:|\n")
        for row in summary_rows:
            stream.write(
                f"| {row['optimization']} | {row['cases']} | "
                f"{row['valid_response']} | {row['clean_trace']} | "
                f"{row['compiles']} | {row['runs_exit_0']} | "
                f"{row['reexecutability']} | "
                f"{row['mean_stdout_token_similarity']} | "
                f"{row['mean_model_wall_seconds']} |\n"
            )

        compile_failures = [row for row in metrics if not int(row["compile_ok"])]
        if compile_failures:
            stream.write("\n## Compile failures\n\n")
            for row in compile_failures:
                stream.write(
                    f"- `{row['optimization']}/{row['program']}`: see "
                    f"`details/{row['detail_path']}/compile.stderr`.\n"
                )

    print(f"wrote metric tables to: {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
