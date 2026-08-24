# Result-only variants

For the complete build-to-Codex workflow, start with the repository-level
[`README.md`](../README.md).

This directory contains one wrapper for every root-level C benchmark. Each
wrapper defines `RESULT_ONLY` and includes the corresponding implementation,
so the computation and data layouts remain shared with the readable version.
Only the selected output expressions differ at preprocessing time.

Result-only stdout is intended to resemble competitive-programming output:
it emits computed values and test-data strings without explanatory labels such
as `found`, `state=`, or `value=`. A vertical bar separates adjacent string
fields where spaces may be part of a value. The wrappers also exclude the
readable format strings selected by `BENCH_OUTPUT` from the compiled program.

Build and strip every result-only program at `-O0` through `-O3` with:

```sh
./result_only/build_matrix.sh
```

Artifacts are written to `artifacts_result_only/`, leaving the readable suite
in `artifacts/`. To build one variant directly:

```sh
gcc -std=gnu11 -O2 result_only/07_singly_linked_list.c -o list-result
./list-result
```

These programs still contain real string-valued inputs (names, paths, tags,
and similar data) and their output calls still reveal primitive formatting.
They are therefore a reduced-semantic-output track, not a strict layout-only
track. A strict track should use an opaque external sink rather than `printf`.
