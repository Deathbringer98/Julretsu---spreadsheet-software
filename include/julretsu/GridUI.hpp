#pragma once
#include "LuaEngine.hpp"
#include "GridViewport.hpp"
#include "Ai.hpp"
#include "WindowFrame.hpp"
#include "SheetHealth.hpp"
#include <optional>
#include <set>
#include <functional>
#include <memory>
#include <chrono>
#include <array>
#include <string>
#include <filesystem>
namespace julretsu {
class GridUI {
    LuaEngine lua_;
    Sheet sheet_;
    struct StoredSheet { std::string name; Sheet sheet; std::string script; };
    std::vector<StoredSheet> sheets_;
    std::size_t current_sheet_=0;
    int switch_sheet_=-1;
    std::array<char,65> sheet_name_{};
    bool sheets_open_=false, recovery_open_=false;
    bool autosave_=true;
    std::string recovery_error_;
    std::chrono::steady_clock::time_point autosave_time_=std::chrono::steady_clock::now();
    Value read_sheet_cell(std::size_t,CellCoord,std::set<std::pair<std::size_t,CellCoord>>&,unsigned&);
    void recalculate_links();
    void change_sheet(std::size_t);
    void sheet_action(int);
    bool save_document_to(const std::filesystem::path&);
    void draw_sheets();
    void recovery_tick();
    bool recovery_disabled_=false;
    GridViewport viewport_;
    CellCoord active_{0,0}, anchor_{0,0};
    std::vector<RowInterval> row_selection_;
    Batch queued_;
    enum class Action { None, Undo, Redo, Clear, Bold, Tint, ResetStyle, Macro, New, Demo, DecimalsLess, DecimalsMore, Save, SaveAs, Open, ImportCsv, ExportCsv, ImportXlsx, ExportXlsx, AiSend, AiApply, FillDown, FillRight, QuickSum, QuickAverage, Copy, Cut, Paste, PasteValues, CellFormat, InsertRow, DeleteRow, InsertColumn, DeleteColumn, SortAscending, SortDescending, AddSheet, DuplicateSheet, DeleteSheet, RenameSheet, MoveSheetLeft, MoveSheetRight, DragFill, PrintReport };
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
    Style format_style_{};
    bool format_open_=false, clipboard_cut_=false;
    std::string clipboard_text_;
    std::vector<Edit> clipboard_cells_;
    std::vector<CellStyleEdit> clipboard_styles_;
    std::vector<Value> clipboard_values_;
    CellCoord clipboard_origin_{};
    std::uint64_t clipboard_revision_{};
    unsigned clipboard_rows_{},clipboard_columns_{};
    void clipboard_command(Action);
    void apply_cell_format();
    void draw_format_dialog();
    bool report_open_=false,chart_line_=false,print_landscape_=false,print_header_=true;
    std::array<char,128> report_title_{};
    CellCoord report_first_{},report_last_{};
    std::vector<float> chart_values_;
    std::vector<std::vector<std::string>> report_rows_;
    void prepare_report();
    void draw_report();
    void print_report(const std::filesystem::path& output={});
    bool filter_open_=false;
    std::array<char,256> filter_text_{};
    unsigned filter_column_=0;
    std::uint64_t filter_revision_=~std::uint64_t{};
    void rebuild_filter();
    bool fill_drag_=false;
    CellCoord fill_first_{},fill_last_{},fill_target_{};
    void apply_drag_fill();
    bool gridlines_=true, show_formulas_=false, find_open_=false;
    bool text_overflow_=false; // Excel-style spill of long text into empty cells to the right
    std::array<char,256> find_text_{};
    void selection_command(Action);
    void find_next();
    void select_used();
    void draw_tools();
    // Safety net: review of risky changes, sheet check and per-cell history.
    struct PendingReview { std::string label; std::vector<CellChange> cells; std::uint32_t total{}, formulas_replaced{}; std::vector<std::string> warnings; };
    std::optional<PendingReview> review_;
    bool review_enabled_=true; int review_threshold_=50;
    std::string review_note_; // why a sort or delete is risky, set by the command itself
    std::vector<HealthIssue> health_;
    std::uint64_t health_revision_=~std::uint64_t{}, health_instance_{};
    std::chrono::steady_clock::time_point health_time_{};
    bool health_force_=false; // opening the panel always shows a fresh scan
    bool health_open_=false, history_open_=false, changelog_open_=false, clear_history_confirm_=false;
    CellCoord history_cell_{};
    std::set<std::pair<int,CellCoord>> health_ignored_;
    void evaluate_latest_change();
    void refresh_health(bool force=false);
    [[nodiscard]] std::size_t visible_health_count() const;
    void draw_health_panel(float height);
    void draw_safety_dialogs();
    // Title bar: rows that drag the window, plus the widgets inside them that stay clickable.
    float caption_height_{};
    bool caption_enabled_=true;
    std::vector<CaptionRect> caption_items_;
    void caption_item();
    void draw_window_buttons();
    // Excel-style workbook window state and ribbon.
    bool ribbon_collapsed_=false, workbook_minimized_=false, workbook_floating_=false, focus_jump_=false;
    void draw_home_ribbon(bool& guide_popup,bool& settings_popup);
    void draw_minimized_workbook(float x,float y,float width,float height);
    void close_workbook();
    // Ribbon commands run in prepare(), never mid-frame.
    std::function<void()> pending_;
    void format_cells(std::function<void(Style&)> change,std::string done);
    void clear_formats(bool contents);
    std::array<char,160> selection_stats_{};
    void update_selection_stats();
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
    void show_scripts(bool show) noexcept { scripts_open_=show; if(show) { ai_open_=false; health_open_=false; } }
    [[nodiscard]] bool ai_open() const noexcept { return ai_open_; }
    void show_ai(bool show) noexcept { ai_open_=show; if(show) { scripts_open_=false; health_open_=false; } }
    void show_health(bool show) noexcept { health_open_=show; if(show) { scripts_open_=false; ai_open_=false; health_force_=true; } }
    bool smoke_safety_net(const std::filesystem::path&);
    void smoke_open_review();
    void smoke_close_review();
    void smoke_health_example(bool show);
    [[nodiscard]] const std::optional<AiProposal>& ai_proposal() const noexcept { return ai_proposal_; }
    void smoke_ai_proposal(std::string_view reply);
    bool smoke_selection_tools();
    bool smoke_workbook_features(const std::filesystem::path&);
    void smoke_show_format(bool show) {format_style_=sheet_.cell_style(active_);format_open_=show;}
    void smoke_show_report(bool show,CellCoord first={},CellCoord last={}) {if(show){select(first);select(last,true);prepare_report();}else report_open_=false;}
    void smoke_ai_apply() { action_=Action::AiApply; }
    // Recovery copies protect against crashes. Tests and benchmarks never touch the user's copy.
    void disable_recovery() noexcept { recovery_disabled_=true; recovery_open_=false; autosave_=false; }
    void discard_recovery();
    void smoke_workbook_window(int state) { workbook_floating_=state==1; workbook_minimized_=state==2; }
    bool custom_frame=false; // set when WindowFrame replaced the OS title bar
    [[nodiscard]] float caption_height() const noexcept { return caption_height_; }
    [[nodiscard]] bool caption_enabled() const noexcept { return caption_enabled_; }
    [[nodiscard]] const std::vector<CaptionRect>& caption_items() const noexcept { return caption_items_; }
    void smoke_queue_edit(CellCoord c,const char* text);
};
}
