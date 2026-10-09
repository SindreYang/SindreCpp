#include <sindre/utils_2d/algorithms.h>
#include "../private/native.h"

#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/optflow.hpp>
#include <opencv2/video.hpp>
#if defined(SINDRE_OPENCV_HAS_XFEATURES2D)
#include <opencv2/xfeatures2d.hpp>
#endif

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace sindre::utils_2d {
namespace {

template <class Function>
auto capture_result(const char *context, Function &&function) -> decltype(function()) {
    using Return = decltype(function());
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        return function();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const cv::Exception &error) {
        return Return::failure(std::make_error_code(std::errc::invalid_argument), error.what(), context);
    } catch (const std::exception &error) {
        return Return::failure(std::make_error_code(std::errc::io_error), error.what(), context);
    } catch (...) {
        return Return::failure(std::make_error_code(std::errc::io_error), "2D algorithm failed", context);
    }
#endif
}

bool valid_kernel(Size value) {
    return value.width > 0 && value.height > 0 && value.width % 2 == 1 && value.height % 2 == 1;
}

Result<Image> to_gray(const Image &image, const char *context) {
    if (image.channels == 1) return Result<Image>::success(image);
    if (image.channels == 3) return convert_color(image, cv::COLOR_BGR2GRAY);
    if (image.channels == 4) return convert_color(image, cv::COLOR_BGRA2GRAY);
    return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                  "Expected a grayscale, BGR or BGRA image", context);
}

Matrix from_native_matrix(const cv::Mat &value) {
    Matrix result{value.rows, value.cols, std::vector<double>(static_cast<std::size_t>(value.rows) * value.cols)};
    cv::Mat converted;
    value.convertTo(converted, CV_64F);
    for (int row = 0; row < value.rows; ++row)
        for (int column = 0; column < value.cols; ++column)
            result.values[static_cast<std::size_t>(row) * value.cols + column] = converted.at<double>(row, column);
    return result;
}

cv::Mat to_native_matrix(const Matrix &value) {
    cv::Mat result(value.rows, value.columns, CV_64F);
    for (int row = 0; row < value.rows; ++row)
        for (int column = 0; column < value.columns; ++column)
            result.at<double>(row, column) = value.at(row, column);
    return result;
}

float iou(const Rect2f &a, const Rect2f &b) {
    const auto left = std::max(a.x, b.x);
    const auto top = std::max(a.y, b.y);
    const auto right = std::min(a.x + a.width, b.x + b.width);
    const auto bottom = std::min(a.y + a.height, b.y + b.height);
    const auto intersection = std::max(0.0f, right - left) * std::max(0.0f, bottom - top);
    const auto area = a.width * a.height + b.width * b.height - intersection;
    return area > 0.0f ? intersection / area : 0.0f;
}

} // namespace

Result<Image> apply_blur(const Image &image, BlurAlgorithm algorithm, Size kernel, double sigma) {
    return capture_result("apply_blur", [&] {
        auto valid = validate_image(image);
        if (!valid) return Result<Image>::failure(valid.error());
        if (!valid_kernel(kernel))
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Blur kernel dimensions must be positive odd values", "apply_blur");
        cv::Mat result;
        auto native = detail::to_native(image);
        switch (algorithm) {
        case BlurAlgorithm::gaussian: cv::GaussianBlur(native, result, detail::to_native(kernel), sigma); break;
        case BlurAlgorithm::median: cv::medianBlur(native, result, kernel.width); break;
        case BlurAlgorithm::bilateral: cv::bilateralFilter(native, result, kernel.width,
                                                             sigma > 0.0 ? sigma : 75.0,
                                                             sigma > 0.0 ? sigma : 75.0); break;
        }
        return Result<Image>::success(detail::from_native(result));
    });
}

