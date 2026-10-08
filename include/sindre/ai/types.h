#pragma once

/// @file
/// @brief AI 张量类型、布局和 Result 校验辅助接口。

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>
#include <sindre/general/runtime.h>

namespace sindre::ai {

namespace detail {

inline std::error_code exception_code(const std::exception &error) noexcept {
    if (const auto *system = dynamic_cast<const std::system_error *>(&error)) {
        return system->code();
    }
    if (dynamic_cast<const std::invalid_argument *>(&error)) {
        return std::make_error_code(std::errc::invalid_argument);
    }
    if (dynamic_cast<const std::out_of_range *>(&error)) {
        return std::make_error_code(std::errc::result_out_of_range);
    }
    if (dynamic_cast<const std::overflow_error *>(&error)) {
        return std::make_error_code(std::errc::value_too_large);
    }
    if (dynamic_cast<const std::bad_alloc *>(&error)) {
        return std::make_error_code(std::errc::not_enough_memory);
    }
    return std::make_error_code(std::errc::io_error);
}

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
            exception_code(error), error.what(), std::string(context));
    } catch (...) {
        return ::sindre::general::Result<Return>::failure(
            std::make_error_code(std::errc::io_error), "Unknown AI operation failure",
            std::string(context));
    }
#endif
}

} // namespace detail

/// @brief sindre 支持的张量元素类型。
enum class DataType {
    float32,
    float16,
    bfloat16,
    float64,
    int8,
    int16,
    int32,
    int64,
    uint8,
    uint16,
    uint32,
    uint64,
    bool8,
    other
};

/// @brief 计算形状对应的元素数量，并把输入错误转换为 Result。
inline ::sindre::general::Result<std::size_t>
try_get_element_count(const std::vector<std::int64_t> &shape) noexcept {
    std::size_t count = 1;
    for (const auto dimension : shape) {
        if (dimension < 0) {
            return ::sindre::general::Result<std::size_t>::failure(
                ::sindre::general::Error::make(
                    std::errc::invalid_argument,
                    "Tensor dimensions must be non-negative",
                    "ai.tensor.shape"));
        }
        if (static_cast<std::uintmax_t>(dimension) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
            return ::sindre::general::Result<std::size_t>::failure(
                ::sindre::general::Error::make(
                    std::errc::value_too_large,
                    "Tensor dimension is too large",
                    "ai.tensor.shape"));
        }
        const auto size = static_cast<std::size_t>(dimension);
        if (size == 0) {
            count = 0;
            continue;
        }
        if (count == 0) continue;
        if (count > std::numeric_limits<std::size_t>::max() / size) {
            return ::sindre::general::Result<std::size_t>::failure(
                ::sindre::general::Error::make(
                    std::errc::value_too_large,
                    "Tensor shape is too large",
                    "ai.tensor.shape"));
        }
        count *= size;
    }
    return ::sindre::general::Result<std::size_t>::success(count);
}

