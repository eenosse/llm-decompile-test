#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

cd "$repo_dir"
OUT_DIR=artifacts_result_only \
    ./benchmark/build_matrix.sh result_only/[0-9][0-9]_*.c
