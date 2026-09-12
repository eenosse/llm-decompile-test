#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

ida_bin=
model_path=LLM4Binary/sk2decompile-struct-6.7b
recover_model_path=LLM4Binary/sk2decompile-ident-6.7
out_root=sk2_ida_runs
trial=trial-1
gpus=1
max_num_seqs=1
gpu_memory_utilization=0.8
quantization=
load_format=auto
dtype=auto
temperature=0
max_total_tokens=32768
max_new_tokens=4096
clang_format=clang-format
config_file=${SK2_IDA_CONFIG:-$script_dir/sk2-ida.conf}
use_config=1
config_explicit=0
prepare_only=0
check_only=0

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
            IDA_BIN) ida_bin=$value ;;
            MODEL_PATH) model_path=$value ;;
            RECOVER_MODEL_PATH) recover_model_path=$value ;;
            OUTPUT) out_root=$value ;;
            TRIAL) trial=$value ;;
            GPUS) gpus=$value ;;
            MAX_NUM_SEQS) max_num_seqs=$value ;;
            GPU_MEMORY_UTILIZATION) gpu_memory_utilization=$value ;;
            QUANTIZATION) quantization=$value ;;
            LOAD_FORMAT) load_format=$value ;;
            DTYPE) dtype=$value ;;
            TEMPERATURE) temperature=$value ;;
            MAX_TOTAL_TOKENS) max_total_tokens=$value ;;
            MAX_NEW_TOKENS) max_new_tokens=$value ;;
            CLANG_FORMAT) clang_format=$value ;;
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

[[ -z ${SK2_IDA_BIN+x} ]] || ida_bin=$SK2_IDA_BIN
[[ -z ${SK2_MODEL_PATH+x} ]] || model_path=$SK2_MODEL_PATH
[[ -z ${SK2_RECOVER_MODEL_PATH+x} ]] || recover_model_path=$SK2_RECOVER_MODEL_PATH
[[ -z ${SK2_IDA_OUT+x} ]] || out_root=$SK2_IDA_OUT
[[ -z ${SK2_TRIAL+x} ]] || trial=$SK2_TRIAL
[[ -z ${SK2_GPUS+x} ]] || gpus=$SK2_GPUS
[[ -z ${SK2_MAX_NUM_SEQS+x} ]] || max_num_seqs=$SK2_MAX_NUM_SEQS
[[ -z ${SK2_GPU_MEMORY_UTILIZATION+x} ]] || gpu_memory_utilization=$SK2_GPU_MEMORY_UTILIZATION
[[ -z ${SK2_QUANTIZATION+x} ]] || quantization=$SK2_QUANTIZATION
[[ -z ${SK2_LOAD_FORMAT+x} ]] || load_format=$SK2_LOAD_FORMAT
[[ -z ${SK2_DTYPE+x} ]] || dtype=$SK2_DTYPE
[[ -z ${SK2_TEMPERATURE+x} ]] || temperature=$SK2_TEMPERATURE
[[ -z ${SK2_MAX_TOTAL_TOKENS+x} ]] || max_total_tokens=$SK2_MAX_TOTAL_TOKENS
[[ -z ${SK2_MAX_NEW_TOKENS+x} ]] || max_new_tokens=$SK2_MAX_NEW_TOKENS
[[ -z ${SK2_CLANG_FORMAT+x} ]] || clang_format=$SK2_CLANG_FORMAT

