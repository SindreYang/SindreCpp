#include <sindre/gui.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ << ": " #condition << '\n'; \
    return EXIT_FAILURE; \
} } while (false)

int main() {
    using namespace sindre::gui;
    Context context;
    context.make_current();
    apply_dark_theme();
    CHECK(ImGui::GetStyle().WindowRounding > 0.0f);

    const std::vector<std::uint8_t> ppm{
        'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 255, 0, 0};
    const auto path = std::filesystem::temp_directory_path() / L"sindre-中文图标.ppm";
    auto memory_image = ImageAsset::load_memory(ppm);
#if defined(SINDRE_GUI_STB_IMAGE)
    CHECK(memory_image && memory_image.value().width == 1 && memory_image.value().height == 1 &&
          memory_image.value().channels == 4 && memory_image.value().pixels.size() == 4);

    { std::ofstream output(path, std::ios::binary); output.write(
        reinterpret_cast<const char *>(ppm.data()), static_cast<std::streamsize>(ppm.size())); }
    auto file_image = ImageAsset::load(path);
    CHECK(file_image && file_image.value().pixels == memory_image.value().pixels);
    auto icon_image = load_icon(path);
    CHECK(icon_image && icon_image.value().pixels == memory_image.value().pixels);
    auto generic_image = load_image(path);
    CHECK(generic_image && generic_image.value().pixels == memory_image.value().pixels);
    ImageCache cache;
    auto cached_a = cache.load(path);
    auto cached_b = cache.load(path);
    CHECK(cached_a && cached_b && cached_a.value() == cached_b.value());
    std::filesystem::remove(path);
#else
    CHECK(!memory_image && memory_image.error().code ==
          std::make_error_code(std::errc::function_not_supported));
#endif

    TextureUploader uploader;
    const ImageAsset test_image = memory_image ? memory_image.value() : ImageAsset{};
    CHECK(!uploader(test_image));
    bool callback_called = false;
    uploader.upload = [&](const ImageAsset &image) {
        callback_called = image.width == (memory_image ? 1 : 0);
        return sindre::general::Result<TextureHandle>::success(TextureHandle{});
    };
    CHECK(uploader(test_image) && callback_called);

    auto missing_font = load_font(FontConfig{{path}, 18.0f, true, {}});
    CHECK(!missing_font && missing_font.error().context == "gui.font");
    auto fallback_font = load_font(FontConfig{{}, 18.0f, false, {path.parent_path()}});
    CHECK(fallback_font && fallback_font.value().font != nullptr);

#if defined(SINDRE_GUI_RUNTIME_TEST)
    GuiConfig config;
    config.title = "sindre 中文 GUI";
    config.width = 640;
    config.height = 480;
    config.load_cjk_font = false;
    auto application = GuiApplication::create(config);
    CHECK(application);
    CHECK(application.value().begin_frame());
    ImGui::Begin("sindre");
    std::string text = "中文";
    (void)input_text("text", text);
    ImGui::End();
    CHECK(application.value().end_frame());
    application.value().request_close();
#endif
    return EXIT_SUCCESS;
}
