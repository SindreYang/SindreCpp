#include <ai/trt.hpp>
#include <chrono>
#include <iostream>

int main(int argc, char** argv) {
    using namespace sindrecpp::ai;
    namespace trt = sindrecpp::ai::trt;
    if (argc != 2) return 1;
    try {
        const auto source = std::filesystem::temp_directory_path() / "sindrecpp-模型-中文.onnx";
        const auto destination = std::filesystem::temp_directory_path() /
            ("sindrecpp-引擎-中文-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".engine");
        std::filesystem::copy_file(argv[1], source,
                                   std::filesystem::copy_options::overwrite_existing);
        auto build = trt::BuildOptions::max_performance();
        build.profiles = {{"input", {1, 3}, {4, 3}, {8, 3}}};
        trt::convert_onnx(source, destination, build);
        trt::Model model(destination);
        Tensors inputs{Tensor{{4, 3}, std::vector<float>(12, 3.f)}};
        if (model.infer(inputs)[0].data != inputs[0].data) return 2;
        if (model.infer_async(inputs).get()[0].data != inputs[0].data) return 3;
        auto pending = model.enqueue(inputs);
        if (pending.get()[0].data != inputs[0].data) return 4;
        bool rejected = false;
        try { (void)model.infer({Tensor{{9, 3}, std::vector<float>(27)}}); }
        catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) return 5;
        if (model.infer(inputs)[0].data != inputs[0].data) return 6;
        std::filesystem::remove(source);
        std::cout << destination << '\n'; // Retain unique engine for diagnostics.
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 9;
    }
}
