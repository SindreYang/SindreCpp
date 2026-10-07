#include <sindre/gui.h>

#include <sindre/general/system.h>
#include <sindre/general/core.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <utility>

#if defined(SINDRE_GUI_GLFW_OPENGL3)
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

#if defined(SINDRE_GUI_STB_IMAGE)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION
#endif

namespace sindre::gui {

#if defined(SINDRE_GUI_GLFW_OPENGL3)
namespace {

class GlfwRuntime {
public:
    static bool acquire() noexcept {
        std::lock_guard<std::mutex> lock(mutex());
        if (ref_count() == 0 && !glfwInit()) return false;
        ++ref_count();
        return true;
    }

    static void release() noexcept {
        std::lock_guard<std::mutex> lock(mutex());
        if (ref_count() == 0) return;
        if (--ref_count() == 0) glfwTerminate();
    }

private:
    static std::mutex &mutex() noexcept {
        static std::mutex value;
        return value;
    }

    static std::size_t &ref_count() noexcept {
        static std::size_t value = 0;
        return value;
    }
};

} // namespace
#endif

Context::Context(ImFontAtlas *shared_font_atlas)
    : context_(ImGui::CreateContext(shared_font_atlas)) {}

Context::Context(Context &&other) noexcept
    : context_(other.context_) {
    other.context_ = nullptr;
}

Context &Context::operator=(Context &&other) noexcept {
    if (this != &other) {
        if (context_) ImGui::DestroyContext(context_);
        context_ = other.context_;
        other.context_ = nullptr;
    }
    return *this;
}

Context::~Context() {
    if (context_) ImGui::DestroyContext(context_);
}

ImGuiContext *Context::get_context() const noexcept {
    return context_;
}

void Context::make_current() const noexcept {
    ImGui::SetCurrentContext(context_);
}

std::vector<std::filesystem::path> default_font_directories() {
    std::vector<std::filesystem::path> result;
#if defined(_WIN32)
    if (auto windows = ::sindre::general::system::environment("WINDIR"))
        result.emplace_back(std::filesystem::path(windows.value()) / "Fonts");
    if (auto local = ::sindre::general::system::environment("LOCALAPPDATA"))
        result.emplace_back(std::filesystem::path(local.value()) / "Microsoft/Windows/Fonts");
#elif defined(__APPLE__)
    result.emplace_back("/System/Library/Fonts");
    result.emplace_back("/Library/Fonts");
    if (auto home = ::sindre::general::system::environment("HOME"))
        result.emplace_back(std::filesystem::path(home.value()) / "Library/Fonts");
#else
    result.emplace_back("/usr/share/fonts");
    result.emplace_back("/usr/local/share/fonts");
    if (auto home = ::sindre::general::system::environment("HOME")) {
        result.emplace_back(std::filesystem::path(home.value()) / ".fonts");
        result.emplace_back(std::filesystem::path(home.value()) / ".local/share/fonts");
    }
#endif
    return result;
}

::sindre::general::Result<FontInfo> load_font(const FontConfig &config) noexcept {
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
                    if (std::filesystem::is_regular_file(path)) {
                        selected = path;
                        break;
                    }
                }
                if (!selected.empty()) break;
            }
        }
        if (!selected.empty()) {
            if (!std::filesystem::is_regular_file(selected))
                return ::sindre::general::Result<FontInfo>::failure(
                    std::make_error_code(std::errc::no_such_file_or_directory),
                    "Font file does not exist", "gui.font");
            ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
                ::sindre::general::path::to_utf8(selected).c_str(), config.size, nullptr,
                ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
            if (!font) return ::sindre::general::Result<FontInfo>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Cannot load font file", "gui.font");
            return ::sindre::general::Result<FontInfo>::success({font, selected, false});
        }
        if (config.require_cjk) return ::sindre::general::Result<FontInfo>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory),
            "No CJK font was found", "gui.font");
        return ::sindre::general::Result<FontInfo>::success(
            {ImGui::GetIO().Fonts->AddFontDefault(), {}, true});
    } catch (const std::exception &error) {
        return ::sindre::general::Result<FontInfo>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "gui.font");
    } catch (...) {
        return ::sindre::general::Result<FontInfo>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown font loading failure", "gui.font");
    }
}

