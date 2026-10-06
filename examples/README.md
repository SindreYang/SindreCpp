# SindreCpp examples

每个示例都是一个可以单独配置的 CMake 项目，目录中包含自己的
`CMakeLists.txt` 和 `main.cpp`。从仓库根目录配置时，示例会随
`SINDRECPP_BUILD_EXAMPLES=ON` 自动加入；带可选依赖的示例只有在对应模块
启用后才会加入。

## 根工程构建

```bash
cmake --preset windows-clang
cmake --build build_win --target sindrecpp_example_general_basics
build_win/bin/sindrecpp_example_general_basics.exe
```

默认构建的 `general_basics` 展示字符串、Result、版本、Base64、scope guard、
临时文件和 UTF-8 文件读写。

启用 JSON 示例：

```bash
cmake --preset windows-clang -DSINDRECPP_WITH_JSON=ON
cmake --build build_win --target sindrecpp_example_general_json_config
```

启用 Utils3d 示例时需要本机可用的 VTK 9：

```bash
cmake --preset windows-clang -DSINDRECPP_WITH_UTILS3D=ON
cmake --build build_win --target sindrecpp_example_utils3d_mesh
```

## 单独配置一个示例

```bash
cmake -S examples/general_basics -B build_general_basics
cmake --build build_general_basics
build_general_basics/bin/sindrecpp_example_general_basics.exe
```

JSON 和 Utils3d 示例也可以用相同方式从自己的目录配置；它们会自动启用
需要的 SindreCpp 模块，并在依赖缺失时由 CMake 给出明确错误。
