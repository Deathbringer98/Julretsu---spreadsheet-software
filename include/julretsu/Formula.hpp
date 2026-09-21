#pragma once
#include "Types.hpp"
#include <functional>
#include <span>
#include <vector>

namespace julretsu {
enum class NodeKind { Literal, Reference, Range, Unary, Binary, Call };
struct AstNode {
    NodeKind kind = NodeKind::Literal;
    Value literal;
    CellCoord first{}, last{};
    std::string op;
    std::vector<std::size_t> children;
    std::size_t height = 1;
};
struct ParsedFormula {
    std::vector<AstNode> nodes;
    std::size_t root{};
    std::vector<CellCoord> precedents;
    std::optional<CellError> error;
};
using CellReader = std::function<Value(CellCoord)>;
// Reads may yield a typed error while the core discovers a new dependency.
// The core retries in dependency order before publishing any result.
struct ExtensionContext { CellReader read; };
class FormulaExtension {
public:
    virtual ~FormulaExtension() = default;
    virtual Value sheet_reference(std::string_view,CellCoord) const { return CellError{ErrorCode::Ref,"Worksheet reference is unavailable",{}}; }
    virtual Value evaluate(std::span<const Value> arguments, std::size_t work_budget) const = 0;
    virtual Value evaluate_with_context(std::span<const Value> arguments, std::size_t work_budget,
                                       const ExtensionContext&) const {
        return evaluate(arguments, work_budget);
    }
};
[[nodiscard]] ParsedFormula parse_formula(std::string_view source, const Limits& limits);
[[nodiscard]] Value evaluate_formula(const ParsedFormula& formula, const CellReader& read,
                                     const Limits& limits, const FormulaExtension* extension = nullptr,
                                     const ExtensionContext* context = nullptr);
}
