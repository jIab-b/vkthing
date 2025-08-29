#!/usr/bin/env python3
import argparse
import os
import platform
import shutil
import subprocess
import sys
from typing import Optional


def which(prog: str) -> Optional[str]:
    return shutil.which(prog)


def run(cmd: list[str], cwd: Optional[str] = None):
    print("$", " ".join(cmd))
    subprocess.check_call(cmd, cwd=cwd)


def is_windows() -> bool:
    return sys.platform.startswith("win")


def default_generator() -> str:
    # Default to MSVC (VS 2022). Users can override with --generator Ninja.
    return "Visual Studio 17 2022"


def is_single_config(gen: str) -> bool:
    return gen in {"Ninja", "Unix Makefiles", "NMake Makefiles"}


def read_cached_generator(build_dir: str) -> Optional[str]:
    cache = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache):
        return None
    try:
        with open(cache, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if line.startswith("CMAKE_GENERATOR:"):
                    # format: CMAKE_GENERATOR:INTERNAL=Generator Name
                    parts = line.strip().split("=", 1)
                    if len(parts) == 2:
                        return parts[1]
    except Exception:
        return None
    return None


def cmake_configure(build_dir: str, generator: Optional[str], config: str, build_samples: bool, arch: str, glfw_dir: Optional[str]):
    gen = generator or default_generator()
    # Detect cache mismatch and guide the user
    cached = read_cached_generator(build_dir)
    if cached and cached != gen:
        raise RuntimeError(
            f"CMake build dir '{build_dir}' already configured with generator '{cached}'. "
            f"Requested '{gen}'. Use --clean or a different --build-dir."
        )
    args = ["cmake", "-S", ".", "-B", build_dir, "-G", gen, f"-DBUILD_SAMPLES={'ON' if build_samples else 'OFF'}"]
    # For Visual Studio generator, set architecture explicitly (x64 by default)
    if "Visual Studio" in gen and arch:
        args += ["-A", arch]
    # GLFW discovery: prioritize third_party, then allow explicit directory or environment hints
    # Preferred: third_party directory first, then explicit paths
    third_party_glfw = os.path.join(os.getcwd(), "third_party", "glfw")
    glfw_dir = (
        glfw_dir
        or (third_party_glfw if os.path.isdir(third_party_glfw) else None)
        or os.environ.get("glfw3_DIR")
        or os.environ.get("GLFW_DIR")
        or os.environ.get("GLFW_ROOT")
        or os.environ.get("GLFW_PATH")
        or os.environ.get("GFLW_PATH")  # user typo compatibility
    )
    # If not provided, probe common install under Program Files
    if not glfw_dir:
        pf = os.environ.get("ProgramFiles")
        pfx86 = os.environ.get("ProgramFiles(x86)")
        candidates = []
        if pf:
            candidates.append(os.path.join(pf, "glfw"))
        if pfx86:
            candidates.append(os.path.join(pfx86, "glfw"))
        for c in candidates:
            if os.path.isdir(c):
                glfw_dir = c
                break
    if glfw_dir:
        cmake_cfg = os.path.join(glfw_dir, "glfw3Config.cmake")
        if os.path.isfile(cmake_cfg):
            args += [f"-Dglfw3_DIR={glfw_dir}"]
        else:
            # Try common subdir layout if a root path was given
            guess = os.path.join(glfw_dir, "lib", "cmake", "glfw3")
            if os.path.isfile(os.path.join(guess, "glfw3Config.cmake")):
                args += [f"-Dglfw3_DIR={guess}"]
            else:
                # No CMake config: pass include+lib explicitly to engine CMake
                inc = os.path.join(glfw_dir, "include")
                subdirs = [
                    "lib", "lib64",
                    "lib-vc2022", "lib-vc2019", "lib-vc2017", "lib-vc2015", "lib-vc2013",
                    "lib-static-ucrt", "lib-mingw-w64",
                ]
                lib_candidates = []
                for sd in subdirs:
                    lib_candidates.append(os.path.join(glfw_dir, sd, "glfw3.lib"))
                    lib_candidates.append(os.path.join(glfw_dir, sd, "glfw3dll.lib"))
                lib = next((p for p in lib_candidates if os.path.isfile(p)), None)
                if os.path.isdir(inc) and lib:
                    args += [f"-DGLFW_INCLUDE_DIRS={inc}", f"-DGLFW_LINK_LIBRARIES={lib}"]
                else:
                    # As a last resort, add to CMAKE_PREFIX_PATH to help CMake search
                    args += [f"-DCMAKE_PREFIX_PATH={glfw_dir}"]
    
    # Set up third_party paths for CMake
    third_party_dir = os.path.join(os.getcwd(), "third_party")
    args += [f"-DTHIRD_PARTY_DIR={third_party_dir}"]
    
    if is_single_config(gen):
        args += [f"-DCMAKE_BUILD_TYPE={config}"]
    run(args)


def cmake_build(build_dir: str, config: str, parallel: Optional[int]):
    args = ["cmake", "--build", build_dir, "--config", config]
    if parallel and parallel > 1:
        args += ["--parallel", str(parallel)]
    run(args)


def check_env() -> bool:
    ok = True
    # glslangValidator from Vulkan SDK
    if not which("glslangValidator"):
        vsdk = os.environ.get("VULKAN_SDK")
        if not vsdk:
            print("[WARN] glslangValidator not found and VULKAN_SDK not set. Install Vulkan SDK and open a Developer Command Prompt.")
            ok = False
        else:
            cand = [os.path.join(vsdk, "Bin", "glslangValidator.exe"), os.path.join(vsdk, "Bin32", "glslangValidator.exe")]
            if not any(os.path.isfile(p) for p in cand):
                print("[WARN] glslangValidator not found under VULKAN_SDK Bin directories.")
                ok = False
    # GLFW is resolved via CMake (vcpkg/CONFIG). We'll warn but not fail.
    return ok


def find_executable(build_dir: str, config: str) -> Optional[str]:
    # VS multi-config
    path_vs = os.path.join(build_dir, "app", "sandbox", config, "sandbox.exe")
    # Ninja single-config
    path_ninja = os.path.join(build_dir, "app", "sandbox", "sandbox.exe")
    if os.path.isfile(path_vs):
        return path_vs
    if os.path.isfile(path_ninja):
        return path_ninja
    return None


def main():
    if not is_windows():
        print("[ERROR] build_win.py is intended to be run on Windows.")
        sys.exit(2)

    parser = argparse.ArgumentParser(description="Windows build script for vkthing")
    parser.add_argument("--build-dir", default="build_win", help="Build directory (default: build_win)")
    parser.add_argument("--config", default="Release", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], help="Build configuration")
    parser.add_argument("--generator", default=None, help="CMake generator (e.g., Ninja, Visual Studio 17 2022)")
    parser.add_argument("--clean", action="store_true", help="Remove build dir before configure")
    parser.add_argument("--with-samples", action="store_true", help="Build optional sample projects (if present)")
    parser.add_argument("--arch", default="x64", choices=["x64", "Win32", "ARM64"], help="VS architecture (-A). Default: x64")
    parser.add_argument("--glfw-dir", default=None, help="Path to GLFW root or cmake config dir (e.g., C:/Program Files/glfw)")
    parser.add_argument("--glm-include", default=None, help="Path to GLM include directory (e.g., C:/Program Files/VulkanSDK/1.4.313.0/Include)")
    parser.add_argument("--run", action="store_true", help="Run sandbox after build")
    parser.add_argument("-j", "--jobs", type=int, default=None, help="Parallel build jobs")
    args = parser.parse_args()

    print(f"[INFO] Host: {platform.system()} {platform.release()} | Python {platform.python_version()}")
    env_ok = check_env()

    bdir = os.path.abspath(args.build_dir)
    if args.clean and os.path.isdir(bdir):
        print(f"[INFO] Removing build directory: {bdir}")
        shutil.rmtree(bdir)
    os.makedirs(bdir, exist_ok=True)

    try:
        # Pass optional GLM include override if provided
        if args.glm_include:
            os.environ["GLM_DIR"] = args.glm_include
        cmake_configure(bdir, args.generator, args.config, args.with_samples, args.arch, args.glfw_dir)
        cmake_build(bdir, args.config, args.jobs)
    except RuntimeError as e:
        print("[ERROR]", e)
        sys.exit(3)
    except subprocess.CalledProcessError as e:
        print("[ERROR] Build failed:", e)
        sys.exit(e.returncode)

    exe = find_executable(bdir, args.config)
    if exe:
        print(f"[INFO] Built sandbox: {exe}")
    else:
        print("[WARN] Could not locate sandbox executable.")

    if args.run:
        if not exe:
            print("[ERROR] Cannot run; executable not found.")
            sys.exit(4)
        try:
            run([exe])
        except subprocess.CalledProcessError as e:
            print("[ERROR] Sandbox run failed:", e)
            sys.exit(e.returncode)

    if not env_ok:
        print("[NOTE] Some environment checks failed; if CMake succeeded you can ignore this.")


if __name__ == "__main__":
    main()
