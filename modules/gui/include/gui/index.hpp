#pragma once

#if !defined(SINDRECPP_WITH_GUI)
#error "Enable SINDRECPP_WITH_GUI and link SindreCpp::Gui before including this header."
#endif

#include <imgui.h>
#include <general/index.hpp>
#include <general/core/path.hpp>
#include <general/core/scopeguard.hpp>
#include <general/core/system.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(SINDRECPP_GUI_GLFW_OPENGL3)
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#if defined(SINDRECPP_GUI_STB_IMAGE) || defined(SINDRECPP_GUI_GLFW_OPENGL3)
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_STATIC
#endif

namespace sindrecpp::utils_gui {

namespace native = ImGui;

class Context {
public:
    explicit Context(ImFontAtlas *shared_font_atlas = nullptr)
        : context_(ImGui::CreateContext(shared_font_atlas)) {}
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&other) noexcept : context_(other.context_) { other.context_ = nullptr; }
    Context &operator=(Context &&other) noexcept {
        if (this != &other) {
            if (context_) ImGui::DestroyContext(context_);
            context_ = other.context_;
            other.context_ = nullptr;
        }
        return *this;
    }
    ~Context() { if (context_) ImGui::DestroyContext(context_); }
    ImGuiContext *get() const noexcept { return context_; }
    void make_current() const noexcept { ImGui::SetCurrentContext(context_); }

private:
    ImGuiContext *context_ = nullptr;
};

struct FontConfig {
    std::filesystem::path path;
    float size = 18.0f;
    bool require_cjk = false;
    std::vector<std::filesystem::path> search_directories;
};

struct FontInfo {
    ImFont *font = nullptr;
    std::filesystem::path path;
    bool fallback = false;
};

inline std::vector<std::filesystem::path> default_font_directories() {
    std::vector<std::filesystem::path> result;
#if defined(_WIN32)
    if (auto windows = ::sindrecpp::general::system::environment("WINDIR"))
        result.emplace_back(std::filesystem::path(windows.value()) / "Fonts");
    if (auto local = ::sindrecpp::general::system::environment("LOCALAPPDATA"))
        result.emplace_back(std::filesystem::path(local.value()) / "Microsoft/Windows/Fonts");
#elif defined(__APPLE__)
    result.emplace_back("/System/Library/Fonts");
    result.emplace_back("/Library/Fonts");
    if (auto home = ::sindrecpp::general::system::environment("HOME"))
        result.emplace_back(std::filesystem::path(home.value()) / "Library/Fonts");
#else
    result.emplace_back("/usr/share/fonts");
    result.emplace_back("/usr/local/share/fonts");
    if (auto home = ::sindrecpp::general::system::environment("HOME")) {
        result.emplace_back(std::filesystem::path(home.value()) / ".fonts");
        result.emplace_back(std::filesystem::path(home.value()) / ".local/share/fonts");
    }
#endif
    return result;
}

