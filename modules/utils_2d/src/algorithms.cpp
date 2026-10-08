#include <sindre/utils_2d/algorithms.h>

#include <opencv2/xfeatures2d.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace sindre::utils_2d {
namespace {

template <class Function>
auto capture_result(const char* context, Function&& function) -> decltype(function()) {
    using Return = decltype(function());
#if defined(SINDRE_NO_EXCEPTIONS)
    (void)context;
    return function();
#else
    try {
        return function();
    } catch (const cv::Exception& error) {
        return Return::failure(std::make_error_code(std::errc::invalid_argument), error.what(), context);
    } catch (const std::invalid_argument& error) {
        return Return::failure(std::make_error_code(std::errc::invalid_argument), error.what(), context);
    } catch (const std::exception& error) {
        return Return::failure(std::make_error_code(std::errc::io_error), error.what(), context);
    } catch (...) {
        return Return::failure(std::make_error_code(std::errc::io_error), "Unknown 2D algorithm failure", context);
    }
#endif
}

Result<Image> to_gray(const Image& image, const char* context) {
    auto valid = validate_image(image);
    if (!valid)
        return Result<Image>::failure(valid.error().with_context(context));
    if (image.channels() == 1)
        return Result<Image>::success(image);
    if (image.channels() == 3)
        return convert_color(image, cv::COLOR_BGR2GRAY);
    return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                  "Expected a grayscale or BGR image", context);
}

bool valid_kernel(cv::Size kernel) {
    return kernel.width > 0 && kernel.height > 0 && kernel.width % 2 == 1 &&
           kernel.height % 2 == 1;
}

float intersection_over_union(const cv::Rect2f& a, const cv::Rect2f& b) {
    const auto intersection = a & b;
    const float union_area = a.area() + b.area() - intersection.area();
    return union_area > 0.f ? intersection.area() / union_area : 0.f;
}

} // namespace

Result<Image> apply_blur(const Image& image, BlurAlgorithm algorithm, cv::Size kernel,
                         double sigma) {
    return capture_result("apply_blur", [&] {
        auto valid = validate_image(image);
        if (!valid)
            return Result<Image>::failure(valid.error());
        if (!valid_kernel(kernel))
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Blur kernel dimensions must be positive odd values",
                                          "apply_blur");
        Image result;
        switch (algorithm) {
        case BlurAlgorithm::gaussian:
            cv::GaussianBlur(image, result, kernel, sigma);
            break;
        case BlurAlgorithm::median:
            cv::medianBlur(image, result, kernel.width);
            break;
        case BlurAlgorithm::bilateral:
            cv::bilateralFilter(image, result, kernel.width, sigma > 0.0 ? sigma : 75.0, sigma > 0.0 ? sigma : 75.0);
            break;
        }
        return Result<Image>::success(std::move(result));
    });
}

Result<Image> threshold_image(const Image& image, ThresholdAlgorithm algorithm, double threshold,
                              double max_value, int block_size, double constant) {
    return capture_result("threshold_image", [&] {
        auto gray = to_gray(image, "threshold_image");
        if (!gray)
            return Result<Image>::failure(gray.error());
        if (!std::isfinite(threshold) || !std::isfinite(max_value) || max_value <= 0.0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Threshold values must be finite and max_value positive",
                                          "threshold_image");
        if ((algorithm == ThresholdAlgorithm::adaptive_mean ||
             algorithm == ThresholdAlgorithm::adaptive_gaussian) &&
            (!valid_kernel({block_size, block_size}) || block_size <= 1))
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Adaptive threshold block_size must be odd and greater than one",
                                          "threshold_image");
        Image result;
        if (algorithm == ThresholdAlgorithm::adaptive_mean ||
            algorithm == ThresholdAlgorithm::adaptive_gaussian) {
            const auto method = algorithm == ThresholdAlgorithm::adaptive_mean
                                    ? cv::ADAPTIVE_THRESH_MEAN_C
                                    : cv::ADAPTIVE_THRESH_GAUSSIAN_C;
            cv::adaptiveThreshold(gray.value(), result, max_value, method, cv::THRESH_BINARY,
                                  block_size, constant);
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
            cv::threshold(gray.value(), result, threshold, max_value, type);
        }
        return Result<Image>::success(std::move(result));
    });
}

