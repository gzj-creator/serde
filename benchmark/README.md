# serde parser benchmarks

This directory contains opt-in parser benchmarks for the serde JSON and TOML
modules. The reference implementations are kept in separate executables:

- `benchmark_serde_json` measures `json::deserialize<benchmark_document>`.
- `benchmark_simdjson` measures simdjson DOM parsing plus the same checksum walk.
- `benchmark_serde_toml` measures `toml::deserialize<benchmark_document>`.
- `benchmark_tomlplusplus` measures toml++ parsing plus the same checksum walk.

The serde and reference libraries are intentionally not included in one
translation unit. serde exports `namespace toml`, which would collide with the
toml++ namespace. simdjson is vendored under `third_party/simdjson` and compiled
into the serde library; the standalone `benchmark_simdjson` still includes that
header directly so the comparison stays in a separate binary.

Build and run all four benchmarks with:

```sh
benchmark/run.sh --iterations 10000 --warmup 3
```

The script reads `benchmark/data/config.json` and
`benchmark/data/config.toml` by default. Pass `--json` and `--toml` to use
other files. Input loading and fixture construction happen outside the timed
region. Each result reports total time, mean time, operations per second,
MiB/s, parsed bytes, and a checksum so the compiler cannot discard parsing.

The comparison is end-to-end parsing into a usable representation. serde's
public API includes typed reflection decoding, so the reference benchmark also
walks the parsed DOM and accumulates equivalent fields. This is not a claim
that the libraries have identical feature sets; malformed-input behavior,
allocation strategy, and schema conversion are separate concerns documented
in `docs/benchmark-plan.md`.

The benchmark binaries are separate targets; `mcpp test` does not run them.
Third-party source remains outside this repository and is never copied into
the serde library.

## Phase and allocation profiles

The default command remains end-to-end and keeps the CSV schema above. For
phase measurements, invoke either serde binary directly with one of:

```sh
target/.../bin/benchmark_serde_json --phase parse-only --iterations 10000 --warmup 3
target/.../bin/benchmark_serde_json --phase decode-only --iterations 10000 --warmup 3
target/.../bin/benchmark_serde_toml --phase parse-only --iterations 10000 --warmup 3
target/.../bin/benchmark_serde_toml --phase decode-only --iterations 10000 --warmup 3
target/.../bin/benchmark_simdjson --phase parse-only --iterations 10000 --warmup 3
target/.../bin/benchmark_simdjson --phase walk-only --iterations 10000 --warmup 3
target/.../bin/benchmark_simdjson --phase dom-to-document --iterations 10000 --warmup 3
```

`parse-only` performs the same input validation and DOM construction used by
`deserialize`; `decode-only` parses one checked document before timing and
measures typed reflection decoding from that immutable DOM. Phase checksums
only keep the measured result live, so compare phase timings within a format,
not with the end-to-end checksum.

The simdjson supplementary phases use one padded input and one reusable parser
in the same way as the default `simdjson-dom` track. `parse-only` times parser
work, `walk-only` times the existing DOM checksum walk over a checked DOM, and
`dom-to-document` times conversion from that DOM into the benchmark's typed
document, including strings, vectors, and map nodes. These tracks are opt-in;
the default `run.sh` output remains four rows.

`benchmark_serde_json_alloc` and `benchmark_serde_toml_alloc` are separate
profiling binaries. Add `--allocations` to report allocation calls and requested
bytes for the timed loop. They intercept allocation only in those binaries and
are deliberately excluded from `run.sh`, so the normal throughput results do
not include profiler overhead.

`benchmark/profile_allocations.sh` creates scalar, long-string, vector, map,
server, nested-zone, default, medium, and large fixtures under `/tmp`, then
reports allocation calls, requested bytes, and mean nanoseconds per operation
for JSON and TOML decode-only. The fixtures are removed on exit and are not
part of the repository.

Cacheline counters were checked before adding representation experiments.
Valgrind/cachegrind is unavailable in the measurement environment, and
`perf_event_paranoid=4` blocks hardware cache events. No cacheline conclusion
is inferred from timing alone; map/node representation work remains deferred.

The default four-row command is unchanged. Default JSON `parse-only` measures
a fresh `json::parse` each iteration. `--reuse-context` switches to a reused
`json::Parser`. The old bytewise/structural custom parser phases were removed.
