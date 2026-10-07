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

从仓库根目录构建时，推荐使用预设：

```powershell
$env:SINDRE_TENSORRT_ROOT = "C:\Program Files\NVIDIA\TensorRT-10.11.0.33"
$env:CUDAToolkit_ROOT = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9"
cmake --preset ai-trt-full
cmake --build --preset ai-trt-full

cmake --preset ai-trt-dispatch
cmake --build --preset ai-trt-dispatch
```

预设输出目录分别是 `build/ai-trt-full` 和 `build/ai-trt-dispatch`。如果只构建本示例，
仍可使用下面的独立 CMake 命令。

```powershell
cmake -S examples/ai_tensorrt_segmentation -B build_ai_seg -G Ninja `
  -DSINDRE_TENSORRT_ROOT=C:/TensorRT
cmake --build build_ai_seg --parallel

build_ai_seg/sindre_example_ai_tensorrt_segmentation.exe `
  --onnx models/fcn-resnet50-12.onnx `
  --input sample.ppm `
  --engine artifacts/fcn.plan `
  --output artifacts/overlay.ppm `
  --mask artifacts/mask.pgm
```

默认是 `FULL` 运行时，程序可以在本机把 ONNX 转成 engine。只部署预构建 engine
时可以使用更小的 Dispatch 运行时：

```powershell
cmake -S examples/ai_tensorrt_segmentation -B build_ai_seg_dispatch -G Ninja `
  -DSINDRE_TENSORRT_ROOT="C:/Program Files/NVIDIA/TensorRT-10.11.0.33" `
  -DSINDRE_AI_TRT_RUNTIME=DISPATCH
cmake --build build_ai_seg_dispatch --parallel

build_ai_seg_dispatch/sindre_example_ai_tensorrt_segmentation.exe `
  --input sample.ppm --engine artifacts/fcn.plan `
  --lean-runtime nvinfer_lean_10.dll
```

Dispatch 不包含 ONNX parser；没有已有 engine 时，`try_convert_onnx()` 会明确返回
`function_not_supported`。构建外置 Lean plan 时，在 Full 构建机使用
`--version-compatible --exclude-lean-runtime`；Full Runtime 可以直接加载该 plan，
而 Dispatch 部署仍要提供匹配的 `nvinfer_lean_10.dll`。

首次运行会构建 engine；已有 engine 时直接复用，不会覆盖它。示例使用输入图片的
实际尺寸建立固定 optimization profile，因此同一个 engine 只接受同样的 `H x W`。

## 兼容性和部署体积

默认使用 `same_compute_capability`，通常比最宽的兼容模式性能更好。需要在 Ampere
及更新架构之间迁移时使用 `--portable`，代价是 TensorRT 可选 tactics 变少，性能
可能下降。

本仓库在 RTX 3060 Laptop、TensorRT 10.11.0.33、CUDA 12.9 上的参考结果如下：

| 配置 | GPU 延迟均值 | 吞吐 | 输出 mask |
| --- | ---: | ---: | --- |
| `build_gpu` | 2.37257 ms | 421.071 qps | 与其他配置一致 |
| `same_compute_capability` | 2.38387 ms | 419.043 qps | 与其他配置一致 |
| `ampere_plus` / `--portable` | 3.47930 ms | 287.225 qps | 与其他配置一致 |

这是固定模型和输入下的性能参考，不是所有 GPU、模型或 TensorRT 版本的保证；正式
发布必须在目标硬件和业务验证集上重新测量。

需要跨 TensorRT 10.x 版本时可组合：

```powershell
... --version-compatible --exclude-lean-runtime --lean-runtime C:/TensorRT/lib/nvinfer_lean_10.dll
```

`--exclude-lean-runtime` 只允许和 `--version-compatible` 一起使用。生产环境不要
默认打开 `--allow-engine-host-code`；只有 engine 来源受信任且确实包含 host code
时才显式打开。Windows 下运行目录必须包含 CUDA、TensorRT 和外部 lean runtime 的
DLL；独立 CMake 会尝试复制已发现的运行时 DLL，但发布包仍应由部署流程固定版本、
校验哈希并进行 GPU smoke test。