Result<Image> apply_morphology(const Image& image, MorphologyOperation operation, cv::Size kernel,
                               int iterations, int shape) {
    return capture_result("apply_morphology", [&] {
        auto valid = validate_image(image);
        if (!valid)
            return Result<Image>::failure(valid.error());
        if (!valid_kernel(kernel) || iterations <= 0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Morphology kernel must be positive odd and iterations positive",
                                          "apply_morphology");
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
        Image result;
        cv::morphologyEx(image, result, code, cv::getStructuringElement(shape, kernel),
                         {-1, -1}, iterations);
        return Result<Image>::success(std::move(result));
    });
}

Result<Image> detect_edges(const Image& image, EdgeAlgorithm algorithm, double low_threshold,
                           double high_threshold, int aperture) {
    return capture_result("detect_edges", [&] {
        auto gray = to_gray(image, "detect_edges");
        if (!gray)
            return Result<Image>::failure(gray.error());
        if (aperture < 3 || aperture > 7 || aperture % 2 == 0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Edge aperture must be an odd value in [3,7]", "detect_edges");
        Image result;
        switch (algorithm) {
        case EdgeAlgorithm::canny:
            if (!std::isfinite(low_threshold) || !std::isfinite(high_threshold) ||
                low_threshold < 0.0 || high_threshold < low_threshold)
                return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                              "Canny thresholds are invalid", "detect_edges");
            cv::Canny(gray.value(), result, low_threshold, high_threshold, aperture);
            break;
        case EdgeAlgorithm::sobel: {
            Image gradient;
            cv::Sobel(gray.value(), gradient, CV_32F, 1, 1, aperture);
            cv::convertScaleAbs(gradient, result);
            break;
        }
        case EdgeAlgorithm::scharr: {
            Image gradient;
            cv::Scharr(gray.value(), gradient, CV_32F, 1, 0);
            cv::convertScaleAbs(gradient, result);
            break;
        }
        case EdgeAlgorithm::laplacian: {
            Image gradient;
            cv::Laplacian(gray.value(), gradient, CV_32F, aperture);
            cv::convertScaleAbs(gradient, result);
            break;
        }
        }
        return Result<Image>::success(std::move(result));
    });
}

Result<Image> warp_affine_image(const Image& image, const cv::Mat& transform, cv::Size size,
                                int interpolation) {
    return capture_result("warp_affine_image", [&] {
        auto valid = validate_image(image);
        if (!valid)
            return Result<Image>::failure(valid.error());
        if (transform.rows != 2 || transform.cols != 3 || size.width <= 0 || size.height <= 0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Affine transform must be 2x3 and output size positive",
                                          "warp_affine_image");
        Image result;
        cv::warpAffine(image, result, transform, size, interpolation);
        return Result<Image>::success(std::move(result));
    });
}

Result<Image> warp_perspective_image(const Image& image, const cv::Mat& transform, cv::Size size,
                                     int interpolation) {
    return capture_result("warp_perspective_image", [&] {
        auto valid = validate_image(image);
        if (!valid)
            return Result<Image>::failure(valid.error());
        if (transform.rows != 3 || transform.cols != 3 || size.width <= 0 || size.height <= 0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Perspective transform must be 3x3 and output size positive",
                                          "warp_perspective_image");
        Image result;
        cv::warpPerspective(image, result, transform, size, interpolation);
        return Result<Image>::success(std::move(result));
    });
}

