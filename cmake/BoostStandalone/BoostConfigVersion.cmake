# CMake version file for the standalone header-only Boost adapter.
# CGAL requests a minimum Boost version; this project fixes the SDK profile to
# Boost 1.86.0 and accepts compatible lower minimum requests.
set(PACKAGE_VERSION "1.86.0")
if(PACKAGE_FIND_VERSION VERSION_LESS_EQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
else()
    set(PACKAGE_VERSION_UNSUITABLE TRUE)
endif()
