#pragma once
#if !defined(SINDRE_UTILS_3D_VTK_DATA)
#error "Enable SINDRE_UTILS_3D_VTK_DATA and link sindre::utils_3d."
#endif
#include "mesh.h"
#include <algorithm>
#include <cctype>
#include <vtkArrayCalculator.h>
#include <vtkArrowSource.h>
#include <vtkCallbackCommand.h>
#include <vtkCellDataToPointData.h>
#include <vtkClipDataSet.h>
#include <vtkCommand.h>
#include <vtkConnectivityFilter.h>
#include <vtkContourFilter.h>
#include <vtkDataSet.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkDelaunay2D.h>
#include <vtkDelaunay3D.h>
#include <vtkGlyph3D.h>
#include <vtkGradientFilter.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPointDataToCellData.h>
#include <vtkPlane.h>
#include <vtkProbeFilter.h>
#include <vtkRectilinearGrid.h>
#include <vtkStreamTracer.h>
#include <vtkStructuredGrid.h>
#include <vtkThreshold.h>
#include <vtkTubeFilter.h>
#include <vtkUnstructuredGrid.h>
#include <vtkWarpScalar.h>
#include <vtkWarpVector.h>
#include <vtkXMLDataSetWriter.h>
#include <vtkXMLGenericDataObjectReader.h>
#include <vtkXMLMultiBlockDataWriter.h>

#define SindreMesh Mesh
#define SindreData Data
namespace sindre::utils_3d::detail::legacy {
// Value wrapper for VTK datasets/composites. Filters return independent objects.
class SindreData {
  protected:
    vtkSmartPointer<vtkDataObject> data_;
    static vtkSmartPointer<vtkPoints> make_points(const Vertices &points) {
        if (!points.allFinite())
            throw std::invalid_argument("Points must be finite");
        auto result = vtkSmartPointer<vtkPoints>::New();
        result->SetDataTypeToDouble();
        for (Eigen::Index i = 0; i < points.rows(); ++i)
            result->InsertNextPoint(points.row(i).data());
        return result;
    }
    static std::size_t grid_size(const std::array<int, 3> &dimensions) {
        std::size_t total = 1;
        for (auto d : dimensions) {
            if (d < 1 ||
                total > std::size_t(std::numeric_limits<vtkIdType>::max()) / std::size_t(d))
                throw std::invalid_argument("Invalid grid dimensions");
            total *= std::size_t(d);
        }
        return total;
    }
    vtkDataArray *require_array(const std::string &name, bool point, int components = 0) const {
        auto *ds = dataset();
        auto *a = point ? ds->GetPointData()->GetArray(name.c_str())
                        : ds->GetCellData()->GetArray(name.c_str());
        if (!a || (components && a->GetNumberOfComponents() != components))
            throw std::invalid_argument("Missing array or incompatible components: " + name);
        return a;
    }
    static int association(bool point) {
        return point ? vtkDataObject::FIELD_ASSOCIATION_POINTS
                     : vtkDataObject::FIELD_ASSOCIATION_CELLS;
    }

