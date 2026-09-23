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
other files. Both scripts select binaries using the fingerprint reported by
the release build, so a newer debug build cannot be selected.
Input loading and fixture construction happen outside the timed
region. Each result reports total time, mean time, operations per second,
MiB/s, parsed bytes, and a checksum so the compiler cannot discard parsing.

The four default tracks perform different work. serde builds an owning typed
document, while the reference tracks build a DOM and walk selected fields for
the checksum. simdjson also receives pre-padded input and reuses its parser;
serde applies its configured document limits and duplicate-key policy. Do not
interpret their ratio as wrapper overhead alone. Use the supplementary phases
below to separate parsing, validation, and typed decoding costs.

The benchmark binaries are separate targets; `mcpp test` does not run them.
toml++ is supplied by the adjacent `tomlplusplus-3.4.0` directory; simdjson is
vendored in this repository.

For a paired comparison of two release builds, use Node.js 18 or newer:

```sh
node benchmark/compare.mjs BASELINE_BIN_DIR CANDIDATE_BIN_DIR /tmp/serde-comparison 0
```

The optional last argument pins each process to that CPU. The script generates
medium, large, and 4096-key fixtures and also runs `benchmark_serde_json_wide`,
which decodes 64 reflected fields from reverse-order input. It runs the binaries
sequentially for five alternating baseline/candidate rounds, checks matching checksums, and
reports median nanoseconds and speedup for all three phases. Raw samples and
fixtures remain in the output directory. Neither build should run concurrently
with the measurements. The 4096-key fixture exercises a map of keys; the
64-field track measures reflected C++ member lookup. Copy the wide benchmark
source and target entry into older baseline checkouts before comparing.

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

## JSON deserialize 结构检查（相对 v0.2.2）

下面的数字不是 `benchmark/run.sh` 的四行输出，而是同一进程里对
`json::deserialize` 的改前/改后对照。编译器是 clang 22.1.8，选项为
`-O3 -DNDEBUG -std=c++23 -stdlib=libc++ -fno-exceptions`，并定义
`SIMDJSON_EXCEPTIONS=0`。计时区间只包含反序列化循环；样本在计时前生成。
预热 5 次。2 台与 64 字段各 10000 次，200 台 2000 次，2000 台 200 次。

200 台服务器不是仓库里的 `benchmark/data/config.json`（那份只有 2 台，约 380 字节）。
样本按该文件的文档形状在内存中生成：外层字段保持不变，`servers` 重复 N 次。
每一项为 `{"host":"cache-<i>","port":<9000+i>,"zones":["cn-sh","us-west"]}`。

| 样本 | 字节 | v0.2.2 平均时间 | 本次平均时间 |
| --- | ---: | ---: | ---: |
| 2 台服务器 | 343 | 1481 ns | 1388 ns |
| 200 台服务器 | 12315 | 73 µs | 49 µs |
| 2000 台服务器 | 124115 | 528 µs | 485 µs |
| 64 个整数字段，键序 `f63` 到 `f00` | 567 | 5579 ns | 5579 ns |

默认 `deserialize` 现在的行为是：输入字节数不超过 `max_nodes`、`max_string_bytes`、
`max_key_bytes`、`max_array_items` 和 `max_object_members` 时，跳过完整的
`enforce_limits`。仍要拒绝重复键，或输入长于 `max_depth` 时，只扫描对象键和嵌套深度。
深度仍由解析器的 `max_depth` 与这次扫描共同约束。用户把某项上限收紧到输入放不下时，
继续走原来的完整检查。`json::parse` 的检查路径没有改变。64 字段整数样本几乎不变，
因为时间主要在建 DOM 和按字段填结构，不在字符串长度检查上。
