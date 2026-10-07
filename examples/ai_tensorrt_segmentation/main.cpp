#include <sindre/ai/trt.h>
#include <sindre/general/cli.h>
#include <sindre/general/string.h>
#include <sindre/general/system.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace general = sindre::general;
namespace ai = sindre::ai;

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;
};

general::Result<void> ensure_parent_directory(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if (parent.empty()) return general::Result<void>::success();
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) return general::Result<void>::failure(
        error, "Cannot create output directory", "example.segmentation.output");
    return general::Result<void>::success();
}

int report_failure(std::string_view operation, const general::Error& error) {
    std::cerr << operation << " failed: " << error.describe() << '\n';
    return 1;
}

bool read_token(std::istream& input, std::string& token) {
    token.clear();
    char character = 0;
    while (input.get(character)) {
        if (character == '#') {
            input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(character))) {
            token.push_back(character);
            break;
        }
    }
    if (token.empty()) return false;
    while (input.get(character)) {
        if (character == '#') {
            input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            break;
        }
        if (std::isspace(static_cast<unsigned char>(character))) break;
        token.push_back(character);
    }
    return true;
}

general::Result<Image> read_ppm(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return general::Result<Image>::failure(
        general::Error::make(std::errc::no_such_file_or_directory,
                             "Cannot open PPM input", "example.segmentation.input"));
    std::string magic, width_text, height_text, max_value_text;
    if (!read_token(input, magic) || !read_token(input, width_text) ||
        !read_token(input, height_text) || !read_token(input, max_value_text) ||
        magic != "P6")
        return general::Result<Image>::failure(
            general::Error::make(std::errc::invalid_argument,
                                 "Input must be a binary P6 PPM image",
                                 "example.segmentation.input"));
    const auto width = general::string::parse_int(width_text);
    const auto height = general::string::parse_int(height_text);
    const auto max_value = general::string::parse_int(max_value_text);
    if (!width || !height || !max_value || width.value() <= 0 || height.value() <= 0 ||
        max_value.value() != 255)
        return general::Result<Image>::failure(
            general::Error::make(std::errc::invalid_argument,
                                 "PPM requires positive dimensions and max value 255",
                                 "example.segmentation.input"));
    // read_token consumed the first delimiter; consume the second byte only
    // for a Windows CRLF header so a whitespace-valued first pixel is kept.
    if (input.peek() == '\n') input.get();
    const auto pixel_count = static_cast<std::uintmax_t>(width.value()) *
                             static_cast<std::uintmax_t>(height.value());
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 3)
        return general::Result<Image>::failure(
            general::Error::make(std::errc::value_too_large,
                                 "PPM image is too large", "example.segmentation.input"));
    Image image{static_cast<int>(width.value()), static_cast<int>(height.value()),
                std::vector<std::uint8_t>(static_cast<std::size_t>(pixel_count) * 3)};
    input.read(reinterpret_cast<char*>(image.rgb.data()),
               static_cast<std::streamsize>(image.rgb.size()));
    if (!input)
        return general::Result<Image>::failure(
            general::Error::make(std::errc::io_error,
                                 "PPM pixel data is truncated", "example.segmentation.input"));
    return general::Result<Image>::success(std::move(image));
}

general::Result<void> write_pgm(const std::filesystem::path& path,
                                int width, int height,
                                const std::vector<std::uint8_t>& labels) {
    const auto directory = ensure_parent_directory(path);
    if (!directory) return directory;
    std::ofstream output(path, std::ios::binary);
    if (!output) return general::Result<void>::failure(
        general::Error::make(std::errc::io_error, "Cannot create mask output",
                             "example.segmentation.output"));
    output << "P5\n" << width << ' ' << height << "\n255\n";
    output.write(reinterpret_cast<const char*>(labels.data()),
                 static_cast<std::streamsize>(labels.size()));
    if (!output) return general::Result<void>::failure(
        general::Error::make(std::errc::io_error, "Cannot write mask output",
                             "example.segmentation.output"));
    return general::Result<void>::success();
}

