#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

ida_out=sk2_ida_runs
vllm_out=sk2_vllm_runs
trial=trial-1
input_trial=
output_trial=
model_path=LLM4Binary/sk2decompile-struct-6.7b
recover_model_path=LLM4Binary/sk2decompile-ident-6.7
gpus=1
max_num_seqs=1
gpu_memory_utilization=0.8
quantization=
load_format=auto
dtype=auto
temperature=0
max_total_tokens=32768
max_new_tokens=4096
limit_cases=0

usage()
{
    cat <<'EOF'
Usage:
  benchmark/run_sk2_vllm_prepared.sh [options]

Run SK2Decompile vLLM inference from already prepared IDA dataset.json files.
This script does not require IDA and is intended for a Linux/NVIDIA VPS.

Options:
  --trial ID                 Prepared dataset trial to consume.
  --input-trial ID           Prepared dataset trial to consume; overrides --trial for input.
  --output-trial ID          vLLM output trial name; overrides --trial for output.
  --ida-output DIR           Prepared IDA run root, default: sk2_ida_runs
  --vllm-output DIR          vLLM run root, default: sk2_vllm_runs
  --model-path MODEL         Structure recovery HF model.
  --recover-model-path MODEL Identifier recovery HF model.
  --gpus N                   vLLM tensor parallel GPU count, default: 1
  --max-num-seqs N           vLLM max_num_seqs value, default: 1
  --gpu-memory-utilization F vLLM GPU memory utilization, default: 0.8
  --quantization METHOD      vLLM quantization method, e.g. bitsandbytes
  --load-format FORMAT       vLLM load format, e.g. bitsandbytes
  --dtype DTYPE              vLLM dtype, e.g. bfloat16
  --temperature FLOAT        Sampling temperature, default: 0
  --max-total-tokens N       vLLM max model length, default: 32768
  --max-new-tokens N         Max generated tokens per phase, default: 4096
  --limit-cases N            Only run the first N prepared datasets, default: all
  -h, --help                 Show this help.

Example:
  ./benchmark/run_sk2_vllm_prepared.sh \
    --trial ida-o0-local-1 \
    --quantization bitsandbytes \
    --load-format bitsandbytes \
    --dtype bfloat16
EOF
}

