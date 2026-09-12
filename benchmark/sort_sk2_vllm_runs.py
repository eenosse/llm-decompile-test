#!/usr/bin/env python3
"""Create a readable sorted view of SK2 vLLM output files."""

from __future__ import annotations

import argparse
import os
import re
import shutil
from pathlib import Path


def load_program_names(repo_root: Path) -> list[str]:
    names = []
    for path in sorted((repo_root / "result_only").glob("[0-9][0-9]_*.c")):
        names.append(path.stem)
    return sorted(names, key=len, reverse=True)


def parse_output_path(path: Path, run_root: Path, program_names: list[str]) -> dict[str, str] | None:
    try:
        rel = path.relative_to(run_root)
    except ValueError:
        return None
    parts = rel.parts
    if len(parts) < 5:
        return None

    case_id = parts[0]
    trial = parts[1]
    opt = parts[-2]
    filename = path.name

    match = re.match(r"(?P<index>.+)_(?P<address>[0-9a-fA-F]+)_(?P<opt>O[0-3])\.c$", filename)
    if not match:
        return None
    if match.group("opt") != opt:
        return None

    index = match.group("index")
    program = next((name for name in program_names if index.startswith(name + "_") or index == name), None)
    if program is None:
        pieces = index.split("_")
        program = "_".join(pieces[:3]) if len(pieces) >= 3 else index

    function = index[len(program) + 1 :] if index.startswith(program + "_") else index
    return {
        "case_id": case_id,
        "trial": trial,
        "opt": opt,
        "program": program,
        "function": function,
        "address": match.group("address"),
        "index": index,
    }


def safe_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]+", "_", value)


def rel_symlink(target: Path, link: Path) -> None:
    link.parent.mkdir(parents=True, exist_ok=True)
    if link.exists() or link.is_symlink():
        link.unlink()
    link.symlink_to(os.path.relpath(target, link.parent))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-root", default="sk2_vllm_runs")
    parser.add_argument("--output", default="sorted_sk2_vllm_runs")
    parser.add_argument("--copy", action="store_true", help="Copy files instead of creating symlinks")
    args = parser.parse_args()

    repo_root = Path.cwd()
    run_root = (repo_root / args.run_root).resolve()
    output_root = repo_root / args.output
    program_names = load_program_names(repo_root)

    if not run_root.is_dir():
        raise SystemExit(f"run root not found: {run_root}")
    if output_root.exists():
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True)

    rows = []
    for src in sorted(run_root.rglob("*.c")):
        if "sk2_output" not in src.parts or src.parent.name not in {"O0", "O1", "O2", "O3"}:
            continue
        meta = parse_output_path(src.resolve(), run_root, program_names)
        if meta is None:
            continue

        dst_name = f"{safe_name(meta['function'])}_{meta['address']}.c"
        dst = output_root / meta["trial"] / meta["opt"] / meta["program"] / dst_name
        if args.copy:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
        else:
            rel_symlink(src, dst)

        log_src = Path(str(src) + ".log")
        log_dst = Path(str(dst) + ".log")
        if log_src.exists():
            if args.copy:
                shutil.copy2(log_src, log_dst)
            else:
                rel_symlink(log_src, log_dst)

        rows.append({**meta, "source": str(src), "sorted": str(dst)})

    index_path = output_root / "index.tsv"
    with index_path.open("w", encoding="utf-8") as handle:
        handle.write("trial\topt\tprogram\tfunction\taddress\tcase_id\tsorted\tsource\n")
        for row in sorted(rows, key=lambda item: (item["trial"], item["opt"], item["program"], item["function"])):
            handle.write(
                "\t".join(
                    [
                        row["trial"],
                        row["opt"],
                        row["program"],
                        row["function"],
                        row["address"],
                        row["case_id"],
                        row["sorted"],
                        row["source"],
                    ]
                )
                + "\n"
            )

    print(f"sorted {len(rows)} files into {output_root}")
    print(f"index: {index_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
