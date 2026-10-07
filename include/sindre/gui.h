#pragma once

#if !defined(SINDRE_WITH_GUI)
#error "Enable SINDRE_WITH_GUI and link sindre::gui before including this header."
#endif

#include <imgui.h>
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

#if defined(SINDRE_GUI_GLFW_OPENGL3)
struct GLFWwindow;
#endif

namespace sindre::gui {

/// @brief 暴露完整的 Dear ImGui API，便于高级用户直接使用原生控件。
namespace imgui = ::ImGui;
namespace native = imgui;

/// @brief 管理一个 Dear ImGui 上下文及其当前线程绑定。
class Context {
public:
    explicit Context(ImFontAtlas *shared_font_atlas = nullptr);
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&other) noexcept;
    Context &operator=(Context &&other) noexcept;
    ~Context();

    ImGuiContext *get_context() const noexcept;
    void make_current() const noexcept;

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

/// @brief 返回当前平台常见的字体目录。
std::vector<std::filesystem::path> default_font_directories();
/// @brief 加载字体；需要中文时会优先选择可覆盖 CJK 字符的字体。
::sindre::general::Result<FontInfo> load_font(const FontConfig &config = {}) noexcept;
/// @brief 应用统一的深色、圆角和间距主题。
void apply_dark_theme(float scale = 1.0f);

/// @brief 可供纹理后端上传的解码后图片。
struct ImageAsset {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<std::uint8_t> pixels;

    /// @brief 判断图片是否没有有效像素。
    bool empty() const noexcept;
    /// @brief 返回连续像素数据；空图片返回 nullptr。
    const std::uint8_t *data() const noexcept;

    /// @brief 从内存中的 PNG/JPEG 等编码数据解码图片。
    static ::sindre::general::Result<ImageAsset> load_memory(
        const std::vector<std::uint8_t> &encoded, int requested_channels = 4) noexcept;
    /// @brief 从 UTF-8 或宽字符路径加载图片。
    static ::sindre::general::Result<ImageAsset> load(
        const std::filesystem::path &path, int requested_channels = 4) noexcept;
};

using IconAsset = ImageAsset;

/// @brief 加载普通图片资源。
::sindre::general::Result<ImageAsset> load_image(
    const std::filesystem::path &path, int requested_channels = 4) noexcept;
/// @brief 加载图标资源；当前与图片使用相同的解码路径。
::sindre::general::Result<IconAsset> load_icon(
    const std::filesystem::path &path, int requested_channels = 4) noexcept;

using TextureHandle = ImTextureID;

struct TextureUploader {
    using Upload = std::function<::sindre::general::Result<TextureHandle>(const ImageAsset &)>;
    Upload upload;

    /// @brief 将图片交给宿主渲染后端并返回纹理句柄。
    ::sindre::general::Result<TextureHandle> operator()(
        const ImageAsset &image) const noexcept;
};

class ImageCache {
public:
    /// @brief 加载并缓存图片，缓存键为规范化后的路径。
    ::sindre::general::Result<std::shared_ptr<const ImageAsset>> load(
        const std::filesystem::path &path) noexcept;
    /// @brief 清空当前缓存。
    void clear() noexcept;

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<const ImageAsset>> images_;
};

/// @brief GLFW/OpenGL3 窗口和 ImGui 帧循环配置。
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
    ImVec4 clear_color = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
};

#if defined(SINDRE_GUI_GLFW_OPENGL3)
class GuiApplication {
public:
    /// @brief 创建窗口、ImGui 上下文并按配置初始化字体和主题。
    static ::sindre::general::Result<GuiApplication> create(
        const GuiConfig &config = {}) noexcept;

    GuiApplication(const GuiApplication &) = delete;
    GuiApplication &operator=(const GuiApplication &) = delete;
    GuiApplication(GuiApplication &&other) noexcept;
    GuiApplication &operator=(GuiApplication &&other) noexcept;
    ~GuiApplication();

    /// @brief 轮询窗口系统事件。
    void poll_events() noexcept;
    /// @brief 判断窗口是否收到关闭请求。
    bool should_close() const noexcept;
    /// @brief 请求关闭窗口。
    void request_close() noexcept;
    GLFWwindow *get_window() const noexcept;
    float get_dpi_scale() const noexcept;
    /// @brief 开始一个 ImGui 帧。
    ::sindre::general::Result<void> begin_frame() noexcept;
    /// @brief 渲染并提交当前 ImGui 帧。
    ::sindre::general::Result<void> end_frame() noexcept;

private:
    GuiApplication(Context context, GLFWwindow *window, ImVec4 clear_color,
                   float scale, bool glfw_runtime_acquired);
    void shutdown() noexcept;
    static void content_scale_callback(GLFWwindow *window, float x, float y);

    Context context_;
    GLFWwindow *window_ = nullptr;
    ImVec4 clear_color_;
    float dpi_scale_ = 1.0f;
    bool backend_initialized_ = false;
    bool glfw_runtime_acquired_ = false;
};
#endif

class GuiApplication;

/// @brief 全局简化 GUI 生命周期的帧对象，析构时自动提交当前帧。
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

/// @brief 使用默认配置初始化全局 GUI；默认配置适合快速创建单窗口应用。
::sindre::general::Result<void> gui_init(
    const GuiConfig &config = {}) noexcept;
/// @brief 轮询事件并开始一帧；返回对象离开作用域时自动结束并提交帧。
::sindre::general::Result<Frame> gui_begin() noexcept;
/// @brief 查询全局 GUI 窗口是否收到关闭请求。
bool gui_should_close() noexcept;
/// @brief 请求关闭全局 GUI 窗口。
void gui_request_close() noexcept;
/// @brief 销毁全局 GUI；可以重复调用。
void gui_shutdown() noexcept;

class ScopedId {
public:
    /// @brief 在作用域内压入一个 ImGui ID。
    explicit ScopedId(const char *id);
    explicit ScopedId(int id);
    ~ScopedId();
    ScopedId(const ScopedId &) = delete;
};

class ScopedDisabled {
public:
    /// @brief 在作用域内按需禁用 ImGui 控件。
    explicit ScopedDisabled(bool disabled = true);
    ~ScopedDisabled();
    ScopedDisabled(const ScopedDisabled &) = delete;

private:
    bool active_ = false;
};

/// @brief 为当前控件显示悬浮提示。
void tooltip(std::string_view text);
/// @brief 绘制帮助标记并显示说明。
void help_marker(std::string_view text);
bool icon_button(const char *id, TextureHandle texture,
                 ImVec2 size = ImVec2(24, 24));
bool input_text(const char *label, std::string &value,
                ImGuiInputTextFlags flags = 0);
void image(TextureHandle texture, const ImVec2 &size,
           const ImVec2 &uv0 = ImVec2(0, 0), const ImVec2 &uv1 = ImVec2(1, 1),
           const ImVec4 &tint = ImVec4(1, 1, 1, 1),
           const ImVec4 &border = ImVec4(0, 0, 0, 0));
bool image_button(std::string_view id, TextureHandle texture, const ImVec2 &size,
                  const ImVec2 &uv0 = ImVec2(0, 0), const ImVec2 &uv1 = ImVec2(1, 1),
                  const ImVec4 &bg = ImVec4(0, 0, 0, 0),
                  const ImVec4 &tint = ImVec4(1, 1, 1, 1));

} // namespace sindre::gui
