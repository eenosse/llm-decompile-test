#!/usr/bin/env python3
"""Run SK2Decompile two-phase inference with llama.cpp GGUF models."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

OPTS = ("O0", "O1", "O2", "O3")


def load_samples(path: Path) -> list[dict[str, Any]]:
    if path.suffix == ".jsonl":
        samples = []
        with path.open("r", encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if line:
                    samples.append(json.loads(line))
        return samples
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def run_generation(
    llama_cli: str,
    model_ref: str,
    prompt: str,
    max_tokens: int,
    temperature: float,
    extra_args: list[str],
) -> str:
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", suffix=".prompt", delete=False) as handle:
        handle.write(prompt)
        prompt_path = handle.name
    try:
        cmd = [
            llama_cli,
            "-hf",
            model_ref,
            "-f",
            prompt_path,
            "-n",
            str(max_tokens),
            "--temp",
            str(temperature),
            "--no-display-prompt",
            "--no-conversation",
            "--simple-io",
            "--device",
            "none",
            "--gpu-layers",
            "0",
            "--batch-size",
            "256",
            "--ubatch-size",
            "128",
            *extra_args,
        ]
        proc = subprocess.run(
            cmd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=sys.stderr,
            check=False,
        )
    finally:
        os.unlink(prompt_path)

    stdout = strip_echoed_prompt(proc.stdout, prompt)
    if proc.returncode != 0:
        raise RuntimeError(
            f"llama-cli failed with exit code {proc.returncode}\n"
            f"command: {' '.join(cmd)}"
        )
    return clean_generated_text(stdout)


def strip_echoed_prompt(text: str, prompt: str) -> str:
    if text.startswith(prompt):
        return text[len(prompt) :]
    return text


def clean_generated_text(text: str) -> str:
    replacements = {
        "\u0120": " ",
        "\u010a": "\n",
        "\u0109": "\t",
    }
    for marker, value in replacements.items():
        text = text.replace(marker, value)
    return repair_compact_c(text.strip())


def repair_compact_c(text: str) -> str:
    replacements = [
        (r"\bunsignedlonglong(?=[A-Za-z_*])", "unsigned long long"),
        (r"\bunsignedlong(?=[A-Za-z_*])", "unsigned long"),
        (r"\blonglong(?=[A-Za-z_*])", "long long"),
        (r"\bunsignedint(?=[A-Za-z_*])", "unsigned int"),
        (r"\bsignedint(?=[A-Za-z_*])", "signed int"),
        (r"\bconstchar(?=[A-Za-z_*])", "const char"),
        (r"\breturn([A-Za-z0-9_])", r"return \1"),
        (r"\bcase([0-9A-Za-z_]+)", r"case \1"),
        (r"\bsizeof([A-Za-z_])", r"sizeof \1"),
    ]
    for pattern, value in replacements:
        text = re.sub(pattern, value, text)

    type_words = (
        "void",
        "char",
        "short",
        "int",
        "float",
        "double",
        "long long",
        "long",
        "unsigned",
        "signed",
        "size_t",
        "uint8_t",
        "uint16_t",
        "uint32_t",
        "uint64_t",
        "int8_t",
        "int16_t",
        "int32_t",
        "int64_t",
        "node1",
        "_node_",
        "type1",
        "type2",
        "hashmap",
        "entry",
    )
    for type_word in sorted(type_words, key=len, reverse=True):
        escaped = re.escape(type_word)
        text = re.sub(rf"\b({escaped})([A-Za-z_][A-Za-z0-9_]*)(?=[,);=])", r"\1 \2", text)
        text = re.sub(rf"\b({escaped})([A-Za-z_][A-Za-z0-9_]*)(?=\()", r"\1 \2", text)

    text = re.sub(r"\b(if|for|while|switch)\(", r"\1 (", text)
    text = re.sub(r"\b(do|else)\{", r"\1 {", text)
    text = re.sub(r"\b(return)\s+([A-Za-z0-9_]+);", r"\1 \2;", text)
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r"\s*([{};])\s*", r"\1\n", text)
    return text.strip() + "\n"


def format_c_if_possible(code: str) -> str:
    clang_format = shutil.which("clang-format")
    if clang_format is None:
        return code
    try:
        proc = subprocess.run(
            [clang_format, "--style=Google"],
            input=code,
            text=True,
            capture_output=True,
            check=True,
            timeout=10,
        )
        return proc.stdout
    except Exception:
        return code


def strip_generated_name(code: str, original_func_name: str) -> str:
    func_name = code.split("(", 1)[0].split(" ")[-1].strip()
    if func_name.startswith("**"):
        func_name = func_name[2:]
    elif func_name.startswith("*"):
        func_name = func_name[1:]
    if not func_name:
        return code
    return code.replace(func_name, original_func_name)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-path", required=True)
    parser.add_argument("--model", required=True, help="llama.cpp -hf structure model reference")
    parser.add_argument("--recover-model", required=True, help="llama.cpp -hf identifier recovery model reference")
    parser.add_argument("--llama-cli", default="llama-cli")
    parser.add_argument("--llama-arg", action="append", default=[], help="Extra argument passed to every llama-cli call")
    parser.add_argument("--decompiler", default="ida_pseudo_norm")
    parser.add_argument("--output-path", required=True)
    parser.add_argument("--temperature", type=float, default=0.0)
    parser.add_argument("--max-new-tokens", type=int, default=4096)
    parser.add_argument("--limit", type=int, default=0, help="Only process the first N samples; 0 means all")
    parser.add_argument("--strip", type=int, default=1)
    args = parser.parse_args()

    dataset_path = Path(args.dataset_path)
    output_path = Path(args.output_path)
    samples = load_samples(dataset_path)
    if args.limit:
        samples = samples[: args.limit]

    if output_path.exists():
        shutil.rmtree(output_path)
    for opt in OPTS:
        (output_path / opt).mkdir(parents=True, exist_ok=True)

    before = "# This is the assembly code:\n"
    after = "\n# What is the source code?\n"
    before_recovery = "# This is the normalized code:\n"
    after_recovery = "\n# What is the source code?\n"

    model1_outputs = []
    for sample in samples:
        prompt = before + sample[args.decompiler].strip() + after
        sample["prompt_model1"] = prompt
        output = run_generation(
            args.llama_cli,
            args.model,
            prompt,
            args.max_new_tokens,
            args.temperature,
            args.llama_arg,
        )
        model1_outputs.append(output)

    for sample, output in zip(samples, model1_outputs):
        sample["gen_result_model1"] = output

    model2_outputs = []
    for sample in samples:
        prompt = before_recovery + sample["gen_result_model1"].strip() + after_recovery
        sample["prompt_model2"] = prompt
        output = run_generation(
            args.llama_cli,
            args.recover_model,
            prompt,
            args.max_new_tokens,
            args.temperature,
            args.llama_arg,
        )
        model2_outputs.append(output)

    for sample, output in zip(samples, model2_outputs):
        sample["gen_result_model2"] = output

    final_outputs = []
    if args.strip:
        for sample in samples:
            final = strip_generated_name(sample["gen_result_model2"], sample["func_name"])
            final = format_c_if_possible(clean_generated_text(final))
            sample["gen_result_model2_stripped"] = final
            final_outputs.append(final)
    else:
        final_outputs = [format_c_if_possible(clean_generated_text(sample["gen_result_model2"])) for sample in samples]

    for sample, final in zip(samples, final_outputs):
        opt = sample["opt"]
        language = sample["language"]
        index = sample["index"]
        save_path = output_path / opt / f"{index}_{opt}.{language}"
        save_path.write_text(final, encoding="utf-8")

        log_data = {
            "index": index,
            "opt": opt,
            "language": language,
            "func_name": sample["func_name"],
            "decompiler": args.decompiler,
            "input_asm": sample[args.decompiler].strip(),
            "prompt_model1": sample["prompt_model1"],
            "gen_result_model1": sample["gen_result_model1"],
            "prompt_model2": sample["prompt_model2"],
            "gen_result_model2": sample["gen_result_model2"],
            "final_result": final,
            "stripped": args.strip,
        }
        if "gen_result_model2_stripped" in sample:
            log_data["gen_result_model2_stripped"] = sample["gen_result_model2_stripped"]
        (Path(str(save_path) + ".log")).write_text(
            json.dumps(log_data, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )

    with (output_path / "inference_results.jsonl").open("w", encoding="utf-8") as handle:
        for sample in samples:
            handle.write(json.dumps(sample, ensure_ascii=False))
            handle.write("\n")

    opt_counts = {opt: 0 for opt in OPTS}
    for sample in samples:
        opt_counts[sample["opt"]] += 1

    stats_lines = [
        f"Total samples processed: {len(samples)}",
        f"Model path: {args.model}",
        f"Recovery model path: {args.recover_model}",
        f"Dataset path: {dataset_path}",
        f"Decompiler: {args.decompiler}",
        "Backend: llama.cpp",
        "Quantization: GGUF model-defined",
        f"Temperature: {args.temperature}",
        f"Max new tokens: {args.max_new_tokens}",
        f"Limit: {args.limit or 'all'}",
        f"Strip function names: {bool(args.strip)}",
        "",
        "Samples per optimization level:",
    ]
    stats_lines.extend(f"  {opt}: {count}" for opt, count in opt_counts.items())
    (output_path / "inference_stats.txt").write_text("\n".join(stats_lines) + "\n", encoding="utf-8")

    print(f"Inference completed: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