inline ::sindrecpp::general::Result<FontInfo> load_font(const FontConfig &config = {}) noexcept {
    try {
        std::vector<std::filesystem::path> directories = config.search_directories;
        if (directories.empty()) directories = default_font_directories();
        const std::vector<std::string> candidates{
            "NotoSansCJK-Regular.ttc", "NotoSansCJKsc-Regular.otf", "NotoSansSC-Regular.otf",
            "Microsoft YaHei.ttf", "msyh.ttc", "simsun.ttc", "SimSun.ttf", "PingFang.ttc",
            "WenQuanYi Zen Hei.ttf"};
        std::filesystem::path selected = config.path;
        if (selected.empty()) {
            for (const auto &directory : directories) {
                for (const auto &candidate : candidates) {
                    const auto path = directory / candidate;
                    if (std::filesystem::is_regular_file(path)) { selected = path; break; }
                }
                if (!selected.empty()) break;
            }
        }
        if (!selected.empty()) {
            if (!std::filesystem::is_regular_file(selected))
                return ::sindrecpp::general::Result<FontInfo>::failure(
                    std::make_error_code(std::errc::no_such_file_or_directory), "Font file does not exist",
                    "utils_gui.font");
            ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
                ::sindrecpp::general::path::to_utf8(selected).c_str(), config.size, nullptr,
                ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
            if (!font) return ::sindrecpp::general::Result<FontInfo>::failure(
                std::make_error_code(std::errc::invalid_argument), "Cannot load font file", "utils_gui.font");
            return ::sindrecpp::general::Result<FontInfo>::success({font, selected, false});
        }
        if (config.require_cjk) return ::sindrecpp::general::Result<FontInfo>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "No CJK font was found",
            "utils_gui.font");
        return ::sindrecpp::general::Result<FontInfo>::success(
            {ImGui::GetIO().Fonts->AddFontDefault(), {}, true});
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<FontInfo>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "utils_gui.font");
    } catch (...) {
        return ::sindrecpp::general::Result<FontInfo>::failure(
            std::make_error_code(std::errc::io_error), "Unknown font loading failure", "utils_gui.font");
    }
}

inline void apply_dark_theme(float scale = 1.0f) {
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 7.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;
    style.WindowPadding = ImVec2(12.0f, 10.0f);
    style.FramePadding = ImVec2(9.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
    style.ScaleAllSizes(std::max(0.5f, scale));
}

struct ImageAsset {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<std::uint8_t> pixels;
    bool empty() const noexcept { return pixels.empty() || width <= 0 || height <= 0; }
    const std::uint8_t *data() const noexcept { return pixels.data(); }

    static ::sindrecpp::general::Result<ImageAsset> load_memory(
        const std::vector<std::uint8_t> &encoded, int requested_channels = 4) noexcept {
#if defined(SINDRECPP_GUI_STB_IMAGE) || defined(SINDRECPP_GUI_GLFW_OPENGL3)
        if (encoded.empty() || requested_channels < 1 || requested_channels > 4)
            return ::sindrecpp::general::Result<ImageAsset>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid image input", "utils_gui.image");
        int width = 0, height = 0, channels = 0;
        stbi_uc *decoded = stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()),
                                                 &width, &height, &channels, requested_channels);
        if (!decoded) return ::sindrecpp::general::Result<ImageAsset>::failure(
            std::make_error_code(std::errc::invalid_argument),
            stbi_failure_reason() ? stbi_failure_reason() : "Invalid image data", "utils_gui.image");
        ImageAsset result;
        result.width = width; result.height = height; result.channels = requested_channels;
        result.pixels.assign(decoded, decoded + static_cast<std::size_t>(width) * height * requested_channels);
        stbi_image_free(decoded);
        return ::sindrecpp::general::Result<ImageAsset>::success(std::move(result));
#else
        (void)encoded; (void)requested_channels;
        return ::sindrecpp::general::Result<ImageAsset>::failure(
            std::make_error_code(std::errc::function_not_supported), "stb_image is not enabled", "utils_gui.image");
#endif
    }

    static ::sindrecpp::general::Result<ImageAsset> load(
        const std::filesystem::path &path, int requested_channels = 4) noexcept {
        try {
            std::ifstream input(path, std::ios::binary);
            if (!input) return ::sindrecpp::general::Result<ImageAsset>::failure(
                std::make_error_code(std::errc::no_such_file_or_directory), "Cannot open image file",
                "utils_gui.image");
            std::vector<std::uint8_t> encoded((std::istreambuf_iterator<char>(input)), {});
            return load_memory(encoded, requested_channels);
        } catch (const std::exception &error) {
            return ::sindrecpp::general::Result<ImageAsset>::failure(
                std::make_error_code(std::errc::io_error), error.what(), "utils_gui.image");
        }
    }
};

using IconAsset = ImageAsset;

inline ::sindrecpp::general::Result<ImageAsset> load_image(
    const std::filesystem::path &path, int requested_channels = 4) noexcept {
    return ImageAsset::load(path, requested_channels);
}

