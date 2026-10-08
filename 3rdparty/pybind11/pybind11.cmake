# pybind11 主要提供头文件；ExternalProject 负责固定版本下载，
# 由 Utils_py 通过接口 target 连接 Python 嵌入库并在安装时复制头文件。
include(ExternalProject)

set(SINDRE_THIRD_UTILS_PY_PYBIND11_VERSION "3.1.0")
set(SINDRE_THIRD_UTILS_PY_PYBIND11_URL
    "https://github.com/pybind/pybind11/archive/refs/tags/v3.1.0.zip")

function(sindre_3rdparty_setup_pybind11)
    if(TARGET sindre::pybind11_embed)
        return()
    endif()
    ExternalProject_Add(
        sindre_ext_pybind11
        PREFIX "${SINDRE_THIRD_PARTY_CACHE_DIR}/pybind11"
        URL "${SINDRE_THIRD_UTILS_PY_PYBIND11_URL}"
        DOWNLOAD_DIR "${SINDRE_THIRD_PARTY_DOWNLOAD_DIR}/pybind11"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND "")
    ExternalProject_Get_Property(sindre_ext_pybind11 SOURCE_DIR)
    file(MAKE_DIRECTORY "${SOURCE_DIR}/include")
    add_library(sindre_pybind11_embed INTERFACE)
    target_include_directories(sindre_pybind11_embed SYSTEM INTERFACE
        $<BUILD_INTERFACE:${SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/pybind11>)
    add_dependencies(sindre_pybind11_embed sindre_ext_pybind11)
    target_link_libraries(sindre_pybind11_embed INTERFACE Python3::Python)
    add_library(sindre::pybind11_embed ALIAS sindre_pybind11_embed)
    install(TARGETS sindre_pybind11_embed EXPORT sindreTargets)
    install(DIRECTORY "${SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/pybind11")
endfunction()
