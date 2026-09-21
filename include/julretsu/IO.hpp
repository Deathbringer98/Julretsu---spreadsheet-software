#pragma once
#include "GridCore.hpp"
#include <istream>
#include <ostream>
#include <atomic>
#include <functional>
#include <memory>
namespace julretsu {
// Portable cancellation (std::stop_token is missing from some standard libraries, such as Apple's).
class CancelToken {
    std::shared_ptr<const std::atomic<bool>> flag_;
public:
    CancelToken()=default;
    explicit CancelToken(std::shared_ptr<const std::atomic<bool>> flag):flag_(std::move(flag)) {}
    [[nodiscard]] bool stop_requested() const noexcept { return flag_&&flag_->load(); }
};
class CancelSource {
    std::shared_ptr<std::atomic<bool>> flag_=std::make_shared<std::atomic<bool>>(false);
public:
    void request_stop() noexcept { flag_->store(true); }
    [[nodiscard]] CancelToken get_token() const { return CancelToken(flag_); }
};
// Bounded stream adapters; implementations report errors without throwing across the UI.
enum class ImportInterpretation { TextOnly, Values, NativeFormulas };
struct StreamOptions {
    bool safe_text_export = true;
    std::size_t buffer_bytes = 64 * 1024;
    std::size_t batch_cells = 4096;
    ImportInterpretation interpretation = ImportInterpretation::TextOnly;
    CancelToken cancellation;
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
