#pragma once

/// @file
/// @brief OpenCV 2D 图像处理、特征、几何和运动算法接口。

#include <sindre/utils_2d.h>

#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/optflow.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/video.hpp>

#include <cstdint>
#include <vector>

namespace sindre::utils_2d {

/// @brief 特征点和描述子算法。
enum class FeatureAlgorithm { fast, gftt, orb, sift, akaze, brief, freak };
/// @brief 描述子匹配器。
enum class MatcherAlgorithm { brute_force, brute_force_hamming, flann };
/// @brief 稀疏/稠密光流后端。
enum class OpticalFlowAlgorithm { lucas_kanade, farneback, rlof };

/// @brief 用于 NMS 的检测框。
struct BoundingBox {
    cv::Rect2f rect;
    float score = 0.f;
    int class_id = -1;
};

/// @brief 特征点与描述子。
struct FeatureSet {
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
};

/// @brief ratio test 后保留的匹配。
struct MatchSet {
    std::vector<cv::DMatch> matches;
    std::vector<std::uint8_t> inlier_mask;
};

/// @brief 稀疏光流跟踪结果。
struct SparseFlowResult {
    std::vector<cv::Point2f> points;
    std::vector<std::uint8_t> status;
    std::vector<float> errors;
};

/// @brief 单应矩阵及 RANSAC 内点掩码。
struct HomographyResult {
    cv::Mat matrix;
    std::vector<std::uint8_t> inlier_mask;
};

/// @brief 相机标定结果。
struct CameraCalibrationResult {
    cv::Mat camera_matrix;
    cv::Mat distortion_coefficients;
    std::vector<cv::Mat> rotation_vectors;
    std::vector<cv::Mat> translation_vectors;
    double rms_error = 0.0;
};

/// @brief ArUco 检测结果。
struct ArucoResult {
    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    std::vector<std::vector<cv::Point2f>> rejected;
};

/// @brief 执行高斯、中值或双边滤波。
Result<Image> apply_blur(const Image& image, BlurAlgorithm algorithm,
                         cv::Size kernel = {5, 5}, double sigma = 0.0);
/// @brief 执行固定阈值、Otsu、Triangle 或自适应阈值。
Result<Image> threshold_image(const Image& image, ThresholdAlgorithm algorithm,
                              double threshold = 0.0, double max_value = 255.0,
                              int block_size = 11, double constant = 2.0);
/// @brief 执行腐蚀、膨胀或形态学组合操作。
Result<Image> apply_morphology(const Image& image, MorphologyOperation operation,
                               cv::Size kernel = {3, 3}, int iterations = 1,
                               int shape = cv::MORPH_RECT);
/// @brief 执行 Canny、Sobel、Scharr 或 Laplacian 边缘检测。
Result<Image> detect_edges(const Image& image, EdgeAlgorithm algorithm,
                           double low_threshold = 50.0, double high_threshold = 150.0,
                           int aperture = 3);
/// @brief 执行仿射变换。
Result<Image> warp_affine_image(const Image& image, const cv::Mat& transform,
                                cv::Size size,
                                int interpolation = cv::INTER_LINEAR);
/// @brief 执行透视变换。
Result<Image> warp_perspective_image(const Image& image, const cv::Mat& transform,
                                     cv::Size size,
                                     int interpolation = cv::INTER_LINEAR);

/// @brief 从二值图像中提取轮廓及其几何属性。
Result<std::vector<ContourInfo>> find_contours(const Image& image,
                                               int retrieval = cv::RETR_EXTERNAL,
                                               int approximation = cv::CHAIN_APPROX_SIMPLE);
/// @brief 获取连通域标签、统计信息和质心。
Result<cv::Mat> label_components(const Image& image, cv::Mat* statistics = nullptr,
                                 cv::Mat* centroids = nullptr, int connectivity = 8);
/// @brief 检测 Hough 直线。
Result<std::vector<cv::Vec4i>> detect_lines(const Image& image, double rho = 1.0,
                                            double theta = CV_PI / 180.0,
                                            int threshold = 50);
/// @brief 检测 Hough 圆。
Result<std::vector<cv::Vec3f>> detect_circles(const Image& image, double dp = 1.0,
                                              double min_distance = 20.0,
                                              double param1 = 100.0,
                                              double param2 = 30.0);
/// @brief 按分数和 IoU 执行类别感知的非极大值抑制。
Result<std::vector<BoundingBox>> non_maximum_suppression(
    const std::vector<BoundingBox>& boxes, float score_threshold = 0.25f,
    float iou_threshold = 0.45f);

/// @brief 创建并计算特征点和描述子。
Result<FeatureSet> detect_features(const Image& image, FeatureAlgorithm algorithm,
                                   int max_features = 1000);
/// @brief 对两个特征集合执行描述子匹配和 ratio test。
Result<MatchSet> match_features(const FeatureSet& source, const FeatureSet& target,
                                MatcherAlgorithm algorithm = MatcherAlgorithm::brute_force,
                                float ratio = 0.75f);
/// @brief 使用 RANSAC 估计两组点之间的单应矩阵。
Result<HomographyResult> estimate_homography(const std::vector<cv::Point2f>& source,
                                             const std::vector<cv::Point2f>& target,
                                             double reprojection_threshold = 3.0);

/// @brief 使用 Lucas-Kanade 或 RLOF 跟踪稀疏点。
Result<SparseFlowResult> track_points(const Image& previous, const Image& current,
                                      const std::vector<cv::Point2f>& points,
                                      OpticalFlowAlgorithm algorithm =
                                          OpticalFlowAlgorithm::lucas_kanade);
/// @brief 使用 Farneback 计算稠密光流场。
Result<Image> calculate_dense_flow(const Image& previous, const Image& current,
                                   double pyramid_scale = 0.5, int levels = 3,
                                   int window_size = 15, int iterations = 3);

/// @brief 根据相机参数去除镜头畸变。
Result<Image> undistort_image(const Image& image, const cv::Mat& camera_matrix,
                              const cv::Mat& distortion_coefficients);
/// @brief 使用标定板点集执行相机标定。
Result<CameraCalibrationResult> calibrate_camera(
    const std::vector<std::vector<cv::Point3f>>& object_points,
    const std::vector<std::vector<cv::Point2f>>& image_points, cv::Size image_size);
/// @brief 检测 ArUco 标记。
Result<ArucoResult> detect_aruco_markers(const Image& image, int dictionary_id =
                                             cv::aruco::DICT_4X4_50);

} // namespace sindre::utils_2d