/// @brief 解析 reshape 的目标 shape；允许最多一个 -1 自动推导。
inline ::sindre::general::Result<std::vector<std::int64_t>>
try_resolve_reshape(const std::vector<std::int64_t> &requested_shape,
                    std::size_t element_count) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
    auto shape = requested_shape;
    std::size_t known = 1;
    std::size_t unknown_index = shape.size();
    for (std::size_t index = 0; index < shape.size(); ++index) {
        const auto dimension = shape[index];
        if (dimension == -1) {
            if (unknown_index != shape.size()) {
                return ::sindre::general::Result<std::vector<std::int64_t>>::failure(
                    ::sindre::general::Error::make(
                        std::errc::invalid_argument,
                        "Reshape accepts at most one inferred dimension",
                        "ai.tensor.reshape"));
            }
            unknown_index = index;
            continue;
        }
        if (dimension < 0) {
            return ::sindre::general::Result<std::vector<std::int64_t>>::failure(
                ::sindre::general::Error::make(
                    std::errc::invalid_argument,
                    "Reshape dimensions must be non-negative or -1",
                    "ai.tensor.reshape"));
        }
        if (static_cast<std::uintmax_t>(dimension) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
            return ::sindre::general::Result<std::vector<std::int64_t>>::failure(
                ::sindre::general::Error::make(
                    std::errc::value_too_large,
                    "Reshape dimension is too large",
                    "ai.tensor.reshape"));
        }
        const auto size = static_cast<std::size_t>(dimension);
        if (size != 0 && known > std::numeric_limits<std::size_t>::max() / size) {
            return ::sindre::general::Result<std::vector<std::int64_t>>::failure(
                ::sindre::general::Error::make(
                    std::errc::value_too_large,
                    "Reshape shape is too large",
                    "ai.tensor.reshape"));
        }
        known *= size;
    }

    if (unknown_index != shape.size()) {
        if (known == 0) {
            if (element_count != 0) {
                return ::sindre::general::Result<std::vector<std::int64_t>>::failure(
                    ::sindre::general::Error::make(
                        std::errc::invalid_argument,
                        "Cannot infer reshape dimension from a zero-sized shape",
                        "ai.tensor.reshape"));
            }
            shape[unknown_index] = 0;
        } else {
            if (element_count % known != 0 ||
                element_count / known >
                    static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
                return ::sindre::general::Result<std::vector<std::int64_t>>::failure(
                    ::sindre::general::Error::make(
                        std::errc::invalid_argument,
                        "Cannot infer reshape dimension from element count",
                        "ai.tensor.reshape"));
            }
            shape[unknown_index] = static_cast<std::int64_t>(element_count / known);
        }
    } else if (known != element_count) {
        return ::sindre::general::Result<std::vector<std::int64_t>>::failure(
            ::sindre::general::Error::make(
                std::errc::invalid_argument,
                "Reshaped tensor must preserve the element count",
                "ai.tensor.reshape"));
    }
    return ::sindre::general::Result<std::vector<std::int64_t>>::success(std::move(shape));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindre::general::Result<std::vector<std::int64_t>>::failure(
            ::sindre::general::Error::make(
                std::errc::not_enough_memory, error.what(), "ai.tensor.reshape"));
    } catch (...) {
        return ::sindre::general::Result<std::vector<std::int64_t>>::failure(
            ::sindre::general::Error::make(
                std::errc::not_enough_memory,
                "Unable to allocate reshape shape",
                "ai.tensor.reshape"));
    }
#endif
}

/// @brief 返回元素类型的字节数，并把不支持的类型转换为 Result。
inline ::sindre::general::Result<std::size_t>
try_data_type_size(DataType type) noexcept {
    switch (type) {
        case DataType::float32: return ::sindre::general::Result<std::size_t>::success(sizeof(float));
        case DataType::float16: return ::sindre::general::Result<std::size_t>::success(sizeof(std::uint16_t));
        case DataType::bfloat16: return ::sindre::general::Result<std::size_t>::success(sizeof(std::uint16_t));
        case DataType::float64: return ::sindre::general::Result<std::size_t>::success(sizeof(double));
        case DataType::int8: return ::sindre::general::Result<std::size_t>::success(sizeof(std::int8_t));
        case DataType::int16: return ::sindre::general::Result<std::size_t>::success(sizeof(std::int16_t));
        case DataType::int32: return ::sindre::general::Result<std::size_t>::success(sizeof(std::int32_t));
        case DataType::int64: return ::sindre::general::Result<std::size_t>::success(sizeof(std::int64_t));
        case DataType::uint8: return ::sindre::general::Result<std::size_t>::success(sizeof(std::uint8_t));
        case DataType::uint16: return ::sindre::general::Result<std::size_t>::success(sizeof(std::uint16_t));
        case DataType::uint32: return ::sindre::general::Result<std::size_t>::success(sizeof(std::uint32_t));
        case DataType::uint64: return ::sindre::general::Result<std::size_t>::success(sizeof(std::uint64_t));
        case DataType::bool8: return ::sindre::general::Result<std::size_t>::success(sizeof(std::uint8_t));
        default:
            return ::sindre::general::Result<std::size_t>::failure(
                ::sindre::general::Error::make(
                    std::errc::invalid_argument,
                    "Unsupported tensor data type",
                    "ai.tensor.data_type"));
    }
}

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
        case DataType::bfloat16: return sizeof(std::uint16_t);
        case DataType::float64: return sizeof(double);
        case DataType::int8: return sizeof(std::int8_t);
        case DataType::int16: return sizeof(std::int16_t);
        case DataType::int32: return sizeof(std::int32_t);
        case DataType::int64: return sizeof(std::int64_t);
        case DataType::uint8: return sizeof(std::uint8_t);
        case DataType::uint16: return sizeof(std::uint16_t);
        case DataType::uint32: return sizeof(std::uint32_t);
        case DataType::uint64: return sizeof(std::uint64_t);
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

    static ::sindre::general::Result<std::size_t>
    try_expected_byte_count(const std::vector<std::int64_t> &shape,
                            DataType type) noexcept {
        const auto elements = try_get_element_count(shape);
        if (!elements) {
            return ::sindre::general::Result<std::size_t>::failure(elements.error());
        }
        const auto bytes_per_element = try_data_type_size(type);
        if (!bytes_per_element) {
            return ::sindre::general::Result<std::size_t>::failure(bytes_per_element.error());
        }
        if (elements.value() > std::numeric_limits<std::size_t>::max() /
                                  bytes_per_element.value()) {
            return ::sindre::general::Result<std::size_t>::failure(
                ::sindre::general::Error::make(
                    std::errc::value_too_large,
                    "Typed tensor is too large",
                    "ai.tensor.byte_count"));
        }
        return ::sindre::general::Result<std::size_t>::success(
            elements.value() * bytes_per_element.value());
    }

    void validate() const {
        if (data.size() != expected_byte_count(shape, type))
            throw std::invalid_argument("Typed tensor data size does not match shape");
    }

    [[nodiscard]] ::sindre::general::Result<void> try_validate() const noexcept {
        const auto expected = try_expected_byte_count(shape, type);
        if (!expected) return ::sindre::general::Result<void>::failure(expected.error());
        if (data.size() != expected.value()) {
            return ::sindre::general::Result<void>::failure(
                ::sindre::general::Error::make(
                    std::errc::invalid_argument,
                    "Typed tensor data size does not match shape",
                    "ai.tensor.validate"));
        }
        return ::sindre::general::Result<void>::success();
    }

    [[nodiscard]] ::sindre::general::Result<void>
    try_reshape(const std::vector<std::int64_t> &new_shape) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
        try {
#endif
        const auto bytes = try_data_type_size(type);
        if (!bytes) return ::sindre::general::Result<void>::failure(bytes.error());
        if (data.size() % bytes.value() != 0)
            return ::sindre::general::Result<void>::failure(
                ::sindre::general::Error::make(
                    std::errc::invalid_argument,
                    "Typed tensor byte count is not aligned to its data type",
                    "ai.tensor.reshape"));
        const auto resolved = try_resolve_reshape(new_shape, data.size() / bytes.value());
        if (!resolved) return ::sindre::general::Result<void>::failure(resolved.error());
        shape = resolved.value();
        return ::sindre::general::Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
        } catch (const std::exception &error) {
            return ::sindre::general::Result<void>::failure(
                ::sindre::general::Error::make(
                    std::errc::not_enough_memory, error.what(), "ai.tensor.reshape"));
        } catch (...) {
            return ::sindre::general::Result<void>::failure(
                ::sindre::general::Error::make(
                    std::errc::not_enough_memory,
                    "Unable to allocate reshaped tensor metadata",
                    "ai.tensor.reshape"));
        }
