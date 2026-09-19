#!/usr/bin/env bash
set -euo pipefail

# Differential success-path profile. All fixtures live below /tmp and are
# removed when the script exits; no benchmark data file is modified.
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mcpp_bin="${MCPP_BIN:-mcpp}"
iterations="10000"
warmup="3"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --iterations) iterations="$2"; shift 2 ;;
        --warmup) warmup="$2"; shift 2 ;;
        --help)
            printf '%s\n' '--iterations N --warmup N'
            exit 0
            ;;
        *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done

source "$root_dir/benchmark/build_release.sh"
target_dir="$(build_release_dir "$root_dir" "$mcpp_bin")"

fixture_dir="$(mktemp -d /tmp/serde-allocation-profile.XXXXXX)"
trap 'rm -rf "$fixture_dir"' EXIT

emit_tags() {
    local count="$1"
    printf '['
    for ((index = 0; index < count; ++index)); do
        ((index == 0)) || printf ','
        printf '"tag-%d"' "$index"
    done
    printf ']'
}

emit_limits() {
    local count="$1"
    printf '{'
    for ((index = 0; index < count; ++index)); do
        ((index == 0)) || printf ','
        printf '"limit-%d":%d' "$index" "$index"
    done
    printf '}'
}

emit_servers() {
    local count="$1"
    local zones="$2"
    printf '['
    for ((server = 0; server < count; ++server)); do
        ((server == 0)) || printf ','
        printf '{"host":"host-%d","port":%d,"zones":[' "$server" "$((9000 + server))"
        for ((zone = 0; zone < zones; ++zone)); do
            ((zone == 0)) || printf ','
            printf '"zone-%d-%d"' "$server" "$zone"
        done
        printf ']}'
    done
    printf ']'
}

emit_message() {
    local count="$1"
    if ((count > 0)); then
        printf '%*s' "$count" '' | tr ' ' 'm'
    fi
}

write_json() {
    local path="$1" message_size="$2" tag_count="$3" limit_count="$4"
    local server_count="$5" zone_count="$6"
    {
        printf '{"application":"app","enabled":true,"retries":5,"ratio":0.875,"tags":'
        emit_tags "$tag_count"
        printf ',"limits":'
        emit_limits "$limit_count"
        printf ',"owner":{"name":"owner","team":"team"},"servers":'
        emit_servers "$server_count" "$zone_count"
        printf ',"message":"'
        emit_message "$message_size"
        printf '"}\n'
    } > "$path"
}

write_toml() {
    local path="$1" message_size="$2" tag_count="$3" limit_count="$4"
    local server_count="$5" zone_count="$6"
    {
        printf 'application = "app"\nenabled = true\nretries = 5\nratio = 0.875\ntags = ['
        for ((index = 0; index < tag_count; ++index)); do
            ((index == 0)) || printf ', '
            printf '"tag-%d"' "$index"
        done
        printf ']\nmessage = "'
        emit_message "$message_size"
        printf '"\n'
        if ((server_count == 0)); then
            printf '\nservers = []\n'
        fi
        printf '\n[limits]\n'
        for ((index = 0; index < limit_count; ++index)); do
            printf 'limit-%d = %d\n' "$index" "$index"
        done
        printf '\n[owner]\nname = "owner"\nteam = "team"\n'
        for ((server = 0; server < server_count; ++server)); do
            printf '\n[[servers]]\nhost = "host-%d"\nport = %d\nzones = [' "$server" "$((9000 + server))"
            for ((zone = 0; zone < zone_count; ++zone)); do
                ((zone == 0)) || printf ', '
                printf '"zone-%d-%d"' "$server" "$zone"
            done
            printf ']\n'
        done
    } > "$path"
}

profile_one() {
    local format="$1" label="$2" path="$3" binary="$4" prefix="$5"
    local output row calls bytes mean
    output="$("$target_dir/$binary" "--$format" "$path" --phase decode-only \
        --iterations "$iterations" --warmup "$warmup" --allocations 2>&1)"
    row="$(printf '%s\n' "$output" | sed -n '/: bytes=/p')"
    calls="$(printf '%s\n' "$output" | sed -n 's/.*allocation_calls=\([0-9][0-9]*\).*/\1/p')"
    bytes="$(printf '%s\n' "$output" | sed -n 's/.*allocation_bytes=\([0-9][0-9]*\).*/\1/p')"
    mean="$(printf '%s\n' "$row" | sed -n 's/.*mean_ns=\([0-9.][0-9.]*\).*/\1/p')"
    awk -v format="$format" -v label="$label" -v calls="$calls" -v bytes="$bytes" \
        -v mean="$mean" -v iterations="$iterations" \
        'BEGIN { printf "%s,%s,%.3f,%.3f,%s\n", format, label, calls / iterations, bytes / iterations, mean }'
}

printf '%s\n' 'format,fixture,allocation_calls_per_op,allocation_bytes_per_op,mean_ns'

declare -a fixtures=(
    scalar long-string tags-vector limits-map servers-vector zones-vector full-default medium large)
declare -a sizes=(0 4096 0 0 0 0 24 256 4096)
declare -a tags=(0 0 32 0 0 0 3 16 64)
declare -a limits=(0 0 0 32 0 0 2 16 64)
declare -a servers=(0 0 0 0 16 1 2 8 64)
declare -a zones=(0 0 0 0 0 32 2 4 8)

for fixture_index in "${!fixtures[@]}"; do
    label="${fixtures[$fixture_index]}"
    json_path="$fixture_dir/$label.json"
    toml_path="$fixture_dir/$label.toml"
    write_json "$json_path" "${sizes[$fixture_index]}" "${tags[$fixture_index]}" "${limits[$fixture_index]}" \
        "${servers[$fixture_index]}" "${zones[$fixture_index]}"
    write_toml "$toml_path" "${sizes[$fixture_index]}" "${tags[$fixture_index]}" "${limits[$fixture_index]}" \
        "${servers[$fixture_index]}" "${zones[$fixture_index]}"
    if [[ "$label" == "full-default" ]]; then
        cp "$root_dir/benchmark/data/config.json" "$json_path"
        cp "$root_dir/benchmark/data/config.toml" "$toml_path"
    fi
    profile_one json "$label" "$json_path" benchmark_serde_json_alloc serde-json
    profile_one toml "$label" "$toml_path" benchmark_serde_toml_alloc serde-toml
done
