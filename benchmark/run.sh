#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mcpp_bin="${MCPP_BIN:-mcpp}"
iterations="10000"
warmup="3"
json_input="${root_dir}/benchmark/data/config.json"
toml_input="${root_dir}/benchmark/data/config.toml"
csv_file=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --iterations) iterations="$2"; shift 2 ;;
        --warmup) warmup="$2"; shift 2 ;;
        --json) json_input="$2"; shift 2 ;;
        --toml) toml_input="$2"; shift 2 ;;
        --csv) csv_file="$2"; shift 2 ;;
        --help)
            printf '%s\n' '--iterations N --warmup N --json PATH --toml PATH --csv PATH'
            exit 0
            ;;
        *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done

cd "$root_dir"
source "$root_dir/benchmark/build_release.sh"
target_dir="$(build_release_dir "$root_dir" "$mcpp_bin")"

args=(--iterations "$iterations" --warmup "$warmup")
if [[ -n "$csv_file" ]]; then
    : > "$csv_file"
    printf '%s\n' 'name,bytes,iterations,total_seconds,mean_nanoseconds,mib_per_second,checksum' >> "$csv_file"
    args+=(--csv)
fi

run_one() {
    local binary="$1"
    shift
    if [[ ! -x "$target_dir/$binary" ]]; then
        printf 'missing benchmark binary: %s/%s\n' "$target_dir" "$binary" >&2
        exit 1
    fi
    if [[ -n "$csv_file" ]]; then
        "$target_dir/$binary" "${args[@]}" "$@" >> "$csv_file"
    else
        "$target_dir/$binary" "${args[@]}" "$@"
    fi
}

run_one benchmark_serde_json --json "$json_input"
run_one benchmark_simdjson --json "$json_input"
run_one benchmark_serde_toml --toml "$toml_input"
run_one benchmark_tomlplusplus --toml "$toml_input"
