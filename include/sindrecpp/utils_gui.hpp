#pragma once

#if !defined(SINDRECPP_WITH_UTILS_GUI)
#error "Enable SINDRECPP_WITH_UTILS_GUI and link SindreCpp::Utils_gui before including this header."
#endif

#include <imgui.h>

namespace sindrecpp::utils_gui {

namespace native = ImGui;

class Context {
public:
    explicit Context(ImFontAtlas* shared_font_atlas = nullptr)
        : context_(ImGui::CreateContext(shared_font_atlas)) {}
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&& other) noexcept : context_(other.context_) { other.context_ = nullptr; }
    Context& operator=(Context&& other) noexcept {
        if (this != &other) {
            if (context_) ImGui::DestroyContext(context_);
            context_ = other.context_;
            other.context_ = nullptr;
        }
        return *this;
    }
    ~Context() { if (context_) ImGui::DestroyContext(context_); }

    ImGuiContext* get() const noexcept { return context_; }
    void make_current() const { ImGui::SetCurrentContext(context_); }

private:
    ImGuiContext* context_;
};

} // namespace sindrecpp::utils_gui
