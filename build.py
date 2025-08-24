#!/usr/bin/env python3
import argparse
import os
import platform
import shutil
import subprocess
import sys


def which(prog: str):
    return shutil.which(prog)


def run(cmd: list, cwd: str | None = None):
    print("$", " ".join(cmd))
    subprocess.check_call(cmd, cwd=cwd)


def is_windows():
    return sys.platform.startswith("win")


def default_generator():
    # Prefer Ninja if available; otherwise VS on Windows, Unix Makefiles on others
    if which("ninja"):
        return "Ninja"
    if is_windows():
        # Best effort default; user can override with --generator
        return "Visual Studio 17 2022"
    return "Unix Makefiles"


def cmake_configure(build_dir: str, generator: str | None, config: str):
    gen = generator or default_generator()
    args = ["cmake", "-S", ".", "-B", build_dir, "-G", gen]
    # For single-config generators, pass CMAKE_BUILD_TYPE
    single_config_gens = {"Ninja", "Unix Makefiles", "NMake Makefiles"}
    if gen in single_config_gens:
        args += [f"-DCMAKE_BUILD_TYPE={config}"]
    run(args)


def cmake_build(build_dir: str, config: str, parallel: int | None):
    args = ["cmake", "--build", build_dir, "--config", config]
    # CMake 3.16 supports --parallel
    if parallel and parallel > 1:
        args += ["--parallel", str(parallel)]
    run(args)


def check_env():
    ok = True
    # Vulkan / glslangValidator
    glslang = which("glslangValidator")
    if not glslang:
        if is_windows() and os.environ.get("VULKAN_SDK"):
            # Try common Vulkan SDK Bin directories
            bin32 = os.path.join(os.environ["VULKAN_SDK"], "Bin32", "glslangValidator.exe")
            bin64 = os.path.join(os.environ["VULKAN_SDK"], "Bin", "glslangValidator.exe")
            if not (os.path.isfile(bin32) or os.path.isfile(bin64)):
                print("[WARN] glslangValidator not found in VULKAN_SDK. Shader compilation will fail.")
                ok = False
        else:
            print("[WARN] glslangValidator not found on PATH. Ensure Vulkan SDK is installed.")
            ok = False

    if not is_windows():
        # Linux: check glfw via pkg-config (as per CMakeLists)
        if which("pkg-config"):
            try:
                subprocess.check_call(["pkg-config", "--exists", "glfw3"])
            except subprocess.CalledProcessError:
                print("[WARN] pkg-config cannot find glfw3. Install it (e.g., sudo apt install libglfw3-dev).")
                ok = False
        else:
            print("[WARN] pkg-config not found; GLFW discovery may fail.")

    # GLM and Vulkan are discovered by CMake; we warn but do not hard fail here
    return ok


def find_executable(build_dir: str, config: str):
    # Try to locate the sandbox app built by CMake
    if is_windows():
        # Multi-config (VS) layout: build/app/sandbox/<Config>/sandbox.exe
        path_vs = os.path.join(build_dir, "app", "sandbox", config, "sandbox.exe")
        # Single-config (Ninja) layout: build/app/sandbox/sandbox.exe
        path_ninja = os.path.join(build_dir, "app", "sandbox", "sandbox.exe")
        return path_vs if os.path.isfile(path_vs) else (path_ninja if os.path.isfile(path_ninja) else None)
    else:
        path = os.path.join(build_dir, "app", "sandbox", "sandbox")
        return path if os.path.isfile(path) else None


def main():
    parser = argparse.ArgumentParser(description="All-in-one build script for vkthing (Linux/Windows)")
    parser.add_argument("--build-dir", default="build", help="Build directory (default: build)")
    parser.add_argument("--config", default="Release", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], help="Build type/config")
    parser.add_argument("--generator", default=None, help="CMake generator override (e.g., Ninja, Visual Studio 17 2022)")
    parser.add_argument("--clean", action="store_true", help="Remove build directory before configuring")
    parser.add_argument("--run", action="store_true", help="Run sandbox after build")
    parser.add_argument("-j", "--jobs", type=int, default=None, help="Parallel build jobs (default: CMake decides)")

    args = parser.parse_args()

    print(f"[INFO] Host: {platform.system()} {platform.release()} | Python {platform.python_version()}")
    env_ok = check_env()

    build_dir = os.path.abspath(args.build_dir)
    if args.clean and os.path.isdir(build_dir):
        print(f"[INFO] Removing build directory: {build_dir}")
        shutil.rmtree(build_dir)

    os.makedirs(build_dir, exist_ok=True)

    try:
        cmake_configure(build_dir, args.generator, args.config)
        cmake_build(build_dir, args.config, args.jobs)
    except subprocess.CalledProcessError as e:
        print("[ERROR] Build failed:", e)
        sys.exit(1)

    exe = find_executable(build_dir, args.config)
    if exe:
        print(f"[INFO] Built sandbox: {exe}")
    else:
        print("[WARN] Could not find built sandbox executable (build may have failed or layout differs).")

    if args.run:
        if not exe:
            print("[ERROR] Cannot run; executable not found.")
            sys.exit(2)
        # Ensure shaders are copied next to the executable (handled by CMake POST_BUILD), just run
        try:
            run([exe])
        except subprocess.CalledProcessError as e:
            print("[ERROR] Sandbox run failed:", e)
            sys.exit(e.returncode)

    if not env_ok:
        print("[NOTE] Some environment checks failed; if CMake succeeded you can ignore this.")


if __name__ == "__main__":
    main()

