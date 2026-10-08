set(SINDRE_THIRD_UTILS_3D_VTK_MIN_VERSION
    "9" CACHE STRING "Minimum VTK major version")

# Formal backends are consumed as prebuilt SDKs or package-manager targets.
# This registry records the pinned stable profile and never triggers source builds.
set(SINDRE_THIRD_UTILS_3D_CGAL_MIN_VERSION
    "6.2.1" CACHE STRING "Minimum stable CGAL version for the mesh backend")
set(SINDRE_THIRD_UTILS_3D_PCL_MIN_VERSION
    "1.15.1" CACHE STRING "Minimum stable PCL version for the point-cloud backend")