usage()
{
    cat <<'EOF'
Usage:
  benchmark/run_sk2_ida.sh [options] stripped.elf [stripped.elf ...]

Options:
  --config FILE              Load SK2/IDA settings from FILE.
  --no-config                Do not load benchmark/sk2-ida.conf.
  --ida-bin PATH             IDA headless executable, e.g. idat64.
  --model-path PATH          Structure recovery Hugging Face model.
  --recover-model-path PATH  Identifier recovery Hugging Face model.
  --gpus N                   vLLM tensor parallel GPU count.
  --max-num-seqs N           vLLM max_num_seqs value retained in run metadata.
  --gpu-memory-utilization F vLLM GPU memory utilization.
  --quantization METHOD      vLLM quantization method, e.g. bitsandbytes.
  --load-format FORMAT       vLLM load format, e.g. bitsandbytes.
  --dtype DTYPE              vLLM dtype, e.g. bfloat16 or auto.
  --temperature F            Sampling temperature.
  --max-total-tokens N       vLLM max model length.
  --max-new-tokens N         Maximum generated tokens per phase.
  --clang-format PATH        clang-format executable.
  --output DIR               Run output directory.
  --trial ID                 Trial label.
  --check                    Validate local setup; do not run IDA or models.
  --prepare-only             Run IDA and dataset preparation; skip model inference.
  -h, --help                 Show this help.

Environment variables override config values:
  SK2_IDA_BIN, SK2_MODEL_PATH, SK2_RECOVER_MODEL_PATH, SK2_IDA_OUT,
  SK2_TRIAL, SK2_GPUS, SK2_GPU_MEMORY_UTILIZATION, SK2_QUANTIZATION,
  SK2_LOAD_FORMAT, SK2_DTYPE, SK2_MAX_TOTAL_TOKENS, SK2_MAX_NEW_TOKENS,
  SK2_CLANG_FORMAT.

Example:
  ./benchmark/run_sk2_ida.sh --prepare-only \
    artifacts_result_only/x86_64-linux-gnu-11.4.0/O0/01_basic_struct/stripped.elf
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
        --ida-bin)
            (($# >= 2)) || { echo "--ida-bin requires a value" >&2; exit 2; }
            ida_bin=$2
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
        --clang-format)
            (($# >= 2)) || { echo "--clang-format requires a value" >&2; exit 2; }
            clang_format=$2
            shift 2
            ;;
        --output)
            (($# >= 2)) || { echo "--output requires a value" >&2; exit 2; }
            out_root=$2
            shift 2
            ;;
        --trial)
            (($# >= 2)) || { echo "--trial requires a value" >&2; exit 2; }
            trial=$2
            shift 2
            ;;
        --check)
            check_only=1
            shift
            ;;
        --prepare-only)
            prepare_only=1
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

[[ $trial =~ ^[A-Za-z0-9._-]+$ ]] || {
    echo "trial may contain only letters, digits, dot, underscore, and hyphen" >&2
    exit 2
}

dump_script=$script_dir/dump_ida_pseudo_jsonl.py
prepare_script=$script_dir/prepare_sk2_dataset.py
sk2_script=$repo_dir/LLM4Decompile/sk2decompile/evaluation/sk2decompile_inf.py

if [[ -z $ida_bin ]]; then
    for candidate in idat64 ida64 idal64 idat ida; do
        if command -v "$candidate" >/dev/null; then
            ida_bin=$(command -v "$candidate")
            break
        fi
    done
fi

for tool in python3; do
    command -v "$tool" >/dev/null || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

[[ -n $ida_bin ]] || {
    echo "set IDA_BIN in $config_file, pass --ida-bin, or set SK2_IDA_BIN" >&2
    exit 2
}
[[ -x $ida_bin ]] || {
    echo "IDA binary not found or not executable: $ida_bin" >&2
    exit 2
}
command -v "$clang_format" >/dev/null || {
    echo "missing required clang-format executable: $clang_format" >&2
    exit 1
}
[[ -f $dump_script ]] || { echo "IDA dump script not found: $dump_script" >&2; exit 1; }
[[ -f $prepare_script ]] || { echo "dataset preparation script not found: $prepare_script" >&2; exit 1; }
[[ -f $sk2_script ]] || { echo "SK2 inference script not found: $sk2_script" >&2; exit 1; }

if command -v sha256sum >/dev/null; then
    hash_file() { sha256sum "$1" | cut -d' ' -f1; }
elif command -v shasum >/dev/null; then
    hash_file() { shasum -a 256 "$1" | cut -d' ' -f1; }
else
    echo "sha256sum or shasum is required" >&2
    exit 1
fi

if ((check_only)); then
    python3 - <<'PY'
import importlib.util
missing = [name for name in ("vllm", "transformers") if importlib.util.find_spec(name) is None]
if missing:
    raise SystemExit("missing Python module(s): " + ", ".join(missing))
PY
    printf '%s\n' \
        'SK2 IDA pipeline setup: OK' \
        "  config:              $config_file" \
        "  IDA:                 $ida_bin" \
        "  clang-format:        $clang_format" \
        "  model:               $model_path" \
        "  recovery model:      $recover_model_path" \
        "  gpus:                $gpus" \
        "  quantization:        ${quantization:-None}" \
        "  load format:         $load_format" \
        "  dtype:               $dtype" \
        "  max total tokens:    $max_total_tokens" \
        "  max new tokens:      $max_new_tokens" \
        "  output:              $out_root" \
        "  trial:               $trial"
    exit 0
fi

((${#binaries[@]})) || {
    echo "provide at least one stripped ELF path" >&2
    usage >&2
    exit 2
}

mkdir -p "$out_root"
if [[ ! -e $out_root/README.txt ]]; then
    printf '%s\n' \
        'SK2 IDA benchmark outputs' \
        '' \
        'Each case directory is keyed by the stripped-binary hash.' \
        'Within a trial, read status.txt first, then dataset.json and sk2_output/.' \
        'IDA runs against a temporary copy of the binary to avoid leaving databases near artifacts.' \
        >"$out_root/README.txt"
fi
manifest=$out_root/manifest.tsv
if [[ ! -e $manifest ]]; then
    printf 'case_id\ttrial\tstatus\tbinary_path\tbinary_sha256\tida_jsonl_sha256\tdataset_sha256\tmodel_path\trecover_model_path\tstarted_utc\tfinished_utc\twall_seconds\n' \
        >"$manifest"
fi

scratch_root=$(mktemp -d "${TMPDIR:-/tmp}/sk2-ida.XXXXXX")
cleanup()
{
    if [[ -n ${scratch_root:-} && -d $scratch_root ]]; then
        rm -rf -- "$scratch_root"
    fi
}
trap cleanup EXIT HUP INT TERM

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
    case_id=case-${binary_hash:0:16}-sk2-ida
    case_out=$out_root/$case_id/$trial
    if [[ -e $case_out ]]; then
        echo "refusing to overwrite existing run: $case_out" >&2
        echo "choose another --trial or --output" >&2
        overall_status=1
        continue
    fi
    mkdir -p "$case_out"
    printf '%s\n' \
        'Files in this trial' \
        '' \
        'status.txt              final validity/completion status' \
        'ida_pseudo.jsonl        raw per-function Hex-Rays output from IDA' \
        'ida.stdout              IDA stdout' \
        'ida.stderr              IDA stderr' \
        'dataset.json            normalized SK2 inference dataset' \
        'normalized_stats.txt    dataset preparation counts' \
        'sk2.stdout              SK2 inference stdout' \
        'sk2.stderr              SK2 inference stderr' \
        'sk2_output/             SK2 generated functions and logs' \
        '' \
        'The parent output manifest is evaluator-only metadata.' \
        >"$case_out/README.txt"

    started_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
    started_epoch=$(date '+%s')
    prep_dir=$scratch_root/$case_id-$trial-prep
    mkdir -p "$prep_dir"
    cp -- "$binary" "$prep_dir/program.elf"

    ida_jsonl=$case_out/ida_pseudo.jsonl
    symbol_args=()
    sibling_nodebug=$(dirname -- "$binary")/nodebug.elf
    sibling_oracle=$(dirname -- "$binary")/oracle.elf
    if [[ -f $sibling_nodebug ]]; then
        symbol_args=(--symbol-binary "$sibling_nodebug")
    elif [[ -f $sibling_oracle ]]; then
        symbol_args=(--symbol-binary "$sibling_oracle")
    fi
    set +e
    "$ida_bin" -A "-S$dump_script $ida_jsonl" "$prep_dir/program.elf" \
        >"$case_out/ida.stdout" 2>"$case_out/ida.stderr"
    ida_status=$?
    set -e

    status=PREPARED
    if ((ida_status != 0)) || [[ ! -s $ida_jsonl ]]; then
        status=IDA_FAILED
    else
        set +e
        python3 "$prepare_script" \
            --ida-jsonl "$ida_jsonl" \
            --binary-path "$binary" \
            --output-json "$case_out/dataset.json" \
            --stats "$case_out/normalized_stats.txt" \
            --clang-format "$clang_format" \
            "${symbol_args[@]}" \
            >"$case_out/prepare.stdout" 2>"$case_out/prepare.stderr"
        prepare_status=$?
        set -e
        if ((prepare_status != 0)) || [[ ! -s $case_out/dataset.json ]]; then
            status=NORMALIZE_FAILED
        fi
    fi

    if [[ $status == PREPARED && $prepare_only -eq 0 ]]; then
        set +e
        (
            cd "$repo_dir/LLM4Decompile/sk2decompile/evaluation"
            python3 "$sk2_script" \
                --dataset_path "$case_out/dataset.json" \
                --model_path "$model_path" \
                --recover_model_path "$recover_model_path" \
                --decompiler ida_pseudo_norm \
                --gpus "$gpus" \
                --max_num_seqs "$max_num_seqs" \
                --gpu_memory_utilization "$gpu_memory_utilization" \
                --quantization "$quantization" \
                --load_format "$load_format" \
                --dtype "$dtype" \
                --temperature "$temperature" \
                --max_total_tokens "$max_total_tokens" \
                --max_new_tokens "$max_new_tokens" \
                --output_path "$case_out/sk2_output"
        ) >"$case_out/sk2.stdout" 2>"$case_out/sk2.stderr"
        sk2_status=$?
        set -e
        if ((sk2_status != 0)); then
            status=SK2_FAILED
        elif [[ ! -s $case_out/sk2_output/inference_results.jsonl ]]; then
            status=EMPTY_SK2_OUTPUT
        else
            status=COMPLETE
        fi
    fi

    ida_hash=-
    dataset_hash=-
    [[ -s $ida_jsonl ]] && ida_hash=$(hash_file "$ida_jsonl")
    [[ -s $case_out/dataset.json ]] && dataset_hash=$(hash_file "$case_out/dataset.json")
    finished_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
    finished_epoch=$(date '+%s')
    wall_seconds=$((finished_epoch - started_epoch))
    printf '%s\n' "$status" >"$case_out/status.txt"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$case_id" "$trial" "$status" "$binary" "$binary_hash" "$ida_hash" \
        "$dataset_hash" "$model_path" "$recover_model_path" "$started_utc" \
        "$finished_utc" "$wall_seconds" >>"$manifest"

    if [[ $status == COMPLETE || $status == PREPARED ]]; then
        echo "$status $case_id: $case_out"
    else
        echo "failed run ($status): $case_out" >&2
        overall_status=1
    fi
done

exit "$overall_status"
