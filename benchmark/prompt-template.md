# Fixed binary-input agent prompts

For repository setup and the recommended first run, start with
[`README.md`](../README.md).

For the stricter experiment where Codex receives only a pre-generated textual
GNU objdump artifact, use `codex-objdump-prompt.txt` with
`run_codex_objdump.sh`. The complete protocol is in `CODEX_OBJDUMP.md`.

These templates assume the evaluator gives the agent an isolated, read-only
directory containing exactly one file named `/input/program.elf`. The source,
DWARF oracle, compiler command, expected stdout, case name, and other
optimization variants must not be mounted into the agent environment.

Run source recovery and type recovery as separate experiments. Asking for both
in one run changes the task and lets one answer scaffold the other.

## Source-recovery prompt

```text
You have been given one fully stripped x86-64 Linux ELF executable at
/input/program.elf. Recover a standalone C implementation of the program using
only this binary and the analysis tools available in this environment.

Write the final implementation to /output/recovered.c. It must compile with the
evaluator's documented C compiler and must reproduce the input/output behavior
of the original executable on unseen tests. The original executable will not
be present when recovered.c is evaluated. Do not emit or invoke the original
ELF, embed it as data, launch another process, use the network, or read files
other than the input channels documented for the program.

Do not ask for source code, symbols, debugging data, expected output, or another
optimization variant. Treat uncertain source-level details conservatively.
At completion, return a JSON object containing the output path, analysis status,
and any limitations. Do not include the C source in that JSON response.
```

## Type-recovery prompt

```text
You have been given one fully stripped x86-64 Linux ELF executable at
/input/program.elf. Recover types using only this binary and the analysis tools
available in this environment. Do not generate decompiler pseudocode as an
input supplied by the evaluator; any disassembly or intermediate representation
you obtain must be produced during your own analysis of the ELF.

Write valid JSON to /output/types.json. Identify functions by virtual address,
not guessed source name. Identify globals by virtual address. Preserve
tool-generated variable identifiers when they exist, and attach a binary
location to every parameter and local. Infer only properties supported by
observable binary behavior.

Use these type kinds: unknown, void, integer, float, pointer, array, struct,
union, and function. Integer types have `bits` and `signed`, where `signed` is
true, false, or null when the binary does not distinguish it. Float types have
`bits`. Pointer types have `to`. Array types have `element` and `length`, where
length is null if unknown. Struct/union definitions have a stable invented ID,
`size_bytes` (null if unknown), and fields. Each field has `offset_bytes` (null
if unknown) and `type`. Recursive references use the invented type ID.

Use this top-level shape:
{
  "types": [],
  "globals": [
    {
      "address": "0x...",
      "size_bytes": null,
      "type": {},
      "confidence": 0.0,
      "evidence": []
    }
  ],
  "functions": [
    {
      "address": "0x...",
      "return_type": {},
      "parameters": [
        {
          "id": "param_1",
          "location": {
            "kind": "abi_slot",
            "value": "arg0",
            "pc_ranges": [["0x...", "0x..."]]
          },
          "type": {}
        }
      ],
      "locals": [
        {
          "id": "local_1",
          "location": {
            "kind": "stack",
            "value": "cfa-0x20",
            "pc_ranges": [["0x...", "0x..."]]
          },
          "type": {}
        }
      ],
      "confidence": 0.0,
      "evidence": []
    }
  ]
}

Location `kind` is one of `abi_slot`, `stack`, `register`, `global`, or
`unknown`. Express `value` using the disassembler's register names, a CFA/frame
offset, an ABI argument slot, or a virtual address. `pc_ranges` contains
half-open address ranges in which the location applies and may be empty when
unknown. Do not use a guessed source name as an identity.

Put human-meaningful guessed identifiers in optional `semantic_name` fields.
They are scored separately and must not affect structural type fields. At
completion, return only a small JSON status object pointing to types.json.
```

## Tool-access tiers

Run and report these as different benchmark tracks:

1. `bytes`: ELF parsing, hex dump, `readelf`, `strings`, and hashing only.
2. `disassembly`: tier 1 plus `objdump`/Capstone and CFG construction, but no
   decompiler.
3. `full-re`: tier 2 plus a fixed Ghidra/IDA/Binary Ninja version. The evaluator
   still supplies only the binary; the agent chooses and operates the tool.
4. `dynamic`: one of the above plus permission to execute/debug the binary and
   supply chosen inputs. Keep this separate from static-only results.

The bytes-only tier is the closest to a literal raw-binary test but is usually
an inefficient representation of model capability. The disassembly tier is the
cleanest primary measurement if the research question excludes decompilers.
The full-RE tier measures an LLM agent plus a decompiler, not the LLM alone.

For every run, record the prompt hash, complete tool policy and versions, all
tool calls and outputs, raw final response, produced-file hashes, model snapshot,
decoding parameters, token counts, wall time, and peak resource usage.
