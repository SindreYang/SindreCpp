#include <iostream>
#include <general/core/python.hpp>
#include <stdexcept>
namespace py = pybind11;
using namespace sindrecpp;
static void check(bool b) {
    if (!b)
        throw std::runtime_error("NumPy conversion test failed");
}
template <class F> static void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected);
}
int main() {
    utils_py::Interpreter interpreter{};
    try {
        auto np = py::module_::import("numpy");
        py::dict scope;
        scope["np"] = np;
        py::exec("v=np.arange(24,dtype=np.float64).reshape(8,3)[::-2]\nf=np.array(["
                 "[0,1,2]],dtype=np.int64)\n",
                 scope);
        auto v = scope["v"].cast<py::array>();
        auto f = scope["f"].cast<py::array>();
        auto mesh = utils_py::mesh_from_arrays(v, f);
        check(mesh.npoints() == 4 && mesh.vertices()(0, 0) == 21);
        auto arrays = utils_py::arrays_from_mesh(mesh);
        auto out = arrays[0].cast<py::array_t<double>>();
        out.mutable_data()[0] = 999;
        check(mesh.vertices()(0, 0) == 21);
        scope["v"].attr("__setitem__")(py::make_tuple(0, 0), -99);
        check(mesh.vertices()(0, 0) == 21);
        auto column = np.attr("asfortranarray")(scope["v"]).cast<py::array>();
        check(utils_py::matrix_from_array<double>(column)(1, 1) == 16);
        utils3d::Matrix x(2, 3);
        x << 1, 2, 3, 4, 5, 6;
        Eigen::MatrixXd col = x;
        auto a = utils_py::array_from_matrix(col.transpose());
        check(a.shape(0) == 3 && a.at(2, 1) == 6);
        rejects([&] {
            utils_py::mesh_from_arrays(
                v, np.attr("array")(py::make_tuple(py::make_tuple(0., 1., 2.))).cast<py::array>());
        });
        py::exec("big=np.array([[2**64-1]],dtype=np.uint64)\nnan=np.array([[float('"
                 "nan')]])\n",
                 scope);
        rejects([&] { utils_py::matrix_from_array<std::int64_t>(scope["big"].cast<py::array>()); });
        rejects([&] { utils_py::matrix_from_array<double>(scope["nan"].cast<py::array>()); });
        auto empty = np.attr("empty")(py::make_tuple(0, 3)).cast<py::array>();
        auto empty_f =
            np.attr("empty")(py::make_tuple(0, 3), py::arg("dtype") = "int64").cast<py::array>();
        check(utils_py::mesh_from_arrays(empty, empty_f).empty());
        std::cout << "Mesh/NumPy tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
