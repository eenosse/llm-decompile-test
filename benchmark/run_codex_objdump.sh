#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
. "$script_dir/codex_objdump_layout.sh"

track=asm-only
model=
reasoning=high
out_root=codex_objdump_runs
prompt_file=benchmark/codex-objdump-prompt.txt
trial=trial-1
bench_home=
image=type-recovery-bench:ubuntu22.04
prepare_only=0
check_only=0
config_file=${CODEX_OBJDUMP_CONFIG:-$script_dir/codex-objdump.conf}
use_config=1
config_explicit=0

preparse_args=("$@")
for ((arg_i = 0; arg_i < ${#preparse_args[@]}; arg_i++)); do
    case ${preparse_args[arg_i]} in
        --config)
            ((arg_i + 1 < ${#preparse_args[@]})) || {
                echo "--config requires a value" >&2
                exit 2
            }
            arg_i=$((arg_i + 1))
            config_file=${preparse_args[arg_i]}
            use_config=1
            config_explicit=1
            ;;
        --no-config)
            use_config=0
            ;;
    esac
done

load_config()
{
    local line key value line_number=0

    while IFS= read -r line || [[ -n $line ]]; do
        line_number=$((line_number + 1))
        line=${line%$'\r'}
        case $line in
            ''|'#'*) continue ;;
            *=*)
                key=${line%%=*}
                value=${line#*=}
                ;;
            *)
                echo "$config_file:$line_number: expected KEY=VALUE" >&2
                exit 2
                ;;
        esac

        case $key in
            MODEL) model=$value ;;
            REASONING) reasoning=$value ;;
            TRACK) track=$value ;;
            OUTPUT) out_root=$value ;;
            PROMPT) prompt_file=$value ;;
            TRIAL) trial=$value ;;
            BENCH_HOME) bench_home=$value ;;
            IMAGE) image=$value ;;
            *)
                echo "$config_file:$line_number: unknown key: $key" >&2
                exit 2
                ;;
        esac
    done <"$config_file"
}

if ((use_config)); then
    if [[ -f $config_file ]]; then
        load_config
    elif ((config_explicit)); then
        echo "config file not found: $config_file" >&2
        exit 2
    fi
fi

# Explicit environment variables override the local config. Command-line
# options are parsed afterward and therefore have the highest precedence.
[[ -z ${CODEX_MODEL+x} ]] || model=$CODEX_MODEL
[[ -z ${CODEX_REASONING_EFFORT+x} ]] || reasoning=$CODEX_REASONING_EFFORT
[[ -z ${CODEX_OBJDUMP_TRACK+x} ]] || track=$CODEX_OBJDUMP_TRACK
[[ -z ${CODEX_OBJDUMP_OUT+x} ]] || out_root=$CODEX_OBJDUMP_OUT
[[ -z ${CODEX_OBJDUMP_PROMPT+x} ]] || prompt_file=$CODEX_OBJDUMP_PROMPT
[[ -z ${CODEX_TRIAL+x} ]] || trial=$CODEX_TRIAL
[[ -z ${CODEX_BENCH_HOME+x} ]] || bench_home=$CODEX_BENCH_HOME
[[ -z ${TYPEBENCH_IMAGE+x} ]] || image=$TYPEBENCH_IMAGE

usage()
{
    cat <<'EOF'
Usage:
  benchmark/run_codex_objdump.sh [options] stripped.elf [stripped.elf ...]

Options:
  --config FILE         Load pipeline settings from FILE.
  --no-config           Do not load the default local config.
  --model MODEL          Exact Codex model ID (or set CODEX_MODEL).
  --track TRACK          asm-only (default) or objdump-complete.
  --reasoning LEVEL      Codex reasoning effort (default: high).
  --output DIR           Evaluator-only run directory.
  --prompt FILE          Fixed prompt file.
  --trial ID             Trial label (default: trial-1).
  --check                Validate tools, config, auth, and Docker; run no model.
  --prepare-only         Generate and record objdump evidence; do not call Codex.
  -h, --help             Show this help.

Environment:
  CODEX_OBJDUMP_CONFIG   Config path (default: benchmark/codex-objdump.conf).
  Environment variables with matching settings override the config file.

Examples:
  ./benchmark/run_codex_objdump.sh \
    artifacts_result_only/x86_64-linux-gnu-11.4.0/O3/07_singly_linked_list/stripped.elf

  ./benchmark/run_codex_objdump.sh --prepare-only --model MODEL \
    --track objdump-complete path/to/stripped.elf
EOF
}