#endif
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

    static ::sindre::general::Result<TypedTensor>
    try_from_bytes(const std::vector<std::int64_t> &shape, DataType type,
                   const void *source, std::size_t byte_count) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
        try {
#endif
            TypedTensor result{shape, type, {}};
            if (byte_count && !source) {
                return ::sindre::general::Result<TypedTensor>::failure(
                    ::sindre::general::Error::make(
                        std::errc::invalid_argument,
                        "Typed tensor data is null",
                        "ai.tensor.from_bytes"));
            }
            const auto expected = try_expected_byte_count(result.shape, result.type);
            if (!expected) return ::sindre::general::Result<TypedTensor>::failure(expected.error());
            if (byte_count != expected.value()) {
                return ::sindre::general::Result<TypedTensor>::failure(
                    ::sindre::general::Error::make(
                        std::errc::invalid_argument,
                        "Typed tensor data size does not match shape",
                        "ai.tensor.from_bytes"));
            }
            result.data.resize(byte_count);
            if (byte_count) std::memcpy(result.data.data(), source, byte_count);
            return ::sindre::general::Result<TypedTensor>::success(std::move(result));
#if !defined(SINDRE_NO_EXCEPTIONS)
        } catch (const std::exception &error) {
            return ::sindre::general::Result<TypedTensor>::failure(
                ::sindre::general::Error::make(
                    std::errc::io_error, error.what(), "ai.tensor.from_bytes"));
        } catch (...) {
            return ::sindre::general::Result<TypedTensor>::failure(
                ::sindre::general::Error::make(
                    std::errc::io_error,
                    "Unknown typed tensor allocation failure",
                    "ai.tensor.from_bytes"));
        }
