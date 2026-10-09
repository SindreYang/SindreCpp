#pragma once

/// @file
/// @brief 跨平台 GUI 生命周期、字体、图片和常用控件 facade。

#if !defined(SINDRE_WITH_GUI)
#error "Enable SINDRE_WITH_GUI and link sindre::gui before including this header."
#endif

#include <sindre/general.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sindre::gui {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

using TextureHandle = std::uintptr_t;
using InputFlags = std::uint32_t;

class Context {
public:
    Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&other) noexcept;
    Context &operator=(Context &&other) noexcept;
    ~Context();

    [[nodiscard]] bool is_valid() const noexcept;
    void make_current() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct FontConfig {
    std::filesystem::path path;
    float size = 18.0f;
    bool require_cjk = false;
    std::vector<std::filesystem::path> search_directories;
};

struct FontInfo {
    std::uintptr_t handle = 0;
    std::filesystem::path path;
    bool fallback = false;

    [[nodiscard]] bool is_valid() const noexcept { return handle != 0; }
};

std::vector<std::filesystem::path> default_font_directories();
::sindre::general::Result<FontInfo> load_font(
    const FontConfig &config = {}) noexcept;
void apply_dark_theme(float scale = 1.0f);

struct ImageAsset {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] const std::uint8_t *data() const noexcept;

    static ::sindre::general::Result<ImageAsset> load_memory(
        const std::vector<std::uint8_t> &encoded,
        int requested_channels = 4) noexcept;
    static ::sindre::general::Result<ImageAsset> load(
        const std::filesystem::path &path,
        int requested_channels = 4) noexcept;
};

using IconAsset = ImageAsset;

::sindre::general::Result<ImageAsset> load_image(
    const std::filesystem::path &path,
    int requested_channels = 4) noexcept;
::sindre::general::Result<IconAsset> load_icon(
    const std::filesystem::path &path,
    int requested_channels = 4) noexcept;

struct TextureUploader {
    using Upload = std::function<
        ::sindre::general::Result<TextureHandle>(const ImageAsset &)>;
    Upload upload;

    ::sindre::general::Result<TextureHandle> operator()(
        const ImageAsset &image) const noexcept;
};

class ImageCache {
public:
    ::sindre::general::Result<std::shared_ptr<const ImageAsset>> load(
        const std::filesystem::path &path) noexcept;
    void clear() noexcept;

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<const ImageAsset>> images_;
};

struct GuiConfig {
    std::string title = "sindre";
    int width = 1280;
    int height = 720;
    int monitor_index = -1;
    bool resizable = true;
    bool maximized = false;
    bool vsync = true;
    float dpi_scale = 0.0f;
    bool load_cjk_font = true;
    FontConfig font;
    Color clear_color{0.08f, 0.08f, 0.10f, 1.0f};
};

#if defined(SINDRE_GUI_GLFW_OPENGL3)
class GuiApplication {
public:
    static ::sindre::general::Result<GuiApplication> create(
        const GuiConfig &config = {}) noexcept;

    GuiApplication(const GuiApplication &) = delete;
    GuiApplication &operator=(const GuiApplication &) = delete;
    GuiApplication(GuiApplication &&other) noexcept;
    GuiApplication &operator=(GuiApplication &&other) noexcept;
    ~GuiApplication();

    void poll_events() noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    void request_close() noexcept;
    [[nodiscard]] float get_dpi_scale() const noexcept;
    ::sindre::general::Result<void> begin_frame() noexcept;
    ::sindre::general::Result<void> end_frame() noexcept;

private:
    GuiApplication(Context context, void *window, Color clear_color,
                   float scale, bool glfw_runtime_acquired);
    void shutdown() noexcept;
    static void content_scale_callback(void *window, float x, float y);

    Context context_;
    void *window_ = nullptr;
    Color clear_color_{};
    float dpi_scale_ = 1.0f;
    bool backend_initialized_ = false;
    bool glfw_runtime_acquired_ = false;
    bool backend_instance_acquired_ = false;
};
#endif

class GuiApplication;

class Frame {
public:
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;
    Frame(Frame &&other) noexcept;
    Frame &operator=(Frame &&other) noexcept;
    ~Frame();

private:
    explicit Frame(std::shared_ptr<GuiApplication> application) noexcept;
    void finish() noexcept;
    friend ::sindre::general::Result<Frame> gui_begin() noexcept;

    std::shared_ptr<GuiApplication> application_;
    bool active_ = false;
};

::sindre::general::Result<void> gui_init(
    const GuiConfig &config = {}) noexcept;
::sindre::general::Result<Frame> gui_begin() noexcept;
bool gui_should_close() noexcept;
void gui_request_close() noexcept;
void gui_shutdown() noexcept;

class ScopedId {
public:
    explicit ScopedId(const char *id);
    explicit ScopedId(int id);
    ~ScopedId();
    ScopedId(const ScopedId &) = delete;
};

class ScopedDisabled {
public:
    explicit ScopedDisabled(bool disabled = true);
    ~ScopedDisabled();
    ScopedDisabled(const ScopedDisabled &) = delete;

private:
    bool active_ = false;
};

void tooltip(std::string_view text);
void help_marker(std::string_view text);
bool icon_button(const char *id, TextureHandle texture,
                 Vec2 size = {24.0f, 24.0f});
bool input_text(const char *label, std::string &value,
                InputFlags flags = 0);
void image(TextureHandle texture, const Vec2 &size,
           const Vec2 &uv0 = {0.0f, 0.0f},
           const Vec2 &uv1 = {1.0f, 1.0f},
           const Color &tint = {1.0f, 1.0f, 1.0f, 1.0f},
           const Color &border = {});
bool image_button(std::string_view id, TextureHandle texture,
                  const Vec2 &size,
                  const Vec2 &uv0 = {0.0f, 0.0f},
                  const Vec2 &uv1 = {1.0f, 1.0f},
                  const Color &background = {},
                  const Color &tint = {1.0f, 1.0f, 1.0f, 1.0f});

} // namespace sindre::gui
