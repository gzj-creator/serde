#pragma once

import std;

#if defined(SERDE_BENCHMARK_ALLOCATION_PROFILE)
namespace benchmark::detail {

struct AllocationTotals {
    std::uint64_t calls{};
    std::uint64_t bytes{};
};

inline thread_local AllocationTotals* active_AllocationTotals = nullptr;

inline void record_allocation(std::size_t bytes) noexcept {
    if (auto* totals = active_AllocationTotals) {
        ++totals->calls;
        totals->bytes += bytes;
    }
}

inline void* allocate(std::size_t bytes) {
    const auto allocation_size = bytes == 0 ? std::size_t{1} : bytes;
    if (void* pointer = std::malloc(allocation_size)) {
        record_allocation(bytes);
        return pointer;
    }
    throw std::bad_alloc{};
}

inline void* allocate_aligned(std::size_t bytes, std::size_t alignment) {
    const auto allocation_size = bytes == 0 ? alignment : bytes;
    if (allocation_size > std::numeric_limits<std::size_t>::max() - (alignment - 1)) {
        throw std::bad_alloc{};
    }
    const auto rounded_size =
        ((allocation_size + alignment - 1) / alignment) * alignment;
    if (void* pointer = std::aligned_alloc(alignment, rounded_size)) {
        record_allocation(bytes);
        return pointer;
    }
    throw std::bad_alloc{};
}

class AllocationScope {
public:
    explicit AllocationScope(AllocationTotals* totals) noexcept
        : previous_(std::exchange(active_AllocationTotals, totals)) {}

    ~AllocationScope() { active_AllocationTotals = previous_; }

private:
    AllocationTotals* previous_;
};

inline constexpr bool allocation_profile_available = true;

}  // namespace benchmark::detail

void* operator new(std::size_t bytes) {
    return benchmark::detail::allocate(bytes);
}

void* operator new[](std::size_t bytes) {
    return benchmark::detail::allocate(bytes);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void* operator new(std::size_t bytes, std::align_val_t alignment) {
    return benchmark::detail::allocate_aligned(bytes, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t bytes, std::align_val_t alignment) {
    return benchmark::detail::allocate_aligned(bytes, static_cast<std::size_t>(alignment));
}

void operator delete(void* pointer, std::align_val_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::align_val_t) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}
#else
namespace benchmark::detail {

struct AllocationTotals {
    std::uint64_t calls{};
    std::uint64_t bytes{};
};

class AllocationScope {
public:
    explicit AllocationScope(AllocationTotals*) noexcept {}
};

inline constexpr bool allocation_profile_available = false;

}  // namespace benchmark::detail
#endif

namespace benchmark {

enum class phase { end_to_end, parse_only, decode_only, index_only, stage2_only };

struct options {
    std::string input_path;
    std::size_t iterations = 10000;
    std::size_t warmup = 3;
    bool csv = false;
    bool allocations = false;
    bool reuse_context = false;
    bool structural_index = false;
    phase selected_phase = phase::end_to_end;
};

inline std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open benchmark input: " + path);
    }
    return {std::istreambuf_iterator<char>{input}, {}};
}

