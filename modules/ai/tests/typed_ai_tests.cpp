#include <sindre/ai/onnxruntime.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc != 3) return 1;
    try {
        using namespace sindre::ai;
        using namespace sindre::ai::onnxruntime;
        Model model(argv[1]);
        const std::string kind = argv[2];
        TypedTensor input;
        if (kind == "float16")
            input = TypedTensor::from<std::uint16_t>({1, 3}, {0x3c00, 0xc000, 0x7bff});
        else if (kind == "int32")
            input = TypedTensor::from<std::int32_t>({1, 3}, {1, -2, 300000});
        else if (kind == "int64")
            input = TypedTensor::from<std::int64_t>({1, 3}, {1, -20000000000LL, 30000000000LL});
        else if (kind == "bool")
            input = TypedTensor::from<std::uint8_t>({1, 3}, {1, 0, 1});
        else
            return 5;
        const auto output = model.infer_typed({input});
        const auto expected_type = kind == "float16" ? DataType::float16
                                  : kind == "int32" ? DataType::int32
                                  : kind == "int64" ? DataType::int64
                                                      : DataType::bool8;
        if (output.size() != 1 || output[0].type != expected_type ||
            output[0].shape != input.shape || output[0].data.size() != input.data.size())
            return 2;
        if (output[0].data != input.data) return 3;
        std::cout << "Typed " << kind << " inference passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 4;
    }
}