void apply_dark_theme(float scale) {
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

bool ImageAsset::empty() const noexcept {
    return pixels.empty() || width <= 0 || height <= 0;
}

const std::uint8_t *ImageAsset::data() const noexcept {
    return pixels.data();
}

::sindre::general::Result<ImageAsset> ImageAsset::load_memory(
    const std::vector<std::uint8_t> &encoded, int requested_channels) noexcept {
#if defined(SINDRE_GUI_STB_IMAGE)
    if (encoded.empty() || requested_channels < 1 || requested_channels > 4)
        return ::sindre::general::Result<ImageAsset>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid image input", "gui.image");
    if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return ::sindre::general::Result<ImageAsset>::failure(
            std::make_error_code(std::errc::file_too_large),
            "Image input is too large", "gui.image");
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc *decoded = stbi_load_from_memory(
        encoded.data(), static_cast<int>(encoded.size()), &width, &height,
        &channels, requested_channels);
    if (!decoded) return ::sindre::general::Result<ImageAsset>::failure(
        std::make_error_code(std::errc::invalid_argument),
        stbi_failure_reason() ? stbi_failure_reason() : "Invalid image data", "gui.image");
    try {
        auto release = ::sindre::general::scope_guard([&] { stbi_image_free(decoded); });
        ImageAsset result;
        result.width = width;
        result.height = height;
        result.channels = requested_channels;
        result.pixels.assign(
            decoded, decoded + static_cast<std::size_t>(width) * height * requested_channels);
        release.dismiss();
        stbi_image_free(decoded);
        return ::sindre::general::Result<ImageAsset>::success(std::move(result));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<ImageAsset>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "gui.image");
    } catch (...) {
        return ::sindre::general::Result<ImageAsset>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown image decoding failure", "gui.image");
    }
#else
    (void)encoded;
    (void)requested_channels;
    return ::sindre::general::Result<ImageAsset>::failure(
        std::make_error_code(std::errc::function_not_supported),
        "stb_image is not enabled", "gui.image");
#endif
}

::sindre::general::Result<ImageAsset> ImageAsset::load(
    const std::filesystem::path &path, int requested_channels) noexcept {
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) return ::sindre::general::Result<ImageAsset>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory),
            "Cannot open image file", "gui.image");
        std::vector<std::uint8_t> encoded((std::istreambuf_iterator<char>(input)), {});
        return load_memory(encoded, requested_channels);
    } catch (const std::exception &error) {
        return ::sindre::general::Result<ImageAsset>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "gui.image");
    } catch (...) {
        return ::sindre::general::Result<ImageAsset>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown image loading failure", "gui.image");
    }
}

::sindre::general::Result<ImageAsset> load_image(
    const std::filesystem::path &path, int requested_channels) noexcept {
    return ImageAsset::load(path, requested_channels);
}

::sindre::general::Result<IconAsset> load_icon(
    const std::filesystem::path &path, int requested_channels) noexcept {
    return ImageAsset::load(path, requested_channels);
}

::sindre::general::Result<TextureHandle> TextureUploader::operator()(
    const ImageAsset &image) const noexcept {
    if (!upload) return ::sindre::general::Result<TextureHandle>::failure(
        std::make_error_code(std::errc::function_not_supported),
        "No texture uploader is configured", "gui.texture");
    try {
        return upload(image);
    } catch (const std::exception &error) {
        return ::sindre::general::Result<TextureHandle>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "gui.texture");
    } catch (...) {
        return ::sindre::general::Result<TextureHandle>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown texture upload failure", "gui.texture");
    }
}

::sindre::general::Result<std::shared_ptr<const ImageAsset>> ImageCache::load(
    const std::filesystem::path &path) noexcept {
    try {
        const auto key = ::sindre::general::path::to_utf8(path);
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = images_.find(key); it != images_.end())
            return ::sindre::general::Result<std::shared_ptr<const ImageAsset>>::success(it->second);
        auto image = ImageAsset::load(path);
        if (!image) return ::sindre::general::Result<std::shared_ptr<const ImageAsset>>::failure(image.error());
        auto stored = std::make_shared<ImageAsset>(std::move(image.value()));
        images_[key] = stored;
        return ::sindre::general::Result<std::shared_ptr<const ImageAsset>>::success(std::move(stored));
    } catch (const std::exception &error) {
        return ::sindre::general::Result<std::shared_ptr<const ImageAsset>>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "gui.image_cache");
    } catch (...) {
        return ::sindre::general::Result<std::shared_ptr<const ImageAsset>>::failure(
            std::make_error_code(std::errc::io_error),
            "Unknown image cache failure", "gui.image_cache");
    }
}

void ImageCache::clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    images_.clear();
}

