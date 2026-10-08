# 3D 后端说明

VTK 是 `utils_3d` 的内部实现依赖，不属于应用层 API。用户代码应使用：

```cpp
#include <sindre/utils_3d.h>
```

公共类型为 `Mesh`、`PointCloud` 和 `SindreMesh`。网格算法通过 `Result<T>` 返回，
应用不需要包含 VTK 头文件、连接 VTK 对象或选择后端。

VTK、CGAL、PCL 的版本、CMake 查找和运行时 DLL 配置只记录在
[utils_3d 依赖说明](../dependencies/utils_3d.md)中，供库维护者使用。

仓库内部仍保留参考 vedo 设计的 VTK Mesh/Data/Image 封装，位置是
`modules/utils_3d/private/core/`；它用于实现 `Mesh`、`SindreMesh` 和后续算法，
不属于安装包的公共接口。

旧的 `utils_3d/vtk.h`、`data.h`、`image.h`、`show.h` 和 `plot.h` 接口属于历史实现，
不再作为稳定公共 API。需要新增数据集、显示或图像能力时，应先设计后端无关的高层类型，
再将第三方调用放入模块私有实现。
