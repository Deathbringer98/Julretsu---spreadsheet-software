#pragma once
#include "GridCore.hpp"
#include <optional>
#include <string>
#include <vector>
namespace julretsu {
// Sheet check: finds the everyday mistakes behind costly spreadsheet errors.
// Checks only read the sheet; fixes are ordinary batches, so every fix can be undone.
enum class HealthKind { InconsistentFormula, TotalMissesRows, Outlier, NumberAsText, BlankRowInTable };
struct HealthIssue {
    HealthKind kind{};
    CellCoord cell{};
    bool serious = false;      // likely to produce a wrong result, not just a tidy-up
    std::string title, detail;
    std::string fix_label;     // empty when there is no safe automatic fix
    Batch fix;                 // applied by the Fix button
    bool delete_row = false;   // fix removes this blank row instead of applying a batch
    std::uint32_t row{};
};
[[nodiscard]] std::vector<HealthIssue> check_sheet_health(const Sheet&, std::size_t max_issues = 300);
// A note when a numeric cell is wildly out of scale with the rest of its column, for instant feedback after typing.
[[nodiscard]] std::optional<std::string> outlier_note(const Sheet&, CellCoord);
// Shape of a formula relative to its own cell: "=C4*D4" in E4 and "=C5*D5" in E5 share a signature.
[[nodiscard]] std::string formula_signature(const ParsedFormula&, CellCoord at);
}