Result<Image> threshold_image(const Image &image, ThresholdAlgorithm algorithm,
                              double threshold, double max_value, int block_size,
                              double constant) {
    return capture_result("threshold_image", [&] {
        auto gray = to_gray(image, "threshold_image");
        if (!gray) return Result<Image>::failure(gray.error());
        if (!std::isfinite(threshold) || !std::isfinite(max_value) || max_value <= 0.0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Threshold values are invalid", "threshold_image");
        cv::Mat result;
        if (algorithm == ThresholdAlgorithm::adaptive_mean || algorithm == ThresholdAlgorithm::adaptive_gaussian) {
            if (!valid_kernel({block_size, block_size}) || block_size <= 1)
                return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                              "Adaptive threshold block size is invalid", "threshold_image");
            const auto method = algorithm == ThresholdAlgorithm::adaptive_mean
                                    ? cv::ADAPTIVE_THRESH_MEAN_C : cv::ADAPTIVE_THRESH_GAUSSIAN_C;
            cv::adaptiveThreshold(detail::to_native(gray.value()), result, max_value, method,
                                  cv::THRESH_BINARY, block_size, constant);
        } else {
            int type = cv::THRESH_BINARY;
            switch (algorithm) {
            case ThresholdAlgorithm::binary_inverse: type = cv::THRESH_BINARY_INV; break;
            case ThresholdAlgorithm::trunc: type = cv::THRESH_TRUNC; break;
            case ThresholdAlgorithm::to_zero: type = cv::THRESH_TOZERO; break;
            case ThresholdAlgorithm::to_zero_inverse: type = cv::THRESH_TOZERO_INV; break;
            case ThresholdAlgorithm::otsu: type = cv::THRESH_BINARY | cv::THRESH_OTSU; break;
            case ThresholdAlgorithm::triangle: type = cv::THRESH_BINARY | cv::THRESH_TRIANGLE; break;
            default: break;
            }
            cv::threshold(detail::to_native(gray.value()), result, threshold, max_value, type);
        }
        return Result<Image>::success(detail::from_native(result));
    });
}

