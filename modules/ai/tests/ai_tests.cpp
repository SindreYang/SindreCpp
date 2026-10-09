#include <sindre/ai.h>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

int main(int argc, char** argv) {
    using namespace sindre::ai;
    using namespace sindre::ai::onnxruntime;
    if (argc != 2) return 1;
    try {
        Tensor reshape_tensor{{2, 3}, {1.f, 2.f, 3.f, 4.f, 5.f, 6.f}};
        if (!reshape_tensor.try_reshape({3, 2}) ||
            reshape_tensor.shape != std::vector<std::int64_t>{3, 2}) return 24;
        if (!reshape_tensor.try_reshape({-1, 2}) ||
            reshape_tensor.shape != std::vector<std::int64_t>{3, 2}) return 29;
        if (reshape_tensor.try_reshape({4, 2})) return 25;
        const auto typed_double = TypedTensor::try_from<double>(
            {2, 2}, std::vector<double>{1.0, 2.0, 3.0, 4.0});
        if (!typed_double || typed_double.value().type != DataType::float64 ||
            !typed_double.value().try_validate()) return 26;
        const auto typed_int16 = TypedTensor::try_from<std::int16_t>(
            {2}, std::vector<std::int16_t>{-2, 7});
        if (!typed_int16 || typed_int16.value().type != DataType::int16 ||
            typed_int16.value().data.size() != sizeof(std::int16_t) * 2) return 27;
        auto typed_uint16 = TypedTensor::try_from<std::uint16_t>(
            {2, 2}, std::vector<std::uint16_t>{1, 2, 3, 4}, DataType::uint16);
        if (!typed_uint16 || !typed_uint16.value().try_reshape({4}) ||
            typed_uint16.value().type != DataType::uint16) return 28;

        Options options;
        options.backend = Backend::cpu;
        options.cpu_threads = 1;
        Model default_model(argv[1]);
        if (default_model.get_backend() != Backend::cpu) return 16;
        Model model(argv[1], options);
        auto providers_result = try_get_available_backends();
        if (!providers_result) return 30;
        auto safe_missing = Model::try_create("not-a-model.onnx", options);
        if (safe_missing || safe_missing.error().context != "ai.onnxruntime.create") return 31;
        auto safe_invalid = model.try_infer({Tensor{{1, 4}, std::vector<float>(4)}});
        if (safe_invalid || safe_invalid.error().code !=
                                std::make_error_code(std::errc::invalid_argument) ||
            safe_invalid.error().context != "ai.onnxruntime.infer") return 32;
        ::sindre::general::TaskOptions expired_options;
        expired_options.deadline = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
        auto expired_async = model.try_infer_async(
            {Tensor{{2, 3}, std::vector<float>(6, 1.f)}}, expired_options);
        if (!expired_async) return 33;
        auto expired_result = expired_async.value().get();
        if (expired_result || expired_result.error().code !=
                                  std::make_error_code(std::errc::timed_out)) return 34;
        const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto chinese_model = std::filesystem::temp_directory_path() /
            ("sindre-模型-中文-" + std::to_string(unique_id) + ".onnx");
        std::filesystem::copy_file(argv[1], chinese_model,
                                   std::filesystem::copy_options::overwrite_existing);
        Model chinese_path_model(chinese_model, options);
        if (chinese_path_model.get_inputs().size() != model.get_inputs().size() ||
            chinese_path_model.get_outputs().size() != model.get_outputs().size()) return 15;
        std::filesystem::remove(chinese_model);
        if (model.get_inputs().size() != 1 || model.get_outputs().size() != 1) return 2;
        for (int size : {1, 4, 8}) {
            Tensor input{{size, 3}, std::vector<float>(size * 3, 2.f)};
            auto output = model.infer({input});
            if (output.size() != 1 || output[0].shape != input.shape || output[0].data != input.data) return 3;
            std::vector<Tensor> buffers{{input.shape, std::vector<float>(input.data.size())}};
            model.infer_into({input}, buffers);
            if (buffers[0].data != input.data) return 4;
            model.warm_up({input}, 2);
        }
        auto typed_input = TypedTensor::from<float>({2, 3}, std::vector<float>(6, 6.f));
        auto typed_output = model.infer_typed({typed_input});
        if (typed_output.size() != 1 || typed_output[0].type != DataType::float32 ||
            typed_output[0].shape != typed_input.shape || typed_output[0].data.size() != 6 * sizeof(float))
            return 19;
        std::vector<float> typed_values(6);
        std::memcpy(typed_values.data(), typed_output[0].data.data(), typed_output[0].data.size());
        if (typed_values[0] != 6.f) return 20;
        TypedTensors typed_buffers{
            TypedTensor::from<float>({2, 3}, std::vector<float>(6, 0.f))};
        model.infer_typed_into({typed_input}, typed_buffers);
        std::vector<float> typed_buffer_values(6);
        std::memcpy(typed_buffer_values.data(), typed_buffers[0].data.data(),
                    typed_buffers[0].data.size());
        if (typed_buffer_values[0] != 6.f) return 22;
        bool rejected = false;
        try {
            (void)TypedTensor::from_bytes({2, 3}, DataType::float32, nullptr, 1024);
        } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) return 21;
        try { (void)model.infer({Tensor{{2, 3}, {1.f}}}); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) return 5;
        rejected = false;
        try { (void)model.infer({Tensor{{1, 4}, std::vector<float>(4)}}); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) return 6;
        rejected = false;
        try { (void)get_element_count({-1, 3}); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected || get_element_count({}) != 1 || get_element_count({0, 3}) != 0) return 7;
        rejected = false;
        try { (void)get_element_count({0, -1}); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) return 18;
        rejected = false;
        try { (void)get_element_count({std::numeric_limits<std::int64_t>::max(), 3}); }
        catch (const std::overflow_error&) { rejected = true; }
        if (!rejected) return 17;
        rejected = false;
        try { Model missing("not-a-model.onnx", options); }
        catch (const std::exception&) { rejected = true; }
        if (!rejected) return 8;
        const auto providers = get_available_backends();
        Options gpu_options = options;
        gpu_options.backend = Backend::cuda;
#if defined(SINDRE_AI_CUDA)
        if (std::find(providers.begin(), providers.end(), "CUDAExecutionProvider") == providers.end()) {
            rejected = false;
            try { Model gpu(argv[1], gpu_options); }
            catch (const std::runtime_error&) { rejected = true; }
            if (!rejected) return 11;
        } else {
            Model gpu(argv[1], gpu_options);
            auto gpu_output = gpu.infer({Tensor{{2, 3}, std::vector<float>(6, 9.f)}});
            if (gpu_output.size() != 1 || gpu_output[0].data != std::vector<float>(6, 9.f))
                return 23;
        }
#else
        rejected = false;
        try { Model gpu(argv[1], gpu_options); }
        catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) return 11;
#endif
        auto first = model.infer_async({Tensor{{2, 3}, std::vector<float>(6, 4.f)}});
        auto second = model.infer_async({Tensor{{4, 3}, std::vector<float>(12, 5.f)}});
        if (first.get()[0].data[0] != 4.f || second.get()[0].data[0] != 5.f) return 12;
        auto failure = model.infer_async({Tensor{{1, 4}, std::vector<float>(4)}});
        rejected = false;
        try { (void)failure.get(); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) return 13;
        auto shared = std::make_shared<Model>(argv[1], options);
        Pipeline<int, Tensors, Tensors> pipeline(
            [](int size) { return Tensors{Tensor{{size, 3}, std::vector<float>(size * 3, 7.f)}}; },
            [shared](Tensors tensors) { return shared->infer(tensors); });
        if (pipeline.infer_async(2).get()[0].data[0] != 7.f) return 14;
        pipeline.close();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 9;
    }
}
