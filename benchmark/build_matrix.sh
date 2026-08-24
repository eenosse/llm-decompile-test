#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

# macOS cannot produce the intended Linux ELF artifacts with its native toolchain.
# Re-enter this script in the pinned container unless explicitly disabled.
if [[ $(uname -s) != Linux && ${TYPEBENCH_NO_DOCKER:-0} != 1 ]]; then
    command -v docker >/dev/null || {
        echo "docker is required on non-Linux hosts" >&2
        exit 1
    }
    image=${TYPEBENCH_IMAGE:-type-recovery-bench:ubuntu22.04}
    if [[ -n ${OUT_DIR:-} ]]; then
        if [[ $OUT_DIR = /* ]]; then
            case $OUT_DIR in
                "$repo_dir"/*) container_out=/work/${OUT_DIR#"$repo_dir"/} ;;
                *) echo "OUT_DIR must be inside $repo_dir when using Docker" >&2; exit 1 ;;
            esac
        else
            container_out=/work/$OUT_DIR
        fi
    else
        container_out=/work/artifacts
    fi
    docker build --platform linux/amd64 -t "$image" -f "$script_dir/Dockerfile" "$script_dir"
    docker run --rm --platform linux/amd64 \
        -e TYPEBENCH_NO_DOCKER=1 \
        -e COMPILERS="${COMPILERS:-gcc}" \
        -e OPT_LEVELS="${OPT_LEVELS:-0 1 2 3}" \
        -e OUT_DIR="$container_out" \
        -v "$repo_dir:/work" -w /work "$image" \
        ./benchmark/build_matrix.sh "$@"
    exit
fi

out_root=${OUT_DIR:-$repo_dir/artifacts}
compilers=${COMPILERS:-gcc}
opt_levels=${OPT_LEVELS:-0 1 2 3}

if (($#)); then
    sources=("$@")
else
    shopt -s nullglob
    sources=("$repo_dir"/*.c)
fi

((${#sources[@]})) || {
    echo "no C sources found" >&2
    exit 1
}

for tool in objcopy strip readelf sha256sum timeout; do
    command -v "$tool" >/dev/null || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

mkdir -p "$out_root"
manifest="$out_root/manifest.tsv"
printf 'compiler\toptimization\tsource\tsource_sha256\tvariant\tsha256\tstdout_sha256\n' >"$manifest"

for compiler in $compilers; do
    command -v "$compiler" >/dev/null || {
        echo "missing compiler: $compiler" >&2
        exit 1
    }
    compiler_id=$($compiler -dumpmachine)-$($compiler -dumpfullversion -dumpversion)
    compiler_dir="$out_root/$compiler_id"
    mkdir -p "$compiler_dir"
    "$compiler" --version >"$compiler_dir/compiler-version.txt"

    for opt in $opt_levels; do
        case "$opt" in
            0|1|2|3) ;;
            *) echo "unsupported optimization level: $opt" >&2; exit 1 ;;
        esac

        for source_arg in "${sources[@]}"; do
            if [[ $source_arg = /* ]]; then
                source=$source_arg
            else
                source=$repo_dir/$source_arg
            fi
            [[ -f $source ]] || {
                echo "source not found: $source_arg" >&2
                exit 1
            }

            stem=$(basename -- "$source" .c)
            source_hash=$(sha256sum "$source" | cut -d' ' -f1)
            case_dir="$compiler_dir/O$opt/$stem"
            mkdir -p "$case_dir"
            full="$case_dir/oracle.elf"
            nodebug="$case_dir/nodebug.elf"
            stripped="$case_dir/stripped.elf"
            debug="$case_dir/oracle.debug"

            compile_args=(
                -std=gnu11
                "-O$opt"
                -g3
                -gdwarf-4
                -fno-lto
                -fno-pie
                -no-pie
                -Wall
                -Wextra
                -Wpedantic
                -Werror=implicit-function-declaration
                -Wl,--build-id=none
                "$source"
                -o "$full"
            )
            printf '%q ' "$compiler" "${compile_args[@]}" >"$case_dir/compile-command.txt"
            printf '\n' >>"$case_dir/compile-command.txt"
            "$compiler" "${compile_args[@]}" 2>"$case_dir/compile.stderr"

            # Keep ground truth out-of-band. Do not add a .gnu_debuglink to either
            # test input because even its filename is avoidable side information.
            objcopy --only-keep-debug "$full" "$debug"
            objcopy --strip-debug "$full" "$nodebug"
            cp -- "$full" "$stripped"
            strip --strip-all "$stripped"

            if readelf -S "$stripped" | grep -Eq '\.(debug_|zdebug_|symtab)'; then
                echo "strip verification failed: $stripped" >&2
                exit 1
            fi

            for variant in oracle nodebug stripped; do
                binary="$case_dir/$variant.elf"
                stdout="$case_dir/$variant.stdout"
                timeout 20s "$binary" >"$stdout"
                binary_hash=$(sha256sum "$binary" | cut -d' ' -f1)
                stdout_hash=$(sha256sum "$stdout" | cut -d' ' -f1)
                printf '%s\tO%s\t%s.c\t%s\t%s\t%s\t%s\n' \
                    "$compiler_id" "$opt" "$stem" "$source_hash" "$variant" \
                    "$binary_hash" "$stdout_hash" >>"$manifest"
            done

            cmp --silent "$case_dir/oracle.stdout" "$case_dir/nodebug.stdout"
            cmp --silent "$case_dir/oracle.stdout" "$case_dir/stripped.stdout"
            file "$full" "$nodebug" "$stripped" >"$case_dir/file.txt"
            readelf -SW "$full" >"$case_dir/oracle-sections.txt"
            readelf -SW "$stripped" >"$case_dir/stripped-sections.txt"
        done
    done
done

echo "built ${#sources[@]} source file(s); manifest: $manifest"
