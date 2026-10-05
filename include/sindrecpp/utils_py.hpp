#pragma once

#if !defined(SINDRECPP_WITH_UTILS_PY)
#error "Enable SINDRECPP_WITH_UTILS_PY and link SindreCpp::Utils_py before including this header."
#endif

#include <pybind11/embed.h>
#include <pybind11/numpy.h>

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <type_traits>
#include <limits>
#include <cmath>

#if defined(SINDRECPP_WITH_UTILS3D)
#include "utils3d/sindremesh.hpp"
#include <Eigen/Core>
#endif

namespace sindrecpp::utils_py {

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

#if defined(SINDRECPP_WITH_UTILS3D)
// Copy-only interchange: no Python/VTK/Eigen object borrows memory from another.
// Strided, transposed, negative-stride and Fortran arrays are materialized safely.
template<class Scalar>
Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic,Eigen::RowMajor>
matrix_from_array(const pybind11::array& input) {
    if(input.ndim()!=2)throw std::invalid_argument("Expected a two-dimensional NumPy array");
    if constexpr(std::is_integral_v<Scalar>) {
        const auto kind=input.dtype().kind();
        if(kind!='i'&&kind!='u')throw std::invalid_argument("Integer matrix requires integer dtype (no floating-point truncation)");
        // Validate before forcecast to avoid unsigned/signed narrowing and wraparound.
        auto np=pybind11::module_::import("numpy");
        if(input.size()) {
            auto min_value=np.attr("min")(input).attr("item")();
            auto max_value=np.attr("max")(input).attr("item")();
            if(pybind11::cast<bool>(min_value.attr("__lt__")(pybind11::int_(std::numeric_limits<Scalar>::min()))) ||
               pybind11::cast<bool>(max_value.attr("__gt__")(pybind11::int_(std::numeric_limits<Scalar>::max()))))
                throw std::overflow_error("NumPy integer value exceeds target matrix dtype");
        }
    } else {
        const auto kind=input.dtype().kind();
        if(kind!='f'&&kind!='i'&&kind!='u')throw std::invalid_argument("Expected a real numeric NumPy dtype");
    }
    using Array=pybind11::array_t<Scalar,pybind11::array::c_style|pybind11::array::forcecast>;
    auto array=Array::ensure(input);if(!array)throw std::invalid_argument("NumPy array conversion failed");
    Eigen::Matrix<Scalar,Eigen::Dynamic,Eigen::Dynamic,Eigen::RowMajor> result(array.shape(0),array.shape(1));
    if(array.size())std::copy(array.data(),array.data()+array.size(),result.data());
    if constexpr(std::is_floating_point_v<Scalar>)
        if(!result.allFinite())throw std::invalid_argument("Matrix values must be finite");
    return result;
}
template<class Derived>
pybind11::array_t<typename Derived::Scalar> array_from_matrix(const Eigen::MatrixBase<Derived>& matrix) {
    using T=typename Derived::Scalar;
    pybind11::array_t<T> out({pybind11::ssize_t(matrix.rows()),pybind11::ssize_t(matrix.cols())});
    auto a=out.template mutable_unchecked<2>();
    for(Eigen::Index i=0;i<matrix.rows();++i)for(Eigen::Index j=0;j<matrix.cols();++j)a(i,j)=matrix(i,j);
    return out;
}
inline utils3d::SindreMesh mesh_from_arrays(const pybind11::array& vertices,const pybind11::array& faces) {
    if(vertices.ndim()!=2||vertices.shape(1)!=3||faces.ndim()!=2||faces.shape(1)!=3)
        throw std::invalid_argument("Mesh vertices/faces must have shape (N,3)/(M,3)");
    return utils3d::SindreMesh(utils3d::Vertices(matrix_from_array<double>(vertices)),
        utils3d::Faces(matrix_from_array<std::int64_t>(faces)));
}
inline pybind11::tuple arrays_from_mesh(const utils3d::SindreMesh& mesh) {
    return pybind11::make_tuple(array_from_matrix(mesh.vertices()),array_from_matrix(mesh.faces()));
}
#endif

} // namespace sindrecpp::utils_py
