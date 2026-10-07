#include <sindre/utils_3d.h>
#include "vtk_coverage.h"
#include <iostream>

using namespace sindre::utils_3d;
namespace math = sindre::math;
static void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, "Invalid data input accepted");
}
int main() {
    try {
        const std::array<int, 3> dims{8, 8, 8};
        Matrix values(512, 1);
        Vertices coordinates(512, 3);
        Matrix vector(512, 3);
        for (int z = 0; z < 8; ++z)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    const int i = x + 8 * (y + 8 * z);
                    values(i, 0) = x + y + z;
                    coordinates.row(i) << x, y, z;
                    vector.row(i) << 1, 0, 0;
                }
        SindreImage image(values, dims);
        image.set_data("velocity", vector);
        Matrix updated_vector = vector * 2;
        image.set_data("velocity", updated_vector);
        check(image.get_data("velocity").isApprox(updated_vector),
              "Replacing an existing dataset attribute");
        check(image.dimensions() == dims && image.values().isApprox(values),
              "Image array roundtrip");
        SindreImage constant(Matrix::Constant(512, 1, 3), dims);
        check(constant.gaussian().values().isApprox(Matrix::Constant(512, 1, 3)),
              "Gaussian preserves a constant field");
        check(image.median().dimensions() == dims, "Median geometry");
        auto binary = image.threshold(0, 1);
        check(binary.values().sum() == 4, "Closed image threshold interval");
        check(binary.morphology().values().sum() >= binary.values().sum(), "Binary dilation");
        check(binary.morphology(false).values().sum() <= binary.values().sum(), "Binary erosion");
        check(binary.connected_regions().values().maxCoeff() > 0, "Image connected labels");
        auto normalized = image.normalize();
        check(std::abs(normalized.values().minCoeff()) < 1e-12 &&
                  std::abs(normalized.values().maxCoeff() - 1) < 1e-12,
              "Image normalization range");
        check(image.cast(VTK_FLOAT).image()->GetScalarType() == VTK_FLOAT,
              "Scalar type conversion");
        check(image.crop({1, 4, 1, 4, 1, 4}).dimensions() == std::array<int, 3>{4, 4, 4},
              "Image crop");
        check(image.pad({-1, 8, -1, 8, -1, 8}).dimensions() == std::array<int, 3>{10, 10, 10},
              "Image padding");
        check(std::abs(image.flip(0).values()(0, 0) - 7) < 1e-12, "Image flip values");
        check(image.resample(math::Vector3::Constant(.5)).npoints() > 512, "Image resampling");
        check(image.reslice(math::Matrix4::Identity(), dims).values().isApprox(values),
              "Identity reslice");
        auto gradient = image.gradient();
        check(gradient.values().cols() == 3 &&
                  gradient.values().row(3 + 8 * (3 + 8 * 3)).isApprox(math::eigen::RowVector3d::Ones()),
              "Image interior gradient");
        check(image.gradient(true).values().cols() == 1, "Gradient magnitude");
        check(std::abs(image.laplacian().values()(3 + 8 * (3 + 8 * 3), 0)) < 1e-12,
              "Linear field Laplacian");
        auto structured = SindreData::structured_grid(coordinates, dims);
        structured.set_data("q", values);
        structured.set_data("velocity", vector);
        auto field_gradient = structured.gradient("q");
        check(field_gradient.get_data("Gradient").cols() == 3, "Dataset gradient");
        auto derivatives = structured.gradient("velocity", true, true, true);
        check(derivatives.get_data("Divergence").cwiseAbs().maxCoeff() < 1e-12,
              "Constant field divergence");
        check(derivatives.get_data("Vorticity").cwiseAbs().maxCoeff() < 1e-12,
              "Constant field curl");
        check(structured.contour("q", {6.5}).ncells() > 0, "Scientific contour");
        check(structured.threshold("q", 0, 6).ncells() > 0, "Scientific threshold");
        check(structured.clip_plane(math::Vector3(3, 0, 0), math::Vector3::UnitX()).ncells() >
                  0,
              "Dataset clipping");
        check(structured.surface().nfaces() > 0, "Dataset surface extraction");
        check(structured.warp_vector("velocity", 2)
                  .points()
                  .row(0)
                  .isApprox(math::eigen::RowVector3d(2, 0, 0)),
              "Vector warp");
        check(structured.warp_scalar("q", 2).points().row(1).isApprox(math::eigen::RowVector3d(1, 0, 2)),
              "Scalar warp");
        auto cells = structured.point_to_cell_data();
        check(cells.get_data("q", false).rows() == structured.ncells(), "Point-to-cell averaging");
        check(cells.cell_to_point_data().get_data("q").rows() == 512, "Cell-to-point averaging");
        check(structured.calculate("q*q", {"q"}, "square")
                  .get_data("square")
                  .isApprox(values.array().square().matrix()),
              "Array calculator");
        auto sampled = SindreData::point_cloud(coordinates).probe(structured);
        check(sampled.get_data("q").isApprox(values), "Field probing");
        check(structured.connected_regions().ncells() == structured.ncells(),
              "Connected dataset regions");
        Vertices seeds(1, 3);
        seeds << 3, 3, 3;
        check(structured.streamlines("velocity", seeds, 3).ncells() > 0, "Streamline integration");
        check(SindreData::point_cloud(coordinates.topRows(4)).type() == "vtkPolyData",
              "Point dataset");
        Vertices tetra(4, 3);
        tetra << 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1;
        math::Matrix<std::int64_t, math::eigen::Dynamic, 4> tc(1, 4);
        tc << 0, 1, 2, 3;
        auto unstructured = SindreData::tetrahedra(tetra, tc);
        check(unstructured.ncells() == 1, "Tetrahedral dataset");
        check(SindreData::point_cloud(tetra).delaunay().ncells() == 1, "3D Delaunay tetrahedron");
        auto planar = tetra.topRows(3).eval();
        check(SindreData::point_cloud(planar).delaunay(false).ncells() == 1,
              "2D Delaunay triangle");
        auto curve = SindreData::polyline(tetra.topRows(3));
        check(curve.tube().ncells() > 0, "Tube generation");
        auto glyph_points = SindreData::point_cloud(tetra);
        Matrix gv(4, 3);
        gv.rowwise() = math::eigen::RowVector3d(1, 0, 0);
        glyph_points.set_data("v", gv);
        check(glyph_points.glyph_vectors("v").ncells() > 0, "Vector arrow glyphs");
        math::VectorXd axis = math::VectorXd::LinSpaced(3, 0, 2);
        auto rectilinear = SindreData::rectilinear_grid(axis, axis, axis);
        check(rectilinear.npoints() == 27 && rectilinear.ncells() == 8, "Rectilinear grid");
        auto group = SindreData::blocks({image, structured, unstructured, rectilinear});
        check(group.nblocks() == 4 && group.block(2).ncells() == 1, "Composite data");
        const auto folder = std::filesystem::current_path() / "vtk-data-roundtrip";
        std::filesystem::create_directories(folder);
        for (auto pair :
             std::vector<std::pair<SindreData, std::string>>{{image, "image.vti"},
                                                             {structured, "structured.vts"},
                                                             {unstructured, "tetra.vtu"},
                                                             {rectilinear, "rectilinear.vtr"},
                                                             {group, "blocks.vtm"}}) {
            const auto path = folder / pair.second;
            pair.first.save(path);
            auto loaded = SindreData::load(path);
            check(loaded.type() == pair.first.type(), "XML data type roundtrip");
        }
        check(SindreData::load(folder / "image.vti").get_data("Scalars").isApprox(values),
              "XML image values");
        const auto chinese_folder = std::filesystem::current_path() / "vtk-数据-中文";
        std::filesystem::create_directories(chinese_folder);
        const auto chinese_path = chinese_folder / "图像-中文.vti";
        image.save(chinese_path);
        check(SindreData::load(chinese_path).get_data("Scalars").isApprox(values),
              "Chinese XML data path roundtrip");
        const auto uppercase_path = chinese_folder / "图像-大写.VTI";
        image.save(uppercase_path);
        check(SindreData::load(uppercase_path).get_data("Scalars").isApprox(values),
              "Uppercase XML extension roundtrip");
        std::filesystem::remove_all(folder);
        std::filesystem::remove_all(chinese_folder);
        rejects([&] { image.crop({-1, 4, 0, 7, 0, 7}); });
        rejects([&] { image.morphology(); });
        rejects([&] { structured.gradient("missing"); });
        rejects([&] { group.surface(); });
        rejects([&] { image.save("wrong.vtu"); });
#if defined(SINDRE_UTILS_3D_SHOW)
        ShowOptions view;
        view.offscreen = true;
        view.interactive = false;
        view.axes = false;
        view.width = 320;
        view.height = 240;
        {
            ShowMesh volume(view);
            volume.volume(image).reset_camera().screenshot("show_volume_preview.PNG");
            ShowMesh slice(view);
            slice.image_slice(image.shift_scale(0, 10).cast(), 2, 3)
                .reset_camera()
                .screenshot("show_slice_preview.PNG");
            ShowMesh dataset_view(view);
            dataset_view.add(unstructured);
            dataset_view.reset_camera().show(false);
        }
        std::vector<double> x{0, 1, 2, 3}, y{0, 1, 4, 9};
        ShowPlot plot(view);
        plot.add(x, y, "line")
            .add(x, y, "points", PlotKind::points)
            .add(x, y, "bars", PlotKind::bars);
        plot.title("VTK chart").axis_titles("x", "y").show(false);
        plot.screenshot("show-图表.PNG");
        vtk_coverage("vtk_coverage_volume_plot.json", {88, 89, 90});
#endif
        std::cout << "VTK data/image/scientific tests passed\n";
        vtk_coverage("vtk_coverage_data.json",
                     {2,  3,  4,  5,  6,  7,  8,  20, 21, 22, 23, 24, 46, 47, 48,
                      49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
                      64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76});
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
