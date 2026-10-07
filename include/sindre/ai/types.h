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
#include <sindre/general/runtime.h>

namespace sindre::ai {

namespace detail {

template <class Function>
using guarded_result_t = ::sindre::general::Result<std::invoke_result_t<Function>>;

template <class Function>
guarded_result_t<Function> guarded(Function &&function, std::string_view context) noexcept {
    using Return = std::invoke_result_t<Function>;
#if defined(SINDRE_NO_EXCEPTIONS)
    if constexpr (std::is_void_v<Return>) {
        std::invoke(std::forward<Function>(function));
        return ::sindre::general::Result<void>::success();
    } else {
        return ::sindre::general::Result<Return>::success(
            std::invoke(std::forward<Function>(function)));
    }
#else
    try {
        if constexpr (std::is_void_v<Return>) {
            std::invoke(std::forward<Function>(function));
            return ::sindre::general::Result<void>::success();
        } else {
            return ::sindre::general::Result<Return>::success(
                std::invoke(std::forward<Function>(function)));
        }
    } catch (const std::exception &error) {
        return ::sindre::general::Result<Return>::failure(
            std::make_error_code(std::errc::io_error), error.what(), std::string(context));
    } catch (...) {
        return ::sindre::general::Result<Return>::failure(
            std::make_error_code(std::errc::io_error), "Unknown AI operation failure",
            std::string(context));
    }
#endif
}

} // namespace detail

/// @brief sindrecpp 支持的张量元素类型。
enum class DataType { float32, float16, int32, int64, bool8, other };

/// @brief 计算具体形状对应的元素数量。
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

/// @brief 返回一个元素的字节数。
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

/// @brief 非 float32 模型 I/O 的拥有型字节存储。
///
/// `data` 的长度必须与 shape 和 type 一致；错误输入抛出标准异常。
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

/// @brief float32 模型 I/O 的拥有型张量。
struct Tensor {
    std::vector<std::int64_t> shape;
    std::vector<float> data;
    void validate() const {
        if (get_element_count(shape) != data.size())
            throw std::invalid_argument("Tensor data size does not match shape");
    }
};

/// @brief 模型输入或输出的元数据。
struct TensorInfo {
    std::string name;
    std::vector<std::int64_t> shape; // -1: dynamic dimension.
    DataType type;
};
using Tensors = std::vector<Tensor>;

} // namespace sindre::ai
