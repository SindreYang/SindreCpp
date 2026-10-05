#pragma once
#include "data.hpp"
#include <vtkExtractVOI.h>
#include <vtkImageCast.h>
#include <vtkImageConnectivityFilter.h>
#include <vtkImageConstantPad.h>
#include <vtkImageData.h>
#include <vtkImageDilateErode3D.h>
#include <vtkImageFlip.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkImageGradient.h>
#include <vtkImageLaplacian.h>
#include <vtkImageMagnitude.h>
#include <vtkImageMathematics.h>
#include <vtkImageMedian3D.h>
#include <vtkImageResample.h>
#include <vtkImageReslice.h>
#include <vtkImageShiftScale.h>
#include <vtkImageThreshold.h>
#include <vtkMatrix4x4.h>

namespace sindrecpp::utils3d {
enum class ImageInterpolation { nearest, linear, cubic };
class SindreImage : public SindreData {
    template <class Filter, class Configure> SindreImage image_filter(Configure configure) const {
        auto result = filtered<Filter>(configure);
        auto *image = vtkImageData::SafeDownCast(result.get_native());
        if (!image)
            throw std::runtime_error("Image filter returned nonimage data");
        return SindreImage(image);
    }
    static void finite3(const Eigen::Vector3d &v) {
        if (!v.allFinite())
            throw std::invalid_argument("Image geometry must be finite");
    }

