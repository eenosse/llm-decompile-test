Codex objdump benchmark outputs

Each case directory is keyed by the stripped-binary hash and evidence track.
Each trial preserves one raw Codex response and its complete audit record.

Start with manifest.tsv to map case IDs to evaluator-only binary paths.
Within a trial, read status.txt first, then recovered.c and trace.jsonl.
Never expose this output directory or manifest to a later model run.

Full guide: benchmark/CODEX_OBJDUMP.md