inline options parse_options(int argc, char** argv, std::string default_path,
                             bool json_tracks = false) {
    options result;
    result.input_path = std::move(default_path);
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        auto next_value = [&](std::string_view name) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error("missing value for " + std::string(name));
            }
            return argv[++index];
        };
        if (argument == "--input" || argument == "--json" || argument == "--toml") {
            result.input_path = next_value(argument);
        } else if (argument == "--iterations") {
            result.iterations = std::stoull(next_value(argument));
        } else if (argument == "--warmup") {
            result.warmup = std::stoull(next_value(argument));
        } else if (argument == "--phase") {
            const auto selected = next_value(argument);
            if (selected == "end-to-end") {
                result.selected_phase = phase::end_to_end;
            } else if (selected == "parse-only") {
                result.selected_phase = phase::parse_only;
            } else if (selected == "decode-only") {
                result.selected_phase = phase::decode_only;
            } else if (json_tracks && selected == "index-only") {
                result.selected_phase = phase::index_only;
            } else if (json_tracks && selected == "stage2-only") {
                result.selected_phase = phase::stage2_only;
            } else {
                throw std::runtime_error("unknown benchmark phase: " + selected);
            }
        } else if (argument == "--allocations") {
            result.allocations = true;
        } else if (json_tracks && argument == "--reuse-context") {
            result.reuse_context = true;
        } else if (json_tracks && argument == "--json-parser") {
            const auto selected = next_value(argument);
            if (selected == "structural") result.structural_index = true;
            else if (selected == "bytewise") result.structural_index = false;
            else throw std::runtime_error("unknown JSON parser: " + selected);
        } else if (argument == "--csv") {
            result.csv = true;
        } else if (argument == "--help") {
            std::println("--input/--json/--toml PATH  input fixture");
            std::println("--iterations N              measured iterations");
            std::println("--warmup N                 warmup iterations");
            std::println("--phase NAME               end-to-end, parse-only, or decode-only");
            std::println("--allocations              report timed allocation counts (profile binary)");
            if (json_tracks) {
                std::println("--json-parser NAME         bytewise (default) or structural");
                std::println("--reuse-context            reuse internal JSON node and scratch storage");
                std::println("--phase index-only/stage2-only  supplementary structural parser phases");
            }
            std::println("--csv                     print CSV row");
            std::exit(0);
        } else {
            throw std::runtime_error("unknown benchmark option: " + std::string(argument));
        }
    }
    if (result.iterations == 0) {
        throw std::runtime_error("iterations must be greater than zero");
    }
    if (result.allocations && !detail::allocation_profile_available) {
        throw std::runtime_error("--allocations requires a *_alloc benchmark binary");
    }
    if ((result.selected_phase == phase::index_only ||
         result.selected_phase == phase::stage2_only) && !result.structural_index) {
        throw std::runtime_error("index-only and stage2-only require --json-parser structural");
    }
    return result;
}

struct result {
    std::string name;
    std::size_t bytes{};
    std::size_t iterations{};
    double seconds{};
    std::uint64_t checksum{};
    std::uint64_t allocation_calls{};
    std::uint64_t allocation_bytes{};
    bool allocations_profiled{};
};

inline void print_result(const result& value, bool csv) {
    const auto mean_ns = value.seconds * 1.0e9 / static_cast<double>(value.iterations);
    const auto ops = static_cast<double>(value.iterations) / value.seconds;
    const auto mib = static_cast<double>(value.bytes) * value.iterations /
                     (value.seconds * 1024.0 * 1024.0);
    if (csv) {
        std::println("{},{},{},{:.3f},{:.3f},{:.3f},{}", value.name, value.bytes,
                     value.iterations, value.seconds, mean_ns, mib, value.checksum);
    } else {
        std::println("{}: bytes={} iterations={} total_s={:.6f} mean_ns={:.2f} ops/s={:.2f} MiB/s={:.2f} checksum={}",
                     value.name, value.bytes, value.iterations, value.seconds, mean_ns,
                     ops, mib, value.checksum);
    }
    if (value.allocations_profiled) {
        std::println(std::cerr, "{}: allocation_calls={} allocation_bytes={}", value.name,
                     value.allocation_calls, value.allocation_bytes);
    }
}

template <class Parse>
result measure(std::string_view name, std::string_view input, const options& options,
               Parse&& parse) {
    for (std::size_t index = 0; index < options.warmup; ++index) {
        static_cast<void>(parse());
    }
    std::uint64_t checksum = 0;
    detail::AllocationTotals allocations;
    const auto started = std::chrono::steady_clock::now();
    {
        detail::AllocationScope AllocationScope{
            options.allocations ? std::addressof(allocations) : nullptr};
        for (std::size_t index = 0; index < options.iterations; ++index) {
            checksum ^= static_cast<std::uint64_t>(parse());
            checksum = (checksum << 7) | (checksum >> 57);
        }
    }
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    return {std::string(name), input.size(), options.iterations, elapsed, checksum,
            allocations.calls, allocations.bytes, options.allocations};
}

template <class Node>
std::uint64_t node_checksum(const Node& value) {
    return std::visit(
        [](const auto& item) -> std::uint64_t {
            using item_type = std::remove_cvref_t<decltype(item)>;
            if constexpr (requires { item.size(); }) {
                return static_cast<std::uint64_t>(item.size());
            } else if constexpr (std::same_as<item_type, bool>) {
                return item ? 1 : 0;
            } else if constexpr (std::integral<item_type>) {
                return static_cast<std::uint64_t>(item);
            } else {
                return sizeof(item_type);
            }
        },
        value.value);
}

}  // namespace benchmark
