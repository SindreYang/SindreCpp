#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <general/core/async.hpp>

namespace sindrecpp::ai {

namespace detail {

template <class Function>
using guarded_result_t = ::sindrecpp::general::Result<std::invoke_result_t<Function>>;

template <class Function>
guarded_result_t<Function> guarded(Function &&function, std::string_view context) noexcept {
    using Return = std::invoke_result_t<Function>;
#if defined(SINDRECPP_NO_EXCEPTIONS)
    if constexpr (std::is_void_v<Return>) {
        std::invoke(std::forward<Function>(function));
        return ::sindrecpp::general::Result<void>::success();
    } else {
        return ::sindrecpp::general::Result<Return>::success(
            std::invoke(std::forward<Function>(function)));
    }
#else
    try {
        if constexpr (std::is_void_v<Return>) {
            std::invoke(std::forward<Function>(function));
            return ::sindrecpp::general::Result<void>::success();
        } else {
            return ::sindrecpp::general::Result<Return>::success(
                std::invoke(std::forward<Function>(function)));
        }
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<Return>::failure(
            std::make_error_code(std::errc::io_error), error.what(), std::string(context));
    } catch (...) {
        return ::sindrecpp::general::Result<Return>::failure(
            std::make_error_code(std::errc::io_error), "Unknown AI operation failure",
            std::string(context));
    }
#endif
}

} // namespace detail

enum class DataType { float32, float16, int32, int64, bool8, other };

inline std::size_t get_element_count(const std::vector<std::int64_t>& shape) {
    std::size_t count = 1;
    for (auto dimension : shape) {
        if (dimension < 0) throw std::invalid_argument("Concrete tensor dimensions must be non-negative");
        if (static_cast<std::uintmax_t>(dimension) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
            throw std::overflow_error("Tensor dimension is too large");
        const auto size = static_cast<std::size_t>(dimension);
        if (size == 0) {
            count = 0;
            continue;
        }
        if (count == 0) continue;
        if (count > std::numeric_limits<std::size_t>::max() / size)
            throw std::overflow_error("Tensor shape is too large");
        count *= size;
    }
    return count;
}

inline std::size_t data_type_size(DataType type) {
    switch (type) {
        case DataType::float32: return sizeof(float);
        case DataType::float16: return sizeof(std::uint16_t);
        case DataType::int32: return sizeof(std::int32_t);
        case DataType::int64: return sizeof(std::int64_t);
        case DataType::bool8: return sizeof(std::uint8_t);
        default: throw std::invalid_argument("Unsupported tensor data type");
    }
}

// Owning tensor storage for models whose I/O is not float32. The legacy
// Tensor type below remains the convenient float32 API.
struct TypedTensor {
    std::vector<std::int64_t> shape;
    DataType type = DataType::other;
    std::vector<std::uint8_t> data;

    static std::size_t expected_byte_count(const std::vector<std::int64_t>& shape,
                                           DataType type) {
        const auto elements = get_element_count(shape);
        const auto bytes_per_element = data_type_size(type);
        if (elements > std::numeric_limits<std::size_t>::max() / bytes_per_element)
            throw std::overflow_error("Typed tensor is too large");
        return elements * bytes_per_element;
    }

    void validate() const {
        if (data.size() != expected_byte_count(shape, type))
            throw std::invalid_argument("Typed tensor data size does not match shape");
    }

    static TypedTensor from_bytes(std::vector<std::int64_t> shape, DataType type,
                                  const void* source, std::size_t byte_count) {
        TypedTensor result{std::move(shape), type, {}};
        if (byte_count && !source) throw std::invalid_argument("Typed tensor data is null");
        if (byte_count != expected_byte_count(result.shape, result.type))
            throw std::invalid_argument("Typed tensor data size does not match shape");
        result.data.resize(byte_count);
        if (byte_count) std::memcpy(result.data.data(), source, byte_count);
        result.validate();
        return result;
    }

    template <class T>
    static TypedTensor from(std::vector<std::int64_t> shape, const std::vector<T>& values) {
        DataType type;
        if constexpr (std::is_same_v<T, float>) type = DataType::float32;
        else if constexpr (std::is_same_v<T, std::int32_t>) type = DataType::int32;
        else if constexpr (std::is_same_v<T, std::int64_t>) type = DataType::int64;
        else if constexpr (std::is_same_v<T, std::uint16_t>) type = DataType::float16;
        else if constexpr (std::is_same_v<T, std::uint8_t>) type = DataType::bool8;
        else static_assert(std::is_same_v<T, void>, "Unsupported TypedTensor element type");
        return from_bytes(std::move(shape), type, values.data(), values.size() * sizeof(T));
    }
};
using TypedTensors = std::vector<TypedTensor>;

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
