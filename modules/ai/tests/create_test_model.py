"""Create reproducible tiny ONNX fixtures, not trained models."""
from pathlib import Path
import onnx
from onnx import TensorProto, helper

def create_identity(path: Path, name: str, tensor_type: int) -> None:
    graph = helper.make_graph(
        [helper.make_node("Identity", ["input"], ["output"])],
        name,
        [helper.make_tensor_value_info("input", tensor_type, ["batch", 3])],
        [helper.make_tensor_value_info("output", tensor_type, ["batch", 3])],
    )
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
    model.ir_version = 8
    onnx.checker.check_model(model)
    onnx.save(model, path)


models = Path(__file__).parent / "models"
models.mkdir(exist_ok=True)
create_identity(models / "identity.onnx", "identity", TensorProto.FLOAT)
create_identity(models / "identity_int32.onnx", "identity_int32", TensorProto.INT32)
create_identity(models / "identity_float16.onnx", "identity_float16", TensorProto.FLOAT16)
create_identity(models / "identity_int64.onnx", "identity_int64", TensorProto.INT64)
create_identity(models / "identity_bool.onnx", "identity_bool", TensorProto.BOOL)
