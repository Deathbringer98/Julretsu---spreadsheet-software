#pragma once
#include "GridCore.hpp"
namespace julretsu {
struct LuaLimits {
    std::size_t memory_bytes = 4 * 1024 * 1024;
    std::size_t instructions = 200'000;
    std::size_t source_bytes = 16'384;
    std::size_t output_bytes = 16'384;
    std::size_t callbacks = 2048;
    std::size_t writes = 1024;
    std::size_t rows = 256;
};
struct MacroResult {
    CommitResult commit;
    std::optional<CellError> error;
    std::size_t writes{};
};
class LuaEngine final : public FormulaExtension {
    LuaLimits limits_;
public:
    std::function<Value(std::string_view,CellCoord)> sheet_reader;
    Value sheet_reference(std::string_view name,CellCoord c) const override {return sheet_reader?sheet_reader(name,c):Value{CellError{ErrorCode::Ref,"Unknown worksheet",{}}};}
    explicit LuaEngine(LuaLimits limits = {});
    Value evaluate(std::span<const Value>,std::size_t work_budget) const override;
    Value evaluate_with_context(std::span<const Value>,std::size_t work_budget,const ExtensionContext&) const override;
    // Macro reads see the original committed sheet; queued writes are applied together.
    [[nodiscard]] MacroResult run_macro(std::string_view source,Sheet&) const;
};
}
