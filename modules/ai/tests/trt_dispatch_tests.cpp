#include <sindre/ai/trt.h>

#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: trt_dispatch_tests <engine> <lean-runtime>\n";
        return 2;
    }

    sindre::ai::trt::LoadOptions options;
    options.lean_runtime_path = argv[2];
    const auto model = sindre::ai::trt::Model::try_create(argv[1], options);
    if (!model || !model.value()) {
        if (!model) std::cerr << model.error().message << '\n';
        return 3;
    }

    const sindre::ai::TypedTensors inputs{
        sindre::ai::TypedTensor::from<float>(
            {4, 3}, std::vector<float>(12, 3.0f))};
    const auto output = model.value()->try_infer_typed(inputs);
    if (!output || output.value().size() != 1 ||
        output.value()[0].type != sindre::ai::DataType::float32 ||
        output.value()[0].data.size() != 12 * sizeof(float)) {
        if (!output) std::cerr << output.error().message << '\n';
        return 4;
    }
    return 0;
}
