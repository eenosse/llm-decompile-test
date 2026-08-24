# Codex objdump-only pipeline

For a first-time installation, follow the repository-level
[`README.md`](../README.md) first. This document is the detailed operational and
troubleshooting reference.

This pipeline evaluates source recovery when Codex receives a fixed textual
`objdump` artifact rather than an ELF or evaluator-supplied pseudocode. Each
binary is processed in a fresh non-interactive Codex thread. The default mode
pipes all evidence through stdin and disables the shell, web search, plugins,
apps, subagents, image tools, hooks, and persisted or injected Codex memories.

The implementation uses `codex exec`, `--ephemeral`, a temporary per-invocation
SQLite directory, JSONL tracing, and `--output-last-message` as documented in OpenAI's
[non-interactive mode guide](https://learn.chatgpt.com/docs/non-interactive-mode).
The memory, history, web-search, shell-tool, and multi-agent settings come from
the official [Codex configuration reference](https://developers.openai.com/codex/config-file/config-reference).

## What this measures

Two evidence tracks are supported:

- `asm-only` (default): `objdump -d`. Codex sees executable-section
  disassembly but not the contents of `.rodata` or other data sections. Use it
  primarily for control-flow and type-recovery measurements.
- `objdump-complete`: `objdump -x`, `objdump -s`, and `objdump -d`. Codex also
  sees headers and section contents, making whole-program source recovery less
  underdetermined. It is still a textual-objdump benchmark, not raw-binary
  access.

Do not merge results from these tracks. In particular, failure to reconstruct
a string literal from `asm-only` evidence is missing evidence, not necessarily
a model error.

## One-time setup

Build the benchmark artifacts first:

```sh
./result_only/build_matrix.sh
```

Create a dedicated Codex home. It should contain only benchmark authentication;
do not install skills, plugins, hooks, MCP servers, or `AGENTS.md` there:

```sh
CODEX_HOME="$HOME/.codex-objdump-benchmark" codex login
```

Treat `auth.json` inside that directory like a password. Do not put this Codex
home inside the repository or commit it.

Create the ignored local config from the template if it is not already present,
then edit it once:

```sh
cp benchmark/codex-objdump.conf.example benchmark/codex-objdump.conf
```

```ini
MODEL=YOUR_FIXED_MODEL_ID
BENCH_HOME=/absolute/path/to/.codex-objdump-benchmark
```

Pin an exact model ID rather than a moving `latest` alias. The local config is
excluded from version control; `codex-objdump.conf.example` is the shareable
template. Do not place API keys or authentication tokens in either file.

Configuration precedence is command line, then environment variables, then the
config file, then built-in defaults. Use `--config FILE` for another experiment
configuration or `--no-config` to ignore the default file.

Validate everything without making a model request:

```sh
./benchmark/run_codex_objdump.sh --check
```

A valid setup prints the resolved model, reasoning level, evidence track,
dedicated Codex home, Codex CLI version, GNU objdump version, and Docker image.
Run this after changing the config or upgrading the Codex CLI.

The dedicated home remains on disk because it contains the OAuth login. Codex
may also keep non-context files there, such as its installation identifier and
model cache. The runner redirects `sqlite_home` to a new temporary directory
for every invocation and removes it on exit. Therefore rerunning a case does
not reuse SQLite-backed memory, job, queue, goal, or log state. Do not delete
`auth.json`; doing so requires another login.

## Prepare without spending a model call

This generates the canonical evidence with the pinned Ubuntu image and records
its hashes and tool versions:

```sh
./benchmark/run_codex_objdump.sh \
  --prepare-only \
  artifacts_result_only/x86_64-linux-gnu-11.4.0/O3/07_singly_linked_list/stripped.elf
```

## Run one case

```sh
./benchmark/run_codex_objdump.sh \
  --track asm-only \
  --trial trial-1 \
  artifacts_result_only/x86_64-linux-gnu-11.4.0/O3/07_singly_linked_list/stripped.elf
```

Use `--track objdump-complete` for the code-plus-data track. Outputs go to
the browsable hierarchy:

```text
codex_objdump_runs/runs/
  <suite>/<compiler>/<optimization>/<program>/<track>/<trial>/
```

For example:

```text
codex_objdump_runs/runs/result-only/
  x86_64-linux-gnu-11.4.0/O3/07_singly_linked_list/asm-only/trial-1/
```

Each completed run contains:

- `program.objdump.txt`: the exact evidence sent to Codex.
- `case-id.txt`: the content-derived binary/track identifier.
- `prompt.txt`: the fixed instruction prompt.
- `recovered.c`: the raw final response; it is never manually repaired.
- `trace.jsonl`: all Codex events and token-usage records.
- `codex.stderr`: CLI progress and diagnostics.
- `prohibited-tool-events.txt`: nonempty if the audit found forbidden activity.
- `status.txt`: `COMPLETE` or a failure/invalidity reason.
- Codex and GNU objdump version files.

The evaluator-only `codex_objdump_runs/manifest.tsv` records both the readable
`run_path` and content-derived case ID alongside the original binary path,
hashes, model, reasoning effort, track, trial, timestamps, and status. Never
mount this directory into a model-visible workspace.

To migrate outputs produced by an older runner, preview and then apply the
organization step:

```sh
./benchmark/organize_codex_runs.sh --check codex_objdump_runs
./benchmark/organize_codex_runs.sh --apply codex_objdump_runs
```

The migration preserves the old manifest under `codex_objdump_runs/metadata/`.

## Run a batch

The runner accepts multiple paths and creates an independent ephemeral Codex
thread and empty working directory for each one:

```sh
./benchmark/run_codex_objdump.sh \
  --track asm-only \
  --trial trial-1 \
  artifacts_result_only/x86_64-linux-gnu-11.4.0/O{0,1,2,3}/*/stripped.elf
```

That command can be expensive: it launches one model request per matching
binary. Start with `--prepare-only`, inspect the case count, and then run a
small subset before launching the full matrix.

Use distinct trial labels for repeated samples:

```sh
for trial in trial-1 trial-2 trial-3; do
  ./benchmark/run_codex_objdump.sh \
    --trial "$trial" \
    path/to/stripped.elf
done
```

The runner refuses to overwrite an existing case/trial.

If a run fails, inspect `codex.stderr` and `codex-exit-code.txt`, fix the cause,
and retry with a new label such as `--trial trial-2`. Failed trials remain
preserved as part of the experimental record.

## Troubleshooting

| Symptom | Action |
|---|---|
| Setup/config/auth error | Run `./benchmark/run_codex_objdump.sh --check` |
| `refusing to overwrite existing run` | Pass a new label such as `--trial trial-2` |
| `CODEX_FAILED` | Read that trial's `codex.stderr`, `trace.jsonl`, and `codex-exit-code.txt` |
| `INVALID_TOOL_USE` | Treat the trial as invalid and inspect `prohibited-tool-events.txt` |
| No `recovered.c` | Check `status.txt`; do not manufacture or manually repair a response |
| Source/test name appears in evidence | Stop; the evidence-generation condition is contaminated |

Keep one Codex CLI version fixed during a reported experiment. If an upgrade is
necessary, record it as a new experimental condition rather than silently
mixing versions.

## Isolation guarantees and limitations

The model receives two inputs: the fixed prompt and an stdin block containing
the objdump text. It does not receive the ELF, source, manifest, expected
stdout, DWARF, compile command, test name, or sibling optimization levels.
The temporary Codex working directory is empty, and the shell tool is disabled.

Every sample uses:

- A new `codex exec` process and thread; never `resume` or `fork`.
- `--ephemeral` and `history.persistence="none"`.
- `memories.use_memories=false` and `memories.generate_memories=false`.
- A new temporary `sqlite_home`, deleted when the runner exits.
- `--ignore-user-config` and `--ignore-rules`.
- Read-only sandboxing and `approval_policy=never`.
- Disabled shell, web, apps, plugins, hooks, subagents, and image/computer tools.
- An audit of `trace.jsonl` for prohibited tool events.

This establishes a fresh context with Codex memory disabled. It cannot erase
knowledge already present in model parameters. Use unpublished programs and
describe the condition accurately rather than claiming the model has no prior
knowledge.

The default pipeline gives Codex no tools because the complete evidence is
small enough to fit directly in context. If interactive `grep`-style access is
required for larger programs, do not enable a general shell and rely on prompt
compliance. Expose a custom MCP server with only bounded operations such as
`line_count()`, `read_lines(start, count)`, and
`search_regex(pattern, context, max_hits)`. Bind that server to one fixed file
and omit any arbitrary path or command parameter. Run it as a separate tool
condition and retain every MCP event from the JSONL trace.

## Experimental controls

Keep these fixed or record them for every run:

- Prompt bytes and hash.
- Exact requested model ID and Codex CLI version.
- Reasoning effort.
- GNU objdump version and flags.
- Evidence track and evidence hash.
- Tool-disable policy.
- Trial count and randomized case order.
- Token usage, runtime, final response, and complete JSONL trace.

Run source recovery and type recovery separately. `recovered.c` should be
compiled and evaluated automatically without access to the original ELF. Exact
identifier recovery should not be included in structural type scores because
stripping normally makes original names unrecoverable.
