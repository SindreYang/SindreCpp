#include <sindrecpp/ai.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    using namespace sindrecpp::ai;
    using namespace sindrecpp::ai::onnxruntime;
    if (argc != 2) return 1;
    try {
        Options options;
        options.backend = Backend::cpu;
        options.cpu_threads = 1;
        Model model(argv[1], options);
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
        bool rejected = false;
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
        if (!rejected || get_element_count({}) != 1) return 7;
        rejected = false;
        try { Model missing("not-a-model.onnx", options); }
        catch (const Ort::Exception&) { rejected = true; }
        if (!rejected) return 8;
        const auto providers = get_available_backends();
        if (std::find(providers.begin(), providers.end(), "CUDAExecutionProvider") == providers.end()) {
            rejected = false;
            try { Model gpu(argv[1]); }
            catch (const std::runtime_error&) { rejected = true; }
            if (!rejected) return 11;
        }
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
