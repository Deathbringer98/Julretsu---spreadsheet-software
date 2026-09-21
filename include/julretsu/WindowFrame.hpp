#pragma once
#include <vector>
namespace julretsu {
struct CaptionRect { float x0{}, y0{}, x1{}, y1{}; };
// Replaces the OS title bar with Julretsu's own top bar while keeping native
// dragging, Aero snap, double-click maximise, edge resizing and the window shadow.
// Windows only; elsewhere install() returns false and the system title bar stays.
class WindowFrame {
public:
    bool install(void* native_window);
    [[nodiscard]] bool active() const noexcept { return active_; }
    // Client-pixel height of the draggable top bar, and the widgets inside it that must stay clickable.
    // While a popup is open the whole bar is treated as ordinary client area.
    void update(float caption_height, bool enabled, const std::vector<CaptionRect>& interactive);
private:
    bool active_ = false;
};
}
