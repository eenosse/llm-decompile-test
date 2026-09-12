#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

source_glob=result_only/[0-9][0-9]_*.c
binary_filter='*/O0/01_basic_struct/stripped.elf'
trial=gguf-q4-$(date '+%Y%m%d-%H%M%S')
ida_out=sk2_ida_runs
gguf_out=sk2_gguf_runs
struct_model='mradermacher/sk2decompile-struct-6.7b-GGUF:Q4_K_M'
recover_model='mradermacher/sk2decompile-ident-6.7-GGUF:Q4_K_M'
llama_cli=llama-cli
temperature=0
max_new_tokens=4096
limit=0
skip_build=0
skip_prepare=0
llama_args=()

usage()
{
    cat <<'EOF'
Usage:
  benchmark/run_sk2_llama_cpp_pipeline.sh [options]

Pipeline:
  1. Build result_only Linux ELF artifacts through Docker/OrbStack.
  2. Run macOS IDA/Hex-Rays prepare-only to create SK2 dataset.json.
  3. Run two-phase SK2 inference through llama.cpp GGUF Q4 models.

Options:
  --binary-filter GLOB       Artifact filter, default: */O0/01_basic_struct/stripped.elf
  --trial ID                 Trial label, default: gguf-q4-<timestamp>
  --ida-output DIR           IDA run root, default: sk2_ida_runs
  --gguf-output DIR          GGUF run root, default: sk2_gguf_runs
  --struct-model REF         llama-cli -hf structure model, default: mradermacher/sk2decompile-struct-6.7b-GGUF:Q4_K_M
  --recover-model REF        llama-cli -hf recovery model, default: mradermacher/sk2decompile-ident-6.7-GGUF:Q4_K_M
  --llama-cli PATH           llama.cpp CLI executable, default: llama-cli
  --llama-arg ARG            Extra argument passed to every llama-cli call. Can be repeated.
  --temperature FLOAT        Generation temperature, default: 0
  --max-new-tokens N         Max generated tokens per phase, default: 4096
  --limit N                  Only process the first N functions per dataset, default: all
  --skip-build               Do not run result_only/build_matrix.sh.
  --skip-prepare             Reuse latest dataset matching --trial/outputs.
  -h, --help                 Show this help.

Required environment:
  SK2_IDA_BIN must point to the macOS IDA idat executable.

Install llama.cpp on macOS with:
  brew install llama.cpp
EOF
}