inline ::sindrecpp::general::Result<IconAsset> load_icon(
    const std::filesystem::path &path, int requested_channels = 4) noexcept {
    return ImageAsset::load(path, requested_channels);
}

using TextureHandle = ImTextureID;
struct TextureUploader {
    using Upload = std::function<::sindrecpp::general::Result<TextureHandle>(const ImageAsset &)>;
    Upload upload;
    ::sindrecpp::general::Result<TextureHandle> operator()(const ImageAsset &image) const noexcept {
        if (!upload) return ::sindrecpp::general::Result<TextureHandle>::failure(
            std::make_error_code(std::errc::function_not_supported), "No texture uploader is configured",
            "utils_gui.texture");
        try { return upload(image); }
        catch (const std::exception &error) { return ::sindrecpp::general::Result<TextureHandle>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "utils_gui.texture"); }
        catch (...) { return ::sindrecpp::general::Result<TextureHandle>::failure(
            std::make_error_code(std::errc::io_error), "Unknown texture upload failure", "utils_gui.texture"); }
    }
};

class ImageCache {
public:
    ::sindrecpp::general::Result<std::shared_ptr<const ImageAsset>> load(
        const std::filesystem::path &path) {
        const auto key = ::sindrecpp::general::path::to_utf8(path);
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = images_.find(key); it != images_.end())
            return ::sindrecpp::general::Result<std::shared_ptr<const ImageAsset>>::success(it->second);
        auto image = ImageAsset::load(path);
        if (!image) return ::sindrecpp::general::Result<std::shared_ptr<const ImageAsset>>::failure(image.error());
        auto stored = std::make_shared<ImageAsset>(std::move(image.value()));
        images_[key] = stored;
        return ::sindrecpp::general::Result<std::shared_ptr<const ImageAsset>>::success(std::move(stored));
    }
    void clear() noexcept { std::lock_guard<std::mutex> lock(mutex_); images_.clear(); }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<const ImageAsset>> images_;
};

#if defined(SINDRECPP_GUI_GLFW_OPENGL3)
struct GuiConfig {
    std::string title = "SindreCpp";
    int width = 1280;
    int height = 720;
    int monitor_index = -1;
    bool resizable = true;
    bool maximized = false;
    bool vsync = true;
    float dpi_scale = 0.0f;
    bool load_cjk_font = true;
    FontConfig font;
    ImVec4 clear_color = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
};

