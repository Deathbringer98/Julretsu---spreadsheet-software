#pragma once
#include "GridCore.hpp"
#include <istream>
#include <ostream>
#include <stop_token>
namespace julretsu {
// Bounded stream adapters; implementations report errors without throwing across the UI.
enum class ImportInterpretation { TextOnly, Values, NativeFormulas };
struct StreamOptions {
    bool safe_text_export = true;
    std::size_t buffer_bytes = 64 * 1024;
    std::size_t batch_cells = 4096;
    ImportInterpretation interpretation = ImportInterpretation::TextOnly;
    std::stop_token cancellation;
    std::function<void(std::size_t bytes, std::size_t cells)> progress;
};
struct StreamResult { std::size_t bytes{}, cells{}; bool cancelled{}; std::optional<CellError> error; };
class SheetStreamAdapter {
public:
    virtual ~SheetStreamAdapter() = default;
    virtual StreamResult import_sheet(std::istream&, Sheet&, const StreamOptions&) = 0;
    virtual StreamResult export_sheet(std::ostream&, const Sheet&, const StreamOptions&) = 0;
};
}
