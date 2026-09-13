#pragma once

#if !defined(SINDRECPP_WITH_PYTHON)
#error "Enable SINDRECPP_WITH_PYTHON and link SindreCpp::Python before including this header."
#endif

#include <pybind11/embed.h>
#include <pybind11/numpy.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace sindrecpp::python {

namespace native = pybind11;
using Interpreter = pybind11::scoped_interpreter;

template <class T>
pybind11::array_t<T> array_from_vector(const std::vector<T>& values) {
    pybind11::array_t<T> result(values.size());
    if (!values.empty()) {
        std::copy(values.begin(), values.end(), result.mutable_data());
    }
    return result;
}

template <class T>
std::vector<T> vector_from_array(const pybind11::array& input) {
    using Array = pybind11::array_t<T, pybind11::array::c_style | pybind11::array::forcecast>;
    const auto array = Array::ensure(input);
    if (!array) {
        throw std::invalid_argument("NumPy array could not be converted to the requested SindreCpp element type");
    }
    if (array.ndim() != 1) {
        throw std::invalid_argument("SindreCpp expects a one-dimensional NumPy array");
    }
    if (array.size() == 0) return {};
    const auto info = array.request();
    const auto* begin = static_cast<const T*>(info.ptr);
    return std::vector<T>(begin, begin + info.size);
}

} // namespace sindrecpp::python