class GuiApplication {
public:
    static ::sindrecpp::general::Result<GuiApplication> create(const GuiConfig &config = {}) noexcept {
        if (config.width <= 0 || config.height <= 0 || config.dpi_scale < 0.0f)
            return ::sindrecpp::general::Result<GuiApplication>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid GUI configuration", "utils_gui.create");
        if (!glfwInit()) return ::sindrecpp::general::Result<GuiApplication>::failure(
            std::make_error_code(std::errc::not_supported), "GLFW initialization failed", "utils_gui.glfw");
        auto cleanup = ::sindrecpp::general::scope_guard([] { glfwTerminate(); });
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        GLFWwindow *window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
        if (!window) return ::sindrecpp::general::Result<GuiApplication>::failure(
            std::make_error_code(std::errc::io_error), "GLFW window creation failed", "utils_gui.window");
        auto destroy_window = ::sindrecpp::general::scope_guard([&] { glfwDestroyWindow(window); });
        int monitor_count = 0;
        GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
        const int selected_monitor = config.monitor_index < 0 ? 0 : config.monitor_index;
        if (monitor_count <= 0 || selected_monitor < 0 || selected_monitor >= monitor_count)
            return ::sindrecpp::general::Result<GuiApplication>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid monitor index", "utils_gui.monitor");
        int work_x = 0, work_y = 0, work_width = 0, work_height = 0;
        glfwGetMonitorWorkarea(monitors[selected_monitor], &work_x, &work_y, &work_width, &work_height);
        glfwSetWindowPos(window, work_x + std::max(0, (work_width - config.width) / 2),
                         work_y + std::max(0, (work_height - config.height) / 2));
        glfwMakeContextCurrent(window);
        float content_scale_x = 1.0f, content_scale_y = 1.0f;
        glfwGetWindowContentScale(window, &content_scale_x, &content_scale_y);
        const float scale = config.dpi_scale > 0.0f ? config.dpi_scale : std::max(content_scale_x, content_scale_y);
        Context context;
        context.make_current();
        apply_dark_theme(scale);
        if (config.load_cjk_font) {
            FontConfig font_config = config.font;
            font_config.size *= scale;
            auto font = load_font(font_config);
            if (!font) return ::sindrecpp::general::Result<GuiApplication>::failure(font.error());
        }
        const bool glfw_backend_initialized = ImGui_ImplGlfw_InitForOpenGL(window, true);
        const bool opengl_backend_initialized = glfw_backend_initialized && ImGui_ImplOpenGL3_Init("#version 150");
        if (!glfw_backend_initialized || !opengl_backend_initialized) {
            if (glfw_backend_initialized) ImGui_ImplGlfw_Shutdown();
            return ::sindrecpp::general::Result<GuiApplication>::failure(
                std::make_error_code(std::errc::io_error), "ImGui backend initialization failed", "utils_gui.backend");
        }
        GuiApplication result(std::move(context), window, config.clear_color, scale);
        result.backend_initialized_ = true;
        glfwSetWindowUserPointer(window, &result);
        glfwSetWindowContentScaleCallback(window, &GuiApplication::content_scale_callback);
        if (config.maximized) glfwMaximizeWindow(window);
        glfwSwapInterval(config.vsync ? 1 : 0);
        glfwShowWindow(window);
        cleanup.dismiss(); destroy_window.dismiss();
        return ::sindrecpp::general::Result<GuiApplication>::success(std::move(result));
    }

    GuiApplication(const GuiApplication &) = delete;
    GuiApplication &operator=(const GuiApplication &) = delete;
    GuiApplication(GuiApplication &&other) noexcept
        : context_(std::move(other.context_)), window_(std::exchange(other.window_, nullptr)),
          clear_color_(other.clear_color_), dpi_scale_(other.dpi_scale_), backend_initialized_(other.backend_initialized_) {
        other.backend_initialized_ = false;
        if (window_) glfwSetWindowUserPointer(window_, this);
    }
    GuiApplication &operator=(GuiApplication &&other) noexcept {
        if (this != &other) {
            shutdown(); context_ = std::move(other.context_); window_ = std::exchange(other.window_, nullptr);
            clear_color_ = other.clear_color_; dpi_scale_ = other.dpi_scale_; backend_initialized_ = other.backend_initialized_;
            other.backend_initialized_ = false; if (window_) glfwSetWindowUserPointer(window_, this);
        }
        return *this;
    }
    ~GuiApplication() { shutdown(); }

    void poll_events() noexcept { glfwPollEvents(); }
    bool should_close() const noexcept { return !window_ || glfwWindowShouldClose(window_) != 0; }
    void request_close() noexcept { if (window_) glfwSetWindowShouldClose(window_, GLFW_TRUE); }
    GLFWwindow *window() const noexcept { return window_; }
    float dpi_scale() const noexcept { return dpi_scale_; }
    ::sindrecpp::general::Result<void> begin_frame() noexcept {
        if (!window_ || !backend_initialized_) return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::operation_not_permitted), "GUI backend is not initialized", "utils_gui.frame");
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        return ::sindrecpp::general::Result<void>::success();
    }
    ::sindrecpp::general::Result<void> end_frame() noexcept {
        if (!window_ || !backend_initialized_) return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::operation_not_permitted), "GUI backend is not initialized", "utils_gui.frame");
        ImGui::Render();
        int width = 0, height = 0; glfwGetFramebufferSize(window_, &width, &height);
        glViewport(0, 0, width, height); glClearColor(clear_color_.x, clear_color_.y, clear_color_.z, clear_color_.w);
        glClear(GL_COLOR_BUFFER_BIT); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glfwSwapBuffers(window_);
        return ::sindrecpp::general::Result<void>::success();
    }