  public:
    explicit SindreData(vtkDataObject *input) {
        if (!input)
            throw std::invalid_argument("Null VTK data");
        data_.TakeReference(input->NewInstance());
        data_->DeepCopy(input);
    }
    SindreData(const SindreData &other) : SindreData(other.data_) {}
    SindreData &operator=(const SindreData &other) {
        if (this != &other) {
            SindreData copy(other);
            std::swap(data_, copy.data_);
        }
        return *this;
    }
    SindreData(SindreData &&) noexcept = default;
    SindreData &operator=(SindreData &&) noexcept = default;
    vtkDataObject *get_native() const {
        if (!data_)
            throw std::logic_error("Moved-from VTK data");
        return data_;
    }
    vtkDataSet *dataset() const {
        auto *ds = vtkDataSet::SafeDownCast(get_native());
        if (!ds)
            throw std::invalid_argument(
                "Operation requires a leaf dataset; use block() for composites");
        return ds;
    }
    std::string type() const { return get_native()->GetClassName(); }
    vtkIdType npoints() const { return dataset()->GetNumberOfPoints(); }
    vtkIdType ncells() const { return dataset()->GetNumberOfCells(); }
    Vertices points() const {
        auto *ds = dataset();
        Vertices result(ds->GetNumberOfPoints(), 3);
        double p[3];
        for (Eigen::Index i = 0; i < result.rows(); ++i) {
            ds->GetPoint(i, p);
            for (int k = 0; k < 3; ++k)
                result(i, k) = p[k];
        }
        return result;
    }
    static SindreData point_cloud(const Vertices &v) {
        vtkNew<vtkPolyData> data;
        data->SetPoints(make_points(v));
        vtkNew<vtkCellArray> verts;
        for (vtkIdType i = 0; i < v.rows(); ++i)
            verts->InsertNextCell(1, &i);
        data->SetVerts(verts);
        return SindreData(data);
    }
    static SindreData polyline(const Vertices &v, bool closed = false) {
        if (v.rows() < 2)
            throw std::invalid_argument("Polyline needs at least two points");
        vtkNew<vtkPolyData> data;
        data->SetPoints(make_points(v));
        vtkNew<vtkCellArray> lines;
        std::vector<vtkIdType> ids(v.rows());
        for (vtkIdType i = 0; i < v.rows(); ++i)
            ids[i] = i;
        if (closed)
            ids.push_back(0);
        lines->InsertNextCell(ids.size(), ids.data());
        data->SetLines(lines);
        return SindreData(data);
    }
    static SindreData structured_grid(const Vertices &v, std::array<int, 3> dimensions) {
        if (grid_size(dimensions) != std::size_t(v.rows()))
            throw std::invalid_argument("Structured grid point count mismatch");
        vtkNew<vtkStructuredGrid> grid;
        grid->SetDimensions(dimensions.data());
        grid->SetPoints(make_points(v));
        return SindreData(grid);
    }
    static SindreData rectilinear_grid(const Eigen::VectorXd &x, const Eigen::VectorXd &y,
                                       const Eigen::VectorXd &z) {
        std::array<const Eigen::VectorXd *, 3> coordinates{&x, &y, &z};
        vtkNew<vtkRectilinearGrid> grid;
        int sizes[3];
        std::array<vtkSmartPointer<vtkDoubleArray>, 3> arrays;
        for (int k = 0; k < 3; ++k) {
            const auto &v = *coordinates[k];
            if (v.size() < 1 || v.size() > std::numeric_limits<int>::max() || !v.allFinite())
                throw std::invalid_argument("Invalid rectilinear coordinates");
            for (Eigen::Index i = 1; i < v.size(); ++i)
                if (v[i] <= v[i - 1])
                    throw std::invalid_argument("Coordinates must be strictly increasing");
            sizes[k] = int(v.size());
            arrays[k] = vtkSmartPointer<vtkDoubleArray>::New();
            for (Eigen::Index i = 0; i < v.size(); ++i)
                arrays[k]->InsertNextValue(v[i]);
        }
        grid->SetDimensions(sizes);
        grid->SetXCoordinates(arrays[0]);
        grid->SetYCoordinates(arrays[1]);
        grid->SetZCoordinates(arrays[2]);
        return SindreData(grid);
    }
    static SindreData
    tetrahedra(const Vertices &v,
               const Eigen::Matrix<std::int64_t, Eigen::Dynamic, 4, Eigen::RowMajor> &cells) {
        vtkNew<vtkUnstructuredGrid> grid;
        grid->SetPoints(make_points(v));
        for (Eigen::Index i = 0; i < cells.rows(); ++i) {
            vtkIdType ids[4];
            for (int k = 0; k < 4; ++k) {
                if (cells(i, k) < 0 || cells(i, k) >= v.rows())
                    throw std::out_of_range("Tetrahedron index");
                ids[k] = cells(i, k);
            }
            grid->InsertNextCell(VTK_TETRA, 4, ids);
        }
        return SindreData(grid);
    }
    static SindreData blocks(const std::vector<SindreData> &items) {
        if (items.size() > std::numeric_limits<unsigned int>::max())
            throw std::overflow_error("Too many blocks");
        vtkNew<vtkMultiBlockDataSet> blocks;
        blocks->SetNumberOfBlocks(unsigned(items.size()));
        for (unsigned i = 0; i < items.size(); ++i)
            blocks->SetBlock(i, items[i].get_native());
        return SindreData(blocks);
    }
    unsigned nblocks() const {
        auto *b = vtkMultiBlockDataSet::SafeDownCast(get_native());
        if (!b)
            throw std::invalid_argument("Not multiblock data");
        return b->GetNumberOfBlocks();
    }
    SindreData block(unsigned index) const {
        auto *b = vtkMultiBlockDataSet::SafeDownCast(get_native());
        if (!b || index >= b->GetNumberOfBlocks() || !b->GetBlock(index))
            throw std::out_of_range("Block index");
        return SindreData(b->GetBlock(index));
    }
    void set_data(const std::string &name, const Matrix &values, bool point = true) {
        if (name.empty() || values.rows() != (point ? npoints() : ncells()) || values.cols() < 1 ||
            values.cols() > std::numeric_limits<int>::max() || !values.allFinite())
            throw std::invalid_argument("Invalid data array");
        vtkNew<vtkDoubleArray> a;
        a->SetName(name.c_str());
        a->SetNumberOfComponents(int(values.cols()));
        a->SetNumberOfTuples(values.rows());
        for (Eigen::Index i = 0; i < values.rows(); ++i)
            for (int k = 0; k < values.cols(); ++k)
                a->SetComponent(i, k, values(i, k));
        if (point)
            dataset()->GetPointData()->AddArray(a);
        else
            dataset()->GetCellData()->AddArray(a);
    }
    Matrix get_data(const std::string &name, bool point = true) const {
        auto *a = require_array(name, point);
        Matrix values(a->GetNumberOfTuples(), a->GetNumberOfComponents());
        for (Eigen::Index i = 0; i < values.rows(); ++i)
            for (int k = 0; k < values.cols(); ++k)
                values(i, k) = a->GetComponent(i, k);
        return values;
    }
    template <class Filter, class Configure> SindreData filtered(Configure configure) const {
        vtkNew<Filter> filter;
        bool failed = false;
        vtkNew<vtkCallbackCommand> error;
        error->SetClientData(&failed);
        error->SetCallback([](vtkObject *, unsigned long, void *client, void *) {
            *static_cast<bool *>(client) = true;
        });
        filter->AddObserver(vtkCommand::ErrorEvent, error);
        filter->SetInputDataObject(get_native());
        configure(filter.GetPointer());
        filter->Update();
        if (failed || filter->GetErrorCode() || !filter->GetOutputDataObject(0))
            throw std::runtime_error("VTK data filter failed");
        return SindreData(filter->GetOutputDataObject(0));
    }
    vtkSmartPointer<vtkTrivialProducer> pipeline_source() const {
        SindreData copy(*this);
        auto source = vtkSmartPointer<vtkTrivialProducer>::New();
        source->SetOutput(copy.get_native());
        return source;
    }
    SindreMesh surface() const {
        dataset();
        auto result = filtered<vtkDataSetSurfaceFilter>([](auto *) {});
        return SindreMesh(vtkPolyData::SafeDownCast(result.get_native()));
    }
    SindreData contour(const std::string &scalar, const std::vector<double> &levels) const {
        require_array(scalar, true, 1);
        if (levels.empty())
            throw std::invalid_argument("Contour values empty");
        for (auto x : levels)
            if (!std::isfinite(x))
                throw std::invalid_argument("Nonfinite contour value");
        return filtered<vtkContourFilter>([&](auto *f) {
            f->SetInputArrayToProcess(0, 0, 0, association(true), scalar.c_str());
            f->SetNumberOfContours(int(levels.size()));
            for (std::size_t i = 0; i < levels.size(); ++i)
                f->SetValue(int(i), levels[i]);
        });
    }
    SindreData threshold(const std::string &scalar, double lower, double upper,
                         bool point = true) const {
        require_array(scalar, point, 1);
        if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper)
            throw std::invalid_argument("Invalid threshold");
        return filtered<vtkThreshold>([&](auto *f) {
            f->SetInputArrayToProcess(0, 0, 0, association(point), scalar.c_str());
            f->SetLowerThreshold(lower);
            f->SetUpperThreshold(upper);
            f->SetThresholdFunction(vtkThreshold::THRESHOLD_BETWEEN);
        });
    }
    SindreData clip_plane(const Eigen::Vector3d &origin, const Eigen::Vector3d &normal,
                          bool inside = false) const {
        dataset();
        if (!origin.allFinite() || !normal.allFinite() || normal.norm() == 0)
            throw std::invalid_argument("Invalid plane");
        vtkNew<vtkPlane> plane;
        plane->SetOrigin(origin.data());
        plane->SetNormal(normal.data());
        return filtered<vtkClipDataSet>([&](auto *f) {
            f->SetClipFunction(plane);
            f->SetInsideOut(inside);
        });
    }
    SindreData gradient(const std::string &name, bool point = true, bool vorticity = false,
                        bool divergence = false) const {
        auto *a = require_array(name, point);
        if ((vorticity || divergence) && a->GetNumberOfComponents() != 3)
            throw std::invalid_argument("Vorticity/divergence require a vector field");
        return filtered<vtkGradientFilter>([&](auto *f) {
            f->SetInputScalars(association(point), name.c_str());
            f->SetResultArrayName("Gradient");
            f->SetComputeVorticity(vorticity);
            f->SetComputeDivergence(divergence);
        });
    }
    SindreData warp_vector(const std::string &name, double scale = 1) const {
        require_array(name, true, 3);
        if (!std::isfinite(scale))
            throw std::invalid_argument("Invalid warp scale");
        return filtered<vtkWarpVector>([&](auto *f) {
            f->SetInputArrayToProcess(0, 0, 0, association(true), name.c_str());
            f->SetScaleFactor(scale);
        });
    }
    SindreData warp_scalar(const std::string &name, double scale = 1,
                           Eigen::Vector3d normal = Eigen::Vector3d::UnitZ()) const {
        require_array(name, true, 1);
        if (!std::isfinite(scale) || !normal.allFinite() || normal.norm() == 0)
            throw std::invalid_argument("Invalid scalar warp");
        return filtered<vtkWarpScalar>([&](auto *f) {
            f->SetInputArrayToProcess(0, 0, 0, association(true), name.c_str());
            f->SetScaleFactor(scale);
            f->UseNormalOn();
            f->SetNormal(normal.normalized().eval().data());
        });
    }
    SindreData point_to_cell_data() const {
        dataset();
        return filtered<vtkPointDataToCellData>([](auto *f) { f->PassPointDataOn(); });
    }
    SindreData cell_to_point_data() const {
        dataset();
        return filtered<vtkCellDataToPointData>([](auto *f) { f->PassCellDataOn(); });
    }
    SindreData probe(const SindreData &source) const {
        dataset();
        source.dataset();
        return filtered<vtkProbeFilter>([&](auto *f) { f->SetSourceData(source.dataset()); });
    }
    SindreData connected_regions(bool largest = false) const {
        dataset();
        return filtered<vtkConnectivityFilter>([&](auto *f) {
            if (largest)
                f->SetExtractionModeToLargestRegion();
            else
                f->SetExtractionModeToAllRegions();
            f->ColorRegionsOn();
        });
    }
    SindreData calculate(const std::string &expression, const std::vector<std::string> &variables,
                         const std::string &output = "Result", bool point = true) const {
        if (output.empty() || expression.empty())
            throw std::invalid_argument("Calculator expression/result empty");
        for (auto &name : variables) {
            require_array(name, point, 1);
            if (name.empty())
                throw std::invalid_argument("Empty calculator variable");
        }
        return filtered<vtkArrayCalculator>([&](auto *f) {
            if (point)
                f->SetAttributeTypeToPointData();
            else
                f->SetAttributeTypeToCellData();
            for (auto &name : variables)
                f->AddScalarArrayName(name.c_str());
            f->SetFunction(expression.c_str());
            f->SetResultArrayName(output.c_str());
        });
    }
    SindreData delaunay(bool three_dimensional = true) const {
        if (!vtkPolyData::SafeDownCast(get_native()))
            throw std::invalid_argument("Delaunay input must be point/polydata");
        if (three_dimensional)
            return filtered<vtkDelaunay3D>([](auto *) {});
        return filtered<vtkDelaunay2D>([](auto *) {});
    }
    SindreData tube(double radius = .1, int sides = 12) const {
        auto *p = vtkPolyData::SafeDownCast(get_native());
        if (!p || !p->GetNumberOfLines() || !std::isfinite(radius) || radius <= 0 || sides < 3)
            throw std::invalid_argument("Tube requires curves and valid radius/sides");
        return filtered<vtkTubeFilter>([&](auto *f) {
            f->SetRadius(radius);
            f->SetNumberOfSides(sides);
            f->CappingOn();
        });
    }
    SindreData glyph_vectors(const std::string &name, double scale = 1) const {
        require_array(name, true, 3);
        if (!std::isfinite(scale) || scale <= 0)
            throw std::invalid_argument("Invalid glyph scale");
        vtkNew<vtkArrowSource> arrow;
        return filtered<vtkGlyph3D>([&](auto *f) {
            f->SetSourceConnection(arrow->GetOutputPort());
            f->SetInputArrayToProcess(1, 0, 0, association(true), name.c_str());
            f->SetVectorModeToUseVector();
            f->SetScaleModeToScaleByVector();
            f->SetScaleFactor(scale);
            f->OrientOn();
        });
    }
    SindreData streamlines(const std::string &vectors, const Vertices &seeds,
                           double length = 10) const {
        require_array(vectors, true, 3);
        if (!seeds.rows() || !std::isfinite(length) || length <= 0)
            throw std::invalid_argument("Invalid streamline seeds/length");
        vtkNew<vtkPolyData> source;
        source->SetPoints(make_points(seeds));
        return filtered<vtkStreamTracer>([&](auto *f) {
            f->SetSourceData(source);
            f->SetInputArrayToProcess(0, 0, 0, association(true), vectors.c_str());
            f->SetMaximumPropagation(length);
            f->SetInitialIntegrationStep(.1);
            f->SetIntegrationDirectionToBoth();
            f->SetIntegratorTypeToRungeKutta45();
        });
    }
    static SindreData load(const std::filesystem::path &path) {
        if (!std::filesystem::is_regular_file(path))
            throw std::invalid_argument("VTK data file missing");
        vtkNew<vtkXMLGenericDataObjectReader> reader;
        const auto filename = path_to_utf8(path);
        reader->SetFileName(filename.c_str());
        reader->Update();
        if (reader->GetErrorCode() || !reader->GetOutput())
            throw std::runtime_error("VTK XML read failed");
        return SindreData(reader->GetOutput());
    }
    void save(const std::filesystem::path &path) const {
        auto extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (vtkMultiBlockDataSet::SafeDownCast(get_native())) {
            if (extension != ".vtm")
                throw std::invalid_argument("Multiblock requires .vtm");
            vtkNew<vtkXMLMultiBlockDataWriter> writer;
            const auto filename = path_to_utf8(path);
            writer->SetFileName(filename.c_str());
            writer->SetInputDataObject(get_native());
            if (!writer->Write() || writer->GetErrorCode())
                throw std::runtime_error("Multiblock write failed");
            return;
        }
        const auto t = type();
        const std::string expected = t == "vtkPolyData"           ? ".vtp"
                                     : t == "vtkImageData"        ? ".vti"
                                     : t == "vtkStructuredGrid"   ? ".vts"
                                     : t == "vtkRectilinearGrid"  ? ".vtr"
                                     : t == "vtkUnstructuredGrid" ? ".vtu"
                                                                  : "";
        if (expected.empty() || extension != expected)
            throw std::invalid_argument("Wrong XML extension for data type");
        vtkNew<vtkXMLDataSetWriter> writer;
        const auto filename = path_to_utf8(path);
        writer->SetFileName(filename.c_str());
        writer->SetInputData(dataset());
        if (!writer->Write() || writer->GetErrorCode())
            throw std::runtime_error("VTK data write failed");
    }
};
#undef SindreData
#undef SindreMesh
} // namespace sindre::utils_3d::detail::legacy
