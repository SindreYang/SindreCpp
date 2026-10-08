#include <sindre/utils_2d/algorithms.h>

#include <cmath>
#include <iostream>

namespace {

bool ok(bool value, const char* message) {
    if (!value)
        std::cerr << message << '\n';
    return value;
}

} // namespace

int main() {
    using namespace sindre::utils_2d;
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        Image image(160, 200, CV_8UC3, cv::Scalar::all(0));
        cv::rectangle(image, {30, 30, 90, 70}, cv::Scalar(255, 255, 255), cv::FILLED);
        cv::circle(image, {145, 80}, 25, cv::Scalar(255, 255, 255), cv::FILLED);
        cv::line(image, {10, 150}, {190, 150}, cv::Scalar(255, 255, 255), 2);

        auto blurred = apply_blur(image, BlurAlgorithm::gaussian);
        if (!ok(static_cast<bool>(blurred), "gaussian blur failed")) return 1;
        auto thresholded = threshold_image(image, ThresholdAlgorithm::otsu);
        if (!ok(static_cast<bool>(thresholded), "threshold failed")) return 2;
        auto morphology = apply_morphology(thresholded.value(), MorphologyOperation::open);
        if (!ok(static_cast<bool>(morphology), "morphology failed")) return 3;
        auto edges = detect_edges(image, EdgeAlgorithm::canny);
        if (!ok(static_cast<bool>(edges), "edge detection failed")) return 4;
        auto affine = warp_affine_image(image, cv::Mat::eye(2, 3, CV_64F), image.size());
        if (!ok(static_cast<bool>(affine), "affine warp failed")) return 5;
        auto perspective = warp_perspective_image(image, cv::Mat::eye(3, 3, CV_64F), image.size());
        if (!ok(static_cast<bool>(perspective), "perspective warp failed")) return 6;

        auto contours = find_contours(thresholded.value());
        if (!ok(static_cast<bool>(contours) && !contours.value().empty(), "contour detection failed")) return 7;
        auto components = label_components(thresholded.value());
        if (!ok(static_cast<bool>(components), "connected components failed")) return 8;
        auto lines = detect_lines(edges.value());
        if (!ok(static_cast<bool>(lines), "line detection failed")) return 9;
        auto circles = detect_circles(image);
        if (!ok(static_cast<bool>(circles), "circle detection failed")) return 10;

        std::vector<BoundingBox> boxes{
            {{0, 0, 20, 20}, 0.9f, 1},
            {{1, 1, 20, 20}, 0.8f, 1},
            {{1, 1, 20, 20}, 0.8f, 2}};
        auto kept = non_maximum_suppression(boxes, 0.1f, 0.5f);
        if (!ok(static_cast<bool>(kept) && kept.value().size() == 2, "NMS failed")) return 11;

        for (const auto algorithm : {FeatureAlgorithm::fast, FeatureAlgorithm::gftt,
                                     FeatureAlgorithm::orb, FeatureAlgorithm::sift,
                                     FeatureAlgorithm::akaze, FeatureAlgorithm::brief,
                                     FeatureAlgorithm::freak}) {
            auto features = detect_features(image, algorithm, 300);
            if (!features) {
                std::cerr << "feature detection failed: " << features.error().describe() << '\n';
                return 12;
            }
        }
        auto source_features = detect_features(image, FeatureAlgorithm::orb, 300);
        auto target_features = detect_features(image, FeatureAlgorithm::orb, 300);
        if (!ok(source_features && target_features, "ORB setup failed")) return 13;
        auto matches = match_features(source_features.value(), target_features.value(),
                                      MatcherAlgorithm::brute_force_hamming);
        if (!ok(static_cast<bool>(matches), "feature matching failed")) return 14;

        const std::vector<cv::Point2f> source{{0, 0}, {10, 0}, {10, 10}, {0, 10}, {5, 5}};
        const std::vector<cv::Point2f> target{{2, 3}, {12, 3}, {12, 13}, {2, 13}, {7, 8}};
        auto homography = estimate_homography(source, target);
        if (!ok(static_cast<bool>(homography), "homography failed")) return 15;

        Image previous(80, 100, CV_8UC1, cv::Scalar::all(0));
        Image current(80, 100, CV_8UC1, cv::Scalar::all(0));
        cv::circle(previous, {30, 30}, 4, cv::Scalar::all(255), cv::FILLED);
        cv::circle(current, {34, 33}, 4, cv::Scalar::all(255), cv::FILLED);
        auto sparse = track_points(previous, current, {{30.f, 30.f}});
        if (!ok(sparse && sparse.value().status.size() == 1, "sparse flow failed")) return 16;
        auto dense = calculate_dense_flow(previous, current);
        if (!ok(static_cast<bool>(dense) && dense.value().channels() == 2, "dense flow failed")) return 17;

        auto undistorted = undistort_image(image, cv::Mat::eye(3, 3, CV_64F), cv::Mat::zeros(1, 5, CV_64F));
        if (!ok(static_cast<bool>(undistorted), "undistort failed")) return 18;

        auto dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
        Image marker(140, 140, CV_8UC1, cv::Scalar::all(255));
        Image marker_body;
        cv::aruco::generateImageMarker(dictionary, 0, 100, marker_body);
        marker_body.copyTo(marker(cv::Rect(20, 20, 100, 100)));
        auto markers = detect_aruco_markers(marker);
        if (!ok(markers && markers.value().ids.size() == 1 && markers.value().ids[0] == 0,
                "ArUco detection failed")) return 19;

        if (!ok(!apply_blur(image, BlurAlgorithm::gaussian, {4, 4}), "invalid blur accepted")) return 20;
        if (!ok(!non_maximum_suppression({{{0, 0, 0, 0}, 1.f, 0}}), "invalid NMS accepted")) return 21;
        return 0;
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 22;
    }
#endif
}
