#!/usr/bin/env bash

build_release_dir() {
    local source_root="$1" builder="$2" build_output fingerprint target_dir
    if ! build_output="$(cd "$source_root" && "$builder" build --profile release --print-fingerprint)"; then
        printf '%s\n' "$build_output" >&2
        return 1
    fi
    printf '%s\n' "$build_output" >&2
    fingerprint="$(sed -n 's/^Fingerprint: \([0-9a-f][0-9a-f]*\)$/\1/p' <<< "$build_output")"
    if [[ -z "$fingerprint" || "$fingerprint" == *$'\n'* ]]; then
        printf '%s\n' 'cannot identify the release build fingerprint' >&2
        return 1
    fi
    target_dir="$(find "$source_root/target" -type f \
        -path "*/$fingerprint/bin/benchmark_serde_json" -perm -111 -printf '%h\n')"
    if [[ -z "$target_dir" || "$target_dir" == *$'\n'* ]]; then
        printf 'cannot locate a unique release benchmark directory for %s\n' "$fingerprint" >&2
        return 1
    fi
    printf '%s\n' "$target_dir"
}
