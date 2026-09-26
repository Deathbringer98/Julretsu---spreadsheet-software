#pragma once
#include "Formula.hpp"
#include <map>
#include <functional>
#include <set>
#include <span>
#include <vector>

namespace julretsu {
struct Style {
    bool bold = false;
    std::uint32_t foreground = 0x202B39FF;
    std::uint32_t background = 0xFFFFFFFF;
    std::uint8_t decimals = 2;
    std::uint8_t alignment=0, number_format=0, font_size=16, font_family=0;
    bool border=false;
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
struct CellStyleEdit { CellCoord coord; std::optional<Style> style; };
enum class ValidationKind { Any, Number, WholeNumber, Date, List };
struct ValidationRule {
    CellCoord first{}, last{};
    ValidationKind kind=ValidationKind::Any;
    bool required=false, unique=false, locked=false, warning=false;
    double minimum=0, maximum=100;
    std::string date_min="1900-01-01", date_max="2100-12-31";
    std::vector<std::string> choices;
    bool contains(CellCoord c) const { return c.row>=first.row&&c.row<=last.row&&c.column>=first.column&&c.column<=last.column; }
    bool operator==(const ValidationRule&) const = default;
};
struct ValidationIssue { CellCoord cell; std::string reason; bool warning=false; };
bool valid_rule(const ValidationRule&);
std::vector<ValidationIssue> check_rules(const std::vector<ValidationRule>&, const std::function<Value(CellCoord)>&);
struct StructuredTable {
    std::string name;
    CellCoord first{},last{}; // header through last data row; optional totals follow
    bool totals=false;
    bool operator==(const StructuredTable&) const = default;
};
struct Batch { std::vector<Edit> cells; std::vector<StyleEdit> rows; std::vector<CellStyleEdit> formats{}; std::optional<std::vector<ValidationRule>> rules{}; std::optional<std::vector<StructuredTable>> tables{}; bool table_scaffold=false; };
// Per-cell change history. Every committed edit, undo and redo is recorded with its
// time and a short label; inputs are kept as display text so history survives saving.
struct CellChange { CellCoord coord; std::string before, after; std::uint8_t before_kind{}, after_kind{}; };
struct ChangeRecord { std::int64_t time{}; std::string label; std::vector<CellChange> cells; std::uint32_t total_cells{}, formats{}; };
inline constexpr std::size_t journal_records = 1000, journal_cells_per_record = 200, journal_text = 160;
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
        std::map<CellCoord,Style> formats;
        Edges precedents, dependents;
        std::vector<ValidationRule> rules;
        std::vector<StructuredTable> tables;
    };
    State state_;
    std::vector<State> undo_, redo_;
    Limits limits_;
    std::uint64_t revision_{};
    const FormulaExtension* extension_{}; // borrowed; outlives Sheet
    std::vector<ChangeRecord> journal_;
    std::string next_label_;
    std::uint64_t journal_serial_{};
    static std::uint64_t next_instance();
    std::uint64_t instance_=next_instance(); // copies share it; a replaced sheet gets a new one
    bool suppress_journal_{};
    void record_change(const State& before,std::string label);
    static Cell* mutable_cell(State&,CellCoord);
    static const Cell* find_cell(const State&,CellCoord);
    static Value read_state(const State&,CellCoord);
    std::size_t recalculate(State&,const std::set<CellCoord>&,std::uint64_t,bool&);
public:
    explicit Sheet(Limits limits = {}, const FormulaExtension* extension = nullptr);
    [[nodiscard]] Value read(CellCoord coord) const;
    const std::vector<StructuredTable>& tables() const { return state_.tables; }
    const std::vector<ValidationRule>& validation_rules() const { return state_.rules; }
    std::vector<ValidationIssue> validation_issues() const { return check_rules(state_.rules,[this](CellCoord c){return read(c);}); }
    [[nodiscard]] const Cell* cell(CellCoord coord) const;
    [[nodiscard]] const Row& row_view(std::uint32_t row) const;
    [[nodiscard]] const std::map<std::uint32_t,Row>& populated_rows() const noexcept { return state_.rows; }
    [[nodiscard]] const std::map<std::uint32_t,Style>& row_styles() const noexcept { return state_.styles; }
    [[nodiscard]] Style cell_style(CellCoord c) const { auto it=state_.formats.find(c); return it==state_.formats.end()?row_style(c.row):it->second; }
    [[nodiscard]] const std::map<CellCoord,Style>& cell_styles() const noexcept { return state_.formats; }
    [[nodiscard]] Style row_style(std::uint32_t row) const;
    [[nodiscard]] std::size_t populated_cells() const noexcept;
    [[nodiscard]] std::size_t edge_count() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] CommitResult apply(const Batch& batch);
    [[nodiscard]] CommitResult refresh_values(const Batch& batch) {
        auto undo=std::move(undo_),redo=std::move(redo_);auto result=apply(batch);undo_=std::move(undo);redo_=std::move(redo);return result;
    }
    [[nodiscard]] CommitResult set(CellCoord coord, Input input);
    [[nodiscard]] CommitResult clear(CellCoord coord);
    [[nodiscard]] CommitResult clear_rows(const RowSelection& selection);
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    void clear_history() noexcept { undo_.clear(); redo_.clear(); }
    // Names the next recorded change ("Paste", "Sort"...); consumed when a change is recorded.
    void label_next_change(std::string label) { next_label_=std::move(label); }
    [[nodiscard]] const std::vector<ChangeRecord>& journal() const noexcept { return journal_; }
    [[nodiscard]] std::uint64_t journal_serial() const noexcept { return journal_serial_; }
    [[nodiscard]] std::uint64_t instance() const noexcept { return instance_; }
    void set_journal(std::vector<ChangeRecord> journal);
    void clear_journal() noexcept { journal_.clear(); }
    // Undo the most recent change without recording it, and drop that change from the history.
    bool revert_last_change();
};
}
