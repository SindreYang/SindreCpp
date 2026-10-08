# VTK 的最低版本约束。VTK 属于 Utils_3d 的后端，具体组件列表和查找逻辑
# 放在模块 CMake 中，避免未启用 3D 模块时引入大型依赖。
set(SINDRE_THIRD_UTILS_3D_VTK_MIN_VERSION
    "9" CACHE STRING "Minimum VTK major version")