while (($#)); do
    case $1 in
        --config)
            (($# >= 2)) || { echo "--config requires a value" >&2; exit 2; }
            shift 2
            ;;
        --no-config)
            shift
            ;;
        --model)
            (($# >= 2)) || { echo "--model requires a value" >&2; exit 2; }
            model=$2
            shift 2
            ;;
        --track)
            (($# >= 2)) || { echo "--track requires a value" >&2; exit 2; }
            track=$2
            shift 2
            ;;
        --reasoning)
            (($# >= 2)) || { echo "--reasoning requires a value" >&2; exit 2; }
            reasoning=$2
            shift 2
            ;;
        --output)
            (($# >= 2)) || { echo "--output requires a value" >&2; exit 2; }
            out_root=$2
            shift 2
            ;;
        --prompt)
            (($# >= 2)) || { echo "--prompt requires a value" >&2; exit 2; }
            prompt_file=$2
            shift 2
            ;;
        --trial)
            (($# >= 2)) || { echo "--trial requires a value" >&2; exit 2; }
            trial=$2
            shift 2
            ;;
        --prepare-only)
            prepare_only=1
            shift
            ;;
        --check)
            check_only=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        -*)
            echo "unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            break
            ;;
    esac
done

binaries=("$@")

if [[ $out_root != /* ]]; then
    out_root=$repo_dir/$out_root
fi
if [[ $prompt_file != /* ]]; then
    prompt_file=$repo_dir/$prompt_file
fi

case $track in
    asm-only|objdump-complete) ;;
    *) echo "unsupported track: $track" >&2; exit 2 ;;
esac

case $reasoning in
    low|medium|high|xhigh) ;;
    *) echo "unsupported reasoning effort: $reasoning" >&2; exit 2 ;;
esac

[[ $trial =~ ^[A-Za-z0-9._-]+$ ]] || {
    echo "trial may contain only letters, digits, dot, underscore, and hyphen" >&2
    exit 2
}

if ((check_only == 0)); then
    ((${#binaries[@]})) || {
        echo "provide at least one stripped ELF path" >&2
        usage >&2
        exit 2
    }
fi

[[ -n $model ]] || {
    echo "set MODEL in $config_file, pass --model, or set CODEX_MODEL" >&2
    exit 2
}

[[ -f $prompt_file ]] || {
    echo "prompt file not found: $prompt_file" >&2
    exit 2
}

for tool in docker codex jq rg; do
    command -v "$tool" >/dev/null || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

if command -v sha256sum >/dev/null; then
    hash_file() { sha256sum "$1" | cut -d' ' -f1; }
elif command -v shasum >/dev/null; then
    hash_file() { shasum -a 256 "$1" | cut -d' ' -f1; }
else
    echo "sha256sum or shasum is required" >&2
    exit 1
fi

if ((prepare_only == 0)); then
    [[ -n $bench_home ]] || {
        echo "BENCH_HOME in the config (or CODEX_BENCH_HOME) must point to a dedicated authenticated Codex home" >&2
        echo "initialize one with: CODEX_HOME=\"\$HOME/.codex-objdump-benchmark\" codex login" >&2
        exit 2
    }
    [[ -d $bench_home ]] || {
        echo "CODEX_BENCH_HOME does not exist: $bench_home" >&2
        exit 2
    }
fi

# Keep Codex runtime databases out of the authenticated benchmark home. The
# home persists only so OAuth credentials can be refreshed; all SQLite-backed
# job, log, queue, goal, and memory state is private to this invocation and is
# deleted by cleanup().
scratch_root=$(mktemp -d "${TMPDIR:-/tmp}/codex-objdump.XXXXXX")
sqlite_root=$scratch_root/codex-sqlite
mkdir -p "$sqlite_root"
cleanup()
{
    if [[ -n ${scratch_root:-} && -d $scratch_root ]]; then
        rm -rf -- "$scratch_root"
    fi
}
trap cleanup EXIT HUP INT TERM

codex_policy_args=(
    --disable shell_tool
    --disable multi_agent
    --disable goals
    --disable apps
    --disable plugins
    --disable hooks
    --disable browser_use
    --disable computer_use
    --disable image_generation
    --disable view_image
    -c 'web_search="disabled"'
    -c 'approval_policy="never"'
    -c 'agents.enabled=false'
    -c 'memories.use_memories=false'
    -c 'memories.generate_memories=false'
    -c 'history.persistence="none"'
    -c "sqlite_home=\"$sqlite_root\""
    -c "model_reasoning_effort=\"$reasoning\""
)

if ((prepare_only == 0)); then
    set +e
    doctor_json=$(CODEX_HOME=$bench_home codex --strict-config \
        "${codex_policy_args[@]}" doctor --json 2>&1)
    set -e
    if ! printf '%s\n' "$doctor_json" | jq -e \
        '.checks["config.load"].status == "ok" and
         .checks["auth.credentials"].status == "ok"' >/dev/null 2>&1; then
        echo "Codex configuration/auth preflight failed:" >&2
        printf '%s\n' "$doctor_json" >&2
        exit 1
    fi
fi

if ! docker image inspect "$image" >/dev/null 2>&1; then
    docker build --platform linux/amd64 -t "$image" \
        -f "$script_dir/Dockerfile" "$script_dir"
fi

if ((check_only)); then
    codex_version=$(codex --version 2>&1)
    codex_version=${codex_version%%$'\n'*}
    objdump_version=$(docker run --rm --platform linux/amd64 "$image" \
        objdump --version)
    objdump_version=${objdump_version%%$'\n'*}
    printf '%s\n' \
        'Codex objdump pipeline setup: OK' \
        "  config:    $config_file" \
        "  model:     $model" \
        "  reasoning: $reasoning" \
        "  track:     $track" \
        "  Codex home: $bench_home" \
        '  SQLite:    temporary per invocation' \
        "  Codex CLI: $codex_version" \
        "  objdump:   $objdump_version" \
        "  image:     $image"
    exit 0
fi

mkdir -p "$out_root"
if [[ ! -e $out_root/README.txt ]]; then
    printf '%s\n' \
        'Codex objdump benchmark outputs' \
        '' \
        'Browse runs/<suite>/<compiler>/<optimization>/<program>/<track>/<trial>.' \
        'Each trial preserves one raw Codex response and its complete audit record.' \
        '' \
        'Start with manifest.tsv to map case IDs to evaluator-only binary paths.' \
        'Within a trial, read status.txt first, then recovered.c and trace.jsonl.' \
        'Never expose this output directory or manifest to a later model run.' \
        '' \
        'Full guide: benchmark/CODEX_OBJDUMP.md' \
        >"$out_root/README.txt"
fi
manifest=$out_root/manifest.tsv
manifest_header=$'case_id\ttrial\tstatus\ttrack\tmodel\treasoning\tbinary_path\tbinary_sha256\tevidence_sha256\tprompt_sha256\trecovered_sha256\tstarted_utc\tfinished_utc\twall_seconds\trun_path\tsuite\tcompiler\toptimization\tprogram'
if [[ ! -e $manifest ]]; then
    printf '%s\n' "$manifest_header" >"$manifest"
else
    IFS= read -r actual_manifest_header <"$manifest"
    [[ $actual_manifest_header == "$manifest_header" ]] || {
        echo "legacy or unsupported manifest layout: $manifest" >&2
        echo "run: ./benchmark/organize_codex_runs.sh --apply ${out_root#"$repo_dir"/}" >&2
        exit 1
    }
fi

prompt=$(<"$prompt_file")
prompt_hash=$(hash_file "$prompt_file")
codex_version=$(codex --version 2>&1)
codex_version=${codex_version%%$'\n'*}
objdump_version=$(docker run --rm --platform linux/amd64 "$image" \
    objdump --version)
objdump_version=${objdump_version%%$'\n'*}
overall_status=0
case_number=0

for binary_arg in "${binaries[@]}"; do
    case_number=$((case_number + 1))
    if [[ $binary_arg = /* ]]; then
        binary=$binary_arg
    else
        binary=$PWD/$binary_arg
    fi
    if [[ ! -f $binary ]]; then
        echo "binary not found: $binary_arg" >&2
        overall_status=1
        continue
    fi

    binary_hash=$(hash_file "$binary")
    case_id=case-${binary_hash:0:16}-$track
    codex_objdump_classify_binary "$binary" "$binary_hash"
    run_path=$codex_layout_relative/$track/$trial
    case_out=$out_root/$run_path
    if awk -F '\t' -v c="$case_id" -v t="$trial" \
        'NR > 1 && $1 == c && $2 == t { found=1 } END { exit !found }' \
        "$manifest"; then
        echo "refusing duplicate manifest entry: $case_id/$trial" >&2
        echo "choose another --trial or --output" >&2
        overall_status=1
        continue
    fi
    if [[ -e $case_out ]]; then
        echo "refusing to overwrite existing run: $case_out" >&2
        echo "choose another --trial or --output" >&2
        overall_status=1
        continue
    fi
    mkdir -p "$case_out"
    printf '%s\n' "$case_id" >"$case_out/case-id.txt"
    printf '%s\n' \
        'Files in this trial' \
        '' \
        'case-id.txt                   content-derived case identifier' \
        'status.txt                    final validity/completion status' \
        'program.objdump.txt           exact evidence sent to Codex' \
        'prompt.txt                    exact fixed instruction prompt' \
        'recovered.c                   raw final model response (real runs only)' \
        'trace.jsonl                   complete Codex JSONL event trace' \
        'codex.stderr                  CLI diagnostics' \
        'codex-exit-code.txt           CLI process exit code' \
        'prohibited-tool-events.txt    nonempty means the trial is invalid' \
        'codex-version.txt             pinned Codex CLI version' \
        'objdump-version.txt           pinned GNU objdump version' \
        '' \
        'The parent output manifest is evaluator-only and must not be shown to Codex.' \
        >"$case_out/README.txt"

    prep_dir=$scratch_root/$case_id-$trial-prep
    work_dir=$scratch_root/work-$case_number
    mkdir -p "$prep_dir" "$work_dir"
    cp -- "$binary" "$prep_dir/program.elf"
    evidence=$case_out/program.objdump.txt

    if [[ $track == asm-only ]]; then
        docker run --rm --platform linux/amd64 \
            -e LC_ALL=C \
            -v "$prep_dir:/input:ro" \
            "$image" \
            objdump -d -M intel --no-show-raw-insn --wide /input/program.elf \
            >"$evidence"
    else
        {
            docker run --rm --platform linux/amd64 \
                -e LC_ALL=C -v "$prep_dir:/input:ro" "$image" \
                objdump -x /input/program.elf
            docker run --rm --platform linux/amd64 \
                -e LC_ALL=C -v "$prep_dir:/input:ro" "$image" \
                objdump -s /input/program.elf
            docker run --rm --platform linux/amd64 \
                -e LC_ALL=C -v "$prep_dir:/input:ro" "$image" \
                objdump -d -M intel --no-show-raw-insn --wide /input/program.elf
        } >"$evidence"
    fi

    test -s "$evidence" || {
        echo "objdump produced no evidence for: $binary_arg" >&2
        overall_status=1
        continue
    }

    evidence_hash=$(hash_file "$evidence")
    cp -- "$prompt_file" "$case_out/prompt.txt"
    printf '%s\n' "$codex_version" >"$case_out/codex-version.txt"
    printf '%s\n' "$objdump_version" >"$case_out/objdump-version.txt"
    started_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
    started_epoch=$(date '+%s')

    if ((prepare_only)); then
        finished_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
        finished_epoch=$(date '+%s')
        wall_seconds=$((finished_epoch - started_epoch))
        printf 'PREPARED\n' >"$case_out/status.txt"
        printf '%s\t%s\tPREPARED\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t-\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
            "$case_id" "$trial" "$track" "$model" "$reasoning" "$binary" \
            "$binary_hash" "$evidence_hash" "$prompt_hash" "$started_utc" \
            "$finished_utc" "$wall_seconds" "$run_path" "$codex_layout_suite" \
            "$codex_layout_compiler" "$codex_layout_optimization" \
            "$codex_layout_program" >>"$manifest"
        echo "prepared $case_id: $case_out"
        continue
    fi

    trace=$case_out/trace.jsonl
    recovered=$case_out/recovered.c
    stderr_log=$case_out/codex.stderr

    set +e
    CODEX_HOME=$bench_home codex exec \
        --model "$model" \
        --ephemeral \
        --ignore-user-config \
        --ignore-rules \
        --skip-git-repo-check \
        --sandbox read-only \
        --strict-config \
        "${codex_policy_args[@]}" \
        -C "$work_dir" \
        --json \
        --output-last-message "$recovered" \
        "$prompt" \
        <"$evidence" >"$trace" 2>"$stderr_log"
    codex_status=$?
    set -e
    printf '%s\n' "$codex_status" >"$case_out/codex-exit-code.txt"

    status=COMPLETE
    if ((codex_status != 0)); then
        status=CODEX_FAILED
    elif [[ ! -s $recovered ]]; then
        status=EMPTY_RESPONSE
    fi

    prohibited=$case_out/prohibited-tool-events.txt
    if [[ -s $trace ]] && rg -n \
        '"type":"(command_execution|file_change|mcp_tool_call|web_search|image_generation|computer_use)"' \
        "$trace" >"$prohibited"; then
        status=INVALID_TOOL_USE
    else
        : >"$prohibited"
    fi

    recovered_hash=-
    if [[ -s $recovered ]]; then
        recovered_hash=$(hash_file "$recovered")
    fi
    finished_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
    finished_epoch=$(date '+%s')
    wall_seconds=$((finished_epoch - started_epoch))
    printf '%s\n' "$status" >"$case_out/status.txt"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$case_id" "$trial" "$status" "$track" "$model" "$reasoning" \
        "$binary" "$binary_hash" "$evidence_hash" "$prompt_hash" \
        "$recovered_hash" "$started_utc" "$finished_utc" "$wall_seconds" \
        "$run_path" "$codex_layout_suite" "$codex_layout_compiler" \
        "$codex_layout_optimization" "$codex_layout_program" \
        >>"$manifest"

    if [[ $status == COMPLETE ]]; then
        echo "completed $case_id: $case_out"
    else
        echo "invalid or failed run ($status): $case_out" >&2
        overall_status=1
    fi
done

exit "$overall_status"
