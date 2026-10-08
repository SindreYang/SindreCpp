#pragma once

/// @file
/// @brief 常用 2D 图像处理、特征和几何算法 facade。

#include <sindre/utils_2d.h>

#include <cstdint>
#include <vector>

namespace sindre::utils_2d {

struct Matrix {
    int rows = 0;
    int columns = 0;
    std::vector<double> values;

    [[nodiscard]] bool empty() const noexcept { return rows <= 0 || columns <= 0; }
    [[nodiscard]] double at(int row, int column) const noexcept {
        return values[static_cast<std::size_t>(row) * columns + column];
    }
};

struct KeyPoint {
    Point2f point;
    float size = 0.0f;
    float angle = -1.0f;
    float response = 0.0f;
    int octave = 0;
    int class_id = -1;
};

struct Match {
    int query_index = -1;
    int train_index = -1;
    int image_index = -1;
    float distance = 0.0f;
};

struct Line4f { float x1 = 0, y1 = 0, x2 = 0, y2 = 0; };
struct Circle3f { float x = 0, y = 0, radius = 0; };

enum class FeatureAlgorithm { fast, gftt, orb, sift, akaze, brief, freak };
enum class MatcherAlgorithm { brute_force, brute_force_hamming, flann };
enum class OpticalFlowAlgorithm { lucas_kanade, farneback, rlof };

struct BoundingBox {
    Rect2f rect;
    float score = 0.0f;
    int class_id = -1;
};

struct FeatureSet {
    std::vector<KeyPoint> keypoints;
    Matrix descriptors;
};

struct MatchSet {
    std::vector<Match> matches;
    std::vector<std::uint8_t> inlier_mask;
};

struct SparseFlowResult {
    std::vector<Point2f> points;
    std::vector<std::uint8_t> status;
    std::vector<float> errors;
};

struct HomographyResult {
    Matrix matrix;
    std::vector<std::uint8_t> inlier_mask;
};

struct CameraCalibrationResult {
    Matrix camera_matrix;
    Matrix distortion_coefficients;
    std::vector<Matrix> rotation_vectors;
    std::vector<Matrix> translation_vectors;
    double rms_error = 0.0;
};

struct ArucoResult {
    std::vector<int> ids;
    std::vector<std::vector<Point2f>> corners;
    std::vector<std::vector<Point2f>> rejected;
};

Result<Image> apply_blur(const Image &image, BlurAlgorithm algorithm,
                         Size kernel = {5, 5}, double sigma = 0.0);
Result<Image> threshold_image(const Image &image, ThresholdAlgorithm algorithm,
                              double threshold = 0.0, double max_value = 255.0,
                              int block_size = 11, double constant = 2.0);
Result<Image> apply_morphology(const Image &image, MorphologyOperation operation,
                               Size kernel = {3, 3}, int iterations = 1,
                               int shape = morphology_rect);
Result<Image> detect_edges(const Image &image, EdgeAlgorithm algorithm,
                           double low_threshold = 50.0,
                           double high_threshold = 150.0, int aperture = 3);
Result<Image> warp_affine_image(const Image &image, const Matrix &transform,
                                Size size,
                                int interpolation = interpolation_linear);
Result<Image> warp_perspective_image(const Image &image, const Matrix &transform,
                                     Size size,
                                     int interpolation = interpolation_linear);
Result<std::vector<ContourInfo>> find_contours(
    const Image &image, int retrieval = retrieval_external,
    int approximation = chain_approx_simple);
Result<Matrix> label_components(const Image &image, Matrix *statistics = nullptr,
                                 Matrix *centroids = nullptr, int connectivity = 8);
Result<std::vector<Line4f>> detect_lines(const Image &image, double rho = 1.0,
                                         double theta = 3.14159265358979323846 / 180.0,
                                         int threshold = 50);
Result<std::vector<Circle3f>> detect_circles(const Image &image, double dp = 1.0,
                                             double min_distance = 20.0,
                                             double param1 = 100.0,
                                             double param2 = 30.0);
Result<std::vector<BoundingBox>> non_maximum_suppression(
    const std::vector<BoundingBox> &boxes, float score_threshold = 0.25f,
    float iou_threshold = 0.45f);
Result<FeatureSet> detect_features(const Image &image, FeatureAlgorithm algorithm,
                                   int max_features = 1000);
Result<MatchSet> match_features(const FeatureSet &source, const FeatureSet &target,
                                MatcherAlgorithm algorithm = MatcherAlgorithm::brute_force,
                                float ratio = 0.75f);
Result<HomographyResult> estimate_homography(
    const std::vector<Point2f> &source, const std::vector<Point2f> &target,
    double reprojection_threshold = 3.0);
Result<SparseFlowResult> track_points(
    const Image &previous, const Image &current,
    const std::vector<Point2f> &points,
    OpticalFlowAlgorithm algorithm = OpticalFlowAlgorithm::lucas_kanade);
Result<Image> calculate_dense_flow(const Image &previous, const Image &current,
                                   double pyramid_scale = 0.5, int levels = 3,
                                   int window_size = 15, int iterations = 3);
Result<Image> undistort_image(const Image &image, const Matrix &camera_matrix,
                              const Matrix &distortion_coefficients);
Result<CameraCalibrationResult> calibrate_camera(
    const std::vector<std::vector<Point3f>> &object_points,
    const std::vector<std::vector<Point2f>> &image_points, Size image_size);
Result<ArucoResult> detect_aruco_markers(
    const Image &image, int dictionary_id = 0);

} // namespace sindre::utils_2d
