Codex objdump benchmark outputs

Browse runs/<suite>/<compiler>/<optimization>/<program>/<track>/<trial>/.
Each trial preserves one raw Codex response and its complete audit record.

Start with manifest.tsv to filter runs or resolve case IDs and run paths.
Within a trial, read status.txt first, then recovered.c and trace.jsonl.
Never expose this output directory or manifest to a later model run.

metadata/ contains the layout-migration record and original manifest backup.
One manifest row may have no run directory when its recorded output was already missing.

Full guide: benchmark/CODEX_OBJDUMP.md