Result<std::vector<ContourInfo>> find_contours(const Image& image, int retrieval, int approximation) {
    return capture_result("find_contours", [&] {
        auto gray = to_gray(image, "find_contours");
        if (!gray)
            return Result<std::vector<ContourInfo>>::failure(gray.error());
        if (gray.value().depth() != CV_8U)
            return Result<std::vector<ContourInfo>>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Contour input must have 8-bit depth", "find_contours");
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(gray.value().clone(), contours, retrieval, approximation);
        std::vector<ContourInfo> result;
        result.reserve(contours.size());
        for (auto& points : contours) {
            ContourInfo info;
            info.points = std::move(points);
            info.area = cv::contourArea(info.points);
            info.perimeter = cv::arcLength(info.points, true);
            info.bounding_box = cv::boundingRect(info.points);
            info.minimum_box = cv::minAreaRect(info.points);
            info.convex = cv::isContourConvex(info.points);
            result.push_back(std::move(info));
        }
        return Result<std::vector<ContourInfo>>::success(std::move(result));
    });
}

Result<cv::Mat> label_components(const Image& image, cv::Mat* statistics, cv::Mat* centroids,
                                 int connectivity) {
    return capture_result("label_components", [&] {
        auto gray = to_gray(image, "label_components");
        if (!gray)
            return Result<cv::Mat>::failure(gray.error());
        if (gray.value().depth() != CV_8U || (connectivity != 4 && connectivity != 8))
            return Result<cv::Mat>::failure(std::make_error_code(std::errc::invalid_argument),
                                            "Connected components require 8-bit input and 4 or 8 connectivity",
                                            "label_components");
        cv::Mat local_statistics;
        cv::Mat local_centroids;
        cv::Mat labels;
        cv::connectedComponentsWithStats(
            gray.value(), labels, local_statistics, local_centroids, connectivity, CV_32S);
        if (statistics)
            *statistics = std::move(local_statistics);
        if (centroids)
            *centroids = std::move(local_centroids);
        return Result<cv::Mat>::success(std::move(labels));
    });
}

Result<std::vector<cv::Vec4i>> detect_lines(const Image& image, double rho, double theta,
                                            int threshold) {
    return capture_result("detect_lines", [&] {
        auto gray = to_gray(image, "detect_lines");
        if (!gray)
            return Result<std::vector<cv::Vec4i>>::failure(gray.error());
        if (!std::isfinite(rho) || rho <= 0.0 || !std::isfinite(theta) || theta <= 0.0 || threshold <= 0)
            return Result<std::vector<cv::Vec4i>>::failure(std::make_error_code(std::errc::invalid_argument),
                                                            "Hough line parameters are invalid", "detect_lines");
        std::vector<cv::Vec4i> lines;
        cv::HoughLinesP(gray.value(), lines, rho, theta, threshold);
        return Result<std::vector<cv::Vec4i>>::success(std::move(lines));
    });
}

Result<std::vector<cv::Vec3f>> detect_circles(const Image& image, double dp, double min_distance,
                                              double param1, double param2) {
    return capture_result("detect_circles", [&] {
        auto gray = to_gray(image, "detect_circles");
        if (!gray)
            return Result<std::vector<cv::Vec3f>>::failure(gray.error());
        if (!std::isfinite(dp) || dp <= 0.0 || !std::isfinite(min_distance) || min_distance <= 0.0 ||
            !std::isfinite(param1) || !std::isfinite(param2) || param1 <= 0.0 || param2 <= 0.0)
            return Result<std::vector<cv::Vec3f>>::failure(std::make_error_code(std::errc::invalid_argument),
                                                           "Hough circle parameters are invalid", "detect_circles");
        std::vector<cv::Vec3f> circles;
        cv::HoughCircles(gray.value(), circles, cv::HOUGH_GRADIENT, dp, min_distance, param1, param2);
        return Result<std::vector<cv::Vec3f>>::success(std::move(circles));
    });
}

