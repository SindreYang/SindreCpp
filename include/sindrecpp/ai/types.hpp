#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace sindrecpp::ai {

enum class DataType { float32, float16, int32, int64, bool8, other };

inline std::size_t get_element_count(const std::vector<std::int64_t>& shape) {
    std::size_t count = 1;
    for (auto dimension : shape) {
        if (dimension <= 0) throw std::invalid_argument("Concrete tensor dimensions must be positive");
        const auto size = static_cast<std::size_t>(dimension);
        if (count > std::numeric_limits<std::size_t>::max() / size)
            throw std::overflow_error("Tensor shape is too large");
        count *= size;
    }
    return count;
}

struct Tensor {
    std::vector<std::int64_t> shape;
    std::vector<float> data;
    void validate() const {
        if (get_element_count(shape) != data.size())
            throw std::invalid_argument("Tensor data size does not match shape");
    }
};

struct TensorInfo {
    std::string name;
    std::vector<std::int64_t> shape; // -1: dynamic dimension.
    DataType type;
};
using Tensors = std::vector<Tensor>;

} // namespace sindrecpp::ai
