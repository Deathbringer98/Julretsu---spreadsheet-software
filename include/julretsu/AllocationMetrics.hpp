#pragma once
#include <atomic>
#include <cstddef>
namespace julretsu::metrics {
inline std::atomic<std::size_t> cpp_allocations{}, cpp_bytes{}, imgui_allocations{}, imgui_bytes{};
}