Result<std::vector<BoundingBox>> non_maximum_suppression(const std::vector<BoundingBox>& boxes,
                                                         float score_threshold, float iou_threshold) {
    return capture_result("non_maximum_suppression", [&] {
        if (!std::isfinite(score_threshold) || !std::isfinite(iou_threshold) ||
            score_threshold < 0.f || iou_threshold < 0.f || iou_threshold > 1.f)
            return Result<std::vector<BoundingBox>>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "NMS thresholds must be finite and IoU must be in [0,1]", "non_maximum_suppression");
        std::vector<BoundingBox> candidates;
        for (const auto& box : boxes) {
            if (box.rect.width <= 0.f || box.rect.height <= 0.f || !std::isfinite(box.score))
                return Result<std::vector<BoundingBox>>::failure(
                    std::make_error_code(std::errc::invalid_argument),
                    "NMS boxes must have positive finite geometry and score", "non_maximum_suppression");
            if (box.score >= score_threshold)
                candidates.push_back(box);
        }
        std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
            if (left.score != right.score)
                return left.score > right.score;
            if (left.class_id != right.class_id)
                return left.class_id < right.class_id;
            return left.rect.x < right.rect.x;
        });
        std::vector<BoundingBox> result;
        std::vector<bool> suppressed(candidates.size(), false);
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            if (suppressed[i])
                continue;
            result.push_back(candidates[i]);
            for (std::size_t j = i + 1; j < candidates.size(); ++j) {
                if (!suppressed[j] && candidates[i].class_id == candidates[j].class_id &&
                    intersection_over_union(candidates[i].rect, candidates[j].rect) > iou_threshold)
                    suppressed[j] = true;
            }
        }
        return Result<std::vector<BoundingBox>>::success(std::move(result));
    });
}

Result<FeatureSet> detect_features(const Image& image, FeatureAlgorithm algorithm, int max_features) {
    return capture_result("detect_features", [&] {
        auto gray = to_gray(image, "detect_features");
        if (!gray)
            return Result<FeatureSet>::failure(gray.error());
        if (max_features <= 0)
            return Result<FeatureSet>::failure(std::make_error_code(std::errc::invalid_argument),
                                               "max_features must be positive", "detect_features");
        cv::Ptr<cv::Feature2D> feature;
        cv::Ptr<cv::Feature2D> descriptor;
        switch (algorithm) {
        case FeatureAlgorithm::fast:
            feature = cv::FastFeatureDetector::create();
            break;
        case FeatureAlgorithm::gftt:
            feature = cv::GFTTDetector::create(max_features);
            break;
        case FeatureAlgorithm::orb:
            feature = cv::ORB::create(max_features);
            break;
        case FeatureAlgorithm::sift:
            feature = cv::SIFT::create(max_features);
            break;
        case FeatureAlgorithm::akaze:
            feature = cv::AKAZE::create();
            break;
        case FeatureAlgorithm::brief:
            feature = cv::ORB::create(max_features);
            descriptor = cv::xfeatures2d::BriefDescriptorExtractor::create();
            break;
        case FeatureAlgorithm::freak:
            feature = cv::ORB::create(max_features);
            descriptor = cv::xfeatures2d::FREAK::create();
            break;
        }
        FeatureSet result;
        if (descriptor) {
            feature->detect(gray.value(), result.keypoints);
            descriptor->compute(gray.value(), result.keypoints, result.descriptors);
        } else if (algorithm == FeatureAlgorithm::fast || algorithm == FeatureAlgorithm::gftt) {
            // These detectors intentionally expose keypoints only.  Their
            // Feature2D::detectAndCompute implementation is not available in
            // OpenCV 4.8 and should not turn a valid detector call into an
            // unsupported-operation Result.
            feature->detect(gray.value(), result.keypoints);
        } else {
            feature->detectAndCompute(gray.value(), cv::noArray(), result.keypoints,
                                      result.descriptors);
        }
        return Result<FeatureSet>::success(std::move(result));
    });
}

