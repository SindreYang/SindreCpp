#include <sindre/utils_3d/image.h>

#include "core/image.h"
#include <stdexcept>

namespace sindre::utils_3d::core {

namespace {
Image wrap(detail::legacy::Image value) { return Image(value.image()); }
vtkSmartPointer<vtkImageData> make_image(const Matrix &values, std::array<int, 3> dimensions,
                                          const Eigen::Vector3d &spacing,
                                          const Eigen::Vector3d &origin) {
    detail::legacy::Image value(values, dimensions, spacing, origin);
    auto result = vtkSmartPointer<vtkImageData>::New();
    result->DeepCopy(value.image());
    return result;
}
}

Image::Image(vtkImageData *image) : Data(image) {}
Image::Image(const Data &data) : Data(data.get_native()) { (void)this->image(); }
Image::Image(const Matrix &values, std::array<int, 3> dimensions,
             ::sindre::math::Vector3 spacing, ::sindre::math::Vector3 origin)
    : Data(make_image(values, dimensions, spacing, origin)) {}

vtkImageData *Image::image() const {
    auto *result = vtkImageData::SafeDownCast(get_native());
    if (!result)
        throw std::logic_error("Not image data");
    return result;
}
std::array<int, 3> Image::dimensions() const { return detail::legacy::Image(image()).dimensions(); }
std::array<int, 6> Image::extent() const { return detail::legacy::Image(image()).extent(); }
::sindre::math::Vector3 Image::spacing() const { return detail::legacy::Image(image()).spacing(); }
::sindre::math::Vector3 Image::origin() const { return detail::legacy::Image(image()).origin(); }
Matrix Image::values() const { return detail::legacy::Image(image()).values(); }
Image Image::gaussian(::sindre::math::Vector3 sigma) const {
    return wrap(detail::legacy::Image(image()).gaussian(sigma));
}
Image Image::median(std::array<int, 3> kernel) const { return wrap(detail::legacy::Image(image()).median(kernel)); }
Image Image::threshold(double lower, double upper, double inside, double outside) const {
    return wrap(detail::legacy::Image(image()).threshold(lower, upper, inside, outside));
}
Image Image::shift_scale(double shift, double scale) const {
    return wrap(detail::legacy::Image(image()).shift_scale(shift, scale));
}
Image Image::normalize(double lower, double upper) const {
    return wrap(detail::legacy::Image(image()).normalize(lower, upper));
}
Image Image::cast(int vtk_scalar_type) const {
    return wrap(detail::legacy::Image(image()).cast(vtk_scalar_type));
}
Image Image::crop(std::array<int, 6> box, std::array<int, 3> stride) const {
    return wrap(detail::legacy::Image(image()).crop(box, stride));
}
Image Image::pad(std::array<int, 6> box, double value) const {
    return wrap(detail::legacy::Image(image()).pad(box, value));
}
Image Image::flip(int axis) const { return wrap(detail::legacy::Image(image()).flip(axis)); }
Image Image::resample(::sindre::math::Vector3 output_spacing,
                      ImageInterpolation interpolation) const {
    return wrap(detail::legacy::Image(image()).resample(output_spacing,
                                                         static_cast<detail::legacy::ImageInterpolation>(interpolation)));
}
Image Image::reslice(const ::sindre::math::Matrix4 &axes, std::array<int, 3> dimensions,
                     ::sindre::math::Vector3 spacing, ::sindre::math::Vector3 origin,
                     ImageInterpolation interpolation) const {
    return wrap(detail::legacy::Image(image()).reslice(
        axes, dimensions, spacing, origin, static_cast<detail::legacy::ImageInterpolation>(interpolation)));
}
Image Image::gradient(bool magnitude) const { return wrap(detail::legacy::Image(image()).gradient(magnitude)); }
Image Image::laplacian() const { return wrap(detail::legacy::Image(image()).laplacian()); }
Image Image::morphology(bool dilate, std::array<int, 3> kernel) const {
    return wrap(detail::legacy::Image(image()).morphology(dilate, kernel));
}
Image Image::connected_regions(bool largest) const {
    return wrap(detail::legacy::Image(image()).connected_regions(largest));
}

} // namespace sindre::utils_3d::core
