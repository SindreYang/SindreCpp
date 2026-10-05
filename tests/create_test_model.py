"""Create a reproducible tiny ONNX fixture, not a trained model."""
from pathlib import Path
import onnx
from onnx import TensorProto, helper

path = Path(__file__).parent / "models" / "identity.onnx"
path.parent.mkdir(exist_ok=True)
graph = helper.make_graph(
    [helper.make_node("Identity", ["input"], ["output"])],
    "identity",
    [helper.make_tensor_value_info("input", TensorProto.FLOAT, ["batch", 3])],
    [helper.make_tensor_value_info("output", TensorProto.FLOAT, ["batch", 3])],
)
model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
model.ir_version = 8
onnx.checker.check_model(model)
onnx.save(model, path)
