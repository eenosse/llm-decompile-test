#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
. "$script_dir/codex_objdump_layout.sh"

mode=check
out_root=$repo_dir/codex_objdump_runs

usage()
{
    cat <<'EOF'
Usage: benchmark/organize_codex_runs.sh [--check | --apply] [OUTPUT_DIR]

Migrates the legacy case-<hash>-<track>/<trial> layout to:
  runs/<suite>/<compiler>/<optimization>/<program>/<track>/<trial>

--check is the default and changes nothing. --apply preserves the old manifest
under metadata/ before moving run directories and adding run_path metadata.
EOF
}

while (($#)); do
    case $1 in
        --check) mode=check; shift ;;
        --apply) mode=apply; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
        *) out_root=$1; shift; (($# == 0)) || { usage >&2; exit 2; } ;;
    esac
done

if [[ $out_root != /* ]]; then
    out_root=$PWD/$out_root
fi
manifest=$out_root/manifest.tsv
[[ -f $manifest ]] || { echo "manifest not found: $manifest" >&2; exit 1; }

legacy_header=$'case_id\ttrial\tstatus\ttrack\tmodel\treasoning\tbinary_path\tbinary_sha256\tevidence_sha256\tprompt_sha256\trecovered_sha256\tstarted_utc\tfinished_utc\twall_seconds'
current_header=$legacy_header$'\trun_path\tsuite\tcompiler\toptimization\tprogram'
IFS= read -r actual_header <"$manifest"

if [[ $actual_header == "$current_header" ]]; then
    echo "manifest already uses the organized layout: $manifest"
    exit 0
fi
[[ $actual_header == "$legacy_header" ]] || {
    echo "unsupported manifest header: $manifest" >&2
    exit 1
}

scratch_dir=$(mktemp -d "${TMPDIR:-/tmp}/organize-codex-runs.XXXXXX")
plan_file=$scratch_dir/plan.tsv
new_manifest=$scratch_dir/manifest.tsv
destinations_file=$scratch_dir/destinations.txt
legacy_dirs_file=$scratch_dir/legacy-dirs.txt
cleanup()
{
    if [[ -f ${plan_file:-} ]]; then unlink "$plan_file"; fi
    if [[ -f ${new_manifest:-} ]]; then unlink "$new_manifest"; fi
    if [[ -f ${destinations_file:-} ]]; then unlink "$destinations_file"; fi
    if [[ -f ${legacy_dirs_file:-} ]]; then unlink "$legacy_dirs_file"; fi
    if [[ -d ${scratch_dir:-} ]]; then rmdir "$scratch_dir"; fi
}
trap cleanup EXIT HUP INT TERM

printf '%s\n' "$current_header" >"$new_manifest"
: >"$plan_file"
: >"$destinations_file"
: >"$legacy_dirs_file"

row_count=0
move_count=0
missing_count=0

while IFS=$'\t' read -r case_id trial status track model reasoning \
    binary_path binary_sha256 evidence_sha256 prompt_sha256 recovered_sha256 \
    started_utc finished_utc wall_seconds; do
    row_count=$((row_count + 1))
    codex_objdump_classify_binary "$binary_path" "$binary_sha256"
    run_path=$codex_layout_relative/$track/$trial
    source_path=$out_root/$case_id/$trial
    destination_path=$out_root/$run_path

    if rg -F -x -q -- "$run_path" "$destinations_file"; then
        echo "duplicate destination in manifest: $run_path" >&2
        exit 1
    fi
    printf '%s\n' "$run_path" >>"$destinations_file"

    if [[ -e $destination_path ]]; then
        echo "destination already exists: $destination_path" >&2
        exit 1
    fi
    if [[ -d $source_path ]]; then
        move_count=$((move_count + 1))
        printf '%s\t%s\t%s\n' "$source_path" "$destination_path" "$case_id" >>"$plan_file"
    else
        missing_count=$((missing_count + 1))
        printf 'missing output: %s/%s (%s)\n' "$case_id" "$trial" "$binary_path" >&2
    fi

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$case_id" "$trial" "$status" "$track" "$model" "$reasoning" \
        "$binary_path" "$binary_sha256" "$evidence_sha256" "$prompt_sha256" \
        "$recovered_sha256" "$started_utc" "$finished_utc" "$wall_seconds" \
        "$run_path" "$codex_layout_suite" "$codex_layout_compiler" \
        "$codex_layout_optimization" "$codex_layout_program" >>"$new_manifest"
done < <(tail -n +2 "$manifest")

printf 'manifest rows: %d\nmovable runs: %d\nmissing runs:  %d\n' \
    "$row_count" "$move_count" "$missing_count"

if [[ $mode == check ]]; then
    echo 'check complete; no files changed'
    exit 0
fi

metadata_dir=$out_root/metadata
backup_manifest=$metadata_dir/manifest.pre-layout-v2.tsv
report_file=$metadata_dir/layout-v2-migration.txt
[[ ! -e $backup_manifest ]] || {
    echo "backup already exists: $backup_manifest" >&2
    exit 1
}
mkdir -p "$metadata_dir"
cp -- "$manifest" "$backup_manifest"

while IFS=$'\t' read -r source_path destination_path case_id; do
    mkdir -p "${destination_path%/*}"
    mv -- "$source_path" "$destination_path"
    printf '%s\n' "$case_id" >"$destination_path/case-id.txt"

    legacy_case_dir=${source_path%/*}
    printf '%s\n' "$legacy_case_dir" >>"$legacy_dirs_file"
done <"$plan_file"

sort -u "$legacy_dirs_file" | while IFS= read -r legacy_case_dir; do
    if [[ -f $legacy_case_dir/.DS_Store ]]; then
        unlink "$legacy_case_dir/.DS_Store"
    fi
    if ! rmdir "$legacy_case_dir"; then
        echo "retained nonempty legacy directory: $legacy_case_dir" >&2
    fi
done

mv -- "$new_manifest" "$manifest"
printf '%s\n' \
    'Codex run layout migration v2' \
    "manifest rows: $row_count" \
    "moved runs: $move_count" \
    "missing runs retained in manifest: $missing_count" \
    "original manifest: metadata/${backup_manifest##*/}" \
    >"$report_file"

printf 'organized runs under: %s/runs\noriginal manifest: %s\n' \
    "$out_root" "$backup_manifest"
