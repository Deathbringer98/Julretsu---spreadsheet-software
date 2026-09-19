#pragma once
#include <filesystem>
struct GLFWwindow;
namespace julretsu {
class Branding {
    unsigned int texture_{}, icon_texture_{};
    int width_{}, height_{};
    bool icon_loaded_{};
public:
    explicit Branding(GLFWwindow*);
    ~Branding();
    Branding(const Branding&)=delete;
    Branding& operator=(const Branding&)=delete;
    void draw(const char* status) const;
    [[nodiscard]] bool loaded() const noexcept { return texture_!=0&&icon_loaded_; }
    unsigned icon_texture() const noexcept { return icon_texture_; }
    static std::filesystem::path asset_directory();
};
}