private:
    GuiApplication(Context context, GLFWwindow *window, ImVec4 clear_color, float scale)
        : context_(std::move(context)), window_(window), clear_color_(clear_color), dpi_scale_(scale) {}
    void shutdown() noexcept {
        if (!window_) return;
        glfwMakeContextCurrent(window_);
        if (backend_initialized_) { ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); backend_initialized_ = false; }
        glfwDestroyWindow(window_); window_ = nullptr; glfwTerminate();
    }
    static void content_scale_callback(GLFWwindow *window, float x, float y) {
        auto *application = static_cast<GuiApplication *>(glfwGetWindowUserPointer(window));
        if (!application) return;
        application->dpi_scale_ = std::max(x, y); apply_dark_theme(application->dpi_scale_);
    }
    Context context_;
    GLFWwindow *window_ = nullptr;
    ImVec4 clear_color_;
    float dpi_scale_ = 1.0f;
    bool backend_initialized_ = false;
};
#endif

class ScopedId {
public:
    explicit ScopedId(const char *id) { ImGui::PushID(id); }
    explicit ScopedId(int id) { ImGui::PushID(id); }
    ~ScopedId() { ImGui::PopID(); }
    ScopedId(const ScopedId &) = delete;
};

class ScopedDisabled {
public:
    explicit ScopedDisabled(bool disabled = true) : active_(disabled) { if (active_) ImGui::BeginDisabled(); }
    ~ScopedDisabled() { if (active_) ImGui::EndDisabled(); }
    ScopedDisabled(const ScopedDisabled &) = delete;
private:
    bool active_;
};

inline void tooltip(std::string_view text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip(); ImGui::TextUnformatted(text.data(), text.data() + text.size()); ImGui::EndTooltip();
    }
}

inline void help_marker(std::string_view text) { ImGui::TextDisabled("(?)"); tooltip(text); }

inline bool icon_button(const char *id, TextureHandle texture, ImVec2 size = ImVec2(24, 24)) {
    return ImGui::ImageButton(id, texture, size);
}

inline bool input_text(const char *label, std::string &value, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    struct CallbackData { std::string *value; } callback_data{&value};
    auto callback = [](ImGuiInputTextCallbackData *data) -> int {
        if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) return 0;
        auto *state = static_cast<CallbackData *>(data->UserData);
        state->value->resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = state->value->data(); data->BufSize = static_cast<int>(state->value->capacity() + 1);
        return 0;
    };
    if (value.capacity() < 32) value.reserve(32);
    return ImGui::InputText(label, value.data(), value.capacity() + 1, flags, callback, &callback_data);
}

inline void image(TextureHandle texture, const ImVec2 &size,
                  const ImVec2 &uv0 = ImVec2(0, 0), const ImVec2 &uv1 = ImVec2(1, 1),
                  const ImVec4 &tint = ImVec4(1, 1, 1, 1),
                  const ImVec4 &border = ImVec4(0, 0, 0, 0)) {
    ImGui::Image(texture, size, uv0, uv1, tint, border);
}

inline bool image_button(std::string_view id, TextureHandle texture, const ImVec2 &size,
                         const ImVec2 &uv0 = ImVec2(0, 0), const ImVec2 &uv1 = ImVec2(1, 1),
                         const ImVec4 &bg = ImVec4(0, 0, 0, 0),
                         const ImVec4 &tint = ImVec4(1, 1, 1, 1)) {
    const std::string stable_id(id);
    return ImGui::ImageButton(stable_id.c_str(), texture, size, uv0, uv1,
                              bg, tint);
}

} // namespace sindrecpp::utils_gui
