from diffusers.utils import load_image
from controlnet_aux.midas import MidasDetector
import torch

# Load the image from base.png
image = load_image("./base.png")

# Create depth map using MidasDetector
midas_depth = MidasDetector.from_pretrained(
  "valhalla/t2iadapter-aux-models", filename="dpt_large_384.pt", model_type="dpt_large"
).to("cuda")

depth_image = midas_depth(
  image, detect_resolution=512, image_resolution=1024
)

# Save depth map as out_depth.png
depth_image.save('./out_depth.png')

print("Depth map generated and saved as out_depth.png")