Result<Image> apply_morphology(const Image &image, MorphologyOperation operation,
                               Size kernel, int iterations, int shape) {
    return capture_result("apply_morphology", [&] {
        auto valid = validate_image(image);
        if (!valid) return Result<Image>::failure(valid.error());
        if (!valid_kernel(kernel) || iterations <= 0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Morphology kernel or iterations are invalid", "apply_morphology");
        int code = cv::MORPH_ERODE;
        switch (operation) {
        case MorphologyOperation::erode: code = cv::MORPH_ERODE; break;
        case MorphologyOperation::dilate: code = cv::MORPH_DILATE; break;
        case MorphologyOperation::open: code = cv::MORPH_OPEN; break;
        case MorphologyOperation::close: code = cv::MORPH_CLOSE; break;
        case MorphologyOperation::gradient: code = cv::MORPH_GRADIENT; break;
        case MorphologyOperation::top_hat: code = cv::MORPH_TOPHAT; break;
        case MorphologyOperation::black_hat: code = cv::MORPH_BLACKHAT; break;
        }
        cv::Mat result;
        cv::morphologyEx(detail::to_native(image), result, code,
                         cv::getStructuringElement(shape, detail::to_native(kernel)), {-1, -1}, iterations);
        return Result<Image>::success(detail::from_native(result));
    });
}

Result<Image> detect_edges(const Image &image, EdgeAlgorithm algorithm,
                           double low_threshold, double high_threshold, int aperture) {
    return capture_result("detect_edges", [&] {
        auto gray = to_gray(image, "detect_edges");
        if (!gray) return Result<Image>::failure(gray.error());
        if (aperture < 3 || aperture > 7 || aperture % 2 == 0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Edge aperture is invalid", "detect_edges");
        cv::Mat result, gradient;
        const auto native = detail::to_native(gray.value());
        switch (algorithm) {
        case EdgeAlgorithm::canny: cv::Canny(native, result, low_threshold, high_threshold, aperture); break;
        case EdgeAlgorithm::sobel: cv::Sobel(native, gradient, CV_32F, 1, 1, aperture); cv::convertScaleAbs(gradient, result); break;
        case EdgeAlgorithm::scharr: cv::Scharr(native, gradient, CV_32F, 1, 0); cv::convertScaleAbs(gradient, result); break;
        case EdgeAlgorithm::laplacian: cv::Laplacian(native, gradient, CV_32F, aperture); cv::convertScaleAbs(gradient, result); break;
        }
        return Result<Image>::success(detail::from_native(result));
    });
}

Result<Image> warp_affine_image(const Image &image, const Matrix &transform,
                                Size size, int interpolation) {
    return capture_result("warp_affine_image", [&] {
        if (transform.rows != 2 || transform.columns != 3)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Affine transform must be 2x3", "warp_affine_image");
        cv::Mat result;
        cv::warpAffine(detail::to_native(image), result, to_native_matrix(transform),
                       detail::to_native(size), interpolation);
        return Result<Image>::success(detail::from_native(result));
    });
}

Result<Image> warp_perspective_image(const Image &image, const Matrix &transform,
                                     Size size, int interpolation) {
    return capture_result("warp_perspective_image", [&] {
        if (transform.rows != 3 || transform.columns != 3)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Perspective transform must be 3x3", "warp_perspective_image");
        cv::Mat result;
        cv::warpPerspective(detail::to_native(image), result, to_native_matrix(transform),
                            detail::to_native(size), interpolation);
        return Result<Image>::success(detail::from_native(result));
    });
}

Result<std::vector<ContourInfo>> find_contours(const Image &image, int retrieval, int approximation) {
    return capture_result("find_contours", [&] {
        auto gray = to_gray(image, "find_contours");
        if (!gray) return Result<std::vector<ContourInfo>>::failure(gray.error());
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(detail::to_native(gray.value()), contours, retrieval, approximation);
        std::vector<ContourInfo> result;
        for (const auto &contour : contours) {
            ContourInfo info;
            for (const auto &point : contour) info.points.push_back({point.x, point.y});
            info.area = cv::contourArea(contour);
            info.perimeter = cv::arcLength(contour, true);
            const auto bounds = cv::boundingRect(contour);
            info.bounding_box = {bounds.x, bounds.y, bounds.width, bounds.height};
            info.convex = cv::isContourConvex(contour);
            const auto rotated = cv::minAreaRect(contour);
            info.minimum_box = {rotated.center.x - rotated.size.width / 2.0f,
                                rotated.center.y - rotated.size.height / 2.0f,
                                rotated.size.width, rotated.size.height};
            result.push_back(std::move(info));
        }
        return Result<std::vector<ContourInfo>>::success(std::move(result));
    });
}

Result<Matrix> label_components(const Image &image, Matrix *statistics,
                                Matrix *centroids, int connectivity) {
    return capture_result("label_components", [&] {
        auto gray = to_gray(image, "label_components");
        if (!gray) return Result<Matrix>::failure(gray.error());
        cv::Mat labels, native_statistics, native_centroids;
        cv::connectedComponentsWithStats(detail::to_native(gray.value()), labels,
                                         native_statistics, native_centroids, connectivity);
        if (statistics) *statistics = from_native_matrix(native_statistics);
        if (centroids) *centroids = from_native_matrix(native_centroids);
        return Result<Matrix>::success(from_native_matrix(labels));
    });
}
Result<std::vector<Line4f>> detect_lines(const Image &image, double rho, double theta,
                                         int threshold) {
    return capture_result("detect_lines", [&] {
        auto gray = to_gray(image, "detect_lines");
        if (!gray) return Result<std::vector<Line4f>>::failure(gray.error());
        std::vector<cv::Vec4i> lines;
        cv::HoughLinesP(detail::to_native(gray.value()), lines, rho, theta, threshold);
        std::vector<Line4f> result;
        for (const auto &line : lines) result.push_back({float(line[0]), float(line[1]), float(line[2]), float(line[3])});
        return Result<std::vector<Line4f>>::success(std::move(result));
    });
}
Result<std::vector<Circle3f>> detect_circles(const Image &image, double dp,
                                             double min_distance, double param1,
                                             double param2) {
    return capture_result("detect_circles", [&] {
        auto gray = to_gray(image, "detect_circles");
        if (!gray) return Result<std::vector<Circle3f>>::failure(gray.error());
        cv::Mat blurred;
        cv::medianBlur(detail::to_native(gray.value()), blurred, 5);
        std::vector<cv::Vec3f> circles;
        cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT, dp, min_distance, param1, param2);
        std::vector<Circle3f> result;
        for (const auto &circle : circles) result.push_back({circle[0], circle[1], circle[2]});
        return Result<std::vector<Circle3f>>::success(std::move(result));
    });
}

Result<std::vector<BoundingBox>> non_maximum_suppression(
    const std::vector<BoundingBox> &boxes, float score_threshold, float iou_threshold) {
    if (score_threshold < 0.0f || iou_threshold < 0.0f || iou_threshold > 1.0f)
        return Result<std::vector<BoundingBox>>::failure(std::make_error_code(std::errc::invalid_argument),
                                                         "NMS thresholds are invalid", "nms");
    std::vector<BoundingBox> candidates;
    for (const auto &box : boxes) if (box.score >= score_threshold) candidates.push_back(box);
    std::sort(candidates.begin(), candidates.end(), [](const auto &left, const auto &right) {
        return left.score > right.score;
    });
    std::vector<BoundingBox> result;
    while (!candidates.empty()) {
        auto current = candidates.front();
        candidates.erase(candidates.begin());
        result.push_back(current);
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](const auto &other) {
            return other.class_id == current.class_id && iou(other.rect, current.rect) > iou_threshold;
        }), candidates.end());
    }
    return Result<std::vector<BoundingBox>>::success(std::move(result));
}

