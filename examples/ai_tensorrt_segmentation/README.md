# TensorRT 语义分割示例

这个示例展示一条接近生产部署的最小路径：

1. 用 TensorRT 将外部 ONNX 构建成 engine；
2. 通过 `sindre::ai::trt::Model::try_create()` 加载；
3. 通过 `try_infer()` 执行一次 NCHW `float32` 推理；
4. 输出类别索引 PGM 和半透明彩色 PPM。

示例只依赖 PPM 读写，不额外引入 OpenCV。输入模型使用 ONNX Model Zoo 的
FCN ResNet-50 语义分割模型。它接受 RGB `[0, 1]` 后按 ImageNet mean/std
归一化，输出 `out` 为 `[1, 21, H, W]`；本示例当前只做 `/255`，因此如果要用于
精度验证，应在业务预处理处补上该模型要求的 mean/std。模型说明和许可见：

<https://github.com/onnx/models/tree/main/validated/vision/object_detection_segmentation/fcn>

GitHub 上的 ONNX Model Zoo 使用 Git LFS，模型文件不提交到本仓库。模型来源和
网络结构说明仍以 GitHub 为准；当前大文件由 ONNX Model Zoo 的 Hugging Face
镜像提供。GitHub 页面给出的历史下载地址是：

```text
https://github.com/onnx/models/raw/main/vision/object_detection_segmentation/fcn/model/fcn-resnet50-12.onnx
```

也可以在示例目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\download_model.ps1
```

下载脚本使用当前镜像地址。上游 README 带有 MIT 许可声明，而镜像 model card
标注 Apache-2.0；正式发布前应按实际文件和上游许可证文件完成法务确认。脚本只
做最小文件完整性检查，不内置随时间变化的哈希值；生产供应链必须固定提交/版本
和 SHA-256。

## 配置和运行

需要 TensorRT 10.11+、CUDA，以及 Ninja 和 clang/clang-cl。模型的输入名历史上通常
是 `input.1`；如果你的导出文件不同，用 `--input-name` 指定实际名称。

```powershell
cmake -S examples/ai_tensorrt_segmentation -B build_ai_seg -G Ninja `
  -DSINDRE_TENSORRT_ROOT=C:/TensorRT
cmake --build build_ai_seg --parallel

build_ai_seg/bin/sindre_example_ai_tensorrt_segmentation.exe `
  --onnx models/fcn-resnet50-12.onnx `
  --input sample.ppm `
  --engine artifacts/fcn.plan `
  --output artifacts/overlay.ppm `
  --mask artifacts/mask.pgm
```

首次运行会构建 engine；已有 engine 时直接复用，不会覆盖它。示例使用输入图片的
实际尺寸建立固定 optimization profile，因此同一个 engine 只接受同样的 `H x W`。

## 兼容性和部署体积

默认使用 `same_compute_capability`，通常比最宽的兼容模式性能更好。需要在 Ampere
及更新架构之间迁移时使用 `--portable`，代价是 TensorRT 可选 tactics 变少，性能
可能下降。

需要跨 TensorRT 10.x 版本时可组合：

```powershell
... --version-compatible --exclude-lean-runtime --lean-runtime C:/TensorRT/lib/lean.dll
```

`--exclude-lean-runtime` 只允许和 `--version-compatible` 一起使用。生产环境不要
默认打开 `--allow-engine-host-code`；只有 engine 来源受信任且确实包含 host code
时才显式打开。Windows 下运行目录必须包含 CUDA、TensorRT 和外部 lean runtime 的
DLL；独立 CMake 会尝试复制已发现的运行时 DLL，但发布包仍应由部署流程固定版本、
校验哈希并进行 GPU smoke test。
