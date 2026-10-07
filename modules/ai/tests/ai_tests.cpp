#include <sindre/ai.h>
#include <cmath>
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
        Options options;
        options.backend = Backend::cpu;
        options.cpu_threads = 1;
        Model default_model(argv[1]);
        if (default_model.get_backend() != Backend::cpu) return 16;
        Model model(argv[1], options);
        const auto chinese_model = std::filesystem::temp_directory_path() / "sindre-模型-中文.onnx";
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
