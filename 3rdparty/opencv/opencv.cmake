# Utils_2d 所需的 OpenCV 最低版本和组件清单集中维护，
# 具体 find_package 与 target 链接在 modules/utils_2d/CMakeLists.txt 中完成。
set(SINDRE_THIRD_UTILS_2D_OPENCV_MIN_VERSION
    "4.12.0" CACHE INTERNAL "Fixed OpenCV version for the ArUco/contrib API" FORCE)
set(SINDRE_THIRD_UTILS_2D_OPENCV_LINUX_MIN_VERSION
    "4.6.0" CACHE INTERNAL "Minimum Linux OpenCV version for the ArUco/contrib API" FORCE)
set(SINDRE_THIRD_UTILS_2D_OPENCV_COMPONENTS
    core imgproc imgcodecs highgui features2d calib3d video videoio objdetect
    aruco optflow tracking xfeatures2d
    CACHE INTERNAL "OpenCV and opencv_contrib components required by Utils_2d" FORCE)
