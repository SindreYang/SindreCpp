#pragma once

/// @file
/// @brief VTK 数据集、数组和常用滤波操作接口。

#include "mesh.h"
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkTrivialProducer.h>

namespace sindre::utils_3d::core {

/// @brief 对 VTK 数据集提供值语义、数据数组和常用滤波操作的封装。
class Data {
    class Impl;
    std::shared_ptr<Impl> impl_;

  public:
    explicit Data(vtkDataObject *input);
    Data(const Data &other);
    Data &operator=(const Data &other);
    Data(Data &&other) noexcept;
    Data &operator=(Data &&other) noexcept;
    ~Data();

    vtkDataObject *get_native() const;
    vtkDataSet *dataset() const;
    std::string type() const;
    vtkIdType npoints() const;
    vtkIdType ncells() const;
    Vertices points() const;

    static Data point_cloud(const Vertices &points);
    static Data polyline(const Vertices &points, bool closed = false);
    static Data structured_grid(const Vertices &points, std::array<int, 3> dimensions);
    static Data rectilinear_grid(const ::sindre::math::VectorXd &x,
                                 const ::sindre::math::VectorXd &y,
                                 const ::sindre::math::VectorXd &z);
    static Data tetrahedra(const Vertices &points,
                           const ::sindre::math::Matrix<std::int64_t,
                               ::sindre::math::eigen::Dynamic, 4> &cells);
    static Data blocks(const std::vector<Data> &items);
    unsigned nblocks() const;
    Data block(unsigned index) const;

    void set_data(const std::string &name, const Matrix &values, bool point = true);
    Matrix get_data(const std::string &name, bool point = true) const;
    vtkSmartPointer<vtkTrivialProducer> pipeline_source() const;
    Mesh surface() const;
    Data contour(const std::string &scalar, const std::vector<double> &levels) const;
    Data threshold(const std::string &scalar, double lower, double upper,
                   bool point = true) const;
    Data clip_plane(const ::sindre::math::Vector3 &origin,
                    const ::sindre::math::Vector3 &normal,
                    bool inside = false) const;
    Data gradient(const std::string &name, bool point = true, bool vorticity = false,
                  bool divergence = false) const;
    Data warp_vector(const std::string &name, double scale = 1) const;
    Data warp_scalar(const std::string &name, double scale = 1,
                     ::sindre::math::Vector3 normal =
                         ::sindre::math::Vector3::UnitZ()) const;
    Data point_to_cell_data() const;
    Data cell_to_point_data() const;
    Data probe(const Data &source) const;
    Data connected_regions(bool largest = false) const;
    Data calculate(const std::string &expression, const std::vector<std::string> &variables,
                   const std::string &output = "Result", bool point = true) const;
    Data delaunay(bool three_dimensional = true) const;
    Data tube(double radius = .1, int sides = 12) const;
    Data glyph_vectors(const std::string &name, double scale = 1) const;
    Data streamlines(const std::string &vectors, const Vertices &seeds,
                     double length = 10) const;

    static Data load(const std::filesystem::path &path);
    void save(const std::filesystem::path &path) const;
};

} // namespace sindre::utils_3d::core