Result<MatchSet> match_features(const FeatureSet& source, const FeatureSet& target,
                                MatcherAlgorithm algorithm, float ratio) {
    return capture_result("match_features", [&] {
        if (source.descriptors.empty() || target.descriptors.empty() ||
            !std::isfinite(ratio) || ratio <= 0.f || ratio >= 1.f)
            return Result<MatchSet>::failure(std::make_error_code(std::errc::invalid_argument),
                                             "Descriptors must be non-empty and ratio in (0,1)",
                                             "match_features");
        cv::Mat source_descriptors = source.descriptors;
        cv::Mat target_descriptors = target.descriptors;
        if (source_descriptors.cols != target_descriptors.cols)
            return Result<MatchSet>::failure(std::make_error_code(std::errc::invalid_argument),
                                             "Descriptor dimensions must match", "match_features");
        cv::Ptr<cv::DescriptorMatcher> matcher;
        if (algorithm == MatcherAlgorithm::flann) {
            if (source_descriptors.depth() != CV_32F) {
                source_descriptors.convertTo(source_descriptors, CV_32F);
                target_descriptors.convertTo(target_descriptors, CV_32F);
            }
            matcher = cv::FlannBasedMatcher::create();
        } else {
            const int norm = algorithm == MatcherAlgorithm::brute_force_hamming
                                 ? cv::NORM_HAMMING
                                 : cv::NORM_L2;
            matcher = cv::BFMatcher::create(norm, false);
        }
        std::vector<std::vector<cv::DMatch>> nearest;
        matcher->knnMatch(source_descriptors, target_descriptors, nearest, 2);
        MatchSet result;
        for (const auto& candidates : nearest) {
            if (candidates.size() == 2 && candidates[0].distance < ratio * candidates[1].distance)
                result.matches.push_back(candidates[0]);
        }
        return Result<MatchSet>::success(std::move(result));
    });
}

Result<HomographyResult> estimate_homography(const std::vector<cv::Point2f>& source,
                                             const std::vector<cv::Point2f>& target,
                                             double reprojection_threshold) {
    return capture_result("estimate_homography", [&] {
        if (source.size() != target.size() || source.size() < 4 ||
            !std::isfinite(reprojection_threshold) || reprojection_threshold <= 0.0)
            return Result<HomographyResult>::failure(std::make_error_code(std::errc::invalid_argument),
                                                     "Homography requires four or more equal point pairs",
                                                     "estimate_homography");
        cv::Mat mask;
        auto matrix = cv::findHomography(source, target, cv::RANSAC, reprojection_threshold, mask);
        if (matrix.empty())
            return Result<HomographyResult>::failure(std::make_error_code(std::errc::result_out_of_range),
                                                     "Homography could not be estimated", "estimate_homography");
        HomographyResult result;
        result.matrix = std::move(matrix);
        result.inlier_mask.assign(mask.begin<uchar>(), mask.end<uchar>());
        return Result<HomographyResult>::success(std::move(result));
    });
}

Result<SparseFlowResult> track_points(const Image& previous, const Image& current,
                                      const std::vector<cv::Point2f>& points,
                                      OpticalFlowAlgorithm algorithm) {
    return capture_result("track_points", [&] {
        auto previous_gray = to_gray(previous, "track_points");
        auto current_gray = to_gray(current, "track_points");
        if (!previous_gray)
            return Result<SparseFlowResult>::failure(previous_gray.error());
        if (!current_gray)
            return Result<SparseFlowResult>::failure(current_gray.error());
        SparseFlowResult result;
        result.points.resize(points.size());
        result.status.resize(points.size(), 0);
        result.errors.resize(points.size(), std::numeric_limits<float>::infinity());
        if (algorithm == OpticalFlowAlgorithm::lucas_kanade) {
            cv::calcOpticalFlowPyrLK(previous_gray.value(), current_gray.value(), points,
                                     result.points, result.status, result.errors);
        } else if (algorithm == OpticalFlowAlgorithm::rlof) {
            cv::Mat dense;
            cv::optflow::calcOpticalFlowDenseRLOF(previous_gray.value(), current_gray.value(), dense);
            for (std::size_t i = 0; i < points.size(); ++i) {
                const auto x = static_cast<int>(std::round(points[i].x));
                const auto y = static_cast<int>(std::round(points[i].y));
                if (x < 0 || y < 0 || x >= dense.cols || y >= dense.rows)
                    continue;
                const auto flow = dense.at<cv::Vec2f>(y, x);
                result.points[i] = points[i] + cv::Point2f(flow[0], flow[1]);
                result.status[i] = 1;
                result.errors[i] = 0.f;
            }
        } else {
            return Result<SparseFlowResult>::failure(std::make_error_code(std::errc::invalid_argument),
                                                     "Farneback is a dense flow algorithm", "track_points");
        }
        return Result<SparseFlowResult>::success(std::move(result));
    });
}

