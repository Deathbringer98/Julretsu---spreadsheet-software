#pragma once
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace julretsu {
inline constexpr std::uint32_t max_rows = 1'000'000;
inline constexpr std::uint32_t max_columns = 16'384;
struct CellCoord {
    std::uint32_t row{}, column{};
    auto operator<=>(const CellCoord&) const = default;
    [[nodiscard]] bool valid() const noexcept { return row < max_rows && column < max_columns; }
};
struct CoordHash {
    std::size_t operator()(CellCoord c) const noexcept {
        std::uint64_t x = (std::uint64_t(c.row) << 32) | c.column;
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return static_cast<std::size_t>(x ^ (x >> 31));
    }
};
enum class ErrorCode { Ref, Cycle, CycleDependency, Name, Value, DivZero, Num, Parse, Limit, Unsupported };
struct CellError {
    ErrorCode code{};
    std::string context;
    std::optional<CellCoord> origin;
    bool operator==(const CellError&) const = default;
};
using Value = std::variant<std::monostate, double, bool, std::string, CellError>;
struct FormulaInput { std::string source; bool operator==(const FormulaInput&) const = default; };
using Input = std::variant<std::monostate, double, bool, std::string, FormulaInput>;
using CoordResult = std::variant<CellCoord, CellError>;
[[nodiscard]] CoordResult from_a1(std::string_view text);
[[nodiscard]] std::variant<std::string, CellError> to_a1(CellCoord coord);
[[nodiscard]] std::string_view error_display(ErrorCode code) noexcept;
[[nodiscard]] std::string display(const Value& value);
struct Limits {
    std::size_t formula_bytes = 8192;
    std::size_t parse_depth = 64;
    std::size_t ast_nodes = 2048;
    std::size_t range_cells = 10'000;
    std::size_t dependencies = 20'000;
    std::size_t evaluation_work = 100'000;
    std::size_t batch_edits = 100'000;
    std::size_t populated_cells = 100'000;
    std::size_t graph_edges = 1'000'000;
    std::size_t text_bytes = 1'048'576;
    std::size_t sheet_input_bytes = 32 * 1024 * 1024;
    std::size_t undo_depth = 8;
};
}
