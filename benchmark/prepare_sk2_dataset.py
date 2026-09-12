#!/usr/bin/env python3
"""Convert IDA pseudocode JSONL into SK2Decompile inference input JSON."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


TYPEDEF_MAP = {
    "cpu_set_t": "int",
    "nl_item": "int",
    "__time_t": "int",
    "__mode_t": "unsigned short",
    "__off64_t": "long long",
    "__blksize_t": "long",
    "__ino_t": "unsigned long",
    "__blkcnt_t": "unsigned long long",
    "__syscall_slong_t": "long",
    "__ssize_t": "long int",
    "wchar_t": "unsigned short int",
    "wctype_t": "unsigned short int",
    "__int64": "long long",
    "__int32": "int",
    "__int16": "short",
    "__int8": "char",
    "_QWORD": "uint64_t",
    "_OWORD": "long double",
    "_DWORD": "uint32_t",
    "size_t": "unsigned int",
    "_BYTE": "uint8_t",
    "_TBYTE": "uint16_t",
    "_BOOL8": "uint8_t",
    "gcc_va_list": "va_list",
    "_WORD": "unsigned short",
    "_BOOL4": "int",
    "__va_list_tag": "va_list",
    "_IO_FILE": "FILE",
    "DIR": "int",
    "__fsword_t": "long",
    "__kernel_ulong_t": "int",
    "cc_t": "int",
    "speed_t": "int",
    "fd_set": "int",
    "__suseconds_t": "int",
    "_UNKNOWN": "void",
    "__sighandler_t": "void (*)(int)",
    "__compar_fn_t": "int (*)(const void *, const void *)",
}


def strip_empty(code: str) -> str:
    return "\n".join(line for line in code.splitlines() if line.strip())


def good_func(func: str, min_body_lines: int = 3, max_body_lines: int = 300) -> bool:
    body = func.split("{", 1)[1] if "{" in func else func
    meaningful = sum(1 for line in body.splitlines() if len(line.strip()) >= 3)
    return min_body_lines < meaningful < max_body_lines


def format_with_clang(func: str, clang_format: str, style: str = "Google") -> str | None:
    if not func.strip():
        return None
    try:
        proc = subprocess.run(
            [clang_format, f"--style={style}"],
            input=func,
            text=True,
            capture_output=True,
            check=True,
            timeout=15,
        )
        return proc.stdout
    except Exception:
        return None


def hex_to_dec(text: str) -> str:
    pattern = re.compile(r"\b(0x[0-9a-fA-F]+)([uUlL]{1,3})?\b")

    def convert(match: re.Match[str]) -> str:
        suffix = match.group(2) or ""
        return str(int(match.group(1), 16)) + suffix

    return pattern.sub(convert, text)


def remove_keywords(text: str) -> str:
    patterns = [
        r"\b__fastcall\b",
        r"\b__cdecl\b",
        r"\b__ptr32\b",
        r"\b__noreturn\s+noreturn\b",
    ]
    return re.sub("|".join(patterns), "", text)


def replace_typedefs(text: str) -> str:
    for alias, original in TYPEDEF_MAP.items():
        text = re.sub(rf"\b{re.escape(alias)}\b", original, text)
    return text


def remove_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def normalize_pseudo(text: str, clang_format: str, keep_bad_format: bool) -> str:
    processed = replace_typedefs(remove_keywords(hex_to_dec(remove_comments(text))))
    formatted = format_with_clang(processed, clang_format)
    if formatted is None:
        if keep_bad_format:
            return strip_empty(processed)
        return ""
    cleaned = strip_empty(formatted)
    if not good_func(cleaned):
        return ""
    return cleaned


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    records = []
    with path.open("r", encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, 1):
            line = line.strip()
            if not line:
                continue
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_number}: invalid JSONL: {exc}") from exc
    return records


def sanitize(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9._-]+", "_", value.strip())
    return value.strip("._-") or "function"


def derive_opt(binary_path: str, fallback: str) -> str:
    for part in Path(binary_path).parts:
        if re.fullmatch(r"O[0-3]", part):
            return part
    return fallback


def load_allowed_symbols(path: str) -> dict[str, str]:
    if not path:
        return {}
    symbol_path = Path(path)
    if not symbol_path.exists():
        raise ValueError(f"symbol binary not found: {symbol_path}")
    try:
        proc = subprocess.run(
            ["nm", "-n", str(symbol_path)],
            text=True,
            capture_output=True,
            check=True,
            timeout=15,
        )
    except Exception as exc:
        raise ValueError(f"failed to read symbols from {symbol_path}: {exc}") from exc

    ignored = {
        "_init",
        "_start",
        "_dl_relocate_static_pie",
        "deregister_tm_clones",
        "register_tm_clones",
        "__do_global_dtors_aux",
        "frame_dummy",
        "_fini",
    }
    allowed = {}
    for line in proc.stdout.splitlines():
        parts = line.split()
        if len(parts) != 3:
            continue
        address, kind, name = parts
        if kind.lower() not in {"t"}:
            continue
        if name in ignored or name.startswith("__"):
            continue
        allowed[f"0x{address.lower().lstrip('0') or '0'}"] = name
    return allowed


def build_dataset(args: argparse.Namespace) -> tuple[list[dict[str, Any]], dict[str, int]]:
    records = load_jsonl(Path(args.ida_jsonl))
    allowed_symbols = load_allowed_symbols(args.symbol_binary)
    opt = args.opt or derive_opt(args.binary_path, "O0")
    if opt not in {"O0", "O1", "O2", "O3"}:
        raise ValueError(f"unsupported optimization level for SK2 output layout: {opt}")

    binary_stem = sanitize(Path(args.binary_path).parent.name or Path(args.binary_path).stem)
    dataset = []
    stats = {"input": len(records), "ok": 0, "failed": 0, "filtered": 0, "symbol_filtered": 0}

    for record in records:
        if record.get("status") != "ok":
            stats["failed"] += 1
            continue
        record_address = str(record.get("address") or "").lower()
        symbol_name = allowed_symbols.get(record_address)
        if allowed_symbols and symbol_name is None:
            stats["symbol_filtered"] += 1
            continue
        pseudo = str(record.get("pseudo") or "")
        norm = normalize_pseudo(pseudo, args.clang_format, args.keep_bad_format)
        if not norm:
            stats["filtered"] += 1
            continue

        func_name = sanitize(symbol_name or str(record.get("name") or "sub"))
        address = sanitize(str(record.get("address") or f"{len(dataset):04d}").replace("0x", ""))
        index = sanitize(f"{binary_stem}_{func_name}_{address}")
        dataset.append(
            {
                "index": index,
                "func_name": func_name,
                "opt": opt,
                "language": args.language,
                "ida_pseudo": pseudo,
                "ida_pseudo_norm": norm,
                "ida_strip_pseudo": pseudo,
                "ida_strip_pseudo_norm": norm,
                "binary_path": args.binary_path,
                "ida_address": record.get("address", ""),
            }
        )
        stats["ok"] += 1

    return dataset, stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ida-jsonl", required=True, help="IDA JSONL dump path")
    parser.add_argument("--binary-path", required=True, help="Original binary path for metadata")
    parser.add_argument("--output-json", required=True, help="SK2 dataset JSON output path")
    parser.add_argument("--stats", required=True, help="Text stats output path")
    parser.add_argument("--opt", default="", help="Optimization level O0-O3; derived from path if omitted")
    parser.add_argument("--language", default="c")
    parser.add_argument("--clang-format", default="clang-format")
    parser.add_argument("--symbol-binary", default="", help="Optional unstripped/nodebug ELF used to keep source-level functions only")
    parser.add_argument("--keep-bad-format", action="store_true")
    args = parser.parse_args()

    try:
        dataset, stats = build_dataset(args)
    except Exception as exc:
        print(f"prepare_sk2_dataset.py: {exc}", file=sys.stderr)
        return 1

    output_path = Path(args.output_json)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(dataset, indent=2, ensure_ascii=False), encoding="utf-8")

    stats_path = Path(args.stats)
    stats_path.parent.mkdir(parents=True, exist_ok=True)
    stats_path.write_text(
        "\n".join(
            [
                f"input records: {stats['input']}",
                f"normalized records: {stats['ok']}",
                f"failed IDA records: {stats['failed']}",
                f"symbol-filtered records: {stats['symbol_filtered']}",
                f"filtered records: {stats['filtered']}",
                f"output json: {output_path}",
            ]
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"prepared {stats['ok']} SK2 sample(s): {output_path}")
    return 0 if dataset else 1


if __name__ == "__main__":
    raise SystemExit(main())
