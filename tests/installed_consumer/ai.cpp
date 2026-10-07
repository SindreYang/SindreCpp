#include <sindre/ai.h>

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    using namespace sindre::ai;
    using namespace sindre::ai::onnxruntime;

    auto model = Model::try_create(argv[1]);
    if (!model) {
        std::cerr << model.error().message << '\n';
        return 2;
    }
    const Tensor input{{1, 3}, {2.0F, 3.0F, 4.0F}};
    auto output = model.value()->try_infer({input});
    if (!output || output.value().size() != 1 || output.value()[0].data != input.data)
        return 3;
    return 0;
}
