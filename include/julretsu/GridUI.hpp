#pragma once
#include "LuaEngine.hpp"
#include "GridViewport.hpp"
#include <array>
#include <string>
namespace julretsu {
class GridUI {
    LuaEngine lua_;
    Sheet sheet_;
    GridViewport viewport_;
    CellCoord active_{0,0}, anchor_{0,0};
    std::vector<RowInterval> row_selection_;
    Batch queued_;
    enum class Action { None, Undo, Redo, Clear, Bold, Tint, ResetStyle, Macro, New, Demo, DecimalsLess, DecimalsMore };
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
    void remember_target(int index);
    void refresh_editor();
    void queue_edit();
    void clear_selection();
    void style_selection(Action);
    void select(CellCoord,bool extend=false);
    void draw_grid(float width,float height);
    void draw_script_panel(float height);
public:
    unsigned brand_icon{};
    struct Point { float x{}, y{}; };
    enum Target { MacroButton, FormulaField, BoldButton, ThemeButton };
    std::array<Point,4> targets{};
    GridUI();
    bool dark() const noexcept { return dark_; }
    void set_dark(bool dark, bool remember=true);
    void prepare(); // Mutations/recalculation happen only here, before NewFrame.
    void draw();
    void set_scale(float scale);
    void load_demo();
    void load_performance_fixture();
    void jump(CellCoord c) { select(c); viewport_.reveal(c); }
    [[nodiscard]] bool modified() const noexcept { return modified_; }
    [[nodiscard]] const Sheet& sheet() const noexcept { return sheet_; }
    [[nodiscard]] const GridViewport& viewport() const noexcept { return viewport_; }
    [[nodiscard]] CellCoord active() const noexcept { return active_; }
    [[nodiscard]] bool scripts_open() const noexcept { return scripts_open_; }
    void show_scripts(bool show) noexcept { scripts_open_=show; }
    void smoke_queue_edit(CellCoord c,const char* text);
};
}
