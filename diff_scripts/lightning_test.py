#!/usr/bin/env python3
"""
Lightning Test Script - Combined depth generation and diffusion
Usage: python lightning_test.py "your prompt here"
"""

import sys
import argparse
from diffusers import StableDiffusionXLAdapterPipeline, StableDiffusionXLPipeline, T2IAdapter, EulerDiscreteScheduler, AutoencoderKL, UNet2DConditionModel
from diffusers.utils import load_image
from controlnet_aux.midas import MidasDetector
from huggingface_hub import hf_hub_download
from safetensors.torch import load_file
import torch

def main():
    parser = argparse.ArgumentParser(description='Generate lightning effect from prompt')
    parser.add_argument('prompt', help='Text prompt for image generation')
    args = parser.parse_args()

    print(f"Generating lightning effect for prompt: {args.prompt}")

    # Device setup
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Using device: {device}")

    # Step 1: Generate base image with SDXL Lightning
    print("Step 1: Generating base image with SDXL Lightning...")
    
    # Load SDXL Lightning UNet for base image generation
    base = "stabilityai/stable-diffusion-xl-base-1.0"
    repo = "ByteDance/SDXL-Lightning"
    ckpt = "sdxl_lightning_4step_unet.safetensors"
    
    unet = UNet2DConditionModel.from_config(base, subfolder="unet").to(device, torch.float16)
    unet.load_state_dict(load_file(hf_hub_download(repo, ckpt), device=device))
    
    # Load VAE
    vae = AutoencoderKL.from_pretrained("madebyollin/sdxl-vae-fp16-fix", torch_dtype=torch.float16)
    
    # Create pipeline with SDXL Lightning UNet (regular pipeline for base image, not adapter pipeline)
    base_pipe = StableDiffusionXLPipeline.from_pretrained(
        base, vae=vae, unet=unet, torch_dtype=torch.float16, variant="fp16"
    ).to(device)
    
    # Set scheduler for Lightning
    base_pipe.scheduler = EulerDiscreteScheduler.from_config(base_pipe.scheduler.config, timestep_spacing="trailing")
    
    # Generate base image using Lightning (4 steps, CFG=0)
    base_image = base_pipe(
        prompt=args.prompt,
        num_inference_steps=4,
        guidance_scale=0.0  # Lightning requires CFG=0
    ).images[0]

    base_image.save('./base.png')
    print("Base image saved as base.png")

    # Step 2: Generate depth map
    print("Step 2: Generating depth map...")
    midas_depth = MidasDetector.from_pretrained(
        "valhalla/t2iadapter-aux-models",
        filename="dpt_large_384.pt",
        model_type="dpt_large"
    ).to(device)

    depth_image = midas_depth(
        base_image, detect_resolution=512, image_resolution=1024
    )
    depth_image.save('./out_depth.png')
    print("Depth map saved as out_depth.png")

    # Step 3: Load models for lightning generation
    print("Step 3: Loading lightning models...")
    depth_image_rgb = load_image("./out_depth.png").convert("RGB")

    # Load T2I adapter
    adapter = T2IAdapter.from_pretrained(
        "TencentARC/t2i-adapter-depth-midas-sdxl-1.0",
        torch_dtype=torch.float16,
        variant="fp16"
    ).to(device)

    # Reuse the same UNet for consistency
    # Create pipeline
    pipe = StableDiffusionXLAdapterPipeline.from_pretrained(
        base, vae=vae, adapter=adapter, unet=unet, torch_dtype=torch.float16, variant="fp16"
    ).to(device)

    # Set scheduler for Lightning
    pipe.scheduler = EulerDiscreteScheduler.from_config(pipe.scheduler.config, timestep_spacing="trailing")

    # Step 4: Generate final lightning image
    print("Step 4: Generating final lightning image...")
    negative_prompt = "anime, cartoon, graphic, text, painting, crayon, graphite, abstract, glitch, deformed, mutated, ugly, disfigured"

    gen_images = pipe(
        prompt=args.prompt,
        negative_prompt=negative_prompt,
        image=depth_image_rgb,
        num_inference_steps=4,
        adapter_conditioning_scale=1,
        guidance_scale=0.0,  # Lightning requires CFG=0
    ).images[0]

    gen_images.save('./out_final.png')
    print("Final lightning image saved as out_final.png")

    print("All steps completed successfully!")

if __name__ == "__main__":
    main()
