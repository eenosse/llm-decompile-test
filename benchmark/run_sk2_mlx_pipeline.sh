#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

source_glob=result_only/[0-9][0-9]_*.c
binary_filter='*/O0/01_basic_struct/stripped.elf'
trial=mlx-q4-$(date '+%Y%m%d-%H%M%S')
ida_out=sk2_ida_runs
mlx_out=sk2_mlx_runs
hf_struct_model=LLM4Binary/sk2decompile-struct-6.7b
hf_recover_model=LLM4Binary/sk2decompile-ident-6.7
mlx_struct_model=models/sk2-struct-q4
mlx_recover_model=models/sk2-ident-q4
mlx_convert_dtype=float16
temperature=0
max_new_tokens=4096
skip_build=0
skip_prepare=0
skip_quantize=0

usage()
{
    cat <<'EOF'
Usage:
  benchmark/run_sk2_mlx_pipeline.sh [options]

Pipeline:
  1. Build result_only Linux ELF artifacts through Docker/OrbStack.
  2. Run macOS IDA/Hex-Rays prepare-only to create SK2 dataset.json.
  3. Convert SK2 Hugging Face models to local MLX Q4 if needed.
  4. Run two-phase SK2 inference with MLX on Apple Silicon.

Options:
  --binary-filter GLOB       Artifact filter, default: */O0/01_basic_struct/stripped.elf
  --trial ID                 Trial label, default: mlx-q4-<timestamp>
  --ida-output DIR           IDA run root, default: sk2_ida_runs
  --mlx-output DIR           MLX run root, default: sk2_mlx_runs
  --hf-struct-model MODEL    HF structure model, default: LLM4Binary/sk2decompile-struct-6.7b
  --hf-recover-model MODEL   HF recovery model, default: LLM4Binary/sk2decompile-ident-6.7
  --mlx-struct-model DIR     Local MLX Q4 structure model, default: models/sk2-struct-q4
  --mlx-recover-model DIR    Local MLX Q4 recovery model, default: models/sk2-ident-q4
  --mlx-convert-dtype DTYPE  MLX conversion dtype, default: float16
  --temperature FLOAT        MLX generation temperature, default: 0
  --max-new-tokens N         Max generated tokens per phase, default: 4096
  --skip-build               Do not run result_only/build_matrix.sh.
  --skip-prepare             Reuse latest dataset matching --trial/outputs.
  --skip-quantize            Do not run mlx_lm.convert, require local MLX models.
  -h, --help                 Show this help.

Required environment:
  SK2_IDA_BIN must point to the macOS IDA idat executable.
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
        --mlx-output)
            (($# >= 2)) || { echo "--mlx-output requires a value" >&2; exit 2; }
            mlx_out=$2
            shift 2
            ;;
        --hf-struct-model)
            (($# >= 2)) || { echo "--hf-struct-model requires a value" >&2; exit 2; }
            hf_struct_model=$2
            shift 2
            ;;
        --hf-recover-model)
            (($# >= 2)) || { echo "--hf-recover-model requires a value" >&2; exit 2; }
            hf_recover_model=$2
            shift 2
            ;;
        --mlx-struct-model)
            (($# >= 2)) || { echo "--mlx-struct-model requires a value" >&2; exit 2; }
            mlx_struct_model=$2
            shift 2
            ;;
        --mlx-recover-model)
            (($# >= 2)) || { echo "--mlx-recover-model requires a value" >&2; exit 2; }
            mlx_recover_model=$2
            shift 2
            ;;
        --mlx-convert-dtype)
            (($# >= 2)) || { echo "--mlx-convert-dtype requires a value" >&2; exit 2; }
            mlx_convert_dtype=$2
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
        --skip-build)
            skip_build=1
            shift
            ;;
        --skip-prepare)
            skip_prepare=1
            shift
            ;;
        --skip-quantize)
            skip_quantize=1
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
    echo "this MLX pipeline is intended to run from macOS on Apple Silicon" >&2
    exit 2
fi

[[ -n ${SK2_IDA_BIN:-} ]] || {
    echo "set SK2_IDA_BIN to your macOS IDA idat path" >&2
    exit 2
}

for tool in python3 docker "$SK2_IDA_BIN"; do
    command -v "$tool" >/dev/null || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

if ((skip_build == 0)); then
    "$repo_dir/result_only/build_matrix.sh" $source_glob
fi

binaries=()
while IFS= read -r binary; do
    binaries+=("$binary")
done < <(find "$repo_dir/artifacts_result_only" -path "$repo_dir/artifacts_result_only/$binary_filter" -type f | sort)
if ((${#binaries[@]} == 0)); then
    echo "no binaries matched: artifacts_result_only/$binary_filter" >&2
    exit 1
fi

if ((skip_prepare == 0)); then
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

python3 - <<'PY'
import importlib.util
missing = [name for name in ("mlx_lm",) if importlib.util.find_spec(name) is None]
if missing:
    raise SystemExit("missing Python module(s): " + ", ".join(missing))
PY

if ((skip_quantize == 0)); then
    prefetch_hf_repo()
    {
        local repo_or_path=$1
        if [[ -d $repo_or_path ]]; then
            return
        fi
        HF_REPO="$repo_or_path" python3 - <<'PY'
import os
from huggingface_hub import snapshot_download
snapshot_download(os.environ["HF_REPO"], local_files_only=False)
PY
    }

    convert_mlx_model()
    {
        local hf_model=$1
        local mlx_model=$2
        if [[ -f $mlx_model/config.json ]]; then
            return
        fi
        if [[ -e $mlx_model ]]; then
            echo "removing incomplete MLX model output: $mlx_model" >&2
            rm -rf -- "$mlx_model"
        fi
        prefetch_hf_repo "$hf_model"
        mlx_lm.convert \
            --hf-path "$hf_model" \
            --mlx-path "$mlx_model" \
            -q \
            --q-bits 4 \
            --dtype "$mlx_convert_dtype"
    }

    convert_mlx_model "$hf_struct_model" "$repo_dir/$mlx_struct_model"
    convert_mlx_model "$hf_recover_model" "$repo_dir/$mlx_recover_model"
fi

[[ -f $repo_dir/$mlx_struct_model/config.json ]] || { echo "missing MLX structure model: $repo_dir/$mlx_struct_model" >&2; exit 1; }
[[ -f $repo_dir/$mlx_recover_model/config.json ]] || { echo "missing MLX recovery model: $repo_dir/$mlx_recover_model" >&2; exit 1; }

mkdir -p "$repo_dir/$mlx_out"
overall_status=0
for dataset in "${datasets[@]}"; do
    case_dir=$(basename -- "$(dirname -- "$(dirname -- "$dataset")")")
    case_out="$repo_dir/$mlx_out/$case_dir/$trial"
    if [[ -e $case_out ]]; then
        if [[ -f $case_out/status.txt && $(cat "$case_out/status.txt") == MLX_FAILED ]]; then
            rm -rf -- "$case_out"
        else
            echo "refusing to overwrite existing MLX output: $case_out" >&2
            overall_status=1
            continue
        fi
    fi
    mkdir -p "$case_out"
    set +e
    python3 "$repo_dir/benchmark/sk2_mlx_infer.py" \
        --dataset-path "$dataset" \
        --model-path "$repo_dir/$mlx_struct_model" \
        --recover-model-path "$repo_dir/$mlx_recover_model" \
        --output-path "$case_out/sk2_output" \
        --temperature "$temperature" \
        --max-new-tokens "$max_new_tokens" \
        >"$case_out/mlx.stdout" 2>"$case_out/mlx.stderr"
    mlx_status=$?
    set -e
    if ((mlx_status == 0)) && [[ -s $case_out/sk2_output/inference_results.jsonl ]]; then
        printf 'COMPLETE\n' >"$case_out/status.txt"
        echo "COMPLETE $case_out"
    else
        printf 'MLX_FAILED\n' >"$case_out/status.txt"
        echo "failed MLX run: $case_out" >&2
        overall_status=1
    fi
done

exit "$overall_status"