while (($#)); do
    case $1 in
        --trial)
            (($# >= 2)) || { echo "--trial requires a value" >&2; exit 2; }
            trial=$2
            shift 2
            ;;
        --input-trial)
            (($# >= 2)) || { echo "--input-trial requires a value" >&2; exit 2; }
            input_trial=$2
            shift 2
            ;;
        --output-trial)
            (($# >= 2)) || { echo "--output-trial requires a value" >&2; exit 2; }
            output_trial=$2
            shift 2
            ;;
        --ida-output)
            (($# >= 2)) || { echo "--ida-output requires a value" >&2; exit 2; }
            ida_out=$2
            shift 2
            ;;
        --vllm-output)
            (($# >= 2)) || { echo "--vllm-output requires a value" >&2; exit 2; }
            vllm_out=$2
            shift 2
            ;;
        --model-path)
            (($# >= 2)) || { echo "--model-path requires a value" >&2; exit 2; }
            model_path=$2
            shift 2
            ;;
        --recover-model-path)
            (($# >= 2)) || { echo "--recover-model-path requires a value" >&2; exit 2; }
            recover_model_path=$2
            shift 2
            ;;
        --gpus)
            (($# >= 2)) || { echo "--gpus requires a value" >&2; exit 2; }
            gpus=$2
            shift 2
            ;;
        --max-num-seqs)
            (($# >= 2)) || { echo "--max-num-seqs requires a value" >&2; exit 2; }
            max_num_seqs=$2
            shift 2
            ;;
        --gpu-memory-utilization)
            (($# >= 2)) || { echo "--gpu-memory-utilization requires a value" >&2; exit 2; }
            gpu_memory_utilization=$2
            shift 2
            ;;
        --quantization)
            (($# >= 2)) || { echo "--quantization requires a value" >&2; exit 2; }
            quantization=$2
            shift 2
            ;;
        --load-format)
            (($# >= 2)) || { echo "--load-format requires a value" >&2; exit 2; }
            load_format=$2
            shift 2
            ;;
        --dtype)
            (($# >= 2)) || { echo "--dtype requires a value" >&2; exit 2; }
            dtype=$2
            shift 2
            ;;
        --temperature)
            (($# >= 2)) || { echo "--temperature requires a value" >&2; exit 2; }
            temperature=$2
            shift 2
            ;;
        --max-total-tokens)
            (($# >= 2)) || { echo "--max-total-tokens requires a value" >&2; exit 2; }
            max_total_tokens=$2
            shift 2
            ;;
        --max-new-tokens)
            (($# >= 2)) || { echo "--max-new-tokens requires a value" >&2; exit 2; }
            max_new_tokens=$2
            shift 2
            ;;
        --limit-cases)
            (($# >= 2)) || { echo "--limit-cases requires a value" >&2; exit 2; }
            limit_cases=$2
            shift 2
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

if [[ $ida_out != /* ]]; then
    ida_out=$repo_dir/$ida_out
fi
if [[ $vllm_out != /* ]]; then
    vllm_out=$repo_dir/$vllm_out
fi

input_trial=${input_trial:-$trial}
output_trial=${output_trial:-$trial}

[[ $input_trial =~ ^[A-Za-z0-9._-]+$ ]] || {
    echo "input trial may contain only letters, digits, dot, underscore, and hyphen" >&2
    exit 2
}
[[ $output_trial =~ ^[A-Za-z0-9._-]+$ ]] || {
    echo "output trial may contain only letters, digits, dot, underscore, and hyphen" >&2
    exit 2
}

sk2_script=$repo_dir/LLM4Decompile/sk2decompile/evaluation/sk2decompile_inf.py
[[ -f $sk2_script ]] || { echo "SK2 inference script not found: $sk2_script" >&2; exit 1; }

for tool in python3; do
    command -v "$tool" >/dev/null || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

datasets=()
while IFS= read -r dataset; do
    datasets+=("$dataset")
done < <(find "$ida_out" -path "*/$input_trial/dataset.json" -type f | sort)
if ((${#datasets[@]} == 0)); then
    echo "no prepared datasets found for trial: $input_trial" >&2
    exit 1
fi
if ((limit_cases > 0 && ${#datasets[@]} > limit_cases)); then
    datasets=("${datasets[@]:0:limit_cases}")
fi

mkdir -p "$vllm_out"
overall_status=0
for dataset in "${datasets[@]}"; do
    case_dir=$(basename -- "$(dirname -- "$(dirname -- "$dataset")")")
    case_out="$vllm_out/$case_dir/$output_trial"
    if [[ -e $case_out ]]; then
        if [[ -f $case_out/status.txt && $(cat "$case_out/status.txt") == VLLM_FAILED ]]; then
            rm -rf -- "$case_out"
        else
            echo "refusing to overwrite existing vLLM output: $case_out" >&2
            overall_status=1
            continue
        fi
    fi

    mkdir -p "$case_out"
    vllm_args=(
        "$sk2_script"
        --dataset_path "$dataset"
        --model_path "$model_path"
        --recover_model_path "$recover_model_path"
        --gpus "$gpus"
        --max_num_seqs "$max_num_seqs"
        --gpu_memory_utilization "$gpu_memory_utilization"
        --load_format "$load_format"
        --dtype "$dtype"
        --temperature "$temperature"
        --max_total_tokens "$max_total_tokens"
        --max_new_tokens "$max_new_tokens"
        --output_path "$case_out/sk2_output"
    )
    if [[ -n $quantization ]]; then
        vllm_args+=(--quantization "$quantization")
    fi

    set +e
    python3 "${vllm_args[@]}" >"$case_out/vllm.stdout" 2>"$case_out/vllm.stderr"
    vllm_status=$?
    set -e
    if ((vllm_status == 0)) && [[ -s $case_out/sk2_output/inference_results.jsonl ]]; then
        printf 'COMPLETE\n' >"$case_out/status.txt"
        echo "COMPLETE $case_out"
    else
        printf 'VLLM_FAILED\n' >"$case_out/status.txt"
        echo "failed vLLM run: $case_out" >&2
        overall_status=1
    fi
done

exit "$overall_status"