Result<Image> calculate_dense_flow(const Image& previous, const Image& current, double pyramid_scale,
                                   int levels, int window_size, int iterations) {
    return capture_result("calculate_dense_flow", [&] {
        auto previous_gray = to_gray(previous, "calculate_dense_flow");
        auto current_gray = to_gray(current, "calculate_dense_flow");
        if (!previous_gray)
            return Result<Image>::failure(previous_gray.error());
        if (!current_gray)
            return Result<Image>::failure(current_gray.error());
        if (!std::isfinite(pyramid_scale) || pyramid_scale <= 0.0 || pyramid_scale >= 1.0 ||
            levels <= 0 || window_size <= 0 || iterations <= 0)
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Dense flow parameters are invalid", "calculate_dense_flow");
        Image flow;
        cv::calcOpticalFlowFarneback(previous_gray.value(), current_gray.value(), flow,
                                     pyramid_scale, levels, window_size, iterations, 5, 1.2, 0);
        return Result<Image>::success(std::move(flow));
    });
}

Result<Image> undistort_image(const Image& image, const cv::Mat& camera_matrix,
                              const cv::Mat& distortion_coefficients) {
    return capture_result("undistort_image", [&] {
        auto valid = validate_image(image);
        if (!valid)
            return Result<Image>::failure(valid.error());
        if (camera_matrix.rows != 3 || camera_matrix.cols != 3 || camera_matrix.empty() ||
            distortion_coefficients.empty())
            return Result<Image>::failure(std::make_error_code(std::errc::invalid_argument),
                                          "Camera matrix must be 3x3 and distortion coefficients non-empty",
                                          "undistort_image");
        Image result;
        cv::undistort(image, result, camera_matrix, distortion_coefficients);
        return Result<Image>::success(std::move(result));
    });
}

Result<CameraCalibrationResult> calibrate_camera(
    const std::vector<std::vector<cv::Point3f>>& object_points,
    const std::vector<std::vector<cv::Point2f>>& image_points, cv::Size image_size) {
    return capture_result("calibrate_camera", [&] {
        if (object_points.empty() || object_points.size() != image_points.size() ||
            image_size.width <= 0 || image_size.height <= 0)
            return Result<CameraCalibrationResult>::failure(
                std::make_error_code(std::errc::invalid_argument),
                "Calibration point sets and image size are invalid", "calibrate_camera");
        CameraCalibrationResult result;
        result.rms_error = cv::calibrateCamera(object_points, image_points, image_size,
                                               result.camera_matrix, result.distortion_coefficients,
                                               result.rotation_vectors, result.translation_vectors);
        return Result<CameraCalibrationResult>::success(std::move(result));
    });
}

Result<ArucoResult> detect_aruco_markers(const Image& image, int dictionary_id) {
    return capture_result("detect_aruco_markers", [&] {
        auto gray = to_gray(image, "detect_aruco_markers");
        if (!gray)
            return Result<ArucoResult>::failure(gray.error());
        if (dictionary_id < cv::aruco::DICT_4X4_50 ||
            dictionary_id > cv::aruco::DICT_APRILTAG_36h11)
            return Result<ArucoResult>::failure(std::make_error_code(std::errc::invalid_argument),
                                                "Unsupported ArUco dictionary id", "detect_aruco_markers");
        auto dictionary = cv::aruco::getPredefinedDictionary(dictionary_id);
        cv::aruco::ArucoDetector detector(dictionary);
        ArucoResult result;
        detector.detectMarkers(gray.value(), result.corners, result.ids, result.rejected);
        return Result<ArucoResult>::success(std::move(result));
    });
}

} // namespace sindre::utils_2d