general::Result<void> write_overlay(const std::filesystem::path& path, const Image& image,
                                    const std::vector<std::uint8_t>& labels) {
    static constexpr std::array<std::array<std::uint8_t, 3>, 21> palette{{
        {{0, 0, 0}}, {{230, 25, 75}}, {{60, 180, 75}}, {{255, 225, 25}},
        {{0, 130, 200}}, {{245, 130, 48}}, {{145, 30, 180}}, {{70, 240, 240}},
        {{240, 50, 230}}, {{210, 245, 60}}, {{250, 190, 190}}, {{0, 128, 128}},
        {{230, 190, 255}}, {{170, 110, 40}}, {{255, 250, 200}}, {{128, 0, 0}},
        {{170, 255, 195}}, {{128, 128, 0}}, {{255, 215, 180}}, {{0, 0, 128}},
        {{128, 128, 128}}
    }};
    const auto directory = ensure_parent_directory(path);
    if (!directory) return directory;
    std::ofstream output(path, std::ios::binary);
    if (!output) return general::Result<void>::failure(
        general::Error::make(std::errc::io_error, "Cannot create overlay output",
                             "example.segmentation.output"));
    output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const auto& color = palette[labels[i] % palette.size()];
        for (int channel = 0; channel < 3; ++channel)
            output.put(static_cast<char>((image.rgb[i * 3 + channel] + color[channel]) / 2));
    }
    if (!output) return general::Result<void>::failure(
        general::Error::make(std::errc::io_error, "Cannot write overlay output",
                             "example.segmentation.output"));
    return general::Result<void>::success();
}

} // namespace

