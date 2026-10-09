#include <sindre/ai/trt.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    using namespace sindre::ai;
    namespace trt = sindre::ai::trt;
    if (argc != 2) return 1;
    try {
        const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto source = std::filesystem::temp_directory_path() /
            ("sindre-模型-中文-" + std::to_string(unique_id) + ".onnx");
        std::string requested_engine;
#if defined(_WIN32)
        char* requested_engine_buffer = nullptr;
        std::size_t requested_engine_size = 0;
        if (_dupenv_s(&requested_engine_buffer, &requested_engine_size,
                      "SINDRE_TRT_TEST_ENGINE_OUTPUT") == 0 &&
            requested_engine_buffer != nullptr) {
            requested_engine.assign(requested_engine_buffer, requested_engine_size - 1);
            std::free(requested_engine_buffer);
        }
#else
        if (const char* requested_engine_value =
                std::getenv("SINDRE_TRT_TEST_ENGINE_OUTPUT")) {
            requested_engine = requested_engine_value;
        }
#endif
        const auto destination = !requested_engine.empty()
            ? std::filesystem::path(requested_engine)
            : std::filesystem::temp_directory_path() /
                  ("sindre-引擎-中文-" +
                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                   ".engine");
        std::filesystem::remove(destination);
        std::filesystem::copy_file(argv[1], source,
                                   std::filesystem::copy_options::overwrite_existing);
        auto build = trt::BuildOptions::max_performance();
        build.profiles = {{"input", {1, 3}, {4, 3}, {8, 3}}};
        // Keep the test engine usable by the DISPATCH runtime as well. The
        // external lean runtime is intentionally not embedded in the plan.
        build.version_compatible = true;
        build.exclude_lean_runtime = true;
        trt::convert_onnx(source, destination, build);
        trt::Model model(destination);
        auto safe_model = trt::Model::try_create(destination);
        if (!safe_model || !safe_model.value()) return 10;
        Tensors inputs{Tensor{{4, 3}, std::vector<float>(12, 3.f)}};
        if (model.infer(inputs)[0].data != inputs[0].data) return 2;
        if (model.infer_async(inputs).get()[0].data != inputs[0].data) return 3;
        auto pending = model.enqueue(inputs);
        if (pending.get()[0].data != inputs[0].data) return 4;
        auto typed_output = model.infer_typed({TypedTensor::from<float>(
            {4, 3}, std::vector<float>(12, 3.f))});
        if (typed_output.size() != 1 || typed_output[0].type != DataType::float32 ||
            typed_output[0].data.size() != 12 * sizeof(float)) return 7;
        if (model.infer_typed_async({TypedTensor::from<float>(
                {4, 3}, std::vector<float>(12, 3.f))}).get()[0].data.size() !=
            12 * sizeof(float)) return 8;
        auto safe_output = model.try_infer_typed({TypedTensor::from<float>(
            {4, 3}, std::vector<float>(12, 3.f))});
        if (!safe_output || safe_output.value().size() != 1) return 11;
        auto safe_async = model.try_infer_typed_async(
            {TypedTensor::from<float>({4, 3}, std::vector<float>(12, 3.f))});
        if (!safe_async) return 12;
        auto safe_async_result = safe_async.value().get();
        if (!safe_async_result || safe_async_result.value().size() != 1) return 12;
        auto typed_pending = model.enqueue(inputs);
        if (typed_pending.get_typed()[0].data.size() != 12 * sizeof(float)) return 9;
        auto safe_pending = model.try_enqueue_typed({TypedTensor::from<float>(
            {4, 3}, std::vector<float>(12, 3.f))});
        if (!safe_pending) return 13;
        auto safe_pending_output = safe_pending.value().try_get_typed();
        if (!safe_pending_output || safe_pending_output.value().size() != 1) return 14;
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
