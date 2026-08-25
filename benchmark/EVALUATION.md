# Evaluating recovered C and recovered types

Evaluate source recovery and type recovery as two related but separate tasks.
The existing `recovered.c` runs support objective compilation and execution
metrics. They do not contain stable binary-address identities for every inferred
variable, so comparing their type spellings directly with source C would produce
ambiguous and misleading type scores.

## Evaluate the existing recovered C runs

Build the pinned evaluation image if it does not already exist:

```sh
docker build --platform linux/amd64 \
  -t type-recovery-bench:ubuntu22.04 \
  -f benchmark/Dockerfile .
```

Then evaluate the organized `asm-only`, `trial-1` results:

```sh
./benchmark/evaluate_codex_runs.py --replace
```

The evaluator copies and compiles each raw `recovered.c` without repair. It
runs candidates with no network, a read-only filesystem, no Linux capabilities,
resource limits, and without the original binary or repository mounted.

Generated tables:

| File | Contents |
|---|---|
| `evaluation/codex-objdump/summary.md` | Headline table grouped by optimization |
| `evaluation/codex-objdump/case_metrics.md` | Human-readable table for every case |
| `evaluation/codex-objdump/case_metrics.tsv` | Machine-readable per-case metrics |
| `evaluation/codex-objdump/summary_by_optimization.tsv` | Machine-readable aggregate table |
| `evaluation/codex-objdump/details/` | Candidate copies and compile/runtime logs |

The current programs execute one fixed, input-free test vector. Consequently,
the behavior result is a **single-vector re-executability** measurement, not a
proof of semantic equivalence. Future source-recovery cases should consume
inputs and be checked against multiple hidden vectors.

## Existing source-recovery metrics

| Metric | Definition | Direction |
|---|---|---|
| Valid response | Status is complete, `recovered.c` is nonempty, and its hash matches the manifest | Higher is better |
| Clean trace | No prohibited tool event was detected | Must be 100% |
| Compile rate | Raw candidate compiles as GNU C11 with no human repair | Higher is better |
| Exit-0 rate | Compiled candidate terminates normally within the timeout | Higher is better |
| Behavior pass | Compile succeeds, exit status is 0, and stdout exactly matches the recorded oracle | Higher is better |
| Output token similarity | `1 - normalized token-level Levenshtein distance` between candidate and oracle stdout | Diagnostic only |

For `asm-only`, GNU objdump executable-section disassembly omits `.rodata`.
Output strings and some initialized data are therefore unavailable to the
model. Do not interpret behavior failure or output-token similarity as a type
failure, and do not compare this track directly with `objdump-complete`.

## Run a separately scored type-recovery condition

Use the structured type prompt in
[`prompt-template.md`](prompt-template.md), but require every predicted entity
to carry a binary identity. Recommended identities are:

- Functions: start virtual address.
- Globals: virtual address and byte size.
- Parameters: function address plus ABI argument slot, with observed location
  ranges where available.
- Locals: function address plus stack/CFA offset or register and instruction
  range.
- Aggregate fields: parent entity identity plus byte offset and observed access
  width.

Create the gold identities from `oracle.debug` and disassembly. Mark a source
entity scorable only when optimized code retains a usable location or access.
Match predictions to gold entities by identity/location—not by guessed names.
When optimized location lists overlap imperfectly, use a deterministic
maximum-weight matching rule based on PC-range overlap, storage location, and
size, fixed before inspecting model results.

The primary type metric table should be:

| Optimization | N cases | Observable entities | Prediction coverage | Pointer class accuracy | Pointer-depth accuracy | Primitive class accuracy | Exact primitive accuracy | Aggregate kind accuracy | Aggregate-size accuracy | Field-offset F1 | Prototype exact match |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| O0 | | | | | | | | | | | |
| O1 | | | | | | | | | | | |
| O2 | | | | | | | | | | | |
| O3 | | | | | | | | | | | |

Definitions:

- **Prediction coverage:** matched observable gold entities divided by all
  observable gold entities. Always publish this beside accuracy so a model
  cannot score well by predicting only easy cases.
- **Pointer class accuracy:** pointer versus non-pointer correctness over
  matched entities.
- **Pointer-depth accuracy:** exact indirection depth over matched gold pointer
  entities.
- **Primitive class accuracy:** integer, float, pointer, array, aggregate,
  function, or void/unknown class correctness.
- **Exact primitive accuracy:** class, bit width, and signedness all correct.
  Also report a sign-ignored version because machine code often does not
  distinguish signedness.
- **Aggregate kind/size accuracy:** struct, union, or array class and exact
  recovered byte size.
- **Field-offset F1:** precision/recall F1 over `(aggregate identity, byte
  offset, access width/type class)` field tuples.
- **Prototype exact match:** return type, parameter count, and every parameter
  type are correct. Publish component accuracies as supporting data.

Names are a separate semantic metric. Exact function, type, variable, or field
names are not recoverable from a fully stripped binary and must not contribute
to structural type scores.

## Aggregation and reporting

Compute each metric per program first, then macro-average the 20 programs at
each optimization level. This prevents `20_combined_stress` or another large
case from dominating the score. Report the number of observable entities and
prediction coverage so the denominator remains visible.

Use at least three independent trials per case for a stochastic model. Report
the mean and a 95% bootstrap confidence interval across programs; retain all
trials rather than selecting the best. For the optimization question, also
report paired per-program changes such as `O3 - O0`.

This separation follows the central cautions in
[TRex](https://www.usenix.org/system/files/usenixsecurity25-bosamiya.pdf): a
source-language type is not always uniquely recoverable from machine code, and
type quality needs a structural, hierarchy-aware comparison. The compile-and-
test behavior metric follows the re-executability idea used by
[SK2Decompile](https://arxiv.org/pdf/2509.22114), while the fixed-input limitation
above is specific to this benchmark's current test programs.
