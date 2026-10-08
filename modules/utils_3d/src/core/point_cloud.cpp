#include <sindre/utils_3d/types.h>

#include "log.h"

#include <cmath>
#include <limits>

namespace sindre::utils_3d {

namespace {
::sindre::general::Result<void> failure(std::errc code, const char *message,
                                         const char *context) {
    detail::logging::error(context, message);
    return ::sindre::general::Result<void>::failure(
        ::sindre::general::Error::make(code, message, context));
}
}

::sindre::general::Result<void> PointCloud::validate() const {
    if (points.cols() != 3 || !points.allFinite())
        return failure(std::errc::invalid_argument,
                       "PointCloud points must be a finite N x 3 matrix",
                       "utils_3d.point_cloud.points");

    const auto count = points.rows();
    if (normals && (normals->rows() != count || normals->cols() != 3 ||
                    !normals->allFinite()))
        return failure(std::errc::invalid_argument,
                       "PointCloud normals must match points and be finite",
                       "utils_3d.point_cloud.normals");
    if (colors && (colors->rows() != count || colors->cols() != 3))
        return failure(std::errc::invalid_argument,
                       "PointCloud colors must match points and have three channels",
                       "utils_3d.point_cloud.colors");
    if (intensity && intensity->size() != count)
        return failure(std::errc::invalid_argument,
                       "PointCloud intensity must match points",
                       "utils_3d.point_cloud.intensity");
    if (intensity) {
        for (Eigen::Index i = 0; i < intensity->size(); ++i)
            if (!std::isfinite((*intensity)[i]))
                return failure(std::errc::invalid_argument,
                               "PointCloud intensity must be finite",
                               "utils_3d.point_cloud.intensity");
    }
    if (labels && labels->size() != count)
        return failure(std::errc::invalid_argument,
                       "PointCloud labels must match points",
                       "utils_3d.point_cloud.labels");
    if (validity && validity->size() != count)
        return failure(std::errc::invalid_argument,
                       "PointCloud validity must match points",
                       "utils_3d.point_cloud.validity");
    return ::sindre::general::Result<void>::success();
}

} // namespace sindre::utils_3d