#if defined(SINDRE_GUI_GLFW_OPENGL3)
::sindre::general::Result<GuiApplication> GuiApplication::create(
    const GuiConfig &config) noexcept {
    if (config.width <= 0 || config.height <= 0 || config.dpi_scale < 0.0f)
        return ::sindre::general::Result<GuiApplication>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid GUI configuration", "gui.create");
    if (!GlfwRuntime::acquire()) return ::sindre::general::Result<GuiApplication>::failure(
        std::make_error_code(std::errc::not_supported),
        "GLFW initialization failed", "gui.glfw");
    auto cleanup = ::sindre::general::scope_guard([] { GlfwRuntime::release(); });
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(
        config.width, config.height, config.title.c_str(), nullptr, nullptr);
    if (!window) return ::sindre::general::Result<GuiApplication>::failure(
        std::make_error_code(std::errc::io_error),
        "GLFW window creation failed", "gui.window");
    auto destroy_window = ::sindre::general::scope_guard([&] { glfwDestroyWindow(window); });
    int monitor_count = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
    const int selected_monitor = config.monitor_index < 0 ? 0 : config.monitor_index;
    if (monitor_count <= 0 || selected_monitor < 0 || selected_monitor >= monitor_count)
        return ::sindre::general::Result<GuiApplication>::failure(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid monitor index", "gui.monitor");
    int work_x = 0;
    int work_y = 0;
    int work_width = 0;
    int work_height = 0;
    glfwGetMonitorWorkarea(monitors[selected_monitor], &work_x, &work_y,
                           &work_width, &work_height);
    glfwSetWindowPos(window, work_x + std::max(0, (work_width - config.width) / 2),
                     work_y + std::max(0, (work_height - config.height) / 2));
    glfwMakeContextCurrent(window);
    float content_scale_x = 1.0f;
    float content_scale_y = 1.0f;
    glfwGetWindowContentScale(window, &content_scale_x, &content_scale_y);
    const float scale = config.dpi_scale > 0.0f
                            ? config.dpi_scale
                            : std::max(content_scale_x, content_scale_y);
    Context context;
    context.make_current();
    apply_dark_theme(scale);
    if (config.load_cjk_font) {
        FontConfig font_config = config.font;
        font_config.size *= scale;
        auto font = load_font(font_config);
        if (!font) return ::sindre::general::Result<GuiApplication>::failure(font.error());
    }
    const bool glfw_backend_initialized = ImGui_ImplGlfw_InitForOpenGL(window, true);
    const bool opengl_backend_initialized =
        glfw_backend_initialized && ImGui_ImplOpenGL3_Init("#version 150");
    if (!glfw_backend_initialized || !opengl_backend_initialized) {
        if (glfw_backend_initialized) ImGui_ImplGlfw_Shutdown();
        return ::sindre::general::Result<GuiApplication>::failure(
            std::make_error_code(std::errc::io_error),
            "ImGui backend initialization failed", "gui.backend");
    }
    GuiApplication result(std::move(context), window, config.clear_color, scale, true);
    result.backend_initialized_ = true;
    glfwSetWindowUserPointer(window, &result);
    glfwSetWindowContentScaleCallback(window, &GuiApplication::content_scale_callback);
    if (config.maximized) glfwMaximizeWindow(window);
    glfwSwapInterval(config.vsync ? 1 : 0);
    glfwShowWindow(window);
    cleanup.dismiss();
    destroy_window.dismiss();
    return ::sindre::general::Result<GuiApplication>::success(std::move(result));
}

GuiApplication::GuiApplication(Context context, GLFWwindow *window,
                               ImVec4 clear_color, float scale,
                               bool glfw_runtime_acquired)
    : context_(std::move(context)), window_(window), clear_color_(clear_color),
      dpi_scale_(scale), glfw_runtime_acquired_(glfw_runtime_acquired) {}

GuiApplication::GuiApplication(GuiApplication &&other) noexcept
    : context_(std::move(other.context_)),
      window_(std::exchange(other.window_, nullptr)),
      clear_color_(other.clear_color_), dpi_scale_(other.dpi_scale_),
      backend_initialized_(other.backend_initialized_),
      glfw_runtime_acquired_(other.glfw_runtime_acquired_) {
    other.backend_initialized_ = false;
    other.glfw_runtime_acquired_ = false;
    if (window_) glfwSetWindowUserPointer(window_, this);
}

GuiApplication &GuiApplication::operator=(GuiApplication &&other) noexcept {
    if (this != &other) {
        shutdown();
        context_ = std::move(other.context_);
        window_ = std::exchange(other.window_, nullptr);
        clear_color_ = other.clear_color_;
        dpi_scale_ = other.dpi_scale_;
        backend_initialized_ = other.backend_initialized_;
        glfw_runtime_acquired_ = other.glfw_runtime_acquired_;
        other.backend_initialized_ = false;
        other.glfw_runtime_acquired_ = false;
        if (window_) glfwSetWindowUserPointer(window_, this);
    }
    return *this;
}

GuiApplication::~GuiApplication() {
    shutdown();
}

void GuiApplication::poll_events() noexcept {
    glfwPollEvents();
}

bool GuiApplication::should_close() const noexcept {
    return !window_ || glfwWindowShouldClose(window_) != 0;
}

void GuiApplication::request_close() noexcept {
    if (window_) glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

GLFWwindow *GuiApplication::get_window() const noexcept {
    return window_;
}

float GuiApplication::get_dpi_scale() const noexcept {
    return dpi_scale_;
}

::sindre::general::Result<void> GuiApplication::begin_frame() noexcept {
    if (!window_ || !backend_initialized_) return ::sindre::general::Result<void>::failure(
        std::make_error_code(std::errc::operation_not_permitted),
        "GUI backend is not initialized", "gui.frame");
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    return ::sindre::general::Result<void>::success();
}

::sindre::general::Result<void> GuiApplication::end_frame() noexcept {
    if (!window_ || !backend_initialized_) return ::sindre::general::Result<void>::failure(
        std::make_error_code(std::errc::operation_not_permitted),
        "GUI backend is not initialized", "gui.frame");
    ImGui::Render();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(clear_color_.x, clear_color_.y, clear_color_.z, clear_color_.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
    return ::sindre::general::Result<void>::success();
}

void GuiApplication::shutdown() noexcept {
    if (window_) {
        glfwMakeContextCurrent(window_);
        if (backend_initialized_) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            backend_initialized_ = false;
        }
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfw_runtime_acquired_) {
        GlfwRuntime::release();
        glfw_runtime_acquired_ = false;
    }
}

void GuiApplication::content_scale_callback(GLFWwindow *window, float x, float y) {
    auto *application = static_cast<GuiApplication *>(glfwGetWindowUserPointer(window));
    if (!application) return;
    application->dpi_scale_ = std::max(x, y);
    apply_dark_theme(application->dpi_scale_);
}
#endif

ScopedId::ScopedId(const char *id) {
    ImGui::PushID(id);
}

ScopedId::ScopedId(int id) {
    ImGui::PushID(id);
}

ScopedId::~ScopedId() {
    ImGui::PopID();
}

ScopedDisabled::ScopedDisabled(bool disabled)
    : active_(disabled) {
    if (active_) ImGui::BeginDisabled();
}

ScopedDisabled::~ScopedDisabled() {
    if (active_) ImGui::EndDisabled();
}

void tooltip(std::string_view text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::EndTooltip();
    }
}

void help_marker(std::string_view text) {
    ImGui::TextDisabled("(?)");
    tooltip(text);
}

bool icon_button(const char *id, TextureHandle texture, ImVec2 size) {
    return ImGui::ImageButton(id, texture, size);
}

bool input_text(const char *label, std::string &value, ImGuiInputTextFlags flags) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    struct CallbackData {
        std::string *value;
    } callback_data{&value};
    auto callback = [](ImGuiInputTextCallbackData *data) -> int {
        if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) return 0;
        auto *state = static_cast<CallbackData *>(data->UserData);
        state->value->resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = state->value->data();
        data->BufSize = static_cast<int>(state->value->capacity() + 1);
        return 0;
    };
    if (value.capacity() < 32) value.reserve(32);
    return ImGui::InputText(label, value.data(), value.capacity() + 1,
                            flags, callback, &callback_data);
}

void image(TextureHandle texture, const ImVec2 &size, const ImVec2 &uv0,
           const ImVec2 &uv1, const ImVec4 &tint, const ImVec4 &border) {
    ImGui::Image(texture, size, uv0, uv1, tint, border);
}

bool image_button(std::string_view id, TextureHandle texture, const ImVec2 &size,
                  const ImVec2 &uv0, const ImVec2 &uv1, const ImVec4 &bg,
                  const ImVec4 &tint) {
    const std::string stable_id(id);
    return ImGui::ImageButton(stable_id.c_str(), texture, size, uv0, uv1, bg, tint);
}

} // namespace sindre::gui