while (($#)); do
    case $1 in
        --binary-filter)
            (($# >= 2)) || { echo "--binary-filter requires a value" >&2; exit 2; }
            binary_filter=$2
            shift 2
            ;;
        --trial)
            (($# >= 2)) || { echo "--trial requires a value" >&2; exit 2; }
            trial=$2
            shift 2
            ;;
        --ida-output)
            (($# >= 2)) || { echo "--ida-output requires a value" >&2; exit 2; }
            ida_out=$2
            shift 2
            ;;
        --gguf-output)
            (($# >= 2)) || { echo "--gguf-output requires a value" >&2; exit 2; }
            gguf_out=$2
            shift 2
            ;;
        --struct-model)
            (($# >= 2)) || { echo "--struct-model requires a value" >&2; exit 2; }
            struct_model=$2
            shift 2
            ;;
        --recover-model)
            (($# >= 2)) || { echo "--recover-model requires a value" >&2; exit 2; }
            recover_model=$2
            shift 2
            ;;
        --llama-cli)
            (($# >= 2)) || { echo "--llama-cli requires a value" >&2; exit 2; }
            llama_cli=$2
            shift 2
            ;;
        --llama-arg)
            (($# >= 2)) || { echo "--llama-arg requires a value" >&2; exit 2; }
            llama_args+=("$2")
            shift 2
            ;;
        --temperature)
            (($# >= 2)) || { echo "--temperature requires a value" >&2; exit 2; }
            temperature=$2
            shift 2
            ;;
        --max-new-tokens)
            (($# >= 2)) || { echo "--max-new-tokens requires a value" >&2; exit 2; }
            max_new_tokens=$2
            shift 2
            ;;
        --limit)
            (($# >= 2)) || { echo "--limit requires a value" >&2; exit 2; }
            limit=$2
            shift 2
            ;;
        --skip-build)
            skip_build=1
            shift
            ;;
        --skip-prepare)
            skip_prepare=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ $(uname -s) != Darwin ]]; then
    echo "this llama.cpp pipeline is intended to run from macOS with your local IDA app" >&2
    exit 2
fi

required_tools=(python3 "$llama_cli")
if ((skip_build == 0)); then
    required_tools+=(docker)
fi
if ((skip_prepare == 0)); then
    [[ -n ${SK2_IDA_BIN:-} ]] || {
        echo "set SK2_IDA_BIN to your macOS IDA idat path" >&2
        exit 2
    }
    required_tools+=("$SK2_IDA_BIN")
fi

for tool in "${required_tools[@]}"; do
    command -v "$tool" >/dev/null || {
        echo "missing required tool: $tool" >&2
        if [[ $tool == "$llama_cli" ]]; then
            echo "install llama.cpp with: brew install llama.cpp" >&2
        fi
        exit 1
    }
done

if ((skip_build == 0)); then
    "$repo_dir/result_only/build_matrix.sh" $source_glob
fi

if ((skip_prepare == 0)); then
    binaries=()
    while IFS= read -r binary; do
        binaries+=("$binary")
    done < <(find "$repo_dir/artifacts_result_only" -path "$repo_dir/artifacts_result_only/$binary_filter" -type f | sort)
    if ((${#binaries[@]} == 0)); then
        echo "no binaries matched: artifacts_result_only/$binary_filter" >&2
        exit 1
    fi

    "$repo_dir/benchmark/run_sk2_ida.sh" \
        --prepare-only \
        --trial "$trial" \
        --output "$ida_out" \
        "${binaries[@]}"
fi

datasets=()
while IFS= read -r dataset; do
    datasets+=("$dataset")
done < <(find "$repo_dir/$ida_out" -path "*/$trial/dataset.json" -type f | sort)
if ((${#datasets[@]} == 0)); then
    echo "no prepared datasets found for trial: $trial" >&2
    exit 1
fi

mkdir -p "$repo_dir/$gguf_out"
overall_status=0
for dataset in "${datasets[@]}"; do
    case_dir=$(basename -- "$(dirname -- "$(dirname -- "$dataset")")")
    case_out="$repo_dir/$gguf_out/$case_dir/$trial"
    if [[ -e $case_out ]]; then
        if [[ -f $case_out/status.txt && $(cat "$case_out/status.txt") == LLAMA_CPP_FAILED ]]; then
            rm -rf -- "$case_out"
        else
            echo "refusing to overwrite existing llama.cpp output: $case_out" >&2
            overall_status=1
            continue
        fi
    fi
    mkdir -p "$case_out"
    infer_llama_args=()
    for llama_arg in "${llama_args[@]}"; do
        infer_llama_args+=(--llama-arg "$llama_arg")
    done
    set +e
    python3 "$repo_dir/benchmark/sk2_llama_cpp_infer.py" \
        --dataset-path "$dataset" \
        --model "$struct_model" \
        --recover-model "$recover_model" \
        --llama-cli "$llama_cli" \
        --output-path "$case_out/sk2_output" \
        --temperature "$temperature" \
        --max-new-tokens "$max_new_tokens" \
        --limit "$limit" \
        "${infer_llama_args[@]}" \
        >"$case_out/llama_cpp.stdout" 2>"$case_out/llama_cpp.stderr"
    llama_status=$?
    set -e
    if ((llama_status == 0)) && [[ -s $case_out/sk2_output/inference_results.jsonl ]]; then
        printf 'COMPLETE\n' >"$case_out/status.txt"
        echo "COMPLETE $case_out"
    else
        printf 'LLAMA_CPP_FAILED\n' >"$case_out/status.txt"
        echo "failed llama.cpp run: $case_out" >&2
        overall_status=1
    fi
done

exit "$overall_status"
