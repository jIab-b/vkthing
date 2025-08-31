import os
import glob
import torch
from diffusers import T2IAdapter

# Initialize models
print("Initializing adapter for meta-graph export...")

# Select device and dtype with CPU fallback
device = "cuda" if torch.cuda.is_available() else "cpu"
dtype = torch.float16 if device == "cuda" else torch.float32
print(f"Using device: {device}, dtype: {dtype}")

# Initialize T2I adapter only (no UNet, no TorchScript)
# We will export an ONNX graph and strip weights into an external data file, then remove it.
adapter = T2IAdapter.from_pretrained(
    "TencentARC/t2i-adapter-depth-midas-sdxl-1.0",
    torch_dtype=(torch.float16 if device == "cuda" else torch.float32),
    variant=("fp16" if device == "cuda" else None),
)
adapter = adapter.to(device, dtype)

# Create dummy inputs
dummy_image = torch.randn(1, 3, 512, 512, device=device, dtype=dtype)

print("Exporting ONNX meta-graph for adapter (no weights in .onnx)...")

def export_onnx_and_strip_weights():
    # Export adapter
    torch.onnx.export(
        adapter,
        dummy_image,
        "adapter.onnx",
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={
            "input": {0: "batch_size", 2: "height", 3: "width"},
            "output": {0: "batch_size", 2: "height", 3: "width"},
        },
        opset_version=17,
    )

    # Force externalize all tensors into single data file then remove it
    try:
        import onnx
        from onnx import save_model
        from onnx.external_data_helper import convert_model_to_external_data

        def _externalize(model_path: str):
            if not os.path.exists(model_path):
                return False
            model = onnx.load(model_path, load_external_data=True)
            data_path = os.path.splitext(model_path)[0] + ".onnx.data"
            convert_model_to_external_data(
                model,
                all_tensors_to_one_file=True,
                location=os.path.basename(data_path),
                size_threshold=0,
                convert_attribute=False,
            )
            save_model(
                model,
                model_path,
                save_as_external_data=True,
                all_tensors_to_one_file=True,
                location=os.path.basename(data_path),
                size_threshold=0,
                convert_attribute=False,
            )
            # Remove the external data file
            if os.path.exists(data_path):
                os.remove(data_path)
            return True

        _externalize("adapter.onnx")
    except Exception as e:
        print(f"Warning: ONNX post-process failed: {e}")

    # Clean any shard files if produced
    removed = 0
    for pattern in ("onnx__*", "*.onnx.data", "unet.*"):
        for p in glob.glob(pattern):
            try:
                os.remove(p)
                removed += 1
            except OSError:
                pass
    if removed:
        print(f"Removed {removed} external shard files")

export_onnx_and_strip_weights()

print("Meta-graph export completed. Created files:")
print("- ONNX graph only: adapter.onnx (weights externalized/removed)")
