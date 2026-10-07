# utils_3d module

本文面向需要 VTK、SindreMesh 或几何后端的使用者，说明 3D 模块的公共入口、功能
开关和测试边界；SDK 要求见 [Utils_3d 依赖说明](../dependencies/utils_3d.md)。

`sindre::utils_3d` is a static library target. VTK/Eigen integration is owned by
the module target. Public headers expose declarations, value types, and the small
template forwarding adapters only; Mesh/Data/Image, algorithms, rendering, plotting,
and the facade implementations are compiled from `modules/utils_3d/src/`. Backend
options remain controlled by the existing CMake switches.

Enable with `SINDRE_WITH_UTILS_3D=ON`. The module discovers VTK 9 and Eigen3;
it can fetch Eigen when no package is available. `SINDRE_UTILS_3D_SHOW` adds
viewer components, while `SINDRE_UTILS_3D_VTK_DATA` adds data/image helpers.

Tests are `sindre.utils_3d` and `sindre.mesh`, with additional targets for
show/data features. Keep heavyweight backend checks opt-in and document any
new SDK-specific include ordering in this file and the development guide.

The VTK 9.7.1 official SDK was verified with clang-cl. Core, mesh, VTK data
and offscreen show tests passed locally, including PNG output, texture/data
arrays, picking/event callbacks and moved-from mesh reuse. Rendering tests are
headless (`offscreen=true`), but the VTK runtime DLL directory is still needed
on `PATH`.
