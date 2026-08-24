# LLM type-recovery benchmark

This repository tests how well an LLM can recover C source and data types from
optimized, fully stripped x86-64 Linux binaries. The primary Codex condition
converts each ELF to fixed GNU objdump text before starting a fresh, isolated
model run.

If you are running the experiment for the first time, follow only the quick
start below. The remaining documents provide deeper protocol details.

## Quick start

Run every command from the repository root.

### 1. Install prerequisites

Required commands:

- Docker
- Codex CLI
- `jq`
- `rg` (ripgrep)

The build scripts use Docker to pin Ubuntu, GCC, binutils, and the target
architecture. The host compiler is not used for benchmark artifacts.

### 2. Build the stripped binaries

For the recommended reduced-semantic-output suite:

```sh
./result_only/build_matrix.sh
```

This builds all 20 programs at `-O0`, `-O1`, `-O2`, and `-O3`. The primary
model inputs are created under:

```text
artifacts_result_only/<compiler>/O<level>/<program>/stripped.elf
```

The readable-output suite is built separately with:

```sh
./benchmark/build_matrix.sh
```

Do not combine scores from the two suites.

### 3. Create isolated Codex authentication

Use a dedicated Codex home so normal user settings, skills, plugins, and prior
project context are not part of the experiment:

```sh
CODEX_HOME="$HOME/.codex-objdump-benchmark" codex login
```

Do not put this directory inside the repository. Its `auth.json` must be
treated like a password.

The runner keeps this home for authentication but places Codex's SQLite-backed
runtime state in a fresh temporary directory for each invocation. You do not
need to clear memory or state database files between benchmark runs. Existing
database files left by an older runner are no longer consulted; keep
`auth.json`, since deleting it logs the benchmark home out.

### 4. Configure the experiment once

If the local config does not exist:

```sh
cp benchmark/codex-objdump.conf.example benchmark/codex-objdump.conf
```

Edit `benchmark/codex-objdump.conf`:

```ini
MODEL=gpt-5.6-sol
BENCH_HOME=/absolute/path/to/.codex-objdump-benchmark

REASONING=high
TRACK=asm-only
TRIAL=trial-1
OUTPUT=codex_objdump_runs
```

The local config is ignored by version control. Do not store API keys or auth
tokens in it.

### 5. Check the setup without a model request

```sh
./benchmark/run_codex_objdump.sh --check
```

This validates the local config, authentication, required commands, disabled
tool policy, Docker image, Codex CLI, and GNU objdump. It does not send assembly
to a model.

### 6. Prepare one case without a model request

```sh
./benchmark/run_codex_objdump.sh \
  --prepare-only \
  artifacts_result_only/x86_64-linux-gnu-11.4.0/O3/01_basic_struct/stripped.elf
```

Inspect `program.objdump.txt` in the generated case directory. Its input name
must be the generic `/input/program.elf`; source paths and test names must not
appear.

### 7. Run one Codex trial

```sh
./benchmark/run_codex_objdump.sh \
  artifacts_result_only/x86_64-linux-gnu-11.4.0/O3/01_basic_struct/stripped.elf
```

The configured `TRACK` and `TRIAL` are used automatically. A successful run has
`COMPLETE` in `status.txt` and produces `recovered.c`.

To retry without overwriting an earlier trial:

```sh
./benchmark/run_codex_objdump.sh \
  --trial trial-2 \
  artifacts_result_only/x86_64-linux-gnu-11.4.0/O3/01_basic_struct/stripped.elf
```

Never manually repair `recovered.c`; retain the raw response for evaluation.

## Choose the evidence track

The configured `TRACK` is one of:

| Track | Evidence visible to Codex | Recommended use |
|---|---|---|
| `asm-only` | `objdump -d` executable-section disassembly | Type and control-flow recovery |
| `objdump-complete` | Headers, section bytes, and disassembly | Whole-program source recovery |

`asm-only` omits `.rodata` contents, so exact recovery of strings and initialized
global data is generally impossible. Report the tracks separately.

## Where files belong

```text
.
├── 01_basic_struct.c ... 20_combined_stress.c  readable source suite
├── result_only/                              reduced-output source wrappers
├── benchmark/                                build and Codex pipeline tooling
├── artifacts/                                generated readable-suite ELFs
├── artifacts_result_only/                    generated result-only ELFs
└── codex_objdump_runs/
    ├── manifest.tsv                          evaluator run index
    ├── metadata/                             migration/audit metadata
    └── runs/result-only/<compiler>/O<level>/<program>/<track>/<trial>/
                                               Codex evidence and results
```

Generated directories are evaluator-only. Never make the sources, artifact
manifests, expected stdout, DWARF files, or sibling optimization levels visible
to a model run.

## Which document should I read?

| Goal | Document |
|---|---|
| Run the project for the first time | This file |
| Understand files and available commands | [`benchmark/README.md`](benchmark/README.md) |
| Run or troubleshoot Codex objdump experiments | [`benchmark/CODEX_OBJDUMP.md`](benchmark/CODEX_OBJDUMP.md) |
| Understand benchmark methodology and scoring | [`benchmark/METHODOLOGY.md`](benchmark/METHODOLOGY.md) |
| See binary-input and type-output prompt schemas | [`benchmark/prompt-template.md`](benchmark/prompt-template.md) |
| Understand the result-only C wrappers | [`result_only/README.md`](result_only/README.md) |

The Codex automation uses `codex exec`, ephemeral sessions, stdin evidence, and
JSONL traces following OpenAI's
[non-interactive-mode documentation](https://learn.chatgpt.com/docs/non-interactive-mode).