int main() {
    general::cli::Specification specification;
    specification.settings.program_name = "sindre_example_ai_tensorrt_segmentation";
    specification.settings.description = "Build and run a TensorRT semantic segmentation engine";
    specification.settings.version = "0.1";
    specification.add_option("--onnx", "-m")
        .help("FCN segmentation ONNX model; required when --engine is absent");
    specification.add_option("--input", "-i").required().help("binary P6 PPM input image");
    specification.add_option("--engine", "-e").default_value("segmentation.plan")
        .help("TensorRT engine path");
    specification.add_option("--output", "-o").default_value("segmentation_overlay.ppm")
        .help("color overlay PPM output");
    specification.add_option("--mask").default_value("segmentation_mask.pgm")
        .help("class-index PGM output");
    specification.add_option("--input-name").default_value("input.1")
        .help("TensorRT input tensor name in the ONNX graph");
    specification.add_option("--lean-runtime").help("external TensorRT lean runtime path");
    specification.add_option("--device").default_value("0").help("CUDA device index");
    specification.add_flag("--portable").help("build for Ampere and newer GPUs");
    specification.add_flag("--same-compute-capability")
        .help("build for the current compute capability family");
    specification.add_flag("--fp16").help("enable TensorRT FP16 tactics");
    specification.add_flag("--version-compatible").help("enable TensorRT version compatibility");
    specification.add_flag("--exclude-lean-runtime")
        .help("omit embedded lean runtime; requires --lean-runtime at load time");
    specification.add_flag("--allow-engine-host-code")
        .help("allow host code embedded in a trusted engine");

    const auto parsed = general::cli::parse_current(specification);
    if (!parsed) return report_failure("parse command line", parsed.error());
    if (parsed.value().wants_help() || parsed.value().wants_version()) {
        std::cout << parsed.value().action_text;
        return 0;
    }
    if (parsed.value().is_set("--portable") &&
        parsed.value().is_set("--same-compute-capability")) {
        std::cerr << "--portable and --same-compute-capability are mutually exclusive\n";
        return 2;
    }
    const auto device = parsed.value().get_int("--device");
    if (!device) return report_failure("read --device", device.error());
    if (device.value() < 0 || device.value() > std::numeric_limits<int>::max()) {
        std::cerr << "--device must be a non-negative integer\n";
        return 2;
    }
    const auto image = read_ppm(general::path::from_utf8(parsed.value().get_string("--input")));
    if (!image) return report_failure("read input image", image.error());
    const auto width = static_cast<std::int64_t>(image.value().width);
    const auto height = static_cast<std::int64_t>(image.value().height);
    std::vector<float> input_data(static_cast<std::size_t>(width * height * 3));
    static constexpr std::array<float, 3> mean{{0.485f, 0.456f, 0.406f}};
    static constexpr std::array<float, 3> standard_deviation{{0.229f, 0.224f, 0.225f}};
    for (std::int64_t y = 0; y < height; ++y) {
        for (std::int64_t x = 0; x < width; ++x) {
            const auto source = static_cast<std::size_t>((y * width + x) * 3);
            for (int channel = 0; channel < 3; ++channel) {
                const auto normalized = static_cast<float>(image.value().rgb[source + channel]) /
                                        255.0f;
                input_data[static_cast<std::size_t>(channel * width * height + y * width + x)] =
                    (normalized - mean[channel]) / standard_deviation[channel];
            }
        }
    }

    const auto engine_path = general::path::from_utf8(parsed.value().get_string("--engine"));
    if (!std::filesystem::exists(engine_path)) {
        if (!parsed.value().has("--onnx")) {
            std::cerr << "--onnx is required when the engine does not exist\n";
            return 2;
        }
        const auto directory = ensure_parent_directory(engine_path);
        if (!directory) return report_failure("prepare engine directory", directory.error());
        ai::trt::BuildOptions options;
        options.device_id = static_cast<int>(device.value());
        options.fp16 = parsed.value().is_set("--fp16");
        options.version_compatible = parsed.value().is_set("--version-compatible");
        options.exclude_lean_runtime = parsed.value().is_set("--exclude-lean-runtime");
        options.compatibility = parsed.value().is_set("--portable")
            ? ai::trt::Compatibility::ampere_plus
            : ai::trt::Compatibility::same_compute_capability;
        options.profiles.push_back({parsed.value().get_string("--input-name"),
                                    {1, 3, height, width}, {1, 3, height, width},
                                    {1, 3, height, width}});
        const auto built = ai::trt::try_convert_onnx(
            general::path::from_utf8(parsed.value().get_string("--onnx")), engine_path, options);
        if (!built) return report_failure("build TensorRT engine", built.error());
    }

    ai::trt::LoadOptions load_options;
    load_options.device_id = static_cast<int>(device.value());
    load_options.allow_engine_host_code = parsed.value().is_set("--allow-engine-host-code");
    if (parsed.value().is_set("--exclude-lean-runtime") &&
        !parsed.value().has("--lean-runtime")) {
        std::cerr << "--exclude-lean-runtime requires --lean-runtime when loading the engine\n";
        return 2;
    }
    if (parsed.value().has("--lean-runtime"))
        load_options.lean_runtime_path = general::path::from_utf8(
            parsed.value().get_string("--lean-runtime"));
    const auto model = ai::trt::Model::try_create(engine_path, load_options);
    if (!model) return report_failure("load TensorRT engine", model.error());

    ai::Tensor input_tensor{{1, 3, height, width}, std::move(input_data)};
    ai::Tensors inputs;
    inputs.push_back(std::move(input_tensor));
    const auto started = std::chrono::steady_clock::now();
    const auto outputs = model.value()->try_infer(inputs);
    if (!outputs) return report_failure("run segmentation", outputs.error());
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();

    const auto& output_infos = model.value()->get_outputs();
    std::size_t output_index = output_infos.size();
    for (std::size_t i = 0; i < output_infos.size(); ++i)
        if (output_infos[i].name == "out") output_index = i;
    if (output_index == output_infos.size()) {
        if (outputs.value().size() != 1) {
            std::cerr << "model must expose an output named 'out' or a single output\n";
            return 2;
        }
        output_index = 0;
    }
    if (output_index >= outputs.value().size() ||
        outputs.value()[output_index].shape.size() != 4 ||
        outputs.value()[output_index].shape[0] != 1 ||
        outputs.value()[output_index].shape[2] != height ||
        outputs.value()[output_index].shape[3] != width) {
        std::cerr << "segmentation output must have shape [1, classes, height, width]\n";
        return 2;
    }
    const auto classes = outputs.value()[output_index].shape[1];
    const auto& scores = outputs.value()[output_index].data;
    if (classes <= 0 || static_cast<std::uintmax_t>(classes) *
        static_cast<std::uintmax_t>(height) * static_cast<std::uintmax_t>(width) != scores.size()) {
        std::cerr << "segmentation output has an invalid element count\n";
        return 2;
    }
    std::vector<std::uint8_t> labels(static_cast<std::size_t>(width * height));
    for (std::int64_t y = 0; y < height; ++y) {
        for (std::int64_t x = 0; x < width; ++x) {
            std::int64_t best = 0;
            float best_score = -std::numeric_limits<float>::infinity();
            for (std::int64_t c = 0; c < classes; ++c) {
                const auto index = static_cast<std::size_t>(c * height * width + y * width + x);
                if (scores[index] > best_score) { best_score = scores[index]; best = c; }
            }
            labels[static_cast<std::size_t>(y * width + x)] =
                static_cast<std::uint8_t>(std::min<std::int64_t>(best, 255));
        }
    }
    const auto mask = write_pgm(general::path::from_utf8(parsed.value().get_string("--mask")),
                                image.value().width, image.value().height, labels);
    if (!mask) return report_failure("write mask", mask.error());
    const auto overlay = write_overlay(
        general::path::from_utf8(parsed.value().get_string("--output")), image.value(), labels);
    if (!overlay) return report_failure("write overlay", overlay.error());
    std::cout << "segmented " << image.value().width << 'x' << image.value().height
              << " in " << elapsed << " ms\n";
    return 0;
}
