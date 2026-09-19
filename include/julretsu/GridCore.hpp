#pragma once
#include "Formula.hpp"
#include <map>
#include <set>
#include <span>
#include <vector>

namespace julretsu {
struct Style {
    bool bold = false;
    std::uint32_t foreground = 0x202B39FF;
    std::uint32_t background = 0xFFFFFFFF;
    std::uint8_t decimals = 2;
    bool operator==(const Style&) const = default;
};
struct Cell {
    Input input;
    std::optional<ParsedFormula> formula;
    Value cached;
    std::uint64_t revision{};
    bool dirty{};
    std::set<CellCoord> runtime_precedents;
};
using Row = std::map<std::uint32_t, Cell>;
struct RowInterval {
    std::uint32_t first{}, last{}; // inclusive
    bool operator==(const RowInterval&) const = default;
};
class RowSelection {
    std::vector<RowInterval> intervals_;
public:
    explicit RowSelection(std::vector<RowInterval> intervals);
    [[nodiscard]] const std::vector<RowInterval>& intervals() const noexcept { return intervals_; }
    [[nodiscard]] bool contains(std::uint32_t row) const noexcept;
};
struct Edit { CellCoord coord; Input input; };
struct StyleEdit { std::uint32_t row; std::optional<Style> style; };
struct Batch { std::vector<Edit> cells; std::vector<StyleEdit> rows; };
struct CommitResult {
    bool accepted{};
    std::uint64_t revision{};
    std::vector<CellCoord> changed; // edited cells plus transitively affected cells
    std::vector<std::uint32_t> styled_rows;
    std::size_t formulas_evaluated{};
    std::optional<CellError> error;
};
class Sheet {
    using Edges = std::map<CellCoord,std::set<CellCoord>>;
    struct State {
        std::map<std::uint32_t,Row> rows;
        std::map<std::uint32_t,Style> styles;
        Edges precedents, dependents;
    };
    State state_;
    std::vector<State> undo_, redo_;
    Limits limits_;
    std::uint64_t revision_{};
    const FormulaExtension* extension_{}; // borrowed; outlives Sheet
    static Cell* mutable_cell(State&,CellCoord);
    static const Cell* find_cell(const State&,CellCoord);
    static Value read_state(const State&,CellCoord);
    std::size_t recalculate(State&,const std::set<CellCoord>&,std::uint64_t,bool&);
public:
    explicit Sheet(Limits limits = {}, const FormulaExtension* extension = nullptr);
    [[nodiscard]] Value read(CellCoord coord) const;
    [[nodiscard]] const Cell* cell(CellCoord coord) const;
    [[nodiscard]] const Row& row_view(std::uint32_t row) const;
    [[nodiscard]] const std::map<std::uint32_t,Row>& populated_rows() const noexcept { return state_.rows; }
    [[nodiscard]] const std::map<std::uint32_t,Style>& row_styles() const noexcept { return state_.styles; }
    [[nodiscard]] Style row_style(std::uint32_t row) const;
    [[nodiscard]] std::size_t populated_cells() const noexcept;
    [[nodiscard]] std::size_t edge_count() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] CommitResult apply(const Batch& batch);
    [[nodiscard]] CommitResult set(CellCoord coord, Input input);
    [[nodiscard]] CommitResult clear(CellCoord coord);
    [[nodiscard]] CommitResult clear_rows(const RowSelection& selection);
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    void clear_history() noexcept { undo_.clear(); redo_.clear(); }
};
}