Result<FeatureSet> detect_features(const Image &image, FeatureAlgorithm algorithm,
                                   int max_features) {
    return capture_result("detect_features", [&] {
        auto gray = to_gray(image, "detect_features");
        if (!gray) return Result<FeatureSet>::failure(gray.error());
        if (max_features <= 0) return Result<FeatureSet>::failure(std::make_error_code(std::errc::invalid_argument),
                                                                   "max_features must be positive", "detect_features");
        cv::Ptr<cv::Feature2D> detector;
        std::vector<cv::KeyPoint> keypoints;
        switch (algorithm) {
        case FeatureAlgorithm::fast: detector = cv::FastFeatureDetector::create(); break;
        case FeatureAlgorithm::gftt: detector = cv::GFTTDetector::create(max_features); break;
        case FeatureAlgorithm::orb: detector = cv::ORB::create(max_features); break;
        case FeatureAlgorithm::sift: detector = cv::SIFT::create(max_features); break;
        case FeatureAlgorithm::akaze: detector = cv::AKAZE::create(); break;
        case FeatureAlgorithm::brief:
        case FeatureAlgorithm::freak: {
#if defined(SINDRE_OPENCV_HAS_XFEATURES2D)
            auto keypoint_detector = cv::FastFeatureDetector::create();
            keypoint_detector->detect(detail::to_native(gray.value()), keypoints);
            if (algorithm == FeatureAlgorithm::brief)
                detector = cv::xfeatures2d::BriefDescriptorExtractor::create();
            else
                detector = cv::xfeatures2d::FREAK::create();
            // Both descriptor implementations sample a neighborhood around
            // each keypoint.  OpenCV's FREAK backend can overrun for border
            // keypoints on small images instead of reporting an exception;
            // discard points that cannot fit the largest sampling pattern.
            constexpr float descriptor_radius = 24.0f;
            keypoints.erase(std::remove_if(keypoints.begin(), keypoints.end(),
                                           [&](const cv::KeyPoint &point) {
                return point.pt.x < descriptor_radius ||
                       point.pt.y < descriptor_radius ||
                       point.pt.x >= gray.value().width - descriptor_radius ||
                       point.pt.y >= gray.value().height - descriptor_radius;
            }), keypoints.end());
            break;
#else
            return Result<FeatureSet>::failure(
                std::make_error_code(std::errc::function_not_supported),
                "BRIEF/FREAK requires OpenCV xfeatures2d", "detect_features");
#endif
        }
        default: return Result<FeatureSet>::failure(std::make_error_code(std::errc::function_not_supported),
                                                    "Selected feature backend is unavailable", "detect_features");
        }
        cv::Mat descriptors;
#if defined(SINDRE_OPENCV_HAS_XFEATURES2D)
        if (algorithm == FeatureAlgorithm::brief || algorithm == FeatureAlgorithm::freak)
            detector->compute(detail::to_native(gray.value()), keypoints, descriptors);
        else
#endif
            detector->detectAndCompute(detail::to_native(gray.value()), cv::noArray(), keypoints, descriptors);
        FeatureSet result;
        for (const auto &point : keypoints)
            result.keypoints.push_back({{point.pt.x, point.pt.y}, point.size, point.angle,
                                        point.response, point.octave, point.class_id});
        if (!descriptors.empty()) result.descriptors = from_native_matrix(descriptors);
        return Result<FeatureSet>::success(std::move(result));
    });
}
Result<MatchSet> match_features(const FeatureSet &source, const FeatureSet &target,
                                MatcherAlgorithm algorithm, float ratio) {
    return capture_result("match_features", [&] {
        if (source.descriptors.empty() || target.descriptors.empty() || ratio <= 0.0f)
            return Result<MatchSet>::failure(std::make_error_code(std::errc::invalid_argument),
                                             "Feature descriptors or ratio are invalid", "match_features");
        if (source.descriptors.columns != target.descriptors.columns)
            return Result<MatchSet>::failure(std::make_error_code(std::errc::invalid_argument),
                                             "Descriptor dimensions do not match", "match_features");
        const auto norm = algorithm == MatcherAlgorithm::brute_force_hamming ? cv::NORM_HAMMING : cv::NORM_L2;
        cv::Mat left = to_native_matrix(source.descriptors), right = to_native_matrix(target.descriptors);
        if (algorithm == MatcherAlgorithm::flann || norm == cv::NORM_L2) {
            left.convertTo(left, CV_32F);
            right.convertTo(right, CV_32F);
        } else {
            left.convertTo(left, CV_8U);
            right.convertTo(right, CV_8U);
        }
        std::vector<std::vector<cv::DMatch>> candidates;
        if (algorithm == MatcherAlgorithm::flann) {
            cv::FlannBasedMatcher matcher;
            matcher.knnMatch(left, right, candidates, 2);
        } else {
            cv::BFMatcher matcher(norm);
            matcher.knnMatch(left, right, candidates, 2);
        }
        MatchSet result;
        for (const auto &pair : candidates) if (pair.size() == 2 && pair[0].distance < ratio * pair[1].distance)
            result.matches.push_back({pair[0].queryIdx, pair[0].trainIdx, pair[0].imgIdx, pair[0].distance});
        return Result<MatchSet>::success(std::move(result));
    });
}
Result<HomographyResult> estimate_homography(const std::vector<Point2f> &source,
                                             const std::vector<Point2f> &target,
                                             double reprojection_threshold) {
    if (source.size() != target.size() || source.size() < 4)
        return Result<HomographyResult>::failure(std::make_error_code(std::errc::invalid_argument),
                                                 "At least four matching points are required", "estimate_homography");
    std::vector<cv::Point2f> left, right;
    for (const auto &point : source) left.push_back(detail::to_native(point));
    for (const auto &point : target) right.push_back(detail::to_native(point));
    cv::Mat mask;
    auto matrix = cv::findHomography(left, right, cv::RANSAC, reprojection_threshold, mask);
    if (matrix.empty()) return Result<HomographyResult>::failure(std::make_error_code(std::errc::invalid_argument),
                                                                  "Homography estimation failed", "estimate_homography");
    HomographyResult result{from_native_matrix(matrix), {}};
    if (!mask.empty()) result.inlier_mask.assign(mask.begin<uchar>(), mask.end<uchar>());
    return Result<HomographyResult>::success(std::move(result));
}
Result<SparseFlowResult> track_points(const Image &previous, const Image &current,
                                      const std::vector<Point2f> &points,
                                      OpticalFlowAlgorithm algorithm) {
    auto left = to_gray(previous, "track_points");
    auto right = to_gray(current, "track_points");
    if (!left || !right) return Result<SparseFlowResult>::failure(left ? right.error() : left.error());
    std::vector<cv::Point2f> native_points;
    for (const auto &point : points) native_points.push_back(detail::to_native(point));
    std::vector<cv::Point2f> next;
    std::vector<uchar> status;
    std::vector<float> errors;
    if (algorithm == OpticalFlowAlgorithm::lucas_kanade) {
        cv::calcOpticalFlowPyrLK(detail::to_native(left.value()), detail::to_native(right.value()),
                                 native_points, next, status, errors);
    } else if (algorithm == OpticalFlowAlgorithm::farneback) {
        cv::Mat flow;
        cv::calcOpticalFlowFarneback(detail::to_native(left.value()),
                                     detail::to_native(right.value()), flow,
                                     0.5, 3, 15, 3, 5, 1.2, 0);
        next.reserve(native_points.size());
        status.reserve(native_points.size());
        errors.reserve(native_points.size());
        for (const auto &point : native_points) {
            const int x = static_cast<int>(std::lround(point.x));
            const int y = static_cast<int>(std::lround(point.y));
            if (x < 0 || y < 0 || x >= flow.cols || y >= flow.rows) {
                next.push_back(point);
                status.push_back(0);
                errors.push_back(0.0f);
                continue;
            }
            const auto vector = flow.at<cv::Vec2f>(y, x);
            next.push_back({point.x + vector[0], point.y + vector[1]});
            status.push_back(1);
            errors.push_back(std::sqrt(vector[0] * vector[0] + vector[1] * vector[1]));
        }
    } else {
        // The OpenCV 4.12 RLOF implementation currently has an unsafe native
        // failure path for valid small-image inputs on the supported Windows
        // toolchain. Do not expose that backend until it can be isolated or
        // upgraded without allowing a process-level crash.
        return Result<SparseFlowResult>::failure(
            std::make_error_code(std::errc::function_not_supported),
            "Selected optical-flow backend is unavailable", "track_points");
    }
    SparseFlowResult result;
    for (const auto &point : next) result.points.push_back({point.x, point.y});
    result.status.assign(status.begin(), status.end());
    result.errors = std::move(errors);
    return Result<SparseFlowResult>::success(std::move(result));
}
Result<Image> calculate_dense_flow(const Image &previous, const Image &current,
                                   double pyramid_scale, int levels,
                                   int window_size, int iterations) {
    return capture_result("calculate_dense_flow", [&] {
        if (!(pyramid_scale > 0.0 && pyramid_scale < 1.0) || levels <= 0 ||
            window_size <= 0 || window_size % 2 == 0 || iterations <= 0) {
            return Result<Image>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Dense-flow parameters are invalid", "calculate_dense_flow");
        }
        auto left = to_gray(previous, "calculate_dense_flow");
        auto right = to_gray(current, "calculate_dense_flow");
        if (!left) return Result<Image>::failure(left.error());
        if (!right) return Result<Image>::failure(right.error());
        if (left.value().width != right.value().width ||
            left.value().height != right.value().height) {
            return Result<Image>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Dense-flow images must have equal dimensions",
                "calculate_dense_flow");
        }

        cv::Mat flow;
        cv::calcOpticalFlowFarneback(
            detail::to_native(left.value()), detail::to_native(right.value()), flow,
            pyramid_scale, levels, window_size, iterations, 5, 1.2, 0);

        // Image is intentionally an owning 8-bit facade, so expose the dense
        // vector field as a stable HSV-to-BGR visualization: hue is direction
        // and value is normalized magnitude. This avoids leaking cv::Mat or a
        // backend-specific two-channel floating-point buffer.
        std::vector<cv::Mat> components;
        cv::split(flow, components);
        cv::Mat magnitude, angle;
        cv::cartToPolar(components[0], components[1], magnitude, angle, true);
        double maximum = 0.0;
        cv::minMaxLoc(magnitude, nullptr, &maximum);
        cv::Mat hsv(flow.size(), CV_8UC3);
        std::vector<cv::Mat> hsv_channels;
        cv::split(hsv, hsv_channels);
        angle.convertTo(hsv_channels[0], CV_8U, 0.5);
        hsv_channels[1].setTo(255);
        if (maximum > 0.0) {
            magnitude.convertTo(hsv_channels[2], CV_8U, 255.0 / maximum);
        } else {
            hsv_channels[2].setTo(0);
        }
        cv::merge(hsv_channels, hsv);
        cv::Mat bgr;
        cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
        return Result<Image>::success(detail::from_native(bgr));
    });
}
Result<Image> undistort_image(const Image &image, const Matrix &camera_matrix,
                              const Matrix &distortion_coefficients) {
    if (camera_matrix.rows != 3 || camera_matrix.columns != 3)
        return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                      "Camera matrix must be 3x3", "undistort_image");
    cv::Mat result;
    cv::undistort(detail::to_native(image), result, to_native_matrix(camera_matrix),
                  to_native_matrix(distortion_coefficients));
    return Result<Image>::success(detail::from_native(result));
}
Result<CameraCalibrationResult> calibrate_camera(const std::vector<std::vector<Point3f>> &object_points,
                                                const std::vector<std::vector<Point2f>> &image_points,
                                                Size image_size) {
    if (object_points.empty() || object_points.size() != image_points.size())
        return Result<CameraCalibrationResult>::failure(std::make_error_code(std::errc::invalid_argument),
                                                        "Calibration point sets do not match", "calibrate_camera");
    std::vector<std::vector<cv::Point3f>> object_native;
    std::vector<std::vector<cv::Point2f>> image_native;
    for (std::size_t i = 0; i < object_points.size(); ++i) {
        object_native.emplace_back(); image_native.emplace_back();
        for (const auto &point : object_points[i]) object_native.back().push_back(detail::to_native(point));
        for (const auto &point : image_points[i]) image_native.back().push_back(detail::to_native(point));
    }
    cv::Mat camera, distortion;
    std::vector<cv::Mat> rotations, translations;
    const auto rms = cv::calibrateCamera(object_native, image_native,
                                         detail::to_native(image_size), camera, distortion,
                                         rotations, translations);
    CameraCalibrationResult result{from_native_matrix(camera), from_native_matrix(distortion), {}, {}, rms};
    for (const auto &value : rotations) result.rotation_vectors.push_back(from_native_matrix(value));
    for (const auto &value : translations) result.translation_vectors.push_back(from_native_matrix(value));
    return Result<CameraCalibrationResult>::success(std::move(result));
}
Result<ArucoResult> detect_aruco_markers(const Image &image, int dictionary_id) {
    return capture_result("detect_aruco_markers", [&] {
        auto dictionary = cv::makePtr<cv::aruco::Dictionary>(
            cv::aruco::getPredefinedDictionary(dictionary_id));
        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners, rejected;
        auto parameters = cv::makePtr<cv::aruco::DetectorParameters>();
        cv::aruco::detectMarkers(detail::to_native(image), dictionary, corners, ids,
                                 parameters, rejected);
        ArucoResult result{std::move(ids), {}, {}};
        for (const auto &set : corners) { result.corners.emplace_back(); for (const auto &point : set) result.corners.back().push_back({point.x, point.y}); }
        for (const auto &set : rejected) { result.rejected.emplace_back(); for (const auto &point : set) result.rejected.back().push_back({point.x, point.y}); }
        return Result<ArucoResult>::success(std::move(result));
    });
}

} // namespace sindre::utils_2d
