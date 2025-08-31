from diffusers import StableDiffusionXLAdapterPipeline, T2IAdapter, EulerDiscreteScheduler, AutoencoderKL, UNet2DConditionModel
from diffusers.utils import load_image
from huggingface_hub import hf_hub_download
from safetensors.torch import load_file
import torch

# Load the depth map from out_depth.png
depth_image = load_image("./out_depth.png").convert("RGB")

# Load T2I adapter
adapter = T2IAdapter.from_pretrained(
  "TencentARC/t2i-adapter-depth-midas-sdxl-1.0", torch_dtype=torch.float16, variant="fp16"
).to("cuda")

# Load SDXL Lightning UNet (using 4-step model as in the examples)
base = "stabilityai/stable-diffusion-xl-base-1.0"
repo = "ByteDance/SDXL-Lightning"
ckpt = "sdxl_lightning_4step_unet.safetensors"

dtype = torch.float16
unet = UNet2DConditionModel.from_config(base, subfolder="unet").to("cuda", dtype)
unet.load_state_dict(load_file(hf_hub_download(repo, ckpt), device="cuda"))

# Load VAE
vae = AutoencoderKL.from_pretrained("madebyollin/sdxl-vae-fp16-fix", torch_dtype=torch.float16)

# Create pipeline with SDXL Lightning UNet
pipe = StableDiffusionXLAdapterPipeline.from_pretrained(
    base, vae=vae, adapter=adapter, unet=unet, torch_dtype=torch.float16, variant="fp16"
).to("cuda")

# Set scheduler for Lightning (trailing timesteps)
pipe.scheduler = EulerDiscreteScheduler.from_config(pipe.scheduler.config, timestep_spacing="trailing")

#pipe.enable_xformers_memory_efficient_attention()

prompt = "ancient statues along a river"
negative_prompt = "anime, cartoon, graphic, text, painting, crayon, graphite, abstract, glitch, deformed, mutated, ugly, disfigured"

# Generate image using Lightning (4 steps, CFG=0)
gen_images = pipe(
  prompt=prompt,
  negative_prompt=negative_prompt,
  image=depth_image,
  num_inference_steps=4,
  adapter_conditioning_scale=1,
  guidance_scale=0.0,  # Lightning requires CFG=0
).images[0]

# Save final image as out_final.png
gen_images.save('out_final.png')

print("Image generated and saved as out_final.png")
