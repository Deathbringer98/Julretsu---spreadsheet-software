#include "julretsu/WindowFrame.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
namespace julretsu {
namespace {
// One main window per process; the window procedure is static, so its state is too.
WNDPROC previous_proc=nullptr;
float caption_height=0;
bool caption_enabled=true;
std::vector<CaptionRect> interactive_rects;

int metric(HWND window,int index) {
    using ForDpi=int(WINAPI*)(int,UINT);
    static const auto for_dpi=reinterpret_cast<ForDpi>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetSystemMetricsForDpi")));
    using DpiOf=UINT(WINAPI*)(HWND);
    static const auto dpi_of=reinterpret_cast<DpiOf>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow")));
    if(for_dpi&&dpi_of) return for_dpi(index,dpi_of(window));
    return GetSystemMetrics(index);
}
int frame_x(HWND w) { return metric(w,SM_CXFRAME)+metric(w,SM_CXPADDEDBORDER); }
int frame_y(HWND w) { return metric(w,SM_CYFRAME)+metric(w,SM_CXPADDEDBORDER); }

LRESULT hit_test(HWND window,LPARAM position) {
    POINT point{GET_X_LPARAM(position),GET_Y_LPARAM(position)}; ScreenToClient(window,&point);
    RECT client{}; GetClientRect(window,&client);
    if(!IsZoomed(window)) {
        const int bx=frame_x(window), by=frame_y(window);
        const bool left=point.x<bx, right=point.x>=client.right-bx, top=point.y<by, bottom=point.y>=client.bottom-by;
        if(top&&left) return HTTOPLEFT; if(top&&right) return HTTOPRIGHT;
        if(bottom&&left) return HTBOTTOMLEFT; if(bottom&&right) return HTBOTTOMRIGHT;
        if(left) return HTLEFT; if(right) return HTRIGHT; if(top) return HTTOP; if(bottom) return HTBOTTOM;
    }
    if(caption_enabled&&float(point.y)<caption_height) {
        for(const auto& r:interactive_rects)
            if(float(point.x)>=r.x0&&float(point.x)<r.x1&&float(point.y)>=r.y0&&float(point.y)<r.y1) return HTCLIENT;
        return HTCAPTION;
    }
    return HTCLIENT;
}
LRESULT CALLBACK frame_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    switch(message) {
    case WM_NCCALCSIZE:
        if(wparam) {
            // The client area covers the whole window. A maximised window extends past the monitor
            // by its frame thickness, so inset it to keep the top bar and edges on screen.
            if(IsZoomed(window)) {
                auto* params=reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
                params->rgrc[0].left+=frame_x(window); params->rgrc[0].right-=frame_x(window);
                params->rgrc[0].top+=frame_y(window); params->rgrc[0].bottom-=frame_y(window);
            }
            return 0;
        }
        break;
    case WM_NCHITTEST: return hit_test(window,lparam);
    default: break;
    }
    return CallWindowProcW(previous_proc,window,message,wparam,lparam);
}
}
bool WindowFrame::install(void* native_window) {
    auto window=static_cast<HWND>(native_window);
    if(!window||previous_proc) return active_;
    previous_proc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&frame_proc)));
    if(!previous_proc) return false;
    // A one-pixel frame extension keeps the DWM drop shadow and Windows 11 rounded corners.
    const MARGINS margins{0,0,1,0}; DwmExtendFrameIntoClientArea(window,&margins);
    SetWindowPos(window,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOACTIVATE);
    active_=true; return true;
}
void WindowFrame::update(float height,bool enabled,const std::vector<CaptionRect>& interactive) {
    caption_height=height; caption_enabled=enabled; interactive_rects.assign(interactive.begin(),interactive.end());
}
}
#else
namespace julretsu {
bool WindowFrame::install(void*) { return false; }
void WindowFrame::update(float,bool,const std::vector<CaptionRect>&) {}
}
#endif
