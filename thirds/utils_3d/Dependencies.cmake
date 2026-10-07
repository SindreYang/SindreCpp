set(SINDRE_THIRD_UTILS_3D_VTK_MIN_VERSION
    "9" CACHE STRING "Minimum VTK major version")

# Optional backends are consumed as prebuilt SDKs or package-manager targets.
# This registry records the tested profile and never triggers source builds.
set(SINDRE_THIRD_UTILS_3D_CGAL_MIN_VERSION
    "5.6.1" CACHE STRING "Minimum CGAL version for the optional backend")
set(SINDRE_THIRD_UTILS_3D_LIBIGL_PORT_VERSION
    "2.5.0" CACHE STRING "Validated libigl vcpkg port version")
set(SINDRE_THIRD_UTILS_3D_LIBIGL_PACKAGE_VERSION
    "2.4.0" CACHE STRING "libigl CMake package version reported by the validated SDK")
set(SINDRE_THIRD_UTILS_3D_OPEN3D_VERSION
    "0.19.0" CACHE STRING "Validated Open3D SDK version")
set(SINDRE_THIRD_UTILS_3D_MESHLIB_VERSION
    "3.1.4.297" CACHE STRING "Validated MeshLib SDK version")
