#pragma once
#if !defined(SINDRECPP_WITH_UTILS3D)
#error "Enable SINDRECPP_WITH_UTILS3D and link SindreCpp::Utils3d."
#endif

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/LU>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCleanPolyData.h>
#include <vtkCurvatures.h>
#include <vtkDataArray.h>
#include <vtkDoubleArray.h>
#include <vtkFeatureEdges.h>
#include <vtkIdTypeArray.h>
#include <vtkNew.h>
#include <vtkOBJReader.h>
#include <vtkOBJWriter.h>
#include <vtkPLYReader.h>
#include <vtkPLYWriter.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataAlgorithm.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkSTLReader.h>
#include <vtkSTLWriter.h>
#include <vtkSmartPointer.h>
#include <vtkStaticPointLocator.h>
#include <vtkStripper.h>
#include <vtkTriangleFilter.h>
#include <vtkTrivialProducer.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#if defined(SINDRECPP_UTILS3D_SHOW)
#include "show_mesh.hpp"
#endif

namespace sindrecpp::utils3d {
using Vertices = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;
using Faces = Eigen::Matrix<std::int64_t, Eigen::Dynamic, 3, Eigen::RowMajor>;
using Matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
using Labels = Eigen::Matrix<std::int64_t, Eigen::Dynamic, 1>;
struct MeshNormals {
    bool points = true, faces = true, consistent = true, auto_orient = false;
    bool flip = false, split = false;
    double feature_angle = 30;
};

// Value semantics: copies and filter results never share mutable VTK storage.
class SindreMesh {
    vtkSmartPointer<vtkPolyData> mesh_ = vtkSmartPointer<vtkPolyData>::New();
    static std::string extension(const std::filesystem::path &path) {
        auto e = path.extension().string();
        std::transform(e.begin(), e.end(), e.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return e;
    }
    template <class Reader>
    static vtkSmartPointer<vtkPolyData> read(const std::filesystem::path &p) {
        vtkNew<Reader> r;
        r->SetFileName(p.string().c_str());
        r->Update();
        if (r->GetErrorCode() || !r->GetOutput()->GetNumberOfPoints())
            throw std::runtime_error("Cannot read mesh: " + p.string());
        auto result = vtkSmartPointer<vtkPolyData>::New();
        result->DeepCopy(r->GetOutput());
        return result;
    }
    template <class Writer> void write(const std::filesystem::path &p) const {
        vtkNew<Writer> w;
        w->SetFileName(p.string().c_str());
        w->SetInputData(mesh_);
        if (!w->Write() || w->GetErrorCode())
            throw std::runtime_error("Cannot write mesh: " + p.string());
    }
    static void validate_geometry(const Vertices &v, const Faces &f) {
        if (!v.allFinite())
            throw std::invalid_argument("Vertices must be finite");
        if (v.rows() > std::numeric_limits<vtkIdType>::max())
            throw std::overflow_error("Too many vertices");
        for (Eigen::Index i = 0; i < f.rows(); ++i)
            for (int k = 0; k < 3; ++k)
                if (f(i, k) < 0 || f(i, k) >= v.rows())
                    throw std::out_of_range("Face vertex index out of range");
    }
    Matrix normals(bool point) const {
        auto copy = clone();
        copy.compute_normals();
        auto *a = point ? copy.mesh_->GetPointData()->GetNormals()
                        : copy.mesh_->GetCellData()->GetNormals();
        const auto n = point ? npoints() : nfaces();
        Matrix result(n, 3);
        if (n && !a)
            throw std::runtime_error("Normal computation failed");
        for (Eigen::Index i = 0; i < n; ++i)
            for (int k = 0; k < 3; ++k)
                result(i, k) = a->GetComponent(i, k);
        return result;
    }

  public:
    SindreMesh() = default;
    SindreMesh(const Vertices &v, const Faces &f) { update_geometry(v, f); }
    explicit SindreMesh(vtkPolyData *data) {
        if (!data)
            throw std::invalid_argument("Null VTK mesh");
        // Triangulation preserves point/cell arrays; line/vertex cells are not
        // meshes.
        vtkNew<vtkTriangleFilter> t;
        t->SetInputData(data);
        t->PassLinesOff();
        t->PassVertsOff();
        t->Update();
        mesh_->DeepCopy(t->GetOutput());
        validate_geometry(vertices(), faces());
    }
    explicit SindreMesh(const std::filesystem::path &path) { load(path); }
    SindreMesh(const SindreMesh &other) { mesh_->DeepCopy(other.mesh_); }
    SindreMesh &operator=(const SindreMesh &other) {
        if (this != &other)
            mesh_->DeepCopy(other.mesh_);
        return *this;
    }
    // Move transfers geometry without copying arrays; source becomes empty.
    SindreMesh(SindreMesh &&other) { std::swap(mesh_, other.mesh_); }
    SindreMesh &operator=(SindreMesh &&other) {
        if (this != &other) {
            SindreMesh temporary(std::move(other));
            std::swap(mesh_, temporary.mesh_);
        }
        return *this;
    }
    SindreMesh clone() const { return *this; }
    vtkPolyData *get_native() const noexcept {
        return mesh_;
    } // Borrowed, mutable native escape hatch.
    Eigen::Index npoints() const { return mesh_->GetNumberOfPoints(); }
    Eigen::Index nfaces() const { return mesh_->GetNumberOfPolys(); }
    bool empty() const { return npoints() == 0; }
#if defined(SINDRECPP_UTILS3D_SHOW)
    ShowMesh show(const ShowOptions &options = {}) const {
        return show_mesh(mesh_.GetPointer(), options);
    }
#endif
    Eigen::Matrix<double, 2, 3, Eigen::RowMajor> bounds() const {
        if (empty())
            throw std::invalid_argument("Empty mesh has no bounds");
        double b[6];
        mesh_->GetBounds(b);
        Eigen::Matrix<double, 2, 3, Eigen::RowMajor> result;
        result << b[0], b[2], b[4], b[1], b[3], b[5];
        return result;
    }
    Eigen::Vector3d dimensions() const {
        auto b = bounds();
        return (b.row(1) - b.row(0)).transpose();
    }
    SindreMesh filtered(vtkPolyDataAlgorithm *filter) const {
        if (!filter)
            throw std::invalid_argument("Null VTK filter");
        filter->SetInputData(mesh_);
        filter->Update();
        if (filter->GetErrorCode() || !filter->GetOutput())
            throw std::runtime_error("VTK filter failed");
        return SindreMesh(filter->GetOutput());
    }
    // Snapshot source for caller-owned lazy VTK pipelines. Later mesh edits
    // do not modify this source; retain the producer while using its port.
    vtkSmartPointer<vtkTrivialProducer> pipeline_source() const {
        auto data = vtkSmartPointer<vtkPolyData>::New();
        data->DeepCopy(mesh_);
        auto source = vtkSmartPointer<vtkTrivialProducer>::New();
        source->SetOutput(data);
        return source;
    }
    Vertices vertices() const {
        Vertices v(npoints(), 3);
        double p[3];
        for (Eigen::Index i = 0; i < v.rows(); ++i) {
            mesh_->GetPoint(i, p);
            for (int k = 0; k < 3; ++k)
                v(i, k) = p[k];
        }
        return v;
    }
    Faces faces() const {
        Faces f(nfaces(), 3);
        vtkIdType n;
        const vtkIdType *ids;
        Eigen::Index i = 0;
        auto *cells = mesh_->GetPolys();
        cells->InitTraversal();
        while (cells->GetNextCell(n, ids)) {
            if (n != 3)
                throw std::runtime_error("SindreMesh requires triangle faces");
            for (int k = 0; k < 3; ++k)
                f(i, k) = ids[k];
            ++i;
        }
        return f;
    }
    void update_geometry(const Vertices &v, const Faces &f) {
        validate_geometry(v, f);
        auto next = vtkSmartPointer<vtkPolyData>::New();
        vtkNew<vtkPoints> points;
        points->SetDataTypeToDouble();
        points->SetNumberOfPoints(v.rows());
        for (Eigen::Index i = 0; i < v.rows(); ++i)
            points->SetPoint(i, v.row(i).data());
        vtkNew<vtkCellArray> cells;
        for (Eigen::Index i = 0; i < f.rows(); ++i) {
            vtkIdType ids[3] = {vtkIdType(f(i, 0)), vtkIdType(f(i, 1)), vtkIdType(f(i, 2))};
            cells->InsertNextCell(3, ids);
        }
        next->SetPoints(points);
        next->SetPolys(cells);
        mesh_ = next; // Geometry replacement drops old attributes.
    }
    void update_geometry(const Vertices &v) {
        if (v.rows() != npoints())
            throw std::invalid_argument("Vertex-only update must retain vertex count");
        validate_geometry(v, faces());
        for (Eigen::Index i = 0; i < v.rows(); ++i)
            mesh_->GetPoints()->SetPoint(i, v.row(i).data());
        mesh_->GetPoints()->Modified();
        mesh_->GetPointData()->SetNormals(nullptr);
        mesh_->GetCellData()->SetNormals(nullptr);
        mesh_->Modified();
    }
    // Selection is index based; attributes follow their original elements, not interpolation.
    void update_faces(const std::vector<bool> &keep) {
        if (keep.size() != std::size_t(nfaces()))
            throw std::invalid_argument("Face mask size mismatch");
        auto next = vtkSmartPointer<vtkPolyData>::New();
        next->DeepCopy(mesh_);
        vtkNew<vtkCellArray> cells;
        auto f = faces();
        next->GetCellData()->Initialize();
        next->GetCellData()->CopyAllocate(mesh_->GetCellData());
        vtkIdType target = 0;
        for (Eigen::Index i = 0; i < f.rows(); ++i) {
            if (!keep[i])
                continue;
            vtkIdType ids[3] = {vtkIdType(f(i, 0)), vtkIdType(f(i, 1)), vtkIdType(f(i, 2))};
            cells->InsertNextCell(3, ids);
            next->GetCellData()->CopyData(mesh_->GetCellData(), i, target++);
        }
        next->SetPolys(cells);
        next->GetPointData()->SetNormals(nullptr);
        next->GetCellData()->SetNormals(nullptr);
        mesh_ = next;
    }
    void update_vertex(const std::vector<bool> &keep) {
        if (keep.size() != std::size_t(npoints()))
            throw std::invalid_argument("Vertex mask size mismatch");
        auto next = vtkSmartPointer<vtkPolyData>::New();
        vtkNew<vtkPoints> points;
        points->SetDataTypeToDouble();
        vtkNew<vtkCellArray> cells;
        std::vector<vtkIdType> mapping(keep.size(), -1);
        next->GetPointData()->CopyAllocate(mesh_->GetPointData());
        next->GetCellData()->CopyAllocate(mesh_->GetCellData());
        double point[3];
        for (Eigen::Index i = 0; i < npoints(); ++i) {
            if (!keep[i])
                continue;
            mesh_->GetPoint(i, point);
            const auto id = points->InsertNextPoint(point);
            mapping[i] = id;
            next->GetPointData()->CopyData(mesh_->GetPointData(), i, id);
        }
        auto f = faces();
        vtkIdType target = 0;
        for (Eigen::Index i = 0; i < f.rows(); ++i) {
            vtkIdType ids[3] = {mapping[f(i, 0)], mapping[f(i, 1)], mapping[f(i, 2)]};
            if (ids[0] < 0 || ids[1] < 0 || ids[2] < 0)
                continue;
            cells->InsertNextCell(3, ids);
            next->GetCellData()->CopyData(mesh_->GetCellData(), i, target++);
        }
        next->SetPoints(points);
        next->SetPolys(cells);
        next->GetPointData()->SetNormals(nullptr);
        next->GetCellData()->SetNormals(nullptr);
        mesh_ = next;
    }
    double area() const { return faces_area().sum(); }
    double signed_volume() const {
        if (!is_watertight())
            throw std::invalid_argument("Volume requires a closed surface");
        auto v = vertices();
        auto f = faces();
        double volume = 0;
        // Shift reference to reduce cancellation for models far from the origin.
        auto c = center();
        for (Eigen::Index i = 0; i < f.rows(); ++i) {
            Eigen::Vector3d a = v.row(f(i, 0)).transpose() - c;
            Eigen::Vector3d b = v.row(f(i, 1)).transpose() - c;
            Eigen::Vector3d d = v.row(f(i, 2)).transpose() - c;
            volume += a.dot(b.cross(d)) / 6.;
        }
        return volume; // Valid solid volume also requires consistent orientation / no
                       // self-intersections.
    }
    struct CheckReport {
        std::size_t duplicate_vertices = 0, degenerate_faces = 0, unused_vertices = 0;
        std::size_t boundary_edges = 0, non_manifold_edges = 0;
        bool edge_closed = false;
    };
    CheckReport check(double area_tolerance = 0) const {
        if (!std::isfinite(area_tolerance) || area_tolerance < 0)
            throw std::invalid_argument("Invalid degeneracy area tolerance");
        auto v = vertices();
        auto f = faces();
        CheckReport r;
        std::set<std::array<double, 3>> unique;
        for (Eigen::Index i = 0; i < v.rows(); ++i)
            if (!unique.insert({v(i, 0), v(i, 1), v(i, 2)}).second)
                ++r.duplicate_vertices;
        std::vector<bool> used(v.rows(), false);
        auto areas = faces_area();
        for (Eigen::Index i = 0; i < f.rows(); ++i) {
            for (int k = 0; k < 3; ++k)
                used[f(i, k)] = true;
            if (areas[i] <= area_tolerance)
                ++r.degenerate_faces;
        }
        r.unused_vertices = std::count(used.begin(), used.end(), false);
        r.boundary_edges = get_boundary().size();
        r.non_manifold_edges = get_non_manifold_edges().size();
        r.edge_closed = is_watertight();
        return r;
    }
    void load(const std::filesystem::path &p) {
        if (!std::filesystem::is_regular_file(p))
            throw std::runtime_error("Mesh file not found: " + p.string());
        vtkSmartPointer<vtkPolyData> d;
        auto e = extension(p);
        if (e == ".stl")
            d = read<vtkSTLReader>(p);
        else if (e == ".ply")
            d = read<vtkPLYReader>(p);
        else if (e == ".obj")
            d = read<vtkOBJReader>(p);
        else if (e == ".vtp")
            d = read<vtkXMLPolyDataReader>(p);
        else
            throw std::invalid_argument("Supported mesh formats: stl, ply, obj, vtp");
        SindreMesh next(d);
        mesh_ = next.mesh_;
    }
    void save(const std::filesystem::path &p) const {
        auto e = extension(p);
        if (e == ".stl")
            write<vtkSTLWriter>(p);
        else if (e == ".ply")
            write<vtkPLYWriter>(p);
        else if (e == ".obj")
            write<vtkOBJWriter>(p);
        else if (e == ".vtp")
            write<vtkXMLPolyDataWriter>(p);
        else
            throw std::invalid_argument("Supported mesh formats: stl, ply, obj, vtp");
    }
    void compute_normals(const MeshNormals &options = {}) {
        if (!std::isfinite(options.feature_angle) || options.feature_angle < 0 ||
            options.feature_angle > 180)
            throw std::invalid_argument("Normal feature angle must lie in [0,180]");
        if (!nfaces())
            return;
        vtkNew<vtkPolyDataNormals> n;
        n->SetInputData(mesh_);
        n->SetSplitting(options.split);
        n->SetFeatureAngle(options.feature_angle);
        n->SetConsistency(options.consistent);
        n->SetAutoOrientNormals(options.auto_orient);
        n->SetFlipNormals(options.flip);
        n->SetComputePointNormals(options.points);
        n->SetComputeCellNormals(options.faces);
        n->Update();
        mesh_->DeepCopy(n->GetOutput());
    }
    bool has_data(const std::string &name, bool point = true) const {
        return (point ? static_cast<vtkDataSetAttributes *>(mesh_->GetPointData())
                      : static_cast<vtkDataSetAttributes *>(mesh_->GetCellData()))
                   ->HasArray(name.c_str()) != 0;
    }
    std::vector<std::string> data_names(bool point = true) const {
        auto *a = point ? static_cast<vtkDataSetAttributes *>(mesh_->GetPointData())
                        : static_cast<vtkDataSetAttributes *>(mesh_->GetCellData());
        std::vector<std::string> names;
        for (int i = 0; i < a->GetNumberOfArrays(); ++i)
            if (a->GetArrayName(i))
                names.emplace_back(a->GetArrayName(i));
        return names;
    }
    void remove_data(const std::string &name, bool point = true) {
        if (!has_data(name, point))
            throw std::out_of_range("Mesh array not found: " + name);
        auto *a = point ? static_cast<vtkDataSetAttributes *>(mesh_->GetPointData())
                        : static_cast<vtkDataSetAttributes *>(mesh_->GetCellData());
        a->RemoveArray(name.c_str());
        mesh_->Modified();
    }
    void rename_data(const std::string &old_name, const std::string &new_name, bool point = true) {
        if (new_name.empty() || has_data(new_name, point))
            throw std::invalid_argument("New array name empty or already exists");
        auto *a = point ? static_cast<vtkDataSetAttributes *>(mesh_->GetPointData())
                        : static_cast<vtkDataSetAttributes *>(mesh_->GetCellData());
        auto *array = a->GetAbstractArray(old_name.c_str());
        if (!array)
            throw std::out_of_range("Mesh array not found: " + old_name);
        array->SetName(new_name.c_str());
        a->Modified();
        mesh_->Modified();
    }
    void clear_data(bool point = true) {
        if (point)
            mesh_->GetPointData()->Initialize();
        else
            mesh_->GetCellData()->Initialize();
        mesh_->Modified();
    }
    void set_uv(const Matrix &uv) {
        if (uv.rows() != npoints() || uv.cols() != 2 || !uv.allFinite())
            throw std::invalid_argument("UV must be finite Nx2");
        set_data("TextureCoordinates", uv);
        mesh_->GetPointData()->SetTCoords(mesh_->GetPointData()->GetArray("TextureCoordinates"));
    }
    Matrix get_uv() const {
        auto *uv = mesh_->GetPointData()->GetTCoords();
        if (!uv || uv->GetNumberOfComponents() != 2)
            throw std::out_of_range("Texture coordinates missing");
        Matrix result(uv->GetNumberOfTuples(), 2);
        for (Eigen::Index i = 0; i < result.rows(); ++i)
            for (int k = 0; k < 2; ++k)
                result(i, k) = uv->GetComponent(i, k);
        return result;
    }
    SindreMesh extract_faces(const std::vector<bool> &keep, bool compact = true) const {
        auto copy = clone();
        copy.update_faces(keep);
        if (compact) {
            std::vector<bool> used(copy.npoints(), false);
            auto f = copy.faces();
            for (Eigen::Index i = 0; i < f.rows(); ++i)
                for (int k = 0; k < 3; ++k)
                    used[f(i, k)] = true;
            copy.update_vertex(used);
        }
        return copy;
    }
    SindreMesh extract_region(const std::string &name, double lower, double upper,
                              bool point = false, bool all_vertices = true) const {
        if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper)
            throw std::invalid_argument("Invalid region range");
        auto values = get_data(name, point);
        if (values.cols() != 1)
            throw std::invalid_argument("Region selection needs scalar data");
        auto f = faces();
        std::vector<bool> keep(f.rows(), false);
        for (Eigen::Index i = 0; i < f.rows(); ++i) {
            if (!point)
                keep[i] = values(i, 0) >= lower && values(i, 0) <= upper;
            else {
                bool value = all_vertices;
                for (int k = 0; k < 3; ++k) {
                    const bool inside = values(f(i, k), 0) >= lower && values(f(i, k), 0) <= upper;
                    if (all_vertices)
                        value = value && inside;
                    else
                        value = value || inside;
                }
                keep[i] = value;
            }
        }
        return extract_faces(keep);
    }
    Matrix vertex_normals() const { return normals(true); }
    Matrix face_normals() const { return normals(false); }
    Matrix get_pointdata(const std::string &name) const { return get_data(name, true); }
    Matrix get_celldata(const std::string &name) const { return get_data(name, false); }
    Matrix get_data(const std::string &name, bool point) const {
        auto *a = point ? mesh_->GetPointData()->GetArray(name.c_str())
                        : mesh_->GetCellData()->GetArray(name.c_str());
        if (!a)
            throw std::out_of_range("Mesh array not found: " + name);
        Matrix x(a->GetNumberOfTuples(), a->GetNumberOfComponents());
        for (Eigen::Index i = 0; i < x.rows(); ++i)
            for (Eigen::Index k = 0; k < x.cols(); ++k)
                x(i, k) = a->GetComponent(i, k);
        return x;
    }
    void set_data(const std::string &name, const Matrix &x, bool point = true) {
        if (name.empty() || x.rows() != (point ? npoints() : nfaces()) || x.cols() < 1 ||
            !x.allFinite())
            throw std::invalid_argument("Invalid mesh attribute name, shape or values");
        vtkNew<vtkDoubleArray> a;
        a->SetName(name.c_str());
        a->SetNumberOfComponents(int(x.cols()));
        a->SetNumberOfTuples(x.rows());
        for (Eigen::Index i = 0; i < x.rows(); ++i)
            for (Eigen::Index k = 0; k < x.cols(); ++k)
                a->SetComponent(i, k, x(i, k));
        if (point)
            mesh_->GetPointData()->AddArray(a);
        else
            mesh_->GetCellData()->AddArray(a);
    }
    void set_labels(const Labels &labels, bool point) {
        if (labels.size() != (point ? npoints() : nfaces()))
            throw std::invalid_argument("Label count mismatch");
        vtkNew<vtkIdTypeArray> a;
        a->SetName("Labels");
        a->SetNumberOfValues(labels.size());
        for (Eigen::Index i = 0; i < labels.size(); ++i) {
            if constexpr (sizeof(vtkIdType) < sizeof(std::int64_t))
                if (labels[i] < std::numeric_limits<vtkIdType>::min() ||
                    labels[i] > std::numeric_limits<vtkIdType>::max())
                    throw std::overflow_error("Label overflow");
            a->SetValue(i, vtkIdType(labels[i]));
        }
        if (point)
            mesh_->GetPointData()->AddArray(a);
        else
            mesh_->GetCellData()->AddArray(a);
    }
    Labels get_labels(bool point) const {
        auto *a = vtkIdTypeArray::SafeDownCast(point ? mesh_->GetPointData()->GetArray("Labels")
                                                     : mesh_->GetCellData()->GetArray("Labels"));
        if (!a)
            throw std::out_of_range("Integer Labels array not found");
        Labels x(a->GetNumberOfValues());
        for (Eigen::Index i = 0; i < x.size(); ++i)
            x[i] = a->GetValue(i);
        return x;
    }
    void set_vertex_labels(const Labels &x) { set_labels(x, true); }
    void set_faces_labels(const Labels &x) { set_labels(x, false); }
    Labels get_vertex_labels() const { return get_labels(true); }
    Labels get_faces_labels() const { return get_labels(false); }
    SindreMesh &apply_transform(const Eigen::Matrix4d &m) {
        if (!m.allFinite() || !m.row(3).isApprox(Eigen::RowVector4d(0, 0, 0, 1)))
            throw std::invalid_argument("Expected finite affine 4x4 transform");
        Vertices v = vertices();
        for (Eigen::Index i = 0; i < v.rows(); ++i)
            v.row(i) = (m.topLeftCorner<3, 3>() * v.row(i).transpose() + m.topRightCorner<3, 1>())
                           .transpose();
        update_geometry(v);
        compute_normals();
        return *this;
    }
    SindreMesh &apply_transform(const Eigen::Matrix3d &m) {
        Eigen::Matrix4d a = Eigen::Matrix4d::Identity();
        a.topLeftCorner<3, 3>() = m;
        return apply_transform(a);
    }
    SindreMesh &apply_inv_transform(const Eigen::Matrix4d &m) {
        if (!m.allFinite())
            throw std::invalid_argument("Transform must be finite");
        Eigen::FullPivLU<Eigen::Matrix4d> lu(m);
        if (!lu.isInvertible())
            throw std::invalid_argument("Singular transform");
        return apply_transform(Eigen::Matrix4d(lu.inverse()));
    }
    SindreMesh &shift_xyz(const Eigen::Vector3d &d) {
        Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
        m.topRightCorner<3, 1>() = d;
        return apply_transform(m);
    }
    SindreMesh &scale_xyz(const Eigen::Vector3d &s) {
        return apply_transform(Eigen::Matrix3d(s.asDiagonal()));
    }
    SindreMesh &scale_xyz(double s) { return scale_xyz(Eigen::Vector3d::Constant(s)); }
    SindreMesh &rotate_xyz(const Eigen::Vector3d &degrees) {
        constexpr double rad = 3.14159265358979323846 / 180.;
        return apply_transform(
            Eigen::Matrix3d((Eigen::AngleAxisd(degrees.z() * rad, Eigen::Vector3d::UnitZ()) *
                             Eigen::AngleAxisd(degrees.y() * rad, Eigen::Vector3d::UnitY()) *
                             Eigen::AngleAxisd(degrees.x() * rad, Eigen::Vector3d::UnitX()))
                                .toRotationMatrix()));
    }
    Eigen::Vector3d center() const {
        if (empty())
            throw std::invalid_argument("Empty mesh has no center");
        return vertices().colwise().mean().transpose();
    }
    double radius() const {
        const auto c = center();
        return (vertices().rowwise() - c.transpose()).rowwise().norm().maxCoeff();
    }
    Vertices faces_barycentre() const {
        auto v = vertices();
        auto f = faces();
        Vertices c(f.rows(), 3);
        for (Eigen::Index i = 0; i < f.rows(); ++i)
            c.row(i) = (v.row(f(i, 0)) + v.row(f(i, 1)) + v.row(f(i, 2))) / 3.;
        return c;
    }
    Eigen::VectorXd faces_area() const {
        auto v = vertices();
        auto f = faces();
        Eigen::VectorXd a(f.rows());
        for (Eigen::Index i = 0; i < f.rows(); ++i) {
            Eigen::Vector3d x = v.row(f(i, 1)) - v.row(f(i, 0)),
                            y = v.row(f(i, 2)) - v.row(f(i, 0));
            a[i] = x.cross(y).norm() * .5;
        }
        return a;
    }
    using Edge = std::array<std::int64_t, 2>;
    std::map<Edge, std::vector<std::int64_t>> edges_face() const {
        std::map<Edge, std::vector<std::int64_t>> result;
        auto f = faces();
        for (Eigen::Index i = 0; i < f.rows(); ++i)
            for (int k = 0; k < 3; ++k) {
                auto a = f(i, k), b = f(i, (k + 1) % 3);
                if (a > b)
                    std::swap(a, b);
                result[{a, b}].push_back(i);
            }
        return result;
    }
    std::vector<Edge> get_edges() const {
        std::vector<Edge> x;
        for (const auto &e : edges_face())
            x.push_back(e.first);
        return x;
    }
    std::vector<Edge> get_boundary() const {
        std::vector<Edge> x;
        for (const auto &e : edges_face())
            if (e.second.size() == 1)
                x.push_back(e.first);
        return x;
    }
    std::vector<Edge> get_non_manifold_edges() const {
        std::vector<Edge> x;
        for (const auto &e : edges_face())
            if (e.second.size() > 2)
                x.push_back(e.first);
        return x;
    }
    std::vector<std::vector<std::int64_t>> boundary_loops() const {
        std::map<std::int64_t, std::vector<std::int64_t>> adjacency;
        std::set<Edge> remaining;
        for (auto e : get_boundary()) {
            adjacency[e[0]].push_back(e[1]);
            adjacency[e[1]].push_back(e[0]);
            remaining.insert(e);
        }
        for (const auto &v : adjacency)
            if (v.second.size() != 2)
                throw std::invalid_argument(
                    "Boundary contains branching/open chains; use get_boundary for raw edges");
        std::vector<std::vector<std::int64_t>> loops;
        while (!remaining.empty()) {
            const auto start = (*remaining.begin())[0];
            auto current = start;
            std::int64_t previous = -1;
            std::vector<std::int64_t> loop;
            do {
                loop.push_back(current);
                const auto &next = adjacency.at(current);
                const auto target = next[0] != previous ? next[0] : next[1];
                remaining.erase({std::min(current, target), std::max(current, target)});
                previous = current;
                current = target;
                if (loop.size() > adjacency.size())
                    throw std::runtime_error("Invalid boundary traversal");
            } while (current != start);
            loops.push_back(std::move(loop));
        }
        return loops; // Undirected cycle order, no clockwise/orientation guarantee.
    }
    vtkSmartPointer<vtkPolyData> feature_edges(double angle = 30) const {
        if (!std::isfinite(angle) || angle < 0 || angle > 180)
            throw std::invalid_argument("Invalid feature angle");
        vtkNew<vtkFeatureEdges> filter;
        filter->SetInputData(mesh_);
        filter->BoundaryEdgesOff();
        filter->NonManifoldEdgesOff();
        filter->ManifoldEdgesOff();
        filter->FeatureEdgesOn();
        filter->SetFeatureAngle(angle);
        filter->Update();
        auto result = vtkSmartPointer<vtkPolyData>::New();
        result->DeepCopy(filter->GetOutput());
        return result;
    }
    std::vector<std::vector<std::int64_t>> get_vertex_adj_list() const {
        std::vector<std::set<std::int64_t>> s(npoints());
        for (const auto &e : get_edges()) {
            s[e[0]].insert(e[1]);
            s[e[1]].insert(e[0]);
        }
        std::vector<std::vector<std::int64_t>> x;
        for (const auto &a : s)
            x.emplace_back(a.begin(), a.end());
        return x;
    }
    std::vector<std::vector<std::int64_t>> get_face_adj_list() const {
        std::vector<std::set<std::int64_t>> s(nfaces());
        for (const auto &e : edges_face())
            for (auto a : e.second)
                for (auto b : e.second)
                    if (a != b)
                        s[a].insert(b);
        std::vector<std::vector<std::int64_t>> x;
        for (const auto &a : s)
            x.emplace_back(a.begin(), a.end());
        return x;
    }
    bool is_watertight() const {
        if (!nfaces())
            return false;
        for (const auto &e : edges_face())
            if (e.second.size() != 2)
                return false;
        return true;
    } // Edge closure only, not a solid-validity proof.
    Labels get_near_idx(const Vertices &query) const {
        if (empty() || !query.allFinite())
            throw std::invalid_argument("Nearest query requires finite points and nonempty source");
        vtkNew<vtkStaticPointLocator> loc;
        loc->SetDataSet(mesh_);
        loc->BuildLocator();
        Labels ids(query.rows());
        for (Eigen::Index i = 0; i < query.rows(); ++i)
            ids[i] = loc->FindClosestPoint(query.row(i).data());
        return ids;
    }
    SindreMesh clean(double tolerance = 0) const {
        if (!std::isfinite(tolerance) || tolerance < 0)
            throw std::invalid_argument("Invalid merge tolerance");
        vtkNew<vtkCleanPolyData> c;
        c->SetInputData(mesh_);
        c->ToleranceIsAbsoluteOn();
        c->SetAbsoluteTolerance(tolerance);
        c->ConvertPolysToLinesOff();
        c->ConvertLinesToPointsOff();
        c->Update();
        return SindreMesh(c->GetOutput());
    }
    SindreMesh largest_component() const {
        vtkNew<vtkPolyDataConnectivityFilter> c;
        c->SetInputData(mesh_);
        c->SetExtractionModeToLargestRegion();
        c->Update();
        return SindreMesh(c->GetOutput()).clean();
    }
    std::vector<SindreMesh> split_component_by_faces() const {
        vtkNew<vtkPolyDataConnectivityFilter> c;
        c->SetInputData(mesh_);
        c->SetExtractionModeToAllRegions();
        c->Update();
        const auto n = c->GetNumberOfExtractedRegions();
        std::vector<SindreMesh> x;
        for (int i = 0; i < n; ++i) {
            c->SetExtractionModeToSpecifiedRegions();
            c->InitializeSpecifiedRegionList();
            c->AddSpecifiedRegion(i);
            c->Update();
            x.emplace_back(SindreMesh(c->GetOutput()).clean());
        }
        return x;
    }
    Eigen::VectorXd get_curvature(bool mean = true) const {
        if (!nfaces())
            throw std::invalid_argument("Curvature requires faces");
        vtkNew<vtkCurvatures> c;
        c->SetInputData(mesh_);
        if (mean)
            c->SetCurvatureTypeToMean();
        else
            c->SetCurvatureTypeToGaussian();
        c->Update();
        auto *a = c->GetOutput()->GetPointData()->GetScalars();
        if (!a)
            throw std::runtime_error("Curvature computation failed");
        Eigen::VectorXd x(npoints());
        for (Eigen::Index i = 0; i < x.size(); ++i)
            x[i] = a->GetComponent(i, 0);
        return x;
    }
};
} // namespace sindrecpp::utils3d
