set(SINDRE_THIRD_UTILS_2D_OPENCV_MIN_VERSION
    "4.8" CACHE INTERNAL "Minimum OpenCV version for the ArUco/contrib API" FORCE)
set(SINDRE_THIRD_UTILS_2D_OPENCV_COMPONENTS
    core imgproc imgcodecs highgui features2d calib3d video videoio objdetect
    aruco optflow tracking xfeatures2d
    CACHE INTERNAL "OpenCV and opencv_contrib components required by Utils_2d" FORCE)
