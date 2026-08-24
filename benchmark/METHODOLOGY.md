# Benchmark methodology

This document describes artifact construction, experimental separation,
scoring, and validity caveats. For commands, start with the repository-level
[`README.md`](../README.md). For the Codex assembly pipeline, read
[`CODEX_OBJDUMP.md`](CODEX_OBJDUMP.md).

## Artifact construction

Every test is compiled for Linux x86-64 at `-O0` through `-O3` using GNU C11.
Each case produces four ground-truth or input artifacts:

- `oracle.elf`: includes DWARF; never provide it to the model.
- `oracle.debug`: detached DWARF ground truth.
- `nodebug.elf`: removes DWARF but retains ordinary symbols.
- `stripped.elf`: removes DWARF and the static symbol table; this is the primary
  model-input binary before objdump conversion.

The build records the compiler version, compile command, section tables,
program output, hashes, and a suite manifest. It fails if stripping leaves
DWARF/static symbols, a program fails, or stripping changes stdout.

Two source suites are built separately:

- Root-level C files retain human-readable output and form the
  `semantic-context` suite.
- `result_only/` wrappers use the same computations and layouts while removing
  explanatory output labels. This is the `reduced-semantic-output` suite.

The result-only programs still contain real string-valued inputs and output
format information. They are not a strict layout-only suite. A strict layout
condition would need an opaque external checksum/sink with no descriptive
strings or format calls.

## Model-input isolation

For a raw binary-input experiment, expose exactly one `stripped.elf` under a
random case identity. For the objdump-text experiment in this repository,
expose only the fixed prompt and canonical `program.objdump.txt` through stdin.

Do not expose:

- Source files or source filenames.
- DWARF or detached debug data.
- Compile commands or compiler diagnostics.
- Expected stdout.
- Artifact or Codex-run manifests.
- Another optimization level of the same program.
- Earlier model responses for the case.

Start a fresh context for every binary. Keep prompt bytes, model ID, reasoning
effort, tool policy, Codex version, objdump version, architecture, timeout, and
resource limits fixed or record them per run. Retain the complete raw response
and JSONL trace.

The objdump conditions are:

1. `asm-only`: executable-section disassembly from `objdump -d`.
2. `objdump-complete`: ELF headers, section contents, and disassembly.

These are different evidence conditions and must not share a headline score.
If a decompiler is made available in another experiment, label that as an
agent-plus-decompiler condition rather than model-only disassembly recovery.

## Scoring

Report these dimensions separately:

1. Function prototypes: return type, parameter count, and parameter types.
2. Aggregate layout: struct/union/array class, byte size, field offsets, field
   widths, array extents, and recursive or pointer relationships.
3. Primitive semantics: integer versus float, bit width, signedness, and
   pointer depth.
4. Identifier semantics: guessed type, function, and field names.
5. Behavior: recompilation success and program behavior.

Signedness should receive lower weight where the binary does not distinguish
it. Exact source identifiers are normally unrecoverable after stripping and
must not affect structural type scores.

Only score variables and source fields for which optimized code retains
observable evidence. Report coverage alongside accuracy: optimization can
inline functions, remove variables, merge fields, or eliminate untouched data.
Absence of evidence is different from an incorrect inference.

For complete source recovery, compile the raw `recovered.c` without manual
repair and test stdout, stderr, exit status, and termination behavior. The
candidate must not access the original ELF during evaluation.

## Test-program design

Keep `main` in the whole-program suite. It makes each case executable, keeps
operations reachable, and supplies a behavioral oracle. Do not add `noinline`,
`volatile`, or `-fno-inline` merely to preserve convenient source boundaries;
their disappearance is part of the optimization experiment.

If stable one-function-to-one-function comparisons are needed, create a
separate controlled ablation in which target functions are externally visible
and marked `noinline`/`used`, with a driver compiled separately. Do not merge
that result with ordinary optimized binaries.

## Source-recovery limitations

The current cases use fixed inputs in `main`. Exact stdout comparison is too
weak as the sole source-recovery score because a candidate could hard-code the
observed output. Stronger source-recovery cases should accept varying stdin or
command-line inputs and be evaluated on hidden vectors.

The evaluation sandbox must reject recovered programs that execute or embed
the original ELF, launch another process to solve the task, use the network, or
read evaluator ground truth.

Type recovery does not require `main`; whole-program source recovery does. A
function-level suite may omit `main`, but the protocol must then document how a
nameless stripped function is selected and linked without leaking its original
prototype.

For deterministic inference, use a fixed decoding configuration. Still run
multiple independent trials when the serving backend is nondeterministic, and
keep every raw response rather than selecting or repairing the best one.
