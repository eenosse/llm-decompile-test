# Codex objdump evaluation

Condition: `asm-only`, `trial-1`; 80 cases. Candidates were compiled without repair and executed in isolated, network-disabled containers.

Re-executability requires compilation, exit status 0, and byte-for-byte agreement with the recorded oracle stdout. Token similarity is a diagnostic partial score, not functional equivalence.

This track omits `.rodata`; therefore exact recovery of output strings and initialized data is generally underdetermined. Treat the behavior and similarity columns as source-recovery diagnostics, not as type-recovery scores.

| Optimization | N | Valid | Clean trace | Compiles | Exits 0 | Re-executable | Token similarity | Mean model time (s) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| O0 | 20 | 100.0% | 100.0% | 100.0% | 100.0% | 0.0% | 0.438 | 302.2 |
| O1 | 20 | 100.0% | 100.0% | 100.0% | 100.0% | 0.0% | 0.369 | 262.6 |
| O2 | 20 | 100.0% | 100.0% | 95.0% | 95.0% | 0.0% | 0.338 | 286.9 |
| O3 | 20 | 100.0% | 100.0% | 95.0% | 95.0% | 0.0% | 0.328 | 305.9 |
| ALL | 80 | 100.0% | 100.0% | 97.5% | 97.5% | 0.0% | 0.368 | 289.4 |

## Compile failures

- `O2/20_combined_stress`: see `details/O2/20_combined_stress/asm-only/trial-1/compile.stderr`.
- `O3/19_state_machine`: see `details/O3/19_state_machine/asm-only/trial-1/compile.stderr`.
