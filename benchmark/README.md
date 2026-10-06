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

## v0.4.0 字段契约发布验收

新 CMake 入口从安装包消费公开头文件和 `serde::serde`，不需要 import std。
同一份 benchmark 可以分别链接基线/候选安装目录，避免测试夹具或编译选项不同：

```sh
cmake -S benchmark -B build/bench -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-fno-exceptions \
  -Dserde_DIR=INSTALL_PREFIX/lib/cmake/serde
cmake --build build/bench --parallel 1
node benchmark/compare.mjs BASELINE_BIN CANDIDATE_BIN OUTPUT_DIR 0
cmake --build build/bench --target benchmark_serde_contract benchmark_serde_contract_alloc --parallel 1
taskset -c 0 build/bench/benchmark_serde_contract --iterations 3000000 --warmup 100
taskset -c 0 build/bench/benchmark_serde_contract_alloc --iterations 1000000 --warmup 100 --allocations
```

发布基线为 `bb02aeb`，含 v0.3.0 后的安装修复。工具链为 GCC14.2、系统
libstdc++、`-O3 -DNDEBUG -std=c++23 -fno-exceptions`；Xeon Platinum 8255C
虚拟机上绑定 CPU0，计时不与构建并发。门槛在测量前固定：既有未配置约束
路径的端到端及 decode-only 中位耗时回退不超过10%，解码分配次数和请求字节
不增加，新校验成功路径零堆分配；默认描述符实例尺寸等于原 name+pointer 布局。

两次完整测量各5轮交替执行，全部保留而非取最好一轮。第二次测量中个别
TOML 默认解码和 JSON 4096键端到端样本超过10%，未改动的 parse-only 对照
也出现明显波动；因此合并全部10轮取中位数，并对这两个争议场景另做更长循环。
下表为合并中位耗时，单位 ns，正变化表示变慢：

| 格式/场景 | 基线 decode | 候选 decode | decode变化 | 端到端变化 |
| --- | ---: | ---: | ---: | ---: |
| JSON default | 1251.77 | 1275.48 | +1.89% | -0.30% |
| JSON medium | 4507.60 | 4575.05 | +1.50% | -3.85% |
| JSON large | 35639.54 | 35555.22 | -0.24% | +0.92% |
| JSON 4096键map | 832427.37 | 838603.29 | +0.74% | +6.74% |
| JSON 64反射字段 | 2689.99 | 2506.16 | -6.83% | -0.80% |
| TOML default | 1126.79 | 1107.55 | -1.71% | +0.01% |
| TOML medium | 3876.16 | 3853.89 | -0.57% | +0.72% |
| TOML large | 30839.88 | 30853.33 | +0.04% | -0.48% |
| TOML 4096键map | 673926.56 | 670021.87 | -0.58% | -0.28% |

争议场景加长为7轮：TOML default decode-only 每轮100万次，基线1321.71ns、
候选1120.49ns（-15.22%）；JSON 4096键端到端每轮300次，基线1782972.34ns、
候选1653386.46ns（-7.27%）。两项均未复现持续回退，全部样本保留。
虚拟机调度和频率波动明显，不将这些额外改善解释为固定的算法加速幅度。

JSON/TOML 的每次解码分配均与基线完全一致：default 7次/521字节，medium
27次/3457字节，large 131次/31233字节，4096键map 4096次/294912字节。
6字段约束校验（含精确整数、浮点、字符串、optional、容器和enum）5轮中位
62.44ns；3值enum名称往返49.35ns。两者各100万次成功调用均为0次/0字节堆分配。
这些是固定夹具的校验成本，不是带约束codec整体耗时，也不代表任意大小enum或字符串成本。

Galay验收工作区保留原始结果在 `build/serde-release-comparison-final-20261006/`
（results.json、combined.json、features.json、investigation.json）和
`build/serde-release-comparison-confirm-20261006/results.json`。不将构建产物提交。
这些数据证明所测路径通过门槛，不宣称所有工具链和工作负载上的绝对最优。