  public:
    explicit SindreImage(vtkImageData *image) : SindreData(image) {}
    explicit SindreImage(const SindreData &data)
        : SindreImage(vtkImageData::SafeDownCast(data.get_native())) {}
    // Rows use VTK's x-fastest ordering: x + nx*(y + ny*z). Columns are channels.
    SindreImage(const Matrix &values, std::array<int, 3> dimensions,
                Eigen::Vector3d spacing = Eigen::Vector3d::Ones(),
                Eigen::Vector3d origin = Eigen::Vector3d::Zero())
        : SindreData(vtkSmartPointer<vtkImageData>::New()) {
        finite3(spacing);
        finite3(origin);
        if ((spacing.array() <= 0).any() || grid_size(dimensions) != std::size_t(values.rows()) ||
            values.cols() < 1 || values.cols() > std::numeric_limits<int>::max() ||
            !values.allFinite())
            throw std::invalid_argument("Invalid image dimensions, values or spacing");
        image()->SetDimensions(dimensions.data());
        image()->SetOrigin(origin.data());
        image()->SetSpacing(spacing.data());
        set_data("Scalars", values);
        image()->GetPointData()->SetActiveScalars("Scalars");
    }
    vtkImageData *image() const {
        auto *result = vtkImageData::SafeDownCast(get_native());
        if (!result)
            throw std::logic_error("Not image data");
        return result;
    }
    std::array<int, 3> dimensions() const {
        std::array<int, 3> d;
        image()->GetDimensions(d.data());
        return d;
    }
    std::array<int, 6> extent() const {
        std::array<int, 6> d;
        image()->GetExtent(d.data());
        return d;
    }
    Eigen::Vector3d spacing() const {
        double p[3];
        image()->GetSpacing(p);
        return Eigen::Vector3d(p[0], p[1], p[2]);
    }
    Eigen::Vector3d origin() const {
        double p[3];
        image()->GetOrigin(p);
        return Eigen::Vector3d(p[0], p[1], p[2]);
    }
    Matrix values() const {
        auto *a = image()->GetPointData()->GetScalars();
        if (!a)
            throw std::out_of_range("Image scalar data missing");
        Matrix result(a->GetNumberOfTuples(), a->GetNumberOfComponents());
        for (Eigen::Index i = 0; i < result.rows(); ++i)
            for (int k = 0; k < result.cols(); ++k)
                result(i, k) = a->GetComponent(i, k);
        return result;
    }
    SindreImage gaussian(Eigen::Vector3d sigma = Eigen::Vector3d::Ones()) const {
        finite3(sigma);
        if ((sigma.array() <= 0).any())
            throw std::invalid_argument("Gaussian sigma must be positive voxel units");
        return image_filter<vtkImageGaussianSmooth>([&](auto *f) {
            f->SetStandardDeviations(sigma.data());
            f->SetRadiusFactors(3, 3, 3);
        });
    }
    SindreImage median(std::array<int, 3> kernel = {3, 3, 3}) const {
        for (int x : kernel)
            if (x < 1 || !(x % 2))
                throw std::invalid_argument("Median kernel must have positive odd sizes");
        return image_filter<vtkImageMedian3D>(
            [&](auto *f) { f->SetKernelSize(kernel[0], kernel[1], kernel[2]); });
    }
    SindreImage threshold(double lower, double upper, double inside = 1, double outside = 0) const {
        if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper ||
            !std::isfinite(inside) || !std::isfinite(outside))
            throw std::invalid_argument("Invalid image threshold");
        return image_filter<vtkImageThreshold>([&](auto *f) {
            f->ThresholdBetween(lower, upper);
            f->ReplaceInOn();
            f->ReplaceOutOn();
            f->SetInValue(inside);
            f->SetOutValue(outside);
            f->SetOutputScalarTypeToDouble();
        });
    }
    SindreImage shift_scale(double shift, double scale) const {
        if (!std::isfinite(shift) || !std::isfinite(scale))
            throw std::invalid_argument("Nonfinite image shift/scale");
        return image_filter<vtkImageShiftScale>([&](auto *f) {
            f->SetShift(shift);
            f->SetScale(scale);
            f->SetOutputScalarTypeToDouble();
        });
    }
    SindreImage normalize(double lower = 0, double upper = 1) const {
        if (!std::isfinite(lower) || !std::isfinite(upper) || lower >= upper)
            throw std::invalid_argument("Invalid normalization range");
        const auto v = values();
        if (!v.size())
            throw std::invalid_argument("Empty image");
        const double minimum = v.minCoeff(), maximum = v.maxCoeff();
        if (maximum == minimum)
            return shift_scale(-minimum, 0).shift_scale(lower, 1);
        return shift_scale(-minimum, (upper - lower) / (maximum - minimum)).shift_scale(lower, 1);
    }
    SindreImage cast(int vtk_scalar_type = VTK_UNSIGNED_CHAR) const {
        if (vtk_scalar_type != VTK_UNSIGNED_CHAR && vtk_scalar_type != VTK_SHORT &&
            vtk_scalar_type != VTK_UNSIGNED_SHORT && vtk_scalar_type != VTK_INT &&
            vtk_scalar_type != VTK_FLOAT && vtk_scalar_type != VTK_DOUBLE)
            throw std::invalid_argument("Unsupported image scalar type");
        return image_filter<vtkImageCast>([&](auto *f) {
            f->SetOutputScalarType(vtk_scalar_type);
            f->ClampOverflowOn();
        });
    }
    SindreImage crop(std::array<int, 6> box, std::array<int, 3> stride = {1, 1, 1}) const {
        const auto old = extent();
        for (int k = 0; k < 3; ++k)
            if (box[2 * k] < old[2 * k] || box[2 * k + 1] > old[2 * k + 1] ||
                box[2 * k] > box[2 * k + 1] || stride[k] < 1)
                throw std::invalid_argument("Invalid image crop/stride");
        return image_filter<vtkExtractVOI>([&](auto *f) {
            f->SetVOI(box.data());
            f->SetSampleRate(stride.data());
        });
    }
    SindreImage pad(std::array<int, 6> box, double value = 0) const {
        if (!std::isfinite(value))
            throw std::invalid_argument("Invalid padding value");
        const auto old = extent();
        for (int k = 0; k < 3; ++k)
            if (box[2 * k] > old[2 * k] || box[2 * k + 1] < old[2 * k + 1])
                throw std::invalid_argument("Pad extent must contain original extent");
        return image_filter<vtkImageConstantPad>([&](auto *f) {
            f->SetOutputWholeExtent(box.data());
            f->SetConstant(value);
        });
    }
    SindreImage flip(int axis) const {
        if (axis < 0 || axis > 2)
            throw std::invalid_argument("Flip axis must be 0..2");
        return image_filter<vtkImageFlip>([&](auto *f) { f->SetFilteredAxis(axis); });
    }
    SindreImage resample(Eigen::Vector3d output_spacing,
                         ImageInterpolation interpolation = ImageInterpolation::linear) const {
        finite3(output_spacing);
        if ((output_spacing.array() <= 0).any())
            throw std::invalid_argument("Output spacing must be positive");
        return image_filter<vtkImageResample>([&](auto *f) {
            for (int k = 0; k < 3; ++k)
                f->SetAxisOutputSpacing(k, output_spacing[k]);
            if (interpolation == ImageInterpolation::nearest)
                f->SetInterpolationModeToNearestNeighbor();
            else if (interpolation == ImageInterpolation::cubic)
                f->SetInterpolationModeToCubic();
            else
                f->SetInterpolationModeToLinear();
        });
    }
    SindreImage reslice(const Eigen::Matrix4d &axes, std::array<int, 3> dimensions,
                        Eigen::Vector3d spacing = Eigen::Vector3d::Ones(),
                        Eigen::Vector3d origin = Eigen::Vector3d::Zero(),
                        ImageInterpolation interpolation = ImageInterpolation::linear) const {
        grid_size(dimensions);
        finite3(spacing);
        finite3(origin);
        if (!axes.allFinite() || !axes.row(3).isApprox(Eigen::RowVector4d(0, 0, 0, 1)) ||
            (spacing.array() <= 0).any() ||
            std::abs(axes.topLeftCorner<3, 3>().determinant()) < 1e-15)
            throw std::invalid_argument("Invalid reslice geometry");
        vtkNew<vtkMatrix4x4> matrix;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                matrix->SetElement(i, j, axes(i, j));
        return image_filter<vtkImageReslice>([&](auto *f) {
            f->SetResliceAxes(matrix);
            f->SetOutputSpacing(spacing.data());
            f->SetOutputOrigin(origin.data());
            f->SetOutputExtent(0, dimensions[0] - 1, 0, dimensions[1] - 1, 0, dimensions[2] - 1);
            if (interpolation == ImageInterpolation::nearest)
                f->SetInterpolationModeToNearestNeighbor();
            else if (interpolation == ImageInterpolation::cubic)
                f->SetInterpolationModeToCubic();
            else
                f->SetInterpolationModeToLinear();
        });
    }
    SindreImage gradient(bool magnitude = false) const {
        auto v = image_filter<vtkImageGradient>([](auto *f) { f->SetDimensionality(3); });
        if (magnitude)
            return v.image_filter<vtkImageMagnitude>([](auto *) {});
        return v;
    }
    SindreImage laplacian() const {
        return image_filter<vtkImageLaplacian>([](auto *f) { f->SetDimensionality(3); });
    }
    SindreImage morphology(bool dilate = true, std::array<int, 3> kernel = {3, 3, 3}) const {
        for (int x : kernel)
            if (x < 1 || !(x % 2))
                throw std::invalid_argument("Morphology kernel needs positive odd sizes");
        const auto v = values();
        if (v.cols() != 1 || ((v.array() != 0) && (v.array() != 1)).any())
            throw std::invalid_argument("Morphology requires a binary 0/1 scalar image");
        return image_filter<vtkImageDilateErode3D>([&](auto *f) {
            f->SetKernelSize(kernel[0], kernel[1], kernel[2]);
            f->SetDilateValue(dilate ? 1 : 0);
            f->SetErodeValue(dilate ? 0 : 1);
        });
    }
    SindreImage connected_regions(bool largest = false) const {
        if (values().cols() != 1)
            throw std::invalid_argument("Image connectivity requires scalar input");
        return image_filter<vtkImageConnectivityFilter>([&](auto *f) {
            f->SetScalarRange(.5, std::numeric_limits<double>::max());
            if (largest)
                f->SetExtractionModeToLargestRegion();
            else
                f->SetExtractionModeToAllRegions();
        });
    }
};
} // namespace sindrecpp::utils3d
