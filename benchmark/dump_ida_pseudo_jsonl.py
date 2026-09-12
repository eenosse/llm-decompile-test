"""
Headless IDA/Hex-Rays helper that dumps one JSONL record per discovered
function.

Usage:
    idat64 -A -S"/path/to/dump_ida_pseudo_jsonl.py /path/to/out.jsonl" binary
"""

from __future__ import annotations

import json
import os
import sys
import traceback

import ida_auto
import ida_funcs
import ida_hexrays
import ida_pro
import idautils
import idc


def _output_path() -> str:
    if len(idc.ARGV) < 2:
        raise RuntimeError("output path argument missing")
    return os.path.abspath(idc.ARGV[1])


def _write_record(handle, record: dict) -> None:
    handle.write(json.dumps(record, ensure_ascii=False, sort_keys=True))
    handle.write("\n")


def main() -> None:
    try:
        output_path = _output_path()
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        ida_auto.auto_wait()

        if not ida_hexrays.init_hexrays_plugin():
            print("[dump_ida_pseudo_jsonl] Hex-Rays decompiler is unavailable", file=sys.stderr)
            ida_pro.qexit(1)
            return

        with open(output_path, "w", encoding="utf-8") as handle:
            for ea in idautils.Functions():
                name = ida_funcs.get_func_name(ea)
                record = {
                    "address": f"0x{ea:x}",
                    "name": name,
                    "status": "ok",
                    "pseudo": "",
                }
                try:
                    cfunc = ida_hexrays.decompile(ea)
                    record["pseudo"] = str(cfunc)
                except ida_hexrays.DecompilationFailure as exc:
                    record["status"] = "decompilation_failed"
                    record["error"] = str(exc)
                except Exception as exc:  # pragma: no cover - runs inside IDA
                    record["status"] = "error"
                    record["error"] = str(exc)
                    record["traceback"] = traceback.format_exc()
                _write_record(handle, record)

        ida_pro.qexit(0)
    except Exception as exc:  # pragma: no cover - runs inside IDA
        print(f"[dump_ida_pseudo_jsonl] {exc}", file=sys.stderr)
        traceback.print_exc(file=sys.stderr)
        ida_pro.qexit(1)


if __name__ == "__main__":
    main()
