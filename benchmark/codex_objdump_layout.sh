#!/usr/bin/env bash

# Shared path classification for Codex objdump runs. The caller supplies an
# absolute binary path and its SHA-256. Results are returned in the global
# codex_layout_* variables.
codex_objdump_classify_binary()
{
    local binary_path=$1
    local binary_hash=$2
    local raw_program

    if [[ $binary_path =~ /artifacts_result_only/([^/]+)/([^/]+)/([^/]+)/stripped\.elf$ ]]; then
        codex_layout_suite=result-only
        codex_layout_compiler=${BASH_REMATCH[1]}
        codex_layout_optimization=${BASH_REMATCH[2]}
        codex_layout_program=${BASH_REMATCH[3]}
    elif [[ $binary_path =~ /artifacts/([^/]+)/([^/]+)/([^/]+)/stripped\.elf$ ]]; then
        codex_layout_suite=readable
        codex_layout_compiler=${BASH_REMATCH[1]}
        codex_layout_optimization=${BASH_REMATCH[2]}
        codex_layout_program=${BASH_REMATCH[3]}
    else
        raw_program=${binary_path%/*}
        raw_program=${raw_program##*/}
        raw_program=${raw_program//[^A-Za-z0-9._+-]/_}
        [[ -n $raw_program ]] || raw_program=binary

        codex_layout_suite=external
        codex_layout_compiler=unknown-compiler
        codex_layout_optimization=unknown-optimization
        codex_layout_program=$raw_program-${binary_hash:0:16}
    fi

    codex_layout_relative=runs/$codex_layout_suite/$codex_layout_compiler/$codex_layout_optimization/$codex_layout_program
}