#endif
    }

    template <class T>
    static TypedTensor from(std::vector<std::int64_t> shape, const std::vector<T>& values) {
        DataType type;
        if constexpr (std::is_same_v<T, float>) type = DataType::float32;
        else if constexpr (std::is_same_v<T, double>) type = DataType::float64;
        else if constexpr (std::is_same_v<T, std::int8_t>) type = DataType::int8;
        else if constexpr (std::is_same_v<T, std::int16_t>) type = DataType::int16;
        else if constexpr (std::is_same_v<T, std::int32_t>) type = DataType::int32;
        else if constexpr (std::is_same_v<T, std::int64_t>) type = DataType::int64;
        else if constexpr (std::is_same_v<T, std::uint16_t>) type = DataType::float16;
        else if constexpr (std::is_same_v<T, std::uint32_t>) type = DataType::uint32;
        else if constexpr (std::is_same_v<T, std::uint64_t>) type = DataType::uint64;
        else if constexpr (std::is_same_v<T, std::uint8_t>) type = DataType::bool8;
        else static_assert(std::is_same_v<T, void>, "Unsupported TypedTensor element type");
        return from_bytes(std::move(shape), type, values.data(), values.size() * sizeof(T));
    }

    template <class T>
    static ::sindre::general::Result<TypedTensor>
    try_from(const std::vector<std::int64_t> &shape, const std::vector<T> &values) noexcept {
        DataType type = DataType::other;
        if constexpr (std::is_same_v<T, float>) type = DataType::float32;
        else if constexpr (std::is_same_v<T, double>) type = DataType::float64;
        else if constexpr (std::is_same_v<T, std::int8_t>) type = DataType::int8;
        else if constexpr (std::is_same_v<T, std::int16_t>) type = DataType::int16;
        else if constexpr (std::is_same_v<T, std::int32_t>) type = DataType::int32;
        else if constexpr (std::is_same_v<T, std::int64_t>) type = DataType::int64;
        else if constexpr (std::is_same_v<T, std::uint16_t>) type = DataType::float16;
        else if constexpr (std::is_same_v<T, std::uint32_t>) type = DataType::uint32;
        else if constexpr (std::is_same_v<T, std::uint64_t>) type = DataType::uint64;
        else if constexpr (std::is_same_v<T, std::uint8_t>) type = DataType::bool8;
        else {
            static_assert(std::is_same_v<T, void>, "Unsupported TypedTensor element type");
        }
        if (values.size() > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            return ::sindre::general::Result<TypedTensor>::failure(
                ::sindre::general::Error::make(
                    std::errc::value_too_large,
                    "Typed tensor data is too large",
                    "ai.tensor.from"));
        }
        return try_from_bytes(shape, type, values.data(),
                              values.size() * sizeof(T));
    }

    /// @brief 按调用方指定的 DataType 从连续标准容器构造张量。
    template <class T>
    static ::sindre::general::Result<TypedTensor>
    try_from(const std::vector<std::int64_t> &shape, const std::vector<T> &values,
             DataType type) noexcept {
        const auto bytes = try_data_type_size(type);
        if (!bytes) return ::sindre::general::Result<TypedTensor>::failure(bytes.error());
        if (bytes.value() != sizeof(T)) {
            return ::sindre::general::Result<TypedTensor>::failure(
                ::sindre::general::Error::make(
                    std::errc::invalid_argument,
                    "C++ element size does not match tensor data type",
                    "ai.tensor.from"));
        }
        if (values.size() > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            return ::sindre::general::Result<TypedTensor>::failure(
                ::sindre::general::Error::make(
                    std::errc::value_too_large,
                    "Typed tensor data is too large",
                    "ai.tensor.from"));
        }
        return try_from_bytes(shape, type, values.data(),
                              values.size() * sizeof(T));
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

    [[nodiscard]] ::sindre::general::Result<void> try_validate() const noexcept {
        const auto elements = try_get_element_count(shape);
        if (!elements) return ::sindre::general::Result<void>::failure(elements.error());
        if (elements.value() != data.size()) {
            return ::sindre::general::Result<void>::failure(
                ::sindre::general::Error::make(
                    std::errc::invalid_argument,
                    "Tensor data size does not match shape",
                    "ai.tensor.validate"));
        }
        return ::sindre::general::Result<void>::success();
    }

    [[nodiscard]] ::sindre::general::Result<void>
    try_reshape(const std::vector<std::int64_t> &new_shape) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
        try {
#endif
        const auto resolved = try_resolve_reshape(new_shape, data.size());
        if (!resolved) return ::sindre::general::Result<void>::failure(resolved.error());
        shape = resolved.value();
        return ::sindre::general::Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
        } catch (const std::exception &error) {
            return ::sindre::general::Result<void>::failure(
                ::sindre::general::Error::make(
                    std::errc::not_enough_memory, error.what(), "ai.tensor.reshape"));
        } catch (...) {
            return ::sindre::general::Result<void>::failure(
                ::sindre::general::Error::make(
                    std::errc::not_enough_memory,
                    "Unable to allocate reshaped tensor metadata",
                    "ai.tensor.reshape"));
        }
#endif
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
