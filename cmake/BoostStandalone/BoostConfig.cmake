# Minimal package config for an official standalone CGAL SDK and an external
# Boost header SDK. This intentionally does not locate or expose vcpkg targets.
set(_sindre_boost_root "${BOOST_ROOT}")
if(NOT _sindre_boost_root)
    set(_sindre_boost_root "${Boost_ROOT}")
endif()
if(NOT _sindre_boost_root)
    message(FATAL_ERROR
        "The sindre standalone CGAL profile requires BOOST_ROOT or Boost_ROOT "
        "to point to an official standalone Boost SDK; refusing to inspect "
        "CMAKE_PREFIX_PATH or vcpkg for Boost.")
endif()
if(_sindre_boost_root MATCHES "[Vv][Cc][Pp][Kk][Gg]")
    message(FATAL_ERROR
        "The sindre standalone CGAL profile rejects vcpkg Boost; "
        "use an official standalone Boost SDK: ${_sindre_boost_root}")
endif()
set(Boost_FOUND TRUE)
if(EXISTS "${_sindre_boost_root}/boost/version.hpp")
    set(Boost_INCLUDE_DIR "${_sindre_boost_root}")
else()
    set(Boost_INCLUDE_DIR "${_sindre_boost_root}/include")
endif()
if(NOT EXISTS "${Boost_INCLUDE_DIR}/boost/version.hpp")
    set(Boost_FOUND FALSE)
    message(FATAL_ERROR
        "The standalone Boost SDK does not contain boost/version.hpp: ${_sindre_boost_root}")
endif()
file(STRINGS "${Boost_INCLUDE_DIR}/boost/version.hpp" _sindre_boost_version_line
     REGEX "^#define BOOST_VERSION [0-9]+")
if(_sindre_boost_version_line MATCHES "BOOST_VERSION ([0-9]+)")
    set(_sindre_boost_version_number "${CMAKE_MATCH_1}")
    math(EXPR _sindre_boost_major "${_sindre_boost_version_number} / 100000")
    math(EXPR _sindre_boost_minor "(${_sindre_boost_version_number} / 100) % 1000")
    math(EXPR _sindre_boost_patch "${_sindre_boost_version_number} % 100")
    # Keep Boost_VERSION in the numeric format used by upstream BoostConfig
    # (for example 108600 for Boost 1.86.0).  CGAL and CMake compare this
    # value numerically; concatenating 1, 86 and 0 would produce 1860 and
    # make version checks ambiguous.
    set(Boost_VERSION "${_sindre_boost_version_number}")
    set(Boost_VERSION_STRING
        "${_sindre_boost_major}.${_sindre_boost_minor}.${_sindre_boost_patch}")
endif()
set(Boost_INCLUDE_DIRS "${Boost_INCLUDE_DIR}")
if(NOT TARGET Boost::boost)
    add_library(Boost::boost INTERFACE IMPORTED)
    set_target_properties(Boost::boost PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIR}")
endif()
