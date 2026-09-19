#pragma once
#include "LuaEngine.hpp"
#include "GridViewport.hpp"
#include "Ai.hpp"
#include <memory>
#include <array>
#include <string>
#include <filesystem>
namespace julretsu {
class GridUI {
    LuaEngine lua_;
    Sheet sheet_;
    GridViewport viewport_;
    CellCoord active_{0,0}, anchor_{0,0};
    std::vector<RowInterval> row_selection_;
    Batch queued_;
    enum class Action { None, Undo, Redo, Clear, Bold, Tint, ResetStyle, Macro, New, Demo, DecimalsLess, DecimalsMore, Save, SaveAs, Open, ImportCsv, ExportCsv, ImportXlsx, ExportXlsx, AiSend, AiApply };
    Action action_=Action::None;
    std::array<char,16385> editor_{}, script_{};
    std::array<char,32> address_{};
    std::string message_="Ready. Double-click a cell to edit.";
    bool selection_changed_=true, editing_=false, focus_editor_=false;
    bool scripts_open_=false, modified_=false, oversized_=false;
    bool grid_focused_=true;
    std::size_t populated_{};
    std::uint64_t counted_revision_=~std::uint64_t{};
    float scale_=1.0f;
    bool dark_=false, remember_theme_=true;
    int ribbon_tab_=0;
    float zoom_=1.0f;
    std::array<char,32> search_{};
    std::filesystem::path file_path_;
    std::string workbook_name_="Untitled workbook", file_label_, file_error_;
    bool editor_dirty_=false;
    bool xlsx_formulas_=false;
    std::unique_ptr<AiSession> ai_session_=std::make_unique<AiSession>();
    AiSettings ai_settings_, ai_request_settings_;
    std::array<char,256> ai_model_{};
    std::array<char,1024> ai_endpoint_{};
    std::array<char,512> ai_key_{};
    std::array<char,2048> ai_prompt_{};
    std::optional<AiProposal> ai_proposal_;
    std::uint64_t ai_revision_{};
    std::string ai_status_;
    bool ai_open_=false, ai_key_saved_=false, ai_connection_open_=false, ai_connection_collapse_=false;
    void ai_use_settings(const AiSettings&);
    void ai_send();
    void ai_apply();
    void ai_poll();
    void draw_ai_panel(float height);
    void xlsx_file(bool exporting);
    void csv_file(bool exporting);
    void open();
    void remember_target(int index);
    void refresh_editor();
    void queue_edit();
    void clear_selection();
    void style_selection(Action);
    void select(CellCoord,bool extend=false);
    void draw_grid(float width,float height);
    void draw_script_panel(float height);
public:
    void* native_window{};
    bool save(bool save_as=false);
    bool save_to(const std::filesystem::path&);
    bool open_from(const std::filesystem::path&);
    bool confirm_close();
    unsigned brand_icon{};
    struct Point { float x{}, y{}; };
    enum Target { MacroButton, FormulaField, BoldButton, ThemeButton, FileButton };
    std::array<Point,5> targets{};
    GridUI();
    bool dark() const noexcept { return dark_; }
    void set_dark(bool dark, bool remember=true);
    void prepare(); // Mutations/recalculation happen only here, before NewFrame.
    void draw();
    void set_scale(float scale);
    void load_demo();
    void load_performance_fixture();
    void jump(CellCoord c) { select(c); viewport_.reveal(c); }
    [[nodiscard]] bool modified() const noexcept { return modified_||editor_dirty_||!queued_.cells.empty(); }
    [[nodiscard]] const Sheet& sheet() const noexcept { return sheet_; }
    [[nodiscard]] const GridViewport& viewport() const noexcept { return viewport_; }
    [[nodiscard]] CellCoord active() const noexcept { return active_; }
    [[nodiscard]] bool scripts_open() const noexcept { return scripts_open_; }
    void show_scripts(bool show) noexcept { scripts_open_=show; if(show) ai_open_=false; }
    [[nodiscard]] bool ai_open() const noexcept { return ai_open_; }
    void show_ai(bool show) noexcept { ai_open_=show; if(show) scripts_open_=false; }
    [[nodiscard]] const std::optional<AiProposal>& ai_proposal() const noexcept { return ai_proposal_; }
    void smoke_ai_proposal(std::string_view reply);
    void smoke_ai_apply() { action_=Action::AiApply; }
    void smoke_queue_edit(CellCoord c,const char* text);
};
}
