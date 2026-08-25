# Benchmark tooling and documentation

New users should begin with the repository-level [`README.md`](../README.md).
It contains the complete first-run path from building binaries through one
isolated Codex trial.

## Commands

| Task | Command |
|---|---|
| Build readable-output binaries at O0-O3 | `./benchmark/build_matrix.sh` |
| Build result-only binaries at O0-O3 | `./result_only/build_matrix.sh` |
| Validate Codex setup without a model call | `./benchmark/run_codex_objdump.sh --check` |
| Generate objdump evidence without a model call | `./benchmark/run_codex_objdump.sh --prepare-only <stripped.elf>` |
| Run one Codex recovery trial | `./benchmark/run_codex_objdump.sh <stripped.elf>` |
| Evaluate organized recovered-C runs | `./benchmark/evaluate_codex_runs.py --replace` |
| Show all runner options | `./benchmark/run_codex_objdump.sh --help` |

All commands are intended to be run from the repository root.

## Files in this directory

| File | Purpose |
|---|---|
| [`CODEX_OBJDUMP.md`](CODEX_OBJDUMP.md) | Detailed setup, batch execution, isolation guarantees, outputs, and troubleshooting |
| [`METHODOLOGY.md`](METHODOLOGY.md) | Experimental design, evidence conditions, scoring, and validity caveats |
| [`EVALUATION.md`](EVALUATION.md) | Metric definitions, evaluation commands, and the type-score table |
| [`prompt-template.md`](prompt-template.md) | Binary-input and structural type-recovery prompt schemas |
| [`codex-objdump-prompt.txt`](codex-objdump-prompt.txt) | Exact fixed prompt used by the objdump-to-C pipeline |
| [`codex-objdump.conf.example`](codex-objdump.conf.example) | Shareable pipeline configuration template |
| `codex-objdump.conf` | Local ignored configuration; model ID and dedicated Codex home |
| [`run_codex_objdump.sh`](run_codex_objdump.sh) | Isolated single-case and batch Codex runner |
| [`organize_codex_runs.sh`](organize_codex_runs.sh) | Preview or migrate legacy hash-only run directories to the browsable hierarchy |
| [`evaluate_codex_runs.py`](evaluate_codex_runs.py) | Sandboxed compilation/execution evaluator and metric-table generator |
| [`build_matrix.sh`](build_matrix.sh) | Compiler/optimization/stripping matrix builder |
| [`Dockerfile`](Dockerfile) | Pinned Ubuntu build and GNU objdump environment |

## Generated directories

| Directory | Contents | Model-visible? |
|---|---|---|
| `artifacts/` | Readable-suite ELF variants, DWARF, stdout, and manifest | No |
| `artifacts_result_only/` | Result-only ELF variants, DWARF, stdout, and manifest | No |
| `codex_objdump_runs/` | Organized runs under `runs/<suite>/<compiler>/<optimization>/<program>/<track>/<trial>`, plus evaluator metadata | Only `program.objdump.txt` is sent through stdin |
| `evaluation/` | Generated summary/per-case metric tables and isolated compile/runtime logs | No |

These generated directories are ignored by version control. They contain
ground truth or cross-run information and must never be mounted wholesale into
a model-visible workspace.
